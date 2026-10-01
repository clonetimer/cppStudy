#include <iostream>
#include <string>
#include <utility>

class Object
{
public:
    Object(std::string name)
        : name_(std::move(name))
    {
        std::cout
            << "Construct: "
            << name_
            << '\n';
    }

    ~Object()
    {
        std::cout
            << "Destroy: "
            << name_
            << '\n';
    }

    Object(const Object& other)
        : name_(other.name_)
    {
        std::cout
            << "Copy construct: "
            << name_
            << '\n';
    }

    Object& operator=(const Object& other)
    {
        std::cout
            << "Copy assign\n";

        name_ = other.name_;

        return *this;
    }

    Object(Object&& other) noexcept
        : name_(std::move(other.name_))
    {
        std::cout
            << "Move construct: "
            << name_
            << '\n';
    }

    Object& operator=(Object&& other) noexcept
    {
        std::cout
            << "Move assign\n";

        name_ = std::move(other.name_);

        return *this;
    }

private:
    std::string name_;
};

int main()
{
    Object a("A");

    std::cout << "--- copy construct ---\n";

    Object b = a;

    std::cout << "--- copy assign ---\n";

    Object c("C");
    c = a;

    std::cout << "--- move construct ---\n";

    Object d = std::move(a);

    std::cout << "--- move assign ---\n";

    Object e("E");
    e = std::move(b);
}