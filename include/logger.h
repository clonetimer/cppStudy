#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>

enum class LogLevel
{
    Debug,
    Info,
    Error
};

class Logger
{
public:
    static void setEnabled(bool enabled)
    {
        std::lock_guard<std::mutex> lock(
            mutex_);

        enabled_ = enabled;
    }

    static void log(
        LogLevel level,
        std::string_view message)
    {
        std::lock_guard<std::mutex> lock(
            mutex_);

        if (!enabled_)
        {
            return;
        }

        std::cerr
            << '['
            << levelName(level)
            << "] [thread "
            << std::this_thread::get_id()
            << "] "
            << message
            << '\n';
    }

private:
    static const char* levelName(
        LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Debug:
            return "DEBUG";

        case LogLevel::Info:
            return "INFO";

        case LogLevel::Error:
            return "ERROR";
        }

        return "UNKNOWN";
    }

private:
    inline static std::mutex mutex_;

    inline static bool enabled_ = true;
};

#endif