#pragma once
#include <vector>
#include <concepts>
#include <cassert>

namespace ECS
{
    class ComponentInfo
    {
        static std::vector<int> byteSizes;
        static int RegisterComponent(int byteSize);
        template<typename T>
        static int RegisterComponent() { return RegisterComponent(sizeof(T)); }

    public:
        static int GetCount();
        static int GetByteSize(int id);

        template <typename T>
        friend class Component;
    };
    inline std::vector<int> ComponentInfo::byteSizes;

    template <typename T>
    struct Component
    {
        const static int id;
    };
    template <typename T>
    const int Component<T>::id = ComponentInfo::RegisterComponent<T>();

    class Entity
    {
        int archetypeID;
        int id;

    public:
        template<typename... TComponents>
        Entity(TComponents&&... components);

        Entity();
        Entity(Entity&& rhs);
        Entity& operator =(Entity&& rhs);
        ~Entity();

        Entity(const Entity&) = delete;
        Entity& operator =(const Entity& rhs) = delete;

        template <typename T>
        void AddComponent(T&& component) const;
        template <typename T>
        bool HasComponent() const { return HasComponent(T::id); }
        template <typename T>
        T& GetComponent() { return *(T*) GetComponent(T::id); }

        template <typename T>
        const T& GetComponent() const { return *(T*) GetComponent(T::id); }

    private:
        bool HasComponent(int componentID) const;
        void* GetComponent(int componentID);
        const void* GetComponent(int componentID) const;
    };

    class ComponentMask
    {
        std::vector<int> field;
    public:

        ComponentMask();
        ComponentMask(int componentCount);

        void SetBit(int index);
        bool GetBit(int index) const;
        int CountSetBits() const;

        ComponentMask& operator&=(const ComponentMask& rhs);
        ComponentMask& operator|=(const ComponentMask& rhs);
        ComponentMask& operator^=(const ComponentMask& rhs);

        ComponentMask operator&(const ComponentMask& rhs);
        ComponentMask operator|(const ComponentMask& rhs);
        ComponentMask operator^(const ComponentMask& rhs);
        bool operator==(const ComponentMask& rhs) const;

    private:
        int& GetChunk(int index);
        const int& GetChunk(int index) const;
    };

    struct ComponentArray
    {
        void* components;

        ComponentArray();
        ComponentArray(ComponentArray&& rhs);
        ComponentArray& operator=(ComponentArray&& rhs);
        ~ComponentArray();

        ComponentArray(const ComponentArray& rhs) = delete;
        ComponentArray& operator=(const ComponentArray& rhs) = delete;

        void insert(void* component, int index, int componentID);
        void erase(int index, int count, int* size, int componentID);

        void reserve(int oldCapacity, int newCapacity, int componentID);
        void* at(int index, int componentID);
        void set_at(int index, void* component, int componentID);
        void* data();
    };

    struct Archetype
    {
        ComponentMask componentMask;
        ComponentArray* sparseComponentArray;
        Entity** entityReferences;
        std::vector<int> denseComponentMap;
        int entityCount;
        int entityCapacity;

        Archetype();
        Archetype(ComponentMask componentMask);
        Archetype(Archetype&& rhs);
        Archetype(const Archetype& rhs);
        Archetype& operator=(Archetype&& rhs);
        Archetype& operator=(const Archetype& rhs);
        ~Archetype();

        void reserve(int newCapacity);
    };

    struct ArchetypePool
    {
        static std::vector<Archetype> archetypes;

        static Archetype* AddArchetype(Archetype&& archetype);

        static Archetype* GetArchetype(ComponentMask mask);
        static std::vector<Archetype*> GetContaining(ComponentMask mask);
    };
    inline std::vector<Archetype> ArchetypePool::archetypes;

    template<typename... TComponents>
    Entity::Entity(TComponents&&... components)
    {
        ComponentMask mask(ComponentInfo::GetCount());
        ((mask.SetBit(TComponents::id)), ...);
        Archetype* archetype;
        if (!(archetype = ArchetypePool::GetArchetype(mask)))
            archetype = ArchetypePool::AddArchetype(Archetype(mask));

        if (archetype->entityCount + 1 >= archetype->entityCapacity)
            archetype->reserve((archetype->entityCapacity + 1) * 2);

        ((archetype->sparseComponentArray[TComponents::id].insert(&components, archetype->entityCount, TComponents::id)), ...);

        id = archetype->entityCount++;
        archetypeID = archetype - &ArchetypePool::archetypes.front();
    }
}