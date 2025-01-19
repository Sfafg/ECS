#include <iostream>
#include <string>
#include "ECS.h"
#include "math.h"

using namespace ECS;

struct vec2
{
    float x, y;
    vec2() :x(0), y(0) {}
    vec2(float x, float y) :x(x), y(y) {}
    void operator+=(const vec2& rhs) { x += rhs.x; y += rhs.y; }
    vec2 operator+(const vec2& rhs) { return vec2(x + rhs.x, y + rhs.y); }
    vec2 operator-(const vec2& rhs) { return vec2(x - rhs.x, y - rhs.y); }
    vec2 operator-() { return vec2(-x, -y); }

    float sqrLength()
    {
        return x * x + y * y;

    }
    float length()
    {
        return sqrt(sqrLength());
    }
    void normalize()
    {
        float l = length();
        if (l == 0) return;
        x /= l;
        y /= l;
    }
};

struct Particle : public Component<Particle>
{
    vec2 position;
    vec2 velocity;

    Particle() :position((float) rand() / (float) RAND_MAX * 10 - 5, (float) rand() / (float) RAND_MAX * 10 - 5) {}
    Particle(vec2 position, vec2 velocity) :position(position), velocity(velocity) {}
};

struct Name : public Component<Name>
{
    std::string name;

    Name(std::string&& name) :name(std::move(name)) {}

    Name(Name&& rhs) noexcept :
        name(std::move(rhs.name))
    {}

    Name& operator=(Name&& rhs)
    {
        if (&rhs != this)
            std::swap(name, rhs.name);
        return *this;
    }
    ~Name()
    {
        std::cout << "Destroy " << name << '\n';
    }
};

int main()
{
    Entity a(Name("A"));
    {
        Entity b(Name("B"));
        Entity c(Name("C"));
        Entity d(Name("D"));

        std::cout << a.GetComponent<Name>().name << '\n';
        std::cout << b.GetComponent<Name>().name << '\n';
        std::cout << c.GetComponent<Name>().name << '\n';
        std::cout << d.GetComponent<Name>().name << '\n';
        d.~Entity();
        std::cout << a.GetComponent<Name>().name << '\n';
        std::cout << b.GetComponent<Name>().name << '\n';
        std::cout << c.GetComponent<Name>().name << '\n';
    }

    std::vector<Entity> entities(2);
    for (int i = 0; i < entities.size(); i++)
        entities[i] = Entity(Particle());

    return 0;
    std::vector<Archetype*> archetypes = ArchetypePool::GetContaining<Particle>();

    int abc = 0;
    while (abc++ != 10)
    {
        for (auto&& i : archetypes)
        {
            for (auto&& particleA : i->GetComponents<Particle>())
            {
                vec2 acceleration;

                for (auto&& i_ : archetypes)
                {
                    for (auto&& particleB : i_->GetComponents<Particle>())
                    {
                        acceleration += particleB.position - particleA.position;
                    }
                }

                acceleration += -particleA.position;

                particleA.velocity += acceleration;
                particleA.position += particleA.velocity;
            }
        }
    }
    return 0;
}