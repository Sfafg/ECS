#include "ECS.h"
#include <math.h>
#include <cstring>
namespace ECS
{
    int ComponentInfo::RegisterComponent(int byteSize)
    {
        byteSizes.push_back(byteSize);
        return byteSizes.size() - 1;
    }

    int ComponentInfo::GetCount()
    {
        return byteSizes.size();
    }

    int ComponentInfo::GetByteSize(int id)
    {
        assert(0 <= id && id < byteSizes.size() && "Invalid Component ID");
        return byteSizes[id];
    }


    Entity::Entity() :
        archetypeID(-1),
        id(0)
    {}

    Entity::Entity(Entity&& rhs) :
        archetypeID(rhs.archetypeID),
        id(rhs.id)
    {}

    Entity& Entity::operator=(Entity&& rhs)
    {
        if (this != &rhs)
            std::swap(*this, rhs);

        return *this;
    }

    Entity::~Entity()
    {
        if (archetypeID == -1)
            return;

        assert(0 <= archetypeID && archetypeID < ArchetypePool::archetypes.size() && "Invalid Archetype ID");
        assert(0 <= id && id < ArchetypePool::archetypes[archetypeID].entityCount && "Invalid Entity ID");

        Archetype& archetype = ArchetypePool::archetypes[archetypeID];
        for (int i = 0; i < archetype.denseComponentMap.size(); i++)
        {
            int index = archetype.denseComponentMap[i];
            archetype.sparseComponentArray[index].set_at(id, archetype.sparseComponentArray[index].at(archetype.entityCount - 1, index), index);
        }

        id = 0;
        archetypeID = -1;
    }

    bool Entity::HasComponent(int componentID) const
    {
        assert(0 <= archetypeID && archetypeID < ArchetypePool::archetypes.size() && "Invalid Archetype ID");

        return ArchetypePool::archetypes[archetypeID].componentMask.GetBit(componentID);
    }

    void* Entity::GetComponent(int componentID)
    {
        assert(0 <= archetypeID && archetypeID < ArchetypePool::archetypes.size() && "Invalid Archetype ID");
        assert(0 <= id && id < ArchetypePool::archetypes[archetypeID].entityCount && "Invalid Entity ID");

        return ArchetypePool::archetypes[archetypeID].sparseComponentArray[componentID].at(id, componentID);
    }

    const void* Entity::GetComponent(int componentID) const
    {
        assert(0 <= archetypeID && archetypeID < ArchetypePool::archetypes.size() && "Invalid Archetype ID");
        assert(0 <= id && id < ArchetypePool::archetypes[archetypeID].entityCount && "Invalid Entity ID");

        return ArchetypePool::archetypes[archetypeID].sparseComponentArray[componentID].at(id, componentID);
    }


    ComponentMask::ComponentMask() {}

    ComponentMask::ComponentMask(int componentCount) :
        field(std::vector<int>(ceil(componentCount / (float) sizeof(int) / 8.0f)))
    {}

    void ComponentMask::SetBit(int index)
    {
        GetChunk(index) |= 1 << index;
    }

    bool ComponentMask::GetBit(int index) const
    {
        return (GetChunk(index) & 1 << index) == 1 << index;
    }

    int ComponentMask::CountSetBits() const
    {
        int count = 0;
        for (int i = 0; i < field.size(); i++)
        {
            int n = field[i];
            while (n)
            {
                n &= (n - 1);
                count++;
            }
        }
        return count;
    }

    ComponentMask& ComponentMask::operator&=(const ComponentMask& rhs)
    {
        for (int i = 0; i < field.size(); i++)
            field[i] &= rhs.field[i];  return *this;
    }

    ComponentMask& ComponentMask::operator|=(const ComponentMask& rhs)
    {
        for (int i = 0; i < field.size(); i++)
            field[i] |= rhs.field[i];  return *this;
    }

    ComponentMask& ComponentMask::operator^=(const ComponentMask& rhs)
    {
        for (int i = 0; i < field.size(); i++)
            field[i] ^= rhs.field[i];  return *this;
    }

    ComponentMask ComponentMask::operator&(const ComponentMask& rhs)
    {
        ComponentMask t(*this);
        t &= rhs;
        return t;
    }

    ComponentMask ComponentMask::operator|(const ComponentMask& rhs)
    {
        ComponentMask t(*this);
        t |= rhs;
        return t;
    }

    ComponentMask ComponentMask::operator^(const ComponentMask& rhs)
    {
        ComponentMask t(*this);
        t ^= rhs;
        return t;
    }

    bool ComponentMask::operator==(const ComponentMask& rhs) const
    {
        return field == rhs.field;

    }
    int& ComponentMask::GetChunk(int index)
    {
        return field[index / sizeof(int) / 8];
    }

    const int& ComponentMask::GetChunk(int index) const
    {
        return field[index / sizeof(int) / 8];
    }


    ComponentArray::ComponentArray() :
        components(nullptr)
    {}

    ComponentArray::ComponentArray(ComponentArray&& rhs) :
        components(components)
    {
        std::swap(components, rhs.components);
    }

    ComponentArray& ComponentArray::operator=(ComponentArray&& rhs)
    {
        if (this != &rhs)
            std::swap(*this, rhs);

        return *this;
    }

    ComponentArray::~ComponentArray()
    {
        if (components)
        {
            free(components);
            components = nullptr;
        }
    }

    void ComponentArray::insert(void* component, int index, int componentID)
    {
        memcpy(at(index, componentID), component, ComponentInfo::GetByteSize(componentID));
    }

    void ComponentArray::erase(int index, int count, int* size, int componentID)
    {
        memcpy(at(index, componentID), at(index + count, componentID), (*size - count - index) * ComponentInfo::GetByteSize(componentID));
        *size -= count;
    }

    void ComponentArray::reserve(int oldCapacity, int newCapacity, int componentID)
    {
        void* newComponents = realloc(components, newCapacity * ComponentInfo::GetByteSize(componentID));

        if (!newComponents)
        {
            newComponents = malloc(newCapacity * ComponentInfo::GetByteSize(componentID));
            memcpy(newComponents, components, std::min(oldCapacity, newCapacity) * ComponentInfo::GetByteSize(componentID));

            if (components)
                free(components);
        }

        components = newComponents;

        assert(components && "Bad reallocation of component");
    }

    void* ComponentArray::at(int index, int componentID)
    {
        return (char*) components + index * ComponentInfo::GetByteSize(componentID);
    }

    void ComponentArray::set_at(int index, void* component, int componentID)
    {
        memcpy(at(index, componentID), component, ComponentInfo::GetByteSize(componentID));
    }

    void* ComponentArray::data()
    {
        return components;
    }


    Archetype::Archetype() :
        sparseComponentArray(nullptr),
        entityReferences(nullptr),
        entityCount(0),
        entityCapacity(0)
    {}

    Archetype::Archetype(ComponentMask componentMask) :
        componentMask(componentMask),
        entityCount(0),
        entityCapacity(0)
    {
        sparseComponentArray = new ComponentArray[ComponentInfo::GetCount()];
        denseComponentMap.reserve(componentMask.CountSetBits());
        for (int i = 0; i < ComponentInfo::GetCount(); i++)
            if (componentMask.GetBit(i))
                denseComponentMap.push_back(i);
    }

    Archetype::Archetype(Archetype&& rhs) :
        Archetype()
    {
        std::swap(componentMask, rhs.componentMask);
        std::swap(sparseComponentArray, rhs.sparseComponentArray);
        std::swap(entityReferences, rhs.entityReferences);
        std::swap(denseComponentMap, rhs.denseComponentMap);
        std::swap(entityCount, rhs.entityCount);
        std::swap(entityCapacity, rhs.entityCapacity);
    }

    Archetype::Archetype(const Archetype& rhs)
    {
        if (sparseComponentArray)
            delete[] sparseComponentArray;
        if (entityReferences)
            delete[] entityReferences;

        componentMask = rhs.componentMask;
        entityCount = rhs.entityCount;
        entityCapacity = rhs.entityCount;

        denseComponentMap = rhs.denseComponentMap;
        sparseComponentArray = new ComponentArray[ComponentInfo::GetCount()];
        for (int i = 0; i < denseComponentMap.size(); i++)
        {
            int index = denseComponentMap[i];
            sparseComponentArray[index].reserve(0, entityCapacity, index);
            memcpy(sparseComponentArray[index].data(), rhs.sparseComponentArray[index].data(), entityCount * ComponentInfo::GetByteSize(index));
        }

        entityReferences = new Entity * [entityCapacity];
        memcpy(entityReferences, rhs.entityReferences, sizeof(Entity*) * entityCount);
    }

    Archetype& Archetype::operator=(Archetype&& rhs)
    {
        if (this != &rhs)
            std::swap(*this, rhs);

        return *this;
    }

    Archetype& Archetype::operator=(const Archetype& rhs)
    {
        if (this != &rhs)
        {
            Archetype temp(rhs);
            std::swap(*this, temp);
        }

        return *this;
    }

    Archetype::~Archetype()
    {
        if (sparseComponentArray)
        {
            delete[] sparseComponentArray;
            sparseComponentArray = nullptr;
        }

        if (entityReferences)
        {
            delete[] entityReferences;
            entityReferences = nullptr;
        }
    }

    void Archetype::reserve(int newCapacity)
    {
        for (int i = 0; i < denseComponentMap.size(); i++)
        {
            int index = denseComponentMap[i];
            sparseComponentArray[index].reserve(entityCapacity, newCapacity, index);
        }

        Entity** newReferences = new Entity * [newCapacity];
        memcpy(newReferences, entityReferences, sizeof(Entity*) * entityCapacity);
        delete[] entityReferences;
        entityReferences = newReferences;

        entityCapacity = newCapacity;
    }


    Archetype* ArchetypePool::AddArchetype(Archetype&& archetype)
    {
        archetypes.emplace_back(std::move(archetype));
        return &archetypes.back();
    }

    Archetype* ArchetypePool::GetArchetype(ComponentMask mask)
    {
        for (auto&& i : archetypes)
            if (i.componentMask == mask)
                return &i;

        return nullptr;
    }

    std::vector<Archetype*> ArchetypePool::GetContaining(ComponentMask mask)
    {
        std::vector<Archetype*> quarry;
        for (auto&& i : archetypes)
            if ((i.componentMask & mask) == mask)
                quarry.push_back(&i);

        return quarry;
    }
}