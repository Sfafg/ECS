#include "PopbackArray.h"
#include <utility>
#include <cstring>
#include <assert.h>

PopbackArray::PopbackArray() :
    m_data(nullptr)
{}

PopbackArray::PopbackArray(PopbackArray&& rhs) :
    PopbackArray()
{
    std::swap(m_data, rhs.m_data);
}

PopbackArray& PopbackArray::operator=(PopbackArray&& rhs)
{
    if (this != &rhs)
        std::swap(m_data, rhs.m_data);

    return *this;
}

PopbackArray::~PopbackArray()
{
    if (m_data)
    {
        free(m_data);
        m_data = nullptr;
    }
}

void PopbackArray::append(const void* element, int size, int byteSize)
{
    memcpy(at(size, byteSize), element, byteSize);
}

void PopbackArray::append(void* element, int size, int byteSize, void(*moveConstructor)(void*, void*))
{
    moveConstructor(at(size, byteSize), element);
}

void PopbackArray::pop(int index, int size, int byteSize)
{
    assert(0 <= index && index < size && "Popping outside the range");
    if (index == size - 1)
        return;

    memcpy(at(index, byteSize), at(size - 1, byteSize), byteSize);
}

void PopbackArray::pop(int index, int size, int byteSize, void(*moveConstructor)(void*, void*))
{
    assert(0 <= index && index < size && "Popping outside the range");
    if (index == size - 1)
        return;

    moveConstructor(at(index, byteSize), at(size - 1, byteSize));
}

void PopbackArray::reserve(int oldCapacity, int newCapacity, int byteSize)
{
    void* newComponents = realloc(m_data, newCapacity * byteSize);

    if (!newComponents)
    {
        newComponents = malloc(newCapacity * byteSize);
        memcpy(newComponents, m_data, (oldCapacity < newCapacity ? oldCapacity : newCapacity) * byteSize);

        if (m_data)
            free(m_data);
    }

    m_data = newComponents;
}

void PopbackArray::reserve(int oldCapacity, int newCapacity, int byteSize, void(*moveConstructor)(void*, void*))
{
    void* newComponents = malloc(newCapacity * byteSize);
    for (int i = 0; i < (oldCapacity < newCapacity ? oldCapacity : newCapacity); i++)
        moveConstructor((char*) newComponents + i * byteSize, (char*) m_data + i * byteSize);

    if (m_data)
        free(m_data);

    m_data = newComponents;
}

const void* PopbackArray::at(int index, int byteSize) const
{
    return (char*) m_data + index * byteSize;
}

void* PopbackArray::at(int index, int byteSize)
{
    return (char*) m_data + index * byteSize;
}

const void* PopbackArray::data() const
{
    return m_data;
}

void* PopbackArray::data()
{
    return m_data;
}