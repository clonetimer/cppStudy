#include <iostream>
#include <sstream>
#include <string>

int main()
{
    std::string line =
        "POST /echo HTTP/1.1";

    std::istringstream stream(line);

    std::string method;
    std::string path;
    std::string version;

    if (
        !(stream
          >> method
          >> path
          >> version)
    )
    {
        std::cerr
            << "Bad request line\n";

        return 1;
    }

    std::cout
        << "method = "
        << method
        << '\n';

    std::cout
        << "path = "
        << path
        << '\n';

    std::cout
        << "version = "
        << version
        << '\n';
}