#include <csignal>
#include <unistd.h>

#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <fmt/format.h>
#include "time.hpp"
#include "plan.hpp"

std::vector<Student> read_student_config(const nlohmann::json& config) {
    // implementation note: the element accesses below will fail if the data is not convertible with the "get" function
    std::vector<Student> students;
    for (const auto& student_config : config) {
        const auto id = student_config.find("id").value().get<unsigned>();
        const auto name = student_config.find("name").value().get<std::string>();
        const auto lesson_duration = student_config.find("lesson_duration").value().get<unsigned>();
        const unsigned student_prio = students.size() + 1;
        Student student(id, name, lesson_duration, student_prio);
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

void print_schedult_result(const std::vector<Plan::schedule_result>& result,
                           const std::vector<const Student *>& skipped) {
    unsigned prio_sum{};
    for (const auto& student_result : result) {
        const auto prio = student_result.student->get_priority(student_result.start) + 1;
        prio_sum += prio;
        fmt::println("{} - {:t}: {} ({})",
            student_result.start,
            student_result.end,
            student_result.student->get_name(),
            prio
        );
    }
    for (auto student_skipped : skipped) {
        fmt::println("SKIPPED: {} ({})", student_skipped->get_name(), student_skipped->get_id());
    }
    fmt::println("priority sum: {}", prio_sum);
}

void signal_handler(int signal) {
    if (signal == SIGALRM)
        fmt::println("timeout reached");

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
        .range_attempts = default_range_attempts,
        .range_increment = default_range_increment,
        .timeout = 0
    };

    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "i:o:a:d:t:h")) != -1)
        switch (c) {
            case 'h':
                fmt::println("usage: {} "
                             "-i <input-json> "
                             "[-o <output-json>] "
                             "[-a <range-attempts>] "
                             "[-d <range-increments>] "
                             "[-t <timeout>]", argv[0]);
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

nlohmann::json export_schedult_result(const std::vector<Plan::schedule_result>& result,
                                      const std::vector<const Student *>& skipped,
                                      const arguments& args) {
    nlohmann::json schedule_array = nlohmann::json::array();
    for (const auto& student_result : result) {
        schedule_array.emplace_back(nlohmann::json::object({
            {"id", student_result.student->get_id()},
            {"name", student_result.student->get_name()},
            {"day", fmt::format("{:d}", student_result.start)},
            {"from_hour", student_result.start.get_hour()},
            {"from_minute", student_result.start.get_minute()},
            {"to_hour", student_result.end.get_hour()},
            {"to_minute", student_result.end.get_minute()},
        }));
    }

    nlohmann::json skipped_array = nlohmann::json::array();
    for (const auto& student_skipped : skipped) {
        skipped_array.emplace_back(nlohmann::json::object({
            {"id", student_skipped->get_id()},
            {"name", student_skipped->get_name()},
        }));
    }

    return nlohmann::json::object({
        {"schedule", schedule_array},
        {"skipped", skipped_array},
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
        fmt::println(stderr, "Error: {}", ex.what());
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

    const struct solve_config cfg = {
        .range_attempts = args.range_attempts,
        .range_increment = args.range_increment,
        .minimize_wishes_prio = true,
        .minimize_holes = true,
        .availability_index_scale = 5,
        .lunch_time_from_hour = 12,
        .lunch_time_from_minute = 0,
        .lunch_time_to_hour = 13,
        .lunch_time_to_minute = 0,
        .lunch_hole_neg_prio = 10,
        .non_lunch_hole_prio = 150,
        .allow_skip = false,
        .skip_prio = 1000000,
    };

    const bool success = plan.schedule(cfg);

    if (!success) {
        fmt::println("could not create plan");
        return EXIT_FAILURE;
    }

    const auto result = plan.get_result();
    const auto skipped = plan.get_skipped();

    if (args.json_output) {
        std::ofstream o(args.json_output);
        nlohmann::json jo = export_schedult_result(result, skipped, args);
        o << jo.dump(4) << std::endl;
    } else {
        print_schedult_result(result, skipped);
    }

    return EXIT_SUCCESS;
}
