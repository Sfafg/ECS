class PopbackArray
{
    void* m_data;

public:
    PopbackArray();
    PopbackArray(PopbackArray&& rhs);
    PopbackArray& operator=(PopbackArray&& rhs);
    ~PopbackArray();

    PopbackArray(const PopbackArray& rhs) = delete;
    PopbackArray& operator=(const PopbackArray& rhs) = delete;

    void append(const void* element, int size, int byteSize);
    void pop(int index, int size, int byteSize);

    void reserve(int oldCapacity, int newCapacity, int byteSize);
    void* at(int index, int byteSize);
    const void* at(int index, int byteSize) const;
    void* data();
    const void* data() const;

    template<typename T>
    void append(const T& element, int size) { append(&element, size, sizeof(T)); }
    template<typename T>
    void pop(int index, int size) { pop(index, size, sizeof(T)); }

    template<typename T>
    T& at(int index) { return *(T*) at(index, sizeof(T)); }
    template<typename T>
    const T& at(int index) const { return *(T*) at(index, sizeof(T)); }

};