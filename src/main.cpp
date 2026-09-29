#include <iostream>
#include <vector>
#include <array>

int main()
{
    std::vector<int> numbers{10, 20, 30};

    numbers.push_back(40);
    numbers.push_back(50);

    std::cout << numbers.size() << std::endl;

    for (int value : numbers)
    {
        std::cout << value << std::endl;
    }

    std::cout << "front():" << numbers.front() << '\n'; // 10
    std::cout << "back():" << numbers.back() << '\n';  // 30  
    numbers.pop_back();
    std::cout << "after pop_back() size:" << numbers.size() << '\n';
    try {
        std::cout << numbers.at(10) << '\n';
    }
    catch (const std::out_of_range& e) {
        std::cerr << "下标越界: " << e.what() << '\n';
    }
    catch (const std::exception& e) {
        std::cerr << "标准异常: " << e.what() << '\n';
    }
    std::cout << "capacity():" << numbers.capacity() << '\n';

    std::array<double, 3> position{1.0, 2.0, 3.0};
    std::cout << position[0] << '\n';
    std::cout << position.size() << '\n';

    for (double value : position)
    {
        std::cout << value << '\n';
    }

    // 返回一个迭代器，指向第一个元素
    auto it = numbers.begin();
    std::cout << "numbers.begin():" << *it << '\n';

    for (
        auto it = numbers.begin();
        it != numbers.end();
        ++it
    )
    {
        std::cout << *it << '\n';
    }
    // 当 vector 被 push_back、insert、erase 等修改时，不要想当然地继续使用之前保存的迭代器、指针或引用。


    std::vector<double> scores{};
    double score = 0.0;
    std::cout << "Enter scores (-1 to finish):" << '\n';
    while(1){
        std::cin >> score;
        if (score != -1) scores.push_back(score);
        else break;
    }
    double sum = 0.0;
    for (double score : scores)
    {
        sum += score;
    }
    std::cout << "Cout:" << scores.size() << '\n';
    std::cout << "Scores:\n";

    for (double score : scores)
    {
        std::cout << score << '\n';
    }

    if (!scores.empty())
    {
        double average =
            sum / static_cast<double>(scores.size());

        std::cout << "Average: "
                  << average
                  << '\n';
    }

    return 0;
}