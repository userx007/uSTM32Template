rm -rf build_stm32f411/

#    $ cmake -Bbuild -GNinja -DCMAKE_TOOLCHAIN_FILE=cmake/cortex_m4.cmake .

mkdir build_stm32f411 && cd build_stm32f411
cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi.cmake
ninja
