IDA-LLVM
========

This project is an IDA Pro plugin which translates the assembler code into an LLVM intermediate representation.
The intermediate representation can then be inspected through python scripting to extract more advanced information.

Compiling
---------

The following instructions apply for Ubuntu 14.04. Most prerequisite steps, i.e., updating CMake, should not be
necessary on Ubuntu 16.04.

To compile the plugin, CMake 3.2 is needed (relevant for Ubuntu <= 14.04). Install it with the following commands:
    sudo apt-get install software-properties-common
    sudo add-apt-repository ppa:george-edison55/cmake-3.x
    sudo apt-get update
    sudo apt-get install cmake

Libqemu requires exactly LLVM 3.5, so install the packages from the LLVM nightly repository (The Ubuntu distro packages
don't contain the CMake configuration files necessary, this is why the nightly packages are needed).
trusty is for Ubuntu 14.04, replace with the correct name of your Ubuntu version
(see the [LLVM packages page](http://llvm.org/apt/) for details).
    sudo wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
    sudo sh -c "echo 'deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.5 main' > /etc/apt/sources.list.d/llvm.list"
    sudo sh -c "echo 'deb-src http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.5 main' >> /etc/apt/sources.list.d/llvm.list"

Install dependencies required by LLVM and Qemu:
    sudo apt-get install llvm-3.5 llvm-3.5-dev llvm-3.5-runtime clang-3.5
    sudo apt-get install zlib1g-dev libedit-dev libglib2.0-dev libfdt-dev

Then create a build directory for the plugin and create the Makefiles (replace the <> parts according to your installation):
   cmake -G "Unix Makefiles" -DIDA_DIRECTORY=<PATH_TO_YOUR_IDA_DIR> -DIDA_SDK_DIRECTORY=<PATH_TO_YOUR_IDA_SDK_DIR> <PATH_TO_CHECKED_OUT_PLUGIN_SOURCE>
   make
   make install



