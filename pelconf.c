/* THIS IS AN EXAMPLE */


#include "pelconflib.c"
#include <stdlib.h>
#include <stdio.h>

int main (int argc, char **argv)
{
	ac_init (".cpp", argc, argv, 1);
	ac_check_each_header_sequence ("unistd.h, sys/mman.h", NULL);
	ac_check_func_lib ("sys/time.h", NULL, "gettimeofday", NULL);

	ac_check_proto ("execinfo.h", "", "backtrace");
	ac_check_proto ("execinfo.h", "", "backtrace_symbols");
	ac_check_func_lib ("dlfcn.h", "", "dladdr", "dl");
	ac_check_proto ("unistd.h", NULL, "readlink");
	ac_check_func_lib ("windows.h, psapi.h", NULL, "EnumProcessModules", "psapi");


	/* Clock_gettime could be in the standard library or in the extra rt library. */
	ac_check_func_lib ("time.h", NULL, "clock_gettime", NULL) ||
	ac_check_func_lib ("time.h", NULL, "clock_gettime", "rt") ||
	ac_check_func_lib ("pthread.h", NULL, "clock_gettime", "pthread");

	/* Check if the tm struct has tm_gmtoff and tm_zone */
	ac_check_member ("time.h", NULL, "tm", "tm_gmtoff");
	ac_check_member ("time.h", NULL, "tm", "tm_zone");

	/* Do we have ntptimeval.tai? */
	ac_check_member ("sys/timex.h", NULL, "ntptimeval", "tai");

	ac_check_proto ("time.h", NULL, "gethrtime");
	ac_check_link ("Has __builtin_ia32_rdtsc()",
	               "int main() { return __builtin_ia32_rdtsc(); }\n",
	               "", NULL, "BUILTIN_IA32_RDTSC");
	ac_check_link ("Has __rdtsc()",
	               "int main() { return __rdtsc(); }\n",
	               "", NULL, "__RDTSC");

	/* Figure out the location of the getcwd() function. */
	ac_check_proto_tag ("unistd.h", NULL, "getcwd", "GETCWD_UNISTD_H") ||
	  ac_check_proto_tag ("dir.h", NULL, "getcwd", "GETCWD_DIR_H") ||
	  ac_msg_error ("The function getcwd() is needed, but could not be found.");

	ac_check_type ("sys/time.h", NULL, "timeval");
	ac_check_type_tag ("time.h", NULL, "timespec", "TIMESPEC_IN_TIME_H") ||
	    ac_check_type_tag ("pthread.h", NULL, "timespec", "TIMESPEC_IN_PTHREAD_H");

	/* Both posix and gnu define strerror_r in incompatible ways! */
	ac_check_signature ("string.h", NULL, "strerror_r", "int (*f)(int,char*,size_t)",
	        "POSIX_STRERROR_R");
	ac_check_signature ("string.h", NULL, "strerror_r", "char* (*f)(int,char*,size_t)",
	        "GNU_STRERROR_R");

	/* What do we need to do to get Woe32 to compile with threads? */
	ac_check_func_lib ("process.h", NULL, "_beginthread", NULL) ||
	ac_check_func_lib ("process.h", "-tWM", "_beginthread", NULL) ||
	ac_check_func_lib ("process.h", "-mthreads", "_beginthread", NULL);

	ac_check_func_lib ("string.h", NULL, "strsignal", NULL);

	/* Where did they hide the open() function. */
	ac_check_proto_tag ("sys/types.h, sys/stat.h, fcntl.h", NULL, "open", "OPEN_IN_FCNTL") ||
	ac_check_proto_tag ("sys/types.h, sys/stat.h, io.h", NULL, "open", "OPEN_IN_IO");

	/* Do we have the field st_blksize in struct stat? Used for I/O buffer sizing. */
	ac_check_member ("sys/stat.h", NULL, "stat", "st_blksize");

	/* Do we have the M$ version _stati64? */
	ac_check_type ("sys/types.h, sys/stat.h", NULL, "struct _stati64");


	/* GNU functions for getting the names of exceptions. */
	ac_check_proto_tag ("cxxabi.h", NULL, "abi::__cxa_demangle", "CXA_DEMANGLE");
	ac_check_proto_tag ("cxxabi.h", NULL, "abi::__cxa_current_exception_type", "CXA_CURRENTEX");

	/* Borland C++ function to get the name of the current exception */
	ac_check_proto_tag ("except.h", NULL, "__ThrowExceptionName", "THROWEXCEPTIONNAME");

	ac_check_proto ("string.h", NULL, "memccpy");
	ac_check_proto ("time.h", NULL, "nanosleep");
	ac_check_func_lib ("stdio.h", NULL, "snprintf", NULL);
	ac_check_func_pkg_config_tag ("pthread.h", NULL, "pthread_mutex_lock", "pthread", "PTHREAD");
	ac_check_proto ("pthread.h", NULL, "pthread_condattr_setclock");
	ac_check_proto ("sys/sysinfo.h", NULL, "get_nprocs");
	ac_check_proto ("pthread.h", NULL, "pthread_num_processors_np");
	ac_check_proto ("unistd.h", NULL, "readlink");

	/* Get at least one of the following or bail out! */
	ac_check_func_lib ("windows.h, imagehlp.h", NULL, "GetModuleFileName", NULL)
		|| ac_check_func_lib ("windows.h, imagehlp.h", NULL, "GetModuleFileName", "imagehlp")
		|| ac_check_func_lib ("dlfcn.h", NULL, "dladdr", "dl")
		|| ac_msg_error ("could not find a suitable implementation for dladdr");

	ac_check_func_lib ("shlobj.h", NULL, "SHGetFolderPathW", "shell32");
	ac_check_func_lib ("shlobj.h", NULL, "SHGetSpecialFolderLocation", "shell32");
	ac_check_proto ("pwd.h", NULL, "getpwuid");

	ac_check_func_lib ("objbase.h", NULL, "CoTaskMemFree", "ole32");

	/* Check GCC atomic's. We need the link step because it may happen that the
	   architecture does not support the operation directly and library calls are
	   generated. */
	ac_check_link ("Has __atomic_fetch_add builtin",
	               "int main() { int x=5; return __atomic_fetch_add(&x, 42, __ATOMIC_ACQUIRE); }\n",
	               "", NULL, "ATOMIC_FETCH_ADD");
	ac_check_link ("Has __sync_fetch_and_add builtin",
	        "int main()  { int x = 5;  return  __sync_fetch_and_add(&x, 2); }\n",
	        "", NULL, "SYNC_FETCH_AND_ADD");

	ac_check_link ("Has __sync_val_compare_and_swap builtin",
	        "int main()  { int x = 5; return __sync_val_compare_and_swap(&x, 3, 4); }\n",
	        NULL, NULL, "SYNC_VAL_COMPARE_AND_SWAP");

	ac_check_compile ("Has atomic.h with atomic_inc_ulong and atomic_dec_ulong_nv",
	        "#include <atomic.h>\n"
			"void foo(volatile unsigned long *x) { atomic_inc_ulong(x); }\n"
			"unsigned long bar(volatile unsigned long *x) { atomic_dec_ulong_nv(x); }\n",
			NULL, "ATOMIC_H_SOLARIS");

	ac_check_compile ("Has fcntl.h flags",
	        "#include <fcntl.h>\n"
			"int main () {\n"
			"   int x = O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_RDONLY | O_WRONLY;\n"
			"}\n", NULL, "FCNTL_FLAGS");

	ac_check_same_cxx_types ("stddef.h", NULL, "ptrdiff_t", "int_fast64_t",
	            "PTRDIFF_FAST64_EQUAL");
	ac_check_same_cxx_types ("stdint.h", NULL, "int16_t", "short", "EQUAL_INT16_SHORT");
	ac_check_same_cxx_types ("stdint.h", NULL, "int32_t", "short", "EQUAL_INT32_SHORT");
	ac_check_same_cxx_types ("stdint.h", NULL, "int32_t", "int", "EQUAL_INT32_INT");
	ac_check_same_cxx_types ("stdint.h", NULL, "int32_t", "long", "EQUAL_INT32_LONG");
	ac_check_same_cxx_types ("stdint.h", NULL, "int64_t", "int", "EQUAL_INT64_INT");
	ac_check_same_cxx_types ("stdint.h", NULL, "int64_t", "long", "EQUAL_INT64_LONG");
	ac_check_same_cxx_types ("stdint.h", NULL, "int64_t", "long long", "EQUAL_INT64_LLONG");

	ac_check_sizeof ("", NULL, "short");
	ac_check_sizeof ("", NULL, "int");
	ac_check_sizeof ("", NULL, "long");
	ac_check_sizeof ("", NULL, "long long");

	ac_check_sizeof ("stddef.h", NULL, "ptrdiff_t");
	ac_check_sizeof ("signal.h", NULL, "sig_atomic_t");
	ac_check_sizeof ("", NULL, "wchar_t");

	ac_check_member ("dirent.h", NULL, "dirent", "d_type");
	ac_check_member_tag ("locale.h", NULL, "lconv", "int_p_cs_precedes", "C99_LCONV");
	ac_check_proto ("langinfo.h", NULL, "nl_langinfo");

	ac_check_proto ("stdlib.h", NULL, "aligned_alloc") ||
	ac_check_proto ("malloc.h", NULL, "memalign") ||
	ac_check_proto ("malloc.h", NULL, "__mingw_aligned_malloc") ||
	ac_msg_error ("Couldn't find a suitable memalign");

	ac_check_link ("std::codecvt<char32_t,char,mbstate_t>",
	            "#include <locale>\n"
				"int main() { new std::codecvt<char32_t,char,mbstate_t>; }\n",
				"", NULL, "CXX_CODECVT_32");
	ac_check_link ("std::codecvt<char16_t,char,mbstate_t>",
	            "#include <locale>\n"
				"int main() { new std::codecvt<char16_t,char,mbstate_t>; }\n",
				"", NULL, "CXX_CODECVT_16");
	ac_check_link ("std::codecvt<wchar_t,char,mbstate_t>",
	            "#include <locale>\n"
				"int main() { new std::codecvt<wchar_t,char,mbstate_t>; }\n",
				"", NULL, "CXX_CODECVT_WC");


	ac_config_out ("config.h", "PELTK_BASE");
	ac_edit_makefile ("makefile.in", "makefile");
	ac_create_pc_file ("peltk-base", "General C++ utilies library");
	ac_finish ();

	return 0;
}


