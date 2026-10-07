#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace
{

std::mutex logMutex;

}

const char* Logger::levelName(
    LogLevel level
)
{
    switch (level)
    {
        case LogLevel::Debug:
            return "DEBUG";

        case LogLevel::Info:
            return "INFO";

        case LogLevel::Warning:
            return "WARN";

        case LogLevel::Error:
            return "ERROR";
    }

    return "UNKNOWN";
}

void Logger::log(
    LogLevel level,
    std::string_view message
)
{
    const auto now =
        std::chrono::system_clock::now();

    const std::time_t time =
        std::chrono::system_clock::
            to_time_t(now);

    std::tm localTime{};

#if defined(_WIN32)

    localtime_s(
        &localTime,
        &time
    );

#else

    localtime_r(
        &time,
        &localTime
    );

#endif

    std::lock_guard<std::mutex>
        lock(logMutex);

    std::ostream& output =
        level == LogLevel::Error
        ? std::cerr
        : std::cout;

    output
        << '['
        << std::put_time(
            &localTime,
            "%Y-%m-%d %H:%M:%S"
        )
        << "] ["
        << levelName(level)
        << "] "
        << message
        << '\n';

    output.flush();
}

void Logger::debug(
    std::string_view message
)
{
    log(
        LogLevel::Debug,
        message
    );
}

void Logger::info(
    std::string_view message
)
{
    log(
        LogLevel::Info,
        message
    );
}

void Logger::warning(
    std::string_view message
)
{
    log(
        LogLevel::Warning,
        message
    );
}

void Logger::error(
    std::string_view message
)
{
    log(
        LogLevel::Error,
        message
    );
}