/*
 * Copyright 2009-2018, Pelayo Bernedo.
 *
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http:/www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include <stddef.h>
#include <limits.h>


/* Size of text buffers. */
enum { BUFSIZE = 3000 };

/* The default source extension to be used for the test snippets. */
const char *aci_source_extension = ".c";

/* Human readable strings for the test results. */
static const char *aci_noyes[] = { "no", "yes" };

/* Provide additional information? */
static int aci_verbose = 0;

/* Did the user request help (the --help option)? */
static int aci_help_wanted = 0;

/* Keep intermediate files? */
static int aci_keep = 0;

/* Use simple GCC commands? */
static int aci_simple = 0;

/* Use static linking. */
static int aci_static = 0;

/* Are we compiling under Woe (includes cygwin)? */
static int aci_have_woe32 = 0;

/* Are we using cygwin? */
static int aci_have_cygwin = 0;

/* Base name of the file used for test snippets. */
static const char aci_test_file[] = "__autotst";

/* Prefix to be used for all the macros that we define. This
   is used for name spacing. */
static const char *aci_macro_prefix = "";

/* The invocation of make. */
static const char *aci_make_cmd;

/* Do we have variadic macro support as in C99 or C++11? */
static int aci_variadic_macros = 0;


static const char *aci_werror = "";

/* Prefix to be used when defining attributes. */
static const char *aci_attrib_pfx = "GCCA_";

static char aci_make_exe_prefix[100];
static char aci_install_prefix[FILENAME_MAX] = "";


/* Which compiler are we using? */
static enum {
	aci_cc_unknown, aci_cc_gcc, aci_cc_bcc32, aci_cc_tinyc, aci_cc_clang
} aci_compiler_id = aci_cc_unknown;


/* The command to change the name of the executable produced. This is -o $@ in all
   compilers that I know of, except in Borland C++, where it is -e$@. */
static const char *aci_exe_cmd = "-o $@";


/* Directive to include files in a makefile */
static const char *aci_include_form;

/* The name of the compiler specific make variables file that
   should be included. */
static char aci_makevars_file[FILENAME_MAX] = "";

/* The name that we computed for aci_makevars_file designates a file that does
   not exist. */
static int aci_warn_makevars = 0;

/* Did the makevar file specify the TARGET_ARCH */
static int aci_target_arch_given = 0;

/* Does the available version of make follow MS-DOS conventions? */
static int aci_dos_make = 0;

/* The command used to invoke the compiler. */
static const char *aci_compile_cmd = "";

/* The prefix used to specify libraries when linking. */
static const char *aci_lib_prefix = "l";

/* The suffix used to specify libraries when linking. */
static const char *aci_lib_suffix = "";







/* Variable length strings with buffer allocation. */


/* If out of memory is detected, sbufbad will be set to true. */
typedef struct sbuf_tag {
	char *s;
	size_t len, cap;
	int bad;
	char fixed[200];
} sbuf_t;

#define sbufchars(sb) ((sb)->s)
#define sbuflen(sb) ((sb)->len)
#define sbufbad(sb) ((sb)->bad)
#define sbufvalid(sb) (!(sb)->bad)



static const size_t rsize_max = ((size_t)-1) / 2;


/* Resize the buffer. This computes thew new capacity to hold at
   least "required" bytes. */

static size_t
compute_new_cap (size_t old_cap, size_t required)
{
	const float factor = 1.2;
	size_t res = old_cap;
	if (required > INT_MAX) {
		return required;
	}

	if (required == 0) {
		return (size_t)(res * factor) + 10;
	} else {
		while (res < required) {
			res = (size_t)(res * factor) + 10;
		}
		return res;
	}
}



void sbufinit (sbuf_t *sb)
{
	sb->s = sb->fixed;
	sb->len = 0;
	sb->cap = sizeof (sb->fixed) - 1;
	sb->bad = 0;
	sb->s[0] = 0;
}

void sbuffree (sbuf_t *sb)
{
	if (sb->s != sb->fixed) {
		free (sb->s);
		sb->s = sb->fixed;
		sb->cap = sizeof (sb->fixed) - 1;
	}
	sb->len = 0;
	sb->s[0] = 0;
	sb->bad = 0;
}


/* Make sure that there are at least n bytes available. May
   set the bad flag. */

char * sbufreserve (sbuf_t *sb, size_t n)
{
	if (n > rsize_max) {
		sb->s[0] = 0;
		sb->len = 0;
		sb->bad = 1;
		return 0;
	}

	if (sb->cap < n) {
		size_t new_cap = compute_new_cap (sb->cap, n);
		char *buff = (sb->s == sb->fixed ? NULL : sb->s);

		buff = (char*) realloc (buff, new_cap + 1);
		if (buff == NULL) {
			return 0;
		}
		if (sb->s == sb->fixed) {
			memcpy (buff, sb->fixed, sb->len + 1);
		}
		sb->s = buff;
		sb->cap = new_cap;
		sb->s[new_cap] = 0;
	}
	sb->bad = 0;
	return sb->s;
}


/* Truncate the string to n bytes. */
void sbuftrunc (sbuf_t *sb, size_t n)
{
	if (n <= sb->cap) {
		sb->len = n;
		sb->s[n] = 0;
	}
}


/* Clear the string. */
static void sbufclear (sbuf_t *sb)
{
	sb->len = 0;
	sb->s[0] = 0;
}

/* Append s to the string. May set the bad flag. */
int sbufcat (sbuf_t *sb, const char *s)
{
	char *cp, *limit;
	int must_copy;

	if (sb->bad || s == NULL) {
		return -1;
	}

	do {
		cp = sb->s + sb->len;
		limit = sb->s + sb->cap;

		while (cp < limit && *s) {
			*cp++ = *s++;
		}
		*cp = 0;
		sb->len = cp - sb->s;

		if (*s) {
			char *buff = sb->s;
			size_t new_cap = compute_new_cap (sb->cap, 0);
			must_copy = 0;
			if (sb->s == sb->fixed) {
				buff = NULL;
				must_copy = 1;
			}
			buff = (char*) realloc (buff, new_cap + 1);
			if (buff) {
				sb->cap = new_cap;
				sb->s = buff;
				if (must_copy) {
					memcpy (buff, sb->fixed, sizeof (sb->fixed));
				}
			} else {
				sb->s[0] = 0;
				sb->len = 0;
				sb->bad = 1;
				return -1;
			}
		 }
	} while (*s);
	return 0;
}


/* Append up to n bytes or stop at end of s, whichever comes first.
   May set the bad flag. */
int sbufncat (sbuf_t *sb, const char *s, size_t n)
{
	char *cp, *limit;
	const char *slimit;
	int must_copy;

	if (sb->bad || s == NULL) {
		return -1;
	}

	if (n > rsize_max) {
		sb->s[0] = 0;
		sb->len = 0;
		sb->bad = 0;
		return -1;
	}

	slimit = s + n;

	do {
		cp = sb->s + sb->len;
		limit = sb->s + sb->cap;

		if (cp + (slimit - s) < limit) {
			limit = cp + (slimit - s);
		}

		while (cp < limit && *s) {
			*cp++ = *s++;
		}
		*cp = 0;
		sb->len = cp - sb->s;

		if (*s && s < slimit) {
			char *buff = sb->s;
			size_t new_cap = compute_new_cap (sb->cap, 0);
			must_copy = 0;
			if (sb->s == sb->fixed) {
				buff = NULL;
				must_copy = 1;
			}
			buff = (char*) realloc (buff, new_cap + 1);
			if (buff) {
				sb->cap = new_cap;
				sb->s = buff;
				if (must_copy) {
					memcpy (buff, sb->fixed, sizeof (sb->fixed));
				}
			} else {
				sb->s[0] = 0;
				sb->len = 0;
				sb->bad = 1;
				return -1;
			}
		 }
	} while (*s && s < slimit);
	return 0;
}

/* Copy the string s into sb. May set the bad flag. */
int sbufcpy (sbuf_t *sb, const char *s)
{
	sb->len = 0;
	sb->bad = 0;
	return sbufcat (sb, s);
}

/* Copy the string s up to n bytes into sb. May set the bad flag. */
int sbufncpy (sbuf_t *sb, const char *s, size_t n)
{
	sb->len = 0;
	sb->bad = 0;
	return sbufncat (sb, s, n);
}


/* Like printf but into the buffer sb. */
void sbufformat (sbuf_t *sb, int reset, const char *fmt, ...)
{
	va_list ap;
	const char *s, *cp;
	int i;
	char small[100];

	if (reset) {
		sb->len = 0;
	}

	va_start (ap, fmt);

	while (*fmt) {
		cp = fmt;
		while (*cp && *cp != '%') ++cp;
		sbufncat (sb, fmt, cp - fmt);

		if (*cp == '%') {
			if (cp[1] == 's') {
				s = va_arg (ap, const char *);
				sbufcat (sb, s);
				fmt = cp + 2;
			} else if (cp[1] == 'd') {
				i = va_arg (ap, int);
				sprintf (small, "%d", i);
				sbufcat (sb, small);
				fmt = cp + 2;
			} else if (cp[1] != 0) {
				char two[2];
				two[0] = cp[1];
				two[1] = 0;
				sbufcat (sb, two);
				fmt = cp + 2;
			} else {
				fmt = cp + 1;
			}
		} else {
			fmt = cp;
		}
	}
	va_end (ap);
}


/* Read a line from the file f. May set the bad flag. Return 0 on success. */
int sbufgets (sbuf_t *sb, FILE *f)
{
	enum { chunk_size = 10 };
	char *cp;
	size_t n;
	int done = 0;

	sbuftrunc (sb, 0);
	do {
		cp = sbufreserve (sb, sbuflen (sb) + chunk_size);
		if (cp == 0) {
			sbuftrunc (sb, 0);
			sb->bad = 1;
			return -1;
		}
		cp += sbuflen (sb);
		if (fgets (cp, chunk_size, f) == NULL) {
			return -1;
		}
		n = strlen (cp);
		if (n > 0 && cp[n - 1] == '\n') {
			--n;
			done = 1;
		}
		sbuftrunc (sb, sbuflen(sb) + n);
	} while (!done);
	return 0;
}


/* End of sbuf routines. */


/* The list of include directories. */
static sbuf_t aci_include_dirs;

/* The list of library directories. */
static sbuf_t aci_lib_dirs;

/* The flags passed when compiling. */
static sbuf_t aci_extra_cflags;

/* The flags passed when linking. */
static sbuf_t aci_extra_ldflags;

/* Additional libraries at the end of the linking. */
static sbuf_t aci_additional_libs;

/* The flags used when testing for features. */
static sbuf_t aci_testing_flags;




/* Turn a string into an identifier usable by C. */
static void aci_identcopy (char *dst, size_t dest_size, const char *src)
{
	char *lim = dst + dest_size - 1;
	while (*src && dst < lim) {
		if (isalnum (*src)) {
			*dst++ = (char) toupper (*src);
		} else if (*src == '*') {
			*dst++ = 'P';
		} else {
			*dst++ = '_';
		}
		++src;
	}
	*dst = 0;
}


/* Turn the string s into a C identifier in place. */
static void aci_make_identifier (char *s)
{
	while (*s) {
		if (isalnum (*s)) {
			*s = (char) toupper (*s);
		} else if (*s == '*') {
			*s = 'P';
		} else {
			*s = '_';
		}
		++s;
	}
}

/* Turn the string s into a C identifier and store the result in sb.
   sb->bad may be true at exit. */
static void aci_identcat (sbuf_t *sb, const char *s)
{
	size_t pos = sbuflen (sb);
	sbufcat (sb, s);
	aci_make_identifier (sbufchars (sb) + pos);
}



/* Our OOM handler. Just abandon :-( */
static void aci_out_of_memory (void)
{
	fprintf (stderr, "Sorry, the program run out of memory. Terminating...\n");
	exit (EXIT_FAILURE);
}


/* Alloc or die */
static void * aci_xmalloc (size_t sz)
{
	void *result = malloc (sz);
	if (result == NULL) {
		aci_out_of_memory ();
	}
	return result;
}

/* Make a copy of the string. */
static char * aci_strsave (const char *s)
{
	size_t slen = strlen (s);
	char *result = (char*) aci_xmalloc (slen + 1);
	memcpy (result, s, slen);
	result[slen] = 0;
	return result;
}

static void aci_strfree (char *s)
{
	free (s);
}


/* A list of strings. */
typedef struct {
	size_t count, capacity;
	char **strs;
} aci_strlist_t;


static void aci_strlist_init (aci_strlist_t *sl)
{
	sl->count = 0;
	sl->capacity = 0;
	sl->strs = 0;
}

static void aci_strlist_destroy (aci_strlist_t *sl)
{
	size_t i;

	for (i = 0; i < sl->count; ++i) {
		aci_strfree (sl->strs[i]);
	}
	free (sl->strs);
}


/* Add s to the list of strings sl. If prepend is true it will be added at
   the beginning of the list. Otherwise at the end of the list. */

static void aci_strlist_add (aci_strlist_t *sl, const char *s, int prepend)
{
	if (sl->count == sl->capacity) {
		size_t i, ncap;
		char **newstrs;
		ncap = sl->capacity * 2 + 20;
		newstrs = (char **) aci_xmalloc (sizeof(const char*) * ncap);

		for (i = 0; i < sl->count; ++i) {
			newstrs[i] = sl->strs[i];
		}
		if (sl->strs) {
			free (sl->strs);
		}
		sl->strs = newstrs;
		sl->capacity = ncap;
	}

	if (prepend) {
		size_t i;

		for (i = sl->count; i > 0; --i) {
			sl->strs[i] = sl->strs[i - 1];
		}
		sl->strs[0] = aci_strsave (s);
	} else {
		sl->strs[sl->count] = aci_strsave (s);
	}
	sl->count++;
}


/* Returns an iterator to the start of the list. */
static char ** aci_strlist_begin (const aci_strlist_t *sl)
{
	return sl->strs;
}

/* Returns an iterator to the end of the list (one beyond the last element
   like in C++). */

static char ** aci_strlist_end (const aci_strlist_t *sl)
{
	return sl->strs + sl->count;
}


/* Return true if the string s is in the list sl. */
static int aci_strlist_find (aci_strlist_t *sl, const char *s)
{
	size_t i;

	for (i = 0; i < sl->count; ++i) {
		if (strcmp (s, sl->strs[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

/* Add s to the list if s is not already there. */
static void aci_strlist_add_unique (aci_strlist_t *sl, const char *s, int prepend)
{
	if (aci_strlist_find (sl, s)) {
		return;
	}
	aci_strlist_add (sl, s, prepend);
}





/* A single variable. Each variable has a name and a list of strings that make its
   value. */
typedef struct aci_varnode_s {
	char *name;
	aci_strlist_t  chunks;
	struct aci_varnode_s *next;
} aci_varnode_t;


/* List of variables. */
typedef struct {
	aci_varnode_t *root, *last;
} aci_varlist_t;


// List of program source code chunks that will be included in the config file.
static aci_strlist_t aci_tdefs;

/* List of packages that were found using pkg-config. */
static aci_strlist_t aci_pkg_config_packs;

/* List of variables to include in the makefile. */
static aci_varlist_t aci_makevars;

/* List of features requested from the command line. */
static aci_varlist_t aci_features;



static void aci_varlist_init (aci_varlist_t *vl)
{
	vl->root = vl->last = 0;
}


static void aci_varlist_destroy (aci_varlist_t *vl)
{
	aci_varnode_t *tmp, *vn = vl->root;

	while (vn) {
		aci_strfree (vn->name);
		aci_strlist_destroy (&vn->chunks);
		tmp = vn->next;
		free(vn);
		vn = tmp;
	}
	vl->root = vl->last = NULL;
}



// Add to the variable list vl a variable with the name "name" and with the
// value "value". If the variable already exits: if replace is true then
// replace the previous value with the new "value". If the variable already
// exists and replace is false either append or prepend to the exiting list
// of values depending on "prepend".

static void
aci_varlist_add (aci_varlist_t *vl, const char *name, const char *value,
                 int replace, int prepend)
{
	aci_varnode_t *vn;

	vn = vl->root;
	while (vn && strcmp (vn->name, name) != 0) {
		vn = vn->next;
	}

	if (vn == 0) {
		/* There is no item with this name. We must create a new one */
		vn = (aci_varnode_t*) aci_xmalloc (sizeof(aci_varnode_t));
		vn->name = aci_strsave (name);
		aci_strlist_init (&vn->chunks);
		vn->next = 0;
		if (vl->last) {
			vl->last->next = vn;
			vl->last = vn;
		} else {
			vl->last = vl->root = vn;
		}
	}
	if (replace && vn->chunks.count != 0) {
		aci_strlist_destroy (&vn->chunks);
		aci_strlist_init (&vn->chunks);
	}
	aci_strlist_add_unique (&vn->chunks, value, prepend);
}


// Add the variable name to the variable list vl with the value value. If
// the variable already exits either prepend or append the value to the
// existing value of the variable.
static void aci_varlist_cat (aci_varlist_t *vl, const char *name, const char *value,
                             int prepend)
{
	aci_varlist_add (vl, name, value, 0, prepend);
}


// Add the variable name to the variable list vl with value "value".
// Overwrite if the variable is already in the list.
static void aci_varlist_set (aci_varlist_t *vl, const char *name, const char *value)
{
	aci_varlist_add (vl, name, value, 1, 0);
}


// Find the variable "name" in the list. Return NULL if not found.
static aci_varnode_t *aci_varlist_find (aci_varlist_t *vl, const char *name)
{
	aci_varnode_t *vn = vl->root;
	while (vn && strcmp (vn->name, name) != 0) {
		vn = vn->next;
	}
	return vn;
}


// Dump the variable list to the file dst. If sep is true separate the name
// of the variable from its value with the equal sign.
static void aci_varlist_dump (const aci_varlist_t *vl, FILE *dst, int sep)
{
	aci_varnode_t *vn = vl->root;
	char **beg, **end;

	while (vn) {
		fprintf (dst, "%s", vn->name);
		if (sep) {
			fprintf (dst, "=");
		}
		beg = aci_strlist_begin (&vn->chunks);
		end = aci_strlist_end (&vn->chunks);

		while (beg != end) {
			fprintf (dst, " %s", *beg);
			++beg;
		}

		fprintf (dst, "\n");
		vn = vn->next;
	}
}


// Dump the feature list.
static void aci_dump_features (const aci_varlist_t *vl, FILE *dst, const char *prefix)
{
	aci_varnode_t *vn = vl->root;
	char **beg, **end;
	int needsep;

	while (vn) {
		fprintf (dst, "#define %s_FEATURE_%s ", prefix, vn->name);
		beg = aci_strlist_begin (&vn->chunks);
		end = aci_strlist_end (&vn->chunks);

		while (beg != end) {
			fprintf (dst, "%s ", *beg);
			++beg;
		}
		fprintf (dst, "\n");

		needsep = 0;

		fprintf (dst, "#define %s_FEATURE_STRING_%s \"", prefix, vn->name);
		beg = aci_strlist_begin (&vn->chunks);
		end = aci_strlist_end (&vn->chunks);

		while (beg != end) {
			if (needsep) {
				fprintf (dst, " ");
			}
			fprintf (dst, "%s", *beg);
			++beg;
			needsep = 1;
		}
		fprintf (dst, "\"\n");

		vn = vn->next;
	}
}


// A node in the list of checked flags.
typedef struct aci_flag_item_t {
	// The name of the flag and its description.
	char *tag, *comment;
	// Did we successfuly pass the test?
	int passed;
	// Next flag in list.
	struct aci_flag_item_t *next;
} aci_flag_item_t;


// Dispose the list of flags.
static void aci_flag_list_free (aci_flag_item_t *root)
{
	aci_flag_item_t *fi;

	while (root) {
		fi = root->next;

		aci_strfree (root->tag);
		aci_strfree (root->comment);
		free (root);
		root = fi;
	}
}

// Add the flag (tag, comment, passed) to the list of flags, just after
// *prev.
static void aci_flag_add_here (aci_flag_item_t **prev, const char *tag,
                               const char *cmt, int passed)
{
	aci_flag_item_t *tmp = (aci_flag_item_t*) aci_xmalloc (sizeof(aci_flag_item_t));
	tmp->tag = aci_strsave (tag);
	tmp->comment = aci_strsave (cmt);
	tmp->passed = passed;
	tmp->next = *prev;
	*prev = tmp;
}


// Add the flag to the list of flags, overwriting the existing value.
static void aci_flag_list_add (aci_flag_item_t **root, const char *tag,
            const char *cmt, int passed)
{
	aci_flag_item_t *newroot = *root;
	aci_flag_item_t **prev = &newroot;
	aci_flag_item_t *fi = newroot;
	sbuf_t newtag;
	char *nt;

	sbufinit (&newtag);

	sbufformat (&newtag, 1, "%sHAVE_%s", aci_macro_prefix, tag);
	nt = sbufchars (&newtag);

	while (fi) {
		if (strcmp(fi->tag, nt) == 0) {
			if (fi->passed) {
				if (passed) {
					*root = newroot;
					sbuffree (&newtag);
					return;
				}
				aci_flag_add_here (prev, nt, cmt, passed);
				*root = newroot;
				return;
			} else if (strcmp (fi->comment, cmt) == 0) {
				if (passed == 0) {
					*root = newroot;
					sbuffree (&newtag);
					return;
				}
			}
		}
		prev = &fi->next;
		fi = fi->next;
	}
	aci_flag_add_here (prev, nt, cmt, passed);
	*root = newroot;
	sbuffree (&newtag);
}


// Write the list of flags to the file dst.
static void aci_flag_list_dump (aci_flag_item_t *fi, FILE *dst)
{
	while (fi) {
		fprintf (dst, "/* %s ? */\n", fi->comment);
		if (fi->passed) {
			fprintf (dst, "#define %s 1\n\n", fi->tag);
		} else {
			fprintf (dst, "/* #define %s */\n\n", fi->tag);
		}
		fi = fi->next;
	}
}


// The list of flags that are checked.
static aci_flag_item_t *aci_flags_root = NULL;


// The public interface to add a flag to the config.h file.
void ac_add_flag (const char *name, const char *comment, int passed)
{
	aci_flag_list_add (&aci_flags_root, name, comment, passed);
}


// Add literal source code to the config file. If unique is nonzero make
// sure that the source code is not duplicated.
void ac_add_code (const char *src_code, int unique)
{
	if (unique) {
		aci_strlist_add_unique (&aci_tdefs, src_code, 0);
	} else {
		aci_strlist_add (&aci_tdefs, src_code, 0);
	}
}

// Set the value of a makefile variable. It makes sure that the variable is
// converted to a syntax compatible with make.
void ac_set_var (const char *name, const char *value)
{
	char big[1000];
	aci_identcopy (big, sizeof big, name);
	aci_varlist_set (&aci_makevars, big, value);
}

// Same as above but if the variable has already been defined then prepend
// value  to the existing value of name.
void ac_add_var_prepend (const char *name, const char *value)
{
	char big[1000];
	aci_identcopy (big, sizeof big, name);
	aci_varlist_cat (&aci_makevars, big, value, 1);
}

// Same as above but if the variable has already been defined then append
// "value"to the existing value of name.
void ac_add_var_append (const char *name, const char *value)
{
	char big[1000];
	aci_identcopy (big, sizeof big, name);
	aci_varlist_cat (&aci_makevars, big, value, 0);
}


// Write to the file dst the list of strings. If with_lf is nonzero then
// write each string in a separate line.
static void aci_dump_strlist (aci_strlist_t *aci_tdefs, FILE *dst, int with_lf)
{
	char **beg, **end;

	beg = aci_strlist_begin (aci_tdefs);
	end = aci_strlist_end (aci_tdefs);

	while (beg != end) {
		if (with_lf) {
			fprintf (dst, "%s\n", *beg);
		} else {
			fprintf (dst, "%s ", *beg);
		}
		++beg;
	}
}


// Copy the output of the commands and their errors to the log file.
static void aci_copy_to_log (void)
{
	FILE *fr, *fw;

	fw = fopen ("configure.log", "a");
	if (!fw) return;

	fr = fopen ("__dummys1", "r");
	if (fr) {
		char ln[1000];
		fprintf (fw, "Stdout: ");
		while (fgets (ln, sizeof ln, fr) != NULL) {
			fputs (ln, fw);
		}
		fclose (fr);
	}

	fr = fopen ("__dummys2", "r");
	if (fr) {
		char ln[1000];
		fprintf (fw, "\nStderr: ");
		while (fgets (ln, sizeof ln, fr) != NULL) {
			fputs (ln, fw);
		}
		fclose (fr);
	}

	fclose (fw);
}


#if 0 // defined(__CYGWIN__) || defined(__MINGW__)

// Woe is extremely slow compared to linux when it comes to launching
// processes. We try to avoid calling bash.
#include <process.h>
#include <unistd.h>
#include <fcntl.h>

static int aci_run_silent (const char *cmd)
{
	enum { avsz = 200 };
	char        *av[avsz + 1], *news;
	int         ac, i, rc = -1;
	const char  *eow;
	ptrdiff_t   slen;
	int         saved1, saved2, out1, out2;
	FILE        *conf;
	const char  *scmd = cmd;

	saved1 = saved2 = out1 = out2 = -1;

	ac = 0;
	while (*cmd) {
		while (isspace(*cmd)) ++cmd;
		eow = cmd;
		while (*eow && !isspace(*eow)) ++eow;
		if (ac >= avsz) goto cleanup;
		slen = eow - cmd;
		if (slen > 0) {
			news = (char*) malloc (slen + 1);
			if (news == NULL) goto cleanup;
			memcpy (news, cmd, slen);
			news[slen] = 0;
			av[ac++] = news;
		}
		if (*eow) {
			cmd = eow + 1;
		} else {
			cmd = eow;
		}
	}
	av[ac] = NULL;

	fflush (stdout);
	fflush (stderr);

	out1 = open ("__dummys1", O_CREAT|O_WRONLY, 0600);
	out2 = open ("__dummys2", O_CREAT|O_WRONLY, 0600);
	if (out1 < 0) {
		printf ("open(__dummys1) failed\n");
		goto cleanup;
	}
	if (out2 < 0) {
		printf ("open(__dummys2) failed\n");
		goto cleanup;
	}

	saved1 = dup(1);
	if (saved1 < 0) {
		printf ("dup(1) failed\n"); fflush (stdout);
		goto cleanup;
	}
	saved2 = dup(2);
	if (saved2 < 0) {
		fprintf (stderr, "dup(2) failed\n");    fflush (stderr);
		goto cleanup;
	}
	dup2 (out1, 1);
	dup2 (out2, 2);


	rc = spawnvp (_P_WAIT, av[0], av);

cleanup:
	for (i = 0; i < ac; ++i) {
		free (av[i]);
	}
	close (out1);
	close (out2);
	dup2 (saved1, 1);
	dup2 (saved2, 2);
	return rc;
}

#else

// Run a command with stdout and stderr redirected.
static int aci_run_silent (const char *cmd)
{
	sbuf_t scmd;
	int result;

	sbufinit (&scmd);
	sbufcpy (&scmd, cmd);
	sbufcat (&scmd, " >__dummys1 2>__dummys2");

	result = system (sbufchars(&scmd));
	sbuffree (&scmd);

	return result;
}

#endif


// Advance s to the first non space character.
static const char * aci_eatws (const char *s)
{
	while (isspace(*s)) ++s;
	return s;
}

// Advance s to the first space character.
static const char * aci_eatnws (const char *s)
{
	while (*s && !isspace(*s)) ++s;
	return s;
}


// Find the last character that is not a space in the string delimited by
// start and end.
static const char * aci_last_non_blank (const char *start, const char *end)
{
	while (end > start && isspace(end[-1])) --end;
	return end;
}



// Add to the string in sb the default compilation flags plus the flags
// specified in "flags".
static void aci_add_cflags (sbuf_t *sb, const char *flags)
{
	const char *sow, *eow;

	if (sbuflen (&aci_include_dirs) != 0) {
		sbufcat (sb, sbufchars (&aci_include_dirs));
		sbufcat (sb, " ");
	}

	sbufcat (sb, " ");
	sbufcat (sb, sbufchars (&aci_extra_cflags));
	sbufcat (sb, sbufchars (&aci_testing_flags));

	sbufformat (sb, 0, " -I%sinclude -L%slib ", aci_install_prefix,
	        aci_install_prefix);

	if (flags != NULL) {
		sow = flags;

		while (*sow) {
			sow = aci_eatws (sow);
			eow = aci_eatnws (sow);
			if (eow != sow) {
				sbufncat (sb, sow, eow - sow);
				sbufcat (sb, " ");
			}
			sow = eow;
		}
	}
}


// Add the given flags to the makefile variable "EXTRA_CFLAGS".
static void aci_add_cflags_to_makevars (const char *cflags)
{
	const char *sow, *eow;
	sbuf_t sb;

	if (cflags == NULL) {
		return;
	}

	sow = cflags;
	sbufinit (&sb);

	while (*sow) {
		sow = aci_eatws (sow);
		eow = aci_eatnws (sow);
		if (eow != sow) {
			sbufncpy (&sb, sow, eow - sow);
			ac_add_var_append ("EXTRA_CFLAGS", sbufchars(&sb));
		}
		sow = eow;
	}
	sbuffree (&sb);
}



// Assuming that sb contains the source code, add the headers that are
// listed in "includes". The headers are separated by spaces or commas.
static void aci_add_headers (sbuf_t *sb, const char *includes)
{
	const char *start, *end, *separator;

	if (includes ==  NULL) {
		return;
	}

	start = aci_eatws (includes);
	while (*start) {
		end = start;
		while (*end && *end != ',' && *end != ' ') ++end;
		separator = end;

		end = aci_last_non_blank (start, end);

		sbufcat (sb, "#include <");
		sbufncat (sb, start, end - start);
		sbufcat (sb, ">\n");

		if (*separator == 0) {
			break;
		}
		start = aci_eatws (separator + 1);
	}
}


// Add the default libraries plus the "libs" libraries to the string sb.
// This will be used as the command line to the compiler when linking. The
// libs string contains the list of libraries separted with either commas or
// spaces.
static void aci_add_libraries (sbuf_t *sb, const char *libs)
{
	const char *sow, *eow;

	if (sbuflen (&aci_lib_dirs) != 0) {
		sbufcat (sb, sbufchars (&aci_lib_dirs));
		sbufcat (sb, " ");
	}

	sbufcat (sb, " ");
	sbufcat (sb, sbufchars (&aci_extra_ldflags));

	if (libs == NULL) {
		return;
	}

	sow = aci_eatws (libs);
	while (*sow) {
		for (eow = sow; *eow && *eow != ',' && !isspace(*eow); ++eow) ;
		sbufcat (sb, aci_lib_prefix);
		sbufncat (sb, sow, eow - sow);
		sbufcat (sb, aci_lib_suffix);
		sbufcat (sb, " ");
		if (*eow == ',') ++eow;
		sow = aci_eatws (eow);
	}
}



// Show the message and stop the configuration.
int ac_msg_error (const char *hint)
{
	printf ("Fatal error while configuring: %s\n", hint);
	printf ("Aborting the configuration\n");
	exit (EXIT_FAILURE);
	return 0;
}



// Check if we can compile the src with the given flags. Returns nonzero if
// we can compile successfully.
static int aci_can_compile (const char *src, const char *cflags)
{
	int rc;
	FILE *f, *logfile;
	sbuf_t sb;

	sbufinit (&sb);

	sbufcpy (&sb, aci_test_file);
	sbufcat (&sb, aci_source_extension);

	f = fopen (sbufchars(&sb), "w");
	if (f == NULL) {
		sbuffree (&sb);
		return 0;
	}
	fprintf (f, "%s\n", src);
	fclose (f);

	sbufformat (&sb, 1, "%s -c %s", aci_compile_cmd, aci_werror);
	aci_add_cflags (&sb, cflags);
	sbufformat (&sb, 0, " %s%s", aci_test_file, aci_source_extension);

	rc = aci_run_silent (sbufchars(&sb));

	logfile = fopen ("configure.log", "a");
	if (logfile) {
		fprintf (logfile, "\n------------------------------------\n");
		fprintf (logfile, "compiling\n%swith command '%s'\n", src, sbufchars(&sb));
		fprintf (logfile, "return code is %d = %s\n\n", rc, strerror(rc));
		fclose (logfile);
	}
	aci_copy_to_log ();

	if (aci_verbose) {
		printf ("\ncompiling\n%swith command '%s'\n", src, sbufchars(&sb));
		printf ("return code is %d = %s\n\n", rc, strerror(rc));
		fflush (stdout);
	}

	sbuffree (&sb);
	return rc == 0;
}


/* Return non-zero if the passed source code can be compiled and linked */
static int aci_can_compile_link (const char *src, const char *cflags,
                                 const char *libs, int verbatim)
{
	int rc;
	FILE *f, *logfile;
	sbuf_t sb;

	sbufinit (&sb);

	sbufcpy (&sb, aci_test_file);
	sbufcat (&sb, aci_source_extension);

	f = fopen (sbufchars(&sb), "w");
	if (f == NULL) {
		sbuffree (&sb);
		return 0;
	}
	fprintf (f, "%s\n", src);
	fclose (f);

	sbufformat (&sb, 1, "%s %s", aci_compile_cmd, aci_werror);
	sbufformat (&sb, 0, " %s%s ", aci_test_file, aci_source_extension);
	aci_add_cflags (&sb, cflags);
	if (verbatim) {
		sbufcat (&sb, libs);
	} else {
		aci_add_libraries (&sb, libs);
	}
	sbufcat(&sb, sbufchars(&aci_additional_libs));

	rc = aci_run_silent (sbufchars (&sb));

	logfile = fopen ("configure.log", "a");
	if (logfile) {
		fprintf (logfile, "\n------------------------------\n");
		fprintf (logfile, "compiling\n[%s] with command '%s'\n", src, sbufchars(&sb));
		fprintf (logfile, "return code is %d = %s\n\n", rc, strerror(rc));
		fclose (logfile);
	}
	aci_copy_to_log ();

	if (aci_verbose) {
		printf ("\ncompiling\n[%s] with command '%s'\n", src, sbufchars(&sb));
		printf ("return code is %d = %s\n\n", rc, strerror(rc));
		fflush (stdout);
	}

	sbuffree (&sb);
	return rc == 0;
}





/* Return non-zero if the given include file(s) exists. */
static int aci_has_includes (const char *includes, const char *cflags)
{
	sbuf_t sb;
	int result;

	sbufinit (&sb);
	aci_add_headers (&sb, includes);
	sbufcat (&sb, "int main() { return 0; }\n");
	result = aci_can_compile (sbufchars(&sb), cflags);
	sbuffree (&sb);
	return result;
}


// Add the flags to the string sb.
static void aci_cat_cflags_cmt (sbuf_t *sb, const char *cflags)
{
	if (cflags != NULL && *cflags) {
		sbufcat (sb, " ");
		sbufcat (sb, cflags);
	}
}




// Check if we can include the headers listed in "includes" while compiling
// with the flags "cflags". If successful add tag to the configuration tags
// defined in config.h and add the cflags to the CFLAGS defined in the
// makefile.
int ac_check_headers_tag (const char *includes, const char *cflags, const char *tag)
{
	int result;
	sbuf_t comment;

	sbufinit (&comment);
	sbufformat (&comment, 1, "Has headers [%s]", includes);
	aci_cat_cflags_cmt (&comment, cflags);

	result = aci_has_includes (includes, cflags);
	aci_flag_list_add (&aci_flags_root, tag, sbufchars(&comment), result);

	printf ("%s : %s\n", sbufchars (&comment), aci_noyes[result]);
	fflush (stdout);
	if (result) {
		aci_add_cflags_to_makevars (cflags);
	}

	sbuffree (&comment);
	return result;
}

// Same as above, but the tag will be automatically defined based on the
// name of the included files.
int ac_check_headers (const char *includes, const char *cflags)
{
	char tag[BUFSIZE];

	aci_identcopy (tag, sizeof tag, includes);

	return ac_check_headers_tag (includes, cflags, tag);
}


// List of headers that we need.
static sbuf_t aci_common_headers;


// Includes provides a list of headers separated by commas or spaces. The
// function checks for the availability of each header in the same order as
// in the string while using the compilation flags cflags. The list of
// headers which are available will be added to the aci_common_headers
// variable.
void ac_check_each_header_sequence (const char *includes, const char *cflags)
{
	const char *start, *end, *separator;
	sbuf_t sb;
	int res;
	char tag[FILENAME_MAX];

	sbufinit (&sb);

	if (includes ==  NULL) {
		sbuffree (&sb);
		return;
	}

	start = aci_eatws (includes);
	while (*start) {
		end = start;
		while (*end && *end != ',' && *end != ' ') ++end;
		separator = end;

		end = aci_last_non_blank (start, end);

		sbufcpy (&sb, sbufchars (&aci_common_headers));
		sbufcat (&sb, "#include <");
		sbufncat (&sb, start, end - start);
		sbufcat (&sb, ">\n");

		sbufcat (&sb, "int main() { return 0; }\n");
		res = aci_can_compile (sbufchars(&sb), cflags);

		sbufncpy (&sb, start, end - start);
		aci_identcopy (tag, sizeof tag, sbufchars (&sb));
		sbufcpy (&sb, "Has header <");
		sbufncat (&sb, start, end - start);
		sbufcat (&sb, ">");
		aci_flag_list_add (&aci_flags_root, tag, sbufchars(&sb), res);
		printf ("%s: %s\n", sbufchars(&sb), aci_noyes[res]);
		fflush (stdout);

		if (res) {
			sbufcat (&aci_common_headers, "#include <");
			sbufncat (&aci_common_headers, start, end - start);
			sbufcat (&aci_common_headers, ">\n");
		}

		if (*separator == 0) {
			break;
		}
		start = aci_eatws (separator + 1);
	}

	sbuffree (&sb);
}



/* Return non-zero if the func is defined after including the file include */
static int aci_have_function_proto (const char *includes, const char *cflags,
                const char *func)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	aci_add_headers (&sb, includes);
	sbufformat (&sb, 0,
	        "int main() {\n"
			"    typedef void (*pvfn)(void);\n"
			"    pvfn p = (pvfn) %s;\n"
			"    return p != 0;\n"
			"}\n", func);

	result = aci_can_compile (sbufchars (&sb), cflags);
	sbuffree (&sb);
	return result;
}


// Return non-zero if the func is defined after including the file include
// and has the signature specified in "signature".
static int aci_have_signature (const char *includes, const char *cflags,
        const char *func, const char *signature)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	aci_add_headers (&sb, includes);
	sbufformat (&sb, 0,
	            "int main() { %s = %s; return 0; }\n", signature, func);
	result = aci_can_compile (sbufchars (&sb), cflags);
	sbuffree (&sb);
	return result;
}


// Check if the function "func" is declared after including the files listed
// in "includes" and compiling with the compilation flags "cflags". If the
// function is declared then define the tag "tag" in the config.h file.
int ac_check_proto_tag (const char *includes, const char *cflags,
            const char *func, const char *tag)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	sbufformat (&sb, 1, "Has prototype of %s in headers [%s]",
	        func, includes);
	aci_cat_cflags_cmt (&sb, cflags);

	result = aci_have_function_proto (includes, cflags, func);
	aci_flag_list_add (&aci_flags_root, tag, sbufchars (&sb), result);

	printf ("%s: %s\n", sbufchars (&sb), aci_noyes[result]);
	fflush (stdout);

	if (result) {
		aci_add_cflags_to_makevars (cflags);
	}

	sbuffree (&sb);
	return result;
}

// Same as above but the tag is automatically deduced from the name of the
// function.
int ac_check_proto (const char *includes, const char *cflags, const char *func)
{
	char tag[BUFSIZE];

	aci_identcopy (tag, sizeof tag, func);

	return ac_check_proto_tag (includes, cflags, func, tag);
}


// Check if the function "func" is declared with the signature
// "signature" after including the files listed in "includes" and compiling
// with the compilation flags "cflags". If the function is declared then
// define the tag "tag" in the config.h file.
int ac_check_signature (const char *includes, const char *cflags,
        const char *func, const char *signature, const char *tag)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	sbufformat (&sb, 1, "Has prototype of %s with signature %s in headers [%s]",
	            func, signature, includes);
	aci_cat_cflags_cmt (&sb, cflags);

	result = aci_have_signature (includes, cflags, func, signature);
	aci_flag_list_add (&aci_flags_root, tag, sbufchars (&sb), result);

	printf ("%s: %s\n", sbufchars (&sb), aci_noyes[result]);
	fflush (stdout);

	if (result) {
		aci_add_cflags_to_makevars (cflags);
	}

	sbuffree (&sb);
	return result;
}



// Check for compilation and linking success with the given included files,
// compilation flags and libraries. Return nonzero if the compilation and
// linking steps succeed.
static int aci_have_lib_function (const char *includes, const char *cflags,
                const char *func, const char *libnames, int verbatim)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	aci_add_headers (&sb, includes);
	sbufformat (&sb, 0,
	            "#include <stdio.h>\n"
				"int main () {\n"
				"    typedef void (*pvfn)(void);\n"
				"    pvfn p = (pvfn) %s;\n"
				"    printf (\"%%p\", p);\n"
				"    return 0;\n"
				"}\n", func);

	result = aci_can_compile_link (sbufchars (&sb), cflags, libnames, verbatim);
	sbuffree (&sb);
	return result;
}




// Check for compilation and linking success with the given included files,
// compilation flags and libraries. The included files are assumed to be
// pure C header files that are used in a C++ source file. They are included
// between #ifdef __cplusplus and #endif. Return nonzero if the compilation
// and linking steps succeed.
static int aci_have_lib_function_cxx (const char *includes, const char *cflags,
                                      const char *func, const char *libnames)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	sbufcat (&sb, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
	aci_add_headers (&sb, includes);
	sbufformat (&sb, 0,
	            "#ifdef __cplusplus\n}\n#endif\n\n"
				"#include <stdio.h>\n"
				"int main () {\n"
				"    typedef void (*pvfn)(void);\n"
				"    pvfn p = (pvfn) %s;\n"
				"    printf (\"%%p\", p);\n"
				"    return 0;\n"
				"}\n", func);

	result = aci_can_compile_link (sbufchars (&sb), cflags, libnames, 0);
	sbuffree (&sb);
	return result;
}


// Check for compilation and linking success with the given included files,
// compilation flags and libraries. Return nonzero if the compilation and
// linking steps succeed. Func is a member function like Foo::bar;
static int aci_have_lib_member (const char *includes, const char *cflags,
                                const char *func, const char *libnames,
                                int verbatim)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	aci_add_headers (&sb, includes);
	sbufformat (&sb, 0,
	     "#if __cplusplus > 201100\n"
		 "template <class T, class A0, class... Args> void use_func (A0 (T::*)(Args...)) {};\n"
		 "template <class T, class A0, class... Args> void use_func (A0 (T::*)(Args...) const) {}\n"
		 "#else\n"
		 "template <class T, class A0> void use_func (A0 (T::*)()) {}\n"
		 "template <class T, class A0> void use_func (A0 (T::*)()const) {}\n"
		 "template <class T, class A0, class A1> void use_func (A0 (T::*)(A1)) {}\n"
		 "template <class T, class A0, class A1> void use_func (A0 (T::*)(A1)const) {}\n"
		 "template <class T, class A0, class A1, class A2> void use_func (A0 (T::*)(A1, A2)) {}\n"
		 "template <class T, class A0, class A1, class A2> void use_func (A0 (T::*)(A1, A2)const) {}\n"
		 "template <class T, class A0, class A1, class A2, class A3> void use_func (A0 (T::*)(A1, A2, A3)) {}\n"
		 "template <class T, class A0, class A1, class A2, class A3> void use_func (A0 (T::*)(A1, A2, A3)const) {}\n"
		 "template <class T, class A0, class A1, class A2, class A3, class A4> void use_func (A0 (T::*)(A1, A2, A3, A4)) {}\n"
		 "template <class T, class A0, class A1, class A2, class A3, class A4> void use_func (A0 (T::*)(A1, A2, A3, A4)const) {}\n"
		 "template <class T, class A0, class A1, class A2, class A3, class A4, class A5> void use_func (A0 (T::*)(A1, A2, A3, A4, A5)) {}\n"
		 "template <class T, class A0, class A1, class A2, class A3, class A4, class A5> void use_func (A0 (T::*)(A1, A2, A3, A4, A5)const) {}\n"
		 "#endif\n"
		 "int main () { use_func (&%s); }\n", func);

	result = aci_can_compile_link (sbufchars (&sb), cflags, libnames, verbatim);
	sbuffree (&sb);
	return result;
}




// Add the libraries listed in libs to the EXTRALIBS variable of the
// makefile. If using DOS-style libraries then it also adds the pragma
// comment(name-of-lib) to the headers.
static void aci_add_libs_to_makevars (const char *libs)
{
	const char *eow, *sow;
	int dos_libs = 0;
	sbuf_t buf, pragma_buf;
	int skip;
	aci_strlist_t paths;
	char **beg, **end;

	if (libs == NULL || *libs == 0) {
		return;
	}

	sbufinit (&buf);
	sbufinit (&pragma_buf);
	aci_strlist_init (&paths);

	if (strcmp (aci_lib_suffix, ".lib") == 0) {
		dos_libs = 1;
	}

	sow = aci_eatws (libs);
	while (*sow) {
		eow = aci_eatnws (sow);
		skip = 0;

		sbufclear (&buf);
		if (sow[0] != '-') {
			 sbufcat (&buf, aci_lib_prefix);
		} else if (strncmp (sow, "-L", 2) == 0) {
			 char tmp = *eow;
			 *(char*)eow = 0;
			 aci_strlist_add_unique (&paths, sow, 0);
			 *(char*)eow = tmp;
			 skip = 1;
		}
		sbufncat (&buf, sow, eow - sow);

		if (dos_libs) {
			sbufcpy (&pragma_buf, "#pragma comment(lib, \"");
			sbufncat (&pragma_buf, sow, eow - sow);
			sbufcat (&pragma_buf, aci_lib_suffix);
			sbufcat (&pragma_buf, "\")");
			aci_strlist_add_unique (&aci_tdefs, sbufchars (&pragma_buf), 1);
		}

		sbufcat (&buf, aci_lib_suffix);
		sbufcat (&buf, " ");

		if (!skip) {
			ac_add_var_prepend ("EXTRALIBS", sbufchars (&buf));
		}

		sow = aci_eatws (eow);
	}

	beg = aci_strlist_begin (&paths);
	end = aci_strlist_end (&paths);
	while (beg != end) {
		--end;
		ac_add_var_prepend ("EXTRALIBS", *end);
	}

	sbuffree (&buf);
	sbuffree (&pragma_buf);
	aci_strlist_destroy (&paths);
}



// Check if we can compile and link using the function func, while including
// the files includes, using the compilation flags cflags and linking with
// libs. If successful add the tag to the configuration file. "libs" is
// interpreted as the names of the libraries. The function completes them to
// turn something like foo into -lfoo or foo.lib as required.
int ac_check_func_lib_tag (const char *includes, const char *cflags,
                           const char *func, const char *libs,
                           int verbatim, const char *tag)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);

	sbufformat (&sb, 1, "Has function %s in headers [%s]",
	            func, includes);
	aci_cat_cflags_cmt (&sb, cflags);

	if (libs && *libs) {
		if (verbatim) {
			sbufformat (&sb, 0, " with lflags: %s", libs);
		} else {
			sbufformat (&sb, 0, " libs [%s]", libs);
		}
	}

	result = aci_have_lib_function (includes, cflags, func, libs, verbatim);
	aci_flag_list_add (&aci_flags_root, tag, sbufchars (&sb), result);
	if (result) {
		aci_add_libs_to_makevars (libs);
		aci_add_cflags_to_makevars (cflags);
	}

	printf ("%s: %s\n", sbufchars (&sb), aci_noyes[result]);
	fflush (stdout);

	sbuffree (&sb);
	return result;
}



int ac_check_func_lib_tag_cxx (const char *includes, const char *cflags,
        const char *func, const char *libs, const char *tag)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);

	sbufformat (&sb, 1, "Has function %s in headers [%s]",
	            func, includes);
	aci_cat_cflags_cmt (&sb, cflags);

	if (libs && *libs) {
		sbufformat (&sb, 0, " libs [%s]", libs);
	}

	result = aci_have_lib_function_cxx (includes, cflags, func, libs);
	aci_flag_list_add (&aci_flags_root, tag, sbufchars (&sb), result);
	if (result) {
		aci_add_libs_to_makevars (libs);
		aci_add_cflags_to_makevars (cflags);
	}

	printf ("%s: %s\n", sbufchars (&sb), aci_noyes[result]);
	fflush (stdout);

	sbuffree (&sb);
	return result;
}



int ac_check_func_lib (const char *includes, const char *cflags,
        const char *func, const char *libs)
{
	char tag[BUFSIZE];

	aci_identcopy (tag, sizeof tag, func);

	return ac_check_func_lib_tag (includes, cflags, func, libs, 0, tag);
}

int ac_check_func_lib_cxx (const char *includes, const char *cflags,
                           const char *func, const char *libs)
{
	char tag[BUFSIZE];

	aci_identcopy (tag, sizeof tag, func);

	return ac_check_func_lib_tag_cxx (includes, cflags, func, libs, tag);
}



int ac_check_member_lib_tag (const char *includes, const char *cflags,
                             const char *func, const char *libs,
                             int verbatim, const char *tag)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);

	sbufformat (&sb, 1, "Has function %s in headers [%s]",
	            func, includes);
	aci_cat_cflags_cmt (&sb, cflags);

	if (libs && *libs) {
		if (verbatim) {
			sbufformat (&sb, 0, " with lflags: %s", libs);
		} else {
			sbufformat (&sb, 0, " libs [%s]", libs);
		}
	}

	result = aci_have_lib_member (includes, cflags, func, libs, verbatim);
	aci_flag_list_add (&aci_flags_root, tag, sbufchars (&sb), result);
	if (result) {
		aci_add_libs_to_makevars (libs);
		aci_add_cflags_to_makevars (cflags);
	}

	printf ("%s: %s\n", sbufchars (&sb), aci_noyes[result]);
	fflush (stdout);

	sbuffree (&sb);
	return result;
}


int ac_check_member_lib (const char *includes, const char *cflags,
                         const char *func, const char *libs,
                         int verbatim)
{
	char tag[BUFSIZE];

	aci_identcopy (tag, sizeof tag, func);

	return ac_check_member_lib_tag (includes, cflags, func, libs, verbatim, tag);
}





// Check if after including the files listed in "includes" and using the
// compilation flags "cflags" the structure "sname" has a field called
// "fname". Return nonzero if the field exists.
static int aci_have_field (const char *includes, const char *cflags,
            const char *sname, const char *fname)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	aci_add_headers (&sb, includes);
	sbufformat (&sb, 0,
	            "int main () {\n"
				"    %s foo;\n"
				"    void *pv = (void*)(&foo.%s);\n"
				"    return pv != 0;\n"
				"}\n", sname, fname);

	result = aci_can_compile (sbufchars (&sb), cflags);
	sbuffree (&sb);
	return result;
}


// If the given structure exits and has the field fname define the label to
// 1. Otherwise undefine it.
int ac_check_member_tag (const char *includes, const char *cflags,
        const char *sname, const char *fname, const char *tag)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	sbufformat (&sb, 1, "Has member %s in structure %s in headers [%s]",
	            fname, sname, includes);
	aci_cat_cflags_cmt (&sb, cflags);

	result = aci_have_field (includes, cflags, sname, fname);
	aci_flag_list_add (&aci_flags_root, tag, sbufchars (&sb), result);

	printf ("%s: %s\n", sbufchars (&sb), aci_noyes[result]);
	fflush (stdout);

	if (result) {
		aci_add_cflags_to_makevars (cflags);
	}

	sbuffree (&sb);
	return result;
}


// Same as above but the tag is deduced from the name of the field.
int ac_check_member (const char *includes, const char *cflags,
        const char *sname, const char *fname)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);

	sbufformat (&sb, 1, "MEMBER_%s_IN_%s", fname, sname);
	aci_make_identifier (sbufchars (&sb));

	result = ac_check_member_tag (includes, cflags, sname, fname,
	                                sbufchars (&sb));

	sbuffree (&sb);
	return result;
}


// Check if "tdname" is available as the name of a type after including the
// files listed in "includes" and compiling with the compilation flags
// "cflags".
static int aci_have_typedef (const char *includes, const char *cflags,
                const char *tdname)
{
	int result;
	sbuf_t src;

	sbufinit (&src);

	aci_add_headers (&src, includes);
	sbufcat (&src, tdname);
	sbufcat (&src, " foo;\n");

	result = aci_can_compile (sbufchars (&src), cflags);

	sbuffree (&src);
	return result;
}



// Check if "tname" is available as the name of a type after including the
// files listed in "includes" and compiling with the compilation flags
// "cflags". If "tname" is available then define tag in the configuration
// file.
int ac_check_type_tag (const char *includes, const char *cflags,
        const char *tname, const char *tag)
{
	int result;
	sbuf_t sb;

	sbufinit (&sb);
	sbufformat (&sb, 1, "Has type %s in headers [%s]",
	        tname, includes);
	aci_cat_cflags_cmt (&sb, cflags);

	result = aci_have_typedef (includes, cflags, tname);
	aci_flag_list_add (&aci_flags_root, tag, sbufchars (&sb), result);

	printf("%s: %s\n", sbufchars (&sb), aci_noyes[result]);
	fflush (stdout);

	if (result) {
		aci_add_cflags_to_makevars (cflags);
	}

	sbuffree (&sb);
	return result;
}


// Same as above but the tag is deduced from the name of the type.
int ac_check_type (const char *includes, const char *cflags, const char *tname)
{
	char tag[BUFSIZE];

	aci_identcopy (tag, sizeof tag, tname);

	return ac_check_type_tag (includes, cflags, tname, tag);
}


// Check if we can compile the source code in "src", using the compilation
// flags in "cflags". The comment "comment" will be put in the configuration
// file before the definition of the tag "tag".
int ac_check_compile (const char *comment, const char *src,
                      const char *cflags, const char *tag)
{
	int result;

	result = aci_can_compile (src, cflags);
	aci_flag_list_add (&aci_flags_root, tag, comment, result);

	if (result) {
		aci_add_cflags_to_makevars (cflags);
	}

	printf ("%s: %s\n", comment, aci_noyes[result]);
	fflush (stdout);
	return result;
}


// Check if we can compile and link the source code in "src", using the
// compilation flags in "cflags" and linking with the libraries in libs. The
// comment "comment" will be put in the configuration file before the
// definition of the tag "tag".
int ac_check_link (const char *comment, const char *src,
                   const char *flags, const char *libs, const char *tag)
{
	int result = aci_can_compile_link (src, flags, libs, 0);

	aci_flag_list_add (&aci_flags_root, tag, comment, result);

	if (result) {
		aci_add_cflags_to_makevars (flags);
	}

	printf ("%s: %s\n", comment, aci_noyes[result]);
	fflush (stdout);
	return result;
}


// Same as ac_check_compile, but define the tag only if the compilation
// fails.
int ac_check_compile_fail (const char *comment, const char *src,
                const char *cflags, const char *tag)
{
	int result;

	result = !aci_can_compile (src, cflags);
	aci_flag_list_add (&aci_flags_root, tag, comment, result);

	if (result) {
		aci_add_cflags_to_makevars (cflags);
	}

	printf ("%s: %s\n", comment, aci_noyes[result]);
	fflush (stdout);
	return result;
}


// Same as ac_check_link, buf define the tag only if the compilation and
// linking fails.
int ac_check_link_fail (const char *comment, const char *src,
            const char *flags, const char *libs, const char *tag)
{
	int result = !aci_can_compile_link (src, flags, libs, 0);

	aci_flag_list_add (&aci_flags_root, tag, comment, result);

	if (result) {
		aci_add_cflags_to_makevars (flags);
	}

	printf ("%s: %s\n", comment, aci_noyes[result]);
	fflush (stdout);
	return result;
}




// Check if the file "name" is available in the compilation environment.
int ac_check_file (const char *name)
{
	FILE *f = fopen (name, "rb");
	if (f) {
		fclose (f);
	}

	return f != NULL;
}



// Detects the size of a type without actually running the output (may be
// useful for cross compilers. It will include the files listed in
// "includes" and it will use the compilation flags "cflags". If show is
// true a macro of the form SIZEOF_##tname will be defined in the
// configuration file. The function returns -1 on failure or the
// sizeof(tname).
int aci_check_sizeof (const char *includes, const char *cflags,
                const char *tname, int show)
{
	int sz;
	enum { MAX_SZ = 100 };
	sbuf_t source, comment;

	sbufinit (&source);
	sbufinit (&comment);

	if (show) {
		sbufformat (&comment, 1, "sizeof(%s) in headers [%s]", tname, includes);
		aci_cat_cflags_cmt (&comment, cflags);
	}

	for (sz = 1; sz < MAX_SZ; ++sz) {
		sbuftrunc (&source, 0);
		aci_add_headers (&source, includes);
		sbufformat (&source, 0, "char dummy[sizeof(%s) == %d ? 1 : -1];\n",
		            tname, sz);

		if (aci_can_compile (sbufchars (&source), cflags)) {
			aci_add_cflags_to_makevars (cflags);
			break;
		}
	}
	if (sz == MAX_SZ) {
		sbuffree (&source);
		sbuffree (&comment);
		return -1;
	}

	if (show) {
		sbuf_t id;
		sbufinit (&id);
		aci_identcat (&id, tname);
		sbufformat (&source, 1,
		        "#define %sSIZEOF_%s %d",
		        aci_macro_prefix, sbufchars (&id), sz);

		aci_strlist_add_unique (&aci_tdefs, sbufchars (&source), 0);
		printf ("%s: %d\n", sbufchars (&comment), sz);
		fflush (stdout);
		sbuffree (&id);
	}

	sbuffree (&source);
	sbuffree (&comment);
	return sz;
}


// Public version of above. It will always put the result of the check in
// the configuration file.
int ac_check_sizeof (const char *includes, const char *cflags, const char *tname)
{
	return aci_check_sizeof (includes, cflags, tname, 1);
}




// Return true if "defname" has been defined as a macro after including the
// files listed in "includes" and compiling with the compilation flags
// "cflags".
int aci_check_define (const char *includes, const char *cflags,
                      const char *defname)
{
	int isdefined;
	sbuf_t source;

	sbufinit (&source);

	aci_add_headers (&source, includes);
	sbufformat (&source, 0,
	            "#ifndef %s\n"
				"#error name not defined\n"
				"#endif\n",
				defname);

	isdefined = aci_can_compile (sbufchars (&source), cflags);

	sbuffree (&source);
	return isdefined;
}


// Same as above buf informing the user about the check.
int ac_check_define (const char *includes, const char *cflags,
                     const char *defname)
{
	int isdefined = aci_check_define (includes, cflags, defname);
	printf ("Has %s defined in headers [%s]%s%s: %s\n", defname, includes,
	        cflags ? " " : "", cflags ? cflags : "",
	        isdefined ? "yes" : "no");
	fflush (stdout);            
	return isdefined;
}



// Check if the expression "expr" is a valid preprocessor expression after
// including the files listed in "includes" and compiling with the
// compilation flags "cflags".
int ac_check_cpp_expression (const char *includes, const char *cflags,
                             const char *expr)
{
	int ok;
	sbuf_t source;

	sbufinit (&source);

	aci_add_headers (&source, includes);
	sbufformat (&source, 0, "#if !(%s)\n#error kk\n#endif\n", expr);
	ok = aci_can_compile (sbufchars (&source), cflags);

	sbuffree (&source);

	return ok;
}




// Provide a suitable definition for inline even in C. Within the source you
// just use inline and if inlining is somehow available it will just be
// used. Otherwise it will be defined as empty.
static void aci_check_inline_keyword (void)
{
	const char *kw;
	int native_inline = 0;

	if (aci_can_compile ("inline int add(int a, int b) { return a + b; }\n", NULL)) {
		kw = "inline";
		native_inline = 1;
	} else if (aci_can_compile ("__inline__ int add(int a, int b) { return a + b; }\n", NULL)) {
		kw = "__inline__";
	} else if (aci_can_compile ("__inline int add(int a, int b) { return a + b; }\n", NULL)) {
		kw = "__inline";
	} else {
		kw = "static";
	}

	if (!native_inline) {
		sbuf_t sb;
		sbufinit (&sb);
		sbufcpy (&sb, "\n/* Ensure that the inline keyword is available. */\n");
		sbufformat (&sb, 0,
		        "#ifndef __cplusplus\n"
				"    #ifndef inline\n"
				"         #define inline %s\n"
				"    #endif\n"
				"#endif\n", kw);
		aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);
		sbuffree (&sb);
	}
	printf ("Using %s for inline\n", kw);
	fflush (stdout);
}


// Provide the restrict keyword.
static void aci_check_restrict_keyword (void)
{
	if (aci_can_compile ("void func(int * __restrict kk);\n", NULL)) {
		printf ("Has keyword __restrict: yes\n");
		ac_add_code ("#define restrict __restrict", 1);
	} else if (aci_can_compile ("void func(int * __restrict__ kk);\n", NULL)) {
		printf ("Has keyword __restrict: no\n");
		printf ("Has keyword __restrict__: yes\n");
		ac_add_code ("#define restrict __restrict__", 1);
	} else if (aci_can_compile ("void func(int * _Restrict kk);\n", NULL)) {
		printf ("Has keyword __restrict: no\n");
		printf ("Has keyword __restrict__: no\n");
		printf ("Has keyword _Restrict: yes\n");
		ac_add_code ("#define restrict _Restrict", 1);
	} else if (aci_can_compile ("void func(int * restrict kk);\n", NULL)) {
		printf ("Has keyword restrict: yes\n");
	} else {
		printf ("Has keyword restrict: no\n");
		printf ("Has keyword __restrict: no\n");
		printf ("Has keyword __restrict__: no\n");
		printf ("Has keyword _Restrict: no\n");
		ac_add_code ("#define restrict", 1);
	}
	fflush (stdout);
}


// Check if the C99 flexible array member feature is available.
static void aci_check_flexible_array_member (void)
{
	sbuf_t s;

	sbufinit (&s);
	printf ("Has C99's flexible array member? ");
	if (aci_can_compile ("struct s { int n; double d[]; };\n", NULL)) {
		sbufcpy (&s, "/* The compiler supports C99's flexible array members. */\n");
		sbufcat (&s, "#define ");
		sbufcat (&s, aci_macro_prefix);
		sbufcat (&s, "FLEXIBLE_ARRAY_MEMBER\n");
		printf ("yes\n");
	} else {
		sbufcpy (&s, "/* The compiler does not support C99's flexible array members. */\n");
		sbufcat (&s, "#define ");
		sbufcat (&s, aci_macro_prefix);
		sbufcat (&s, "FLEXIBLE_ARRAY_MEMBER 1\n");
		printf ("no\n");
	}
	ac_add_code (sbufchars (&s), 1);
	sbuffree (&s);
	fflush (stdout);
}


// Check if we can put variable declarations mixed with statements as in C99.
static void aci_check_mixed_code_vars (void)
{
	int res;
	printf ("Has C99's mixed declarations of variables and code? ");
	res  = aci_can_compile ("int foo(void) { int kk1;  kk1 = 3; int kk2;  kk2 = kk1; return kk2; };\n", NULL);
	printf ("%s\n", aci_noyes[res]);
	ac_add_flag ("C99_MIXED_VAR_DECLS", "Declarations of vars everywhere", res);
	fflush (stdout);
}





#if 0

/* See if there is a command to turn warnings into errors */
static void aci_check_werror (void)
{
	if (!aci_can_compile ("static int x;\n", "-Werror ")) {
		aci_werror = "-Werror "; /* GCC */
	} else if (!aci_can_compile ("static int x;\n", "-w! ")) {
		aci_werror = "-w! ";     /* Borland */
	}
}

#endif


static int          aci_have_int64 = 0;
static const char  *aci_int64_type = NULL;
static int          aci_have_stdint = 0;

static sbuf_t stdint_proxy;



// Helper to detect the number of value bits of the integer type "name".
static int aci_get_unsigned_type_bits (const char *name)
{
	int i;
	size_t n;

	char src[1000];

	for (i = 1; i < 256; ++i) {
		n = sprintf (src, "%s\n%s\n",
		    "#define IMAX_BITS(M) ((M)/((M)%0x3FFFFFFFL + 1)/0x3FFFFFFFL%0x3FFFFFFFL * 30 + (M)%0x3FFFFFFFL / ((M)%31 + 1)/31%31*5 + 4 - 12/((M)%31 + 3))\n",
		    "#define UVALUEBITS(T) IMAX_BITS((T)-1)");
		sprintf (src + n, "int v[UVALUEBITS(%s) == %d ? 1 : -1];\n", name, i);
		if (aci_can_compile (src, NULL)) {
			return i;
		}
	}
	return 0;
}


// Provide workarounds for the lack of long long and stdint.h If stdint.h is
// available it will be included. If not the types
// [u]int_[least|fast][16,32,64]_t will be defined.
static void aci_check_stdint (void)
{
	int has_long_long = 1;
	int char_bits, short_bits, int_bits, long_bits, llong_bits;
	int have_llong_max;
	const char *lltag;
	sbuf_t sb;

	sbufinit (&sb);

	/* Typedef the _Longlong and _ULonglong types as proposed in WG21 N1568. */

	if (aci_can_compile ("long long x;\n", NULL)) {
		printf ("Has keyword long long: yes\n");
		aci_flag_list_add (&aci_flags_root, "LONG_LONG_INT", "Has long long type", 1);
		aci_have_int64 = 1;
		aci_int64_type = "long long";
		lltag = "LL";
		sbufformat (&sb, 1,
		        "#ifndef PELCONF_LONGLONG_TYPEDEFED\n"
				"    #define PELCONF_LONGLONG_TYPEDEFED\n"
				"    /* Typedefs proposed in WG21 N1568. */\n"
				"    typedef long long _Longlong;\n"
				"    typedef unsigned long long _ULonglong;\n"
				"#endif", aci_macro_prefix, aci_macro_prefix);
		aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);
	} else if (aci_can_compile ("__int64 x;\n", NULL)) {
		printf ("Has keyword long long: no\n");
		printf ("Has keyword __int64: yes\n");
		aci_flag_list_add (&aci_flags_root, "MS_INT64", "Has MS __int64 variant", 1);
		aci_have_int64 = 1;
		aci_int64_type = "__int64";
		lltag = "i64";
		sbufformat (&sb, 1,
		        "#ifndef PELCONF_LONGLONG_TYPEDEFED\n"
				"    #define PELCONF_LONGLONG_TYPEDEFED\n"
				"    /* Typedefs proposed in WG21 N1568. */\n"
				"    typedef __int64 _Longlong;\n"
				"    typedef unsigned __int64 _ULonglong;\n"
				"#endif", aci_macro_prefix, aci_macro_prefix);
		aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);
	} else {
		has_long_long = 0;
		printf ("Has keyword long long: no\n");
		printf ("Has keyword __int64: no\n");
	}

	aci_strlist_add (&aci_tdefs, "\n/* Ensure that the types defined in stdint.h are available. */", 0);

	if (aci_has_includes ("cstdint", NULL)) {
		aci_flag_list_add (&aci_flags_root, "CSTDINT", "Has the <cstdint> header", 1);
		if (aci_check_define ("stdint.h cstdint", NULL, "INT32_C")) {
			aci_strlist_add_unique (&aci_tdefs, "#include <cstdint>", 0);
			printf ("Has header <cstdint>: yes\n");
		} else {
			aci_strlist_add_unique (&aci_tdefs,
			        "#ifndef __STDC_LIMIT_MACROS\n"
					"    #define __STDC_LIMIT_MACROS 1\n"
					"#endif\n"
					"#ifndef __STDC_CONSTANT_MACROS\n"
					"    #define __STDC_CONSTANT_MACROS 1\n"
					"#endif\n"
					"#include <cstdint>\n", 0);
			aci_add_cflags_to_makevars ("-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS");
			printf ("Has header <cstdint>: yes\n");
			printf ("<cstdint> requires the __STDC... macros, non conforming!\n");
		}
		sbufcat (&stdint_proxy, "#include <cstdint>\n");
		sbuffree (&sb);
		return;
	} else if (aci_has_includes ("stdint.h", NULL)) {
		aci_flag_list_add (&aci_flags_root, "STDINT_H", "Has the <stdint.h> header", 1);

		aci_strlist_add_unique (&aci_tdefs,
		        "#ifndef __STDC_LIMIT_MACROS\n"
				"    #define __STDC_LIMIT_MACROS 1\n"
				"#endif\n"
				"#ifndef __STDC_CONSTANT_MACROS\n"
				"    #define __STDC_CONSTANT_MACROS 1\n"
				"#endif\n"
				"#include <stdint.h>\n", 0);
		aci_add_cflags_to_makevars ("-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS");
		printf ("Has header <stdint.h>: yes\n");
		aci_have_stdint = 1;
		sbufcat (&stdint_proxy, "#include <stdint.h>\n");
		sbuffree (&sb);
		return;
	}

	printf ("Has header <stdint.h>: no\n");

	char_bits = aci_get_unsigned_type_bits ("unsigned char");
	printf ("unsigned char has %d value bits\n", char_bits);
	short_bits = aci_get_unsigned_type_bits ("unsigned short");
	printf ("unsigned short has %d value bits\n", short_bits);
	int_bits = aci_get_unsigned_type_bits ("unsigned int");
	printf ("unsigned int has %d value bits\n", int_bits);
	long_bits = aci_get_unsigned_type_bits ("unsigned long");
	printf ("unsigned long has %d value bits\n", long_bits);

	if (has_long_long) {
		char ull[100];
		sprintf (ull, "unsigned %s", aci_int64_type);
		llong_bits = aci_get_unsigned_type_bits (ull);
		printf ("%s has %d value bits\n", ull, llong_bits);
	}

	aci_strlist_add_unique (&aci_tdefs, "#include <limits.h>\n", 0);
	sbufcat (&stdint_proxy, "#include <limits.h>\n");

	sbufcpy (&sb, "#ifndef INT8_C\n");
	sbufcat (&sb, "  typedef signed char int_least8_t;\n");
	sbufcat (&sb, "  typedef unsigned char uint_least8_t;\n");
	sbufcat (&sb, "  typedef signed char int_fast8_t;\n");
	sbufcat (&sb, "  typedef unsigned char uint_fast8_t;\n");
	if (char_bits == 8) {
		sbufcat (&sb, "  typedef signed char int8_t;\n");
		sbufcat (&sb, "  typedef unsigned char uint8_t;\n");
	}
	sbufcat (&sb, "  #define INT8_C(x) (x)\n");
	sbufcat (&sb, "  #define UINT8_C(x) (x)\n");
	sbufcat (&sb, "  #define INT8_MAX CHAR_MAX\n");
	sbufcat (&sb, "  #define INT8_MIN CHAR_MIN\n");
	sbufcat (&sb, "  #define UINT8_MAX UCHAR_MAX\n");
	sbufcat (&sb, "  #define INT_LEAST8_MAX CHAR_MAX\n");
	sbufcat (&sb, "  #define INT_LEAST8_MIN CHAR_MIN\n");
	sbufcat (&sb, "  #define UINT_LEAST8_MAX UCHAR_MAX\n");
	sbufcat (&sb, "  #define INT_FAST8_MAX CHAR_MAX\n");
	sbufcat (&sb, "  #define INT_FAST8_MIN CHAR_MIN\n");
	sbufcat (&sb, "  #define UINT_FAST8_MAX UCHAR_MAX\n");
	sbufcat (&sb, "#endif\n");
	aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);
	sbufcat (&stdint_proxy, sbufchars (&sb));

	sbufcpy (&sb, "#ifndef INT16_C\n");
	sbufcat (&sb, "  typedef int int_fast16_t;\n");
	sbufcat (&sb, "  typedef unsigned int uint_fast16_t;\n");
	sbufcat (&sb, "  #define INT_FAST16_MAX INT_MAX\n");
	sbufcat (&sb, "  #define INT_FAST16_MIN INT_MIN\n");
	sbufcat (&sb, "  #define UINT_FAST16_MAX UINT_MAX\n");
	if (char_bits >= 16) {
		sbufcat (&sb, "  typedef char int_least16_t;\n");
		sbufcat (&sb, "  typedef unsigned char uint_least16_t;\n");
		sbufcat (&sb, "  #define INT_LEAST16_MAX CHAR_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST16_MIN CHAR_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST16_MAX UCHAR_MAX\n");
		if (char_bits == 16) {
			sbufcat (&sb, "  typedef char int16_t;\n");
			sbufcat (&sb, "  typedef unsigned char uint16_t;\n");
			sbufcat (&sb, "  #define INT16_MAX CHAR_MAX\n");
			sbufcat (&sb, "  #define INT16_MIN CHAR_MIN\n");
			sbufcat (&sb, "  #define UINT16_MAX UCHAR_MAX\n");
		}
	} else {
		sbufcat (&sb, "  typedef short int_least16_t;\n");
		sbufcat (&sb, "  typedef unsigned short uint_least16_t;\n");
		sbufcat (&sb, "  #define INT_LEAST16_MAX SHRT_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST16_MIN SHRT_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST16_MAX USHRT_MAX\n");
		if (short_bits == 16) {
			sbufcat (&sb, "  typedef short int16_t;\n");
			sbufcat (&sb, "  typedef unsigned short uint16_t;\n");
			sbufcat (&sb, "  #define INT16_MAX SHRT_MAX\n");
			sbufcat (&sb, "  #define INT16_MIN SHRT_MIN\n");
			sbufcat (&sb, "  #define UINT16_MAX USHRT_MAX\n");
		}
	}
	sbufcat (&sb, "  #define INT16_C(x) (x)\n");
	sbufcat (&sb, "  #define UINT16_C(x) (x)\n");
	sbufcat (&sb, "#endif\n");
	aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);
	sbufcat (&stdint_proxy, sbufchars (&sb));

	sbufcpy (&sb, "#ifndef INT32_C\n");
	if (char_bits >= 32) {
		sbufcat (&sb, "  typedef char int_least32_t;\n");
		sbufcat (&sb, "  typedef unsigned char uint_least32_t;\n");
		sbufcat (&sb, "  typedef int int_fast32_t;\n");
		sbufcat (&sb, "  typedef unsigned int uint_fast32_t;\n");
		sbufcat (&sb, "  #define INT_LEAST32_MAX CHAR_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST32_MIN CHAR_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST32_MAX UCHAR_MAX\n");
		sbufcat (&sb, "  #define INT_FAST32_MAX CHAR_MAX\n");
		sbufcat (&sb, "  #define INT_FAST32_MIN CHAR_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST32_MAX UCHAR_MAX\n");
		if (char_bits == 32) {
			sbufcat (&sb, "  typedef char int32_t;\n");
			sbufcat (&sb, "  typedef unsigned char uint32_t;\n");
			sbufcat (&sb, "  #define INT32_MAX CHAR_MAX\n");
			sbufcat (&sb, "  #define INT32_MIN CHAR_MIN\n");
			sbufcat (&sb, "  #define UINT32_MAX UCHAR_MAX\n");
		}
		sbufcat (&sb, "  #define INT32_C(x) (x)\n");
		sbufcat (&sb, "  #define UINT32_C(x) (x)\n");
	} else if (short_bits >= 32) {
		sbufcat (&sb, "  typedef short int_least32_t;\n");
		sbufcat (&sb, "  typedef unsigned short uint_least32_t;\n");
		sbufcat (&sb, "  typedef int int_fast32_t;\n");
		sbufcat (&sb, "  typedef unsigned int uint_fast32_t;\n");
		sbufcat (&sb, "  #define INT_LEAST32_MAX SHRT_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST32_MIN SHRT_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST32_MAX USHRT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST32_MAX INT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST32_MIN INT_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST32_MAX UINT_MAX\n");
		if (short_bits == 32) {
			sbufcat (&sb, "  typedef short int32_t;\n");
			sbufcat (&sb, "  typedef unsigned short uint32_t;\n");
			sbufcat (&sb, "  #define INT32_MAX SHRT_MAX\n");
			sbufcat (&sb, "  #define INT32_MIN SHRT_MIN\n");
			sbufcat (&sb, "  #define UINT32_MAX USHRT_MAX\n");
		}
		sbufcat (&sb, "  #define INT32_C(x) (x)\n");
		sbufcat (&sb, "  #define UINT32_C(x) (x)\n");
	} else if (int_bits >= 32) {
		sbufcat (&sb, "  typedef int int_least32_t;\n");
		sbufcat (&sb, "  typedef unsigned int uint_least32_t;\n");
		sbufcat (&sb, "  typedef int int_fast32_t;\n");
		sbufcat (&sb, "  typedef unsigned int uint_fast32_t;\n");
		sbufcat (&sb, "  #define INT_LEAST32_MAX INT_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST32_MIN INT_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST32_MAX UINT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST32_MAX INT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST32_MIN INT_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST32_MAX UINT_MAX\n");
		if (int_bits == 32) {
			sbufcat (&sb, "  typedef int int32_t;\n");
			sbufcat (&sb, "  typedef unsigned int uint32_t;\n");
			sbufcat (&sb, "  #define INT32_MAX INT_MAX\n");
			sbufcat (&sb, "  #define INT32_MIN INT_MIN\n");
			sbufcat (&sb, "  #define UINT32_MAX UINT_MAX\n");
		}
		sbufcat (&sb, "  #define INT32_C(x) (x)\n");
		sbufcat (&sb, "  #define UINT32_C(x) (x)\n");
	} else {
		sbufcat (&sb, "  typedef long int_least32_t;\n");
		sbufcat (&sb, "  typedef unsigned long uint_least32_t;\n");
		sbufcat (&sb, "  #define INT_LEAST32_MAX LONG_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST32_MIN LONG_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST32_MAX ULONG_MAX\n");
		sbufcat (&sb, "  #define INT_FAST32_MAX LONG_MAX\n");
		sbufcat (&sb, "  #define INT_FAST32_MIN LONG_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST32_MAX ULONG_MAX\n");
		if (long_bits == 32) {
			sbufcat (&sb, "  typedef long int32_t;\n");
			sbufcat (&sb, "  typedef unsigned long uint32_t;\n");
			sbufcat (&sb, "  #define INT32_MAX LONG_MAX\n");
			sbufcat (&sb, "  #define INT32_MIN LONG_MIN\n");
			sbufcat (&sb, "  #define UINT32_MAX ULONG_MAX\n");
		}
		sbufcat (&sb, "  #define INT32_C(x) (x##L)\n");
		sbufcat (&sb, "  #define UINT32_C(x) (x##UL)\n");
	}
	sbufcat (&sb, "#endif\n");
	aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);
	sbufcat (&stdint_proxy, sbufchars (&sb));

	have_llong_max = ac_check_define ("limits.h", NULL, "LLONG_MAX");

	sbufcpy (&sb, "#ifndef INT64_C\n");
	if (char_bits >= 64) {
		sbufcat (&sb, "  typedef char int_least64_t;\n");
		sbufcat (&sb, "  typedef unsigned char uint_least64_t;\n");
		sbufcat (&sb, "  typedef int int_fast64_t;\n");
		sbufcat (&sb, "  typedef unsigned int uint_fast64_t;\n");
		sbufcat (&sb, "  #define INT_LEAST64_MAX CHAR_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST64_MIN CHAR_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST64_MAX UCHAR_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MAX INT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MIN INT_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST64_MAX UINT_MAX\n");
		if (char_bits == 64) {
			sbufcat (&sb, "  typedef char int64_t;\n");
			sbufcat (&sb, "  typedef unsigned char uint64_t;\n");
			sbufcat (&sb, "  #define INT64_MAX CHAR_MAX\n");
			sbufcat (&sb, "  #define INT64_MIN CHAR_MIN\n");
			sbufcat (&sb, "  #define UINT64_MAX UCHAR_MAX\n");
		}
		sbufcat (&sb, "  #define INT64_C(x) (x)\n");
		sbufcat (&sb, "  #define UINT64_C(x) (x)\n");
	} else if (short_bits >= 64) {
		sbufcat (&sb, "  typedef short int_least64_t;\n");
		sbufcat (&sb, "  typedef unsigned short uint_least64_t;\n");
		sbufcat (&sb, "  typedef int int_fast64_t;\n");
		sbufcat (&sb, "  typedef unsigned int uint_fast64_t;\n");
		sbufcat (&sb, "  #define INT_LEAST64_MAX SHRT_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST64_MIN SHRT_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST64_MAX USHRT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MAX INT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MIN INT_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST64_MAX UINT_MAX\n");
		if (short_bits == 64) {
			sbufcat (&sb, "  typedef short int64_t;\n");
			sbufcat (&sb, "  typedef unsigned short uint64_t;\n");
			sbufcat (&sb, "  #define INT64_MAX SHRT_MAX\n");
			sbufcat (&sb, "  #define INT64_MIN SHRT_MIN\n");
			sbufcat (&sb, "  #define UINT64_MAX USHRT_MAX\n");
		}
		sbufcat (&sb, "  #define INT64_C(x) (x)\n");
		sbufcat (&sb, "  #define UINT64_C(x) (x)\n");
	} else if (int_bits >= 64) {
		sbufcat (&sb, "  typedef int int_least64_t;\n");
		sbufcat (&sb, "  typedef unsigned int uint_least64_t;\n");
		sbufcat (&sb, "  typedef int int_fast64_t;\n");
		sbufcat (&sb, "  typedef unsigned int uint_fast64_t;\n");
		sbufcat (&sb, "  #define INT_LEAST64_MAX INT_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST64_MIN INT_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST64_MAX UINT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MAX INT_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MIN INT_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST64_MAX UINT_MAX\n");
		if (int_bits == 64) {
			sbufcat (&sb, "  typedef int int64_t;\n");
			sbufcat (&sb, "  typedef unsigned int uint64_t;\n");
			sbufcat (&sb, "  #define INT64_MAX INT_MAX\n");
			sbufcat (&sb, "  #define INT64_MIN INT_MIN\n");
			sbufcat (&sb, "  #define UINT64_MAX UINT_MAX\n");
		}
		sbufcat (&sb, "  #define INT64_C(x) (x)\n");
		sbufcat (&sb, "  #define UINT64_C(x) (x)\n");
	} else if (long_bits >= 64) {
		sbufcat (&sb, "  typedef long int_least64_t;\n");
		sbufcat (&sb, "  typedef unsigned long uint_least64_t;\n");
		sbufcat (&sb, "  typedef long int_fast64_t;\n");
		sbufcat (&sb, "  typedef unsigned long uint_fast64_t;\n");
		sbufcat (&sb, "  #define INT_LEAST64_MAX LONG_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST64_MIN LONG_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST64_MAX ULONG_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MAX LONG_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MIN LONG_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST64_MAX ULONG_MAX\n");
		if (long_bits == 64) {
			sbufcat (&sb, "  typedef long int64_t;\n");
			sbufcat (&sb, "  typedef unsigned long uint64_t;\n");
			sbufcat (&sb, "  #define INT64_MAX LONG_MAX\n");
			sbufcat (&sb, "  #define INT64_MIN LONG_MIN\n");
			sbufcat (&sb, "  #define UINT64_MAX ULONG_MAX\n");
		}
		sbufcat (&sb, "  #define INT64_C(x) (x##L)\n");
		sbufcat (&sb, "  #define UINT64_C(x) (x##UL)\n");
	} else if (has_long_long) {
		sbufformat (&sb, 0, "  typedef %s int_fast64_t;\n", aci_int64_type);
		sbufformat (&sb, 0, "  typedef unsigned %s uint_fast64_t;\n", aci_int64_type);
		sbufformat (&sb, 0, "  typedef %s int_least64_t;\n", aci_int64_type);
		sbufformat (&sb, 0, "  typedef unsigned %s uint_least64_t;\n", aci_int64_type);
		if (!have_llong_max) {
			sbufformat (&sb, 0, "  #define ULLONG_MAX (~(0u%s))\n", lltag);
			sbufformat (&sb, 0, "  #define LLONG_MIN (1%s << %d)\n", lltag, llong_bits - 1);
			sbufcat (&sb, "  #define LLONG_MAX (-(LLONG_MIN + 1))\n");
		}
		sbufcat (&sb, "  #define INT_LEAST64_MAX LLONG_MAX\n");
		sbufcat (&sb, "  #define INT_LEAST64_MIN LLONG_MIN\n");
		sbufcat (&sb, "  #define UINT_LEAST64_MAX ULLONG_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MAX LLONG_MAX\n");
		sbufcat (&sb, "  #define INT_FAST64_MIN LLONG_MIN\n");
		sbufcat (&sb, "  #define UINT_FAST64_MAX ULLONG_MAX\n");
		if (llong_bits == 64) {
			sbufformat (&sb, 0, "  typedef %s int64_t;\n", aci_int64_type);
			sbufformat (&sb, 0, "  typedef unsigned %s uint64_t;\n", aci_int64_type);
			sbufcat (&sb, "  #define INT_LEAST64_MAX LLONG_MAX\n");
			sbufcat (&sb, "  #define INT_LEAST64_MIN LLONG_MIN\n");
			sbufcat (&sb, "  #define UINT_LEAST64_MAX ULLONG_MAX\n");
		}
		sbufformat (&sb, 0, "  #define INT64_C(x) (x##%s)\n", lltag);
		sbufformat (&sb, 0, "  #define UINT64_C(x) (x##u%s)\n", lltag);
	}
	sbufcat (&sb, "#endif\n");
	aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);
	sbufcat (&stdint_proxy, sbufchars (&sb));

	sbufcpy (&sb, "#ifndef INTMAX_C\n");
	if (has_long_long) {
		sbufcat (&sb, "  typedef int_fast64_t intmax_t;\n");
		sbufcat (&sb, "  typedef uint_fast64_t uintmax_t;\n");
		sbufcat (&sb, "  #define INTMAX_C(x) INT64_C(x)\n");
		sbufcat (&sb, "  #define UINTMAX_C(x) UINT64_C(x)\n");
		sbufcat (&sb, "  #define INTMAX_MAX LLONG_MAX\n");
		sbufcat (&sb, "  #define INTMAX_MIN LLONG_MIN\n");
		sbufcat (&sb, "  #define UINTMAX_MAX ULLONG_MAX\n");
	} else {
		sbufcat (&sb, "  typedef long intmax_t;\n");
		sbufcat (&sb, "  typedef unsigned long uintmax_t;\n");
		sbufcat (&sb, "  #define INTMAX_C(x) (x##L)\n");
		sbufcat (&sb, "  #define UINTMAX_C(x) (x##UL)\n");
		sbufcat (&sb, "  #define INTMAX_MAX LONG_MAX\n");
		sbufcat (&sb, "  #define INTMAX_MIN LONG_MIN\n");
		sbufcat (&sb, "  #define UINTMAX_MAX ULONG_MAX\n");
	}
	sbufcat (&sb, "#endif\n");
	aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);
	sbufcat (&stdint_proxy, sbufchars (&sb));

	aci_strlist_add_unique (&aci_tdefs,
	        "#ifndef SIZE_MAX\n"
			"  #define SIZE_MAX (~((size_t)0))\n"
			"#endif\n", 0);

	sbuffree (&sb);
	fflush (stdout);
}



// Provide format macros.
static void aci_check_some_inttypes (void)
{
	if (aci_has_includes ("inttypes.h", NULL)) {
		ac_add_code ("#ifndef __STDC_FORMAT_MACROS\n"
					 "#define __STDC_FORMAT_MACROS 1\n"
					 "#include <inttypes.h>\n"
					 "#endif\n", 1);
		return;
	}

	/* We have no way to detect at compile time the behaviour of printf. */
	if (strcmp (aci_int64_type, "__int64") == 0) {
		/* Assume this uses I64 crap */
		ac_add_code ("#define PRIiMAX \"I64i\"", 1);
		ac_add_code ("#define PRIdMAX \"I64d\"", 1);
		ac_add_code ("#define PRIuMAX \"I64u\"", 1);
		ac_add_code ("#define PRIxMAX \"I64x\"", 1);
		ac_add_code ("#define PRIXMAX \"I64X\"", 1);
		ac_add_code ("#define PRIoMAX \"I64o\"", 1);
	} else {
		ac_add_code ("#define PRIiMAX \"lli\"", 1);
		ac_add_code ("#define PRIdMAX \"lld\"", 1);
		ac_add_code ("#define PRIuMAX \"llu\"", 1);
		ac_add_code ("#define PRIxMAX \"llx\"", 1);
		ac_add_code ("#define PRIXMAX \"llX\"", 1);
		ac_add_code ("#define PRIoMAX \"llo\"", 1);
	}
}



// Find the block of bytes "block" of size "block_size" within the buffer
// "buffer" of size "buffer_size". If found return non-zero.
static int aci_find_block (const unsigned char *buffer, size_t buffer_size,
                           const unsigned char *block, size_t block_size)
{
	const unsigned char *pos = buffer;
	const unsigned char *end = buffer + buffer_size - block_size;

	if (buffer_size < block_size) {
		return 0;
	}

	while (pos && pos < end) {
		pos = (const unsigned char *) memchr (pos, *block, buffer_size - (pos - buffer));
		if (pos && pos < end) {
			if (memcmp(pos, block, block_size) == 0) {
				return 1;
			}
			++pos;
		}
	}
	return 0;
}



// This will define WORDS_BIGENDIAN or WORDS_LITTLEENDIAN if CHAR_BIT is 8
// and the generated code uses either big endian or little endian. If
// CHAR_BIT is not 8 or the endianness is mixed neither symbol will be
// defined.

static void aci_check_endian_cross (const char *objext)
{
	sbuf_t sb;
	FILE *bf = NULL;
	long obj_size;
	const char src[] = "long v = 0x11223344;\n";
	unsigned char *buffer = NULL;
	const unsigned char be[4] = { 0x11, 0x22, 0x33, 0x44 };
	const unsigned char le[4] = { 0x44, 0x33, 0x22, 0x11 };

	sbufinit (&sb);
	sbufformat (&sb, 1, "%s%s", aci_test_file, objext);

	remove (sbufchars(&sb));
	if (!aci_can_compile (src, NULL)) {
		goto leave;
	}


	bf = fopen (sbufchars(&sb), "rb");
	if (bf) {
		fseek (bf, 0, SEEK_END);
		obj_size = ftell(bf);
		buffer = (unsigned char*) aci_xmalloc (obj_size);
		if (buffer == NULL) {
			goto leave;
		}
		fseek (bf, 0, SEEK_SET);
		fread (buffer, 1, obj_size, bf);

		if (aci_find_block (buffer, obj_size, be, sizeof be)) {
			printf ("Generating code for big endian cpu\n");
			fflush (stdout);
			sbufcpy (&sb, "/* Generating code for big endian cpu */\n");
			sbufformat (&sb, 0, "#define %sWORDS_BIGENDIAN 1\n", aci_macro_prefix);
			ac_add_code (sbufchars (&sb), 1);
		} else if (aci_find_block (buffer, obj_size, le, sizeof le)) {
			printf ("Generating code for little endian cpu\n");
			fflush (stdout);
			sbufcpy (&sb, "/* Generating code for little endian cpu */\n");
			sbufformat (&sb, 0, "#define %sWORDS_LITTLEENDIAN 1\n", aci_macro_prefix);
			ac_add_code (sbufchars (&sb), 1);
		}
	}
leave:
	if (bf) fclose (bf);
	if (buffer) free (buffer);
	sbuffree (&sb);
}



static void aci_check_builtin_overflow(void)
{
	ac_check_compile("Has GNU builtin overflow check",
	    "bool foo(int a, int b, int *c) {\n"
		"    return __builtin_add_overflow(a, b, c);\n"
		"}\n", NULL, "GCC_OVERFLOW");
}




static int aci_need_cxx_check = 0;
static void aci_check_cxx (void);


// Are we running under Woe.
static void aci_check_woe32 (void)
{
	/* We check explicitely for Win32, instead of using _WIN32. Cygwin
	does not define _WIN32, although Win32 is available. */

	aci_have_woe32 = aci_have_function_proto ("windows.h", NULL, "GetWindowsDirectory");
}


// Are we running under Woe.
int ac_check_woe32 (void)
{
	return aci_have_woe32;
}


// Check for variants of inline assembly, backtrace and return address.
static void check_inline_assembly (void)
{
	int have_retaddr = 1;
	int have_backtrace;

	ac_check_compile ("Has GNU style inline assembly",
	    "int swap_local(volatile int *x, int newv) {\n"
		"    int res = newv;\n"
		"    __asm__ volatile (\"xchgl %0, (%2)\"\n"
		"               : \"=r\"(res) : \"0\"(res), \"r\"(x));\n"
		"    return res; }\n", NULL,
		"GNU_STYLE_ASSEMBLY");

	ac_check_compile ("Has register pseudovariables and __emit__",
	    "unsigned int foo(void) {\n"
		"    unsigned int lo, hi;\n"
		"   __emit__(0x0f, 0x31);   // rdtsc\n"
		"   lo = _EAX;\n"
		"   hi = _EDX;\n"
		"   return (hi + lo);\n}\n", NULL,
		"REGISTER_PSEUDOVARS");

	if (ac_check_compile ("Has _ReturnAddress()",
	    "void* foo(void) { return _ReturnAddress(); }\n",
	    NULL, "MSC_RETURN_ADDRESS")) {
		/* Do nothing. Use the builtin function. */
	} else if (ac_check_compile ("Has __builtin_return_address",
	        "void* foo(void) { return __builtin_return_address(0); }\n",
	        NULL, "BUILTIN_RETURN_ADDRESS")) {
		ac_add_code ("#define _ReturnAddress() __builtin_return_address(0)", 1);
	} else {
		have_retaddr = 0;
	}

	have_backtrace = ac_check_proto ("execinfo.h", NULL, "backtrace");

	if (!have_retaddr) {
		if (have_backtrace) {
			ac_add_code ("static inline void * _ReturnAddress(void) {\n"
					 "    void *reta = 0;\n"
					 "    backtrace (&reta, 1);\n"
					 "    return reta;\n"
					 "}\n", 1);
		} else {
			ac_add_code ("#define _ReturnAddress() 0", 1);
		}
	}
}


static void aci_check_align_keyword (void)
{
	sbuf_t sb;
	int have_align = ac_check_compile ("Has __attribute__((aligned(n)))",
	                    "__attribute__((aligned(16))) int x;\n",
	                    NULL, "ALIGNED");
	sbufinit (&sb);
	sbufformat (&sb, 1, "#define %s%sALIGN(n) ", aci_macro_prefix,
	        aci_attrib_pfx);
	if (have_align) {
		sbufcat (&sb, "__attribute__((aligned(n)))");
	} else if (ac_check_compile ("Has __declspec(align(n))",
	                    "__declspec(align(16)) int x;\n",
	                    NULL, "ALIGNED")) {
		sbufcat (&sb, "__declspec(align(n))");
	}
	ac_add_code (sbufchars(&sb), 1);
	sbuffree (&sb);
}


static void aci_check_thread_local (void)
{
	int has_tls = aci_can_compile ("thread_local int x;\n", NULL);
	if (!has_tls) {
		has_tls = aci_can_compile ("__thread int x;\n", NULL);
		if (has_tls) {
			aci_strlist_add_unique (&aci_tdefs,
			        "#define thread_local __thread\n", 0);
		} else {
			has_tls = aci_can_compile ("__declspec(thread) int x;\n", NULL);
			if (has_tls) {
				aci_strlist_add_unique (&aci_tdefs,
				        "#define thread_local __declspec(thread)\n", 0);
			}
		}
	}
	aci_flag_list_add (&aci_flags_root, "THREAD_LOCAL_STORAGE_CLASS",
	            "Has thread_local storage class available", has_tls);

	printf ("Has thread_local storage class available: %s\n", aci_noyes[has_tls]);
	fflush (stdout);
}


static void aci_check_stdbool (void)
{
	if (aci_has_includes ("stdbool.h", NULL)) {
		ac_add_code ("#ifndef __cplusplus\n#include <stdbool.h>\n#endif", 1);
	} else if (aci_can_compile("_Bool kk;\n", NULL)) {
		ac_add_code ("#ifndef __cplusplus\ntypedef _Bool bool;\n#endif", 1);
	} else {
		sbuf_t sb;
		sbufinit (&sb);
		sbufformat (&sb, 1,
		             "#ifndef PELCONF_C99_STDBOOL_DEFINED\n"
					 "    #define PELCONF_C99_STDBOOL_DEFINED 1\n"
					 "    #ifndef __cplusplus\n"
					 "        typedef enum { false, true } bool;\n"
					 "    #endif\n"
					 "#endif\n");
		ac_add_code (sbufchars (&sb), 1);
		sbuffree (&sb);
	}
}


static void aci_check_va_copy (void)
{
	if (ac_check_define ("stdarg.h", NULL, "va_copy")) {
		ac_add_code ("/* Ensure that va_copy is available.*/\n#include <stdarg.h>\n", 1);
	} else {
		ac_add_code ("/* Ensure that va_copy is available.*/\n#include <stdarg.h>\n"
					 "#ifndef va_copy\n"
					 "    #define va_copy(dst,src) memmove (&dst, &src, sizeof(va_list))\n"
					 "#endif\n", 1);
	}
}


static void aci_check_variadic_macros (void)
{
	static const char src[] = "#define VAM(...) printf(__VA_ARGS__)\n";
	aci_variadic_macros = ac_check_compile ("Has C99 variadic macros", src, NULL, "VARIADIC_MACROS");
}


// Check for variants of shell commands to manipulate files.
static void aci_check_commands (void)
{
	const char *cp = "cp";
	const char *cpr = "cp -r";
	const char *rm = "rm";
	const char *ln = "ln";
	const char *lns = "ln -s";
	const char *cwd = "./";
	FILE *f;
	sbuf_t sb;

	if (aci_have_woe32) {
		if (aci_run_silent ("cp --help") != 0) {
			cp = "copy";
			cpr = "xcopy /s";
		}
		if (aci_run_silent ("rm --help") != 0) {
			rm = "del";
		}
		if (aci_run_silent ("ln --help") != 0) {
			ln = "copy";
			lns = "copy";
		} else {
			FILE *fw = fopen (aci_test_file, "w");
			ln = "ln";
			if (fw) {
				fprintf (fw, "kk");
				fclose (fw);

				sbufinit (&sb);
				sbufformat (&sb, 1, "ln -s %s %s2", aci_test_file, aci_test_file);

				if (aci_run_silent (sbufchars (&sb)) == 0) {
					lns = "ln -s";
				} else {
					lns = "ln";
				}
				sbuffree (&sb);
			} else {
				lns = "ln";
			}
		}


		/* We must check whether the make program uses DOS or UNIX conventions. */
		sbufinit (&sb);

		/* First create a small program with the strange name in our dir */
		sbufformat (&sb, 1, "%s%s", aci_test_file, aci_source_extension);
		f = fopen (sbufchars (&sb), "w");
		if (f != NULL) {
			fprintf (f, "int main () { return 0; }\n");
			fclose (f);
			sbufformat (&sb, 1, "%s %s", aci_compile_cmd, aci_exe_cmd);
			if (sbufchars (&sb)[sbuflen(&sb) - 1] == '@' &&
			    sbufchars (&sb)[sbuflen(&sb) - 2] == '$') {
				sbuftrunc (&sb, sbuflen (&sb) - 2);
			}
			sbufformat (&sb, 0, "%s.exe %s%s", aci_test_file, aci_test_file,
			        aci_source_extension);
			aci_run_silent (sbufchars (&sb));

			/* Now create a make file that attempts to run our program. */
			sbufformat (&sb, 1, "%s.mk", aci_test_file);
			f = fopen (sbufchars (&sb), "w");
			if (f != NULL) {
				fprintf (f, "all:\n\t%s.exe\n\n", aci_test_file);
				fclose (f);
				sbufformat (&sb, 1, "%s -f%s.mk", aci_make_cmd, aci_test_file);
				if (aci_run_silent (sbufchars (&sb)) == 0) {
					cwd = "";
				}
			}
		}
		sbuffree (&sb);
	}

	printf ("The command to copy files is %s\n", cp);
	ac_add_var_append ("COPY", cp);
	printf ("The command to copy directories is %s\n", cpr);
	ac_add_var_append ("COPYREC", cpr);
	printf ("The command to remove files is %s\n", rm);
	ac_add_var_append ("REMOVE", rm);
	printf ("The command to create hard links is %s\n", ln);
	ac_add_var_append ("LN", ln);
	printf ("The command to create symbolic links is %s\n", lns);
	ac_add_var_append ("LN_S", lns);
	printf ("The prefix to run files from the current dir in the makefile is %s\n", cwd);
	ac_add_var_append ("ME", cwd);
	strcpy (aci_make_exe_prefix, cwd);

	fflush (stdout);

	if (aci_run_silent ("install --help") == 0) {
		ac_add_var_append ("INSTALL", "install -m 0755");
		ac_add_var_append ("INSTALL711", "install -m 0711");
		ac_add_var_append ("INSTALL_DATA", "install -m 0644");
		ac_add_var_append ("INSTALL_DIR", "install -d");

	} else {
		printf ("Could not find the program install(1).\n");
		ac_add_var_append ("INSTALL", cp);
		ac_add_var_append ("INSTALL711", cp);
		ac_add_var_append ("INSTALL_DATA", cp);
		ac_add_var_append ("INSTALL_DIR", "mkdir");
	}

	if (aci_run_silent ("/sbin/ldconfig --help") == 0) {
		ac_add_var_append ("LDCONFIG", "/sbin/ldconfig -nv");
	} else {
		ac_add_var_append ("LDCONFIG", "echo");
	}
	fflush (stdout);
}


typedef enum { att_gnu, att_cxx11, att_both } Attsyn;

static const char *aci_attsyn[] = {
	"__attribute__((%s))",
	"[[%s]]"
};


int aci_check_var_attribute (const char *attribute, Attsyn as)
{
	sbuf_t sb;
	int res;

	sbufinit (&sb);
	sbufformat (&sb, 1, aci_attsyn[as], attribute);
	sbufcat (&sb, " int x;\n");

	res = aci_can_compile (sbufchars (&sb), NULL);

	sbuffree (&sb);
	return res;
}

int aci_check_func_attribute (const char *attribute, int usedef, Attsyn as)
{
	sbuf_t sb;
	int res;

	sbufinit (&sb);
	sbufformat (&sb, 1, aci_attsyn[as], attribute);
	sbufcat (&sb, " int foo(void)");
	if (usedef) {
		sbufcat (&sb, " { return 0; }\nint main () { return 0; }\n");
		res = aci_can_compile_link (sbufchars (&sb), NULL, "", 0);
	} else {
		sbufcat (&sb, ";\n");
		res = aci_can_compile (sbufchars (&sb), NULL);
	}

	sbuffree (&sb);
	return res;
}


int aci_check_func_attribute_with_args (const char *body_att,
         const char *body, Attsyn as)
{
	sbuf_t sb;
	int res;

	sbufinit (&sb);
	sbufformat (&sb, 1, aci_attsyn[as], body_att);
	sbufcat (&sb, body);
	sbufcat (&sb, "\n");

	res = aci_can_compile (sbufchars (&sb), NULL);

	sbuffree (&sb);
	return res;
}



int ac_check_var_attribute (const char *attribute, const char *external, Attsyn as)
{
	sbuf_t sb1, sb2, sb3;
	int res = 0;

	sbufinit (&sb1);
	sbufinit (&sb2);
	sbufinit (&sb3);

	if (as == att_cxx11 || as == att_both) {
		res = aci_check_var_attribute (attribute, att_cxx11);
		printf ("Has variable attribute [[%s]]: %s\n", attribute, aci_noyes[res]);
		if (res) {
			sbufformat (&sb2, 1, "#define %s%s%s [[%s]]", aci_macro_prefix,
			        aci_attrib_pfx, external, attribute);
			ac_add_code (sbufchars(&sb3), 1);
			goto clean;
		}
	}

	if (as == att_cxx11) {
		goto clean;
	}

	as = att_gnu;
	sbufformat (&sb1, 1, "__%s__", attribute);
	res = aci_check_var_attribute (sbufchars (&sb1), as);
	printf("Has variable attribute __%s__: %s\n", attribute, aci_noyes[res]);


	if (res) {
		sbufformat (&sb2, 1, aci_attsyn[as], sbufchars(&sb1));
		sbufformat (&sb3, 1,
		        "#define %s%s%s %s", aci_macro_prefix, aci_attrib_pfx,
		        external, sbufchars(&sb2));
		ac_add_code (sbufchars(&sb3), 1);
	} else {
		res = aci_check_var_attribute (attribute, as);
		printf("Has variable attribute %s: %s\n", attribute, aci_noyes[res]);
		if (res) {
			sbufformat (&sb2, 1, aci_attsyn[as], attribute);
			sbufformat (&sb3, 1, "\n#define %s%s%s %s",
			            aci_macro_prefix, aci_attrib_pfx, external,
			            sbufchars(&sb2));
			ac_add_code (sbufchars(&sb3), 1);
		} else {
			sbufformat (&sb1, 1,
			    "#define %s%s%s", aci_macro_prefix, aci_attrib_pfx, external);
			ac_add_code (sbufchars(&sb1), 1);
		}
	}

clean:
	sbufcpy (&sb1, aci_attrib_pfx);
	aci_identcat (&sb1, external);
	sbufformat (&sb2, 1, "Has attribute %s", attribute);
	aci_flag_list_add (&aci_flags_root, sbufchars (&sb1), sbufchars (&sb2), res);

	sbuffree (&sb3);
	sbuffree (&sb2);
	sbuffree (&sb1);

	fflush (stdout);
	return res;
}

int ac_check_func_attribute (const char *attribute, const char *external,
        int usedef, int literal, Attsyn as)
{
	sbuf_t sb1, sb2, sb3;
	int res = 0;

	sbufinit (&sb1);
	sbufinit (&sb2);
	sbufinit (&sb3);

	if (as == att_cxx11 || as == att_both) {
		res = aci_check_func_attribute (attribute, usedef, att_cxx11);
		printf ("Has function attribute [[%s]]: %s\n", attribute, aci_noyes[res]);
		if (res) {
			sbufformat (&sb2, 1, "#define %s%s%s [[%s]]", aci_macro_prefix,
			        aci_attrib_pfx, external, attribute);
			ac_add_code (sbufchars(&sb2), 1);
			goto clean;
		}
	}

	if (as == att_cxx11) {
		goto clean;
	}

	as = att_gnu;

	if (!literal) {
		sbufformat (&sb1, 1, "__%s__", attribute);
		res = aci_check_func_attribute (sbufchars (&sb1), usedef, as);
		printf("Has function attribute __%s__: %s\n", attribute, aci_noyes[res]);
	}
	if (as == att_cxx11) res = 0;

	if (res) {
		sbufformat (&sb2, 1, aci_attsyn[as], sbufchars(&sb1));
		sbufformat (&sb3, 1, "#define %s%s%s %s", aci_macro_prefix,
		            aci_attrib_pfx, external, sbufchars(&sb2));
		ac_add_code (sbufchars(&sb3), 1);
	} else {
		res = aci_check_func_attribute (attribute, usedef, as);
		printf("Has function attribute %s: %s\n", attribute, aci_noyes[res]);
		if (res) {
			sbufformat (&sb2, 1, aci_attsyn[as], attribute);
			sbufformat (&sb3, 1, "#define %s%s%s %s", aci_macro_prefix,
			            aci_attrib_pfx, external, sbufchars(&sb2));
			ac_add_code (sbufchars(&sb3), 1);
		} else {
			sbufformat (&sb1, 1, "#define %s%s%s", aci_macro_prefix,
			            aci_attrib_pfx, external);
			ac_add_code (sbufchars(&sb1), 1);
		}
	}

clean:
	sbufcpy (&sb1, aci_attrib_pfx);
	aci_identcat (&sb1, external);
	sbufformat (&sb2, 1, "Has attribute %s", attribute);
	aci_flag_list_add (&aci_flags_root, sbufchars (&sb1), sbufchars (&sb2), res);

	sbuffree (&sb3);
	sbuffree (&sb2);
	sbuffree (&sb1);

	fflush (stdout);
	return res;
}



static int aci_check_att_format_1 (const char *fmt, const char *fmt_ext,
                int attempt)
{
	int res;
	sbuf_t sb0, sb1, sb2;

	sbufinit (&sb0);
	sbufinit (&sb1);
	sbufinit (&sb2);

	sbufformat (&sb0, 1, "__format__(__%s__,1,2)", fmt);
	res = aci_check_func_attribute_with_args (sbufchars (&sb0),
	                "int myprintf(const char *f, ...);", att_gnu);
	printf ("Has attribute __format__(__%s__,i,j): %s\n", fmt, aci_noyes[res]);

	if (res) {
		sbufformat (&sb0, 1, "__format__(__%s__,I,J)", fmt);
		sbufformat (&sb1, 1, aci_attsyn[0], sbufchars (&sb0));
	} else {
		sbufformat (&sb0, 1, "format(%s,1,2)", fmt);
		res = aci_check_func_attribute_with_args (sbufchars (&sb0),
		            "int myprintf(const char *f, ...);", att_gnu);
		printf ("Has attribute format(%s,i,j): %s\n", fmt, aci_noyes[res]);
		if (res) {
			sbufformat (&sb0, 1, "format(%s,I,J)", fmt);
			sbufformat (&sb1, 1, aci_attsyn[0], "format(printf,I,J)");
		}
	}

	sbufformat (&sb2, 1, "#define %s%s%s(I,J) %s",
	        aci_macro_prefix, aci_attrib_pfx, fmt_ext, sbufchars (&sb1));
	if (res || !attempt) {
		ac_add_code (sbufchars (&sb2), 1);
	}

	sbuffree (&sb2);
	sbuffree (&sb1);
	sbuffree (&sb0);

	fflush (stdout);
	return res;
}


void aci_check_att_format (void)
{
	sbuf_t sb1, sb2;
	int res;

	sbufinit (&sb1);
	sbufinit (&sb2);

	if (aci_check_att_format_1 ("gnu_printf", "PRINTF", 1) == 0) {
		aci_check_att_format_1 ("printf", "PRINTF", 0);
	}

	aci_check_att_format_1 ("scanf", "SCANF", 0);

	res = aci_check_func_attribute_with_args ("__format_arg__(1)",
	            "const char * foo(const char *f);", att_gnu);
	printf ("Has attribute __format_arg__(i): %s\n", aci_noyes[res]);

	if (res) {
		sbufformat (&sb1, 1, aci_attsyn[0], "__format_arg__(I)");
	} else {
		res = aci_check_func_attribute_with_args ("format_arg(1)",
		        "const char * foo(const char *f);", att_gnu);
		printf ("Has attribute format_arg(i): %s\n", aci_noyes[res]);

		if (res) {
			sbufformat (&sb1, 1, aci_attsyn[0], "format_arg(I)");
		}
	}

	sbufformat (&sb2, 1, "#define %s%sFORMAT_ARG(I) %s",
	        aci_macro_prefix, aci_attrib_pfx, sbufchars (&sb1));
	ac_add_code (sbufchars (&sb2), 1);

	sbuffree (&sb2);
	sbuffree (&sb1);

	fflush (stdout);
}



// See if the compiler supports the compilation flag "flag". If it does set
// the makefile variable "makevar" to "flag".
int ac_check_compiler_flag (const char *flag, const char *makevar)
{
	printf ("Does the compiler accept the option %s ", flag);
	if (aci_can_compile_link ("int func(int x) { return x; }\nint main () { return func(42); }\n", flag, NULL, 0)) {
		ac_set_var (makevar, flag);
		printf ("yes\n");
		fflush (stdout);
		return 0;
	} else {
		printf ("no\n");
		fflush (stdout);
		return -1;
	}
}


// Check for the presence of ssize_t and typedef it otherwise.
static void aci_check_ssize (void)
{
	if (aci_can_compile ("#include <unistd.h>\nssize_t x;\n", "")) {
		ac_add_code ("#include <unistd.h>", 1);
	} else {
		sbuf_t sb;
		int szu = aci_check_sizeof ("stddef.h", "", "size_t", 0);
		int szi = aci_check_sizeof ("stddef.h", "", "ptrdiff_t", 0);
		if (szu == 0) {
			printf ("could not find the size of size_t\n");
			exit (1);
		}
		if (szu == szi) {
			ac_add_code ("typedef ptrdiff_t ssize_t;", 1);
		} else {
			sbufinit (&sb);
			sbufformat (&sb, 0, "typedef int_least%d_t ssize_t;", szu * 8);
			ac_add_code (sbufchars (&sb), 1);
			sbuffree (&sb);
		}
	}
}


// Check for the unicode types char16_t and char32_t.
static void aci_check_char32 (void)
{
	sbuf_t sb;
	int have = ac_check_type ("", NULL, "char32_t");
	ac_check_type_tag ("uchar.h", NULL, "char32_t", "CHAR32_T_IN_UCHAR_H");
	ac_check_headers ("cuchar", NULL);

#if 0
	if (have) {
		sbufcat (&aci_testing_flags, " -D__STDC_UTF_32__");
	}
#endif

	// Ensure that wchar_t is always distinct from char16_t and char32_t.
	// This is what the C++11 standard says.

	sbufinit (&sb);
	sbufcpy (&sb,
	        "#ifndef HAVE_CHAR32_T\n"
			"  #ifdef CHAR32_T_IN_UCHAR_H\n"
			"    #include <uchar.h>\n"
			"  #elif defined(HAVE_CUCHAR)\n"
			"    #include <cuchar>\n"
			"  #elif !defined(__STDC_UTF_32__)\n");

	sbufcat (&sb,
	        "    typedef uint_least16_t char16_t;\n"
			"    typedef uint_least32_t char32_t;\n");


	sbufcat (&sb,
	        "    #define __STDC_UTF_32__\n"
			"    #define __STDC_UTF_16__\n"
			"  #endif\n"
			"#endif\n\n");

	ac_add_code (sbufchars (&sb), 1);

	if (ac_check_compile ("Has U\"text\" support",
	                      "const void *p = U\"text\";\n",
	                      NULL, "NATIVE_U_ESCAPE")) {
		ac_add_code ("#ifndef UCS\n#define UCS(X) U##X\n#endif\n", 1);
		ac_add_code ("#ifndef __cpp_unicode_literals\n"
				 "  #define __cpp_unicode_literals 200710\n"
				 "#endif\n", 1);
	} else if (ac_check_sizeof ("stddef.h", NULL, "wchar_t") == 4) {
		ac_add_code ("#ifndef UCS\n#define UCS(X) ((const char32_t*)L##X)\n#endif\n", 1);
	} else {
		printf ("can't find a suitable definition for UCS-4 encoded string literals.\n");
	}

	sbuffree (&sb);
	fflush (stdout);
}


static void aci_check_c11_atomics (void)
{
	int has_atomic_size_type, has_atomic_load_explicit, has_atomic_store_explicit;
	int has_atomic_exchange_explicit, has_cas_explicit;
	int has_add_explicit, has_sub_explicit;

	has_atomic_size_type = ac_check_compile ("Has native support for atomic_size_t",
	      "#include <stdatomic.h>\natomic_size_t x = ATOMIC_INIT_VAR(0);\n", NULL, "ATOMIC_SIZE_T");

	has_atomic_load_explicit = ac_check_compile ("Has atomic_load_explicit",
	      "#include <stdatomic.h>\n"
		  "atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "size_t foo (void) { return atomic_load_explicit (&x, memory_order_acquire); }\n",
		  NULL, "ATOMIC_LOAD_EXPLICIT");

	has_atomic_store_explicit = ac_check_compile ("Has atomic_store_explicit",
	      "#include <stdatomic.h>\n"
		  "atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "void foo (size_t v) { atomic_store_explicit (&x, v, memory_order_release); }\n",
		  NULL, "ATOMIC_STORE_EXPLICIT");

	has_atomic_exchange_explicit = ac_check_compile ("Has atomic_exchange_explicit",
	      "#include <stdatomic.h>\n"
		  "atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "size_t foo (size_t v) { return atomic_exchange_explicit (&x, v, memory_order_acq_rel); }\n",
		  NULL, "ATOMIC_EXCHANGE_EXPLICIT");

	has_cas_explicit = ac_check_compile ("Has atomic_compare_exchange_weak_explicit",
	      "#include <stdatomic.h>\n"
		  "atomic_int = ATOMIC_INIT_VAR(0);\n"
		  "int foo (size_t v) { \n"
		  "   return atomic_compare_exchange_weak_explicit (&x, 1, v, memory_order_acq_rel, memory_order_relaxed);\n"
		  "}\n",
		  NULL, "ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT");

	has_add_explicit = ac_check_compile ("Has atomic_fetch_add_explicit",
	      "#include <stdatomic.h>\n"
		  "atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "size_t foo (size_t v) { \n"
		  "   return atomic_fetch_add_explicit (&x, v, memory_order_relaxed);\n"
		  "}\n",
		  NULL, "ATOMIC_FETCH_SUB_EXPLICIT");

	has_sub_explicit = ac_check_compile ("Has atomic_fetch_sub_explicit",
	      "#include <stdatomic.h>\n"
		  "atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "size_t foo (size_t v) { \n"
		  "   return atomic_fetch_sub_explicit (&x, v, memory_order_acq_rel);\n"
		  "}\n",
		  NULL, "ATOMIC_FETCH_SUB_EXPLICIT");


	has_atomic_load_explicit = ac_check_compile ("Has std::atomic_load_explicit",
	      "#include <atomic>\n"
		  "std::atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "size_t foo (void) { return std::atomic_load_explicit (&x, std::memory_order_acquire); }\n",
		  NULL, "CXX_ATOMIC_LOAD_EXPLICIT");

	has_atomic_store_explicit = ac_check_compile ("Has std::atomic_store_explicit",
	      "#include <atomic>\n"
		  "std::atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "void foo (size_t v) { std::atomic_store_explicit (&x, v, std::memory_order_release); }\n",
		  NULL, "CXX_ATOMIC_STORE_EXPLICIT");

	has_atomic_exchange_explicit = ac_check_compile ("Has std::atomic_exchange_explicit",
	      "#include <atomic>\n"
		  "std::atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "size_t foo (size_t v) { return std::atomic_exchange_explicit (&x, v, std::memory_order_acq_rel); }\n",
		  NULL, "CXX_ATOMIC_EXCHANGE_EXPLICIT");

	has_cas_explicit = ac_check_compile ("Has std::atomic_compare_exchange_weak_explicit",
	      "#include <atomic>\n"
		  "std::atomic_int x = ATOMIC_INIT_VAR(0);\n"
		  "int foo (size_t v) { \n"
		  "   return std::atomic_compare_exchange_weak_explicit (&x, 1, v, std::memory_order_acq_rel, std::memory_order_relaxed);\n"
		  "}\n",
		  NULL, "CXX_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT");

	has_add_explicit = ac_check_compile ("Has std::atomic_fetch_add_explicit",
	      "#include <atomic>\n"
		  "std::atomic_int x = 0;\n"
		  "size_t foo (size_t v) { \n"
		  "   return std::atomic_fetch_add_explicit (&x, v, std::memory_order_relaxed);\n"
		  "}\n",
		  NULL, "CXX_ATOMIC_FETCH_ADD_EXPLICIT");

	has_sub_explicit = ac_check_compile ("Has std::atomic_fetch_sub_explicit",
	      "#include <atomic>\n"
		  "std::atomic_size_t x = 0;\n"
		  "size_t foo (size_t v) { \n"
		  "   return std::atomic_fetch_sub_explicit (&x, v, std::memory_order_acq_rel);\n"
		  "}\n",
		  NULL, "CXX_ATOMIC_FETCH_SUB_EXPLICIT");

}


// Check for a bunch of features in one go.
static void aci_check_misc_once (void)
{
	int has_func_name, has_att;
	sbuf_t sb;

	sbufinit (&sb);

	/* Enable GNU __attribute__ syntax. */
	has_att = ac_check_compile ("Has native support for __attribute__(()) syntax",
	    "__attribute__((unused)) void foo(void);\n", NULL, "GCC_ATTRIBUTE");

	aci_check_thread_local ();
	aci_check_align_keyword ();
	aci_check_stdbool ();
	aci_check_restrict_keyword ();
	aci_check_va_copy ();
	aci_check_variadic_macros ();
	aci_check_flexible_array_member ();
	aci_check_some_inttypes ();
	if (!aci_need_cxx_check) {
		aci_check_mixed_code_vars ();
	}
	aci_check_ssize ();
	aci_check_char32 ();
// Not mature enough, yet.
// aci_check_c11_atomics ();
	aci_check_builtin_overflow();

	aci_flag_list_add (&aci_flags_root, "WOE32", "Are we running MS-Windows", aci_have_woe32);

	if (aci_have_woe32) {
		if (aci_have_function_proto ("sys/cygwin.h", NULL, "cygwin_conv_to_win32_path")) {
			aci_flag_list_add (&aci_flags_root, "CYGWIN", "Are we running under Cygwin", 1);
			aci_have_cygwin = 1;
		}
	}

	if (aci_check_define (NULL, NULL, "__MINGW32__")) {
		 printf ("detected mingw\n");
		 fflush (stdout);
		 ac_check_compiler_flag ("-posix", "GCC_POSIX");
	}



	if (aci_have_woe32) {
		ac_set_var ("EXE", ".exe");
		if (!has_att) {
			aci_attsyn[0] = "__declspec(%s)";
			has_att = 1;
		}
	} else {
		ac_set_var ("EXE", "");
	}


	if (!has_att) {
		if (aci_have_woe32) {
			ac_add_code ("#define __attribute__(x) __declspec x\n", 1);
		} else {
			ac_add_code ("#define __attribute__(x)\n", 1);
		}
	}

	if (has_att) {
		if (aci_have_woe32) {
			ac_check_func_attribute ("dllexport", "EXPORT", 1, 0, att_both);
			ac_check_var_attribute ("dllimport", "IMPORT", att_both);
		} else if (ac_check_func_attribute ("__visibility__(\"default\")", "EXPORT", 1, 1, att_both)) {
			sbufformat (&sb, 1, "#define %s%sIMPORT %s%sEXPORT",
			            aci_macro_prefix, aci_attrib_pfx, aci_macro_prefix,
			            aci_attrib_pfx);
			ac_add_code (sbufchars (&sb), 1);
		} else {
			ac_check_func_attribute ("visibility(\"default\")", "EXPORT", 1, 1, att_both);
			sbufformat (&sb, 1, "#define %s%sIMPORT %s%sEXPORT",
			            aci_macro_prefix, aci_attrib_pfx, aci_macro_prefix,
			            aci_attrib_pfx);
			ac_add_code (sbufchars (&sb), 1);
		}
	} else if (aci_can_compile ("__global int foo(int x) { return 2 * x; }\n", NULL)) {
		sbufformat (&sb, 1, "#define %s%sEXPORT __global\n", aci_macro_prefix,
		        aci_attrib_pfx);
		sbufformat (&sb, 0, "#define %s%sIMPORT __global\n", aci_macro_prefix,
		        aci_attrib_pfx);
		ac_add_code (sbufchars (&sb), 1);
	} else {
		sbufformat (&sb, 1, "#define %s%sEXPORT", aci_macro_prefix,
		        aci_attrib_pfx);
		sbufformat (&sb, 0, "#define %s%sIMPORT", aci_macro_prefix,
		        aci_attrib_pfx);
		ac_add_code (sbufchars (&sb), 1);
	}
	sbufformat (&sb, 1, "#define EXPORTFN %s%sEXPORT", aci_macro_prefix, aci_attrib_pfx);
	ac_add_code (sbufchars (&sb), 1);

	if (aci_have_woe32) {
		sbufformat (&sb, 1, "#define %s%sHIDDEN", aci_macro_prefix,
		        aci_attrib_pfx);
		ac_add_code (sbufchars (&sb), 1);
	} else if (has_att) {
		if (!ac_check_func_attribute ("__visibility__(\"hidden\")", "HIDDEN", 1, 1, att_both)) {
			ac_check_func_attribute ("visibility(\"hidden\")", "HIDDEN", 1, 1, att_both);
		}
	} else if (aci_can_compile ("__hidden int foo(int x) { return 2 * x; }\n", NULL)) {
		sbufformat (&sb, 1, "#define %s%sHIDDEN __hidden", aci_macro_prefix,
		        aci_attrib_pfx);
		ac_add_code (sbufchars (&sb), 1);
	} else {
		sbufformat (&sb, 1, "#define %s%sHIDDEN", aci_macro_prefix,
		        aci_attrib_pfx);
		ac_add_code (sbufchars (&sb), 1);
	}

	ac_check_func_attribute ("deprecated", "DEPRECATED", 0, 0, att_both);
	ac_check_func_attribute ("warn_unused_result", "WARN_UNUSED_RESULT", 0, 0, att_both);
	ac_check_func_attribute ("nodiscard", "NODISCARD", 0, 0, att_cxx11);
	ac_check_func_attribute ("unused", "UNUSED", 0, 0, att_both);
	ac_check_func_attribute ("maybe_unused", "MAYBE_UNUSED", 0, 0, att_cxx11);


	aci_check_att_format ();

	has_func_name = ac_check_compile("Has the C99 __func__ identifier",
	            "void show(const char *s);\n"
				"void foo(void) {   const char *me = __func__;  show(me); }\n",
				NULL, "C99_FUNCNAME");

	if (!has_func_name) {
		has_func_name = ac_check_compile("Has the __FUNC__ identifier",
		            "void show(const char *s);\n"
					"void foo(void) { const char *me = __FUNC__; show(me); }\n",
					NULL, "UPPER_CASE_FUNC");
		if (has_func_name) {
			ac_add_code("#define __func__ __FUNC__", 1);
		}
	}

	if (!has_func_name) {
		aci_strlist_add_unique (&aci_tdefs,
		    "\n/* Ensure that the __func__ syntax is available. */\n"
			"#ifndef __func__\n#define __func__ \"unknown\"\n#endif", 0);
	}

	aci_check_inline_keyword ();
	check_inline_assembly ();
	aci_check_commands ();

	if (aci_can_compile ("_Pragma (\"GCC visibility push(hidden)\")\nint x;\n", NULL)) {
		ac_add_code ("#define GCC_VISIBILITY_PUSH_HIDDEN _Pragma (\"GCC visibility push(hidden)\")", 1);
	} else {
		ac_add_code ("#define GCC_VISIBILITY_PUSH_HIDDEN", 1);
	}
	if (aci_can_compile ("_Pragma (\"GCC visibility push(default)\")\nint x;\n", NULL)) {
		ac_add_code ("#define GCC_VISIBILITY_PUSH_DEFAULT _Pragma (\"GCC visibility push(default)\")", 1);
	} else {
		ac_add_code ("#define GCC_VISIBILITY_PUSH_DEFAULT", 1);
	}
	if (aci_can_compile ("#pragma GCC visibility push(default)\n"
						 "int x;\n"
						 "_Pragma (\"GCC visibility pop\")\n", NULL)) {
		ac_add_code ("#define GCC_VISIBILITY_POP _Pragma (\"GCC visibility pop\")", 1);
	} else {
		ac_add_code ("#define GCC_VISIBILITY_POP", 1);
	}

	if (!ac_check_compile ("Has __COUNTER__ macro", "int x[__COUNTER__ + 2];\n",
	                       NULL, "COUNTER_MACRO")) {
		ac_add_code ("#define __COUNTER__ __LINE__", 1);
	}

	if (!ac_check_link ("Has __builtin_expect()",
	                       "int main(int argc, char**argv) { \n"
						   "   if (__builtin_expect(argc,1) == 1) {\n"
						   "      return 2;\n"
						   "   }\n"
						   "   return 0;\n"
						   "}\n",
						   NULL, "", "BUILTIN_EXPECT")) {
		ac_add_code ("#define __builtin_expect(a,b) a", 1);
	}


	if (aci_need_cxx_check) {
		aci_check_cxx ();
	}

	sbuffree (&sb);
}




// Include the files listed in "includes" and compile with the compilation
// flags "cflags". Are the typedefed types "t1" and "t2" the same types in
// C++? If yes then define "tag" in the configuration file.
void ac_check_same_cxx_types (const char *includes, const char *cflags,
        const char *t1, const char *t2, const char *tag)
{
	sbuf_t sb, source;

	sbufinit (&sb);
	sbufinit (&source);

	sbufformat (&sb, 1, "Are \"%s\" and \"%s\" the same type in headers [stdint.h %s] %s ",
	            t1, t2, includes, cflags ? cflags : "");

	aci_add_headers (&source, includes);
	sbufcat (&source, sbufchars (&stdint_proxy));
	sbufcat (&source,
	    "template <class T1, class T2> struct Equal { enum { yes = 0 }; };\n"
		"template <class T> struct Equal<T,T> { enum { yes = 1 }; };\n"
		"typedef int foo[Equal<");
	sbufformat (&source, 0, "%s, %s>::yes ? 1 : -1];\n", t1, t2);

	ac_check_compile (sbufchars (&sb), sbufchars (&source), cflags, tag);

	sbuffree (&sb);
	sbuffree (&source);
}


// Do we have a working SFINAE?
static void aci_check_sfinae (void)
{
	/* Example taken from Alexandrescu's Modern C++ design. */
	ac_check_compile_fail ("Has buggy SFINAE",
	    "template <class T, class U>\n"
		"class Conversion {\n"
		"   typedef char Small;\n"
		"   class Big { char dummy[2]; };\n"
		"   static Small Test(U);\n"
		"   static Big Test(...);\n"
		"   static T MakeT();\n"
		"public:\n"
		"   enum { exists = sizeof(Test(MakeT())) == sizeof(Small) };\n"
		"};\n\n"
		"int main() { return Conversion<int, double>::exists;  }\n",
		NULL, "BUGGY_SFINAE");
}


// Is std::numeric_limits<> specialized for int64_t and uint64_t?
static void aci_specialize_numeric_limits (void)
{
	static const char src1[] =
			"#include <limits>\n"
			"enum { val = std::numeric_limits<";
	static const char src2[] = ">::is_specialized };\n"
			"int foo[val ? 1 : -1];\n";
	sbuf_t source;

	if (!aci_have_int64) {
		return;
	}

	sbufinit (&source);

	if (aci_have_stdint) {
		sbufformat (&source, 1, "#include <stdint.h>\n%sint_fast64_t%s",
		            src1, src2);
	} else if (aci_int64_type != NULL) {
		sbufformat (&source, 1, "%s%s%s", src1, aci_int64_type, src2);
	} else {
		sbuffree (&source);
		return;
	}

	if (aci_can_compile (sbufchars (&source), NULL)) {
		sbuffree (&source);
		return;
	}

	sbufcpy (&source, "\n/* Specialize numeric limits for 64-bit integers. */\n");
	sbufcat (&source, "#ifdef __cplusplus\n");
	sbufformat (&source, 0, "#ifndef PELCONF_CXX_NUMERIC_LIMITS_64_DEFINED\n"
							"    #define PELCONF_CXX_NUMERIC_LIMITS_64_DEFINED\n");
	sbufcat (&source, "    #include <limits>\n");

	sbufcat (&source, "    template <> struct std::numeric_limits<signed ");
	sbufcat (&source, aci_int64_type);
	sbufcat (&source, "> {\n");
	sbufcat (&source, "       static const bool is_specialized = true;\n");
	sbufcat (&source, "       static signed ");
	sbufcat (&source, aci_int64_type);
	sbufcat (&source, " min()  { return -9223372036854775808; }\n");
	sbufcat (&source, "       static signed ");
	sbufcat (&source, aci_int64_type);
	sbufcat (&source, " max()  { return 9223372036854775807; }\n");

	sbufcat (&source, "       static const int digits = 64;\n");
	sbufcat (&source, "       static const int digits10 = 20;\n");
	sbufcat (&source, "       static const bool is_signed = false;\n");
	sbufcat (&source, "       static const bool is_integer = true;\n");
	sbufcat (&source, "       static const bool is_exact = true;\n");
	sbufcat (&source, "       static const int radix = 2;\n");
	sbufcat (&source, "   };\n");

	aci_strlist_add_unique (&aci_tdefs, sbufchars (&source), 0);

	sbufcpy (&source, "    template <> struct std::numeric_limits<unsigned ");
	sbufcat (&source, aci_int64_type);
	sbufcat (&source, "> {\n");
	sbufcat (&source, "       static const bool is_specialized = true;\n");
	sbufcat (&source, "       static unsigned ");
	sbufcat (&source, aci_int64_type);
	sbufcat (&source, " min() { return 0; }\n");
	sbufcat (&source, "       static unsigned ");
	sbufcat (&source, aci_int64_type);
	sbufcat (&source, " max() { return 18446744073709551615; }\n");

	sbufcat (&source, "       static const int digits = 64;\n");
	sbufcat (&source, "       static const int digits10 = 20;\n");
	sbufcat (&source, "       static const bool is_signed = true;\n");
	sbufcat (&source, "       static const bool is_integer = true;\n");
	sbufcat (&source, "       static const bool is_exact = true;\n");
	sbufcat (&source, "       static const int radix = 2;\n");
	sbufcat (&source, "   };\n");
	sbufcat (&source, "#endif\n");
	sbufcat (&source, "#endif\n");

	aci_strlist_add_unique (&aci_tdefs, sbufchars (&source), 0);

	sbuffree (&source);
}



// The standard specifies the two phase lookup for dependent base names. It
// may be necessary to qualify the names, but at least BCC32 does not allow
// using the correct syntax.

static void aci_check_buggy_using (void)
{
	const char src[] =
		"template <class T> struct Foo { int v; };\n"
		"template <class T> struct Bar : Foo<T> {\n"
		"   using Foo<T>::v;\n"
		"   void set() { v = 42; }\n"
		"};\n"
		"int main() { Bar<float> bf; bf.set(); return 0; }\n";

	ac_check_compile_fail ("Has bug when using correct syntax for template dependent bases",
	                 src, NULL, "BUGGY_DEP_BASE");
}



static void aci_check_strong_using (void)
{
	sbuf_t sb;

	sbufinit (&sb);

	sbufformat (&sb, 1, "#define %sCXX_INLINE_NAMESPACE(N) ", aci_macro_prefix);

	// Note: clang++ 3.1 chokes when reopening an inline namespace without
	// the inline keyword. The C++11 standard says the inline keyword is
	// optional when reopening the inline namespace (C++11, 7.3.1, par. 7)

	if (ac_check_compile ("Has C++11 inline namespace",
	        "namespace enclosing {\n"
			"    inline namespace inner { int x; }\n"
			"    namespace inner { int y; }\n"
			"}\n",
			NULL, "CXX_INLINE_NS")) {
		sbufcat (&sb, "inline namespace N {}");
	} else if (ac_check_compile ("Has GCC's strong using namespace",
	            "namespace enclosing { namespace inner {}\n"
				"using namespace inner __attribute__((__strong__));}\n",
				NULL, "STRONG_ALIAS")) {
		sbufcat (&sb, "namespace N {}\\\n"
				"    using namespace N __attribute__((__strong__));");
	} else {
		sbufcat (&sb, "namespace N {} using namespace N;");
	}

	aci_strlist_add_unique (&aci_tdefs, sbufchars (&sb), 0);

	sbuffree (&sb);
}

static void aci_check_cv_overload (void)
{
	const char src[] =
		"template <class T> struct remove_cv { typedef T type; };\n"
		"template <class T> struct remove_cv<const T>   { typedef T type; };\n"
		"template <class T> struct remove_cv<const T&>  { typedef T &type; };\n"
		"template <class T> struct remove_cv<volatile T>   { typedef T type; };\n"
		"template <class T> struct remove_cv<volatile T&>  { typedef T &type; };\n"
		"template <class T> struct remove_cv<const volatile T>   { typedef T type; };\n"
		"template <class T> struct remove_cv<const volatile T&>  { typedef T &type; };\n";

	ac_check_compile_fail ("Has bug with cv-qualified templates",
	            src, NULL, "BUGGY_CV_TEMPLATE");
}


static int has_gcc_typeof = 0;

static void aci_check_decltype (void)
{
	sbuf_t sb;
	int has_dt;

	sbufinit (&sb);

	has_dt = ac_check_compile ("Has the C++11 decltype keyword",
	        "double f();   decltype(f()) x;\n",
	        NULL, "CXX_DECLTYPE_NATIVE");

	has_gcc_typeof = ac_check_compile ("Has the GCC typeof extension",
	                  "double f();   __typeof__(f()) x;\n",
	                  NULL, "GCC_TYPEOF");

	if (!has_dt && has_gcc_typeof) {
		aci_strlist_add_unique (&aci_tdefs,
		          "\n#ifndef HAVE_CXX_DECLTYPE_NATIVE\n"
				"#ifndef decltype\n"
				"#define decltype __typeof__\n"
				"#endif\n"
				"#endif\n", 0);
	}

	aci_flag_list_add (&aci_flags_root, "DECLTYPE_MACRO", "Can use decltype",
	                  has_dt || has_gcc_typeof);

	// Define the macro name from N3694.
	ac_add_code ("#if defined(HAVE_DECLTYPE_MACRO) && !defined(__cpp_decltype)\n"
			 "  #define __cpp_decltype 200707\n"
			 "#endif\n", 1);

	sbuffree (&sb);
}


static void aci_check_auto (void)
{
	ac_check_compile ("Has the C++11 auto keyword",
	                  "int foo() { return 42; }\n"
				"int bar() { auto v = foo(); return v; }\n",
				NULL, "CXX_AUTO");

	ac_add_code ("#ifdef HAVE_CXX_AUTO\n"
			 "  #define CXX_AUTO(V,...) auto V = __VA_ARGS__\n"
			 "#elif defined(HAVE_GCC_TYPEOF)\n"
			 "  #define CXX_AUTO(V,...) __typeof__(__VA_ARGS__) V = __VA_ARGS__\n"
			 "#elif defined(HAVE_CXX_DECLTYPE_NATIVE)\n"
			 "  #define CXX_AUTO(V,...) decltype(__VA_ARGS__) V = __VA_ARGS__\n"
			 "#else\n"
			 "  #error Can not find a suitable implementation for CXX_AUTO\n"
			 "#endif\n", 1);
}


static void aci_check_abi_tag (void)
{
	int b = ac_check_compile ("Has the gnu::abi_tag attribute",
	            "struct __attribute__((abi_tag(\"foo\"))) Foo { int x; };\n",
	            NULL, "GCC_ABI_TAG");

	if (b) {
		ac_add_code ("#define GCCA_ABITAG(...) __attribute__((abi_tag(__VA_ARGS__)))", 1);
	} else {
		ac_add_code ("#define GCCA_ABITAG(...)", 1);
	}
}


// Check if we can use 64-bit arithmetic as template parameters.
static void aci_check_intmax_template_param (void)
{
	if (aci_have_int64) {
		sbuf_t sb;

		sbufinit (&sb);
		sbufcpy (&sb, "typedef ");
		sbufcat (&sb, aci_int64_type);
		sbufcat (&sb, " foo_t;\n"
			"template <foo_t N> struct Foo { static const foo_t value = N; };\n"
			"foo_t instantiate() { return Foo<42>::value; }\n");
		ac_check_compile ("Has template arithmetic on int64_t",
		            sbufchars (&sb), NULL, "TEMPLATE_ARITHMETIC_64");
		sbuffree (&sb);
	}
}


// Check for C++11 explicit extern template instantiation control.
static void aci_check_extern_templ_inst (void)
{
	ac_check_compile ("Has C++11 extern template explicit instantiation",
	        "template <class T> struct kk { void foo(); };\n"
			"extern template class kk<int>;\n", NULL,
			"CXX_EXTERN_TEMPLATE_INST");
}


static void aci_check_rvalue_refs (void)
{
	ac_check_compile ("Has C++11 rvalue references",
	        "void foo(int &&);\n", NULL, "CXX_RVALUE_REFS");

	// Define the macro name from N3694.
	ac_add_code ("#if defined(HAVE_CXX_RVALUE_REFS) && !defined(__cpp_rvalue_reference)\n"
			 "  #define __cpp_rvalue_reference 200610\n"
			 "#endif\n", 1);
}


static void aci_check_variadic_templates (void)
{
	ac_check_compile ("Has C++11 variadic templates",
	        "template <class T1, class ... Args> void print (const T1 &t, Args ... args);\n",
	        NULL, "CXX_VARIADIC_TEMPLATES");

	ac_add_code ("#if defined(HAVE_CXX_VARIADIC_TEMPLATES) && !defined(__cpp_variadic_templates)\n"
			 "  #define __cpp_variadic_templates 200704\n"
			 "#endif\n", 1);
}


static void aci_check_override (void)
{
	ac_check_compile ("Has C++11 override",
	        "struct Base { virtual void foo (float); };\n"
			"struct Derived : Base { virtual void foo (float) override; };\n",
			NULL, "CXX_OVERRIDE");

	ac_add_code ("#ifdef HAVE_CXX_OVERRIDE\n"
			 "  #define CXX_OVERRIDE override\n"
			 "#else\n"
			 "  #define CXX_OVERRIDE\n"
			 "#endif\n", 1);
}

static void aci_check_final (void)
{
	ac_check_compile ("Has C++11 final", "struct Base final {};\n",
	                              NULL, "CXX_FINAL");

	ac_add_code ("#ifdef HAVE_CXX_FINAL\n"
			 "  #define CXX_FINAL final\n"
			 "#else\n"
			 "  #define CXX_FINAL\n"
			 "#endif\n", 1);
}


static void aci_check_constexpr (void)
{
	ac_check_compile ("Has C++11 constexpr",
	                              "struct Foo { int x;  constexpr Foo (int i) : x(i) {} };\n",
	                              NULL, "CXX_CONSTEXPR");
	ac_add_code ("#ifndef HAVE_CXX_CONSTEXPR\n"
			 "  #define constexpr inline\n"
			 "#elif !defined(__cpp_constexpr)\n"
			 "  #define __cpp_constexpr 200704\n"
			 "#endif\n", 1);
}


// Perform all C++ specific checks.
static void aci_check_cxx (void)
{
	aci_check_sfinae ();                /* Required to support enable_if */
	aci_check_buggy_using ();           /* Two phase look up. */
	aci_check_cv_overload ();
	aci_check_strong_using ();          /* G++/C++11 extension. */
	aci_check_decltype ();              /* G++/C++11 extension. */
	aci_specialize_numeric_limits ();
	aci_check_intmax_template_param ();
	aci_check_extern_templ_inst ();
	aci_check_rvalue_refs ();
	aci_check_variadic_templates ();
	aci_check_override ();
	aci_check_final ();
	aci_check_constexpr();
	aci_check_auto();
	aci_check_abi_tag();
//  ac_check_each_header_sequence("type_traits chrono tuple system_error ratio atomic thread", "");
}



// Add func_name.o to the list of object files that must be compiled. This
// list is available as the value of the makefile variable LIBOBJS. It will
// usually be the list of files which provide replacements for missing
// functions.
void ac_libobj (const char *func_name)
{
	sbuf_t sb;

	sbufinit (&sb);
	sbufcpy (&sb, func_name);
	sbufcat (&sb, "$(OBJ)");
	ac_add_var_append ("LIBOBJS", sbufchars (&sb));
	ac_set_var ("LIBOBJSINCLUDE", "-I.");
	sbuffree (&sb);
}



// After including the files listed in "includes" and compiling with the
// compilation flags "cflags" check for the presence of each of the
// functions listed in "funcs". If the function is not available the add the
// object file "function_name.o" to the makefile variable LIBOBJS.
void ac_replace_funcs (const char *includes, const char *cflags, const char *funcs)
{
	const char *sow = funcs;
	const char *eow;
	sbuf_t sb;

	sbufinit (&sb);

	while (*sow) {
		while (*sow && *sow != '_' && !isalnum (*sow)) ++sow;
		eow = sow;
		while (*eow && (*eow == '_' || isalnum (*eow))) eow++;

		if (sow != eow) {
			sbufncpy (&sb, sow, eow - sow);
			if (!ac_check_proto (includes, cflags, sbufchars (&sb))) {
				ac_libobj (sbufchars (&sb));
			}
		}
		sow = eow;
	}

	sbuffree (&sb);
}


// Check for the presence of each function in "funcs". Use it after
// ac_check_each_header_sequence().
void ac_check_each_func (const char *funcs, const char *cflags)
{
	const char *sow = funcs;
	const char *eow;
	sbuf_t sb, src;
	int result;
	char tag[FILENAME_MAX];

	sbufinit (&sb);
	sbufinit (&src);

	while (*sow) {
		while (*sow && *sow != '_' && !isalnum (*sow)) ++sow;
		eow = sow;
		while (*eow && (*eow == '_' || isalnum (*eow))) eow++;

		if (sow != eow) {
			sbufncpy (&sb, sow, eow - sow);
			sbufcpy (&src, sbufchars(&aci_common_headers));
			sbufformat (&src, 0,
			    "int main() {\n"
				"    typedef void (*pvfn)(void);\n"
				"    pvfn p = (pvfn) %s;\n"
				"    return p != 0;\n"
				"}\n", sbufchars(&sb));

			result = aci_can_compile (sbufchars (&src), cflags);

			sbufformat (&src, 1, "Has prototype of %s", sbufchars(&sb));
			aci_identcopy (tag, sizeof tag, sbufchars(&sb));
			aci_flag_list_add (&aci_flags_root, tag, sbufchars (&src), result);
			printf ("%s: %s\n", sbufchars (&src), aci_noyes[result]);
			fflush (stdout);
		}
		sow = eow;
	}

	sbuffree (&sb);
	sbuffree (&src);
}



// See if the boolean option "opt" has been given and remove it from argc,
// argv.
static int aci_has_option (int *argc, char **argv, const char *opt)
{
	int i;
	int found = 0;

	for (i = 1; i < *argc; ++i) {
		char *cp = argv[i];
		while (*cp == '-') ++cp;
		if (strcmp (cp, opt) == 0) {
			int j;
			found = 1;
			for (j = i + 1; j < *argc; ++j) {
				argv[j - 1] = argv[j];
			}
			--*argc;
			--i;
			/* Keep searching to discard duplicates. */
		}
	}
	return found;
}


// See if the option "opt" with an argument has been given. Remove it form
// argc, argv and return the argument.
static const char * aci_has_optval (int *argc, char **argv, const char *opt)
{
	int i;
	const char *result = NULL;
	size_t optlen = strlen (opt);

	for (i = 1; i < *argc; ++i) {
		char *cp = argv[i];
		while (*cp == '-') ++cp;
		if (strncmp (cp, opt, optlen) == 0) {
			int j;

			if (cp[optlen] == '=') {
				result = cp + optlen + 1;

				for (j = i + 1; j < *argc; ++j) {
					argv[j - 1] = argv[j];
				}
				--*argc;
				break;
			} else if (cp[optlen] == 0 && i + 1 < *argc) {
				result = argv[i + 1];
				for (j = i + 2; j < *argc; ++j) {
					argv[j - 2] = argv[j];
				}
				*argc -= 2;
				break;
			}
		}
	}
	return result;
}


static aci_strlist_t aci_given_options;
static aci_strlist_t aci_valid_options, aci_valid_options_desc;
static int aci_valid_opts_inited = 0;


void ac_show_help (void)
{
	char **beg, **end;
	char **begdesc;

	beg = aci_strlist_begin (&aci_valid_options);
	end = aci_strlist_end (&aci_valid_options);

	begdesc = aci_strlist_begin (&aci_valid_options_desc);

	if (beg != end) {
		printf ("The following features are allowed: \n");
		while (beg != end) {
			printf ("%s, %s\n", *beg, *begdesc);
			++beg;
			++begdesc;
		}
	} else {
		printf ("No additional options are allowed\n");
	}
	fflush (stdout);
}



// Add the description of one option.
void ac_add_option_info (const char *opt, const char *desc)
{
	sbuf_t sb;

	sbufinit (&sb);

	if (!aci_valid_opts_inited) {
		aci_valid_opts_inited = 1;
		aci_strlist_init (&aci_valid_options);
		aci_strlist_init (&aci_valid_options_desc);
	}

	aci_identcat (&sb, opt);
	aci_strlist_add (&aci_valid_options, sbufchars (&sb), 0);
	aci_strlist_add (&aci_valid_options_desc, desc, 0);

	sbuffree (&sb);
}


// Add the options passed in the command line to the makefile features
// variables.
void aci_add_cmd_vars(int argc, char **argv)
{
	int i;
	const char *p, *sow;
	sbuf_t sb;

	sbufinit (&sb);

	for (i = 1; i < argc; ++i) {
		p = argv[i];
		if (p[0] == '-') {
			if (p[1] == '-') {
				p += 2;
			} else {
				p++;
			}
		}
		sow = p;
		while (*p && *p != '=') ++p;
		sbufncpy (&sb, sow, p - sow);
		aci_make_identifier (sbufchars (&sb));
		aci_strlist_add_unique (&aci_given_options, sbufchars (&sb), 0);

		if (*p == 0) {
			aci_varlist_set (&aci_features, sbufchars (&sb), "1");
			aci_varlist_set (&aci_makevars, sbufchars (&sb), "1");
		} else {
			aci_varlist_set (&aci_features, sbufchars (&sb), p + 1);
			aci_varlist_set (&aci_makevars, sbufchars (&sb), p + 1);
		}
	}

	sbuffree (&sb);
}


// Check if the option "name" was given and put its value in dest.
ptrdiff_t ac_have_feature (const char *name, char *dest, size_t dest_size)
{
	aci_varnode_t *vn;
	ptrdiff_t result = -1;
	sbuf_t feature;
	sbuf_t value;

	sbufinit (&feature);
	sbufinit (&value);

	aci_identcat (&feature, name);
	vn = aci_varlist_find (&aci_features, sbufchars (&feature));
	if (vn) {
		size_t needed;
		char **beg = aci_strlist_begin (&vn->chunks);
		char **end = aci_strlist_end (&vn->chunks);

		while (beg != end) {
			sbufcat (&value, *beg);
			sbufcat (&value, " ");
			++beg;
		}
		while (sbufchars (&value)[sbuflen (&value) - 1] == ' ') {
			sbuftrunc (&value, sbuflen (&value) - 1);
		}

		needed = sbuflen (&value);
		if (dest != NULL && dest_size != 0) {
			if (needed >= dest_size) {
				memcpy (dest, sbufchars (&value), dest_size - 1);
				dest[dest_size - 1] = 0;
			} else {
				memcpy (dest, sbufchars (&value), needed + 1);
			}
		}
		result = needed;
	}
	sbuffree (&feature);
	sbuffree (&value);
	return result;
}


// We are finished. Clean up the temporary files.
static void aci_cleanup (void)
{
	sbuf_t sb;

	/* Created by aci_run_silent() */
	remove ("__temp1__");
	remove ("__temp2__");

	/* Remove the source file */
	sbufinit (&sb);
	sbufcpy (&sb, "_test_");
	sbufcat (&sb, aci_source_extension);
	remove (sbufchars (&sb));
	sbuffree (&sb);

	/* Remove the possible binary files created while compiling. */
	remove ("a.out");
	remove ("a.exe");
	remove ("__kkkk1");
	remove ("__kkkk2");
}



/* Command line option names. */

static const char aci_dos_name[] = "dos";
static const char aci_verbose_name[] = "verbose";
static const char aci_namespace_name[] = "ns";
static const char aci_cc_name[] = "cc";
static const char aci_keep_name[] = "keep";
static const char aci_makevars_name[] = "makevars";
static const char aci_stdver[] = "stdver";
static const char aci_nostdver[] = "nostdver";
static const char aci_simple_name[] = "simple";
static const char aci_static_name[] = "static";
static int aci_use_stdver = 0;



void aci_usage (const char *progname)
{
	printf ("usage is %s [options] features\n",
	        progname);

	printf ("The available options are:\n");
	printf ("--%s will use the Windows convention of .LIB for libraries. Default is .a and -l<lib>\n", aci_dos_name);
	printf ("--%s will output verbose information about each test.\n", aci_verbose_name);
	printf ("--%s=pfx will add the prefix pfx to all the defines.\n", aci_namespace_name);
	printf ("--%s=comp selects the compilation command.\n", aci_cc_name);
	printf ("--%s will keep the intermediate files\n", aci_keep_name);
	printf ("--%s=name will force using name as the makevars file\n", aci_makevars_name);
	printf ("--%s will check for GCC's -std=gnu99 or gnu++11 (gnu++0x) options\n", aci_stdver);
	printf ("--%s will select the default version of the language as provided by the compiler\n", aci_nostdver);
	printf ("--%s will choose simple command line options for GCC which are not likely to be buggy\n", aci_simple_name);
	printf ("--%s will use static linking when probing.\n", aci_static_name); 
	printf ("--prefix=name will use the given prefix for the generation of INSTALL_INCLUDE and INSTALL_LIB make variables\n");
	printf ("--with-extra-includes <name> will use the given additional include directories\n");
	printf ("--with-extra-libs <name> will use the given additional library directories\n");
	printf ("--extra-cflags <flags> will use the additional CFLAGS\n");
	printf ("--extra-ldflags <flags> will use the additional LDFLAGS\n");
	printf ("--abiname=<name> will ensure that the libraries are created with <name> used as a suffix.\n");
	printf ("   This allows you to create different versions of the library ABI which can coexist.\n");
	printf ("Each feature is give as --feature_name=value\n");
	printf ("The double hyphen may be omitted or replaced with a single hyphen.\n");
}


/* Get the value of a macro defined by make. */
static int aci_get_make_var (const char *varname, char *s, size_t n)
{
	const char dummy_mk[] = "__dummy.mk";
	const char dummy_txt[] = "__dummy.txt";
	size_t nread;
	FILE *fr, *mkf;
	int result;
	sbuf_t cmd;

	mkf = fopen (dummy_mk, "w");
	if (mkf == NULL) {
		return -1;
	}
	fprintf (mkf, "%s:\n\techo $(%s) >%s\n", dummy_txt, varname, dummy_txt);
	fclose (mkf);

	remove (dummy_txt);

	sbufinit (&cmd);
	sbufformat (&cmd, 1, "%s -f%s %s >__kkkk1", aci_make_cmd, dummy_mk, dummy_txt);
	system (sbufchars (&cmd));
	sbuffree (&cmd);

	fr = fopen (dummy_txt, "r");
	if (fr == NULL) {
		return -1;
	}

	result = -1;
	nread = fread (s, 1, n, fr);
	if (nread < n) {
		char *cp = s;
		char *lim = s + nread;
		while (cp < lim && *cp != '\n') ++cp;
		*cp = 0;
		if (cp != s) {
			result = 0;
		}
	}
	fclose (fr);
	remove (dummy_mk);
	remove (dummy_txt);

	if (strstr (s, "ECHO is") != 0) {
		result = -1;
	}
	return result;
}


// Check if the include form "form" actually works.
static int aci_has_include_form (const char *form)
{
	static const char dummy_inc[] = "__dummy.inc";
	static const char dummy_mk[] = "__dummy.mk";
	FILE *f;
	int result;
	char cmd[FILENAME_MAX];

	f = fopen (dummy_inc, "w");
	if (f == NULL) {
		return 0;
	}
	fprintf (f, "# just a dummy\n");
	fclose (f);

	f = fopen (dummy_mk, "w");
	if (f == NULL) {
		return 0;
	}
	fprintf (f, "%s %s\n", form, dummy_inc);
	fprintf (f, "all:\n\techo hello\n");
	fclose (f);

	sprintf (cmd, "%s -f%s", aci_make_cmd, dummy_mk);
	result = (aci_run_silent (cmd) == 0);

	remove (dummy_mk);
	remove (dummy_inc);

	return result;
}


// Find out how to include files within the makefile.
static const char * aci_get_include_form (void)
{
	static const char *variants[] = {
			"!include", /* Borland/MS variant */
			".include", /* BSD pmake */
			"include",  /* GNU make */
			".INCLUDE:" /* dmake */
	};
	/* This is just a comment in make, but due to the similarity with C's
	   preprocessor we may get away with it. */
	static const char crippled[] = "#include";
	size_t i;

	for (i = 0; i < sizeof(variants)/sizeof(variants[0]); ++i) {
		if (aci_has_include_form (variants[i])) {
			printf ("make includes files using '%s file'\n", variants[i]);
			if (i == 0) {
				aci_dos_make = 1;
			}
			return variants[i];
		}
	}

	printf ("make is not able to include files!\n");
	return crippled;
}


static int aci_try_alldeps (const char *s)
{
	char cmd[FILENAME_MAX];
	FILE *fw = fopen ("__dummy.mk", "w");
	if (fw == 0) {
		return -1;
	}

	fprintf (fw, "__dummy.1: __dummy.2 __dummy.3\n");
	fprintf (fw, "\t%s__dummy %s\n\n", aci_make_exe_prefix, s);
	fclose (fw);

	sprintf (cmd, "%s -f__dummy.mk __dummy.1", aci_make_cmd);
	return aci_run_silent (cmd);
}


// Figure out how the makefile denotes all the dependencies of a target.
static int aci_get_alldeps (char *alldeps)
{
	FILE *fw;
	const char * choices[] = { "$^", "$**", "$&" };
	size_t i;
	sbuf_t sb;

	sbufinit (&sb);

	/* Prepare the argument counting program */
	fw = fopen ("__dummy.c", "w");
	if (fw == 0) {
		return -1;
	}
	fprintf (fw, "int main(int argc, char **argv) { return argc == 2 ? 1 : 0; }\n");
	fclose (fw);

	sbufformat (&sb, 1, "%s %s", aci_compile_cmd, aci_exe_cmd);
	if (sbufchars (&sb)[sbuflen(&sb) - 1] == '@' &&
	    sbufchars (&sb)[sbuflen(&sb) - 2] == '$') {
		sbuftrunc (&sb, sbuflen (&sb) - 2);
	}
	sbufformat (&sb, 0, "%s", "__dummy __dummy.c");
	if (aci_run_silent (sbufchars (&sb)) != 0) {
		printf ("could not create testing file for alldeps\n");
	}


	/* Prepare the prerequisites */
	fw = fopen ("__dummy.2", "w");
	fclose (fw);
	fw = fopen ("__dummy.3", "w");
	fclose (fw);

	sbuffree (&sb);

	for (i = 0; i < sizeof(choices)/sizeof(choices[0]); ++i) {
		if (aci_try_alldeps (choices[i]) == 0) {
			strcpy (alldeps, choices[i]);
			return 0;
		}
	}

	return -1;
}



static int aci_try_wall (const char *cc, const char *src, const char *proposed)
{
	char cmd[FILENAME_MAX];

	sprintf (cmd, "%s -c %s %s", cc, proposed, src);
	return aci_run_silent (cmd);
}


// Figure out how to enable all warnings in the compiler.
static int aci_get_wall (const char *cc, char *wall)
{
	int rc = 0;
	static const char src[] = "__dummy.c";
	FILE *f;

	f = fopen (src, "w");
	if (f == NULL) {
		return -1;
	}
	fprintf (f, "int x;\n");
	fclose (f);

	if (aci_try_wall (cc, src, "-Wall -Wextra") == 0) {
		strcpy (wall, "-Wall -Wextra");     /* GCC */
	} else if (aci_try_wall (cc, src, "-Wall") == 0) {
		strcpy (wall, "-Wall");     /* Some other compiler */
	} else if (strstr(cc, "bcc32") != NULL) {
		strcpy (wall, "-w");                /* Borland */
	} else if (strstr(cc, "dmc") != NULL) {
		strcpy (wall, "-w-");               /* Digital Mars */
	} else if (aci_try_wall (cc, src, "-W4") == 0) {
		strcpy (wall, "-W4");               /* Microsoft */
	} else if (aci_try_wall (cc, src, "-wx") == 0) {
		strcpy (wall, "-wx");               /* Watcom */
	} else {
		*wall = 0;
		rc = -1;
	}

	return rc;
}



// Get the name of the compiler from the environment.
static int aci_find_compiler_name (char *s, size_t n, int prefer_cxx)
{
	if (prefer_cxx && aci_get_make_var ("CXX", s, n) == 0) {
		return 0;
	}
	if (aci_get_make_var ("CC", s, n) == 0) {
		return 0;
	}
	return -1;
}


// Does this compiler use DOS conventions.
static int aci_is_dos_compiler (void)
{
	static const char src[] = "__dummy.c";
	static const char obj_dos[] = "__dummy.obj";
	FILE *f;
	char cmd[FILENAME_MAX];

	remove (obj_dos);
	f = fopen (src, "w");
	if (f == NULL) {
		return 0;
	}
	fprintf (f, "int x;\n");
	fclose (f);

	sprintf (cmd, "%s -c %s", aci_compile_cmd, src);
	if (aci_run_silent (cmd) == 0) {
		f = fopen (obj_dos, "rb");
		if (f != NULL) {
			fclose (f);
			return 1;
		}
	}
	return 0;
}


// How do we specify the name of the output file of the compiler?
static void aci_find_exe_out (void)
{
	static const char src[] = "__dummy.c";
	static const char exe_dos[] = "__kkk.exe";  /* Valid name for Woe and UNIX. */
	FILE *f;
	char cmd[FILENAME_MAX];

	remove (exe_dos);
	f = fopen (src, "w");
	if (f == NULL) {
		return;
	}
	fprintf (f, "int main() { return 0; } \n");
	fclose (f);

	sprintf (cmd, "%s -o %s %s", aci_compile_cmd, exe_dos, src);
	if (aci_run_silent (cmd) == 0) {
		f = fopen (exe_dos, "rb");
		if (f != NULL) {
			fclose (f);
			aci_exe_cmd = "-o $@";
		} else {
			sprintf (cmd, "%s -e%s %s", aci_compile_cmd, exe_dos, src);
			if (aci_run_silent (cmd) == 0) {
				f = fopen (exe_dos, "rb");
				if (f != NULL) {
					fclose (f);
					aci_exe_cmd = "-e$@";
				}
			}
		}
	}
	remove (src);  
	remove (exe_dos);
}



static void aci_set_source_extension (const char *ext)
{
	aci_source_extension = ext;
}

void ac_use_macro_prefix (const char *pfx)
{
	aci_macro_prefix = pfx;
}


// See if the makevars file specified the TARGET_ARCH.
static void aci_check_targetarch_in_makevars (void)
{
	 char s[1000];
	 FILE *f = fopen (aci_makevars_file, "r");
	 if (f == NULL) {
		  return;
	 }

	 while (fgets (s, sizeof s, f) != NULL) {
		  if (strncmp (s, "TARGET_ARCH", 11) == 0) {
			   size_t len;
			   char *cp = s + 11;
			   while (*cp && *cp != '=') ++cp;
			   if (*cp != '=') {
					break;
			   }

			   ++cp;
			   while (isspace (*cp)) ++cp;

			   len = strlen (cp);
			   if (len > 0 && cp[len - 1] == '\n') {
					--len;
					cp[len] = 0;
			   }

			   aci_target_arch_given = 1;
			   sbufcat (&aci_testing_flags, " ");
			   sbufcat (&aci_testing_flags, cp);
			   printf ("Using TARGET_ARCH=%s\n", cp);
			   break;
		  }
	 }

	 fclose (f);
}


// See if there is a site makevars file.
static void aci_check_makevars (void)
{
	FILE *f;

	ac_set_var ("LIBBIN", aci_have_woe32 ? "bin" : "lib");

	if (*aci_makevars_file) {
		aci_check_targetarch_in_makevars ();
		return;
	}

	f = fopen ("pelconf.var", "r");
	if (f) {
		strcpy (aci_makevars_file, "pelconf.var");
		fclose (f);
		aci_check_targetarch_in_makevars ();
		return;
	}

	if (aci_install_prefix[0]) {
		FILE *f;
		size_t n = strlen (aci_install_prefix);
		char tmp[FILENAME_MAX];

		strcpy (tmp, aci_install_prefix);
		if (tmp[n - 1] != '/') {
			tmp[n++] = '/';
		}
		strcpy (tmp + n, "etc/pelconf.var");
		f = fopen (tmp, "r");
		if (f) {
			strcpy (aci_makevars_file, tmp);
			fclose (f);
			aci_check_targetarch_in_makevars ();
			return;
		}
	}

	strcpy (aci_makevars_file, "pelconf.var");
	aci_warn_makevars = 1;
}



// Try to locate where the compiler has been hidden.
static int aci_locate_woe_compiler(const char *cmd, char *fullpath, size_t n)
{
	sbuf_t sb;
	const char *sow, *eow, *cmdeow;
	const char *path;
	int result = -1;
	FILE *fr;
	char cmdword[FILENAME_MAX];

	sbufinit(&sb);

	/* Locate end of the first part of the command */
	for (cmdeow = cmd; *cmdeow && *cmdeow != ' '; ++cmdeow) ;
	memcpy (cmdword, cmd, cmdeow - cmd);
	cmdword[cmdeow - cmd] = 0;
	if (strcmp(cmdword + (cmdeow - cmd) - 4, ".exe") != 0) {
		strcat (cmdword, ".exe");
	}

	if (*cmdword == '/') {
		size_t nw = strlen(cmdword);
		char *cw = cmdword + nw - 1;
		while (cw > cmdword && *cw != '/') {
			--cw;
		}
		*++cw = 0;
		nw = cw - cmdword;
		if (nw > n) {
			nw = n;
			if (nw > 0) {
				cmdword[nw - 1] = 0;
			}
		}
		memcpy (fullpath, cmdword, nw);
		result = 0;
		goto done;
	}

	path = getenv("PATH");
	if (path == NULL) {
		goto done;
	}

	sow = path;
	while (*sow) {
		eow = sow;
		while (*eow && *eow != ';') ++eow;
		if (eow != sow) {
			size_t prefix_length;
			sbufncpy (&sb, sow, eow - sow);
			sbufcat (&sb, "/");
			prefix_length = sbuflen (&sb);
			sbufcat (&sb, cmdword);

			fr = fopen (sbufchars(&sb), "rb");
			if (fr) {
				fclose (fr);
				sbuftrunc(&sb, prefix_length);
				if (strcmp("bin/", sbufchars(&sb) + prefix_length - 4) == 0) {
					char *wp;
					prefix_length -= 4;

					if (n > prefix_length + 1) {
						n = prefix_length + 1;
					}
					for (wp = sbufchars(&sb); *wp; ++wp) {
						if (*wp == '\\') {
							*wp = '/';
						}
					}
					memcpy (fullpath, sbufchars(&sb), n);
					fullpath[n - 1] = 0;

					result = 0;
					break;
				}
			}

			sow = eow;
			while (*sow && *sow == ';') ++sow;
		}
	}


done:
	sbuffree(&sb);
	return result;
}



// Figure out which prefix to use. The user may specify it in the command
// line. If we are non running windows we assume it is /usr/local. Otherwise
// it will be the directory where the compiler is located, with /local added
// to it.
static void aci_get_prefix(void)
{
	char pfx[FILENAME_MAX];
	ptrdiff_t n = ac_have_feature ("prefix", pfx, sizeof pfx);

	if (n > 0 && n < (ptrdiff_t)(sizeof pfx) + 1) {
		if (pfx[n - 1] != '/') {
			pfx[n] = '/';
			pfx[n + 1] = 0;
		}
		ac_set_var ("PREFIX", pfx);
		strcpy (aci_install_prefix, pfx);
	} else if (aci_have_woe32 && !aci_have_cygwin) {
		if (aci_locate_woe_compiler (aci_compile_cmd, pfx, sizeof pfx) == 0) {
			strcat (pfx, "local/");
			ac_set_var ("PREFIX", pfx);
			strcpy (aci_install_prefix, pfx);
		} else {
			ac_set_var ("PREFIX", "/usr/local/");
			strcpy (aci_install_prefix, "/usr/local/");
		}
	} else {
		ac_set_var ("PREFIX", "/usr/local/");
		strcpy (aci_install_prefix, "/usr/local/");
	}
}


// Check for the availability of different GCC flags.
static void aci_check_gcc_flags (int prefer_cxx)
{
	sbufcat (&aci_testing_flags, " -Werror");
	if (!aci_target_arch_given) {
		 if (ac_check_compiler_flag ("-march=native", "TARGET_ARCH") == 0) {
			 sbufcat (&aci_testing_flags, " -march=native");
		 }
	}

	if (ac_check_compiler_flag ("-fpic", "GCC_FPIC") == 0) {
		 sbufcat (&aci_testing_flags, " -fpic");
	}

	/* Use position independent executables for increased security
	   According to some reports the run time overhead is negligible (less than
	   1% on x64 and around 5% on i386.
	   See http://d-sbd.alioth.debian.org/www/?page=pax_pie
	 */
	ac_check_compiler_flag ("-fpie", "GCC_FPIE");
	if (aci_have_woe32) {
		ac_check_compiler_flag ("-Wl,--dynamicbase,--nxcompat", "GCC_PIE");
	} else {
		ac_check_compiler_flag ("-pie", "GCC_PIE");
	}

	ac_check_compiler_flag ("-fextended-identifiers", "GCC_EXTIDENT");
	if (ac_check_compiler_flag ("-fvisibility=hidden", "GCC_VISHIDDEN") == 0) {
		 sbufcat (&aci_testing_flags, " -fvisibility=hidden");
	}
	if (ac_check_compiler_flag ("-Wl,--enable-new-dtags", "GCC_NEWDTAGS") == 0) {
		 sbufcat (&aci_testing_flags, " -Wl,--enable-new-dtags");
	}
	if (ac_check_compiler_flag ("-Wl,--rpath='$$ORIGIN'", "GCC_RPATH_LIB") == 0) {
	   ac_set_var ("GCC_RPATH_BIN", "-Wl,--rpath='$$ORIGIN/../lib'");
		 ac_set_var ("GCC_RPATH_LIB", "-Wl,--rpath='$$ORIGIN'");
		 ac_set_var ("GCC_RPATH_PREFIX", "-Wl,--rpath=$(PREFIX)lib");
	}

	if (ac_check_compiler_flag ("-Wl,--as-needed", "GCC_ASNEEDED") == 0) {
		 sbufcat (&aci_testing_flags, " -Wl,--as-needed");
	}
	if (ac_check_compiler_flag ("-mthreads", "GCC_MTHREADS") == 0) {
		 sbufcat (&aci_testing_flags, " -mthreads");
	}
	ac_check_compiler_flag ("-O2", "GCC_O2");
	ac_check_compiler_flag ("-fomit-frame-pointer", "GCC_OMITFRAMEPOINTER");
	ac_check_compiler_flag ("-ftree-vectorize", "GCC_TREEVECTORIZE");
	ac_check_compiler_flag ("-ffast-math", "GCC_FASTMATH");
	ac_check_compiler_flag ("-g", "GCC_G");
	ac_check_compiler_flag ("-fstack-protector", "GCC_STACK_PROTECTOR");
	ac_check_compiler_flag ("-fstack-protector-all", "GCC_STACK_PROTECTOR_ALL");
	// Does not work in Cygwin yet.
	if (!aci_have_woe32) {
		ac_check_compiler_flag ("-gsplit-dwarf", "GCC_SPLIT_DWARF");
		ac_check_compiler_flag ("-Wa,--compress-debug-sections", "GCC_COMPRESS_DEBUG_SECTIONS");
	}
	ac_check_compiler_flag ("-Wl,--gdb-index", "GCC_GDB_INDEX");
	ac_check_compiler_flag ("-ftrapv", "GCC_TRAPV");
	ac_check_compiler_flag ("-fnon-call-exception", "GCC_NON_CALL_EXCEPTION");
	ac_check_compiler_flag ("-Wabi-tag", "GCC_WABI_TAG");

	if (ac_check_compiler_flag ("-shared -Wl,--soname=foo", "GCC_SONAME") == 0) {
		ac_set_var ("GCC_SONAME", "-Wl,--soname=$(notdir $@)");
	}

	if (ac_check_compiler_flag ("-shared -Wl,--out-implib=foo.a", "GCC_OUTIMPLIB") == 0) {
		ac_set_var ("GCC_OUTIMPLIB", "-Wl,--out-implib=$@$(A)");
	}

	if (strstr (sbufchars (&aci_extra_cflags), "-std=") != NULL) {
		aci_use_stdver = 0;
	}

	if (aci_use_stdver) {
		if (prefer_cxx) {
			if (ac_check_compiler_flag ("-std=gnu++17", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu++17");
			} else if (ac_check_compiler_flag ("-std=gnu++1z", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu++1z");
			} else if (ac_check_compiler_flag ("-std=gnu++14", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu++14");
			} else if (ac_check_compiler_flag ("-std=gnu++1y", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu++1y");
			} else if (ac_check_compiler_flag ("-std=gnu++11", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu++11");
			} else if (ac_check_compiler_flag ("-std=gnu++0x", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu++0x");
			}
		} else {
			if (ac_check_compiler_flag ("-std=gnu11", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu11");
			} else if (ac_check_compiler_flag ("-std=gnu99", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu99");
			}
		}
	}
	if (aci_static) {
		ac_set_var("GCC_STATIC", "-static");
		sbufcat(&aci_testing_flags, " -static");
	}


	if (aci_simple) {
		ac_add_var_append("CFLAGS", "$(TARGET_ARCH) $(GCC_STD) $(GCC_POSIX)");
		ac_add_var_append("LDFLAGS", "$(TARGET_ARCH) $(GCC_MTHREADS) $(GCC_STD) $(GCC_POSIX)");
		ac_add_var_append ("CFLAGS_DEBUG", "$(GCC_G)");
		ac_add_var_append ("CFLAGS_OPTIMIZE", "$(GCC_O2) -DNDEBUG");
		ac_add_var_append ("LDFLAGS_DEBUG", "$(GCC_G)");
		ac_add_var_append ("LDFLAGS_OPTIMIZE", "$(GCC_O2)");
		ac_add_var_append ("SO_CFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_MTHREADS) $(GCC_FPIC)\\\n"
							 "          $(GCC_STD) $(GCC_G) \\\n"
							 "          $(GCC_POSIX)\n");
		ac_add_var_append ("SO_LDFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_NEWDTAGS) $(GCC_RPATH_LIB) \\\n"
							  "         $(GCC_RPATH_PREFIX) $(GCC_ASNEEDED) $(GCC_MTHREADS) $(GCC_STD) \\\n"
							  "         $(GCC_SONAME) $(GCC_POSIX) -shared");

	} else {
		ac_add_var_append ("CFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_MTHREADS) $(GCC_STD)\\\n"
						  "         $(GCC_TREEVECTORIZE) \\\n"
						  "         $(GCC_FASTMATH) $(GCC_POSIX) $(GCC_EXTIDENT)");
		ac_add_var_append ("SO_CFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_MTHREADS) $(GCC_FPIC)\\\n"
							 "          $(GCC_STD) $(GCC_G) $(GCC_SPLIT_DWARF) \\\n"
							 "          $(GCC_TREEVECTORIZE) $(GCC_FASTMATH) $(GCC_POSIX) $(GCC_EXTIDENT)\n");
		ac_add_var_append ("PIE_CFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_MTHREADS) $(GCC_FPIE)\\\n"
							 "          $(GCC_STD) $(GCC_STATIC)\\\n"
							 "          $(GCC_TREEVECTORIZE) $(GCC_FASTMATH) $(GCC_POSIX) $(GCC_EXTIDENT)\n");
		ac_add_var_append ("LDFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_NEWDTAGS) $(GCC_RPATH_LIB) $(GCC_RPATH_BIN)\\\n"
						   "            $(GCC_RPATH_PREFIX) $(GCC_ASNEEDED) $(GCC_MTHREADS) $(GCC_STD) $(GCC_POSIX) $(GCC_EXTIDENT)\\\n");
		ac_add_var_append ("PIE_LDFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_NEWDTAGS) $(GCC_RPATH_LIB) $(GCC_RPATH_BIN)\\\n"
						   "            $(GCC_RPATH_PREFIX) $(GCC_ASNEEDED) $(GCC_MTHREADS) $(GCC_STD) $(GCC_POSIX) $(GCC_EXTIDENT)\\\n"
						 "            $(GCC_PIE)");
		ac_add_var_append ("SO_LDFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_NEWDTAGS) $(GCC_RPATH_LIB) \\\n"
							  "         $(GCC_RPATH_PREFIX) $(GCC_ASNEEDED) $(GCC_MTHREADS) $(GCC_STD) \\\n"
							  "         $(GCC_G) $(GCC_GDB_INDEX) $(GCC_SONAME) $(GCC_POSIX) $(GCC_EXTIDENT) -shared");

		ac_add_var_append ("CFLAGS_DEBUG", "$(GCC_G) $(GCC_STACK_PROTECTOR_ALL) $(GCC_COMPRESS_DEBUG_SECTIONS) $(GCC_SPLIT_DWARF) $(GCC_TRAPV) $(GCC_NON_CALL_EXCEPTION)");
		ac_add_var_append ("CFLAGS_OPTIMIZE", "$(GCC_O2) $(GCC_OMITFRAMEPOINTER) -DNDEBUG");
		ac_add_var_append ("LDFLAGS_DEBUG", "$(GCC_G) $(GCC_STACK_PROTECTOR_ALL) $(GCC_SPLIT_DWARF) $(GCC_GDB_INDEX) $(GCC_TRAPV) $(GCC_NON_CALL_EXCEPTION)");
		ac_add_var_append ("LDFLAGS_OPTIMIZE", "$(GCC_O2)");
	}
}

// Check for the availability of Tiny CC flags.
static void aci_check_tinyc_flags (void)
{
	sbufcat (&aci_testing_flags, " -Werror");
	if (!aci_target_arch_given) {
		 if (ac_check_compiler_flag ("-march=native", "TARGET_ARCH") == 0) {
			 sbufcat (&aci_testing_flags, " -march=native");
		 }
	}

	if (ac_check_compiler_flag ("-fpic", "GCC_FPIC") == 0) {
		 sbufcat (&aci_testing_flags, " -fpic");
	}
	if (ac_check_compiler_flag ("-fvisibility=hidden", "GCC_VISHIDDEN") == 0) {
		 sbufcat (&aci_testing_flags, " -fvisibility=hidden");
	}
	if (ac_check_compiler_flag ("-Wl,--enable-new-dtags", "GCC_NEWDTAGS") == 0) {
		 sbufcat (&aci_testing_flags, " -Wl,--enable-new-dtags");
	}
	if (ac_check_compiler_flag ("-Wl,--rpath='$$ORIGIN'", "GCC_RPATH_LIB") == 0) {
		 ac_set_var ("GCC_RPATH_BIN", "-Wl,--rpath='$$ORIGIN/../lib'");
		 ac_set_var ("GCC_RPATH_LIB", "-Wl,--rpath='$$ORIGIN'");
		 ac_set_var ("GCC_RPATH_PREFIX", "-Wl,--rpath=$(PREFIX)lib");
	}

	if (ac_check_compiler_flag ("-Wl,--as-needed", "GCC_ASNEEDED") == 0) {
		 sbufcat (&aci_testing_flags, " -Wl,--as-needed");
	}
	if (ac_check_compiler_flag ("-mthreads", "GCC_MTHREADS") == 0) {
		 sbufcat (&aci_testing_flags, " -mthreads");
	}
	ac_check_compiler_flag ("-O2 -s", "GCC_O2");
	ac_check_compiler_flag ("-fomit-frame-pointer", "GCC_OMITFRAMEPOINTER");
	ac_check_compiler_flag ("-ftree-vectorize", "GCC_TREEVECTORIZE");
	ac_check_compiler_flag ("-ffast-math", "GCC_FASTMATH");
	ac_check_compiler_flag ("-g", "GCC_G");

	if (ac_check_compiler_flag ("-shared -Wl,--soname=foo", "GCC_SONAME") == 0) {
		ac_set_var ("GCC_SONAME", "-Wl,--soname=$(notdir $@)");
	}

	if (ac_check_compiler_flag ("-shared -Wl,--out-implib=foo.a", "GCC_OUTIMPLIB") == 0) {
		ac_set_var ("GCC_OUTIMPLIB", "-Wl,--out-implib=$@$(A)");
	}

	if (strstr (sbufchars (&aci_extra_cflags), "-std=") != NULL) {
		aci_use_stdver = 0;
	}

	if (aci_use_stdver) {
		  if (ac_check_compiler_flag ("-std=gnu11", "GCC_STD") == 0) {
			   sbufcat (&aci_testing_flags, " -std=gnu11");
		  } else if (ac_check_compiler_flag ("-std=gnu99", "GCC_STD") == 0) {
			   sbufcat (&aci_testing_flags, " -std=gnu99");
		  }
	}

	ac_add_var_append ("CFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_MTHREADS) $(GCC_STD)\\\n"
						  "         $(GCC_TREEVECTORIZE)\\\n"
						  "         $(GCC_FASTMATH) $(GCC_POSIX)");
	ac_add_var_append ("SO_CFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_MTHREADS) $(GCC_FPIC)\\\n"
							 "          $(GCC_STD) $(GCC_OMITFRAMEPOINTER) \\\n"
							 "          $(GCC_TREEVECTORIZE) $(GCC_FASTMATH) $(GCC_POSIX)");
	ac_add_var_append ("LDFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_NEWDTAGS) $(GCC_RPATH_BIN)\\\n"
						   "            $(GCC_RPATH_PREFIX) $(GCC_ASNEEDED) $(GCC_MTHREADS) $(GCC_STD) $(GCC_POSIX)");
	ac_add_var_append ("SO_LDFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_NEWDTAGS) $(GCC_RPATH_LIB) \\\n"
							  "         $(GCC_RPATH_PREFIX) $(GCC_ASNEEDED) $(GCC_MTHREADS) \\\n"
							  "         $(GCC_G) $(GCC_STD) $(GCC_SONAME) $(GCC_POSIX) -shared");

	ac_add_var_append ("CFLAGS_DEBUG", "$(GCC_G)");
	ac_add_var_append ("CFLAGS_OPTIMIZE", "$(GCC_O2) $(GCC_OMITFRAMEPOINTER) -DNDEBUG");
//    ac_add_var_append ("LDFLAGS_DEBUG", "");
	ac_add_var_append ("LDFLAGS_OPTIMIZE", "$(GCC_O2)");
}


// Check for the availability of clang flags.
void aci_check_clang_flags (int prefer_cxx)
{
	sbufcat (&aci_testing_flags, " -Werror");

	if (!aci_target_arch_given) {
		 if (ac_check_compiler_flag ("-march=native", "TARGET_ARCH") == 0) {
			 sbufcat (&aci_testing_flags, " -march=native");
		 }
	}

	ac_check_compiler_flag ("-fpic", "GCC_FPIC");

	/* Use position independent executables for increased security
	   According to some reports the run time overhead is negligible (less than
	   1% on x64 and around 5% on i386.
	   See http://d-sbd.alioth.debian.org/www/?page=pax_pie
	 */
	ac_check_compiler_flag ("-fpie", "GCC_FPIE");
	ac_check_compiler_flag ("-pie", "GCC_PIE");

	ac_check_compiler_flag ("-fvisibility=hidden", "GCC_VISHIDDEN");
	ac_check_compiler_flag ("-Wl,--enable-new-dtags", "GCC_NEWDTAGS");
	if (ac_check_compiler_flag ("-Wl,--rpath='$$ORIGIN'", "GCC_RPATH_LIB") == 0) {
		 ac_set_var ("GCC_RPATH_BIN", "-Wl,--rpath='$$ORIGIN/../lib'");
		 ac_set_var ("GCC_RPATH_LIB", "-Wl,--rpath='$$ORIGIN'");
		 ac_set_var ("GCC_RPATH_PREFIX", "-Wl,--rpath=$(PREFIX)lib");
	}

	ac_check_compiler_flag ("-Wl,--as-needed", "GCC_ASNEEDED");
	if (ac_check_compiler_flag ("-mthreads", "GCC_MTHREADS") == 0) {
		 sbufcat (&aci_testing_flags, " -mthreads");
	}
	ac_check_compiler_flag ("-O2 -s", "GCC_O2");
	ac_check_compiler_flag ("-fomit-frame-pointer", "GCC_OMITFRAMEPOINTER");
	ac_check_compiler_flag ("-ftree-vectorize", "GCC_TREEVECTORIZE");
	ac_check_compiler_flag ("-ffast-math", "GCC_FASTMATH");
	ac_check_compiler_flag ("-g", "GCC_G");
	ac_check_compiler_flag ("-fstack-protector", "GCC_STACK_PROTECTOR");
	// Fails on Woe
//    ac_check_compiler_flag ("-ftrapv", "GCC_TRAPV");
	ac_check_compiler_flag ("-fnon-call-exception", "GCC_NON_CALL_EXCEPTION");

	if (ac_check_compiler_flag ("-shared -Wl,--soname=foo", "GCC_SONAME") == 0) {
		ac_set_var ("GCC_SONAME", "-Wl,--soname=$(notdir $@)");
	}

	if (ac_check_compiler_flag ("-shared -Wl,--out-implib=foo.a", "GCC_OUTIMPLIB") == 0) {
		ac_set_var ("GCC_OUTIMPLIB", "-Wl,--out-implib=$@$(A)");
	}


	if (strstr (sbufchars (&aci_extra_cflags), "-std=") != NULL) {
		aci_use_stdver = 0;
	}

	if (aci_use_stdver) {
		if (prefer_cxx) {
			if (ac_check_compiler_flag ("-std=gnu++11", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu++11");
			} else if (ac_check_compiler_flag ("-std=gnu++0x", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu++0x");
			}
		} else {
			if (ac_check_compiler_flag ("-std=gnu11", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu11");
			} else if (ac_check_compiler_flag ("-std=gnu99", "GCC_STD") == 0) {
				sbufcat (&aci_testing_flags, " -std=gnu99");
			}
		}
	}
	if (aci_static) {
		sbufcat(&aci_testing_flags, " -static");
	}

	ac_add_var_append ("CFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_MTHREADS) $(GCC_STD)\\\n"
						  "         $(GCC_TREEVECTORIZE) \\\n"
						  "         $(GCC_FASTMATH) $(GCC_POSIX)");
	ac_add_var_append ("SO_CFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_MTHREADS) $(GCC_FPIC)\\\n"
							 "          $(GCC_STD) $(GCC_OMITFRAMEPOINTER) $(GCC_G)\\\n"
							 "          $(GCC_TREEVECTORIZE) $(GCC_FASTMATH) $(GCC_POSIX)");
	ac_add_var_append ("LDFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_NEWDTAGS) $(GCC_RPATH_LIB) $(GCC_RPATH_BIN)\\\n"
						   "            $(GCC_RPATH_PREFIX) $(GCC_ASNEEDED) $(GCC_MTHREADS) $(GCC_STD) $(GCC_POSIX)\\\n"
						   "            $(GCC_PIE)");
	ac_add_var_append ("SO_LDFLAGS", "$(TARGET_ARCH) $(GCC_VISHIDDEN) $(GCC_NEWDTAGS) $(GCC_RPATH_LIB) \\\n"
							  "         $(GCC_RPATH_PREFIX) $(GCC_ASNEEDED) $(GCC_MTHREADS) $(GCC_STD)\\\n"
							  "         $(GCC_SONAME) $(GCC_POSIX) $(GCC_G) -shared");

	ac_add_var_append ("CFLAGS_DEBUG", "$(GCC_G) $(GCC_STACK_PROTECTION) $(GCC_TRAPV) $(GCC_NON_CALL_EXCEPTION)");
	ac_add_var_append ("CFLAGS_OPTIMIZE", "$(GCC_O2) $(GCC_OMITFRAMEPOINTER) -DNDEBUG");
	ac_add_var_append ("LDFLAGS_DEBUG", "$(GCC_STACK_PROTECTION) $(GCC_TRAPV) $(GCC_NON_CALL_EXCEPTION)");
	ac_add_var_append ("LDFLAGS_OPTIMIZE", "$(GCC_O2)");
}



// Start everything.
void ac_init (const char *extension, int argc, char **argv, int latest_c_version)
{
	int prefer_cxx = 0;
	const char *pfx;
	int use_dos_conventions = 0;
	const char *cp;
	static char compiler_name[FILENAME_MAX];
	char wall[200];
	sbuf_t config_string;
	int i;

	remove ("configure.log");

	sbufinit (&aci_include_dirs);
	sbufinit (&aci_lib_dirs);
	sbufinit (&aci_extra_cflags);
	sbufinit (&aci_extra_ldflags);
	sbufinit (&aci_additional_libs);
	sbufinit (&aci_testing_flags);
	sbufinit (&aci_common_headers);

	sbufinit (&config_string);
	for (i = 1; i < argc; ++i) {
		sbufcat (&config_string, " ");
		sbufcat (&config_string, argv[i]);
	}

	sbufinit (&stdint_proxy);
	aci_varlist_init (&aci_makevars);
	aci_varlist_init (&aci_features);
	if (!aci_valid_opts_inited) {
		aci_valid_opts_inited = 1;
		aci_strlist_init (&aci_valid_options);
		aci_strlist_init (&aci_valid_options_desc);
	}
	aci_strlist_init (&aci_given_options);
	aci_strlist_init (&aci_tdefs);
	aci_strlist_init (&aci_pkg_config_packs);
	if (aci_has_option (&argc, argv, "help")) {
		aci_usage ("configure");
		ac_show_help ();
		aci_help_wanted = 1;
		exit (EXIT_SUCCESS);
	}

	aci_set_source_extension (extension);

	if (strcmp (aci_source_extension, ".c") != 0) {
		prefer_cxx = 1;
		aci_need_cxx_check = 1;
	}

	aci_make_cmd = getenv ("MAKE");
	if (aci_make_cmd == NULL) {
		aci_make_cmd = "make";
	}

	aci_compile_cmd = aci_has_optval (&argc, argv, aci_cc_name);

	if (aci_compile_cmd == NULL && prefer_cxx) {
		aci_compile_cmd = aci_has_optval (&argc, argv, "CXX");
	}

	if (aci_compile_cmd == NULL) {
		aci_compile_cmd = aci_has_optval (&argc, argv, "CC");
	}

	if (aci_compile_cmd == NULL && prefer_cxx) {
		aci_compile_cmd = getenv ("CXX");
	}
	if (aci_compile_cmd == NULL) {
		aci_compile_cmd = getenv ("CC");
	}
	if (aci_compile_cmd == NULL &&
	        aci_find_compiler_name (compiler_name, sizeof compiler_name, prefer_cxx) == 0) {
		aci_compile_cmd = compiler_name;
	}
	if (aci_compile_cmd == NULL) {
		printf ("specify the compiler using --%s", aci_cc_name);
		sbuffree (&stdint_proxy);
		exit (EXIT_FAILURE);
	}

	aci_add_cmd_vars (argc, argv);
	aci_check_woe32 ();
	aci_get_prefix ();
	aci_check_makevars ();
	aci_check_woe32 ();

	while ((cp = aci_has_optval (&argc, argv, "with-extra-includes")) != NULL) {
		if (sbuflen (&aci_include_dirs) != 0) {
			sbufcat (&aci_include_dirs, aci_have_woe32 ? ";" : ":");
		} else {
			sbufcat (&aci_include_dirs, "-I");
		}
		sbufcat (&aci_include_dirs, cp);
	}
	ac_set_var ("EXTRA_INCLUDE_DIRS", sbufchars (&aci_include_dirs));

	while ((cp = aci_has_optval (&argc, argv, "with-extra-libs")) != NULL) {
		if (sbuflen (&aci_lib_dirs) != 0) {
			sbufcat (&aci_lib_dirs, aci_have_woe32 ? ";" : ":");
		} else {
			sbufcat (&aci_lib_dirs, "-L");
		}
		sbufcat (&aci_lib_dirs, cp);
	}
	ac_set_var ("EXTRA_LIB_DIRS", sbufchars (&aci_lib_dirs));

	while ((cp = aci_has_optval (&argc, argv, "extra-cflags")) != NULL) {
		if (sbuflen (&aci_extra_cflags) != 0) {
			sbufcat (&aci_extra_cflags, " ");
		}
		sbufcat (&aci_extra_cflags, cp);
	}

	aci_add_cflags_to_makevars (sbufchars (&aci_extra_cflags));

	while ((cp = aci_has_optval (&argc, argv, "extra-ldflags")) != NULL) {
		if (sbuflen (&aci_extra_ldflags) != 0) {
			sbufcat (&aci_extra_ldflags, " ");
		}
		sbufcat (&aci_extra_ldflags, cp);
	}
	sbufcat(&aci_extra_ldflags, " ");
	ac_set_var ("EXTRA_LDFLAGS", sbufchars (&aci_extra_ldflags));

	while ((cp = aci_has_optval (&argc, argv, "add-libs")) != NULL) {
		if (sbuflen (&aci_additional_libs) != 0) {
			sbufcat (&aci_additional_libs, " ");
		}
		sbufcat (&aci_additional_libs, cp);
	}
	sbufcat(&aci_additional_libs, " ");

	aci_lib_prefix = "-l";
	aci_lib_suffix = "";

	if (aci_is_dos_compiler()) {
		use_dos_conventions = 1;
	}

	if (aci_check_define ("", NULL, "__clang__")) {
		aci_compiler_id = aci_cc_clang;
	} else if (aci_check_define ("", NULL, "__GNUC__")) {
		aci_compiler_id = aci_cc_gcc;
	} else if (aci_check_define ("", NULL, "__BORLANDC__")) {
		aci_compiler_id = aci_cc_bcc32;
	} else if (aci_check_define ("", NULL, "__TINYC__")) {
		aci_compiler_id = aci_cc_tinyc;
	}

	if (aci_compiler_id == aci_cc_bcc32) {
		aci_exe_cmd = "-e$@";
	} else {
		aci_find_exe_out ();
	}

	aci_verbose = aci_has_option (&argc, argv, aci_verbose_name);
	aci_keep    = aci_has_option (&argc, argv, aci_keep_name);
	aci_simple  = aci_has_option (&argc, argv, aci_simple_name);
	aci_static  = aci_has_option (&argc, argv, aci_static_name);

	if (aci_has_option (&argc, argv, aci_stdver)) {
		aci_use_stdver = 1;
	} else if (aci_has_option (&argc, argv, aci_nostdver)) {
		aci_use_stdver = 0;
	} else {
		aci_use_stdver = latest_c_version;
	}

	ac_set_var ("CFLAGS", "$(EXTRA_CFLAGS) $(EXTRA_INCLUDE_DIRS)");
	ac_set_var ("SO_CFLAGS", "$(EXTRA_CFLAGS) $(EXTRA_INCLUDE_DIRS)");
	ac_set_var ("LDFLAGS", "$(EXTRA_LDFLAGS) $(EXTRA_LIB_DIRS)");
	ac_set_var ("SO_LDFLAGS", "$(EXTRA_LDFLAGS) $(EXTRA_LIB_DIRS)");

	switch (aci_compiler_id) {
	case aci_cc_gcc:
		aci_check_gcc_flags (prefer_cxx);
		break;

	case aci_cc_bcc32:
		ac_check_compiler_flag ("-WD", "BCC_WD");
		ac_check_compiler_flag ("-tWCR", "BCC_TWCR");
		ac_check_compiler_flag ("-tWM", "BCC_TWM");
		ac_check_compiler_flag ("-lGi", "BCC_LGI");
		ac_check_compiler_flag ("-6", "TARGET_ARCH");

		ac_add_var_append ("CFLAGS", "$(BCC_TWM) $(BCC_TWCR) -q");
		ac_add_var_append ("SO_CFLAGS", "$(BCC_TWM) $(BCC_TWCR) -q");
		ac_add_var_append ("LDFLAGS", "$(BCC_TWM) $(BCC_TWCR) -q");
		ac_add_var_append ("SO_LDFLAGS", "$(BCC_TWM) $(BCC_TWCR) $(BCC_WD) $(BCC_LGI) -q");
		break;

	case aci_cc_tinyc:
		aci_check_tinyc_flags();
		break;

	case aci_cc_clang:
		aci_check_clang_flags (prefer_cxx);
		break;
	}


	if (aci_has_option (&argc, argv, aci_dos_name)) {
		use_dos_conventions = 1;
	}

	if (use_dos_conventions) {
		aci_lib_suffix = ".lib";
		aci_lib_prefix = "";
	}

	ac_set_var ("LDPOS", aci_lib_suffix);
	ac_set_var ("LDPRE", aci_lib_prefix);
	ac_set_var ("OBJ", use_dos_conventions ? ".obj" : ".o");
	ac_set_var ("A", use_dos_conventions ? ".lib" : ".a");
	ac_set_var ("LIB", use_dos_conventions ? "" : "lib");

	pfx = aci_has_optval (&argc, argv, aci_namespace_name);
	if (pfx) {
		ac_use_macro_prefix (pfx);
	}

	cp = aci_has_optval (&argc, argv, aci_makevars_name);
	if (cp) {
		strcpy (aci_makevars_file, cp);
	}

	ac_set_var ("OUTPUT_OPTION", aci_exe_cmd);
	ac_set_var ("CC", aci_compile_cmd);
	ac_set_var ("CXX", aci_compile_cmd);

	aci_get_wall (aci_compile_cmd, wall);
	ac_set_var ("WALL", wall);

	if (!aci_help_wanted) {
		printf ("The compilation command is %s\n", aci_compile_cmd);
		printf ("The make command is %s\n", aci_make_cmd);
		printf ("The libraries have the form %s<name>%s\n", aci_lib_prefix, aci_lib_suffix);
	}

/*    aci_check_werror (); */
	aci_check_stdint ();
	aci_check_endian_cross (use_dos_conventions ? ".obj" : ".o");
#if 0
	aci_check_imp_hack (use_dos_conventions ? ".obj" : ".o");
#endif
	aci_check_misc_once ();

	int has_abiname = (aci_varlist_find(&aci_features, "ABINAME") != NULL);

	if (ac_check_woe32()) {
		ac_set_var ("SO", ".dll");
		if (has_abiname) {
			ac_set_var ("SOV", "-$(ABINAME)-$(SOMAJOR).dll");
		} else {
			ac_set_var ("SOV", "-$(SOMAJOR).dll");
		}
	} else {
		ac_set_var ("SO", ".so");
		if (has_abiname) {
			ac_set_var ("SOV", "-$(ABINAME).so.$(SOMAJOR)");
		} else {
			ac_set_var ("SOV", ".so.$(SOMAJOR)");
		}
	}
	if (aci_get_alldeps(wall) == 0) {
		ac_set_var ("ALLDEPS", wall);
	}

	aci_include_form = aci_get_include_form ();
	sbufinit (&aci_common_headers);

	aci_varlist_set (&aci_features, "CONFIGURATION", sbufchars (&config_string));
	sbuffree (&config_string);
}



// Write out the configuration file to "config_name". Prefix the
// configuration macros with "feature_pfx".
void ac_config_out (const char *config_name, const char *feature_pfx)
{
	FILE *f;
	char config_name_upper[FILENAME_MAX], *wp;
	const char *rp;

	printf ("Writing configuration file '%s'\n", config_name);

	f = fopen (config_name, "w");
	if (f == NULL) {
		fprintf (stderr, "ERROR: could not create the configuration header %s\n",
		         config_name);
		exit (EXIT_FAILURE);
	}

	rp = config_name;
	wp = config_name_upper;
	while (*rp) {
		*wp = toupper (*rp);
		if (!isalnum (*wp)) {
			*wp = '_';
		}
		++rp;
		++wp;
	}
	*wp = 0;

	fprintf (f, "#ifndef %s_%s_INCLUDED\n", feature_pfx, config_name_upper);
	fprintf (f, "#define %s_%s_INCLUDED\n", feature_pfx, config_name_upper);
	fprintf (f, "/* Automatically generated by the pelconf program, do not edit. */\n\n");

	aci_flag_list_dump (aci_flags_root, f);
	fputs ("\n\n", f);

	aci_dump_strlist (&aci_tdefs, f, 1);
	fputs ("\n\n", f);

	aci_dump_features (&aci_features, f, feature_pfx);

	fprintf (f, "#endif\n");
	fclose (f);
}


// Write the completed makefile reading it from "make_in" and putting it in
// "make_out".
void ac_edit_makefile (const char *make_in, const char *make_out)
{
	FILE *fr, *fw;
	sbuf_t sb;

	printf ("Generating file '%s' from '%s'\n", make_out, make_in);

	fr = fopen (make_in, "r");
	if (fr == NULL) {
		fprintf (stderr, "error: could not open the configuration file "
				"template %s\n", make_in);
		exit (EXIT_FAILURE);
	}
	fw = fopen (make_out, "wb");
	if (fw == NULL) {
		fprintf (stderr, "ERROR: could not create the new configuration file %s\n",
		        make_out);
		exit (EXIT_FAILURE);
	}

	fprintf (fw, "# Automatically generated by the pelconf program, do not edit.\n");
	fprintf (fw, "# This file was generated from %s\n", make_in);

	aci_varlist_dump (&aci_makevars, fw, 1);

	fprintf (fw, "PKG_CONFIG_PACKS= ");
	aci_dump_strlist (&aci_pkg_config_packs, fw, 0);
	fprintf (fw, "\n");

	if (aci_have_woe32) {
		fprintf (fw, "%%: %%.exe\n\t\n");
	}


	fprintf (fw, "\n\n# Compiler specific makefile\n");
	if (aci_warn_makevars) {
		fprintf (fw, "#include here the compiler specific configuration file.\n#");
	}
	fprintf (fw, "%s %s\n", aci_include_form, aci_makevars_file);
	if (aci_warn_makevars) {
		fprintf (fw, "AR=ar -cru\n");
	}

	printf ("Using compiler specific configuration file %s\n", aci_makevars_file);
	fprintf (fw, "\n\n# Start of the input file %s\n\n", make_in);

	sbufinit (&sb);
	while (sbufgets (&sb, fr) == 0) {
		if (strncmp (sbufchars (&sb), "#include ", 9) == 0) {
			fprintf (fw, "%s %s\n", aci_include_form, sbufchars (&sb) + 9);
		} else {
			fprintf (fw, "%s\n", sbufchars (&sb));
		}
	}

	fclose (fr);
	fclose (fw);
	sbuffree (&sb);
}


// Write a makefile variable.
static void aci_varnode_dump (const char *name, FILE *f)
{
	const aci_varnode_t *vn;
	char **beg, **end;

	vn = aci_varlist_find (&aci_makevars, name);

	if (vn) {
		beg = aci_strlist_begin (&vn->chunks);
		end = aci_strlist_end (&vn->chunks);

		while (beg != end) {
			fprintf (f, " %s", *beg);
			++beg;
		}
	}
}


// Create a .pc file for pkg-config.
void ac_create_pc_file (const char *libname, const char *desc)
{
	sbuf_t sb;
	FILE *f;

	sbufinit (&sb);
	sbufcpy (&sb, libname);
	sbufcat (&sb, ".pc");

	f = fopen (sbufchars(&sb), "w");
	sbuffree (&sb);
	if (f == NULL) {
		return;
	}

	fprintf (f, "Name: %s\n", libname);
	fprintf (f, "Version: None\n");
	fprintf (f, "Description: %s\n", desc);

	// Dump Cflags:
	fprintf (f, "Cflags: ");
	aci_varnode_dump ("EXTRA_CFLAGS", f);
//  aci_varnode_dump ("GCC_STD", f);
	fprintf (f, "\n");
	fprintf (f, "Libs: -L%slib -l%s\n", aci_install_prefix, libname);

	fprintf (f, "Requires.private: ");
	aci_dump_strlist (&aci_pkg_config_packs, f, 0);
	fprintf (f, "\n");

	fprintf (f, "Libs.private: ");
	aci_varnode_dump ("EXTRALIBS", f);
	fprintf (f, "\n");

	fclose (f);
}



// Finish everything.
void ac_finish (void)
{
	sbuffree (&aci_include_dirs);
	sbuffree (&aci_lib_dirs);
	sbuffree (&aci_extra_cflags);
	sbuffree (&aci_extra_ldflags);
	sbuffree (&aci_additional_libs);
	sbuffree (&aci_testing_flags);

	aci_strlist_destroy (&aci_tdefs);
	aci_strlist_destroy (&aci_pkg_config_packs);
	aci_varlist_destroy (&aci_makevars);
	aci_varlist_destroy (&aci_features);

	aci_strlist_destroy (&aci_given_options);
	aci_strlist_destroy (&aci_valid_options_desc);
	aci_strlist_destroy (&aci_valid_options);

	if (!aci_keep) {
		printf ("removing temporary files...\n");
		aci_cleanup ();
	}

	sbuffree (&stdint_proxy);
	sbuffree (&aci_common_headers);

	aci_flag_list_free (aci_flags_root);


	if (aci_warn_makevars) {
		printf ("No pelconf.var file has been found... using defaults.\n");
	}
	printf ("all done\n");
}


// See if pkg-config is available.
int ac_has_pkg_config (void)
{
	return aci_run_silent ("pkg-config --help") == 0;
}


typedef enum { pkgconf_cflags, pkgconf_libs } pkgconf_flags;


// Get the flags as given by pkg-config.
int ac_pkg_config_flags (const char *s, char *buf, size_t n, pkgconf_flags what)
{
	 sbuf_t sb;
	 int    err;

	 *buf = 0;
	 sbufinit (&sb);

	 sbufcpy (&sb, "pkg-config ");

	 if (what == pkgconf_cflags) {
		  sbufcat (&sb, "--cflags");
	 } else if (what == pkgconf_libs) {
		  sbufcat (&sb, "--libs");
		  if (aci_have_woe32) {
			   sbufcat (&sb, " --static");
		  }
	 }

	 sbufcat (&sb, " ");
	 sbufcat (&sb, s);

	 err = aci_run_silent (sbufchars (&sb));

	 if (err == 0) {
		  FILE *f = fopen ("__dummys1", "r");
		  if (f) {
			   size_t count = fread (buf, 1, n - 1, f);
			   if (count > 0 && buf[count - 1] == '\n') {
					buf[count - 1] = 0;
			   } else {
					buf[count] = 0;
			   }

			   fclose (f);
		  }
	 }

	 sbuffree (&sb);
	 return err;
}


static int aci_pkg_config = 0;
static int aci_pkg_config_checked = 0;



// Check for a function. If package config is available its information will
// be used to deduce the required flags.
int ac_check_func_pkg_config_tag (const char *includes,
        const char *cflags, const char *func, const char *package,
        const char *tag)
{
	int res = 0;
	sbuf_t sb;
	char pcflags[500], libs[500];
	FILE *logfile;

	if (!aci_pkg_config_checked) {
		aci_pkg_config = ac_has_pkg_config ();
		aci_pkg_config_checked = 1;
	}

	if (aci_pkg_config &&
	           ac_pkg_config_flags (package, pcflags, sizeof pcflags, pkgconf_cflags) == 0 &&
	           ac_pkg_config_flags (package, libs, sizeof libs, pkgconf_libs) == 0) {
		sbufinit (&sb);
		sbufcpy (&sb, cflags ? cflags : "");
		sbufcat (&sb, " ");    sbufcat (&sb, pcflags);
		res = ac_check_func_lib_tag (includes, sbufchars (&sb), func, libs, 1, tag);

		logfile = fopen ("configure.log", "a");
		if (logfile) {
			fprintf (logfile, "\nFound package %s in pkg-config: %d\n", package, res);
			fclose (logfile);
		}

		if (res) {
			aci_strlist_add (&aci_pkg_config_packs, package, 1);
		}
		sbuffree (&sb);
	} else {
		res = ac_check_func_lib_tag (includes, cflags, func, package, 0, tag);
#if 0
		if (res) {
			aci_strlist_add (&aci_pkg_config_packs, package, 1);
		}
#endif
	}
	return res;
}


int ac_check_func_pkg_config (const char *includes, const char *cflags,
                    const char *func, const char *package)
{
	char tag[BUFSIZE];

	aci_identcopy (tag, sizeof tag, func);
	return ac_check_func_pkg_config_tag (includes, cflags, func, package, tag);
}


int ac_check_member_pkg_config_tag (const char *includes, const char *cflags,
                                    const char *func, const char *package,
                                    const char *tag)
{
	int res = 0;
	sbuf_t sb;
	char pcflags[500], libs[500];
	FILE *logfile;

	if (!aci_pkg_config_checked) {
		aci_pkg_config = ac_has_pkg_config ();
		aci_pkg_config_checked = 1;
	}

	if (aci_pkg_config &&
	           ac_pkg_config_flags (package, pcflags, sizeof pcflags, pkgconf_cflags) == 0 &&
	           ac_pkg_config_flags (package, libs, sizeof libs, pkgconf_libs) == 0) {
		sbufinit (&sb);
		sbufcpy (&sb, cflags ? cflags : "");
		sbufcat (&sb, " ");    sbufcat (&sb, pcflags);
		res = ac_check_member_lib_tag (includes, sbufchars (&sb), func, libs, 1, tag);
		
		logfile = fopen ("configure.log", "a");
		if (logfile) {
			fprintf (logfile, "\nFound package %s in pkg-config: %d\n", package, res);
			fclose (logfile);
		}
		if (res) {
			aci_strlist_add (&aci_pkg_config_packs, package, 1);
		}
		sbuffree (&sb);
	} else {
		res = ac_check_member_lib_tag (includes, cflags, func, package, 0, tag);
#if 0
		if (res) {
			aci_strlist_add (&aci_pkg_config_packs, package, 1);
		}
#endif
	}
	return res;
}


int ac_check_member_pkg_config (const char *includes, const char *cflags,
                                const char *func, const char *package)
{
	char tag[BUFSIZE];

	aci_identcopy (tag, sizeof tag, func);
	return ac_check_member_pkg_config_tag (includes, cflags, func, package, tag);
}



