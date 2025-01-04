#pragma once
#include <iostream>
#include <span>
#include <vector>
#include <functional>
#include <concepts>
#include <cassert>
#include <math.h>
#include "PopbackArray.h"

namespace ECS
{
    template <typename TComponent>
    struct Component;
    template <typename TComponent>
    concept ComponentDerived = std::is_base_of_v<Component<TComponent>, TComponent>;

    class ComponentInfo
    {
        static std::vector<int> byteSizes;
        static std::vector<std::function<void(void*)>> destructors;
        static int RegisterComponent(int byteSize, std::function<void(void*)> destructor);
        template<ComponentDerived T>
        static int RegisterComponent() { return RegisterComponent(sizeof(T), Component<T>::Destroy); }

    public:
        static constexpr int GetMaxComponentCount() { return 32; }
        static constexpr int GetFieldSize() { return (int) ceil(GetMaxComponentCount() / (double) sizeof(int) / 8.0); }
        static int GetCount();
        static int GetByteSize(int id);
        static std::function<void(void*)> GetDestructor(int id);

        template <typename T>
        friend class Component;
    };
    inline std::vector<int> ComponentInfo::byteSizes;
    inline std::vector<std::function<void(void*)>> ComponentInfo::destructors;

    template <typename T>
    class Component
    {
        const static int id;
        static void Destroy(void* component) { ((T*) component)->~T(); }

        friend ComponentInfo;
        friend class Entity;
        friend class Archetype;
    };
    template <typename T>
    const int Component<T>::id = ComponentInfo::RegisterComponent<T>();

    class Entity
    {
        int archetypeID;
        int id;

    public:
        template<ComponentDerived... TComponents>
        Entity(TComponents&&... components);

        Entity();
        Entity(Entity&& rhs);
        Entity& operator =(Entity&& rhs);
        ~Entity();

        Entity(const Entity&) = delete;
        Entity& operator =(const Entity& rhs) = delete;

        template <ComponentDerived T>
        void AddComponent(T&& component) const;
        template <ComponentDerived T>
        void RemoveComponent() const;
        template <ComponentDerived T>
        bool HasComponent() const { return HasComponent(T::id); }
        template <ComponentDerived T>
        T& GetComponent() { return *(T*) GetComponent(T::id); }
        template <ComponentDerived T>
        const T& GetComponent() const { return *(T*) GetComponent(T::id); }

    private:
        bool HasComponent(int componentID) const;
        void* GetComponent(int componentID);
        const void* GetComponent(int componentID) const;

        friend class Archetype;
    };

    class ComponentMask
    {
        int field[ComponentInfo::GetFieldSize()]{};
    public:
        ComponentMask();

        void SetBit(int index);
        void UnsetBit(int index);
        void ToggleBit(int index);
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

    struct Archetype
    {
        ComponentMask componentMask;
        PopbackArray* sparseComponentArray;
        PopbackArray entityReferences;
        std::vector<int> denseComponentMap;
        int entityCount;
        int entityCapacity;

        Archetype(ComponentMask componentMask);

        Archetype();
        Archetype(Archetype&& rhs);
        Archetype(const Archetype& rhs);
        Archetype& operator=(Archetype&& rhs);
        Archetype& operator=(const Archetype& rhs);
        ~Archetype();

        template<ComponentDerived... TComponents>
        void Push(Entity* entity, TComponents&&... components);
        void MoveEntity(int index, Archetype* newArchetype);
        void RemoveEntity(int index);
        void Reserve(int newCapacity);

        template<ComponentDerived T>
        std::span<T> GetComponents()
        {
            assert(componentMask.GetBit(T::id) && "Trying to access components that are not present on archetype");
            return std::span<T>((T*) sparseComponentArray[T::id].data(), entityCount);
        }
    };

    class ArchetypePool
    {
        static std::vector<Archetype> archetypes;

    public:
        static Archetype* AddArchetype(Archetype&& archetype);
        static Archetype& Get(int index);
        static Archetype* GetArchetype(ComponentMask mask);
        static std::vector<Archetype*> GetContaining(ComponentMask mask);
        template<ComponentDerived... TComponents>
        static std::vector<Archetype*> GetContaining()
        {
            ComponentMask mask;
            ((mask.SetBit(TComponents::id)), ...);
            return GetContaining(mask);
        }

        friend Archetype;
    };
    inline std::vector<Archetype> ArchetypePool::archetypes;
}

namespace ECS
{
    template<ComponentDerived... TComponents>
    Entity::Entity(TComponents&&... components) :
        Entity()
    {
        ComponentMask mask;
        auto setBitsAndAssertUnique = [&mask](int id) {
            assert(!mask.GetBit(id) && "Trying to add multiple components of same type to an entity");
            mask.SetBit(id);
            };
        ((setBitsAndAssertUnique(TComponents::id)), ...);

        Archetype* archetype = nullptr;
        if (!(archetype = ArchetypePool::GetArchetype(mask)))
            archetype = ArchetypePool::AddArchetype(Archetype(mask));

        archetype->Push(this, std::move(components)...);
    }

    template <ComponentDerived T>
    void Entity::AddComponent(T&& component) const
    {
        assert(!HasComponent<T>() && "Trying to add multiple components of same type to an entity");
        Archetype& archetype = ArchetypePool::Get(archetypeID);
        ComponentMask newMask = archetype.componentMask;
        newMask.SetBit(T::id);

        Archetype* newArchetype = nullptr;
        if (!(newArchetype = ArchetypePool::GetArchetype(newMask)))
            newArchetype = ArchetypePool::AddArchetype(Archetype(newMask));

        archetype.MoveEntity(id, newArchetype);
        newArchetype->sparseComponentArray[T::id].emplace_back(component, newArchetype->entityCount);
    }

    template <ComponentDerived T>
    void Entity::RemoveComponent() const
    {
        assert(HasComponent<T>() && "Trying to remove component that is not on an entity");
        Archetype& archetype = ArchetypePool::Get(archetypeID);
        ComponentMask newMask = archetype.componentMask;
        newMask.UnsetBit(T::id);

        if (newMask == ComponentMask())
        {
            archetype.RemoveEntity(id);
            id = 0;
            archetypeID = -1;
            return;
        }

        Archetype* newArchetype = nullptr;
        if (!(newArchetype = ArchetypePool::GetArchetype(newMask)))
            newArchetype = ArchetypePool::AddArchetype(Archetype(newMask));

        archetype.MoveEntity(id, newArchetype);
    }

    template<ComponentDerived... TComponents>
    void Archetype::Push(Entity* entity, TComponents&&... components)
    {
        ComponentMask mask;
        ((mask.SetBit(TComponents::id)), ...);
        assert(mask == componentMask && "Archetype component mask does not match provided components");

        if (entityCount + 1 >= entityCapacity)
            Reserve((entityCapacity + 1) * 1.7);

        ((sparseComponentArray[TComponents::id].emplace_back(std::move(components), entityCount)), ...);

        entity->id = entityCount;
        entity->archetypeID = this - &ArchetypePool::archetypes[0];
        entityReferences.append(entity, entityCount);
        entityCount++;
    }
}