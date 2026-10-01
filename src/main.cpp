#include <iostream>
#include <string>

class Task
{
public:
    Task(std::string text)
        : text_(text)
    {
        std::cout
            << "Construct: "
            << text_
            << '\n';
    }

    ~Task()
    {
        std::cout
            << "Destroy: "
            << text_
            << '\n';
    }

private:
    std::string text_;
};

int main()
{
    Task first("First");

    {
        Task second("Second");
        Task third("Third");
    }

    Task fourth("Fourth");

    return 0;
}