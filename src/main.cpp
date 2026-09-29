#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <set>


int main(){
    // key, value
    std::map<std::string, int> scores;
    scores["Alice"] = 90;
    scores["Bob"] = 85;
    scores["Carol"] = 95;

    std::cout << scores["Alice"] << '\n';

    //遍历
    for (const auto& item:scores)
    {
        std::cout << item.first
                  << ": "
                  << item.second
                  << '\n';
    }
    // 未定义的key，value取默认值`0`
    std::cout << "David: " << scores["David"] << '\n';

    if (scores.find("Bob") != scores.end()) std::cout << "Bob exist" << '\n';
    // C++20支持contain
    if (scores.contains("Alice")) std::cout << "Alice exist" << '\n';

    //insert
    scores["Timer1"] = 100;
    scores.insert({"Timer2", 95});
    scores.emplace("Timer3", 95);
    for (const auto& [name, score] : scores)
    {
        std::cout << name
                  << ": "
                  << score
                  << '\n';
    }
    // modify
    scores["Timer3"] = 100;
    std::cout << "Timer3: " << scores["Timer3"] << std::endl;
    // delete
    scores.erase("Bob");
    if(scores.find("Bob") == scores.end()) std::cout << "Bob not found" << '\n';
    // clear
    scores.clear();
    if (scores.empty()) std::cout << "Empty Map" << '\n';

    //不保证顺序的键值表
    std::unordered_map<std::string, int> ages;
    ages["Alice"] = 20;
    ages["Bob"] = 21;
    ages["Carol"] = 22;

    for (const auto& age : ages)
    {
        std::cout << age.first
                  << ": "
                  << age.second
                  << '\n';
    }

    // 集合:去重+排序
    std::set<std::string> names{"Bob","Alice"};

    names.insert("Bob");
    names.insert("Alice");
    for (const auto& name : names)
    {
        std::cout << name << '\n';
    }

    if(names.find("Alice") != names.end()) std::cout << "Alice exist in set" << '\n';
    if(names.count("Bob")) std::cout << "Bob exist in set" << '\n';
    if(!names.contains("Carol")) std::cout << "Carol not found" << '\n';

    

    return 0;
}