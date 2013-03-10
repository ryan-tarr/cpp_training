#include <iostream>
#include <sstream>
#include <thread>
#include <system_error>

#include <cstring>
#include <cerrno>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

struct scoped_thread
{
    std::thread thread;

    scoped_thread (std::thread t) 
        : thread {std::move (t)} {}

    scoped_thread (scoped_thread const &) = delete;
    scoped_thread &operator= (scoped_thread const &) = delete;
    
    ~scoped_thread ()
    {
        if (thread.joinable ())
            thread.join ();
    }
};

namespace io { namespace net {

namespace detail 
{
    bool try_string_to_inet4 (std::string const &str, sockaddr_in &addr)
    {
        using std::string;
        using std::stoul; // WARN: can throw

        string host;
        size_t portmark = str.rfind (':');

        host = str.substr (0, portmark);
        if (portmark != string::npos) 
            addr.sin_port = htons (stoul (str.substr (portmark + 1)));

        int result = inet_pton (AF_INET, host.c_str(), &addr.sin_addr);
        bool success = result > 0;

        return success;
    }

    bool try_inet4_to_string (sockaddr_in const &addr, std::string &str)
    {
        using std::to_string; // WARN: can throw

        char buf [INET6_ADDRSTRLEN];

        char const *result = inet_ntop (AF_INET, &addr.sin_addr, buf, sizeof (buf));
        bool success = result != nullptr;

        if (success)
        {
            str = buf; 
            str += ':';
            str += to_string (ntohs (addr.sin_port));
        }
        
        return success;
    }

    void load_last_system_error_code (std::error_code &error)
    {
        // PORT: errno used in POSIX plaforms only
        return error.assign (errno, std::system_category ()); 
    }
}

class socket
{
    public:
        enum class type { TCP, UDP }; // TODO: VDP et al.

        class address
        {
            public:
                address ()
                {
                    std::memset (&address_, 0, sizeof (address_));
                    address_.sin_family = AF_INET;
                    error_ = std::errc::bad_address;
                }

                address (address const &other) = default;
                address &operator= (address const &other) = default;
                ~address () = default;

            public:
                address (std::string const &addr)
                {
                    if (!detail::try_string_to_inet4 (addr, address_))
                        detail::load_last_system_error_code (error_);
                }

                operator std::string () const 
                {
                    std::string result;
                    detail::try_inet4_to_string (address_, result);
                    return result;
                }

                operator sockaddr* () { return reinterpret_cast<sockaddr *> (&address_); }

            public:
                operator bool () const { return !error_; }
                std::error_code error () const { return error_; }

            public:
                bool has_same_port (address const &other) const
                {
                    return address_.sin_port == other.address_.sin_port;
                }

                bool has_same_host (address const &other) const
                {
                    return address_.sin_addr.s_addr == other.address_.sin_addr.s_addr;
                }

                bool operator== (address const &other) const 
                {
                    return has_same_port (other) && has_same_host (other);
                }

            public:
                uint16_t port () const { return ntohs (address_.sin_port); }
                void set_port (uint16_t port) { address_.sin_port = htons (port); }

                uint32_t host () const { return ntohl (address_.sin_addr.s_addr); }
                void set_host (uint32_t host) { address_.sin_addr.s_addr = htonl (host); }

            private:
                sockaddr_in     address_;
                std::error_code error_;
        };

        using handle_type = int;

    public:
        socket (type kind) 
        {
            int sockfam, socktype, sockproto;
            map_socket_type (kind, sockfam, socktype, sockproto);
            if (handle_ = ::socket (sockfam, socktype, sockproto) < 0) 
                detail::load_last_system_error_code (error_);
        }

        socket (socket const &other) = default;
        socket &operator= (socket const &socket) = default;
        ~socket () = default;

    public:
        operator bool () const { return !error_; }
        std::error_code error () const { return error_; }

    private:
        void map_socket_type (type kind, int &sockfam, int &socktype, int &sockprot)
        {
            switch (kind)
            {
                case type::TCP:
                    sockfam = AF_INET;
                    socktype = SOCK_STREAM;
                    sockprot = IPPROTO_TCP;
                    break;

                case type::UDP:
                    sockfam = AF_INET;
                    socktype = SOCK_DGRAM;
                    sockprot = IPPROTO_UDP;
                    break;

                default:
                    sockfam = socktype = sockprot = -1;
                    break;
            }
        }

    private:
        handle_type     handle_ = 0;
        std::error_code error_;
};

}}

int main (int argc, char **argv)
{
    io::net::socket::address addr {"127.0.0.1:8080"};
    cout << (std::string) addr << endl;
    cout << addr.host() << endl;
    cout << addr.port() << endl;

    //scoped_thread network { std::thread { [] { } } };

    //bool quit = false;
    //std::string command;

    //while (!quit && cin >> command)
    //{
    //    if (command == "quit")
    //        quit = true;
    //}

    return 0;
}
