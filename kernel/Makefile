# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
CC ?= clang
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -pedantic
.PHONY: all test bitcode clean
all: build/test_kernel
build/test_kernel: src/skytrace.c src/skytrace.h tests/test_kernel.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ src/skytrace.c tests/test_kernel.c
test: build/test_kernel
	./build/test_kernel
bitcode:
	mkdir -p build
	clang -g -O0 -emit-llvm -c src/skytrace.c -o build/skytrace.bc
clean:
	rm -rf build
