// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdio>

namespace Nova {

    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error
    };

    class Log {
    public:
        static void SetMinLevel(LogLevel level) { minLevel = level; }

        template<typename... Args>
        static void Debug(const char* tag, const char* fmt, Args... args) {
            if (minLevel <= LogLevel::Debug)
                Print(LogLevel::Debug, tag, fmt, args...);
        }

        template<typename... Args>
        static void Info(const char* tag, const char* fmt, Args... args) {
            if (minLevel <= LogLevel::Info)
                Print(LogLevel::Info, tag, fmt, args...);
        }

        template<typename... Args>
        static void Warn(const char* tag, const char* fmt, Args... args) {
            if (minLevel <= LogLevel::Warning)
                Print(LogLevel::Warning, tag, fmt, args...);
        }

        template<typename... Args>
        static void Error(const char* tag, const char* fmt, Args... args) {
            if (minLevel <= LogLevel::Error)
                Print(LogLevel::Error, tag, fmt, args...);
        }

    private:
        static LogLevel minLevel;

        static std::string GetTimestamp() {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ) % 1000;

            std::tm tm_buf;
            localtime_r(&time, &tm_buf);

            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d",
                tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count());
            return buffer;
        }

        static const char* LevelString(LogLevel level) {
            switch (level) {
                case LogLevel::Debug:   return "DBG";
                case LogLevel::Info:    return "INF";
                case LogLevel::Warning: return "WRN";
                case LogLevel::Error:   return "ERR";
            }
            return "???";
        }

        template<typename... Args>
        static void Print(LogLevel level, const char* tag, const char* fmt, Args... args) {
            // Format: [HH:MM:SS.mmm] [LVL] [tag] message
            fprintf(stderr, "[%s] [%s] [%s] ", GetTimestamp().c_str(), LevelString(level), tag);
            fprintf(stderr, fmt, args...);
            fprintf(stderr, "\n");
        }
    };

    // Convenience macros
    #define LOG_DBG(tag, ...) Nova::Log::Debug(tag, __VA_ARGS__)
    #define LOG_INF(tag, ...) Nova::Log::Info(tag, __VA_ARGS__)
    #define LOG_WRN(tag, ...) Nova::Log::Warn(tag, __VA_ARGS__)
    #define LOG_ERR(tag, ...) Nova::Log::Error(tag, __VA_ARGS__)
}
