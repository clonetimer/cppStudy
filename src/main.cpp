#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Student
{
    std::string name;
    int score;
};

int main()
{
    std::ifstream input("scores.txt");

    if (!input)
    {
        std::cerr
            << "Error: failed to open scores.txt\n";
        return 1;
    }

    std::vector<Student> students;

    std::string line;
    int lineNumber = 0;

    while (std::getline(input, line))
    {
        ++lineNumber;

        if (line.empty())
        {
            continue;
        }

        std::istringstream parser(line);

        Student student;

        if (!(parser >> student.name >> student.score))
        {
            std::cerr
                << "Error: invalid format at line "
                << lineNumber
                << ": "
                << line
                << '\n';

            return 1;
        }

        students.push_back(student);
    }

    std::sort(
        students.begin(),
        students.end(),
        [](const Student& a, const Student& b)
        {
            return a.score > b.score;
        }
    );

    std::ofstream output("results.txt");

    if (!output)
    {
        std::cerr
            << "Error: failed to create results.txt\n";
        return 1;
    }

    for (const auto& student : students)
    {
        std::cout
            << student.name
            << ' '
            << student.score
            << '\n';

        output
            << student.name
            << ' '
            << student.score
            << '\n';
    }

    return 0;
}