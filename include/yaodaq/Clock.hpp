#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <stdexcept>

#include <fmt/format.h>

namespace yaodaq
{

class clock
{
public:

    /*
     * UTC timestamp in nanoseconds
     * since Unix epoch
     */
    static std::int64_t
    utc_nanoseconds() noexcept
    {
        static const auto base_system =
            std::chrono::system_clock::now();

        static const auto base_steady =
            std::chrono::steady_clock::now();

        const auto delta =
            std::chrono::steady_clock::now()
            - base_steady;

        const auto now =
            base_system + delta;

        return std::chrono::duration_cast<
            std::chrono::nanoseconds
        >(
            now.time_since_epoch()
        ).count();
    }

    /*
     * UTC timestamp in microseconds
     */
    static std::int64_t
    utc_microseconds() noexcept
    {
        return utc_nanoseconds() / 1'000;
    }

    /*
     * UTC timestamp in milliseconds
     */
    static std::int64_t
    utc_milliseconds() noexcept
    {
        return utc_nanoseconds() / 1'000'000;
    }

    /*
     * UTC timestamp in seconds
     */
    static std::int64_t
    utc_seconds() noexcept
    {
        return utc_nanoseconds() / 1'000'000'000;
    }

    /*
     * Convert nanoseconds timestamp
     * to ISO8601 UTC string
     *
     * Example:
     * 2026-05-25T14:32:45.123456789Z
     */
    static std::string
    to_string(std::int64_t timestamp_ns)
    {
        using namespace std::chrono;

        const nanoseconds ns(timestamp_ns);

        const auto sec =
            duration_cast<seconds>(ns);

        const auto ns_part =
            ns - sec;

        const system_clock::time_point tp(sec);

        const std::time_t tt =
            system_clock::to_time_t(tp);

        std::tm tm{};

    #if defined(_WIN32)
        gmtime_s(&tm, &tt);
    #else
        gmtime_r(&tt, &tm);
    #endif

        return fmt::format(
            "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:09}Z",
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec,
            ns_part.count()
        );
    }

    /*
     * Current UTC ISO8601 string
     */
    static std::string
    utc_string()
    {
        return to_string(
            utc_nanoseconds()
        );
    }

    /*
     * Convert ISO8601 UTC string
     * to nanoseconds timestamp
     *
     * Input:
     * 2026-05-25T14:32:45.123456789Z
     */
    static std::int64_t
    from_string(const std::string& s)
    {
        if (s.size() != 30)
        {
            throw std::runtime_error(
                "Invalid timestamp size"
            );
        }

        std::tm tm{};

        tm.tm_year =
            std::stoi(s.substr(0, 4)) - 1900;

        tm.tm_mon =
            std::stoi(s.substr(5, 2)) - 1;

        tm.tm_mday =
            std::stoi(s.substr(8, 2));

        tm.tm_hour =
            std::stoi(s.substr(11, 2));

        tm.tm_min =
            std::stoi(s.substr(14, 2));

        tm.tm_sec =
            std::stoi(s.substr(17, 2));

        const auto ns =
            std::stoll(s.substr(20, 9));

        if (s[29] != 'Z')
        {
            throw std::runtime_error(
                "Timestamp must be UTC"
            );
        }

    #if defined(_WIN32)
        const std::time_t tt =
            _mkgmtime(&tm);
    #else
        const std::time_t tt =
            timegm(&tm);
    #endif

        if (tt == -1)
        {
            throw std::runtime_error(
                "Invalid UTC timestamp"
            );
        }

        return
            static_cast<std::int64_t>(tt)
            * 1'000'000'000LL
            + ns;
    }
};

} // namespace yaodaq
