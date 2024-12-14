#include <iostream>
#include <string>
#include "Profiler/Profiler.cpp"
#include "Profiler/Profiler.h"
#include "ECS.h"
using namespace ECS;

struct Test : public ECS::Component<Test>
{
    float value;
    Test(float value) :value(value) {}
};

struct Test2 : public ECS::Component<Test2>
{
    float value;
    Test2(float value) :value(value) {}
};

ENTITY(Test, Test2);

struct TestSystem : public ECS::System<Test>
{
    static float Run()
    {
        float sum = 0;
        for (auto&& i : components)
        {
            Test2& o = i.GetComponent<Test2>();
            i.value += o.value;
            sum += i.value;
        }

        return 0.1f;
    }
};

int main()
{
    std::vector<Entity> entities(1024 * 1024);
    for (auto&& i : entities)
        i = Entity::AddEntity(Test(0.1f), Test2(0.3f));

    while (true)
    {
        Profiler::BeginFrame();
        {
            PROFILE_NAMED_FUNCTION("Test");
            TestSystem::Run();
        }
        Profiler::EndFrame();
    }

    return 0;
}