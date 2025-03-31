#include "ECS.h"
#include "math.h"
#include <iostream>
#include <string>

using namespace ECS;

struct Name : public Component<Name>
{
	std::string name;

	Name(std::string&& name) : name(std::move(name)) {}

	Name(Name&& rhs) noexcept : name(std::move(rhs.name)) {}

	Name& operator=(Name&& rhs)
	{
		if (&rhs != this) std::swap(name, rhs.name);
		return *this;
	}
	~Name() { std::cout << "Destroy Name" << name << '\n'; }
};

struct Test : public Component<Test>
{
	std::string name;

	Test(std::string&& name) : name(std::move(name)) {}

	Test(Test&& rhs) noexcept : name(std::move(rhs.name)) {}

	Test& operator=(Test&& rhs)
	{
		if (&rhs != this) std::swap(name, rhs.name);
		return *this;
	}
	~Test() { std::cout << "Destroy Test" << name << '\n'; }
};

int main()
{
	{
		Entity a;
		a.AddComponent(Name("Test"));
		std::cout << a.HasComponent<Name>() << '\n';
		std::cout << a.GetComponent<Name>().name << '\n';
		a.RemoveComponent<Name>();
		a.AddComponent(Name("Test"));
		std::cout << a.HasComponent<Name>() << '\n';
		std::cout << a.GetComponent<Name>().name << '\n';
	}
	Entity a(Name("A"));
	std::cout << '\n' << '\n';
	std::cout << a.id << '\n';
	{
		Entity b(Name("B"));
		Entity c(Name("C"), Test("C"));
		Entity d(Name("D"));


		for (auto&& i : ArchetypePool::GetContaining<Name, Test>())
			std::cout << i.name << '\n';

		c.~Entity();

		for (auto&& i : ArchetypePool::GetContaining<Name, Test>())
			std::cout << i.name << '\n';

	}

	std::cout << '\n' << '\n';
	std::cout << a.id << '\n';
	{
		Entity* b = new Entity(Test("C"));

	}
	std::cout << '\n' << '\n';
	std::cout << a.id << '\n';
	return 0;
}
