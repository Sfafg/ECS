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
    int a;

    Comp1(int a) : a(a) {}
};

struct Comp2 : public Component<Comp2>
{
    float a;

    Comp2(float a) : a(a) {}
};

int main()
{
    Entity a(Comp("sus"));
    Entity b(Comp("sus2"));
    Entity c(Comp("sus3"));
    b.~Entity();
    // a.AddComponent(Comp1(10));
    std::cout << a.GetComponent<Comp>().name << '\n';
    std::cout << b.GetComponent<Comp>().name << '\n';
    std::cout << c.GetComponent<Comp>().name << '\n';

    return 0;
}