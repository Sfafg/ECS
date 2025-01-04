#include <iostream>
#include <string>
#include "ECS.h"
#include "Profiler.h"
#include "Profiler.cpp"
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

int main()
{
    std::vector<Entity> entities(1024 * 4);
    for (int i = 0; i < entities.size(); i++)
    {
        entities[i] = Entity(Particle());
    }

    std::vector<Archetype*> archetypes = ArchetypePool::GetContaining<Particle>();

    while (true)
    {
        Profiler::BeginFrame();
        Profiler::AddFunction("Single Component Archetypes")->BeginSample();

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
        Profiler::GetFunction("Single Component Archetypes")->EndSample();
        Profiler::EndFrame();
    }
    return 0;
}