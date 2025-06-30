# uSTM32Template
Template for building STM32 projects using libcm3

## Install the toolchain
    wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
    tar -xf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
    mv arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi arm-gnu-toolchain
    echo 'export PATH=~/gcc-arm-none-eabi/arm-gnu-toolchain/bin:$PATH' >> ~/.bashrc
    source ~/.bashrc

### check the installation status
    cat ~/.bashrc
    arm-none-eabi-gcc --version

## clone the libopencm3 repository
    git clone https://github.com/libopencm3/libopencm3.git
