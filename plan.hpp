#include <string>
#include <vector>
#include "config.hpp"
#include "time.hpp"

constexpr unsigned default_range_attempts = 2;
constexpr unsigned default_range_increment = 2;

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

        #ifdef PLAN_PY
        PyObject* py_obj_name = nullptr;
        #endif

    protected:
        const unsigned id;
        const std::string name;
        const unsigned lesson_duration;
        const unsigned student_prio;
        std::vector<std::pair<Time, Time>> availability_ranges; // pair = (start_time, end_time)
        std::vector<Time> availabilities;
};

class Plan {
    public:
        Plan(std::vector<Student>&& students) : students{students} {}

        bool schedule(unsigned range_attempts=1, unsigned range_increment=1) {
            planning.resize(slots_per_week);

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
