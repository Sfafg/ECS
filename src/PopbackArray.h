#include <utility>

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
    void append(void* element, int size, int byteSize, void(*moveConstructor)(void*, void*));
    void pop(int index, int size, int byteSize);
    void pop(int index, int size, int byteSize, void(*moveConstructor)(void*, void*));

    void reserve(int oldCapacity, int newCapacity, int byteSize);
    void reserve(int oldCapacity, int newCapacity, int byteSize, void(*moveConstructor)(void*, void*));
    void* at(int index, int byteSize);
    const void* at(int index, int byteSize) const;
    void* data();
    const void* data() const;

    template<typename T>
    void append(const T& element, int size) { append(&element, size, sizeof(T)); }
    template<typename T>
    void pop(int index, int size) { pop(index, size, sizeof(T)); }
    template<typename T>
    void pop(int index, int size, void(*moveConstructor)(void*, void*)) { pop(index, size, sizeof(T), moveConstructor); }

    template<typename T>
    T& at(int index) { return *(T*) at(index, sizeof(T)); }
    template<typename T>
    const T& at(int index) const { return *(T*) at(index, sizeof(T)); }

    template <typename T>
    void emplace_back(T&& element, int size)
    {
        new(&at<T>(size)) T(std::move(element));
    }
};