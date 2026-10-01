#include <iostream>
#include <memory>
#include <string>

class Resource
{
public:
    Resource(std::string name)
        : name_(std::move(name))
    {
        std::cout
            << "Create: "
            << name_
            << '\n';
    }

    ~Resource()
    {
        std::cout
            << "Destroy: "
            << name_
            << '\n';
    }

private:
    std::string name_;
};

void uniqueDemo()
{
    auto first =
        std::make_unique<Resource>("Unique");

    auto second =
        std::move(first);

    if (!first)
    {
        std::cout << "first is empty\n";
    }
}

void sharedDemo()
{
    auto first =
        std::make_shared<Resource>("Shared");

    std::cout
        << first.use_count()
        << '\n';

    {
        auto second = first;

        std::cout
            << first.use_count()
            << '\n';
    }

    std::cout
        << first.use_count()
        << '\n';
}

void weakDemo()
{
    std::weak_ptr<Resource> weak;

    {
        auto shared =
            std::make_shared<Resource>("Weak target");

        weak = shared;

        if (auto locked = weak.lock())
        {
            std::cout
                << "Object is alive\n";
        }
    }

    if (weak.expired())
    {
        std::cout
            << "Object is gone\n";
    }
}

int main()
{
    uniqueDemo();

    std::cout << "---\n";

    sharedDemo();

    std::cout << "---\n";

    weakDemo();
}