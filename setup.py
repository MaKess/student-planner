#!/usr/bin/env python3

"Solve challenging student scheduling"

from setuptools import setup, Extension

# OR_PATH = "/pools/datapool/home/martin/coding/or-tools_x86_64_Ubuntu-22.04_cpp_v9.10.4067"
OR_PATH = "/pools/datapool/home/martin/coding/or-tools_x86_64_Ubuntu-22.04_cpp_v9.12.4544"

module = Extension(
    "studentplanner",
    sources=[
        "studentplannermodule.cpp",
    ],
    include_dirs = [
        "include",
        "fmt/include",
        f"{OR_PATH}/include",
    ],
    define_macros = [
        ("OR_PROTO_DLL", ""), # needed for v9.12
    ],
    extra_compile_args=[
        "-std=c++20",
    ],
    library_dirs = [
        f"{OR_PATH}/lib",
        "fmt/build",
    ],
    libraries = [
        "ortools",
        "fmt",
    ],
    extra_link_args=[
        f"-Wl,-rpath,{OR_PATH}/lib",
    ],
)

setup(name="studentplanner",
      version='1.0',
      description=__doc__,
      ext_modules=[module])
