#include "ECS.h"
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
using namespace std::chrono_literals;
using namespace std::chrono;
using namespace ECS;

struct Name : public Component<Name>
{
	int id;

	Name(int id) : id(id) {}
};

struct Test : public Component<Test>
{
	int id;
	Test(int id) : id(id) {}
};

struct Particle : public Component<Particle>
{
	float x, y, vx, vy;

	Particle() : x(0), y(0), vx(0), vy(0) {}
	Particle(float x, float y) : x(x), y(y), vx(0), vy(0) {}

	bool operator!=(const Particle &rhs) const { return x != rhs.x || y != rhs.y || vx != rhs.vx || vy != rhs.vy; }
};

struct FrictionConstraint : public Component<FrictionConstraint>
{
	float frictionCeofficient;
	FrictionConstraint(float frictionCeofficient) : frictionCeofficient(frictionCeofficient) {}
};

struct BoxConstraint : public Component<BoxConstraint>
{
	float w, h;
	BoxConstraint(float w = 0.0f, float h = 0.0f) : w(w), h(h) {}
};

int main()
{
	{
		std::vector<Entity> entities;
		entities.push_back(Entity(Name(4)));
		entities.erase(entities.end() - 1);
		entities.push_back(Entity(Name(4)));
		entities.push_back(Entity(Name(5)));
		entities.push_back(Entity(Name(6), Test(1)));
		entities.push_back(Entity(Test(2)));
		entities.erase(entities.end() - 1);
		entities.erase(entities.end() - 1);
		entities.push_back(Entity(Name(6), Test(1)));
		entities.push_back(Entity(Name(2), Test(2)));
		entities.back().RemoveComponent<Name>();
		entities.back().RemoveComponent<Test>();
		entities.back().AddComponent(Test(2));

		int nameIds[] = {4, 5, 6};
		int testIds[] = {1, 2};

		int i = 0;
		for (auto &&[name] : GetComponents<Name>())
			if (nameIds[i++] != name.id)
			{
				std::cout << "Failed Name at ID: " << name.id << " should be " << nameIds[i - 1] << '\n';
				return 1;
			}
		i = 0;
		for (auto &&[test] : GetComponents<Test>())
			if (testIds[i++] != test.id)
			{
				std::cout << "Failed Test at ID: " << test.id << " should be " << testIds[i - 1] << '\n';
				return 1;
			}

		for (auto &&[name, test] : GetComponents<Name, Test>())
			if (test.id != 1 || name.id != 6)
			{
				std::cout << "Failed Test and Name at ID: " << test.id << ", " << name.id << " should be 1, 6 " << '\n';
				return 1;
			}

		i = 0;
		for (auto &&[test] : GetComponents<Exclude<Name>, Test>())
			if (2 != test.id)
			{
				std::cout << "Failed Excluded Iteration Test at ID: " << test.id << " should be " << 2 << '\n';
				return 1;
			}

		entities.erase(entities.end() - 2);
		for (auto &&[name, test] : GetComponents<Name, Test>())
		{
			std::cout << "Failed iterating over empty component set " << name.id << ", " << test.id << "\n";
			return 1;
		}
	}

	// Performance test.
	const int particleCount = 1024 * 32;
	const int iterationCount = 1024;

	std::cout << "Ground truth: \n";
	Particle particles[particleCount];
	{
		auto start = high_resolution_clock::now();
		srand(0);
		for (int i = 0; i < particleCount; i++)
			particles[i] = Particle(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX);
		auto end = high_resolution_clock::now();
		std::cout << "\tSetup time " << (end - start).count() / 1000000.0 << "ms\n";

		start = high_resolution_clock::now();
		for (int n = 0; n < iterationCount; n++)
		{
			for (int i = 0; i < particleCount; i++)
			{
				Particle &p = particles[i];
				p.vy -= p.y * 0.1;
				p.vx -= p.x * 0.1;
				p.x += p.vx;
				p.y += p.vy;
			}
		}
		end = high_resolution_clock::now();
		std::cout << "\tRun time " << (end - start).count() / 1000000.0 << "ms\n";
	}

	{
		std::cout << "\nMy ECS with all Extra Components: \n";
		Entity entities[particleCount];
		{
			auto start = high_resolution_clock::now();
			srand(0);
			for (int i = 0; i < particleCount; i++)
			{
				entities[i] = Entity(Particle(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX));
				entities[i].AddComponent(FrictionConstraint(0.1f));
				entities[i].AddComponent(BoxConstraint(10.1f, 10.1f));
			}
			auto end = high_resolution_clock::now();
			std::cout << "\tSetup time " << (end - start).count() / 1000000.0 << "ms\n";

			start = high_resolution_clock::now();
			for (int n = 0; n < iterationCount; n++)
			{
				for (auto &&[p] : GetComponents<Particle>())
				{
					p.vy -= p.y * 0.1;
					p.vx -= p.x * 0.1;
					p.x += p.vx;
					p.y += p.vy;
				}
			}
			end = high_resolution_clock::now();
			std::cout << "\tRun time " << (end - start).count() / 1000000.0 << "ms\n";
		}

		int i = 0;
		for (auto &&[p] : GetComponents<Particle>())
		{
			if (p != particles[i++])
			{
				std::cout << "Failed performance test: Impopper particle data\n";
				return 1;
			}
		}
		if (i != particleCount)
		{
			std::cout << "Failed performance test: Impropper particle count\n";
			return 1;
		}
	}
	{
		std::cout << "\nSame with my ECS: \n";
		Entity entities[particleCount];
		{
			auto start = high_resolution_clock::now();
			srand(0);
			for (int i = 0; i < particleCount; i++)
				entities[i] = Entity(Particle(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX));
			auto end = high_resolution_clock::now();
			std::cout << "\tSetup time " << (end - start).count() / 1000000.0 << "ms\n";

			start = high_resolution_clock::now();
			for (int n = 0; n < iterationCount; n++)
			{
				for (auto &&[p] : GetComponents<Particle>())
				{
					p.vy -= p.y * 0.1;
					p.vx -= p.x * 0.1;
					p.x += p.vx;
					p.y += p.vy;
				}
			}
			end = high_resolution_clock::now();
			std::cout << "\tRun time " << (end - start).count() / 1000000.0 << "ms\n";
		}

		int i = 0;
		for (auto &&[p] : GetComponents<Particle>())
		{
			if (p != particles[i++])
			{
				std::cout << "Failed performance test: Impopper particle data\n";
				return 1;
			}
		}
		if (i != particleCount)
		{
			std::cout << "Failed performance test: Impropper particle count\n";
			return 1;
		}
	}

	{
		std::this_thread::sleep_for(1s);
		Entity entities[particleCount];
		{
			auto start = high_resolution_clock::now();
			srand(0);
			for (int i = 0; i < particleCount; i++)
			{
				entities[i] = Entity(Particle(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX));
			}
			for (int i = 0; i < particleCount; i++)
			{
				if (rand() < RAND_MAX / 2) entities[i].AddComponent(FrictionConstraint(0.1f));
				if (rand() < RAND_MAX / 2) entities[i].AddComponent(BoxConstraint(10.1f, 10.1f));
			}
			auto end = high_resolution_clock::now();
			std::cout << "\tSetup time " << (end - start).count() / 1000000.0 << "ms\n";

			start = high_resolution_clock::now();
			for (int n = 0; n < iterationCount; n++)
			{
				for (auto &&[p] : GetComponents<Particle>())
				{
					p.vy -= p.y * 0.1;
					p.vx -= p.x * 0.1;
					p.x += p.vx;
					p.y += p.vy;
				}
			}
			end = high_resolution_clock::now();
			std::cout << "\tRun time " << (end - start).count() / 1000000.0 << "ms\n";
		}

		int i = 0;
		for (auto &&[p] : GetComponents<Particle>()) i++;
		if (i != particleCount)
		{
			std::cout << "Failed performance test: Impropper particle count\n";
			return 1;
		}
	}
	return 0;
}
