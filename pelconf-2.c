#include "pelconflib.c"
#include <stdlib.h>
#include <stdio.h>

int main (int argc, char **argv)
{
	ac_init (".cpp", argc, argv, 1);

	if (!ac_has_func_pkg_config ("peltk/base/locale.hpp", NULL,
	                         "peltk::base::set_cxx_locale", "peltk-base")) {
		ac_msg_error ("I need the library peltk-base. Please install it first.");
		return -1;
	}

	if (!ac_has_func_pkg_config("peltk/ucs/ucspp.hpp", NULL,
	                    "peltk::ucs::ucs_numeric_value", "peltk-ucs")) {
		ac_msg_error ("I need the library peltk::ucs. Please install it first.");
		return -1;
	}

	ac_config_out ("config.h", "PELTK_FORMATS");
	ac_edit_makefile ("makefile.in", "makefile");
	ac_create_pc_file ("peltk-formats", "File formats");
	ac_finish ();

	return 0;
}


