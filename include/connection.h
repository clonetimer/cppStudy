#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>

struct Connection
{
    int fd = -1;

    std::string nickname;

    std::string readBuffer;

    std::string writeBuffer;

    bool registered = false;

    bool closeRequested = false;
};

#endif