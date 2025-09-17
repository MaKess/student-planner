#pragma once

#include "config.hpp"
#include "time.hpp"
#include "fmt/format.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#ifdef PLAN_PY
#include <Python.h>
#endif
#include <algorithm>

constexpr unsigned default_range_attempts = std::numeric_limits<unsigned>::max();
constexpr unsigned default_range_increment = 1;

using operations_research::sat::BoolVar;
using operations_research::sat::CpModelBuilder;
using operations_research::sat::CpSolverResponse;
using operations_research::sat::CpSolverStatus;
using operations_research::sat::Model;
using operations_research::sat::SatParameters;
using operations_research::sat::NewFeasibleSolutionObserver;
using operations_research::sat::LinearExpr;

class Student {
    public:
        Student(unsigned id, const std::string& name, unsigned lesson_duration, unsigned student_prio=1) :
            id{id},
            name{name},
            lesson_duration{lesson_duration / MIN_ALIGNMENT},
            student_prio{student_prio} {
        }

        #ifdef PLAN_PY
        ~Student() {
            // Py_DecRef(py_obj_name);
            // fmt::println("destructing {}", name);
        }
        #endif

        unsigned get_id() const { return id; }
        const std::string& get_name() const { return name; }
        unsigned get_lesson_duration() const { return lesson_duration * MIN_ALIGNMENT; }
        unsigned get_lesson_chunks() const { return lesson_duration; }
        size_t get_availability_count() const { return availabilities.size(); }

        void add_availability(Time start, Time end) {
            availability_ranges.emplace_back(start, end);
        }

        void calculate_availabilities(CpModelBuilder& cp_model,
                                      std::vector<std::list<std::tuple<BoolVar, unsigned>>>& wishes,
                                      const struct solve_config& cfg) {
            availabilities.clear();

            std::vector<BoolVar> all_vars;

            // XXX
            if (cfg.allow_skip) {
                const auto s = fmt::format("skip {}", name);
                skip = cp_model.NewBoolVar().WithName(s);
                all_vars.push_back(skip);
            }

            unsigned availability_index{};
            for (const auto& [start, end] : availability_ranges) {
                const Time check_end = end - lesson_duration;
                Time t = start;
                unsigned attempt{};
                do {
                    const auto s = fmt::format("{} at {} (+{})", name, t, get_lesson_duration());
                    auto var = cp_model.NewBoolVar().WithName(s);
                    availabilities.emplace_back(t, var);
                    all_vars.push_back(var);
                    // the factor "10" doesn't really do much here, since *everyone* gets it.
                    // it's just there so that the division by "student_prio" has something to work with and stay an integer
                    const unsigned prio = 10 * availability_index / student_prio;
                    AT(wishes, t.get_chunk_of_week()).emplace_back(var, prio);
                    t += cfg.range_increment;
                    ++attempt;
                } while (t <= check_end && attempt < cfg.range_attempts);
                ++availability_index;
            }

            // there should be only one lesson per week for each student
            cp_model.AddExactlyOne(all_vars);
        }

        void register_conflicts(CpModelBuilder& cp_model,
                                std::vector<std::list<std::tuple<BoolVar, unsigned>>>& wishes) {
            for (const auto& [t, var] : availabilities) {
                const auto end = t + lesson_duration;
                for (auto t2 = t; t2 < end; t2 += 1)
                    for (const auto& [w, prio] : AT(wishes, t2.get_chunk_of_week()))
                        if (w != var)
                            cp_model.AddImplication(var, w.Not());
            }
        }

        void register_impact(std::vector<std::vector<BoolVar>>& impact) {
            for (const auto& [t, var] : availabilities) {
                const auto end = t + lesson_duration;
                for (auto t2 = t; t2 < end; t2 += 1)
                    AT(impact, t2.get_chunk_of_week()).push_back(var);
            }
        }

        // XXX
        bool is_skipped(const CpSolverResponse& response) {
            return SolutionIntegerValue(response, skip) == 1;
        }

        Time get_solution_time(const CpSolverResponse& response) {
            for (const auto& [t, var] : availabilities)
                if (SolutionIntegerValue(response, var) == 1)
                    return t;
            throw std::runtime_error(fmt::format("no solution was found for {}", name));
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

        #ifdef PLAN_PY
        PyObject* py_obj_name = nullptr;
        #endif

    BoolVar get_skip_var() { return skip; }

    protected:
        const unsigned id;
        const std::string name;
        const unsigned lesson_duration;
        const unsigned student_prio;
        std::list<std::pair<Time, Time>> availability_ranges; // pair = (start_time, end_time)
        std::list<std::pair<Time, BoolVar>> availabilities;

        // XXX
        BoolVar skip;
};

#ifdef DEBUG
static const char* yon(bool b) { return b ? "\e[0;32m" "y" "\e[0m" : "\e[0;31m" "n" "\e[0m"; }
static const char* ymn(bool b, bool m) { return m ? "\e[0;34m" "/" "\e[0m" : yon(b); }
#endif

static void AddOrEquality(CpModelBuilder& cp_model,
    const BoolVar& target,
    const std::vector<BoolVar>& vars) {
    cp_model.AddBoolOr(vars).OnlyEnforceIf(target);
    std::vector<BoolVar> vars_not;
    for (auto& var : vars)
        vars_not.push_back(var.Not());
    cp_model.AddBoolAnd(vars_not).OnlyEnforceIf(target.Not());
}

static void AddAndEquality(CpModelBuilder& cp_model,
    const BoolVar& target,
    const std::vector<BoolVar>& vars) {
    cp_model.AddBoolAnd(vars).OnlyEnforceIf(target);
    std::vector<BoolVar> vars_not;
    for (auto& var : vars)
        vars_not.push_back(var.Not());
    cp_model.AddBoolOr(vars_not).OnlyEnforceIf(target.Not());
}

class Plan {
    public:
        Plan(std::vector<Student>&& students) : students{students} {}

        struct schedule_result {
            Time start;
            Time end;
            const Student *student;
        };

        struct first_last_info {
            unsigned first;
            unsigned last;
            bool found;
        };
        std::array<first_last_info, 7> first_last_info_per_day{};
        std::array<BoolVar, slots_per_week> used{};
        std::array<BoolVar, slots_per_week> usage_before{};
        std::array<BoolVar, slots_per_week> usage_after{};
        std::array<BoolVar, slots_per_week> hole{};

        int64_t get_hole_weight(Time t, const struct solve_config& cfg) {
            const Time lunch_from(t.get_day(), cfg.lunch_time_from_hour, cfg.lunch_time_from_minute),
                       lunch_to(t.get_day(), cfg.lunch_time_to_hour, cfg.lunch_time_to_minute);

            // if the hole falls into a lunch break, that's OK! :-)
            if (t >= lunch_from && t < lunch_to)
                return -int64_t(cfg.lunch_hole_neg_prio);
            else
                return cfg.non_lunch_hole_prio;
        }

        void constraint_minimize_holes(CpModelBuilder& cp_model,
                            std::vector<BoolVar>& objective_var,
                            std::vector<int64_t>& objective_prio,
                            const struct solve_config& cfg) {

            std::vector<std::vector<BoolVar>> impact(slots_per_week);
            for (auto& student : students)
                student.register_impact(impact);

            const auto& FalseVar = cp_model.FalseVar();

            std::fill(used.begin(), used.end(), FalseVar);
            std::fill(usage_before.begin(), usage_before.end(), FalseVar);
            std::fill(usage_after.begin(), usage_after.end(), FalseVar);
            std::fill(hole.begin(), hole.end(), FalseVar);

            constexpr unsigned chunks_per_day = 24 * 60 / MIN_ALIGNMENT;
            for (unsigned day{}; day < 7; ++day) {
                auto& [first, last, found] = AT(first_last_info_per_day, day);
                for (unsigned chunk_of_day{}; chunk_of_day < chunks_per_day; ++chunk_of_day) {
                    const unsigned chunk_of_week = day * chunks_per_day + chunk_of_day;

                    const auto& impact_for_slot = AT(impact, chunk_of_week);
                    if (impact_for_slot.empty())
                        continue;

                    if (!found)
                        first = chunk_of_week;
                    last = chunk_of_week;
                    found = true;

                    const Time t(chunk_of_week);
                    AT(used, chunk_of_week) = cp_model.NewBoolVar().WithName(fmt::format("used {}", t));
                    AT(usage_before, chunk_of_week) = cp_model.NewBoolVar().WithName(fmt::format("usage_before {}", t));
                    AT(usage_after, chunk_of_week) = cp_model.NewBoolVar().WithName(fmt::format("usage_after {}", t));
                    AT(hole, chunk_of_week) = cp_model.NewBoolVar().WithName(fmt::format("hole {}", t));

                    objective_var.push_back(AT(hole, chunk_of_week));
                    objective_prio.push_back(get_hole_weight(t, cfg));

                    // used = wish1 | wish2 | ... | wishN
                    AddOrEquality(cp_model, AT(used, chunk_of_week), impact_for_slot);

                    // hole == ~used & usage_before & usage_after
                    AddAndEquality(cp_model, AT(hole, chunk_of_week), {AT(used, chunk_of_week).Not(), AT(usage_before, chunk_of_week), AT(usage_after, chunk_of_week)});
                }
            }
            for (unsigned day{}; day < 7; ++day) {
                auto& [first, last, found] = AT(first_last_info_per_day, day);
                if (!found) {
                    #ifdef DEBUG
                    fmt::println("{}: no used slots", Day(day));
                    #endif

                    continue;
                }

                #ifdef DEBUG
                fmt::println("{}: first={:t}, last={:t} ({} slots)", Day(day), Time(first), Time(last), last - first + 1);
                #endif

                {
                    std::vector<BoolVar> rest_of_day_before{};
                    for (unsigned chunk_of_day{first}; chunk_of_day <= last; ++chunk_of_day) {
                        auto& ub = AT(usage_before, chunk_of_day);
                        if (!rest_of_day_before.empty() && ub != FalseVar)
                            AddOrEquality(cp_model, ub, rest_of_day_before);
                        auto& u = AT(used, chunk_of_day);
                        if (u != FalseVar)
                            rest_of_day_before.push_back(u);
                    }
                }

                {
                    std::vector<BoolVar> rest_of_day_after{};
                    for (unsigned chunk_of_day{last}; chunk_of_day >= first; --chunk_of_day) {
                        auto& ua = AT(usage_after, chunk_of_day);
                        if (!rest_of_day_after.empty() && ua != FalseVar)
                            AddOrEquality(cp_model, AT(usage_after, chunk_of_day), rest_of_day_after);
                        auto& u = AT(used, chunk_of_day);
                        if (u != FalseVar)
                            rest_of_day_after.push_back(u);
                    }
                }
            }
        }

        bool schedule(const struct solve_config& cfg) {
            CpModelBuilder cp_model;

            std::vector<BoolVar> objective_var;
            std::vector<int64_t> objective_prio;
            bool objective{false};

            std::vector<std::list<std::tuple<BoolVar, unsigned>>> wishes(slots_per_week);
            for (auto& student : students)
                student.calculate_availabilities(cp_model, wishes, cfg);
            for (auto& student : students)
                student.register_conflicts(cp_model, wishes);

            if (cfg.minimize_wishes_prio) {
                for (const auto& l : wishes) {
                    for (const auto& [var, prio] : l) {
                        objective_var.push_back(var);
                        objective_prio.push_back(prio);
                    }
                }
                objective = true;
            }
            if (cfg.allow_skip) {
                for (auto& student : students) {
                    objective_var.push_back(student.get_skip_var());
                    objective_prio.push_back(cfg.skip_prio);
                }
            }

            if (cfg.minimize_holes) {
                constraint_minimize_holes(cp_model, objective_var, objective_prio, cfg);
                objective = true;
            }

            if (objective) {
                auto prio_sum = LinearExpr::WeightedSum(objective_var, objective_prio);
                cp_model.Minimize(prio_sum);
            }

            CpSolverResponse response;
            if constexpr (enumerate_all_solutions) {
                Model model;
                SatParameters parameters;
                parameters.set_linearization_level(0);
                parameters.set_enumerate_all_solutions(true);
                model.Add(NewSatParameters(parameters));

                unsigned solution_count{};
                model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& resp) {
                    fmt::println("SOLUTION {}", ++solution_count);
                    for (auto& student : students) {
                        if (cfg.allow_skip && student.is_skipped(resp)) {
                            fmt::println("SKIPPED {}", student.get_name());
                            continue;
                        }
                        const auto start = student.get_solution_time(resp);
                        const auto end = start + student.get_lesson_chunks();
                        fmt::println("{} - {}: {} ({})", start, end, student.get_name(), student.get_priority(start) + 1);
                    }
                }));
                response = SolveCpModel(cp_model.Build(), &model);
            } else {
                response = Solve(cp_model.Build());
            }

            if constexpr (print_stats)
                fmt::print("{}", CpSolverResponseStats(response));

            if (response.status() != CpSolverStatus::OPTIMAL)
                return false;

#ifdef DEBUG
            for (unsigned day{}; day < 7; ++day) {
                const auto& [first, last, found] = AT(first_last_info_per_day, day);
                if (!found)
                    continue;
                for (unsigned chunk_of_week{first}; chunk_of_week <= last; ++chunk_of_week) {
                    fmt::println("{}: used={}, usage_before={}, usage_after={}, hole={}",
                        Time(chunk_of_week),
                        yon(SolutionIntegerValue(response, AT(used, chunk_of_week))),
                        yon(SolutionIntegerValue(response, AT(usage_before, chunk_of_week))),
                        yon(SolutionIntegerValue(response, AT(usage_after, chunk_of_week))),
                        ymn(SolutionIntegerValue(response, AT(hole, chunk_of_week)), AT(hole, chunk_of_week) == cp_model.FalseVar()));
                }
            }
#endif

            skipped.clear();
            result.clear();
            for (auto& student : students) {
                if (cfg.allow_skip && student.is_skipped(response)) {
                    fmt::println("skipping {} ({})", student.get_name(), student.get_id());
                    skipped.push_back(&student);
                    continue;
                }
                const auto start = student.get_solution_time(response);
                const auto end = start + student.get_lesson_chunks();
                result.emplace_back(start, end, &student);
            }

            return true;
        }

        std::vector<schedule_result> get_result() {
            return result;
        }

        std::vector<const Student *> get_skipped() {
            return skipped;
        }

    protected:
        std::vector<Student> students;
        std::vector<schedule_result> result;
        std::vector<const Student *> skipped;
};
