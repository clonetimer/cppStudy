#include <iostream>
#include <vector>
#include <algorithm>

struct Student
{
    std::string name;
    int score;
};

int main(){
    std::vector<int> numbers{5, 2, 8, 1, 3};
    for(const auto& number : numbers)
    {
        std::cout << "Number: " << number << '\n'; 
    }
    std::cout << "-----\n";
    std::sort(numbers.begin(), numbers.end());
    for(const auto& number : numbers)
    {
        std::cout << "Number: " << number << '\n'; 
    }    
    // lambda函数，采用严格弱序，使用相等符号会导致sort内部的快排分区逻辑越过数组边界读写
    // []捕获外部变量
    std::sort(
        numbers.begin(), 
        numbers.end(), 
        [](int a, int b)
        {
            return a > b;
        }    
    );
    std::cout << "-----\n";
    for(const auto& number : numbers)
    {
        std::cout << "Number: " << number << '\n'; 
    }
    // -------
    auto it = std::find_if(
        numbers.begin(),
        numbers.end(),
        [](int value)
        {
            return value > 3;
        }
    );
    if(it != numbers.end())
    {
        std::cout << ">3 number exist\n"; 
    }
    else
    {
        std::cout << ">3 number not found\n";
    }

    int threshold = 5;
    it = std::find_if(
        numbers.begin(),
        numbers.end(),
        [threshold](int value)
        {
            return value > threshold;
        }
    );
    if(it != numbers.end())
    {
        std::cout << ">" << threshold << " number exist\n"; 
    }
    else
    {
        std::cout << ">" << threshold << " number not found\n";
    }
    // std::transform
    std::vector<int> squared(numbers.size());
    std::transform(
        numbers.begin(),
        numbers.end(),
        squared.begin(),
        [](int value)
        {
            return value * value;
        }
    );
    for (const auto& item : squared)
    {
        std::cout << item << '\n';
    }

    std::vector<Student> students{
        {"Alice", 85},
        {"Bob", 92},
        {"Carol", 78},
        {"David", 95}
    };

    std::sort(
        students.begin(),
        students.end(),
        [](const Student& a, const Student& b)
        {
            return a.score > b.score;
        }
    );

    for (const auto& student : students)
    {
        std::cout
            << student.name
            << ": "
            << student.score
            << '\n';
    }

    auto it2 = std::find_if(
        students.begin(),
        students.end(),
        [](const Student& student)
        {
            return student.score < 80;
        }
    );

    if (it2 != students.end())
    {
        std::cout
            << "Found: "
            << it2->name
            << '\n';
    }
    

    return 0;
}
