#pragma once
#include "PopbackArray.h"
#include <cassert>
#include <iostream>
#include <set>
#include <span>
#include <tuple>
#include <vector>

namespace ECS
{
static consteval int ceil(double num) { return (int)num + (num != int(num)); }
template <typename TComponent> struct Component;
template <typename TComponent>
concept ComponentDerived = std::is_base_of_v<Component<TComponent>, TComponent>;

/// @brief Holds information about components, like byte size, destructors, maximum components, accessed using
/// ComponentIDs.
class ComponentInfo
{
  public:
	typedef void (*MoveConstructorPtr)(void *, void *);
	typedef void (*DestructorPtr)(void *);

  private:
	static std::vector<int> byteSizes;
	static std::vector<MoveConstructorPtr> moveConstructors;
	static std::vector<DestructorPtr> destructors;

  private:
	static int RegisterComponent(int byteSize, MoveConstructorPtr moveConstructor, DestructorPtr destructor);

	/// @brief Registers a component, saving it's byte size and destructor function.
	/// @tparam T Component type
	/// @return unique id, used in GetByteSize and GetDestructor functions
	template <ComponentDerived T> static int RegisterComponent()
	{
		return RegisterComponent(sizeof(T), Component<T>::Move, Component<T>::Destroy);
	}

  public:
	/// @brief Get the number of registered components.
	/// @return number of registered components
	static int GetCount();

	/// @brief Get byte size of a component.
	/// @param id ID of component
	/// @return Byte size of the component
	static int GetByteSize(int id);

	/// @brief Get destructor of a component.
	/// @param id ID of component
	/// @return Destructor of the component.
	static DestructorPtr GetDestructor(int id);

	/// @brief Get move constructor of a component.
	/// @param id ID of component
	/// @return Move constructor of the component.
	static MoveConstructorPtr GetMoveConstructor(int id);

	template <typename T> friend class Component;
};

/// @brief Component class, every component must inherit from this class.
/// Memory address of a component is not guaranteed to be static, and only the destructor is applied to the
/// components. Relocation is done by simply copying the data inside of the component.
/// @tparam T Component that is inheriting from this class (CRTP)
template <typename T> class Component
{
	/// @brief Unique ID used to associate components with things like byte size and destructors.
	const static int ___componentID;

	/// @brief Function invoking child's destructor.
	/// @param component Pointer to child's memory address
	static void Destroy(void *component) { ((T *)component)->~T(); }

	/// @brief Function invoking child's move constructor.
	/// @param destination
	/// @param source
	static void Move(void *destination, void *source) { new ((T *)destination) T(std::move(*(T *)source)); }

	friend ComponentInfo;
	friend class Entity;
	friend class Archetype;
	friend class ArchetypePool;
};
template <typename T> const int Component<T>::___componentID = ComponentInfo::RegisterComponent<T>();

/// @brief Entity class, representing a collection of components.
class Entity
{
  public:
	unsigned int archetypeID;
	unsigned int id;

	/// @brief Constructor with components.
	/// @tparam ...TComponents List of component types that will be added to the entity
	/// @param ...components List of components that will be added to the entity
	template <ComponentDerived... TComponents> Entity(TComponents &&...components);

	Entity();
	Entity(Entity &&rhs);
	Entity &operator=(Entity &&rhs);
	~Entity();

	Entity(const Entity &) = delete;
	Entity &operator=(const Entity &rhs) = delete;

	/// @brief Adds a component to the entity.
	/// @tparam T type of new component
	/// @param component new component
	template <ComponentDerived T> void AddComponent(T &&component);

	/// @brief Removes a component from entity.
	/// @tparam T Type of removed component
	template <ComponentDerived T> void RemoveComponent();

	/// @brief Checks if entity has a component.
	/// @tparam T type of checked component
	/// @return true if component is present, false otherwise
	template <ComponentDerived T> bool HasComponent() const { return HasComponent(T::___componentID); }

	/// @brief Gets a reference to a component from entity.
	/// @tparam T type of component
	/// @return reference to the component
	template <ComponentDerived T> T &GetComponent() { return *(T *)GetComponent(T::___componentID); }

	/// @brief Gets a reference to a component from entity.
	/// @tparam T type of component
	/// @return reference to the component
	template <ComponentDerived T> const T &GetComponent() const { return *(T *)GetComponent(T::___componentID); }

  private:
	bool HasComponent(int componentID) const;
	void *GetComponent(int componentID);
	const void *GetComponent(int componentID) const;

	friend class Archetype;
};

template <ComponentDerived... T> struct Exclude
{
};
template <typename T>
concept Excludion = requires { []<ComponentDerived... U>(Exclude<U...>) {}(std::declval<T>()); };

template <Excludion E, ComponentDerived... T> struct EntityRangeIterator
{
	size_t archetypeID;
	EntityRangeIterator(size_t archetypeID);

	std::tuple<std::span<Entity *>, std::span<T>...> operator*() const;

	EntityRangeIterator &operator++();
	bool operator!=(const EntityRangeIterator &rhs) const;

  private:
	bool IsCurrentArchetypeOk() const;
};

template <Excludion E, ComponentDerived... T> struct EntityRangeView
{
	EntityRangeIterator<E, T...> begin();
	EntityRangeIterator<E, T...> end();
};

template <Excludion E, ComponentDerived... T> struct EntityIterator
{
	EntityRangeIterator<E, T...> entityRange;
	size_t entityID;
	EntityIterator(EntityRangeIterator<E, T...> entityRange, size_t entityID);

	std::tuple<Entity &, T &...> operator*() const;

	EntityIterator &operator++();
	bool operator!=(const EntityIterator &rhs) const;
};

template <Excludion E, ComponentDerived... T> struct EntityView
{
	EntityIterator<E, T...> begin();
	EntityIterator<E, T...> end();
};

template <ComponentDerived... T> static EntityRangeView<Exclude<>, T...> GetComponentsArrays()
{
	return EntityRangeView<Exclude<>, T...>();
}
template <Excludion E, ComponentDerived... T> static EntityRangeView<E, T...> GetComponentsArrays()
{
	return EntityRangeView<E, T...>();
}
template <Excludion E, ComponentDerived... T> static EntityView<E, T...> GetComponents()
{
	return EntityView<E, T...>();
}
template <ComponentDerived... T> static EntityView<Exclude<>, T...> GetComponents()
{
	return EntityView<Exclude<>, T...>();
}

/// @brief Class holding entities with same component types.
struct Archetype
{
	PopbackArray *sparseComponentArray;
	PopbackArray entityReferences;
	std::set<int> denseComponentMap;
	int entityCount;
	int entityCapacity;

	/// @brief Creates a new Archetype with a cpecified mask.
	/// @param componentMask mask of components present in all entities.
	Archetype(const std::set<int> &componentIDs);

	Archetype();
	Archetype(Archetype &&rhs);
	Archetype(const Archetype &rhs) = delete;
	Archetype &operator=(Archetype &&rhs);
	Archetype &operator=(const Archetype &rhs) = delete;
	~Archetype();

	/// @brief Adds entity to the archetype's list, and sets it's data.
	/// @tparam ...TComponents Types of components that entity has
	/// @param entity entity pointer
	/// @param ...components components present on entity
	template <ComponentDerived... TComponents> void Push(Entity *entity, TComponents &&...components);

	/// @brief Moves entity to a new archetype, all components not present in new archetype are destroyed.
	/// @param index position of entity to be moved
	/// @param newArchetype archetype that the entity is moved to
	void MoveEntity(int index, Archetype *newArchetype);

	/// @brief Removes an entity and all it's components.
	/// @param index position of entity to be removed
	void RemoveEntity(int index);

	/// @brief Reserves space for the entities and their components.
	/// @param newCapacity new capacity
	void Reserve(int newCapacity);

	/// @brief Gets a span to specified components.
	/// @tparam T component type
	/// @return span of components of type T, of all entities in archetype.
	template <ComponentDerived T> std::span<T> GetComponents();

	/// @brief Checks whether Archetype stores a component.
	/// @tparam T component type
	/// @return True if stores the component false otherwise.
	template <ComponentDerived T> bool StoresComponent();

	/// @brief Gets a span of entity pointers connected with the archetype
	/// @return span of entity pointers.
	std::span<Entity *> GetEntities();
};

/// @brief Class holding an array of archetypes with unique component masks.
class ArchetypePool
{
	static std::vector<Archetype> archetypes;

  public:
	/// @brief Adds a new archetype, it's component mask has to be unique.
	/// @param archetype archetype to be added
	/// @return pointer to the new archetype
	static Archetype *AddArchetype(Archetype &&archetype);

	static std::span<Archetype> GetArchetypes() { return archetypes; }

	/// @brief Gets archetype by it's mask.
	/// @T param components
	/// @return archetype
	template <ComponentDerived... T> static Archetype *GetArchetype();

	/// @brief Get archetype by it's mask.
	/// @param componentsID mask
	/// @return archetype containing all components specified in mask
	static Archetype *GetArchetype(const std::set<int> &componentsID);

	friend Archetype;
	template <Excludion E, ComponentDerived... T> friend struct EntityRangeIterator;
	template <Excludion E, ComponentDerived... T> friend struct EntityRangeView;
	template <Excludion E, ComponentDerived... T> friend struct EntityView;
};

} // namespace ECS

namespace ECS
{
template <ComponentDerived... TComponents> Entity::Entity(TComponents &&...components) : Entity()
{
	std::set<int> componentsID;
	auto setComponentsAndAssertUnique = [&componentsID](int id) {
		assert(!componentsID.contains(id) && "Trying to add multiple components of same type to an entity");
		componentsID.insert(id);
	};
	((setComponentsAndAssertUnique(TComponents::___componentID)), ...);

	Archetype *archetype = nullptr;
	if (!(archetype = ArchetypePool::GetArchetype(componentsID))) archetype = ArchetypePool::AddArchetype(componentsID);

	archetype->Push(this, std::move(components)...);
}

template <ComponentDerived T> void Entity::AddComponent(T &&component)
{
	assert(!HasComponent<T>() && "Trying to add multiple components of same type to an entity");
	if (archetypeID == -1)
	{
		std::set<int> newComponentIDs;
		newComponentIDs.insert(T::___componentID);

		Archetype *newArchetype = nullptr;
		if (!(newArchetype = ArchetypePool::GetArchetype(newComponentIDs)))
			newArchetype = ArchetypePool::AddArchetype(Archetype(newComponentIDs));
		newArchetype->Push(this, std::move(component));
	}
	else
	{
		Archetype *archetype = &ArchetypePool::GetArchetypes()[archetypeID];

		std::set<int> newComponentIDs = archetype->denseComponentMap;
		newComponentIDs.insert(T::___componentID);

		Archetype *newArchetype = nullptr;
		if (!(newArchetype = ArchetypePool::GetArchetype(newComponentIDs)))
		{
			newArchetype = ArchetypePool::AddArchetype(Archetype(newComponentIDs));
			archetype = &ArchetypePool::GetArchetypes()[archetypeID];
		}

		archetype->MoveEntity(id, newArchetype);
		newArchetype->sparseComponentArray[T::___componentID].emplace_back(component, newArchetype->entityCount);
	}
}

template <ComponentDerived T> void Entity::RemoveComponent()
{
	assert(HasComponent<T>() && "Trying to remove component that is not on an entity");
	Archetype *archetype = &ArchetypePool::GetArchetypes()[archetypeID];
	std::set<int> newComponentIDs = archetype->denseComponentMap;
	newComponentIDs.erase(T::___componentID);

	if (newComponentIDs.empty())
	{
		archetype->RemoveEntity(id);
		id = 0;
		archetypeID = -1;
		return;
	}

	Archetype *newArchetype = nullptr;
	if (!(newArchetype = ArchetypePool::GetArchetype(newComponentIDs)))
	{
		newArchetype = ArchetypePool::AddArchetype(Archetype(newComponentIDs));
		archetype = &ArchetypePool::GetArchetypes()[archetypeID];
	}

	archetype->MoveEntity(id, newArchetype);
}

template <ComponentDerived... TComponents> void Archetype::Push(Entity *entity, TComponents &&...components)
{
	std::set<int> newComponentIDs;
	((newComponentIDs.insert(TComponents::___componentID)), ...);
	assert(newComponentIDs == denseComponentMap && "Archetype component mask does not match provided components");

	if (entityCount + 1 >= entityCapacity) Reserve((entityCapacity + 1) * 1.7);

	((sparseComponentArray[TComponents::___componentID].emplace_back(std::move(components), entityCount)), ...);

	entity->id = entityCount;
	entity->archetypeID = this - &ArchetypePool::archetypes[0];
	entityReferences.append(entity, entityCount);
	entityCount++;
}

template <ComponentDerived T> std::span<T> Archetype::GetComponents()
{
	T *begin = (T *)sparseComponentArray[T::___componentID].data();
	T *end = begin + entityCount;

	return std::span<T>(begin, end);
}

template <ComponentDerived T> bool Archetype::StoresComponent()
{
	return denseComponentMap.contains(T::___componentID);
}

template <Excludion E, ComponentDerived... T>
EntityRangeIterator<E, T...>::EntityRangeIterator(size_t archetypeID) : archetypeID(archetypeID)
{
	if (this->archetypeID < ArchetypePool::archetypes.size())
		while (this->archetypeID < ArchetypePool::archetypes.size() && !IsCurrentArchetypeOk()) ++this->archetypeID;
}

template <Excludion E, ComponentDerived... T>
std::tuple<std::span<Entity *>, std::span<T>...> EntityRangeIterator<E, T...>::operator*() const
{
	return {
		ArchetypePool::archetypes[archetypeID].GetEntities(),
		(ArchetypePool::archetypes[archetypeID].GetComponents<T>())...,
	};
}

template <Excludion E, ComponentDerived... T> EntityRangeIterator<E, T...> &EntityRangeIterator<E, T...>::operator++()
{
	while (++archetypeID < ArchetypePool::archetypes.size() && !IsCurrentArchetypeOk())
		;
	return *this;
}

template <Excludion E, ComponentDerived... T>
bool EntityRangeIterator<E, T...>::operator!=(const EntityRangeIterator &rhs) const
{
	return archetypeID != rhs.archetypeID;
}

template <Excludion E, ComponentDerived... T> bool EntityRangeIterator<E, T...>::IsCurrentArchetypeOk() const
{

	auto handleExcludion = []<ComponentDerived... U>(size_t archetypeID, Exclude<U...> *e) {
		return (!ArchetypePool::archetypes[archetypeID].template StoresComponent<U>() && ...);
	};
	return ArchetypePool::archetypes[archetypeID].entityCount != 0 && handleExcludion(archetypeID, (E *)0) &&
		   (ArchetypePool::archetypes[archetypeID].StoresComponent<T>() && ...);
}

template <Excludion E, ComponentDerived... T> EntityRangeIterator<E, T...> EntityRangeView<E, T...>::begin()
{
	return EntityRangeIterator<E, T...>(0);
}
template <Excludion E, ComponentDerived... T> EntityRangeIterator<E, T...> EntityRangeView<E, T...>::end()
{
	return EntityRangeIterator<E, T...>(ArchetypePool::archetypes.size());
}

template <Excludion E, ComponentDerived... T>
EntityIterator<E, T...>::EntityIterator(EntityRangeIterator<E, T...> entityRange, size_t entityID)
	: entityRange(entityRange), entityID(entityID)
{
}

template <Excludion E, ComponentDerived... T> std::tuple<Entity &, T &...> EntityIterator<E, T...>::operator*() const
{
	return {
		*std::get<0>(*entityRange)[entityID],
		(std::get<std::span<T>>(*entityRange)[entityID])...,
	};
}

template <Excludion E, ComponentDerived... T> EntityIterator<E, T...> &EntityIterator<E, T...>::operator++()
{
	++entityID;
	if (entityID < std::get<0>(*entityRange).size()) return *this;

	entityID = 0;
	++entityRange;
	return *this;
}

template <Excludion E, ComponentDerived... T> bool EntityIterator<E, T...>::operator!=(const EntityIterator &rhs) const
{
	return entityRange != rhs.entityRange || entityID != rhs.entityID;
}

template <Excludion E, ComponentDerived... T> EntityIterator<E, T...> EntityView<E, T...>::begin()
{
	return EntityIterator<E, T...>(EntityRangeView<E, T...>().begin(), 0);
}
template <Excludion E, ComponentDerived... T> EntityIterator<E, T...> EntityView<E, T...>::end()
{
	return EntityIterator<E, T...>(EntityRangeView<E, T...>().end(), 0);
}

template <ComponentDerived... T> Archetype *ArchetypePool::GetArchetype()
{
	std::set<int> ids;
	((ids.insert(T::___componentID)), ...);

	return GetArchetype(ids);
}
} // namespace ECS
