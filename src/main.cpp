#include <iostream>
#include "calculator.h"

int main()
{
    std::cout << "add:" << add(3, 5) << std::endl;
    std::cout << "sub:" << subtract(3, 5) << std::endl;
    std::cout << "mul:" << multiply(3, 5) << std::endl;
    std::cout << "div:" << divide(10, 5) << std::endl;
    return 0;
}