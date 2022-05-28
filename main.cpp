#include <csignal>
#include <unistd.h>

#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#if 0
#define AT(vec, entry) vec.at(entry)
#else
#define AT(vec, entry) vec[entry]
#endif

constexpr unsigned MIN_ALIGNMENT = 10;

enum class Day : unsigned {
    MONDAY = 0,
    TUESDAY = 1,
    WEDNESDAY = 2,
    THURSDAY = 3,
    FRIDAY = 4,
    SATURDAY = 5,
    SUNDAY = 6,
};

const std::array<std::string, 7> day_names = {
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
    constexpr auto format(Day d, FormatContext& ctx) {
        return formatter<string_view>::format(day_names.at(unsigned(d)), ctx);
    }
};

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
        Time operator-(unsigned chunk_decrement) const { return Time(chunk_of_week - chunk_decrement); }

        bool operator==(const Time& other) const { return chunk_of_week == other.chunk_of_week; }
        bool operator!=(const Time& other) const { return chunk_of_week != other.chunk_of_week; }
        bool operator<(const Time& other) const { return chunk_of_week < other.chunk_of_week; }
        bool operator>(const Time& other) const { return chunk_of_week > other.chunk_of_week; }
        bool operator<=(const Time& other) const { return chunk_of_week <= other.chunk_of_week; }
        bool operator>=(const Time& other) const { return chunk_of_week >= other.chunk_of_week; }

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
        const char c = *it;
        if (it != end && (c == 't' || c == 'd' || c == 's')) {
            p = presentation(c);
            it++;
        }

        // Check if reached the end of the range:
        if (it != end && *it != '}') throw format_error("invalid format");

        // Return an iterator past the end of the parsed range:
        return it;
    }

    template <typename FormatContext>
    auto format(const Time& t, FormatContext& ctx) -> decltype(ctx.out()) {
        // ctx.out() is an output iterator to write to.
        switch (p) {
        case presentation::time: return format_to(ctx.out(), "{:02d}:{:02d}", t.get_hour(), t.get_minute());
        case presentation::day: return format_to(ctx.out(), "{}", t.get_day());
        case presentation::string: return format_to(ctx.out(), "{} {:02d}:{:02d}", t.get_day(), t.get_hour(), t.get_minute());
        }
        throw format_error("invalid format");
    }
};


class Student {
    public:
        Student(const std::string& name, unsigned lesson_duration) :
            name{name},
            lesson_duration{lesson_duration / MIN_ALIGNMENT} {
        }

        const std::string& get_name() const { return name; }
        unsigned get_lesson_duration() const { return lesson_duration * MIN_ALIGNMENT; }
        unsigned get_lesson_chunks() const { return lesson_duration; }
        size_t get_availability_count() const { return availabilities.size(); }
        Time get_availability_option(size_t n) const { return AT(availabilities, n); }

        void add_availability(Time start, Time end) {
            availability_ranges.emplace_back(start, end);
        }

        void calculate_availabilities(unsigned max_attemps, unsigned range_increment) {
            availabilities.clear();

            for (const auto& [start, end] : availability_ranges) {
                const Time check_end = end - lesson_duration;
                Time t = start;
                unsigned attempt{};
                do {
                    availabilities.emplace_back(t);
                    t += range_increment;
                    ++attempt;
                } while (t <= check_end && attempt < max_attemps);
            }
        }

        unsigned get_priority(Time t) const {
            unsigned priority{};
            for (const auto& [start, end] : availability_ranges) {
                if (start <= t && t <= end)
                    return priority;

                ++priority;
            }
            throw std::runtime_error(fmt::format("time {} is not available for {}", t, name));
        }

    protected:
        const std::string name;
        const unsigned lesson_duration;
        std::vector<std::pair<Time, Time>> availability_ranges; // pair = (start_time, end_time)
        std::vector<Time> availabilities;
};

class Plan {
    public:
        Plan(std::vector<Student>&& students) : students{students} {}

        bool schedule(unsigned range_attempts=1, unsigned range_increment=1) {
            const size_t slots = (7 * 24 * 60) / MIN_ALIGNMENT;
            planning.resize(slots);

            for (auto& student : students)
                student.calculate_availabilities(range_attempts, range_increment);

            return schedule_student();
        }

        struct schedule_result {
            Time start;
            Time end;
            const Student *student;
        };

        std::vector<schedule_result> get_result() {
            std::vector<schedule_result> result;

            const Student *current_student = nullptr;
            Time start(0);

            for (unsigned chunk{}; chunk < planning.size(); ++chunk) {
                const Student* student_at_chunk = AT(planning, chunk);
                if (student_at_chunk == current_student)
                    continue;

                Time time_at_chunk(chunk);

                if (current_student)
                    result.emplace_back(schedule_result{start, time_at_chunk, current_student});

                current_student = student_at_chunk;
                start = time_at_chunk;
            }

            return result;
        }

    protected:
        bool take_available(const Student& student, size_t availability_option) {
            const auto lesson_chunks = student.get_lesson_chunks();
            const auto availability_chunk = student.get_availability_option(availability_option).get_chunk_of_week();
            for (unsigned i{}; i < lesson_chunks; ++i)
                if (AT(planning, availability_chunk + i))
                    return false;

            for (unsigned i{}; i < lesson_chunks; ++i)
                AT(planning, availability_chunk + i) = &student;

            return true;
        }

        void clear_available(const Student& student, size_t availability_option) {
            const auto lesson_chunks = student.get_lesson_chunks();
            const auto availability_chunk = student.get_availability_option(availability_option).get_chunk_of_week();
            for (unsigned i{}; i < lesson_chunks; ++i)
                AT(planning, availability_chunk + i) = nullptr;
        }

        bool schedule_student(size_t student_index=0) {
            if (student_index >= students.size())
                return true;

            const auto& student = AT(students, student_index);
            const auto student_availability_count = student.get_availability_count();
            for (unsigned i{}; i < student_availability_count; ++i) {
                if (!take_available(student, i))
                    continue;

                if (schedule_student(student_index + 1))
                    return true;

                clear_available(student, i);
            }
            return false;
        }

        std::vector<Student> students;
        std::vector<const Student*> planning;
};

std::vector<Student> read_student_config(const nlohmann::json& config) {
    // implementation note: the element accesses below will fail if the data is not convertible with the "get" function
    std::vector<Student> students;
    for (const auto& student_config : config) {
        const auto name = student_config.find("name").value().get<std::string>();
        const auto lesson_duration = student_config.find("lesson_duration").value().get<unsigned>();
        Student student(name, lesson_duration);
        for (const auto& [_, availability] : student_config.find("availabilities").value().items()) {
            const auto day         = parse_day(availability.find("day").value().get<std::string>());
            const auto from_hour   = availability.find("from_hour").value().get<unsigned>();
            const auto from_minute = availability.find("from_minute").value().get<unsigned>();
            const auto to_hour     = availability.find("to_hour").value().get<unsigned>();
            const auto to_minute   = availability.find("to_minute").value().get<unsigned>();
            student.add_availability(Time(day, from_hour, from_minute), Time(day, to_hour, to_minute));
        }
        students.push_back(student);
    }
    return students;
}

void print_schedult_result(const std::vector<Plan::schedule_result>& result) {
    for (const auto& student_result : result) {
        fmt::print("{} - {}: {} ({})\n",
            student_result.start,
            student_result.end,
            student_result.student->get_name(),
            student_result.student->get_priority(student_result.start) + 1
        );
    }
}

void signal_handler(int signal) {
    if (signal == SIGALRM)
        fmt::print("timeout reached\n");

    exit(EXIT_FAILURE);
}

struct arguments {
    const char *json_input;
    const char *json_output;
    unsigned range_attempts;
    unsigned range_increment;
    unsigned timeout;
};

class argument_exception : std::exception {
    public:
        argument_exception(const std::string &&msg) : msg{msg} {}
        const char* what() const noexcept override { return msg.c_str(); }
    private:
        const std::string msg;
};

arguments parse_arguments(int argc, char* const* argv) {
    arguments ret{
        .json_input = nullptr,
        .json_output = nullptr,
        .range_attempts = 2,
        .range_increment = 2,
        .timeout = 0
    };

    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "i:o:a:d:t:h")) != -1)
        switch (c) {
            case 'h':
                fmt::print("usage: {} "
                           "-i <input-json> "
                           "[-o <output-json>] "
                           "[-a <range-attempts>] "
                           "[-d <range-increments>] "
                           "[-t <timeout>]\n", argv[0]);
                exit(EXIT_SUCCESS);

            case 'i':
                ret.json_input = optarg;
                break;

            case 'o':
                ret.json_output = optarg;
                break;

            case 'a':
                ret.range_attempts = atoi(optarg);
                break;

            case 'd':
                ret.range_increment = atoi(optarg);
                break;

            case 't':
                ret.timeout = atoi(optarg);
                break;

            case '?':
                if (optopt == 'i' || optopt == 'o' || optopt == 'a' || optopt == 'd')
                    throw argument_exception(fmt::format("Option -{:c} requires an argument.", char(optopt)));
                else if (isprint(optopt))
                    throw argument_exception(fmt::format("Unknown option `-{:c}'.", char(optopt)));
                else
                    throw argument_exception(fmt::format("Unknown option character `\\x{:x}'.", optopt));
            default:
                throw argument_exception("unknown error");
        }

    if (optind < argc)
        throw argument_exception(fmt::format("Unknown argument `{}'", argv[optind]));

    return ret;
}

nlohmann::json export_schedult_result(const std::vector<Plan::schedule_result>& result, const arguments& args) {
    nlohmann::json s = nlohmann::json::array();
    for (const auto& student_result : result) {
        s.emplace_back(nlohmann::json::object({
            {"name", student_result.student->get_name()},
            {"day", fmt::format("{:d}", student_result.start)},
            {"from_hour", student_result.start.get_hour()},
            {"from_minute", student_result.start.get_minute()},
            {"to_hour", student_result.end.get_hour()},
            {"to_minute", student_result.end.get_minute()},
        }));
    }
    return nlohmann::json::object({
        {"schedule", s},
        {"options", {
            {"range_attempts", args.range_attempts},
            {"range_increments", args.range_increment},
        },
        //{"statistics", {}} // TODO: add generation statistics
        }
    });
}

int main(int argc, char* const* argv) {
    arguments args;
    try {
        args = parse_arguments(argc, argv);
    } catch (argument_exception &ex) {
        fmt::print(stderr, "Error: {}\n", ex.what());
        return EXIT_FAILURE;
    }

    std::ifstream i(args.json_input);
    nlohmann::json ji;
    i >> ji;

    Plan plan(read_student_config(ji));

    std::signal(SIGINT, signal_handler);
    if (args.timeout) {
        std::signal(SIGALRM, signal_handler);
        alarm(args.timeout);
    }

    const bool success = plan.schedule(args.range_attempts, args.range_increment);

    if (!success) {
        fmt::print("could not create plan\n");
        return EXIT_FAILURE;
    }

    const auto result = plan.get_result();

    if (args.json_output) {
        std::ofstream o(args.json_output);
        nlohmann::json jo = export_schedult_result(result, args);
        o << jo.dump(4) << std::endl;
    } else {
        print_schedult_result(result);
    }

    return EXIT_SUCCESS;
}
