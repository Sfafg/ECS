#pragma once
#include "ECSDefs.h"

namespace ECS
{
    template <typename... TSystems, ComponentDerived TComponent>
    TComponent& InvokeAddComponent(std::tuple<TSystems...> tuple, TComponent& component)
    {
        (TSystems::Add(component), ...);

        return component;
    }

    template <typename... TSystems, ComponentDerived TComponent>
    TComponent& InvokeDestroyComponent(std::tuple<TSystems...> tuple, TComponent& component)
    {
        (TSystems::Destroy(component), ...);

        return component;
    }

#pragma region ComponentTable
    template<ComponentDerived... TComponents>
    ComponentTable<TComponents...>::ComponentTable()
    {
        (SetEmpty<TComponents>(), ...);
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    uint64_t ComponentTable<TComponents...>::GetIndex() const
    {
        static_assert(("Component table index type has to be unsigned", std::is_unsigned_v<typename TComponent::TIndex>));
        return std::get<ComponentIndex<TComponent>>(table).index;
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    void ComponentTable<TComponents...>::SetIndex(uint64_t index)
    {
        static_assert(("Component table index type has to be unsigned", std::is_unsigned_v<typename TComponent::TIndex>));
        assert(("Component table index overflow", index < (typename TComponent::TIndex) - 1));
        std::get<ComponentIndex<TComponent>>(table).index = index;
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    void ComponentTable<TComponents...>::SetEmpty()
    {
        static_assert(("Component table index type has to be unsigned", std::is_unsigned_v<typename TComponent::TIndex>));
        std::get<ComponentIndex<TComponent>>(table).index = -1;
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    bool ComponentTable<TComponents...>::IsEmpty()
    {
        static_assert(("Component table index type has to be unsigned", std::is_unsigned_v<typename TComponent::TIndex>));
        return std::get<ComponentIndex<TComponent>>(table).index == (typename TComponent::TIndex) - 1;
    }
#pragma endregion

#pragma region Entity
    template<ComponentDerived... TComponents>
    std::set<EntityTemplate<TComponents...>*> EntityTemplate<TComponents...>::entityReferances;

    template<ComponentDerived... TComponents>
    std::vector<ComponentTable<TComponents...>> EntityTemplate<TComponents...>::entities;

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>::EntityTemplate(uint32_t index) :index(index)
    {
        assert(("Invalid Index", index == -1 || index < entities.size()));
        if (index != -1)
            entityReferances.insert(this);
    }

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>::EntityTemplate(EntityTemplate<TComponents...>&& other) noexcept
    {
        index = other.index;
        entityReferances.erase(&other);
        entityReferances.insert(this);
    }

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>::EntityTemplate(const EntityTemplate<TComponents...>& other)
    {
        index = other.index;
        entityReferances.insert(this);
    }

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>& EntityTemplate<TComponents...>::operator=(EntityTemplate<TComponents...>&& other) noexcept
    {
        if (&other == this) return *this;

        index = other.index;
        entityReferances.erase(&other);
        entityReferances.insert(this);

        return *this;
    }
    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...>& EntityTemplate<TComponents...>::operator=(const EntityTemplate<TComponents...>& other)
    {
        if (&other == this) return *this;

        index = other.index;
        entityReferances.insert(this);

        return *this;
    }

    template<ComponentDerived... TComponents>
    template <typename TEntity, ComponentDerived... UComponents>
    TEntity EntityTemplate<TComponents...>::AddEntity(UComponents&&... components)
    {
        TEntity entity;
        entity.index = entities.size();
        entities.resize(entities.size() + 1);
        entityReferances.insert(&entity);

        (entity.template AddComponent<UComponents>(std::move(components)), ...);

        return entity;
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    TComponent& EntityTemplate<TComponents...>::AddComponent(TComponent&& component)
    {
        assert(("Invalid Entity", this->index != -1));
        assert(("Cannot add multiple components of same type", entities[this->index].template IsEmpty<TComponent>()));
        component.entityIndex = this->index;
        uint64_t index = System<TComponent>::AddComponent(std::move(component));
        entities[this->index].template SetIndex<TComponent>(index);

        return InvokeAddComponent(typename SystemsCollection<TComponent>::Systems(), GetComponent<TComponent>());
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    bool EntityTemplate<TComponents...>::HasComponent() const
    {
        assert(("Invalid Entity", this->index != -1));

        uint64_t index = entities[this->index].template GetIndex<TComponent>();
        return System<TComponent>::IsValid(index);
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    TComponent& EntityTemplate<TComponents...>::GetComponent() const
    {
        assert(("Invalid Entity", this->index != -1));
        assert(("Cannot get nonexistant component", HasComponent<TComponent>()));

        return System<TComponent>::GetComponent(entities[this->index].template GetIndex<TComponent>());
    }

    template<ComponentDerived... TComponents>
    template <ComponentDerived TComponent>
    EntityTemplate<TComponents...>& EntityTemplate<TComponents...>::DestroyComponent()
    {
        assert(("Invalid Entity", this->index != -1));
        assert(("Cannot destroy nonexistant component", HasComponent<TComponent>()));

        auto componentIndex = entities[this->index].template GetIndex<TComponent>();
        System<TComponent>::template DestroyComponent<EntityTemplate<TComponents...>>(componentIndex);
        return *this;
    }

    template<ComponentDerived... TComponents>
    void EntityTemplate<TComponents...>::Destroy()
    {
        assert(("Invalid Entity", this->index != -1));

        uint64_t other = entities.size() - 1;
        uint64_t index = this->index;

        if (other != index)
        {
            EntityTemplate o(other);

            ((o.HasComponent<TComponents>() ? o.GetComponent<TComponents>().entityIndex = index : uint32_t()), ...);

            std::swap(entities[index], entities[other]);
        }
        ((HasComponent<TComponents>() ? DestroyComponent<TComponents>() : *this), ...);
        entities.pop_back();

        std::erase_if(entityReferances,
            [&other, &index](auto& entity) {
                if (entity->index == index)
                {
                    entity->index = -1;
                    return true;
                }


                if (entity->index == other)
                    entity->index = index;

                return false;
            });
    }

    template<ComponentDerived... TComponents>
    EntityTemplate<TComponents...> ::~EntityTemplate()
    {
        if (!entityReferances.contains(this))return;
        for (auto&& referance : entityReferances)
            if (referance != this && referance->index == index)
                return;

        Destroy();
        entityReferances.erase(this);
    }
#pragma endregion

#pragma region Component

    template <typename TComponent>
    Component<TComponent>::Component() :entityIndex(-1) {}

    template <typename TComponent>
    template <typename TEntity>
    TEntity Component<TComponent>::GetEntity() const
    {
        return TEntity(entityIndex);
    }

    template <typename TComponent>
    template <ComponentDerived UComponent, typename TEntity>
    bool Component<TComponent>::HasComponent() const
    {
        return GetEntity<TEntity>().template HasComponent<UComponent>();
    }

    template <typename TComponent>
    template<ComponentDerived UComponent, typename TEntity>
    UComponent& Component<TComponent>::GetComponent() const
    {
        return GetEntity<TEntity>().template GetComponent<UComponent>();
    }

    template <typename TComponent>
    template<ComponentDerived UComponent, typename TEntity>
    UComponent& Component<TComponent>::AddComponent(UComponent&& component)
    {
        return GetEntity<TEntity>().template AddComponent<UComponent>(std::move(component));
    }

    template <typename TComponent>
    template <ComponentDerived UComponent, typename TEntity>
    void Component<TComponent>::DestroyComponent()
    {
        GetEntity<TEntity>().template DestroyComponent<UComponent>();
    }

    template <typename TComponent>
    template<typename TEntity>
    void Component<TComponent>::Destroy()
    {
        GetEntity<TEntity>().template DestroyComponent<TComponent>();
    }
#pragma endregion

#pragma region System
    template <ComponentDerived TComponent>
    std::vector<TComponent> System<TComponent>::components;

    template <ComponentDerived TComponent>
    uint64_t System<TComponent>::AddComponent(TComponent&& component)
    {
        uint64_t index = components.size();
        components.emplace_back(std::move(component));
        assert(("Make sure to call parent assignement operator in component assignment operator", components[index].entityIndex == component.entityIndex));

        return index;
    }

    template <ComponentDerived TComponent>
    bool System<TComponent>::IsValid(uint64_t index)
    {
        return index < components.size();
    }

    template <ComponentDerived TComponent>
    TComponent& System<TComponent>::GetComponent(uint64_t index)
    {
        assert(("Invalid Component", System<TComponent>::IsValid(index)));
        return components[index];
    }

    template <ComponentDerived TComponent>
    template<typename TEntity>
    void System<TComponent>::DestroyComponent(uint64_t index)
    {
        assert(("Invalid Component", System<TComponent>::IsValid(index)));

        InvokeDestroyComponent(typename SystemsCollection<TComponent>::Systems(), components[index]);

        uint64_t other = components.size() - 1;
        TEntity::entities[components[index].entityIndex].template SetEmpty<TComponent>();
        TEntity::entities[components[other].entityIndex].template SetIndex<TComponent>(index);

        components[index] = std::move(components[other]);
        assert(("Make sure to call parent assignement operator in component assignment operator", components[index].entityIndex == components[other].entityIndex));

        components.pop_back();
    }
#pragma endregion

}