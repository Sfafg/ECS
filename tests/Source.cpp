#include <iostream>
#include <string>
#include "ECS.h"
using namespace ECS;


struct Comp : public Component<Comp>
{
    std::string name;

    Comp(std::string name) : name(name) {}
};

struct Comp1 : public Component<Comp1>
{
    std::string name;

    Comp1(std::string name) : name(name) {}
};
struct Comp2 : public Component<Comp2>
{
    std::string name;

    Comp2(std::string name) : name(name) {}
};

int main()
{
    ECS::AddEntity(Comp("impostor"), Comp2("amogus"));
    ECS::AddEntity(Comp("impostor1"));


    for (int i = 0; i < ArchetypePool::archetypes[0].entityCount; i++)
    {
        std::cout
            << ArchetypePool::archetypes[0].GetComponent<Comp>(i).name << '\n'
            << ArchetypePool::archetypes[0].GetComponent<Comp2>(i).name << '\n';
    }

    for (int i = 0; i < ArchetypePool::archetypes[1].entityCount; i++)
    {
        std::cout
            << ArchetypePool::archetypes[1].GetComponent<Comp>(i).name << '\n';
    }

    return 0;
}