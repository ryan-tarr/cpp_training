template <typename T>
class MyUniquePtr {
    private:
    T* ptr;

    public:
    // default constructor
    MyUniquePtr(T* other_ptr = nullptr){
        ptr = other_ptr;
    }

    // destructor
    ~MyUniquePtr(){
        if (ptr != nullptr){
            delete ptr;
        }
    }

    // copy
    MyUniquePtr(const MyUniquePtr<T>& other)=delete;
    MyUniquePtr& operator=(const MyUniquePtr<T>& other)=delete;

    // move
    MyUniquePtr(MyUniquePtr<T>&& other){
        ptr = other.ptr;
        other.ptr = nullptr;
    }
    MyUniquePtr& operator=(MyUniquePtr<T>&& other){
        if (this != &other){
            reset(other.ptr);
            other.ptr = nullptr;
        }
        return *this;
    }

    // equality operators
    bool operator==(const MyUniquePtr<T>& other) const{return ptr==other.ptr;}
    bool operator!=(const MyUniquePtr<T>& other) const{return ptr!=other.ptr;}

    // dereference operators
    T& operator*() const{return *ptr;}
    T* operator->() const{return ptr;}

    // observers
    const T* get() const{return ptr;}

    // acccessors
    T* release(){
        T* out = ptr;
        ptr = nullptr;
        return out;
    }
    void reset(T* other_ptr = nullptr){
        if (ptr != nullptr) {
            delete ptr;
        }
        ptr = other_ptr;
    }
};