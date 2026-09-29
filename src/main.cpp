#include <iostream>
#include <string>
#include <limits>


void printMessage(const std::string& message)
{
    std::cout << message << std::endl;
}
int main()
{
    std::string name = "Alice";

    std::cout << name << std::endl;
    std::cout << name.size() << std::endl;

    std::string first = "Hello";
    std::string second = "World";

    std::string result = first + " " + second;
    std::cout << result << '\n';

    if(~result.empty()) std::cout << "result not empty" << std::endl;
    std::cout << result[0] << '\n'; 

    if (result == "Hello World") std::cout << "result is 'Hello World' " << '\n';

    for (char c : result) std::cout << c << std::endl;
    for (char& c : result) if (c=='l') c='L';
    std::cout << "result:" << result << '\n';
    std::cout << "W is at:" << result.find("W") << '\n';
    if (result.find("world") == std::string::npos) std::cout << "Can't find world" << '\n';
    std::cout << result.substr(6,5) << '\n';
    std::cout << result.erase(5,6) << '\n';
    std::cout << result.insert(5," insert") << '\n';

    std::string text;
    std::cout << "input two word with blank:";
    std::cin >> text;
    std::cout << "input is " << text << '\n';

    std::cin.ignore(
        std::numeric_limits<std::streamsize>::max(),
        '\n'
    );

    std::cout << "input two word with blank again:";
    std::getline(std::cin, text);
    std::cout << "input is " << text << '\n';

    printMessage(text);
    printMessage("hello");
    
    return 0;
}