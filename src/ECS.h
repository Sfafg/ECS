#pragma once
#include "PopbackArray.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <set>
#include <span>
#include <tuple>
#include <vector>

namespace ECS
{
static consteval int ceil(double num) { return (int)num + (num != int(num)); }
template <typename T> class MultiIterator
{
	std::vector<std::span<T>> containers;
	unsigned int currContainer = 0;
	unsigned int currElement = 0;

  public:
	MultiIterator() {}
	MultiIterator(const std::vector<std::span<T>> &containers) : containers(containers) {}

	T &operator*() { return containers[currContainer][currElement]; }
	MultiIterator begin() { return MultiIterator(containers); }
	MultiIterator end()
	{
		MultiIterator r(containers);
		r.currContainer = containers.size();
		r.currElement = 0;
		return r;
	}
	MultiIterator &operator++()
	{
		currElement++;
		if (containers[currContainer].size() <= currElement)
		{
			currElement = 0;
			currContainer++;
		};

		return *this;
	}

	bool operator!=(const MultiIterator &it) const
	{
		if (containers.size() != it.containers.size() || currContainer != it.currContainer ||
			currElement != it.currElement)
			return true;

		for (int i = 0; i < containers.size(); i++)
		{
			if (containers[i].begin() != it.containers[i].begin()) return true;
		}
		return false;
	}

	friend class ArchetypePool;
};

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
	const static int id;

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
template <typename T> const int Component<T>::id = ComponentInfo::RegisterComponent<T>();

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
	template <ComponentDerived T> bool HasComponent() const { return HasComponent(T::id); }

	/// @brief Gets a reference to a component from entity.
	/// @tparam T type of component
	/// @return reference to the component
	template <ComponentDerived T> T &GetComponent() { return *(T *)GetComponent(T::id); }

	/// @brief Gets a reference to a component from entity.
	/// @tparam T type of component
	/// @return reference to the component
	template <ComponentDerived T> const T &GetComponent() const { return *(T *)GetComponent(T::id); }

  private:
	bool HasComponent(int componentID) const;
	void *GetComponent(int componentID);
	const void *GetComponent(int componentID) const;

	friend class Archetype;
};

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
	template <ComponentDerived... T> static Archetype *GetArchetype()
	{
		std::set<int> ids;
		((ids.insert(T::id)), ...);

		return GetArchetype(ids);
	}

	/// @brief Get archetype by it's mask.
	/// @param componentsID mask
	/// @return archetype containing all components specified in mask
	static Archetype *GetArchetype(const std::set<int> &componentsID);

	template <ComponentDerived U> static MultiIterator<U> GetFiltered(std::function<bool(const Archetype &)> filter)
	{
		MultiIterator<U> results;
		for (auto &&i : GetArchetypes())
			if (filter(i)) results.containers.push_back(i.GetComponents<U>());

		return results;
	}

	/// @brief Gets all archetypes containing all components specified in mask.
	/// @T param mask
	/// @return archetypes containing all components specified in mask
	template <ComponentDerived U, ComponentDerived... T> static MultiIterator<U> GetContaining()
	{
		std::set<int> ids;
		ids.insert(U::id);
		((ids.insert(T::id)), ...);

		auto arch = GetContaining(ids);

		MultiIterator<U> results;
		for (auto &&i : arch) results.containers.push_back(i->GetComponents<U>());

		return results;
	}

	/// @brief Gets all archetypes containing all components specified in mask.
	/// @param componentsID mask
	/// @return archetypes containing all components specified in mask
	static std::vector<Archetype *> GetContaining(const std::set<int> &componentsID);

	friend Archetype;
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
	((setComponentsAndAssertUnique(TComponents::id)), ...);

	Archetype *archetype = nullptr;
	if (!(archetype = ArchetypePool::GetArchetype<TComponents...>()))
		archetype = ArchetypePool::AddArchetype(componentsID);

	archetype->Push(this, std::move(components)...);
}

template <ComponentDerived T> void Entity::AddComponent(T &&component)
{
	assert(!HasComponent<T>() && "Trying to add multiple components of same type to an entity");
	if (archetypeID == -1)
	{
		std::set<int> newComponentIDs;
		newComponentIDs.insert(T::id);

		Archetype *newArchetype = nullptr;
		if (!(newArchetype = ArchetypePool::GetArchetype(newComponentIDs)))
			newArchetype = ArchetypePool::AddArchetype(Archetype(newComponentIDs));
		newArchetype->Push(this, std::move(component));
	}
	else
	{
		Archetype &archetype = ArchetypePool::GetArchetypes()[archetypeID];

		std::set<int> newComponentIDs = archetype.denseComponentMap;
		newComponentIDs.insert(T::id);

		Archetype *newArchetype = nullptr;
		if (!(newArchetype = ArchetypePool::GetArchetype(newComponentIDs)))
			newArchetype = ArchetypePool::AddArchetype(Archetype(newComponentIDs));

		archetype.MoveEntity(id, newArchetype);
		newArchetype->sparseComponentArray[T::id].emplace_back(component, newArchetype->entityCount);
	}
}

template <ComponentDerived T> void Entity::RemoveComponent()
{
	assert(HasComponent<T>() && "Trying to remove component that is not on an entity");
	Archetype &archetype = ArchetypePool::GetArchetypes()[archetypeID];
	std::set<int> newComponentIDs = archetype.denseComponentMap;
	newComponentIDs.erase(T::id);

	if (newComponentIDs.empty())
	{
		archetype.RemoveEntity(id);
		id = 0;
		archetypeID = -1;
		return;
	}

	Archetype *newArchetype = nullptr;
	if (!(newArchetype = ArchetypePool::GetArchetype(newComponentIDs)))
		newArchetype = ArchetypePool::AddArchetype(Archetype(newComponentIDs));

	archetype.MoveEntity(id, newArchetype);
}

template <ComponentDerived... TComponents> void Archetype::Push(Entity *entity, TComponents &&...components)
{
	std::set<int> newComponentIDs;
	((newComponentIDs.insert(TComponents::id)), ...);
	assert(newComponentIDs == denseComponentMap && "Archetype component mask does not match provided components");

	if (entityCount + 1 >= entityCapacity) Reserve((entityCapacity + 1) * 1.7);

	((sparseComponentArray[TComponents::id].emplace_back(std::move(components), entityCount)), ...);

	entity->id = entityCount;
	entity->archetypeID = this - &ArchetypePool::archetypes[0];
	entityReferences.append(entity, entityCount);
	entityCount++;
}
template <ComponentDerived T> std::span<T> Archetype::GetComponents()
{
	T *begin = (T *)sparseComponentArray[T::id].data();
	T *end = begin + entityCount;
	return std::span<T>(begin, end);
}
} // namespace ECS
