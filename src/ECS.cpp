#include "ECS.h"
#include <math.h>
#include <cstring>
namespace ECS
{
    int ComponentInfo::RegisterComponent(int byteSize, std::function<void(void*)> destructor)
    {
        byteSizes.push_back(byteSize);
        destructors.push_back(destructor);
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
        Entity()
    {
        std::swap(archetypeID, rhs.archetypeID);
        std::swap(id, rhs.id);
    }

    Entity& Entity::operator=(Entity&& rhs)
    {
        if (this != &rhs)
        {
            std::swap(archetypeID, rhs.archetypeID);
            std::swap(id, rhs.id);
        }

        return *this;
    }

    Entity::~Entity()
    {
        if (archetypeID == -1)
            return;

        Archetype& archetype = ArchetypePool::Get(archetypeID);
        archetype.RemoveEntity(id);

        id = 0;
        archetypeID = -1;
    }

    bool Entity::HasComponent(int componentID) const
    {
        return ArchetypePool::Get(archetypeID).componentMask.GetBit(componentID);
    }

    void* Entity::GetComponent(int componentID)
    {
        return ArchetypePool::Get(archetypeID).sparseComponentArray[componentID].at(id, ComponentInfo::GetByteSize(componentID));
    }

    const void* Entity::GetComponent(int componentID) const
    {
        return ArchetypePool::Get(archetypeID).sparseComponentArray[componentID].at(id, ComponentInfo::GetByteSize(componentID));
    }


    ComponentMask::ComponentMask() {}

    ComponentMask::ComponentMask(int componentCount) :
        field(std::vector<int>(ceil(componentCount / (float) sizeof(int) / 8.0f)))
    {}

    void ComponentMask::SetBit(int index)
    {
        GetChunk(index) |= 1 << index;
    }

    void ComponentMask::UnsetBit(int index)
    {
        GetChunk(index) &= ~(1 << index);
    }

    void ComponentMask::ToggleBit(int index)
    {
        GetChunk(index) ^= 1 << index;
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
            field[i] &= rhs.field[i];
        return *this;
    }

    ComponentMask& ComponentMask::operator|=(const ComponentMask& rhs)
    {
        for (int i = 0; i < field.size(); i++)
            field[i] |= rhs.field[i];
        return *this;
    }

    ComponentMask& ComponentMask::operator^=(const ComponentMask& rhs)
    {
        for (int i = 0; i < field.size(); i++)
            field[i] ^= rhs.field[i];
        return *this;
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


    Archetype::Archetype() :
        sparseComponentArray(nullptr),
        entityCount(0),
        entityCapacity(0)
    {}

    Archetype::Archetype(ComponentMask componentMask) :
        componentMask(componentMask),
        entityCount(0),
        entityCapacity(0)
    {
        sparseComponentArray = new PopbackArray[ComponentInfo::GetCount()];
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
        componentMask = rhs.componentMask;
        entityCount = rhs.entityCount;
        entityCapacity = rhs.entityCount;
        denseComponentMap = rhs.denseComponentMap;
        sparseComponentArray = new PopbackArray[ComponentInfo::GetCount()];
        entityReferences.reserve(0, entityCapacity, sizeof(Entity*));

        for (int i = 0; i < denseComponentMap.size(); i++)
        {
            int index = denseComponentMap[i];
            sparseComponentArray[index].reserve(0, entityCapacity, ComponentInfo::GetByteSize(index));
            memcpy(sparseComponentArray[index].data(), rhs.sparseComponentArray[index].data(), entityCount * ComponentInfo::GetByteSize(index));
        }

        memcpy(entityReferences.data(), rhs.entityReferences.data(), sizeof(Entity*) * entityCount);
    }

    Archetype& Archetype::operator=(Archetype&& rhs)
    {
        if (this != &rhs)
        {
            std::swap(componentMask, rhs.componentMask);
            std::swap(sparseComponentArray, rhs.sparseComponentArray);
            std::swap(entityReferences, rhs.entityReferences);
            std::swap(denseComponentMap, rhs.denseComponentMap);
            std::swap(entityCount, rhs.entityCount);
            std::swap(entityCapacity, rhs.entityCapacity);
        }

        return *this;
    }

    Archetype& Archetype::operator=(const Archetype& rhs)
    {
        if (this != &rhs)
        {
            delete[] sparseComponentArray;

            componentMask = rhs.componentMask;
            entityCount = rhs.entityCount;
            entityCapacity = rhs.entityCount;
            denseComponentMap = rhs.denseComponentMap;
            sparseComponentArray = new PopbackArray[ComponentInfo::GetCount()];
            entityReferences.reserve(0, entityCapacity, sizeof(Entity*));

            for (int i = 0; i < denseComponentMap.size(); i++)
            {
                int index = denseComponentMap[i];
                sparseComponentArray[index].reserve(0, entityCapacity, ComponentInfo::GetByteSize(index));
                memcpy(sparseComponentArray[index].data(), rhs.sparseComponentArray[index].data(), entityCount * ComponentInfo::GetByteSize(index));
            }

            memcpy(entityReferences.data(), rhs.entityReferences.data(), sizeof(Entity*) * entityCount);
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
    }

    void Archetype::MoveEntity(int index, Archetype* newArchetype)
    {
        if (newArchetype->entityCount + 1 >= newArchetype->entityCapacity)
            newArchetype->Reserve((newArchetype->entityCapacity + 1) * 1.7);

        for (int i = 0; i < denseComponentMap.size(); i++)
        {
            int componentID = denseComponentMap[i];
            if (newArchetype->componentMask.GetBit(componentID))
                newArchetype->sparseComponentArray[componentID].append(sparseComponentArray->at(index, ComponentInfo::GetByteSize(componentID)), newArchetype->entityCount, ComponentInfo::GetByteSize(componentID));

            sparseComponentArray[componentID].pop(index, entityCount, ComponentInfo::GetByteSize(componentID));
        }

        entityReferences.at<Entity*>(index)->archetypeID = newArchetype - &ArchetypePool::archetypes[0];
        entityReferences.at<Entity*>(index)->id = newArchetype->entityCount;

        newArchetype->entityReferences.append(entityReferences.at<Entity*>(index), newArchetype->entityCount);
        entityReferences.pop<Entity*>(index, entityCount);
        entityReferences.at<Entity*>(index)->id = index;

        entityCount--;
        newArchetype->entityCount++;
    }

    void Archetype::RemoveEntity(int index)
    {
        for (int i = 0; i < denseComponentMap.size(); i++)
        {
            int componentID = denseComponentMap[i];
            sparseComponentArray[componentID].pop(index, entityCount, ComponentInfo::GetByteSize(componentID));
        }
        entityReferences.pop(index, entityCount, sizeof(Entity*));
        Entity& entity = *entityReferences.at<Entity*>(index);
        entity.id = index;
        entityCount--;
    }

    void Archetype::Reserve(int newCapacity)
    {
        for (int i = 0; i < denseComponentMap.size(); i++)
        {
            int index = denseComponentMap[i];
            sparseComponentArray[index].reserve(entityCapacity, newCapacity, ComponentInfo::GetByteSize(index));
        }
        entityReferences.reserve(entityCapacity, newCapacity, sizeof(Entity*));

        entityCapacity = newCapacity;
    }


    Archetype* ArchetypePool::AddArchetype(Archetype&& archetype)
    {
        archetypes.emplace_back(std::move(archetype));
        return &archetypes.back();
    }

    Archetype& ArchetypePool::Get(int index)
    {
        assert(0 <= index && index < ArchetypePool::archetypes.size() && "Invalid Archetype index");

        return archetypes[index];
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