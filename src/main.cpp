#include <iostream>
#include <string>
#include <vector>

class Task
{
public:
    Task(std::string text)
        : text_(text)
    {
        std::cout
            << "Contruct: "
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

void run()
{
    Task first("First");
    std::vector<int> values{
        1, 2, 3, 4, 5
    };
    Task second("Second");
    std::cout << "run() ending\n";
}

int main()
{
    std::cout << "Before run()\n";
    run();
    std::cout << "After run()\n";
}