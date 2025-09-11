#pragma once
#include <stdexcept>
#include <fmt/format.h>
#include "config.hpp"
#include "day.hpp"

class Time {
    public:
        Time(Day day, unsigned hour, unsigned minute) : Time(((unsigned(day) * 24 + hour) * 60 + minute) / MIN_ALIGNMENT) {
            if (hour >= 24)
                throw std::runtime_error(fmt::format("hour needs to be in range 0-23, but is {}", hour));
            if (minute >= 60)
                throw std::runtime_error(fmt::format("minute needs to be in range 0-59, but is {}", minute));
        }
        Time(unsigned chunk_of_week) : chunk_of_week{chunk_of_week} {}

        unsigned get_chunk_of_week() const { return chunk_of_week; }

        unsigned get_chunk_of_day() const { return chunk_of_week % (24 * 60 / MIN_ALIGNMENT); }

        Day get_day() const {
            const unsigned minute_of_week = chunk_of_week * MIN_ALIGNMENT;
            return Day(minute_of_week / (24 * 60));
        }

        unsigned get_hour() const {
            const unsigned minute_of_week = chunk_of_week * MIN_ALIGNMENT;
            const unsigned minute_of_day = minute_of_week % (24 * 60);
            return minute_of_day / 60;
        }

        unsigned get_minute() const {
            const unsigned minute_of_week = chunk_of_week * MIN_ALIGNMENT;
            return minute_of_week % 60;
        }

        void operator+=(unsigned chunk_increment) { chunk_of_week += chunk_increment; }
        Time operator+(unsigned chunk_increment) const { return Time(chunk_of_week + chunk_increment); }
        Time operator-(unsigned chunk_decrement) const { return Time(chunk_of_week - chunk_decrement); }
        friend auto operator<=>(const Time&, const Time&) = default;

    protected:
        unsigned chunk_of_week;
};

template <>
struct fmt::formatter<Time> {
    enum presentation : char {
        time = 't',
        day = 'd',
        string = 's',
    };
    presentation p = presentation::string;
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        // Parse the presentation format. we accept the "t" presentation to represent time
        auto it = ctx.begin(), end = ctx.end();

        if (it != end) {
            const char c = *it;
            if (c == 't' || c == 'd' || c == 's') {
                p = presentation(c);
                it++;
            }
        }

        // Check if reached the end of the range:
        if (it != end && *it != '}') throw format_error("invalid format");

        // Return an iterator past the end of the parsed range:
        return it;
    }

    template <typename FormatContext>
    auto format(const Time& t, FormatContext& ctx) const -> decltype(ctx.out()) {
        // ctx.out() is an output iterator to write to.
        switch (p) {
        case presentation::time: return format_to(ctx.out(), "{:02d}:{:02d}", t.get_hour(), t.get_minute());
        case presentation::day: return format_to(ctx.out(), "{}", t.get_day());
        case presentation::string: return format_to(ctx.out(), "{} {:02d}:{:02d}", t.get_day(), t.get_hour(), t.get_minute());
        }
        throw format_error("invalid format");
    }
};
