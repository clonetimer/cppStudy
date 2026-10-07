#ifndef LOGGER_H
#define LOGGER_H

#include <string_view>

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

class Logger
{
public:
    static void log(
        LogLevel level,
        std::string_view message
    );

    static void debug(
        std::string_view message
    );

    static void info(
        std::string_view message
    );

    static void warning(
        std::string_view message
    );

    static void error(
        std::string_view message
    );

private:
    static const char* levelName(
        LogLevel level
    );
};

#endif