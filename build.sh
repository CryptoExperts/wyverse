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

copy_wyverse_to_llvm() {
    # keep the slash
    rsync -av lib/ source/llvm/lib/
    rsync -av tools/ source/llvm/tools/
    rsync -av include/ source/llvm/include/
}

generate_build_scripts() {
    mkdir -p build && mkdir -p install
    cd build
    cmake -G "Ninja" \
	  -DCMAKE_SKIP_INSTALL_ALL_DEPENDENCY=TRUE \
	  -DCMAKE_INSTALL_PREFIX=../install \
	  -DLLVM_TARGETS_TO_BUILD="X86" \
	  -DLLVM_ENABLE_RTTI=ON \
	  ../source/llvm
    cd ..
}

build() {
    cd build
    ninja wyverse
    cd ..
}

test_example() {
    # init a git repo
    mkdir -p tests/kryptologik && cd tests/kryptologik
    git init
    git remote add -f origin https://github.com/SideChannelMarvels/Deadpool.git

    # sparse checkout
    git config core.sparseCheckout true
    echo "wbs_aes_kryptologik/target/" >> .git/info/sparse-checkout
    git pull --depth=1 origin master
    cp wbs_aes_kryptologik/target/DemoKey_table_encrypt.c aes.c
    cp wbs_aes_kryptologik/target/DemoKey_table.bin .
    clang -emit-llvm -S -c  aes.c -o aes.ll
    ../../build/bin/wyverse -trace aes.ll 00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff

    # exit
    cd ../..
}

download_llvm_and_clang && copy_wyverse_to_llvm
generate_build_scripts
build
test_example
