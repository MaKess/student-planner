#!/usr/bin/env bash

set -ex

OR_PATH=/pools/datapool/home/martin/coding/or-tools_x86_64_Ubuntu-22.04_cpp_v9.12.4544

gcc \
	-Wno-unused-result \
	-Wsign-compare \
	-DNDEBUG \
	-fwrapv \
	-O2 \
	-Wall \
	-fstack-protector-strong \
	-Wformat \
	-Werror=format-security \
	-Wdate-time \
	-fPIC \
	-DOR_PROTO_DLL= \
	-Iinclude \
	-Ifmt/include \
	-I${OR_PATH}/include \
	-I/usr/include/python3.10 \
	-c \
	studentplannermodule.cpp \
	-o studentplannermodule.o \
	-std=c++20
g++ \
	-shared \
	-Wl,-O1 \
	-Wl,-Bsymbolic-functions \
	-fwrapv \
	-O2 \
	-fstack-protector-strong \
	-Wformat \
	-Werror=format-security \
	-Wdate-time \
	studentplannermodule.o \
	-L${OR_PATH}/lib \
	-Lfmt/build \
	-Wl,--enable-new-dtags,-R${OR_PATH}/lib \
	-lortools \
	-lfmt \
	-o studentplanner.so
