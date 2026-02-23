#!/bin/bash


#----------------------------------------------
# remove the build files
#----------------------------------------------

do_cleanup() {

    echo "======================================================"
    echo "Cleaning up..."
    echo "======================================================"

    echo "Cleaning up the build folder .."
    rm -rf build_stm32f103/*

    echo "Cleaning up finished!"

}


#----------------------------------------------
# build and install
#----------------------------------------------

do_build() {

    echo "======================================================"
    echo "Building ..."
    echo "======================================================"

    mkdir -p build_stm32f103

    cd build_stm32f103
    echo "Cleaning up the build folder .."
    rm -rf *
    echo "Building in folder: build .."

    cmake -DCMAKE_TOOLCHAIN_FILE=stm32f1-arm-none-eabi.cmake ..

    make
    echo "Build finished !"
    cd -
}


#----------------------------------------------
# main
#----------------------------------------------

if [[ $1 == "clean" ]]; then
    do_cleanup
else
    do_build
fi

