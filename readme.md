Pelconf and mkdeps: configuring and building C/C++ software
===========================================================

The mkdeps and pelconf tools are intended to provide a configuration and build system
that is flexible and simple to use. In general terms mkdeps maintains the dependencies
and helps to maintain the makefile, while the pelconf adapts the source code to the
current compilation environment.

The mkdeps program expects that source files are organized in pairs of *.hpp and *.cpp
files. For each file xxx.hpp that defines the public interface of the translation unit
xxx there is an implementation file called xxx.cpp. Note that we talk about
translation units in the sense of the C++ standard. Each translation unit may contain
many classes. It is expected that all items in the project are contained in the
namespace of the project. The project-dir path should reflect the name of the
namespace. For instance if the project is contained in the namespaces:

	namespace peltk {  namespace base11 {

then project-dir should be peltk/base11/

In this way we do not need to worry about name collisions, as long as we keep the
namespaces separate.

The organization of the source tree shall be as follows:

 project-dir/
	readme.md
	makefile
	bin/
	doc/
	src/
	test/

The src directory contains both the *.cpp and *.hpp files. There is no distinction in 
this directory whether a file is intended to be a public interface or not. You can 
have several main programs and several libraries in the src directory. The mkdeps 
program reads the source files and determines which programs and libraries it has to 
build and which source files are required for each. Programs are detected by the
presence of the declaration of main() in the source file. For each source file 
defining main a rule will be created in the make file to build the corresponding 
program. The source files that are required to build each program are the one 
declaring main() and all the CPP files that correspond to the files included by 
main(). This is expanded recursively. For example assume that foo.cpp declares a main 
function and #includes bar1.hpp and bar2.hpp. If bar1.cpp and bar2.cpp are present in 
the source directories they will be assumed to be required to create foo. Furthermore 
if bar2.cpp includes foobar.hpp and foobar.cpp is present then it will also be assumed 
to be required to build foo. This is a transitive depends-on property. You may have 
several programs in the same directory and mkdeps will sort out which files are 
required to build each. The only condition is that you always pair each cpp file with 
the corresponding hpp file.

A library will be represented here with a file of the form libxxx.cpp, where xxx is to 
be the name of the file. It will start with a comment /* LIBRARY */ and will include 
the headers that are part of the public interface of the library. mkdeps will then 
create a makefile that will contain the dependencies required to create a library that 
supports those interfaces. It will also produce the list of files that must be copied 
to /usr/local/include in order to have this interface available. This list will 
include the files included by libxxx.cpp and any other files that are included by them.

The make file shall use the VPATH technique to include both src/ and test/ as the
potential directories for source files. If other source directories are present (for
instance experimental/) they should also be added to the VPATH.

For the purposes of mkdeps you can use several directories to distribute the source
files and you can specify the include directories. So if you wish you can have some of
the include files in an include directory and some of the source files in another
directory.

Letting mkdeps figure out which files are needed for each target simplifies the
maintenance of the makefile. You must only define the pattern rules that compile each
kind of source. The maintenance of the dependencies is done automatically by mkdeps.


pelconf creates the config.h header and modifies the makefile to adapt it to the
compilation environment. The concept of pelconf is the same as autoconf. The main
difference from autoconf is that the tests are written in C and not in M4 and bash.
Therefore there is no need to learn a different language: you are already programming
in C/C++ and you just write each test in C. The pelconflib.c file provides the support
required to make this easy. Your tests are written in pelconf.c. The configure script
just compiles pelconf.c and pelconflib.c together and runs pelconf. pelconf will run
predefined tests and your own specific tests. It will create a config.h file and will
modify an existing makefile.in file to create a makefile. For the user of the program
there is no difference from autoconf. You just run the commands ./configure; make;
make install. For the creator of the program pelconf is much easier due to the use of
C to specify the tests.


