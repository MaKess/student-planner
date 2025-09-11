#pragma once
#include <array>
#include <string>
#include <stdexcept>
#include <fmt/format.h>

enum class Day : unsigned {
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY,
    SUNDAY,
};

static const std::array<std::string, 7> day_names = {
    "MONDAY",
    "TUESDAY",
    "WEDNESDAY",
    "THURSDAY",
    "FRIDAY",
    "SATURDAY",
    "SUNDAY",
};

Day parse_day(const std::string& str) {
    for (unsigned i{}; i < day_names.size(); ++i)
        if (str == day_names[i])
            return Day(i);
    throw std::runtime_error(fmt::format("invalid day '{}'", str));
}

template <>
struct fmt::formatter<Day> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format(Day d, FormatContext& ctx) const {
        return formatter<string_view>::format(day_names.at(unsigned(d)), ctx);
    }
};
