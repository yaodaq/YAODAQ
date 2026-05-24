#include <doctest/doctest.h>

#include <thread>
#include <vector>
#include <set>
#include <mutex>

#include "yaodaq/Clock.hpp"

TEST_SUITE("yaodaq::clock")
{

TEST_CASE("utc_nanoseconds increases monotonically")
{
    const auto t1 =
        yaodaq::clock::utc_nanoseconds();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1)
    );

    const auto t2 =
        yaodaq::clock::utc_nanoseconds();

    CHECK(t2 > t1);
}

TEST_CASE("utc_microseconds consistency")
{
    const auto ns =
        yaodaq::clock::utc_nanoseconds();

    const auto us =
        yaodaq::clock::utc_microseconds();

    CHECK(us <= ns / 1000);
    CHECK(us >= (ns / 1000) - 1000);
}

TEST_CASE("utc_milliseconds consistency")
{
    const auto ns =
        yaodaq::clock::utc_nanoseconds();

    const auto ms =
        yaodaq::clock::utc_milliseconds();

    CHECK(ms <= ns / 1'000'000);
    CHECK(ms >= (ns / 1'000'000) - 10);
}

TEST_CASE("utc_seconds consistency")
{
    const auto ns =
        yaodaq::clock::utc_nanoseconds();

    const auto sec =
        yaodaq::clock::utc_seconds();

    CHECK(sec <= ns / 1'000'000'000);
    CHECK(sec >= (ns / 1'000'000'000) - 1);
}

TEST_CASE("to_string produces valid ISO8601 UTC format")
{
    const auto ts =
        yaodaq::clock::utc_nanoseconds();

    const auto str =
        yaodaq::clock::to_string(ts);

    CHECK(str.size() == 30);

    CHECK(str[4]  == '-');
    CHECK(str[7]  == '-');
    CHECK(str[10] == 'T');
    CHECK(str[13] == ':');
    CHECK(str[16] == ':');
    CHECK(str[19] == '.');
    CHECK(str[29] == 'Z');
}

TEST_CASE("from_string roundtrip")
{
    const auto original =
        yaodaq::clock::utc_nanoseconds();

    const auto str =
        yaodaq::clock::to_string(original);

    const auto restored =
        yaodaq::clock::from_string(str);

    CHECK(restored == original);
}

TEST_CASE("utc_string roundtrip")
{
    const auto str =
        yaodaq::clock::utc_string();

    const auto ts =
        yaodaq::clock::from_string(str);

    const auto str2 =
        yaodaq::clock::to_string(ts);

    CHECK(str == str2);
}

TEST_CASE("from_string rejects invalid format")
{
    CHECK_THROWS(
        yaodaq::clock::from_string(
            "invalid"
        )
    );

    CHECK_THROWS(
        yaodaq::clock::from_string(
            "2026-01-01"
        )
    );

    CHECK_THROWS(
        yaodaq::clock::from_string(
            "2026-05-25T14:32:45Z"
        )
    );

    CHECK_THROWS(
        yaodaq::clock::from_string(
            "2026-05-25T14:32:45.123456789"
        )
    );
}

TEST_CASE("thread safety")
{
    constexpr int count = 1000;

    std::vector<std::thread> threads;

    std::mutex mutex;

    std::set<std::int64_t> values;

    for(int i = 0; i < 8; ++i)
    {
        threads.emplace_back(
            [&]()
            {
                for(int j = 0; j < count; ++j)
                {
                    const auto ts =
                        yaodaq::clock::utc_nanoseconds();

                    std::lock_guard<std::mutex>
                        lock(mutex);

                    values.insert(ts);
                }
            }
        );
    }

    for(auto& t : threads)
    {
        t.join();
    }

    CHECK(values.size() > 0);
}

TEST_CASE("nanoseconds precision preserved")
{
    constexpr std::int64_t ts =
        1779719565123456789LL;

    const auto str =
        yaodaq::clock::to_string(ts);

    const auto restored =
        yaodaq::clock::from_string(str);

    CHECK(restored == ts);
}

}
