#include "calculator.h"

#include <iostream>
#include <stdexcept>

int main()
{
    double first = 0.0;
    double second = 0.0;
    char operation = '\0';

    std::cout << "Enter first number: ";
    std::cin >> first;

    std::cout << "Enter operator (+ - * /): ";
    std::cin >> operation;

    std::cout << "Enter second number: ";
    std::cin >> second;

    try
    {
        double result = 0.0;

        switch (operation)
        {
        case '+':
            result = add(first, second);
            break;

        case '-':
            result = subtract(first, second);
            break;

        case '*':
            result = multiply(first, second);
            break;

        case '/':
            result = divide(first, second);
            break;

        default:
            std::cerr << "Error: unsupported operator." << std::endl;
            return 1;
        }

        std::cout << "Result: " << result << std::endl;
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "Error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}