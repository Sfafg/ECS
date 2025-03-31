#include "ECS.h"
#include <algorithm>
#include <cstring>
namespace ECS
{
	std::vector<int> ComponentInfo::byteSizes = {};
	std::vector<ComponentInfo::MoveConstructorPtr> ComponentInfo::moveConstructors = {};
	std::vector<ComponentInfo::DestructorPtr> ComponentInfo::destructors = {};

	int ComponentInfo::RegisterComponent(int byteSize, ComponentInfo::MoveConstructorPtr moveConstructor,
		ComponentInfo::DestructorPtr destructor)
	{
		byteSizes.push_back(byteSize);
		moveConstructors.push_back(moveConstructor);
		destructors.push_back(destructor);
		return byteSizes.size() - 1;
	}

	int ComponentInfo::GetCount() { return byteSizes.size(); }

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

	Entity::Entity() : archetypeID(-1), id(0) {}

	Entity::Entity(Entity&& rhs) : Entity()
	{
		std::swap(archetypeID, rhs.archetypeID);
		std::swap(id, rhs.id);
		if (archetypeID != -1) ArchetypePool::GetArchetypes()[archetypeID].entityReferences.at<Entity*>(id) = this;
	}

	Entity& Entity::operator=(Entity&& rhs)
	{
		if (this != &rhs)
		{
			std::swap(archetypeID, rhs.archetypeID);
			std::swap(id, rhs.id);
			if (archetypeID != -1) ArchetypePool::GetArchetypes()[archetypeID].entityReferences.at<Entity*>(id) = this;
		}

		return *this;
	}

	Entity::~Entity()
	{
		if (archetypeID == -1) return;

		Archetype& archetype = ArchetypePool::GetArchetypes()[archetypeID];
		archetype.RemoveEntity(id);

		id = 0;
		archetypeID = -1;
	}

	bool Entity::HasComponent(int componentID) const
	{
		if (archetypeID >= ArchetypePool::GetArchetypes().size()) return false;
		return ArchetypePool::GetArchetypes()[archetypeID].denseComponentMap.contains(componentID);
	}

	void* Entity::GetComponent(int componentID)
	{
		if (archetypeID >= ArchetypePool::GetArchetypes().size()) return nullptr;
		return ArchetypePool::GetArchetypes()[archetypeID].sparseComponentArray[componentID].at(
			id, ComponentInfo::GetByteSize(componentID));
	}

	const void* Entity::GetComponent(int componentID) const
	{
		if (archetypeID >= ArchetypePool::GetArchetypes().size()) return nullptr;
		return ArchetypePool::GetArchetypes()[archetypeID].sparseComponentArray[componentID].at(
			id, ComponentInfo::GetByteSize(componentID));
	}

	Archetype::Archetype() : sparseComponentArray(nullptr), entityCount(0), entityCapacity(0) {}

	Archetype::Archetype(const std::set<int>& componentIDs)
		: entityCount(0), entityCapacity(0), denseComponentMap(componentIDs)
	{
		int max = 0;
		for (auto&& i : componentIDs)
			max = i + 1 > max ? i + 1 : max;

		sparseComponentArray = new PopbackArray[max];
	}

	Archetype::Archetype(Archetype&& rhs) : Archetype()
	{
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
			for (auto i = denseComponentMap.begin(); i != denseComponentMap.end(); i++)
			{
				int componentID = *i;
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

		for (auto i = denseComponentMap.begin(); i != denseComponentMap.end(); i++)
		{
			int componentID = *i;
			int byteSize = ComponentInfo::GetByteSize(componentID);
			auto moveConstructor = ComponentInfo::GetMoveConstructor(componentID);

			if (newArchetype->denseComponentMap.contains(componentID))
				newArchetype->sparseComponentArray[componentID].append(
					sparseComponentArray->at(index, byteSize), newArchetype->entityCount, byteSize, moveConstructor);
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
		for (auto i = denseComponentMap.begin(); i != denseComponentMap.end(); i++)
		{
			int componentID = *i;
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
		for (auto i = denseComponentMap.begin(); i != denseComponentMap.end(); i++)
		{
			int componentID = *i;
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
		auto it = std::find_if(archetypes.begin(), archetypes.end(), [&archetype](const Archetype& a) {
			return archetype.denseComponentMap == a.denseComponentMap;
			});
		assert(it == archetypes.end() && "Trying to add archetype with non unique component mask");

		archetypes.emplace_back(std::move(archetype));
		return &archetypes.back();
	}

	Archetype* ArchetypePool::GetArchetype(const std::set<int>& componentsID)
	{
		for (auto i = archetypes.begin(); i != archetypes.end(); i++)
			if (i->denseComponentMap == componentsID) return &i[0];
		return nullptr;
	}

	std::vector<Archetype*> ArchetypePool::GetContaining(const std::set<int>& componentsID)
	{
		std::vector<Archetype*> results;
		for (auto i = archetypes.begin(); i != archetypes.end(); i++)
		{
			if (i->denseComponentMap.size() < componentsID.size()) continue;

			bool containsAll = true;
			for (auto j = componentsID.begin(); j != componentsID.end(); j++)
			{
				if (!i->denseComponentMap.contains(*j))
				{
					containsAll = false;
					break;
				}
			}
			if (containsAll) results.push_back(&i[0]);
		}
		return results;
	}
} // namespace ECS
