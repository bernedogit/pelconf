MKDEPS
======

This is a small utility to maintain the dependencies part of makefiles. It scans the
source files given in the command line and updates the dependencies part in the
makefile. It is similar to other existing tools, but it also supports the automatic
computation of the source files required to compile a program or library.

Mkdeps assumes that programs and libraries are made of C/C++ implementation files and
header files. Implementation files have usually the extension .c or .cpp, but *mkdeps*
does not care about it. Header files have usually the extension .h or .hpp, but
*mkdeps* does not care about it. Mkdeps considers that implementation and header files
come in pairs. For instance the file *tables.cpp* will implement the facilities
declared in the file *tables.hpp*. This is not always required. For instance the
implementation file that contains the function *main()* will not have a corresponding
header file. There are also header files which do not have a corresponding
implementation file. This is likely the case of configuration files and header only
libraries.

*Mkdeps* expects as arguments the list of implementation files to be considered. It
will scan these files and check if they include other files. The included files will
also be scanned recursively in order to find out all the files included by each of the
source files. Those included files that could be found will be then considered to be
the prerequisites for the object file corresponding to each implementation file. For
instance assume that the implementation file *foo.cpp* was passed in the command line
and that it includes the files *foo.hpp* and *bar.hpp". *Mkdeps* will scan the file
*foo.cpp* and will then attempt to find the files *foo.hpp* and *bar.hpp* in the
current directory or any of the include directories supplied with the `-I` option. If
it finds them then they will be included as prerequisites for *foo.o*. Assume that
*bar.hpp* could be found and that it includes *foobar.hpp*. If *foobar.hpp* could be
found then it will also be added as a dependency of *foo.o*.

*Mkdeps* scans each file given in the command line and computes all direct and 
transitive dependencies that could be found. For the case above the corresponding rule 
in the makefile would be

  foo.o: foo.cpp foo.hpp bar.hpp foobar.hpp

*Mkdeps* understands the preprocessor statements `#define`, `#ifdef`, `#ifndef`,
`defined()` and the corresponding `#if`, `#elif` and `#endif`. It will keep track of
what is defined and it will skip sections of code that are skipped by the preprocessor.

In addition to computing the compilation dependencies of each implementation file,
*mkdeps* also computes the dependencies of executable programs and libraries. If a
file contains a `main()` function then it is considered to be the main implementation
file of an executable program. Assume in the case above that *foo.cpp* declares a
`main()` function. Also assume that we provided `bar.cpp` as argument. We have seen
that *foo.cpp* includes *bar.hpp*. *Mkdeps* assumes that the final program *foo* will
need not only the object file *foo.o* but also *bar.o* because *bar.o* implements the
facilities declared by *bar.hpp*. If *bar.cpp* then includes *foobar.hpp* this implies
that if `foobar.cpp` exists *foobar.o* will also be needed to satisfy the references
to its functions from *bar.o*. The *mkdeps* program will continue this process until
all required components have been added to the executable dependencies:

   foo: foo.o bar.o foobar.o

In this way you do not need to explicitely list the object files that make up each
program. As long as you keep to the rule that header and implementation files come in
pairs then the program will be able to find the required object files. The same logic
applies to libraries. *Mkdeps* scans the source files and if it finds a line with the
string `/* LIBRARY */` then it assumes that this file lists the interfaces provided by
the library. For instance assume that *foo.cpp* contains:

   /* LIBRARY */
   #include "bar1.hpp"
   #include "bar2.hpp"
   #include "bar3.hpp"

Further assume that *bar3.cpp* uses the facilities provided by *bar4.cpp". Then
*mkdeps* will generate a make rule like this:

	libfoo.so: bar1.o bar2.o bar3.o bar4.o
	DEPS_libfoo = bar1.hpp bar2.hpp bar3.hpp

You can use DEPS_libfoo to copy only the public header files and they required
includes when installing the library.



USAGE
=====

The normal use of mkdeps is

	mkdeps [options] implementation_files

where *implementation_files* are the *.c or *.cpp files, not the header files. You can
just pass all the C or C++ files present in the directory. For example `mkdeps *.cpp`
would consider all C++ implementation files.

The following options are available.

	-h
	--help

This will show a help message.

	-v

Verbose output. Good for figuring out what the program is doing.

	--trace

Show the name of each file as it is scanned.

	-d

Show the preprocessor's defined symbols.

	-f <makefile>

Use <makefile> as the name of the makefile to be modified. By default it is `makefile`.

	--append

Append the generated dependencies to the makefile instead of modifying the existing
dependencies.

	-I <dir>

<dir> will be added to the list of directories to be searched for included files.

	--potdeps

Generate dependencies for Gettext's .pot files.

	-o <ext>

Set the extension of the object files to <ext>. The default value is `.o`.

	-e <ext>

Set the extension of the executable files to <ext>. The default value is the empty string.

	-a <ext>

Set the extension of the static library files to <ext>. The default value is `.a`.

	--libpfx <pre>

Set the prefix for dynamic libraries to <pre>. The default value is `lib`.

	--libsfx <sfx>

Set the suffic for dynamic libraries to <sfx>. The default value is '.so'.

	--odir <dir>

Add an additional output directory. If a given file <fname> would have been generated
as target, then <dir>/<fname> will also be generated. For instance if both `--odir
optimized` and `--odir debug` are given then targets will be generated in the
`optimized` and `debug` directories.

	--abi <abiname>

Add an additional kind of binary. For instance adding `--abi pic` will generate
targets with the normal name and also targets with `-pic` added. For instance you may
have a normal rule `hello.o: hello.c` and also a rule `hello-pic.o: hello.c`. The
second rule would create position independent code.

	--prefix <pfx>

Prepend <pfx> to the name of each dependency file.

	--pch

Use precompiled headers for each file.

	--tch

Use precompiled headers for each target.

	--hpfx <pfx>

Prepend pfx to the name of each header file in the generated makefile.



Examples
--------

	mkdeps --odir bin --abi pic -I src --potdeps src/*.cpp test/*.cpp

This will update the dependencies in the current makefile by creating targets in the
bin directory. Normal object and executables will be generated and also object  and
executables which have "-pic" appended to the name. Included files will be searched
in the current directory and in the src directory. Also dependencies for the  creation 
of pot files from source files (for gettext) will be generated. The  resulting 
makefile may contain this:

	...
	bin/poly1305.o bin/poly1305-pic.o : src/poly1305.cpp src/poly1305.hpp  \
		src/soname.hpp  src/misc.hpp

	bin/protobuf.o bin/protobuf-pic.o : src/protobuf.cpp src/protobuf.hpp  \
		src/soname.hpp  src/hasopt.hpp
	...
	bin/genpass: \
		bin/poly1305.o bin/genpass.o bin/symmetric.o bin/misc.o bin/hasopt.o  \
		bin/blake2.o
	...
	bin/libamber$(SOV): \
		bin/sha2-pic.o bin/blockbuf-pic.o bin/symmetric-pic.o bin/inplace-pic.o  \
		bin/noise-pic.o bin/zwrap-pic.o bin/hkdf-pic.o bin/blake2-pic.o  \
		bin/combined-pic.o bin/hasopt-pic.o bin/misc-pic.o bin/keys-pic.o  \
		bin/pack-pic.o bin/field25519-pic.o bin/group25519-pic.o  \
		bin/poly1305-pic.o bin/protobuf-pic.o bin/siphash24-pic.o
	...

You can see that dependencies are being created for normal objects and position
independent objects. Normal programs will use the normal object files, like
poly1305.o, whereas libraries use the poly1305-pic.o variant (with position
independent code). mkdeps only creates the dependencies. It is up to you to define the
pattern rules for each compilation type:

	VPATH=src:test
	$(BIN)/%.o: %.cpp
		g++ $(CCFLAGS) -c -I$(INCLUDE) -o$@ $<

	$(BIN)/%-pic.o: %.cpp
		g++ $(CCFLAGS) -c -I$(INCLUDE) -fpic -o$@ $<



