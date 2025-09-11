#!/usr/bin/env python3

import gc
import json
from collections import namedtuple

# add PYTHONPATH to "studentplanner" location
from sys import path
path.append("install")

from studentplanner import solve

def test():
    Student = namedtuple("Student", ["id", "name", "lesson_duration", "availabilities"])
    Availability = namedtuple("Availability", ["day", "from_hour", "from_minute", "to_hour", "to_minute"])

    with open("availability_2023_1.json") as fd:
        j = json.load(fd)

    students = []

    for student_j in j:
        availabilities = [Availability(**availability) for availability in student_j["availabilities"]]
        students.append(Student(student_j["id"], student_j["name"], student_j["lesson_duration"], availabilities))

    s = solve(students)
    print(s)

def main():
    gc.disable()
    gc.collect()
    print(f"objects before: {len(gc.get_objects())}")
    test()
    gc.collect()
    print(f"objects after: {len(gc.get_objects())}")

if __name__ == "__main__":
    main()