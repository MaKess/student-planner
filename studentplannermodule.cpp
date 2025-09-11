#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define PLAN_PY
#ifdef USE_OR
#include "plan-or.hpp"
#else
#include "plan.hpp"
#endif

static PyStructSequence_Field studentplanner_result_fields[] = {
    {"id", "ID of the Student"},
    {"name", "Name of the Student"},
    {"day", "Day on which the student is scheduled"},
    {"from_hour", "Hour at which the lesson starts"},
    {"from_minute", "Minute at which the lesson starts"},
    {"to_hour", "Hour at which the lesson ends"},
    {"to_minute", "Minute at which the lesson ends"},
    {nullptr}
};

static PyStructSequence_Desc studentplanner_result_desc = {
    "result",
    "tuple of scheduling results for one single student",
    studentplanner_result_fields,
    7
};

static PyTypeObject* result_type = nullptr;

struct PyObjectGuard {
    PyObjectGuard() {}
    PyObjectGuard(PyObject* obj) : obj{obj} {}
    ~PyObjectGuard() { Py_DecRef(obj); }
    operator PyObject*() { return obj; }
    PyObject* obj = nullptr;
};

static unsigned long getattr_unsigned_long(PyObject* obj, const char* attr) {
    PyObjectGuard obj_attr = PyObject_GetAttrString(obj, attr);
    if (!obj_attr)
        throw std::runtime_error(fmt::format("attribute '{}' does not exist", attr));
    const auto value = PyLong_AsUnsignedLong(obj_attr);
    if (value == (unsigned long)-1 && PyErr_Occurred())
        throw std::runtime_error(fmt::format("attribute {} was not a long integer", attr));
    return value;
}

static std::string getattr_string(PyObject* obj, const char* attr) {
    PyObjectGuard obj_attr = PyObject_GetAttrString(obj, attr);
    if (!obj_attr)
        throw std::runtime_error(fmt::format("attribute '{}' does not exist", attr));

    PyObjectGuard py_obj_name_unicode = PyUnicode_FromObject(obj_attr);
    if (!py_obj_name_unicode)
        throw std::runtime_error(fmt::format("attribute '{}' cannot be converted to unicode", attr));
    PyObjectGuard py_obj_name_utf8 = PyUnicode_AsUTF8String(py_obj_name_unicode);
    if (!py_obj_name_utf8)
        throw std::runtime_error(fmt::format("attribute '{}' cannot be converted to UTF-8 string", attr));

    return PyBytes_AsString(py_obj_name_utf8);
}

static PyObject* getattr_list(PyObject* obj, const char* attr) {
    PyObject* obj_attr = PyObject_GetAttrString(obj, attr); // New reference
    if (!obj_attr)
        throw std::runtime_error(fmt::format("attribute '{}' does not exist", attr));
    if (!PyList_Check(obj_attr))
        throw std::runtime_error(fmt::format("attribute '{}' is not a list", attr));
    return obj_attr;
}

static std::vector<Student> read_student_config(PyObject* py_list_students) {
    const auto students_count = PyList_Size(py_list_students);
    std::vector<Student> students;
    students.reserve(students_count);

    for (Py_ssize_t students_index{0}; students_index < students_count; ++students_index) {
        PyObject* py_obj_student = PyList_GetItem(py_list_students, students_index); // Borrowed reference
        if (!py_obj_student)
            throw std::runtime_error("could not retrieve student from list");

        const auto id = getattr_unsigned_long(py_obj_student, "id");
        const auto name = getattr_string(py_obj_student, "name");
        const auto lesson_duration = getattr_unsigned_long(py_obj_student, "lesson_duration");
        const unsigned student_prio = students_index + 1;

        Student student(id, name, lesson_duration, student_prio);

        student.py_obj_name = PyObject_GetAttrString(py_obj_student, "name");

        PyObjectGuard py_obj_availabilies = getattr_list(py_obj_student, "availabilities");
        const auto availabilities_count = PyList_Size(py_obj_availabilies);
        for (Py_ssize_t availabilities_index{0}; availabilities_index < availabilities_count; ++availabilities_index) {
            PyObject* py_obj_availability = PyList_GetItem(py_obj_availabilies, availabilities_index); // Borrowed reference
            const auto day         = parse_day(getattr_string(py_obj_availability, "day"));
            const auto from_hour   = getattr_unsigned_long(py_obj_availability, "from_hour");
            const auto from_minute = getattr_unsigned_long(py_obj_availability, "from_minute");
            const auto to_hour     = getattr_unsigned_long(py_obj_availability, "to_hour");
            const auto to_minute   = getattr_unsigned_long(py_obj_availability, "to_minute");
            student.add_availability(Time(day, from_hour, from_minute), Time(day, to_hour, to_minute));
        }

        students.push_back(student);
    }

    return students;
}

static PyObject* export_schedult_result(const std::vector<Plan::schedule_result>& result) {
    PyObject* result_list = PyList_New(result.size());
    Py_ssize_t result_list_index{0};
    for (const auto& student_result : result) {
        PyObject* result_tuple = PyStructSequence_New(result_type);
        PyStructSequence_SetItem(result_tuple, 0, PyLong_FromLong(student_result.student->get_id()));

        PyObject* name = student_result.student->py_obj_name;
        //Py_IncRef(name);
        PyStructSequence_SetItem(result_tuple, 1, name);

        Day day = student_result.start.get_day();
        // TODO: pass this as string
        PyStructSequence_SetItem(result_tuple, 2, PyLong_FromUnsignedLong(unsigned(day) + 1));

        PyStructSequence_SetItem(result_tuple, 3, PyLong_FromUnsignedLong(student_result.start.get_hour()));
        PyStructSequence_SetItem(result_tuple, 4, PyLong_FromUnsignedLong(student_result.start.get_minute()));
        PyStructSequence_SetItem(result_tuple, 5, PyLong_FromUnsignedLong(student_result.end.get_hour()));
        PyStructSequence_SetItem(result_tuple, 6, PyLong_FromUnsignedLong(student_result.end.get_minute()));

        PyList_SetItem(result_list, result_list_index++, result_tuple);
    }
    return result_list;
}
static PyObject* export_schedult_skipped(const std::vector<const Student*>& skipped) {
    PyObject* result_list = PyList_New(skipped.size());
    Py_ssize_t result_list_index{0};
    for (const auto student_skipped : skipped) {
        PyObject* skipped_id = PyLong_FromLong(student_skipped->get_id());
        PyList_SetItem(result_list, result_list_index++, skipped_id);
    }
    return result_list;
}

static PyObject* studentplanner_solve(PyObject* self, PyObject* args, PyObject* keywds) {
    static const char* kwlist[] = {
        "students",
        "range_attempts",
        "range_increment",
        "minimize_wishes_prio",
        "minimize_holes",
        "availability_index_scale",
        "lunch_time_from_hour",
        "lunch_time_from_minute",
        "lunch_time_to_hour",
        "lunch_time_to_minute",
        "lunch_hole_neg_prio",
        "non_lunch_hole_prio",
        "allow_skip",
        "skip_prio",
        nullptr
    };
    PyObject* py_list_students;
    struct solve_config cfg = {
        .range_attempts = default_range_attempts,
        .range_increment = default_range_increment,
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

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!|IIppIIIIIIIpI", (char**) kwlist,
        &PyList_Type, &py_list_students,
        &cfg.range_attempts,
        &cfg.range_increment,
        &cfg.minimize_wishes_prio,
        &cfg.minimize_holes,
        &cfg.availability_index_scale,
        &cfg.lunch_time_from_hour,
        &cfg.lunch_time_from_minute,
        &cfg.lunch_time_to_hour,
        &cfg.lunch_time_to_minute,
        &cfg.lunch_hole_neg_prio,
        &cfg.non_lunch_hole_prio,
        &cfg.allow_skip,
        &cfg.skip_prio))
        return nullptr;

    Plan plan(read_student_config(py_list_students));
    const bool success = plan.schedule(cfg);
    if (!success) {
        PyErr_SetString(PyExc_RuntimeError,  "could not create plan");
        return nullptr;
    }

    const auto result = plan.get_result();
    const auto skipped = plan.get_skipped();
    return PyTuple_Pack(2, export_schedult_result(result), export_schedult_skipped(skipped));
}

static PyMethodDef StudentPlannerMethods[] = {
    {"solve", (PyCFunction) studentplanner_solve, METH_VARARGS | METH_KEYWORDS, "provide an optimal scheduling for the given constraints"},
    {nullptr}
};

static struct PyModuleDef StudentPlannerModule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "studentplanner",
    .m_doc = "Solve challenging student scheduling",
    .m_size = -1,
    .m_methods = StudentPlannerMethods
};

static PyObject* studentplanner_module = nullptr;

PyMODINIT_FUNC
PyInit_studentplanner(void) {
    // TODO: create better exception classes
    // PyErr_NewException

    if (!studentplanner_module)
        studentplanner_module = PyModule_Create(&StudentPlannerModule);

    if (!result_type)
        result_type = PyStructSequence_NewType(&studentplanner_result_desc);
    PyModule_AddObject(studentplanner_module, "result", (PyObject *) result_type);

    return studentplanner_module;
}
