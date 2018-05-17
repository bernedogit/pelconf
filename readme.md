Pelconf and mkdeps: configuring and building C/C++ software
===========================================================

The *mkdeps* and *pelconf* tools are intended to provide a configuration and build system
that is flexible and simple to use. In general terms *mkdeps* maintains the dependencies
and helps to maintain the makefile, while the *pelconf* adapts the source code to the
current compilation environment.

The *mkdeps* program expects that source files are organized in pairs of implementation
and interface files. For instance, when using C++ for each file xxx.hpp that defines
the public interface of the translation unit xxx there is an implementation file
called xxx.cpp. Note that we talk about translation units in the sense of the C++
standard. Each translation unit may contain many classes. See the file *mkdeps*.md for
details.

For the purposes of *mkdeps* you can use several directories to distribute the source
files and you can specify the include directories. So if you wish you can have some of
the include files in an include directory and some of the source files in another
directory.

Letting *mkdeps* figure out which files are needed for each target simplifies the
maintenance of the makefile. You must only define the pattern rules that compile each
kind of source. The maintenance of the dependencies is done automatically by *mkdeps*.

The *pelconf* program examines the compilation environment and configures the program.
*pelconf* creates the config.h header and modifies the makefile to adapt it to the
compilation environment. The concept of *pelconf* is the same as autoconf. The main
difference from autoconf is that the tests are written in C and not in M4 and bash.
Therefore there is no need to learn a different language: you are already programming
in C/C++ and you just write each test in C. The pelconflib.c file provides the support
required to make this easy. Your tests are written in *pelconf*.c. The configure script
just compiles *pelconf*.c and pelconflib.c together and runs *pelconf*. *pelconf* will
run predefined tests and your own specific tests. It will create a config.h file and
will modify an existing makefile.in file to create a makefile. For the user of the
program there is no difference from autoconf. You just run the commands ./configure;
make; make install. For the creator of the program *pelconf* is much easier due to the
use of C to specify the tests. See *pelconflib.md* for details.

