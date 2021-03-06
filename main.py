#!/usr/bin/env pypy3

import argparse
import json
import re
from typing import Dict, Iterable, Tuple, Union, List, Any
from enum import IntEnum, auto
from functools import total_ordering

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet
from reportlab.platypus import SimpleDocTemplate, Table
from reportlab.platypus.paragraph import Paragraph

MIN_ALIGN = 10

class Day(IntEnum):
    MONDAY = auto()
    TUESDAY = auto()
    WEDNESDAY = auto()
    THURSDAY = auto()
    FRIDAY = auto()

@total_ordering
class Time:
    def __init__(self, h: int, m: int) -> None:
        assert 0 <= h <= 23
        assert 0 <= m <= 59
        self._h: int = h
        self._m: int = m

    def is_minute_aligned(self, m: int) -> bool:
        return self._m % m == 0

    def round_up(self, r: int) -> "Time":
        m = self._m
        m -= m % -r
        h_add, self._m = divmod(m, 60)
        self._h += h_add
        return self

    def copy(self) -> "Time":
        return Time(self._h, self._m)

    def __iadd__(self, other: Union["Time", int]) -> "Time":
        h = self._h
        m = self._m
        if isinstance(other, Time):
            m += other._m
            h += other._h
        elif isinstance(other, int):
            m += other
        else:
            raise TypeError

        h_add, self._m = divmod(m, 60)
        self._h = h + h_add
        return self

    def __add__(self, other: Union["Time", int]) -> "Time":
        ret = Time(self._h, self._m)
        ret += other
        return ret

    def __eq__(self, other: "Time"):
        return ((self._h, self._m) == (other._h, other._m))

    def __ne__(self, other: "Time"):
        return not (self == other)

    def __lt__(self, other: "Time"):
        return ((self._h, self._m) < (other._h, other._m))

    def __hash__(self) -> int:
        return self._h * 60 + self._m

    def __str__(self) -> str:
        return f"{self._h:d}h{self._m:02d}"

class Student:
    def __init__(self, name: str, lesson_duration: int) -> None:
        assert lesson_duration > 0
        assert lesson_duration % MIN_ALIGN == 0
        self._name: str = name
        self._lesson_duration: int = lesson_duration
        self._availabilities: List[tuple[Day, Time, Time]] = []

    def get_name(self) -> str:
        return self._name

    def get_lesson_duration(self) -> int:
        return self._lesson_duration

    def add_availability(self, day: Day, start: Time, end: Union[None, Time]=None) -> None:
        assert start.is_minute_aligned(MIN_ALIGN)
        assert end is None or end.is_minute_aligned(MIN_ALIGN)
        self._availabilities.append((day, start, end))

    def iter_availabilities(self, range_attempts: int, range_increment: int) -> Iterable[Tuple[Day, Time]]:
        for prio, (day, start, end) in enumerate(self._availabilities):
            prio += 1 # humans start counting from 1
            yield day, start, prio
            if end is None:
                continue

            attemps = 0
            time = start + range_increment
            while time < end and attemps < range_attempts:
                yield day, time, prio
                time = time + range_increment
                attemps += 1

    def __repr__(self) -> str:
        return f"<Student: {self._name}>"

class Plan:
    def __init__(self) -> None:
        self._students: List[Student] = []
        self._student_map: Dict[str, Student] = {}
        self._availability: Dict[Day, Dict[Time, Any]] = {d: {} for d in Day}

    def export_availability(self):
        ret = []
        for student_id, student in enumerate(self._students):
            lesson_duration = student.get_lesson_duration()

            availabilities = []
            for day, start, end in student._availabilities:
                if end is None:
                    end = start + lesson_duration
                availabilities.append({
                    "day": day.name,
                    "from_hour": start._h,
                    "from_minute": start._m,
                    "to_hour": end._h,
                    "to_minute": end._m,
                })

            ret.append({
                "name": student.get_name(),
                "id": student_id,
                "lesson_duration": lesson_duration,
                "availabilities": availabilities,
            })
        return ret

    def add_student(self, student: Student) -> None:
        self._students.append(student)
        self._student_map[student.get_name()] = student

    def get_student(self, name: str) -> Student:
        return self._student_map.get(name)

    def add_availability(self, day: Day, start: Time, end: Time) -> None:
        day_availability = self._availability[day]
        t = start.copy()
        while t < end:
            day_availability[t] = None
            t = t + MIN_ALIGN # since we use "t" as a key, we have to create a copy for the next iteration

    def take_available(self, day: Day, time: Time, duration: int, take_key: Any) -> bool:
        assert duration % MIN_ALIGN == 0
        assert take_key is not None

        day_availability = self._availability[day]

        for t in range(0, duration, MIN_ALIGN):
            time_check = time + t
            try:
                if day_availability[time_check] is not None:
                    return False
            except KeyError:
                return False

        for t in range(0, duration, MIN_ALIGN):
            time_check = time + t
            day_availability[time_check] = take_key

        return True

    def clear_availability(self, day: Day, time: Time, duration: int) -> None:
        assert duration % MIN_ALIGN == 0

        day_availability = self._availability[day]

        for t in range(0, duration, MIN_ALIGN):
            time_check = time + t
            day_availability[time_check] = None

    def schedule(self, range_attempts=1, range_increment=MIN_ALIGN) -> bool:
        # ideas:
        # try to place one student after another
        #  - tyring one availability after another
        #  - backtrack if there are no options left for a given student
        #  - IMPROVEMENT:
        #    - instead of backtracking, remove *only* the students that have conflicting time slots.
        #      BUT: how to ensure that we're not trying constellations that we already tried again and again in a loop?

        def schedule_students(students: List[Student], depth: int = 0) -> bool:
            if not students:
                return True

            student = students[0]
            lesson_duration = student.get_lesson_duration()

            for availability_day, availability_start, prio in student.iter_availabilities(range_attempts, range_increment):
                #print(f"trying {student.get_name()} on {availability_day.name} from {availability_start} for {lesson_duration}min")

                if not self.take_available(availability_day, availability_start, lesson_duration, (student, prio)):
                    continue

                # if scheduling succeeds, continue with the next student
                if schedule_students(students[1:], depth + 1):
                    return True

                #print(f"backtrack {student.get_name()}")
                # scheduling one of the next students was not sucessfull, so we have to rewind and try our next slot
                self.clear_availability(availability_day, availability_start, lesson_duration)
            else:
                #print(f"no options found for {student.get_name()}")
                # none of our availabilities worked
                return False

        return schedule_students(self._students)

    def print_schedule(self) -> None:
        for day, day_schedule in self._availability.items():
            if not day_schedule:
                continue

            print(f"{day.name}:")
            for time, student_prio in day_schedule.items():
                if not student_prio:
                    continue
                student, prio = student_prio
                print(f" - {time}: {student.get_name()} ({prio})")

    def export_pdf(self, fd, title=None):
        doc = SimpleDocTemplate(fd,
                                pagesize=A4,
                                title=title if title else None,
                                author="",
                                subject="",
                                producer="",
                                creator="",
                                topMargin=35,
                                bottomMargin=35)

        styles = getSampleStyleSheet()
        styleH = styles['Heading1']

        headline = [""]

        data = [headline]

        style = [
            ('GRID', (0,0), (-1,-1), 0.5, colors.black),

            #('BACKGROUND', (3, 3), (4, 4), colors.pink),

            #('SPAN', (0,1), (1,2)),
            #('SPAN', (-2,-2), (-1,-1)),
        ]

        time_min = Time(9, 0)
        time_max = Time(21, 0)
        time_map = {}
        time_y = {}
        time = time_min.copy()
        y = 1
        while time <= time_max:
            time_line = [str(time)] + [""] * len(Day)
            time_map[time] = time_line
            time_y[time] = y

            data.append(time_line)

            time = time + MIN_ALIGN
            y += 1

        for day, day_schedule in self._availability.items():
            headline.append(day.name)

            if not day_schedule:
                continue

            for time, student_prio in day_schedule.items():
                if not student_prio:
                    continue

                student, prio = student_prio

                time_map[time][day] = student.get_name()

                x = int(day)
                y = time_y[time]
                coord = (x, y)

                if prio == 1:
                    color = colors.limegreen
                elif prio == 2:
                    color = colors.yellow
                else:
                    color = colors.lightcoral

                style.append(("BACKGROUND", coord, coord, color))

        elements = []
        if title:
            elements.append(Paragraph(title, styleH))
        elements.append(Table(data, style=style))
        doc.build(elements)

def read_students(fd):
    fr_en = {
        "Lundi": Day.MONDAY,
        "Mardi": Day.TUESDAY,
        "Mercredi": Day.WEDNESDAY,
        "Jeudi": Day.THURSDAY,
        "Vendredi": Day.FRIDAY,
    }

    def get_int(array, column, fallback):
        if len(array) <= column:
            return fallback
        s = array[column].strip()
        if not s:
            return fallback
        return int(s)

    for line in fd:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        first_name, last_name, lesson_duration, *availabilities = line.split("\t")
        student = Student(f"{first_name} {last_name.upper()}", int(lesson_duration))

        choice = 0
        while True:
            try:
                parts = availabilities[choice * 5:]
                choice += 1

                try:
                    day = fr_en[parts[0]]
                except KeyError:
                    print(f"{student.get_name()} is stupid ({parts[0]})")
                    continue

                from_h = get_int(parts, 1, None)
                from_min = get_int(parts, 2, 0)
                availability_from = Time(from_h, from_min)
                availability_from.round_up(MIN_ALIGN)

                to_h = get_int(parts, 3, None)
                to_min = get_int(parts, 4, 0)
                availability_to = None if to_h is None else Time(to_h, to_min).round_up(MIN_ALIGN)

                student.add_availability(day, availability_from, availability_to)
            except IndexError:
                break

        yield student

def read_schedule(fd, plan: Plan):
    day_map = {f"{day.name}:": day for day in Day}
    current_day = None

    schedule_regex = re.compile(r" - ([0-9]{1,2})h([0-9]{1,2}): (.+) \(([1-9])\)")

    for line in fd:
        line = line.rstrip()

        check_day = day_map.get(line)
        if check_day is not None:
            current_day = check_day
            continue

        m = schedule_regex.match(line)

        if m is not None:
            start_h, start_m, name, prio = m.groups()
            student = plan.get_student(name)
            start = Time(int(start_h), int(start_m))
            plan.take_available(current_day, start, student.get_lesson_duration(), (student, int(prio)))

    return True

def parse_args():
    parser = argparse.ArgumentParser(description='Primitive constraint solver for student scheduling.')
    parser.add_argument("--availability", "-a", type=argparse.FileType("r"), required=True, help="student availability")
    parser.add_argument("--range-attempts", "-r", type=int, default=1, help="number of increments that are attempted for availability ranges")
    parser.add_argument("--range-increment", "-i", type=int, default=MIN_ALIGN * 2, help="increments that is for availability ranges")
    parser.add_argument("--read-schedule", "-s", type=argparse.FileType("r"), help="read back an already printed schedule")
    parser.add_argument("--export-pdf", "-p", type=argparse.FileType("wb"), help="export the schedule as a PDF")
    parser.add_argument("--export-json-input", type=argparse.FileType("w"), help="export the student settings as JSON")

    return parser.parse_args()

def main(args) -> Union[None, int]:
    plan = Plan()

    plan.add_availability(Day.MONDAY,    Time(14, 0),  Time(21, 0))
    plan.add_availability(Day.WEDNESDAY, Time(9, 0),   Time(12, 30))
    plan.add_availability(Day.WEDNESDAY, Time(13, 30), Time(15, 0))
    plan.add_availability(Day.WEDNESDAY, Time(16, 50), Time(21, 0))
    plan.add_availability(Day.THURSDAY,  Time(16, 0), Time(21, 0))
    plan.add_availability(Day.FRIDAY,    Time(13, 0),  Time(18, 30))
    plan.add_availability(Day.FRIDAY,    Time(19, 30), Time(21, 0))

    for student in read_students(args.availability):
        plan.add_student(student)

    if args.export_json_input:
        json.dump(plan.export_availability(), args.export_json_input, indent=4)

    if args.read_schedule:
        success = read_schedule(args.read_schedule, plan)
    else:
        success = plan.schedule(range_attempts=args.range_attempts, range_increment=args.range_increment)

    if success:
        plan.print_schedule()
        if args.export_pdf:
            plan.export_pdf(args.export_pdf, f"Student schedule r{args.range_attempts} i{args.range_increment}")
    else:
        print("no schedule found")

if __name__ == "__main__":
    exit(main(parse_args()))