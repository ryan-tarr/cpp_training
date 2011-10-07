#include <stdio.h>
#include <util/ChannelManager.h>
#include <Platform/Platform.h>

#include <cell/sysmodule.h>
#include <netex/net.h>
#include <netex/libnetctl.h>
#include <sys/timer.h>
#include <sdk_version.h>
#include <np.h>
#include <np/basic.h>

#include <Tasks/OnlineTasks.h>
#include <Peer2Peer/Peer2Peer.h>

namespace AK
{
	void * AllocHook( size_t bytes )
	{
		void* pmem = XP_ALLOCATE_TAG(bytes, "Wwise");
		ASSERT(pmem != NULL);
		return pmem;
	}
	
    void FreeHook( void * memory )
	{
		XP_FREE(memory);
	}
}

template <typename T>
class Using
{
    public:
        Using(T *o) : obj(o) 
        { 
            obj->Initialize(); 
        }

        ~Using() 
        { 
            obj->Finalize(); 
            delete obj, obj = 0;
        }

        T& operator*() const { return *obj; }
        T* operator->() const { return obj; }

    private:
        T   *obj;
};

template <typename T>
struct Buffer
{
        size_t  size;
        T      *data;

        Buffer() : size(0), data(0) {}
        Buffer(size_t s) { Allocate(s); }
        ~Buffer() { Release(); }

        void Allocate(size_t s) { size = s, data = new T[size]; }
        void Release() { delete [] data, data = 0, size = 0; }
};

// NOTE: the API documents are inconsistent in their integer type treatment:
// assuming integers are 4-byte. This is perhaps not an unfair assumption 
// given the stable nature of the PS3 hardware, but may not always be true.
// We just use unadorned (unsigned) int.
//
// TODO: it's not appropriate to assert on every failed return code; 
// case-by-case return code error handling should be more robust.

namespace Network
{
    namespace PS3
    {
        namespace Product
        {
            static const SceNpCommunicationId Id = 
            {
                {},
                '\0',
                0,
                0
            };

            static const SceNpCommunicationPassphrase Passphrase = 
            {
                {
                }
            };

            static const SceNpCommunicationSignature Signature = 
            {
                {
                }
            };
        };

        struct Status
        {
            enum { NONE, WAITING, ABORTED, READY, ERROR };
            Status() : state (NONE) {}
            int state;
        };

        struct Updater
        {
            Status *object;
            Updater() : object(0) {}
            Updater(Status *o) : object(o) {}
            void operator()(int status) { object->state = status; }
        };

        struct User
        {
            User()
            {
                memset(&info, 0, sizeof(info));
                memset(&onlineId, 0, sizeof(onlineId));
                memset(&about, 0, sizeof(about));
                memset(&languages, 0, sizeof(languages));
                memset(&country, 0, sizeof(country));
                memset(&avatar, 0, sizeof(avatar));
            }

            User& operator=(const User &u)
            {
                memcpy(&info, &u.info, sizeof(info));
                memcpy(&onlineId, &u.onlineId, sizeof(onlineId));
                memcpy(&about, &u.about, sizeof(about));
                memcpy(&languages, &u.languages, sizeof(languages));
                memcpy(&country, &u.country, sizeof(country));
                memcpy(&avatar, &u.avatar, sizeof(avatar));
                return *this;
            }

            SceNpUserInfo       info;
            SceNpOnlineId       onlineId;
            SceNpAboutMe        about;
            SceNpMyLanguages    languages;
            SceNpCountryCode    country;
            SceNpAvatarImage    avatar;
        };

        struct Ticket : public Status
        {
            Ticket(const char *s) : service(s)
            {
                memset(&version, 0, sizeof(version));
            }

            Ticket& operator=(const Ticket &t)
            {
                service = t.service;
                memcpy(&version, &t.version, sizeof(version));
                return *this;
            }

            const char*         service;
            SceNpTicketVersion  version;

            struct Param
            {
                Param(int p) : id(p)
                {
                    memset(&param, 0, sizeof(param));

                    int res = sceNpManagerGetTicketParam(id, &param);
                    ASSERTF(res == 0, "[Network::PS3::Ticket::Param] Could not get ticket parameter. (0x%x)\n", res);
                }

                Param& operator=(const Param &p)
                {
                    id = p.id;
                    memcpy(&param, &p.param, sizeof(param));
                    return *this;
                }

                int                 id;
                SceNpTicketParam    param;
            };

            struct Entitlement
            {
                Entitlement(const char *e, size_t n) 
                    : entitlement(e), number(n)
                {}

                Entitlement& operator=(const Entitlement &e)
                {
                    entitlement = e.entitlement;
                    number = e.number;
                    return *this;
                }

                const char* entitlement;
                size_t      number;
            };

            struct Cookie
            {
                Cookie(const char *d, size_t s) 
                    : data(d), size(s) 
                {}

                Cookie& operator=(const Cookie &c) 
                { 
                    data = c.data; 
                    size = c.size; 
                    return *this;
                }

                const void* data;
                size_t      size;
            };
        };

        struct Transaction : public Status
        {
            Transaction(int ctx, const User &u) 
                : lookupctx(ctx), user(u)
            {
                id = sceNpLookupCreateTransactionCtx(lookupctx);
                ASSERTF(id == 0, "Could not create transaction context. (0x%x)\n", id);
            }

            ~Transaction()
            {
                id = sceNpLookupDestroyTransactionCtx(lookupctx);
                ASSERTF(id == 0, "Could not destroy transaction context. (0x%x)\n", id);
            }

            bool Poll()
            {
                int res = sceNpLookupPollAsync(id, &result);
                ASSERTF(id >= 0, "Failed to poll transaction. (0x%x)\n", res);

                switch (res)
                {
                    case 0: state = Status::READY; break;
                    case 1: state = Status::WAITING; break;
                }

                return state == Status::WAITING;
            }

            bool Wait()
            {
                int res = sceNpLookupWaitAsync(id, &result);
                ASSERTF(id >= 0, "Failed to wait for transaction. (0x%x)\n", res);
                
                switch (res)
                {
                    case 0: state = Status::READY; break;
                    case 1: state = Status::WAITING; break;
                }

                return state == Status::READY;
            }

            bool Abort()
            {
                int res = sceNpLookupAbortTransaction(id);
                ASSERTF(id >= 0, "Failed to abort transaction. (0x%x)\n", res);
                state = Status::ABORTED;

                return state == Status::ABORTED;
            }

            int         id;
            int         result;

            const int   lookupctx;
            const User& user;
        };

        struct Server : public Status
        {
            Server()
            {
                memset(&info, 0, sizeof(info));
            }

            Server& operator=(const Server &s)
            {
                memcpy(&info, &s.info, sizeof(info));
                return *this;
            }

            SceNpMatching2GetServerInfoResponse info;
        };

        class System
        {
            enum { SYSUTIL_SLOT = 3 };

            public:
                System() {}
                ~System() {}

                void Initialize()
                {
                    if (cellSysmoduleInitialize()							!= CELL_OK ||
                        cellSysmoduleLoadModule(CELL_SYSMODULE_NET)			!= CELL_OK ||
                        cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP)	!= CELL_OK ||
                        cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP2) != CELL_OK)
                        ABORTF("Could not load CELL_SYSMODULE_NET or CELL_SYSMODULE_SYSUTIL_NP2 or could not initialize PS3 sockets.");

                    int res = sys_net_initialize_network(); // initialize UDP/IP stack
                    ASSERTF(res >= 0, "sys_net_initialize_network() failed. (0x%x)\n", res);

                    res = sys_net_show_ifconfig(); // print ifconfig to TTY
                    ASSERTF(res >= 0, "sys_net_show_ifconfig() failed. (0x%x)\n", res);

                    res = cellNetCtlInit(); // initialize network control
                    ASSERTF(res >= 0,"cellNetCtlInit() failed. (0x%x)\n", res);

                    res = sceNp2Init(SCE_NP_MIN_POOL_SIZE, m_nppool);
                    ASSERTF(res == 0, "Could not initialize system. (0x%x)\n", res);

                    res = cellSysutilRegisterCallback(SYSUTIL_SLOT, &System::sysutil_cb, 0);
                    ASSERTF(res == 0, "Could not set sysutil callback. (0x%x)\n", res);
                }

                void Finalize()
                {
                    int res = cellSysutilUnregisterCallback(SYSUTIL_SLOT);
                    ASSERTF(res == 0, "Could not terminate system. (0x%x)\n", res);

                    res = sceNp2Term();
                    ASSERTF(res == 0, "Could not terminate system. (0x%x)\n", res);

                    cellNetCtlTerm();

                    res = sys_net_finalize_network();
                    ASSERTF(res == 0, "Could not finalize system. (0x%x)\n", res);

                    if (cellSysmoduleUnloadModule(CELL_SYSMODULE_NET)			!= CELL_OK ||
                        cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP)	!= CELL_OK ||
                        cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP2)   != CELL_OK ||
                        cellSysmoduleFinalize()							        != CELL_OK)
                        ABORTF("Could not load CELL_SYSMODULE_NET or CELL_SYSMODULE_SYSUTIL_NP2 or could not finalize PS3 sockets.");
                }

                bool Update()
                {
                    int res = cellSysutilCheckCallback(); // pump system utility events
                    ASSERTF(res == 0, "Could not update system. (0x%x)\n", res);
                    return true;
                }

            private:
                static void sysutil_cb(uint64_t status, uint64_t param, void *data)
                {
                    //switch (status)
                    //{
                    //    case CELL_SYSUTIL_REQUEST_EXITGAME:
                    //    case CELL_SYSUTIL_DRAWING_BEGIN:
                    //    case CELL_SYSUTIL_DRAWING_END:
                    //    case CELL_SYSUTIL_SYSTEM_MENU_OPEN:
                    //    case CELL_SYSUTIL_SYSTEM_MENU_CLOSE:
                    //    case CELL_SYSUTIL_BGMPLAYBACK_PLAY:
                    //    case CELL_SYSUTIL_BGMPLAYBACK_STOP:
                    //    case CELL_SYSUTIL_NP_INVITATION_SELECTED:
                    //        break;
                    //}
                }

            private:
                uint8_t m_nppool[SCE_NP_MIN_POOL_SIZE];
        };

        class Manager
        {
            public:
                Manager() {}
                ~Manager() {}

                void Initialize()
                {
                    m_online = false;

                    int res = sceNpManagerInit();
                    ASSERTF(res == 0, "Could not initialize manager. (0x%x)\n", res);

                    res = sceNpLookupInit();
                    ASSERTF(res == 0, "Could not initialize lookup.(0x%x)\n", res);

                    res = sceNpManagerRegisterCallback(&Manager::manager_cb, this);
                    ASSERTF(res == 0, "Could not register callback. (0x%x)\n", res);

                    // must be online before we can get user info: try for 10 * 1 seconds
                    for (int i=0, sec=10; i < sec; ++i)
                        if (res = TryGetLocalUser(m_local)) 
                            break;
                        else cellSysutilCheckCallback(), sys_timer_sleep(1);
                    
                    ASSERTF(res == 1, "Could not get local user.\n");

                    res = sceNpLookupCreateTitleCtx(&Product::Id, &m_local.info.userId);
                    ASSERTF(res > 0, "Could not create lookup context. (0x%x)\n", res);

                    m_context = res;
                }

                void Finalize()
                {
                    int res = sceNpLookupDestroyTitleCtx(m_context);
                    ASSERTF(res == 0, "Could not destroy lookup context. (0x%x)\n", res);

                    res = sceNpManagerUnregisterCallback();
                    ASSERTF(res == 0, "Could not unregister callback. (0x%x)\n", res);
                    
                    res = sceNpLookupTerm();
                    ASSERTF(res == 0, "Could not finalize lookup. (0x%x)\n", res);

                    res = sceNpManagerTerm();
                    ASSERTF(res == 0, "Could not finalize manager. (0x%x)\n", res);
                }

                bool TryGetStatus(int &status) const
                {
                    return sceNpManagerGetStatus(&status) == 0;
                }

                bool TryGetLocalUser(User &user) const
                {
                    bool success = false;

                    if (m_online)
                    {
                        int res = sceNpManagerGetNpId(&user.info.userId);
                        ASSERTF(res >= 0, "Get NpId failed. (0x%x)\n", res);

                        res = sceNpManagerGetOnlineId(&user.onlineId);
                        ASSERTF(res >= 0, "Get OnlineId failed. (0x%x)\n", res);

                        res = sceNpManagerGetOnlineName(&user.info.name);
                        ASSERTF(res >= 0, "Get OnlineName failed. (0x%x)\n", res);

                        success = true;
                    }
                    
                    return success;
                }

                bool TryGetTicket(Ticket &ticket)
                {
                    // todo: handle cookies and entitlements
                    int res = sceNpManagerRequestTicket2(&m_local.info.userId, &ticket.version, ticket.service, 0, 0, 0, 0);
                    ASSERTF(res == 0, "Could not request ticket. (0x%x)\n", res);

                    ASSERTF(m_updater.object->state == Status::READY, "Previous request ticket did not complete. (%s)\n", \
                            static_cast<Ticket *>(m_updater.object)->service);

                    m_updater = Updater(&ticket);

                    return true;
                }

                Transaction CreateTransaction(const User &user) const
                {
                    return Transaction(m_context, user);
                }

            private:
                static void manager_cb(int event, int result, void *data)
                {
                    Manager *m = static_cast<Manager *> (data);

                    ASSERTF(result >= 0, "Error with ticket request. (0x%x)\n", result);
                        
                    switch (event)
                    {
                        case SCE_NP_MANAGER_STATUS_OFFLINE:
                            m->m_online = false;
                            break;

                        case SCE_NP_MANAGER_STATUS_GETTING_TICKET:
                            break;

                        case SCE_NP_MANAGER_STATUS_GETTING_PROFILE:
                            break;

                        case SCE_NP_MANAGER_STATUS_LOGGING_IN:
                            break;

                        case SCE_NP_MANAGER_STATUS_ONLINE:
                            m->m_online = true;
                            break;

                        case SCE_NP_MANAGER_EVENT_GOT_TICKET:
                            m->m_updater(Status::READY);
                            break;
                    }
                }

            private:
                User        m_local;
                Updater     m_updater;
                int         m_context;
                bool        m_online;
        };

        class MatchMaker
        {
            typedef std::vector<SceNpMatching2RequestId>    RequestIdQueue;
            typedef std::vector<Updater>                    UpdaterQueue;

            public:
                MatchMaker(const Manager &m) : m_manager(m) {}
                ~MatchMaker() {}

                void Initialize()
                {
                    int res = sceNpMatching2Init2(0, 0, 0); // use default values for heap and queues
                    ASSERTF(res == 0, "Could not initialize matchmaking. (0x%x)\n", res);

                    res = m_manager.TryGetLocalUser(m_local);
                    ASSERTF(res == 1, "Could not get local user.\n");

                    int opt = SCE_NP_MATCHING2_CONTEXT_OPTION_USE_ONLINENAME | SCE_NP_MATCHING2_CONTEXT_OPTION_USE_AVATARURL; 
                    res = sceNpMatching2CreateContext(&m_local.info.userId, &Product::Id, &Product::Passphrase, &m_context, opt);
                    ASSERTF(res == 0, "Could not get matchmaking context. (0x%x)\n", res);

                    int timeout = 10 * 1000 * 1000; // wait for ten seconds
                    res = sceNpMatching2ContextStartAsync(m_context, timeout);
                    ASSERTF(res == 0, "Could not start matchmaking context. (0x%x)\n", res);

                    res = sceNpMatching2RegisterContextCallback(m_context, context_event_cb, this);
                    ASSERTF(res == 0, "Could not register context callback. (0x%x)\n", res);

                    SceNpMatching2RequestOptParam param;
                    memset(&param, 0, sizeof(param));
                    param.cbFunc = request_event_cb;
                    param.cbFuncArg = this;
                    param.timeout = 20 * 1000 * 1000; // 20s

                    res = sceNpMatching2SetDefaultRequestOptParam(m_context, &param);
                    ASSERTF(res == 0, "Could not register request callback. (0x%x)\n", res);

                    res = sceNpMatching2RegisterSignalingCallback(m_context, signaling_event_cb, this);
                    ASSERTF(res == 0, "Could not register signaling callback. (0x%x)\n", res);

                    res = sceNpMatching2RegisterRoomEventCallback(m_context, room_event_cb, this);
                    ASSERTF(res == 0, "Could not register room callback. (0x%x)\n", res);

                    res = sceNpMatching2RegisterRoomMessageCallback(m_context, room_message_event_cb, this);
                    ASSERTF(res == 0, "Could not register room message callback. (0x%x)\n", res);

                    res = sceNpMatching2RegisterLobbyEventCallback (m_context, lobby_event_cb, this);
                    ASSERTF(res == 0, "Could not register lobby callback. (0x%x)\n", res);

                    res = sceNpMatching2RegisterLobbyMessageCallback(m_context, lobby_message_event_cb, this);
                    ASSERTF(res == 0, "Could not register lobby message callback. (0x%x)\n", res);

                }

                void Finalize()
                {
                    int res = sceNpMatching2ContextStop(m_context);
                    ASSERTF(res == 0, "Could not stop matchmaking context. (0x%x)\n", res);

                    res = sceNpMatching2Term2();
                    ASSERTF(res == 0, "Could not terminate matchmaking. (0x%x)\n", res);
                }

                bool TryGetServerInfo(Server &server, int index = -1)
                {
                    bool success = false;

                    if (m_servers.size > 0 && m_servers.size < 255)
                    {
                        if (index < 0 || index > m_servers.size)
                            index = rand() % m_servers.size; // todo: seed

                        SceNpMatching2RequestId id; 
                        SceNpMatching2GetServerInfoRequest req;
                        req.serverId = m_servers.data[index]; // convert index to id

                        int res = sceNpMatching2GetServerInfo(m_context, &req, 0, &id);
                        ASSERTF(res == 0, "Could not request server info. (0x%x)\n", res);

                        m_request_queue.push_back(id); // needed to track this request
                        m_updater_queue.push_back(Updater(&server)); // update result when ready

                        success = true;
                    }

                    return success;
                }

            private:
                void setup_server_id_list()
                {
                    int res = sceNpMatching2GetServerIdListLocal(m_context, 0, 0); // get number of servers
                    ASSERTF(res >= 0, "Could not get number of servers. (0x%x)\n", res);
                    
                    m_servers.Release();
                    m_servers.Allocate(res);

                    res = sceNpMatching2GetServerIdListLocal(m_context, m_servers.data, m_servers.size);
                    ASSERTF(res >= 0, "Could not get server list. (0x%x)\n", res);
                }

                static void context_event_cb
                    (SceNpMatching2ContextId ctx, 
                     SceNpMatching2Event event, 
                     SceNpMatching2EventCause cause,
                     int error, void *arg)
                {
                    MatchMaker *mm = static_cast<MatchMaker *>(arg);

                    if (error < 0)
                        return;

                    switch (event)
                    {
                        case SCE_NP_MATCHING2_CONTEXT_EVENT_StartOver:
                            break;

                        case SCE_NP_MATCHING2_CONTEXT_EVENT_Start:
                            if (cause == SCE_NP_MATCHING2_EVENT_CAUSE_CONTEXT_ACTION)
                                mm->setup_server_id_list();
                            break;

                        case SCE_NP_MATCHING2_CONTEXT_EVENT_Stop:
                            break;

                        default:
                            break;
                    }
                }

                static void request_event_cb 
                    (SceNpMatching2ContextId ctx, 
                     SceNpMatching2RequestId req, 
                     SceNpMatching2Event event, 
                     SceNpMatching2EventKey key,
                     int error, size_t size, void *arg)
                {
                    MatchMaker *mm = static_cast<MatchMaker *>(arg);

                    typename RequestIdQueue::iterator ri = mm->m_request_queue.begin(); 
                    typename RequestIdQueue::iterator re = mm->m_request_queue.end(); 
                    typename UpdaterQueue::iterator ui = mm->m_updater_queue.begin();
                    typename UpdaterQueue::iterator ue = mm->m_updater_queue.end();

                    // find the updater that matches this request
                    for (; ri != re; ++ri, ++ui)
                        if (*ri == req) break; 
                    
                    ASSERTF(ui != ue, "Count not find request id.");

                    Updater updater (*ui);
                    mm->m_request_queue.erase(ri);
                    mm->m_updater_queue.erase(ui);

                    if (error < 0)
                    {
                        updater(Status::ERROR);
                        return;
                    }

                    int res = 0;
                    switch (event)
                    {
                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetServerInfo:
                            {
                                Server *obj = static_cast<Server *>(updater.object);
                                ASSERTF(size == sizeof(obj->info), "Event data wrong size.\n");

                                res = sceNpMatching2GetEventData(mm->m_context, key, &obj->info, size);
                                ASSERTF(res >= 0, "Could not get event data. (0x%x)\n", res);
                                ASSERTF(res == size, "Could not get all event data. (0x%x)\n", res);

                                updater(Status::READY);
                            }
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetWorldInfoList:
                            //SceNpMatching2GetWorldInfoListResponse *resp = (SceNpMatching2GetWorldInfoListResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetRoomMemberDataExternalList:
                            //SceNpMatching2GetRoomMemberDataExternalListResponse *resp = (SceNpMatching2GetRoomMemberDataExternalListResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SetRoomDataExternal:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetRoomDataExternalList:
                            //SceNpMatching2GetRoomDataExternalListResponse *resp = (SceNpMatching2GetRoomDataExternalListResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetLobbyInfoList:
                            //SceNpMatching2GetLobbyInfoListResponse *resp = (SceNpMatching2GetLobbyInfoListResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SetUserInfo:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetUserInfoList:
                            //SceNpMatching2GetUserInfoListResponse *resp = (SceNpMatching2GetUserInfoListResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_CreateServerContext:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_DeleteServerContext:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_CreateJoinRoom:
                            //SceNpMatching2CreateJoinRoomResponse *resp = (SceNpMatching2CreateJoinRoomResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_JoinRoom:
                            //SceNpMatching2JoinRoomResponse *resp = (SceNpMatching2JoinRoomResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_LeaveRoom:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GrantRoomOwner:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_KickoutRoomMember:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SearchRoom:
                            //SceNpMatching2SearchRoomResponse *resp = (SceNpMatching2SearchRoomResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SendRoomChatMessage:
                            //SceNpMatching2SendRoomChatMessageResponse *resp = (SceNpMatching2SendRoomChatMessageResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SendRoomMessage:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SetRoomDataInternal:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetRoomDataInternal:
                            //SceNpMatching2GetRoomDataInternalResponse *resp = (SceNpMatching2GetRoomDataInternalResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SetRoomMemberDataInternal:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetRoomMemberDataInternal:
                            //SceNpMatching2GetRoomMemberDataInternalResponse *resp = (SceNpMatching2GetRoomMemberDataInternalResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SetSignalingOptParam:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_JoinLobby:
                            //SceNpMatching2JoinLobbyResponse *resp = (SceNpMatching2JoinLobbyResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_LeaveLobby:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SendLobbyChatMessage:
                            //SceNpMatching2SendLobbyChatMessageResponse *resp = (SceNpMatching2SendLobbyChatMessageResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SendLobbyInvitation:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SetLobbyMemberDataInternal:
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetLobbyMemberDataInternal:
                            //SceNpMatching2GetLobbyMemberDataInternalResponse *resp = (SceNpMatching2GetLobbyMemberDataInternalResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_GetLobbyMemberDataInternalList:
                            //SceNpMatching2GetLobbyMemberDataInternalListResponse *resp = (SceNpMatching2GetLobbyMemberDataInternalListResponse *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_REQUEST_EVENT_SignalingGetPingInfo:
                            //SceNpMatching2SignalingGetPingInfoResponse *resp = (SceNpMatching2SignalingGetPingInfoResponse *)buf.data;
                            break;
                    }
                }

                static void signaling_event_cb
                    (SceNpMatching2ContextId ctx, 
                     SceNpMatching2RoomId room,
                     SceNpMatching2RoomMemberId src,
                     SceNpMatching2Event event, 
                     int error, void *arg)
                {
                    MatchMaker *mm = static_cast<MatchMaker *>(arg);

                    if (error < 0)
                        return;

                    switch (event)
                    {
                        case SCE_NP_MATCHING2_SIGNALING_EVENT_Dead:
                            break;

                        case SCE_NP_MATCHING2_SIGNALING_EVENT_Established:
                            break;
                    }
                }

                static void room_event_cb 
                    (SceNpMatching2ContextId ctx, 
                     SceNpMatching2RoomId room, 
                     SceNpMatching2Event event, 
                     SceNpMatching2EventKey key,
                     int error, size_t size, void *arg)
                {
                    MatchMaker *mm = static_cast<MatchMaker *>(arg);

                    if (error < 0)
                        return;

                    switch (event)
                    {
                        case SCE_NP_MATCHING2_ROOM_EVENT_MemberJoined:
                            //SceNpMatching2RoomMemberUpdateInfo *resp = (SceNpMatching2RoomMemberUpdateInfo *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_ROOM_EVENT_MemberLeft:
                            //SceNpMatching2RoomMemberUpdateInfo *resp = (SceNpMatching2RoomMemberUpdateInfo *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_ROOM_EVENT_Kickedout:
                            //SceNpMatching2RoomUpdateInfo *resp = (SceNpMatching2RoomUpdateInfo *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_ROOM_EVENT_RoomDestroyed:
                            //SceNpMatching2RoomUpdateInfo *resp = (SceNpMatching2RoomUpdateInfo *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_ROOM_EVENT_RoomOwnerChanged:
                            //SceNpMatching2RoomOwnerUpdateInfo *resp = (SceNpMatching2RoomOwnerUpdateInfo *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_ROOM_EVENT_UpdatedRoomDataInternal:
                            //SceNpMatching2RoomMemberDataInternalUpdateInfo *resp = (SceNpMatching2RoomMemberDataInternalUpdateInfo *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_ROOM_EVENT_UpdatedRoomMemberDataInternal:
                            //SceNpMatching2RoomMemberUpdateInfo *resp = (SceNpMatching2RoomMemberUpdateInfo *)buf.data;
                            break;

                        case SCE_NP_MATCHING2_ROOM_EVENT_UpdatedSignalingOptParam:
                            //SceNpMatching2SignalingOptParamUpdateInfo *resp = (SceNpMatching2SignalingOptParamUpdateInfo *)buf.data;
                            break;
                    }
                }

                static void room_message_event_cb
                    (SceNpMatching2ContextId ctx, 
                     SceNpMatching2RoomId room,
                     SceNpMatching2RoomMemberId src,
                     SceNpMatching2Event event, 
                     SceNpMatching2EventKey key,
                     int error, size_t size, void *arg)
                {
                    //MatchMaker *mm = static_cast<MatchMaker *>(arg);

                    //if (error < 0)
                    //    return;

                    //switch (event)
                    //{
                    //    case SCE_NP_MATCHING2_ROOM_MSG_EVENT_ChatMessage:
                    //    case SCE_NP_MATCHING2_ROOM_MSG_EVENT_Message:
                    //        break;
                    //}
                }

                static void lobby_event_cb
                    (SceNpMatching2ContextId ctx, 
                     SceNpMatching2LobbyId lobby, 
                     SceNpMatching2Event event, 
                     SceNpMatching2EventKey key,
                     int error, size_t size, void *arg)
                {
                    //MatchMaker *mm = static_cast<MatchMaker *>(arg);

                    //if (error < 0)
                    //    return;

                    //switch (event)
                    //{
                    //    case SCE_NP_MATCHING2_LOBBY_EVENT_MemberJoined:
                    //    case SCE_NP_MATCHING2_LOBBY_EVENT_MemberLeft:
                    //    case SCE_NP_MATCHING2_LOBBY_EVENT_LobbyDestroyed:
                    //    case SCE_NP_MATCHING2_LOBBY_EVENT_UpdatedLobbyMemberDataInternal:
                    //        break;
                    //}
                }

                static void lobby_message_event_cb
                    (SceNpMatching2ContextId ctx, 
                     SceNpMatching2LobbyId lobby, 
                     SceNpMatching2LobbyMemberId src,
                     SceNpMatching2Event event, 
                     SceNpMatching2EventKey key,
                     int error, size_t size, void *arg)
                {
                    //MatchMaker *mm = static_cast<MatchMaker *>(arg);

                    //if (error < 0)
                    //    return;

                    //switch (event)
                    //{
                    //    case SCE_NP_MATCHING2_LOBBY_MSG_EVENT_ChatMessage:
                    //    case SCE_NP_MATCHING2_LOBBY_MSG_EVENT_Invitation:
                    //        break;
                    //}
                }

            private:
                const Manager &                 m_manager;
                User                            m_local;

                SceNpMatching2ContextId         m_context;
                //SceNpMatching2                  m_server_context;
                Buffer<SceNpMatching2ServerId>  m_servers;

                RequestIdQueue                  m_request_queue;
                UpdaterQueue                    m_updater_queue;
        };
    }

    typedef PS3::System     System;
    typedef PS3::Manager    Manager;
    typedef PS3::MatchMaker MatchMaker;
    typedef PS3::Status     Status;
    typedef PS3::Server     Server;
}

class Application
{
    enum { START, RUNNING, SEARCHING, STOP };

    public:
        Application() : 
            system (new Network::System), 
            manager (new Network::Manager), 
            matchmaker (new Network::MatchMaker(*manager)),
            state (START)
        {}

        bool Update()
        {
            bool update = system->Update();

            switch (state)
            {
                case START:
                    {
                        state = SEARCHING;
                        server.state = Network::Status::NONE;
                    }
                    break;

                case SEARCHING:
                    if (server.state == Network::Status::NONE)
                    {
                        if (matchmaker->TryGetServerInfo(server))
                            state = RUNNING;

                        server.state == Network::Status::WAITING;
                    }
                    break;

                case RUNNING:
                    if (server.state == Network::Status::READY)
                    {
                        if (server.info.server.status == SCE_NP_MATCHING2_SERVER_STATUS_AVAILABLE)
                        {
                            state = STOP;
                            printf("found available server!");
                        }
                        else
                        {
                            state = SEARCHING;
                            server.state = Network::Status::NONE;
                        }
                    }
                    break;

                case STOP:
                    {
                        update = false;
                        server.state = Network::Status::NONE;
                    }
                    break;
            }

            return update;
        }

    private:
        Using<Network::System>      system;
        Using<Network::Manager>     manager;
        Using<Network::MatchMaker>  matchmaker;
        Network::Server             server;
        int                         state;
};

int main(int argc, char** argv)
{
    Application app;

    while (app.Update());

	return 0;
}
