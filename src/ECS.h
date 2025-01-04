#pragma once
#include <iostream>
#include <span>
#include <vector>
#include <functional>
#include <concepts>
#include <cassert>
#include "PopbackArray.h"

namespace ECS
{
    template <typename T>
    concept DestructableComponent = requires {
        { T::Destroy() };
    };
    class ComponentInfo
    {
        static std::vector<int> byteSizes;
        static std::vector<std::function<void(void*)>> destructors;
        static int RegisterComponent(int byteSize, std::function<void(void*)> destructor);
        template<typename T>
        static int RegisterComponent() { return RegisterComponent(sizeof(T), nullptr); }
        template<DestructableComponent T>
        static int RegisterComponent() { return RegisterComponent(sizeof(T), T::Destroy); }

    public:
        static int GetCount();
        static int GetByteSize(int id);

        template <typename T>
        friend class Component;
    };
    inline std::vector<int> ComponentInfo::byteSizes;
    inline std::vector<std::function<void(void*)>> ComponentInfo::destructors;

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
        void RemoveComponent() const;
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

        friend class Archetype;
    };

    class ComponentMask
    {
        std::vector<int> field;
    public:
        ComponentMask();
        ComponentMask(int componentCount);

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

        Archetype();
        Archetype(ComponentMask componentMask);
        Archetype(Archetype&& rhs);
        Archetype(const Archetype& rhs);
        Archetype& operator=(Archetype&& rhs);
        Archetype& operator=(const Archetype& rhs);
        ~Archetype();

        template<typename... TComponents>
        void Push(Entity* entity, TComponents... components);
        void MoveEntity(int index, Archetype* newArchetype);
        void RemoveEntity(int index);
        void Reserve(int newCapacity);

        template<typename T>
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
        template<typename... TComponents>
        static std::vector<Archetype*> GetContaining()
        {
            ComponentMask mask(ComponentInfo::GetCount());
            ((mask.SetBit(TComponents::id)), ...);
            return GetContaining(mask);
        }

        friend Archetype;
    };
    inline std::vector<Archetype> ArchetypePool::archetypes;
}

namespace ECS
{
    template<typename... TComponents>
    Entity::Entity(TComponents&&... components) :
        Entity()
    {
        ComponentMask mask(ComponentInfo::GetCount());
        ((mask.SetBit(TComponents::id)), ...);

        Archetype* archetype = nullptr;
        if (!(archetype = ArchetypePool::GetArchetype(mask)))
            archetype = ArchetypePool::AddArchetype(Archetype(mask));

        archetype->Push(this, std::move(components)...);
    }

    template <typename T>
    void Entity::AddComponent(T&& component) const
    {
        Archetype& archetype = ArchetypePool::Get(archetypeID);
        ComponentMask newMask = archetype.componentMask;
        newMask.SetBit(T::id);

        Archetype* newArchetype = nullptr;
        if (!(newArchetype = ArchetypePool::GetArchetype(newMask)))
            newArchetype = ArchetypePool::AddArchetype(Archetype(newMask));

        archetype.MoveEntity(id, newArchetype);
        newArchetype->sparseComponentArray[T::id].append(&component, newArchetype->entityCount, ComponentInfo::GetByteSize(T::id));
    }

    template <typename T>
    void Entity::RemoveComponent() const
    {
        Archetype& archetype = ArchetypePool::Get(archetypeID);
        ComponentMask newMask = archetype.componentMask;
        newMask.UnsetBit(T::id);

        Archetype* newArchetype = nullptr;
        if (!(newArchetype = ArchetypePool::GetArchetype(newMask)))
            newArchetype = ArchetypePool::AddArchetype(Archetype(newMask));

        archetype.MoveEntity(id, newArchetype);
    }

    template<typename... TComponents>
    void Archetype::Push(Entity* entity, TComponents... components)
    {
        ComponentMask mask(ComponentInfo::GetCount());
        ((mask.SetBit(TComponents::id)), ...);
        assert(mask == componentMask && "Archetype component mask does not match provided components");

        if (entityCount + 1 >= entityCapacity)
            Reserve((entityCapacity + 1) * 1.7);

        ((sparseComponentArray[TComponents::id].append(&components, entityCount, ComponentInfo::GetByteSize(TComponents::id))), ...);

        entity->id = entityCount;
        entity->archetypeID = this - &ArchetypePool::archetypes[0];
        entityReferences.append(entity, entityCount);
        entityCount++;
    }
}