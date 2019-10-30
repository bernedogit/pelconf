PELCONF - AUTOCONF IN C
=======================

*Pelconf* is a simple tool that is similar to GNU's autoconf. It is used to
test the compilation system and figure out what is available and adapt the
software accordingly. The difference with autoconf is that it does not
depend on the POSIX shell and tools. Instead it uses the C language to
define the tests. Unlike other autoconf replacements like cmake, pelconf
does not need any previous installation: your C compiler is enough. You
don't have to learn a new configuration language or syntax. Everything is
done in C. The tests to be run are written in C.


1 Porting software
------------------

There are many ways of porting software and plenty of books have been
written about the subject. This section describes only enough background to
understand what pelconf does.

A typical way of adapting software to work on multiple platforms is to use
conditional compilation. The preprocessor is used to check for a condition
and then according to the test result, different sections of code are
selected for compilation. For instance:

	#ifdef __GNUC__
		... use GNU features
	#endif

This fragment relies on the GCC compiler's predefined macro `__GNUC__`. If
we are using GCC it is expected that `__GNUC__` will be defined.

There are several problems inherent to this approach. First, you need to
know which is the symbol that is defined by each of the supported compilers
and environments. If a new symbol or a new version must be added then we must
revisit each test. A more difficult problem is that it is not clear what
should be activated by each define. For instance, in the item above we are
just checking if we are using GCC. However GCC is available on many
platforms and in many versions. Each of them offers features that are
specific to the particular combination of platform and version. Maintaining
such a battery of tests quickly becomes unmanageable. Just consider the
different versions of GCC under Unix and Windows.

A better approach is to test for features, not for systems. This is what
autoconf does. You just check for the presence of a particular feature, and
if it exists use it. This way we do not need to adapt each test to each
particular version of the compiler and system. A single check is valid for
all existing and future systems. For instance if we want to use `opendir()`
we would have something like:

	#ifdef HAVE_OPENDIR
		... use opendir()
	#endif

Of course you will be asking: who defines *HAVE_OPENDIR*? In the GNU
configuration system this task is performed by the configure script. The
configure script is a bash script that runs the compiler with different test
cases and generates configuration files. For the case shown above it would
try to compile something like:

	#include <sys/types.h>
	#include <dirent.h>

	int main()
	{
		DIR * (*f)(const char *) = opendir;
		return f != 0;
	}

If this program compiles and links without errors then it means that the
`opendir()` function is available. The configure script created by autoconf
is very convenient for the user, but to generate the script itself the
programmer must use a combination of Bash, M4 macro language and C. This
feels awkward.

Pelconf removes the need to prepare the script with shell, M4 and C and
instead allows the whole configuration test to be written in C. The user
still runs the configure script as usual: `./configure`


2 What pelconf does not do
--------------------------

The approach described above is what is used by autoconf, pelconf and many
other configuration systems. There are some limitations to the approach. The
configure script can check for the presence of features in the compiler,
library or environment. The program can then adapt (using conditional
compilation) by defining workarounds or limiting its functionality. However
the configure script can't adapt the program if features that were
considered to be always available are missing. For instance a program that
uses `fopen()` will not compile on a system which lacks the `fopen()`
function. If the author of a program did not foresee this possibility (which
is likely only on embedded systems) then there is no way for any
configuration system to solve the problem.

Although all hosted implementations are expected to have `fopen()`, other
functions or features may be assumed to exist, and actually exist on all
systems tested by the author. If the program is later ported to a system
where this assumption fails then no configuration system can solve the
problem. An example is the `opendir()` function. It is not part of the C
standard. It is defined by POSIX and most compilers for Windows also
implement it. Thus it is likely that the programmer will assume that the
`opendir()` function is always available. If the program is later ported to
a compiler that does not have the `opendir()` function the program will not
compile, even with the configuration system.

The previous paragraph makes it clear that when porting a program that uses
autoconf/pelconf to a platform on which it has never been tested, new
portability problems may become apparent. Thus it is better to consider
*pelconf* as a way to adapt a program to many known optional features with a
minimum effort.


3  General structure
--------------------

The *pelconf* system assumes that you have a compiler that supports C90. This
is the case for any current C or C++ implementation. Under *pelconf* you
write a small C program called *pelconf.c*. You also distribute with it the
*pelconflib.c* and *configure* files. The *configure* file attempts to
compile *pelconf.c* and then run the resulting executable file. The
executable file then conducts compilation and linking tests as directed by
the *pelconf.c* file. Here is an example *pelconf.c* file

	0   #include "pelconflib.c"

		int main (int argc, char **argv)
		{
	1       ac_init (".cpp", argc, argv, 1);

	2       ac_has_headers ("unistd.h", NULL);
	3       ac_has_func_lib ("sys/time.h", NULL, "gettimeofday", NULL);

			/* Clock_gettime could be in the standard library or in the extra rt library. */
	4       ac_has_func_lib ("time.h", NULL, "clock_gettime", NULL) ||
	5       ac_has_func_lib ("time.h", NULL, "clock_gettime", "rt");

	6       ac_config_out ("config.h", "PELTK");
	7       ac_edit_makefile ("makefile.in", "makefile");
	8       ac_create_pc_file ("peltk", "General C++ utilies library");
	9       ac_finish ();

			return 0;
		}

Line 0 just includes the *pelconflib.c* file and makes its functions
available for the pelconf.c program. Including it in *pelconf.c* allows us
to simplify the generation of the executable file.

Line 1 tells pelconf that we are going to use a C++ compiler and that we
would like to use the latest C++ standard that is supported by the compiler.

Line 2 checks for the presence of the `<unistd.h>` header.

Line 3 checks for the presence of the `gettimeofday()` function
in the `<sys/time.h>` header and the default system library.

Lines 4 and 5 check for the `clock_gettime()` function in the header
`<time.h>` and first in the default system library and if not found there in
the *rt* library.

Line 6 writes a file called *config.h* which contains the preprocessor macros
that reflect the features that are available.

Line 7 processes the file *makefile.in* and puts the results in *makefile*.
The file *makefile* contains suitable expansions of macros used in the
input *makefile.in*.

Line 8 creates a *peltk.pc* file suitable for package-config.

Line 9 cleans up temporary files.


If we run configure with this *pelconf.c* file a *config.h* file will be
created. On a Mingw system *pelconf* found that `gettimeofday()` was
available and defined the `HAVE_GETTIMEOFDAY` macro in *config.h*. On the
other hand it could not find `clock_gettime()` and did not define any macro.
Thus the program can adapt as follows:

	#include "config.h"

	#ifdef HAVE_GETTIMEOFDAY
		timeval tv;
		gettimeofday(&tv, NULL);
		// use tv.
	#else
		// just use time().
	#endif
		...
		...
	#ifdef HAVE_CLOCK_GETTIME
		timespec ts;
		// use clock_gettime()
	#else
		// just use time()
	#endif

The *config.h* file also contains many other pieces of information concerning
the compiler and the library. For instance it checks if `<stdint.h>` is
available and if it isn't it creates the suitable typedefs.

*Pelconf* also processes the existing *makefile.in* and puts the result in
*makefile*. *makefile* contains the contents of *makefile.in* prepended with
information about the compiler, its options and required libraries. If for
instance the `clock_gettime()` was found to be in the *rt* library the
*makefile* would contain a line like this: `EXTRALIBS=-lrt` (or
`EXTRALIBS=rt.lib` for a DOS/Windows compiler). In some cases you want to
have some particular makefile variables set to your own values. The
configuration program searches the compilation environment for a file called
*pelconf.var*. If this file is found then a directive will be inserted in
the makefile to include it in the file. The *pelconf.var* file usually
contains predefined settings of makefile variables. The include statement is
inserted after all the variable definitions created by the program: this
implies that whatever you write in *pelconf.var* overrides the settings
created by the configuration program. The configuration program will search
for *pelconf.var* in several places:

1. the file name and location may be given in the command line using the
   `--makevars=name` command line option. Using this option you can specify
   the exact path and name of the makefile variables file.

2. the current directory will be searched for the `pelconf.var` file.

3. the program will attempt to use the file *<install-prefix>*`/etc/pelconf.var`,
   where *<install-prefix>* is the installation prefix (by default `/usr/local`
   but can be set using the `--prefix=name` option).

Using option 2 enables having project specific options. Using option 3
enables having machine specific options that apply to all projects.


The pelconf program produces the configuration file, usually named
*config.h*, the makefile and the pkg-config configuration file. The
configuration file, *config.h*, contains all the code (defines, typedefs,
inlines) that the source code will include. The makefile is generated by
prepending to an existing input makefile (usually *makefile.in*) the set of
makefile variables that the configuration program defines. The resulting
makefile can then use these variables. Finally the generated pkg-config file
can be directly stored in the directory containing the pkg-config database.

The different tests that are run by the pelconf program are written to the
*configure.log* file. If a test fails and you want to figure out which is
the problem have a look at *configure.log*.


4 The configure file
--------------------

The *configure* file just attempts to compile the *pelconf.c* file and run
it. If the compilation command is not given explicitely with `--cc=<cmd>`
option then it will try using the make program to compile *pelconf* using
the system's default compiler. If this fails it will try *gcc* or *g++*. If
for whatever reason *configure* fails to compile the *pelconf.c* program you
can try to compile it manually. For instance if your compiler is called `dmc`
use

	./configure --cc=dmc

If this fails then try to compile manually and run the resulting program.

	dmc pelconf.c -o pelconf
	./pelconf --cc=dmc


5 Cross Compilation
-------------------

All tests in pelconflib.c do not require to run the generated code and can be used to
detect properties when using a cross compiler. In this case you need to have another
compiler that generates programs that will run in the compilation environment:

	host-cc -o pelconf pelconf.c
	./pelconf --cc=cross-cc

Here host-cc is the compiler that generates binary files that can be executed in the
computer hosting the compilation environment. cross-cc is the compiler that will be
checked.


6 The *pelconflib.c* file
-------------------------

The *pelconflib.c* file provides the functionality to check the system. It
contains many functions, some of them intended for internal use. Those
functions that are intended for internal use have the `aci_` prefix. The ones
that are part of the public API use the prefix `ac_`. The rest of this
document describes the functions that are provided by *pelconflib.c*.

7 Structure of *pelconf.c*
--------------------------

You write the *pelconf.c* file. Every *pelconf.c* includes *pelconflib.c*
first. The *pelconf.c* file has the `main()` function. `main()` always
starts with the `ac_init()` function. This function performs argument
parsing and runs a series of tests that are always performed. After
`ac_init()` follow the tests that are specific to this particular project:
checking for headers, functions, etc. Then `main()` ends calling some
functions to create the output. `ac_config_out()` writes the configuration*
file which contains the results of the tests. `ac_edit_makefile()` creates
a customized makefile. `ac_create_pc_file()` is optionally called to create
`.pc` file that is ready for pkg-config. The last function called is
`ac_finish()` which performs clean up.

An skeleton file would be:

	ac_init(...);
	...
	project_specific_tests(...);
	...
	ac_config_out(...);
	ac_edit_makefile(...);
	ac_create_pc_file(...);
	ac_finish();

General tests are run by `ac_init()`. Project specific tests are written by
you using the functions that are available in *pelconflib.c*.

8 General tests
---------------

The general tests run by `ac_init()` determine information about the
compiler command line, support for standards. They are listed below.

8.1 Compiler flags
------------------

The configuration program probes the compiler to identify it and assert
which compiler flags are accepted. The compilation command is taken from the
command line or the environment variable CC (or CXX for C++).

Next the program checks for some known predefined macros used by compilers:
`__clang__`, `__GNUC__`, `__BORLANDC__` or `__TINYC__`. If any of these are
found then the compiler will be check to verify that it accepts some common
compiler flags. For each of the found valid flags a corresponding makefile
variable will be set.

For GCC the following options are checked: `-march=native`, `-fpic`, `-fpie`,
`-Wl,--dynamicbase,--nxcompat`, `-fextended-identifiers`,
`-fvisibility=hidden`, `-Wl,--enable-new-dtags`, `-Wl,--rpath='$$ORIGIN'`,
`-Wl,--as-needed`, `-mthreads`, `-O2`, `-fomit-frame-pointer`,
`-ftree-vectorize`, `-ffast-math`, `-g`, `-fstack-protector`,
`-fstack-protector-all`, `-gsplit-dwarf`, `-Wa,--compress-debug-sections`,
`-Wl,--gdb-index`, `-ftrapv`, `-fnon-call-exception`, `-Wabi-tag`, `-shared
-Wl,--soname=foo`, `-shared -Wl,--out-implib=foo.a`, `-static`, `-std`.

If an option is valid then a makefile variable will be defined with the
value being the option found. For instance if the `-fpic` option is accepted
then the variable GCC_FPIC will be set in the makefile to take the value
`-fpic`.

You can set the compilation flags in terms of makefile variables. If a
particular flag is not available then the corresponding makefile variable
will not be defined. For instance you may want to use the following flags in
debug mode if they are available:

	CFLAGS_DBG = $(GCC_G) $(GCC_STACK_PROTECTOR_ALL) \
		$(GCC_COMPRESS_DEBUG_SECTIONS) $(GCC_SPLIT_DWARF) \
		$(GCC_TRAPV) $(GCC_NON_CALL_EXCEPTION)

You can just use this definition of CFLAGS_DBG. If it turns out that this
particular implementation of GCC does not support splitting of debugging
information then GCC_SPLIT_DWARF will be undefined and it will expand to the
empty string. Within the selected set of flags only those which are really
available will be used.

The configuration program also checks if the compiler uses UNIX or DOS
conventions. UNIX uses object files of the form *.o and library files of the
form lib*.a and lib*.so whereas DOS uses *.obj and *.lib.

The configuration program will try to figure out how to select the name
of the output file. It will also attempt to enable all warnings.

If the compiler is GCC then the following flags are checked and their
corresponding makefile variables  are defined if they are allowed:

 - `-march=native`, variable `TARGET_ARCH`

 - `-fpic`, variable `GCC_FPIC`

 - `-fpie`, variable `GCC_FPIE`

 - if compiling for Windows check for `-Wl,--dynamicbase,--nxcompat` and if
   accepted set `GCC_PIE` to these options.

 - if not compiling for Windows check for `-pie`, variable `GCC_PIE`.

 - `-fextended-identifiers`, variable `GCC_EXTIDENT`

 - `-fvisibility=hidden`, variable `GCC_VISHIDDEN`

 - `-Wl,--enable-new-dtags`, variable `GCC_NEWDTAGS`

 - `-Wl,--rpath='$$ORIGIN'`, `GCC_RPATH_LIB`. If this variable is set then
   also set the variables `GCC_RPATH_BIN` to `-Wl,--rpath='$$ORIGIN/../lib'`
   and `GCC_RPATH_PREFIX` to `-Wl,--rpath=$(PREFIX)lib`

 - `-Wl,--as-needed`, variable `GCC_ASNEEDED`

 - `-mthreads`, variable `GCC_MTHREADS`

 - `-O2`, variable `GCC_O2`

 - `-fomit-frame-pointer`, variable `GCC_OMITFRAMEPOINTER`

 - `-ftree-vectorize`, variable `GCC_TREEVECTORIZE`

 - `-ffast-math`, variable `GCC_FASTMATH`

 - `-g`, variable `GCC_G`

 - `-fstack-protector`, variable `GCC_STACK_PROTECTOR`

 - `-fstack-protector-all`, variable `GCC_STACK_PROTECTOR_ALL`

 - if not compiling for Windows check for `-gsplit-dwarf`, variable
   `GCC_SPLIT_DWARF` and `-Wa,--compress-debug-sections`, variable
   `GCC_COMPRESS_DEBUG_SECTIONS`

 - `-Wl,--gdb-index`, variable `GCC_GDB_INDEX`

 - `-ftrapv`, variable `GCC_TRAPV`

 - `-fnon-call-exception`, variable `GCC_NON_CALL_EXCEPTION`

 - `-Wabi-tag`, variable `GCC_WABI_TAG`

 - if SONAME is supported then set `GCC_SONAME` to `-Wl,--soname=$(notdir $@)`

 - if `-out-implib` is supported then set `GCC_OUTIMPLIB` to `-Wl,--out-implib=$@$(A)`

 - if the user requested a modern version of C or C++ then select the highest available
 version and define the variable `GCC_STD` to one of `-std=gnu++17`, `-std=gnu++1z`,
 `-std=gnu++14`, `-std=gnu++1y`, `-std=gnu++11`, `-std=gnu++0x` for C++ or `-std=gnu11`,
 `-std=gnu99` for C.

 - `-static`, variable `GCC_STATIC`




8.2 Parameters
--------------

The program will check the support for the following features:

- Support for *<stdint.h>*. If it is present it will be included by
  *config.h*. If it is not present then the macros and typedefs specified by
  *<stdint.h>* will be created by the configuration program.


- Whether we are compiling for a little or big endian computer. It will
  define the macros WORDS_BIGENDIAN or WORDS_LITTLEENDIAN as appropriate.

- Support for the `__attribute__((x))` syntax. The `GCC_ATTRIBUTE` macro
  will be defined if supported.

- Support for some kind of thread local storage. It checks for either
  `thread_local`, `__thread` or `__declspec(thread)` storage classes. If
  `thread_local` is not supported but the other options are available then
  the `thread_local` will be defined as a macro with the correct value. The
  macro `HAVE_THREAD_LOCAL_STORAGE_CLASS` will be defined if thread local
  storage is supported.

- Support for alignment attributes. The macro ALIGN(X) will be defined.

- Support for `bool` in C mode.

- Support for the `restrict` keyword or its variant `__restrict`. It will
  define `restrict` if required.

- Ensure that `va_copy()` is available, either through *<stdarg.h>* or by
  defining it in *config.h*.

- Support for variadic macros, defining `HAVE_VARIADIC_MACROS`.

- Support for the flexible array member syntax. If available it will define
  the macro `FLEXIBLE_ARRAY_MEMBER` to empty. If not available it will be
  defined to 1. With this you can declare an structure like this:

	struct foo {
		int a, b;
		char c[FLEXIBLE_ARRAY_MEMBER];
	};

  If the flexible array member is supported then the last field will be
  declared as `char c[];`. If it is not supported it will be declared as
  `char c[1];`.

- Define the `PRIiMAX` family of macros if *<inttypes.h>* does not support
  them.

- Define the macro `HAVE_C99_MIXED_VAR_DECLS` if we can mix declarations and
  code in C mode.

- Ensure that `ssize_t` is available by typedefing it if required.

- Ensure that `char32_t` is available by typedefing it if required.

- Check if `__builtin_add_overflow()` is available and define
  `HAVE_GCC_OVERFLOW` if available.

- Check how to export symbols from libraries and define the `EXPORTFN` macro.

- Check if the attributes `deprecated`, `warn_unused_result`, `unused` are
  supported.

- Check the GCC format attribute family.

- Ensure that the `__func__` syntax is valid.

- Ensure that the `inline` keyword is valid (even in C90 mode).

- Check if inline assembly is available.

- Check for a suitable definition for _ReturnAddress().

- Check if the `__COUNTER__` macro is supported.

- Define the makefile variables COPY, COPYREC, REMOVE, LN, LN_S, ME,
  INSTALL, INSTALL711, INSTALL_DATA, INSTALL_DIR. COPY will act like `cp`.
  COPYREC will act like `cp -r`. REMOVE will act like `rm`. LN will act like
  `ln` if hard links are supported, otherwise it will act like `cp`. LN_S
  will act like `ln -s` if symbolic links are supported, otherwise it will
  act like `cp`. ME is the prefix that is required to run a command from the
  current directory: empty for DOS/Windows and `./` for UNIX. The INSTALL*
  variables will be defined to `install *` if supported or otherwise as
  invocations of COPY.

If the compilation mode is C++ then the following checks for bugs are also
performed:

- Check if SFINAE works (some old compilers used to be buggy)

- Check if overloading templates with const or volatile is buggy.

- Ensure that numeric limits has been specialized for int64_t/uint64_t.

- Support for intmax_t as template parameter.

If the compilation mode is C++ then the following checks for features are
also performed.

- Support for inline namespaces or the strong attribute.

- Support for decltype, auto, constexpr, override and final.

- Support for explicit extern instantiation of templates.

- Support for rvalue references.

- Support for variadic templates.

- Support for the headers type_traits, chrono, tuple, system_error, ratio,
  atomic, thread.

- Support for the abi_tag attribute.



9 Checking the compilation environment
--------------------------------------

Note that all the tests shown below are performed by running the compiler. No test
requires that the compiled program is run. The compiler could be a cross compiler and
our system may be unable to run the compiled programs. All the information is gathered
from the compilation environment, not from the runtime environment. This applies even
to such things like determining the size of a given type or determining the
endianness. This is done without running the compiled program.



9.1 Checking if header files are available
------------------------------------------

In some cases we need to know whether a header file is present. Note that
the presence of a header file is a necessary condition but it may not be
enough to ensure that the program will compile and link properly. You check
for the presence of a set of headers as a single unit by using the function
`ac_has_headers()` or `ac_has_headers_tag()`. For instance

	ac_has_headers_tag ("sys/types.h, sys/stat.h", NULL, "STAT_HEADER");

will check that after including first *<sys/types.h>* and then *<sys/stat.h>*
we can compile an empty program. If both headers exist the HAVE_STAT_HEADER
macro will be defined. Note that according to the POSIX specification we
must include *<sys/types.h>* before including *<sys/stat.h>*.

The `ac_check_each_header_sequence()` will run each individual header through
ac_has_headers(). This is different from above. Consider the call

	ac_check_each_header_sequence ("sys/types.h, sys/stat.h", NULL);

This will first check if we can include *<sys/types.h>*. If the file exists
it will define HAVE_SYS_TYPES_H. Then it will check if we can include
*<sys/stat.h>*. If *<sys/stat.h>* needs that the definitions of
*<sys/types.h>* are available this attempt will fail because we didn't
include *<sys/types.h>* before including *<sys/stat.h>*.



9.2 Checking if function declarations are available
---------------------------------------------------

Some times we need to check not only that a header file is available, but
also that it declares a given function prototype. This may be enough to
compile a source file that uses the declaration, but it may need further
flags to specify the libraries containing the function. We check if a
function declaration is available with the `ac_has_proto_tag()`,
`ac_has_proto()` and `ac_has_signature()` functions. The
`ac_has_proto()` and `ac_has_proto_tag()` functions check if a function
has been declared with the requested name. For instance:

	ac_has_proto ("process.h", "-mthreads", "_beginthread");

will check if the `_beginthread()` function has been declared after including
*<process.h>* and compiled with the flag -mthreads. If it has been declared
it will define the macro `HAVE__BEGINTHREAD`

Note that the `ac_has_proto*()` functions do not care about how the
function has been declared, only that it has been declared. This may not be
enough if we want to check for different possible declarations of the
function. One example is the strerror_r function. It is defined by both
POSIX and GNU in incompatible ways. If we use the proto functions above we
may discover that strerror_r has been declared, but we do not know if it has
been declared in the POSIX or GNU flavor. The `ac_has_signature()` can
distinguish both:

	ac_has_signature ("string.h", NULL, "strerror_r",
	                    "int (*f)(int,char*,size_t)", "POSIX_STRERROR_R");

	ac_has_signature ("string.h", NULL, "strerror_r",
	                    "char* (*f)(int,char*,size_t)", "GNU_STRERROR_R");

Here we define `HAVE_POSIX_STRERROR_R` if *strerror_r* has been declared as
`int strerror_r(int,char*,size_t)` and we define `HAVE_GNU_STRERROR_R` if
*strerror_r* has been declared as `char* strerror_r(int,char*,size_t)`. Note
that the syntax of the signature is a function pointer with the requested
signature.

Note that this check works only in C++ mode. C allows casting between
incompatible types of function pointers. Even in C11 this is allowed, with
only the call through an incompatible pointer type being undefined behaviour.



9.3 Checking for type declarations
----------------------------------

Often we need to verify that a type is available after including some
headers. This is done with the `ac_has_type()` and `ac_has_type_tag()`
functions. For instance:

	ac_has_type ("sys/time.h", NULL, "struct timeval");

This will check if the struct timeval is available after including
*<sys/time.h>*. If it is available `HAVE_STRUCT_TIMEVAL` will be defined.



9.4 Checking for members of structures
--------------------------------------

In some cases it is not enough to know that a type or struct is declared. We
may need to check if it contains a given member. For instance, the `struct
lconv` contains the field `int_p_cs_precedes` in C99 but not in C90. We can
check for this field with

	ac_has_member_tag ("locale.h", NULL, "struct lconv","int_p_cs_precedes",
	                   "C99_LCONV");

If the field is present `HAVE_C99_LCONV` will be defined.


9.5 Checking the size of a type
-------------------------------

In some cases we need to know the size of a given type. We could use
sizeof() at run time but in some cases we need to know the size at compile
time. `ac_get_sizeof()` can figure out the size of a type without running
any program. Thus it is usable even when cross-compiling. The following can
be used to figure out the size of a `wchar_t`:

	size_t n = ac_get_sizeof ("stddef.h", NULL, "wchar_t");

The test above will return the `sizeof(wchar_t)` and will define the macro
`SIZEOF_WCHAR_T` to the size of `wchar_t`.


9.6 Checking for defined macros
-------------------------------

Any program may check for the presence of macros by using #ifdef. The test
`ac_has_define()` allows the configuration program to verify if a given
macro is defined. It may then modify the makefile accordingly or run
additional tests.


9.7 Checking for preprocessor expressions
-----------------------------------------

We may need in some cases to check if a given expression is valid in the
preprocessor. Use the `ac_valid_cpp_expression()` function for this purpose.



9.8 General checks for compilation of code
------------------------------------------

The tests above can be seen as specialized versions of a general check to
verify that some source code can compile. We can check if an arbitrary
source code compiles or fails to compile with the `ac_does_compile()` and
`ac_does_compile_fail()` functions. For instance, we can quickly check if a
given set of flags is defined by using the following check:

	ac_does_compile ("has fcntl.h flags",
	              "#include <fcntl.h>\n"
				  "int x = O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_RDONLY | O_WRONLY;\n",
				  NULL, "FCNTL_FLAGS");

This will check if the above statement can be compiled. This will succeed if
after including *<fcntl.h>* the `O_CREAT`, `O_EXCL`, `O_TRUNC`, `O_APPEND`,
`O_RDONLY` and `O_WRONLY` symbols are available. If they are the macro
HAVE_FCNTL_FLAGS will be defined.

In some cases we are checking for bugs in the compilation environment. In
this case we use `ac_does_compile_fail()`. If the given source code fails to
compile it will define the macro. This may be used to work around bugs.





10 Checking the linking environment
----------------------------------

As stated above it is not enough to verify that symbols are declared after
including header files and specifying the correct compiler flags. In some
cases we need to provide additional flags to the linker for it to locate and
use the required libraries. The functions in this section check that both
the compilation and linking steps succeed.


10.1 Checking if a function is available in a library
----------------------------------------------------

We need to check that functions are not only declared but that they are also
present in the libraries. The functions `ac_has_func_lib()` and
`ac_has_func_lib_tag()` perform the compilation and linking steps to verify
that a function can be used. For instance the function `clock_gettime()` is
usually declared in *<time.h>* but it may be available in the default library
or we may need to link it explicitely with the *rt* library. The following
tests check that the `clock_gettime` is declared in *<time.h>* and then
first if it is available in the default library and if it is not available
in the default library it checks if it is available in the *rt* library.

	ac_has_func_lib ("time.h", NULL, "clock_gettime", NULL) ||
	ac_has_func_lib ("time.h", NULL, "clock_gettime", "rt")

If it turns out that the *rt* library is required, the name of the required
library will be added to the makefile variable EXTRALIBS.



10.2 General testing of the linking environment
----------------------------------------------

Similar to `ac_does_compile()` you can check if a given source code compiles
and links using some given flags. For an example of the need for this
function consider GCC's atomic builtin functions. GCC supports the function
`__sync_fetch_and_add()` as an intrinsic function. However if the
architecture does not support the atomic fetch-and-add GCC will generate a
library call.

	ac_does_compile_and_link ("has __sync_fetch_and_add builtin",
	        "int main()  { int x = 5;  return  __sync_fetch_and_add(&x, 2); }\n",
	        "", NULL, "SYNC_FETCH_AND_ADD");

If we compile this with *gcc -march=i386* the program will not link because
`__sync_fetch_and_add` is missing from the library. If we compile it with
*gcc -march=i686* the program will compile.




11 Miscelaneous tests
--------------------

`ac_has_file()` can be used to verify if a given file is present in the
compilation environment. Note that the file may not be present in the
execution environent.



12 Reference
------------


### ac_add_code

	void ac_add_code (const char *src_code, int unique);

Adds the given src_code verbatim to the configuration file. If unique is set
you may call this function several times with the same src_code, but it
will be output only once. A typical use is to perform a check and then
output code if the feature is not available. The code would then emulate
the feature.


### ac_add_flag

	void ac_add_flag (const char *name, const char *comment, int passed)

The public interface to add a preprocessor define to the configuration file 
`config.h`. Name is the name of the flag. The comment will be displayed
before the flag. The flag will be #defined if passed is true.


### ac_add_option_info

	void ac_add_option_info (const char *opt, const char *desc)

Add the description of one command line option. This information will be
shown to the user when the `ac_show_help()` function is called or when the
user gives the "--help" option. Call this function for each of the options
that the configuration program processes. `opt` is the command line option
that is being described and desc is the text explaining it.


### ac_add_var_append

	void ac_add_var_append (const char *name, const char *value);

Append text to a variable in the makefile. Appends *value to the current
value of the makefile variable *name. If this function is called several
times, the last value appended will be found at the end of *name's value.


### ac_add_var_prepend

	void ac_add_var_prepend (const char *name, const char *value);

Prepend text to a variable in the makefile. Prepends *value to the current
value of the makefile variable *name. If this function is called several
times, the last value prepended will be found at the beginning.


### ac_check_each_func

	void ac_check_each_func (const char *funcs, const char *cflags)

Check for the presence of each function in *funcs*. Use it after
`ac_check_each_header_sequence()`.


### ac_check_each_header_sequence

	void ac_check_each_header_sequence (const char *includes,
	                                    const char *cflags);

This will check for each header file in *includes*, behaving as if
`ac_has_headers()` was called with each of the header files individually.


### ac_check_same_cxx_types

	void ac_check_same_cxx_types (const char *includes, const char *cflags,
	                              const char *t1, const char *t2, const char *tag)

Include the files listed in *includes* and compile with the compilation
flags *cflags*. Are the typedefed types *t1* and *t2* the same types in C++?
If yes then define *tag* in the configuration file.


### ac_config_out

	void ac_config_out (const char *config_name, const char *feature_pfx)

Write out the configuration file to *config_name*. Prefix the configuration
macros with *feature_pfx*.


### ac_create_pc_file

	void ac_create_pc_file (const char *libname, const char *desc)

Create a .pc file for pkg-config. *libname* is the name of the library. You
provide the one line description of the library in *desc*.



### ac_does_compile

	int ac_does_compile (const char *comment, const char *src,
	                     const char *cflags, const char *tag);

Check if we can compile the source code in *src*, using the compilation flags
in *cflags*. The comment *comment* will be put in the configuration file
before the definition of the tag *tag*. If the code compiles it returns true
and defines the tag in the configuration file. If the code does not compile
the definition is left as a comment and the function returns zero.


### ac_does_compile_and_link

	int ac_does_compile_and_link (const char *comment, const char *src,
	              const char *flags, const char *libs, const char *tag);

Check if we can compile and link the source code in *src*, using the
compilation flags in *cflags* and linking with the libraries in *libs*. The
comment *comment* will be put in the configuration file before the definition
of the tag *tag*. If the compilationg and linking is successful it will
return non zero. If something fails zero will be returned.



### ac_does_compile_and_link_fail

	int ac_does_compile_and_link_fail (const char *comment, const char *src,
	              const char *flags, const char *libs, const char *tag);

Perform the same check as `ac_does_compile_and_link()` but set the flags and
return value only if the compilation fails.


### ac_does_compile_fail

	int ac_does_compile_fail (const char *comment, const char *src,
	                           const char *cflags, const char *tag);

Similar to `ac_does_compile()`, but define the tag only if the compilation
fails. It returns zero if the source code compiles and non zero if the
compilation fails.


### ac_edit_makefile

	void ac_edit_makefile (const char *make_in, const char *make_out)

Write the completed makefile reading it from *make_in* and putting it in
*make_out*.

### ac_finish

	void ac_finish (void);

This is the last function that pelconf.c should call. It performs the
required clean up tasks.



### ac_get_sizeof

	int ac_get_sizeof (const char *includes, const char *cflags,
	                     const char *tname);

Detects the size of a type without actually running the compiler program
(may be useful for cross compilers). It will include the files listed in
*includes* and it will use the compilation flags *cflags*. A macro of the
form SIZEOF_##*tname* will be defined in the configuration file. The
function  returns -1 on failure or the `sizeof(tname)` on success.


### ac_has_compiler_flag

	int ac_has_compiler_flag (const char *flag, const char *makevar)

See if the compiler supports the compilation flag *flag*. If it does set the
makefile variable *makevar* to *flag*. It returns true if the flag is
accepted by the compiler.


### ac_has_define

	int ac_has_define (const char *includes, const char *cflags,
	                     const char *defname);

Return true if *defname* has been defined as a macro after including the
files listed in *includes* and compiling with the compilation flags *cflags*.


### ac_has_feature

	int ac_has_feature (const char *name, char *dest, size_t dest_size)

Check if the option *name* was given in the command line and put its value
in  *dest*.


### ac_has_file

	int ac_has_file (const char *name);

Check if the file *name* is available in the **compilation** environment.


### ac_has_func_attribute

	int ac_has_func_attribute (const char *attribute, const char *external,
	                             int usedef, int literal);

Check if the attribute *attribute* is a valid attribute for functions. If
usedef is not zero a default definition will be added to the function. This
is sometimes required when an attribute is allowed only with function
definitions but not with function declarations. If literal is zero then
check if the attribute is available in the form __attribute__ before
checking the name of the attribute without underscores. If defines a macro
with the name *external* to the correct syntax for the attribute. It returns
non zero if the attribute is available.



### ac_has_func_lib

	int ac_has_func_lib (const char *includes, const char *cflags,
	                       const char *func, const char *libs, int verbatim);

Same as `ac_has_func_lib_tag()` but the tag name is deduced from the name of
the function. For instance we may want to check how to compile under Win32 
when using threads:

	ac_has_func_lib ("process.h", NULL, "_beginthread", NULL) ||
	ac_has_func_lib ("process.h", "-tWM", "_beginthread", NULL) ||
	ac_has_func_lib ("process.h", "-mthreads", "_beginthread", NULL);

This will define `HAVE__BEGINTHREAD` in the configuration file if one of
the above tests compiles and links. In addition `EXTRA_CFLAGS` will get the
compiler option that is required for multithreaded compilation under
windows.


### ac_has_func_lib_cxx

	int ac_has_func_lib_cxx (const char *includes, const char *cflags,
	                           const char *func, const char *libs)

Same as `ac_has_func_lib_cxx_tag()` but the tag name is deduced from the name
of the function.


### ac_has_func_lib_tag

	int ac_has_func_lib_tag (const char *includes, const char *cflags,
	                           const char *func, const char *libs,
	                           int verbatim, const char *tag);

Checks if the function *func* is declared in the headers listed in *includes*
and defined in the libraries listed in *libs* when compiled with the compiler
options *cflags*. If the function is defined then it adds HAVE_<*tag*> to the
configuration and returns non-zero.

If verbatim is true the *libs* contents is passed to the command line as is.
Otherwise the *libs* contents are processed to add the required prefixes and
suffixes as required to convert `foo bar` into `-lfoo -lbar` or `foo.lib
bar.lib` as needed.

If the function is found the contents of cflags will be added to the
variable `EXTRA_CFLAGS` in the makefile and the additional libraries are
added to the `EXTRALIBS` variable in the makefile. For instance:

	ac_has_func_lib_tag ("pthread.h", NULL, "clock_gettime", "pthread",
	                       "CLOCK_GETTIME_IN_PTHREAD");

If found, this will define `HAVE_CLOCK_GETTIME_IN_PTHREAD` in the
configuration file and it will add `-lpthread` to the `EXTRALIBS` variable
in the makefile.


### ac_has_func_lib_tag_cxx

	int ac_has_func_lib_tag_cxx (const char *includes, const char *cflags,
	                    const char *func, const char *libs, const char *tag);

Similar to `ac_has_func_lib_tag()`. The difference is that this function
assumes that the compiler is a C++ compiler and that the included files must
be wrapped with `extern "C" {}` within **our** program.


### ac_has_func_pkg_config

	int ac_has_func_pkg_config (const char *includes, const char *cflags,
	                const char *func, const char *package)

Same as `ac_has_func_pkg_config_tag()` but the tag is deduced from func.


### ac_has_func_pkg_config_tag

	int ac_has_func_pkg_config_tag (const char *includes,
	          const char *cflags, const char *func, const char *package,
	          const char *tag)

Check for a function. If package config is available its information will
be used to deduce the required flags. Otherwise check if the function is in
the library *package*. It returns true if the function was found.


### ac_has_headers

	int ac_has_headers (const char *includes, const char *cflags);

Same as `ac_has_headers_tag()`, but the tag name is deduced from the name
of the headers.


### ac_has_headers_tag

	int ac_has_headers_tag (const char *includes, const char *cflags,
	                        const char *tag);

Checks for the presence of the headers listed in includes, when compiled
with the compilation flags given in *cflags*. If all the header files could
be included then it will define the preprocessor macro HAVE_<*tag*> in the
configuration file. You may write several headers in *includes* by
separating them with commas. For instance you may want to check if the
headers *<sys/types.h>* and *<sys/stat.h>* are available:

	ac_has_headers_tag ("sys/types.h, sys/stat.h", "", "SYS_TYPES_STAT");

This will define `HAVE_SYS_TYPES_STAT` in the configuration file if the
headers are available.

The function returns nonzero if the headers are available. If the headers
are available and *cflags* is not NULL or empty it will add its contents to
the `EXTRA_CFLAGS` makefile variable.


### ac_has_member

	int ac_has_member (const char *includes, const char *cflags,
	        const char *sname, const char *fname);

Same as `ac_has_member_tag()` but the tag is deduced from the name of the
field.


### ac_has_member_lib

	int ac_has_member_lib (const char *includes, const char *cflags,
	                       const char *func, const char *libs,
	                       int verbatim)

Same as `ac_has_member_lig_tag()` but the tag name is deduced from the name
of the function. For instance:

	ac_has_member_lib ("peltk/formats/exif.hpp", NULL,
	        "peltk::formats::Exif_reader::get_metering", "peltk-formats");

This checks if the member function
`peltk::formats::Exif_reader::get_metering()` is declared after including
*<peltk/formats/exif.hpp>* and it can be found by the linker after linking
with the library *peltk-formats*. If the `get_metering()` function is found
then it `#defines HAVE_PELTK__FORMATS__EXIF_READER__GET_METERING`.


### ac_has_member_lib_tag

	int ac_has_member_lib_tag (const char *includes, const char *cflags,
	                           const char *func, const char *libs,
	                           int verbatim, const char *tag)

This function checks the the member function specified in func is present
after including the headers listed in *includes*, compiling with *cflags*
and linking with *libs*. It assumes that the compiler is a C++ compiler.


### ac_has_member_pkg_config

	int ac_has_member_pkg_config (const char *includes, const char *cflags,
	                                const char *func, const char *package)

Same as `ac_has_member_pkg_config_tag()` but the tag is deduced from func.



### ac_has_member_pkg_config_tag

	int ac_has_member_pkg_config_tag (const char *includes, const char *cflags,
	                                    const char *func, const char *package,
	                                    const char *tag)

Similar to `ac_has_member_lib_tag()` but it first uses pkg-config if
available and if this fails it uses `ac_has_member_lib_tag()`.

Pkg-config is needed on ELF systems when linking with static libraries. For
instance library A needs library B. If the library A is a static library it
will contain no indication that libB is needed and we must supply it.

Pkg-config is needed on Woe for both static and dynamic linking. For static
linking the same reasons as with ELF apply. For dynamic linking there is a
problem with PE linking: it expects that all its dependencies are
explicitely listed in the command line. For instance if we use *libA.dll* and
the headers of *libA.dll* have inline functions that refer to functions
*libB.dll* our object code will contain calls to functions in *libB.dll*.
When linking with ELF all functions are looked up in all needed shared
objects. The *libA.so* shared object will introduce *libB.so* into the list
of needed shared objects. When the linker looks for the functions required
for our program it will also look into *libB.so* and will find them there.
ELF was designed to provide this flexibility. However PE is very primitive
and we must supply all libraries in the command line, even those which are
required only due to inline references and are not part of the API. This is
why Woe needs pkg-config for all libraries.


### ac_has_member_tag

	int ac_has_member_tag (const char *includes, const char *cflags,
	        const char *sname, const char *fname, const char *tag);

After adding the *includes* files anc compiling with *cflags* check if the
structure *sname* contains a member with the name *fname*. If it does the
define *tag* and return 1.


### ac_has_pkg_config

	int ac_has_pkg_config (void)

See if pkg-config is installed and working.


### ac_has_proto

	int ac_has_proto (const char *includes, const char *cflags,
	                    const char *func);

Same as calling `ac_has_proto_tag()` but with the tag deduced from the
function name. For instance:

	ac_has_proto ("stdio.h", "-ansi", "fopen);

will define `HAVE_FOPEN` if fopen is defined when including *stdio.h* and
compiling with the compiler option -ansi.


### ac_has_proto_tag

	int ac_has_proto_tag (const char *includes, const char *cflags,
	                        const char *func, const char *tag);

Checks if the prototype for the function func is defined in the headers
listed in includes when compiled with the compilation flags cflags. If it is
available it will add HAVE_<*tag*> to the configuration file and return a
non-zero value. If the prototype is found and *cflags* is neither NULL nor
empty it will add its contents to the `EXTRA_CFLAGS` variable in the
makefile.

For instance:

	ac_has_proto_tag ("stdio.h", "-ansi", "fopen",
	                    "FOPEN_IN_STDIO_WITH_ANSI");

This will check if fopen() is defined in stdio.h when compiled with the
-ansi flag. If it is it will define `HAVE_FOPEN_IN_STDIO_WITH_ANSI`.


### ac_has_signature

	int ac_has_signature (const char *includes, const char *cflags,
	                        const char *func, const char *signature,
	                        const char *tag);

Checks if the header files listed in includes, when compiled with cflags
define the function func with the signature given in signature. If it does
it adds HAVE_<*tag*> to the configuration file and returns non-zero. If  the
signature is found and *cflags* is neither NULL nor empty it will add  its
contents to the EXTRA_CFLAGS variable in the makefile. For instance:

	ac_has_signature ("string.h", NULL, "strerror_r", "int (*f)(int,char*,size_t)",
	        "POSIX_STRERROR_R");

	ac_has_signature ("string.h", NULL, "strerror_r", "char* (*f)(int,char*,size_t)",
	        "GNU_STRERROR_R");

Both GNU and POSIX define strerror_r but differ in the signature. The  above
checks will detect if we have the POSIX or GNU version. The  signature must
be written as a declaration of a function pointer with the  desired
signature.

It returns non zero if the signature is present, zero if it is missing.


### ac_has_type

	int ac_has_type (const char *includes, const char *cflags,
	                   const char *tname);

Same as `ac_has_type_tag()` but the tag is deduced from the name of the type.


### ac_has_type_tag

	int ac_has_type_tag (const char *includes, const char *cflags,
	                       const char *tname, const char *tag);

Check if *tname* is available as the name of a type after including the
files listed in *includes* and compiling with the compilation flags
*cflags*. If *tname* is available then define tag in the configuration file
and return 1.


### ac_has_var_attribute

	int ac_has_var_attribute (const char *attribute, const char *external);

Check if the attribute *attribute* is a valid attribute for variables. The
macro *external* will be defined to the syntax required for the attribute.


### ac_has_woe32

	int ac_has_woe32 (void)

Are we compiling for Woe?


### ac_init

	void ac_init (const char *extension, int argc, char **argv,
	              int latest_c_version);

This is the first function that *pelconf.c* should call. Extension is the
extension used for the file that will contain the source code to be tested.
Use ".c" for C files. Any other extensions will be treated as C++. The
recomended extension for C++ files which is understood by most compilers is
".cpp". argc and argv are just the arguments passed by the user. Set
latest_c_version if you want to check for features of the latest C or C++
standards. If latest_c_version is 0 it will not check for the latest
standards. On GCC if *latest_c_version* is true then it will also select the
appropriate flag like *-std=gnu++14*



### ac_libobj

	void ac_libobj (const char *func_name)

Add *func_name.o* to the list of object files that must be compiled. This
list is available as the value of the makefile variable `LIBOBJS`. It will
usually be the list of files which provide replacements for missing
functions.


### ac_msg_error

	int ac_msg_error (const char *msg);

It will output an error message and stop the configuration. It always returns
zero. It is intended to be used as the last term in a sequence of tests:
`ac_check...() || ac_check...() || ac_msg_erro("Give up");`


### ac_pkg_config_flags

	int ac_pkg_config_flags (const char *s, char *buf, size_t n, pkgconf_flags what)

Get the flags as given by pkg-config. *what* can be `pkgconf_cflags` or
`pkgconf_libs`. Store the flags in the buffer *buf* of size *n*.


### ac_replace_funcs

	void ac_replace_funcs (const char *includes, const char *cflags, const char *funcs)

After including the files listed in *includes* and compiling with the
compilation flags *cflags* check for the presence of each of the functions
listed in *funcs*. If the function is not available the add the object file
*function_name.o* to the makefile variable *LIBOBJS*.


### ac_set_var

	void ac_set_var (const char *name, const char *value);

Sets the makefile variable whose name is *name to the value stored in
*value.


### ac_show_help

	void ac_show_help (void)

Show the available options for the configuration program.


### ac_use_macro_prefix

	void ac_use_macro_prefix (const char *pfx)

Prepend the prefix *pfx* to the macros created by the program.


### ac_valid_cpp_expression

	int ac_valid_cpp_expression (const char *includes, const char *cflags,
	                             const char *expr)

Check if the expression *expr* is a valid preprocessor expression after
including the files listed in *includes* and compiling with the compilation
flags *cflags*. Return true if the expresion compiles.



