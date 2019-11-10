#include "pelconflib.c"
#include <stdlib.h>
#include <stdio.h>


int main (int argc, char **argv)
{
	ac_init (".cpp", argc, argv, 1);

	ac_has_func_lib_tag ("curses.h", NULL, "initscr", "pdcursesw", 0, "PDCURSESW_INCL") ||
	ac_has_func_lib_tag ("curses.h", NULL, "initscr", "pdcurses", 0, "PDCURSESW_INCL") ||
	ac_has_func_lib_tag ("ncursesw/curses.h", NULL, "initscr", "ncursesw", 0, "NCURSESW_INCL") ||
	ac_has_func_lib_tag ("curses.h", NULL, "initscr", "ncursesw", 0, "NCURSESW_CURSES") ||
	ac_has_func_lib_tag ("curses.h", NULL, "initscr", "curses", 0, "CURSES_PLAIN") ||
	ac_msg_error("Cannot find a suitable curses/ncurses library.");

	ac_has_func_lib ("io.h fcntl.h", NULL, "_setmode", "");
	ac_has_func_lib ("fnmatch.h", NULL, "fnmatch", "");
	if (!ac_has_func_lib_tag ("peltk/ucs/ucspp.hpp", NULL, "peltk::ucs::ucs_width", "peltk-ucs", 0, "PELTK_UCS")) {
		ac_msg_error("Cannot find the peltk-ucs library.");
	}
	if (!ac_has_func_pkg_config_tag ("peltk/base/logging.hpp", NULL, "peltk::base::warnx_errno", "peltk-base", "PELTK_BASE")) {
		ac_msg_error("Cannot find the peltk-base library.");
	}

	if (!ac_has_func_pkg_config_tag ("portacrypt/util.hpp", NULL, "portacrypt::crypto_bzero", "portacrypt", "PORTACRYPT")) {
		ac_msg_error("Cannot find the portacrypt library.");
	}
	ac_has_proto ("unistd.h", NULL, "getpass");


	ac_config_out ("config.h", "PELCONF");
	ac_edit_makefile ("makefile.in", "makefile");
	ac_finish ();

	return 0;
}








