#include "ECS.h"
#include <cstring>
#include <algorithm>
namespace ECS
{
    std::vector<int> ComponentInfo::byteSizes = {};
    std::vector<ComponentInfo::MoveConstructorPtr> ComponentInfo::moveConstructors = {};
    std::vector<ComponentInfo::DestructorPtr> ComponentInfo::destructors = {};

    int ComponentInfo::RegisterComponent(int byteSize, ComponentInfo::MoveConstructorPtr moveConstructor, ComponentInfo::DestructorPtr destructor)
    {
        byteSizes.push_back(byteSize);
        moveConstructors.push_back(moveConstructor);
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

    ComponentInfo::DestructorPtr ComponentInfo::GetDestructor(int id)
    {
        assert(0 <= id && id < destructors.size() && "Invalid Component ID");
        return destructors[id];
    }

    ComponentInfo::MoveConstructorPtr ComponentInfo::GetMoveConstructor(int id)
    {
        assert(0 <= id && id < moveConstructors.size() && "Invalid Component ID");
        return moveConstructors[id];
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
        if (archetypeID != -1)
            ArchetypePool::Get(archetypeID).entityReferences.at<Entity*>(id) = this;
    }

    Entity& Entity::operator=(Entity&& rhs)
    {
        if (this != &rhs)
        {
            std::swap(archetypeID, rhs.archetypeID);
            std::swap(id, rhs.id);
            if (archetypeID != -1)
                ArchetypePool::Get(archetypeID).entityReferences.at<Entity*>(id) = this;
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
        for (int i = 0; i < fieldSize; i++)
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
        for (int i = 0; i < fieldSize; i++)
            field[i] &= rhs.field[i];
        return *this;
    }

    ComponentMask& ComponentMask::operator|=(const ComponentMask& rhs)
    {
        for (int i = 0; i < fieldSize; i++)
            field[i] |= rhs.field[i];
        return *this;
    }

    ComponentMask& ComponentMask::operator^=(const ComponentMask& rhs)
    {
        for (int i = 0; i < fieldSize; i++)
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
        for (int i = 0; i < fieldSize; i++)
            if (field[i] != rhs.field[i])
                return false;

        return true;
    }
    int& ComponentMask::GetChunk(int index)
    {
        assert((index < ComponentInfo::maxComponentCount) && "To much components set max component count higher");
        return field[index / sizeof(int) / 8];
    }

    const int& ComponentMask::GetChunk(int index) const
    {
        assert((index < ComponentInfo::maxComponentCount) && "To much components set max component count higher");
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

    Archetype::~Archetype()
    {
        if (sparseComponentArray)
        {
            for (int i = 0; i < denseComponentMap.size(); i++)
            {
                int componentID = denseComponentMap[i];
                int byteSize = ComponentInfo::GetByteSize(componentID);
                auto moveConstructor = ComponentInfo::GetMoveConstructor(componentID);
                for (int j = 0; j < entityCount; j++)
                {
                    void* component = sparseComponentArray[componentID].at(j, byteSize);
                    ComponentInfo::GetDestructor(componentID)(component);
                }
            }
            entityCount = 0;
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
            int byteSize = ComponentInfo::GetByteSize(componentID);
            auto moveConstructor = ComponentInfo::GetMoveConstructor(componentID);

            if (newArchetype->componentMask.GetBit(componentID))
                newArchetype->sparseComponentArray[componentID].append(sparseComponentArray->at(index, byteSize), newArchetype->entityCount, byteSize, moveConstructor);
            else
            {
                void* component = sparseComponentArray[componentID].at(index, byteSize);
                ComponentInfo::GetDestructor(componentID)(component);
            }
            sparseComponentArray[componentID].pop(index, entityCount, byteSize, moveConstructor);
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
            int byteSize = ComponentInfo::GetByteSize(componentID);
            auto moveConstructor = ComponentInfo::GetMoveConstructor(componentID);

            void* component = sparseComponentArray[componentID].at(index, byteSize);
            ComponentInfo::GetDestructor(componentID)(component);
            sparseComponentArray[componentID].pop(index, entityCount, byteSize, moveConstructor);
        }
        void* p = sparseComponentArray[0].data();
        entityReferences.pop(index, entityCount, sizeof(Entity*));
        Entity& entity = *entityReferences.at<Entity*>(index);
        entity.id = index;
        entityCount--;
    }

    void Archetype::Reserve(int newCapacity)
    {
        for (int i = 0; i < denseComponentMap.size(); i++)
        {
            int componentID = denseComponentMap[i];
            int byteSize = ComponentInfo::GetByteSize(componentID);
            auto moveConstructor = ComponentInfo::GetMoveConstructor(componentID);

            sparseComponentArray[componentID].reserve(entityCapacity, newCapacity, byteSize, moveConstructor);
        }
        entityReferences.reserve(entityCapacity, newCapacity, sizeof(Entity*));

        entityCapacity = newCapacity;
    }

    std::vector<Archetype> ArchetypePool::archetypes = {};

    Archetype* ArchetypePool::AddArchetype(Archetype&& archetype)
    {
        auto it = std::find_if(archetypes.begin(), archetypes.end(), [&archetype](const Archetype& a) {return archetype.componentMask == a.componentMask; });
        assert(it == archetypes.end() && "Trying to add archetype with non unique component mask");

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