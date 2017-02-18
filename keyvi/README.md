# C++ keyvi library

## Compile

You need scons, simple compile by typing

    scons

That compiles the library in debug mode, to compile in release mode (required for the python extension):

    scons mode=release


### Pre-Requisites

#### Linux

In addition to a working gcc build environment, install the following libraries, e.g. using apt: boost (dev packages, at least version 1.54), snappy, zlib, cmake.

For example on Ubuntu (14.04 to 16.04) should install all the dependencies you need:

    apt-get install cmake cython g++ libboost-all-dev libsnappy-dev libzzip-dev python-stdeb zlib1g-dev

#### MAC

In addtion to a working build setup (Xcode) install the following libraries using homebrew:

    brew install boost --c++11
    brew install snappy
    brew install lzlib
    brew install cmake

Now you should be able to compile as explained above.
