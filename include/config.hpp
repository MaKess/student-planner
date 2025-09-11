#pragma once

#define DEBUG

#ifdef DEBUG
#define AT(vec, entry) vec.at(entry)
#else
#define AT(vec, entry) vec[entry]
#endif

struct solve_config {
    unsigned range_attempts;
    unsigned range_increment;
    bool minimize_wishes_prio;
    bool minimize_holes;
    unsigned availability_index_scale;
    unsigned lunch_time_from_hour;
    unsigned lunch_time_from_minute;
    unsigned lunch_time_to_hour;
    unsigned lunch_time_to_minute;
    unsigned lunch_hole_neg_prio;
    unsigned non_lunch_hole_prio;
    bool allow_skip;
    unsigned skip_prio;
};

constexpr unsigned MIN_ALIGNMENT = 10;
constexpr size_t slots_per_week = (7 * 24 * 60) / MIN_ALIGNMENT;
constexpr bool enumerate_all_solutions = false;
constexpr bool print_stats = true;
