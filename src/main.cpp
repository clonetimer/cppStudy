#include <iostream>

int main()
{
    int* numbers = new int[3];

    numbers[0] = 10;
    numbers[1] = 20;
    numbers[2] = 30;

    numbers[3] = 40;  // BUG

    std::cout << numbers[0] << '\n';

    delete[] numbers;

    return 0;
}