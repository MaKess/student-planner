#!/usr/bin/env python3

"Solve challenging student scheduling"

from distutils.core import setup, Extension

# OR_PATH = "/pools/datapool/home/martin/coding/or-tools_x86_64_Ubuntu-22.04_cpp_v9.10.4067"
OR_PATH = "/pools/datapool/home/martin/coding/or-tools_x86_64_Ubuntu-22.04_cpp_v9.12.4544"

module = Extension(
    "studentplanner",
    sources=[
        "studentplannermodule.cpp",
    ],
    extra_compile_args=[
        "-DUSE_OR",
        "-DOR_PROTO_DLL=", # needed for v9.12
        "-std=c++20",
        "-I", "fmt/include",
        "-I", f"{OR_PATH}/include",
    ],
    extra_link_args=[
        "-L", f"{OR_PATH}/lib",
        f"-Wl,-rpath,{OR_PATH}/lib",
        "-lortools",
        "-L", "fmt/build",
        "-lfmt",
    ],
)

setup(name="studentplanner",
      version='1.0',
      description=__doc__,
      ext_modules=[module])
