#include "pelconflib.c"

int main (int argc, char **argv)
{
	ac_init (".c", argc, argv, 1);

	ac_check_each_header_sequence ("inttypes.h  sys/types.h  foobar.h  windows.h", NULL);
	ac_check_each_func ("gettimeofday  SetWindowText", NULL);
	ac_has_proto ("sys/types.h dirent.h", NULL, "opendir");

	ac_config_out ("config.h", "TEST");
	ac_edit_makefile ("makefile.in", "makefile");
	ac_finish ();

	return 0;
}
