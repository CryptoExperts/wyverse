#!/usr/bin/env bash

set -x
set -e


LLVM_VER=8.0.0
CMAKE_INSTALL_PREFIX=install

download_llvm_and_clang() {
    mkdir -p source/llvm
    cd source
    wget http://releases.llvm.org/8.0.0/llvm-${LLVM_VER}.src.tar.xz
    tar Jxf llvm-8.0.0.src.tar.xz -C llvm --strip-components=1
    cd ..
}

copy_TOOL() {
    # keep the slash
    rsync -av lib/ source/llvm/lib/
    rsync -av tools/ source/llvm/tools/
    rsync -av include/ source/llvm/include/
}

build() {
    mkdir -p build && mkdir -p install
    cd build
    cmake -G "Ninja" \
	  -DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=TRUE \
	  -DCMAKE_INSTALL_PREFIX=../install \
	  -DLLVM_TARGETS_TO_BUILD="X86" \
	  -DLLVM_ENABLE_RTTI=ON \
	  ../source/llvm
    ninja wyverse
    cd ..
}

rebuild() {
    cd build
    ninja wyverse
    cd ..
}

test() {
    clang -emit-llvm -S -c tests/BranchInstructions.c -o test.ll
    build/bin/wyverse -trace test.ll
}

download_llvm_and_clang
copy_TOOL
build
