/*
 * Compute dependencies of programs for use in makefiles.
 * Copyright (C) 2009-2018 P. Bernedo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http:/www.gnu.org/licenses/>.
 */


#include <stddef.h>
#include <string>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <typeinfo>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

// For getcwd()
#include <unistd.h>

static std::list<std::string>  search_dirs;
static bool verbose = false;
static bool trace = false;
static bool show_defs = false;
static std::string object_ext (".o");
static std::string exe_ext ("");
static std::string lib_prefix ("lib");
static std::string lib_suffix (".so.$(SONAME)");
static std::string ar_suffix (".a");
static std::string makefile_name ("makefile");
static std::string header_prefix;
typedef std::unordered_set<std::string> String_set_type;
static String_set_type object_dirs;
static String_set_type abis;
static bool append = false;
static bool precomp_headers = false;
static bool potdeps = false;

// Current directory.
static std::string gcwd;

static int add_search_dir (const char *dir)
{
	search_dirs.push_back (std::string(dir));
	return 0;
}


static int add_object_dir (const char *dir)
{
	std::string s (dir);
	object_dirs.insert (s);
	return 0;
}

static int add_abi (const char *abi)
{
	std::string s(abi);
	abis.insert (s);
	return 0;
}


template <class T, size_t N>
constexpr size_t countof(T (&)[N])
{
	return N;
}


static const char * get_base_name (const char *s)
{
	const char *cp = s;
	const char *dirsep = NULL;

	while (*cp) {
		if (*cp == '/' || *cp == '\\') {
			dirsep = cp;
		}
		++cp;
	}
	if (dirsep) {
		return dirsep + 1;
	} else {
		return s;
	}
}


static bool file_exists (const char *name)
{
	std::ifstream is(name);
	return is.good();
}

static const char dirsep = '/';


static void convert_to_fwd_slash (std::string *sb)
{
	char *i, *e;

	i = &(*sb)[0];;
	e = i + sb->size();

	if (i != e && (i + 1 != e) && i[1] == ':') {
		*i = (char) tolower (*i);
	}

	while (i != e) {
		if (*i == '\\') {
			*i = '/';
		}
		++i;
	}
}



static int try_full_cwd (std::string *sb)
{
	size_t n = 100;
	const char *res;

	n = std::max (n, sb->capacity ());

	do {
		sb->clear ();
		sb->resize (n);
		errno = 0;
		res = getcwd (&(*sb)[0], (int)n);
		n <<= 1;
	} while (res == NULL && errno == ERANGE);

	if (res == NULL) {
		sb->clear ();
	} else {
		sb->resize (strlen (sb->c_str()));
	}

	convert_to_fwd_slash (sb);
	return res == NULL;
}


static void full_cwd (std::string *sb)
{
	if (try_full_cwd (sb) != 0) {
		throw std::runtime_error ("Failure getting cwd.");
	}
}

static char *
last_sep_before (char *start, char *limit)
{
	--limit;
	while (limit > start && *limit != dirsep) {
		--limit;
	}
	if (limit < start) {
		return start;
	} else {
		return limit;
	}
}



static void normalize_path (std::string *sb)
{
	char *cr, *cw, *beg, *end;

	beg = &(*sb)[0];
	end = beg + sb->size();

	cr = cw = beg;

	while (cr != end) {
		if (*cr == '.') {
			if (cr + 1 != end && cr[1] == dirsep) {
				cr += 2;
				continue;
			} else if (cr + 1 != end && cr[1] == '.'
					   && cr + 2 != end && cr[2] == dirsep) {
				char *sep = last_sep_before (beg, cw - 1);
				if (*sep == dirsep && strncmp (sep, "/../", 4) != 0 ) {
					cw = sep + 1;
				} else if (cw != beg && cw[-1] == dirsep && strncmp(beg, "../", 3) != 0) {
					cw = beg;
				} else {
					*cw++ = '.';
					*cw++ = '.';
					*cw++ = dirsep;
				}
				cr += 3;
				continue;
			}
		}
		while (cr != end && *cr != dirsep) {
			*cw++ = *cr++;
		}
		if (cr != end) {
			*cw++ = *cr++;
			if (*cr == dirsep) {
				++cr;
			}
		}
	}
	sb->resize (size_t(cw - beg));

	if (sb->back() == '/') {
		sb->resize (sb->size() - 1);
	}
}

static bool path_is_absolute (const char *s)
{
	char drive = (char)tolower (*s);
	return *s == dirsep || (('a' <= drive && drive <= 'z') && s[1] == ':');
}


static int try_merge_paths (std::string *dest, const char *head, const char *tail)
{
	if (path_is_absolute (tail)) {
		return -1;
	} else {
		*dest = head;
		char dsep[2] = { dirsep, 0 };
		*dest += dsep;
		*dest += tail;
		normalize_path (dest);
		return 0;
	}
}


static void normalize_to_cwd (std::string *p)
{
	normalize_path (p);
	if (p->compare (0, 3, "../") == 0) {
		std::string cwd (gcwd);
		std::string res;
		if (try_merge_paths (&res, cwd.c_str(), p->c_str()) == 0) {
			if (res.size() > cwd.size() && res.compare (0, cwd.size(), cwd) == 0) {
				*p = res.substr (cwd.size() + 1);
			}
		}
	}
}


static void shrink_to_dir (std::string *sb)
{
   char *cp, *last_sep, *beg, *end;

	cp = beg = &(*sb)[0];
	last_sep = end = beg + sb->size();

	while (cp != end) {
		if (*cp == '/') {
			last_sep = cp;
		} else if (*cp == '\\') {
			last_sep = cp;
			*cp = '/';
		}
		++cp;
	}

	if (last_sep == end) {
		sb->resize (0);
	} else {
		*last_sep = 0;
		sb->resize (size_t(last_sep - beg));
	}
}



enum Target_type { Not_target, Main_target, Lib_target };

class State {
	String_set_type  deps;
	String_set_type  defines;
	Target_type      target;
	String_set_type  already_seen;

	void process_line (std::istream &is, const char *line, const char *parent);
	void process_include (const char *name);
	void process_ifdef (std::istream &is, const char *line, bool pos);
	bool locate_file (const char *fn, std::string &expanded, const char *parent);
public:
	State () : target (Not_target) {}
	void add_dependency (const std::string &def);
	void add_define (const char *def);
	void scan_source_file (std::istream &is, const char *name);
	void show_info (const char *fn);

	Target_type get_target() const { return target; }

	const String_set_type & get_deps () const { return deps; }
};


// List with the names of the library header variables of the form DEP_xxx
static std::string full_lib_headers;



static const char * skip_space (const char *s)
{
	wchar_t wc;
	while (*s) {
		ptrdiff_t nb = mbtowc(&wc, s, 4);
		if (nb > 0) {
			if (!iswspace(wc)) return s;
			s += nb;
		} else {
			return s;
		}
	}
	return s;
}


static const char * skip_word (const char *s)
{
	wchar_t wc;
	while (*s) {
		ptrdiff_t nb = mbtowc(&wc, s, 4);
		if (nb > 0) {
			if (wc != '_' && !iswalnum(wc)) return s;
			s += nb;
		} else {
			return s;
		}
	}
	return s;
}


static bool starts_with (const std::string &s, const std::string &pfx)
{
	return s.compare (0, pfx.size(), pfx) == 0;
}

void State::add_dependency (const std::string &s)
{
	deps.insert (s);
}


void State::add_define (const char *def)
{
	const char *cp = def;

	cp = skip_word (cp);
	if (cp != def) --cp;
	std::string r (def, cp - def);
	std::pair<String_set_type::iterator, bool> res = defines.insert (r);
	if (show_defs && res.second) {
		std::cout << "#defined '" << def << "'\n";
	}
}

void State::scan_source_file (std::istream &is, const char *name)
{
	std::string sb;
	const char *i;

	std::string ns(name);
	std::pair<String_set_type::iterator, bool> res = already_seen.insert(ns);
	if (!res.second) {
		// We have already processed this file.
		return;
	}

	if (trace) {
		std::cout << "scanning " << name << '\n';
	}

	shrink_to_dir (&ns);
	if (ns.empty()) {
		ns = ".";
	}
	while (!is.eof()) {
		getline (is, sb);
		i = sb.c_str ();
		i = skip_space (i);

		if (*i == '#') {
			process_line (is, skip_space (i + 1), ns.c_str());
		} else if (strncmp (i, "int main", 8) == 0 ||
		           strncmp (i, "main", 4) == 0) {
			target = Main_target;
		} else if (strncmp (i, "/* LIBRARY */", 13) == 0) {
			target = Lib_target;
		}
	}
}


void State::process_line (std::istream &is, const char *def, const char *parent_dir)
{
	if (strncmp (def, "include", 7) == 0) {
		const char *dep_name = skip_space (def + 7);
		std::string expanded;
		if (locate_file (dep_name, expanded, parent_dir)) {
			add_dependency (expanded);
			process_include (expanded.c_str());
		}
	} else if (strncmp (def, "define", 6) == 0) {
		add_define (skip_space (def + 6));
	} else if (strncmp (def, "ifdef", 5) == 0) {
		process_ifdef (is, skip_space (def + 5), true);
	} else if (strncmp (def, "ifndef", 6) == 0) {
		process_ifdef (is, skip_space (def + 6), false);
	} else if (strncmp (def, "if defined(", 11) == 0) {
		process_ifdef (is, skip_space (def + 11), true);
	} else if (strncmp (def, "if !defined(", 12) == 0) {
		process_ifdef (is, skip_space (def + 12), false);
	}
}


static std::string trim_spaces (std::string &s)
{
	const char *from = skip_space (s.c_str());
	const char *last = from;
	const char *cp = last;
	while (*cp) {
		if (!isspace(*cp)) last = cp;
		++cp;
	}
	return std::string (from, last - from + 1);
}


struct Dir_pusher {
	bool active;
	Dir_pusher() : active(false) {}
	void push (const char *dir) {
		active = true;
		std::string dirs (dir);
		search_dirs.push_front (dirs);
	}
	~Dir_pusher() {
		if (active) {
			search_dirs.pop_front();
		}
	}
};

bool State::locate_file (const char *name, std::string &expanded, const char *parent_dir)
{
	if (*name != '"' && *name != '<') {
		return false;
	}
	bool local = *name == '"';

	std::string stripped = name + 1;
	if (stripped.empty()) {
		return false;
	}

	stripped = trim_spaces (stripped);
	if (stripped.back() == '"' || stripped.back () == '>') {
		stripped.resize (stripped.size() - 1);
	}

	std::string cwd (gcwd);

	std::list<std::string>::iterator i, e;

	Dir_pusher dp;
	if (local) {
		dp.push (parent_dir);
	}

	i = search_dirs.begin ();
	e = search_dirs.end ();

	while (i != e) {
		if (*i != ".") {
			expanded = *i;
			expanded += '/';
		}
		expanded += stripped;

		std::ifstream is (expanded.c_str());
		if (is.good()) {
			if (expanded.size() > 3 && expanded.compare (0, 3, "../") == 0) {
				normalize_to_cwd (&expanded);
			}
			if (expanded.size() > cwd.size() && expanded.compare (0, cwd.size(), cwd) == 0 && expanded[cwd.size()] == '/') {
				expanded.erase (0, cwd.size() + 1);
			}
			return true;
		}
		++i;
	}
	return false;
}


void State::process_include (const char *name)
{
	std::ifstream is (name);
	if (is.good()) {
		scan_source_file (is, name);
	}
	return;
}


template <class C>
void show_container (const C &cont, const char *sep = "\n", std::ostream &os=std::cout)
{
	bool started = false;
	typename C::const_iterator i, e;

	i = cont.begin ();
	e = cont.end ();

	while (i != e) {
		if (started) {
			os << sep;
		}
		os << *i;
		started = true;
		++i;
	}
	os << "\n\n";
}


void State::show_info (const char *fn)
{
	std::cout << fn << ": ";
	show_container (deps, "\\\n    ");
	std::cout << "\n\n";

	if (show_defs) {
		std::cout << "defines:\n";
		show_container (defines);
	}
}


void State::process_ifdef (std::istream &is, const char *line, bool pos)
{
	const char *eow;

	eow = skip_word (line);
	if (eow != line) --eow;
	std::string needle(line, eow - line);
	bool have_it = defines.find (needle) != defines.end();

	if (needle != "0") {
		if (pos && have_it) return;
		if (!pos && !have_it) return;
	}

	int nesting = 1;
	std::string sb;
	while (!is.eof()) {
		getline (is, sb);
		const char *i = sb.c_str ();
		i = skip_space (i);

		if (strncmp (i, "#if", 3) == 0) {
			nesting++;
		} else if (nesting == 1 && strncmp (i, "#else", 5) == 0) {
			return;
		} else if (strncmp (i, "#endif", 6) == 0) {
			--nesting;
			if (nesting == 0) {
				return;
			}
		}
	}
}


struct Source_file {
	std::string name, full_name;
	String_set_type deps, deps_full_path;
	Target_type target;
	mutable int depcount;
};

inline bool operator < (const Source_file &lhs, const Source_file &rhs)
{
	return lhs.name < rhs.name;
}

inline bool operator== (const Source_file &lhs, const Source_file &rhs)
{
	 return lhs.name == rhs.name;
}

namespace std {
template <> class hash<Source_file> {
public:
	 size_t operator() (const Source_file &sf) const { return hash<std::string>()(sf.name); }
};

}


class Project {
	// An unordered_set may be faster, but the resulting makefile will have
	// the entries in an arbitrary order. Using the ordered set we can
	// output the makefile ordered alphabetically.
	typedef std::set<Source_file> Source_file_set_type;
	Source_file_set_type files;
	typedef Source_file_set_type Mains_type;
	typedef String_set_type::const_iterator Listit;
	Mains_type mains;
	String_set_type given_files, given_files_full;
	void list_deps (const String_set_type &target,
	                const std::string &ext, const std::string &dir,
	                std::string *res) const;
	void list_pots (const String_set_type &target, std::string *res) const;

	std::string curdir;
	void clean_file_name (const char *name, std::string *res) const;

public:
	Project ();
	template <class Cont>
	void add (const char *name, const Cont &src, Target_type target);
	void dump_single_deps (std::ostream &os) const;
	void add_given_file (const char *file);

	void compute_mains ();
	void dump_main_progs (std::ostream &os) const;
	void show_levels ();
	void compute_and_show_libs () const;
	void compute_target_pchs (std::ostream &os) const;
};


Project::Project ()
{
	 full_cwd (&curdir);
}


static void replace_extension (std::string &s, const char *newext)
{
	size_t pos = s.rfind ('.');
	if (pos != s.npos) {
		size_t count = s.size() - pos;
		s.replace (pos, count, newext);
	}
}


void Project::add_given_file (const char *fn)
{
	const char *cp = fn;
	const char *dirsep = NULL;

	while (*cp) {
		if (*cp == '/' || *cp == '\\') {
			dirsep = cp;
		}
		++cp;
	}
	std::string tmp;
	if (dirsep) {
		tmp = dirsep + 1;
	} else {
		tmp = fn;
	}

	replace_extension (tmp, "");
	given_files.insert (tmp);
	given_files_full.insert (fn);
}



template <class Cont>
void Project::add (const char *name, const Cont &cont, Target_type target)
{
	Source_file src;
	std::string chopped = name;
	replace_extension (chopped, "");
	src.name = get_base_name (chopped.c_str());
	src.target = target;
	src.full_name = name;

	typename Cont::const_iterator beg = cont.begin ();
	typename Cont::const_iterator end = cont.end ();

	while (beg != end) {
		src.deps.insert (*beg);
		++beg;
	}

	files.insert (src);
}


template <class T>
inline T & noconst(const T&t) {
	return const_cast<T&>(t);
}


static void cat_and_wrap (std::string *dest, const std::string &s)
{
	const char *cp = &(*dest)[dest->size()];
	const char *lim = &(*dest)[0];

	do {
		--cp;
		if (*cp == '\n') {
			break;
		}
	} while (cp > lim);

	int lnlen = &(*dest)[dest->size()] - cp;
	if (lnlen + s.size() > 72) {
		dest->append ("  \\\n   ");
	}
	dest->append (" ");
	dest->append (s);
}


void Project::dump_single_deps (std::ostream &os) const
{
	Source_file_set_type::const_iterator i = files.begin ();
	Source_file_set_type::const_iterator e = files.end ();

	static const char pch_incl[] = "-incls.hpp";
	static const char pch_suffix[] = ".gch";
	std::string pchs;

	while (i != e) {
		pchs.clear ();
		size_t count = 0, item_len;
		if (object_dirs.empty ()) {
			if (i->name == "precompiled") {
				count = ::strlen ("precompiled.hpp.gch") + 1;
				os << "precompiled.hpp.gch ";
			} else {
				count = i->name.size() + object_ext.size() + 1;
				os << i->name << object_ext << " ";
				auto abib = abis.begin();
				auto abie = abis.end();
				while (abib != abie) {
					item_len = i->name.size() + abib->size() + 2;
					if (count + item_len > 80) {
						os << "\\\n    ";
						count = 0;
					}
					os << i->name << "-" << *abib << object_ext << " ";
					count += item_len;
					++abib;
				}
			}
			os << ": ";
		} else {
			String_set_type::const_iterator odb, ode;
			odb = object_dirs.begin ();
			ode = object_dirs.end ();
			count = 0;
			while (odb != ode) {
				if (i->name == "precompiled") {
					os << *odb << "/precompiled.hpp.gch ";
				} else {
					if (precomp_headers) {
						os << *odb << "/" << i->name << object_ext << ": " << *odb
							<< "/" << i->name << pch_incl << pch_suffix << '\n';
						pchs.append (*odb).append ("/").append (i->name);
						pchs.append (pch_incl).append (pch_suffix).append (" ");
						auto abib = abis.begin();
						auto abie = abis.end();
						while (abib != abie) {
							os << *odb << "/" << i->name << "-" << *abib << ": "
								<< *odb << "/" << i->name << pch_incl << "-"
								<< *abib << pch_suffix << '\n';
							pchs.append (*odb).append ("/").append (i->name).append (pch_incl);
							pchs.append("-").append(*abib).append (pch_suffix).append (" ");
							++abib;
						}
					} else {
						item_len = odb->size() + 1 + i->name.size() + object_ext.size() + 1;
						if (count + item_len > 80) {
							os << "\\\n     ";
							count = 0;
						}
						os << *odb << "/" << i->name << object_ext << " ";
						count += item_len;
						auto abib = abis.begin();
						auto abie = abis.end();
						while (abib != abie) {
							item_len = odb->size() + 1 + i->name.size() + 1 + abib->size() + object_ext.size() + 1;
							if (count + item_len > 80) {
								os << "\\\n    ";
								count = 0;
							}
							os << *odb << "/" << i->name << "-" << *abib << object_ext << " ";
							count += item_len;

							++abib;
						}
					}
				}
				++odb;
			}
			if (precomp_headers) {
				os << pchs;
				count += pchs.size();
			}
			os << ": ";
			count += 2;
		}


		std::ofstream pch;
		if (precomp_headers) {
			std::string pch_name;
			pch_name = i->name + pch_incl;
			pch.open (pch_name.c_str());
		}

		item_len = i->full_name.size() + 1;
		if (count + item_len > 80) {
			os << "\\\n    ";
			count = 0;
		}
		os << i->full_name << " ";
		count += item_len;
		String_set_type::const_iterator di, de;
		di = i->deps.begin ();
		de = i->deps.end ();

		while (di != de) {
			std::string tmp;
			clean_file_name (di->c_str(), &tmp);
			size_t delta = tmp.size() + header_prefix.size();
			if (count + delta > 80) {
				os << "\\\n    ";
				count = 4;
			}
			os << header_prefix << tmp << "  ";
			if (pch.good()) {
				 pch << "#include \"" << header_prefix << tmp << "\"\n";
			}
			count += delta + 2;
			++di;
		}

		os << "\n\n";

		if (i->target == Lib_target) {
			std::string tmp;
			count = 0;
			clean_file_name (i->name.c_str(), &tmp);
			os << "DEPS_" << tmp << " = ";
			full_lib_headers += "$(DEPS_" + tmp + ") ";
			di = i->deps.begin();
			de = i->deps.end();
			while (di != de) {
				clean_file_name (di->c_str(), &tmp);
				size_t delta = tmp.size() + header_prefix.size();
				if (count + delta > 80) {
					os << "\\\n    ";
					count = 4;
				}
				os << header_prefix << tmp << "  ";
				count += delta + 2;
				++di;
			}
			os << "\n\n";
		}


		++i;
	}
}


static bool add_deps (String_set_type &target,
                      const String_set_type &src,
                      const String_set_type &filter)
{
	bool changed = false;
	String_set_type::const_iterator i = src.begin();
	String_set_type::const_iterator e = src.end();

	while (i != e) {
		std::string chopped = *i;
		replace_extension (chopped, "");
		std::string base = get_base_name (chopped.c_str());

		String_set_type::const_iterator fr = filter.find(base), fe = filter.end();

		if (fr != fe) {
			if (target.insert (base).second) {
				changed = true;
			}
		}
		++i;
	}
	return changed;
}

static bool add_single_dep (String_set_type &target, const char *dep, const String_set_type &filter, bool literal=true)
{
	std::string chopped (dep);
	replace_extension (chopped, "");
	std::string base = get_base_name (chopped.c_str());

	for (auto fi = filter.begin(); fi != filter.end(); ++fi) {
		std::string tmp = *fi;
		replace_extension (tmp, "");
		std::string fbase = get_base_name (tmp.c_str());
		if (fbase == base) {
			if (target.insert (*fi).second) {
				if (literal) {
					target.insert (dep);
				}
				return true;
				break;
			}
		}
	}
	return false;
}

static bool add_full_deps (String_set_type &target,
                           const String_set_type &src,
                           const String_set_type &filter)
{
	bool changed = false;
	String_set_type::const_iterator i = src.begin();
	String_set_type::const_iterator e = src.end();

	while (i != e) {
		changed = add_single_dep (target, i->c_str(), filter);
		++i;
	}
	return changed;
}


void Project::compute_mains ()
{
	Source_file_set_type::iterator i = files.begin ();
	Source_file_set_type::iterator e = files.end ();

	std::string target_name;

	for (; i != e; ++i) {
		Source_file src;
		src.name = i->name;
		src.target = i->target;
		add_deps (src.deps, i->deps, given_files);
		add_full_deps (src.deps_full_path, i->deps, given_files_full);
		if (src.target == Main_target) {
			src.deps.insert (src.name);
			add_single_dep (src.deps_full_path, src.name.c_str(), given_files_full, false);
		}
		mains.insert (src);
	}

	bool changed;
	Mains_type::iterator mi = mains.begin();
	Mains_type::iterator me = mains.end();

	do {
		changed = false;
		mi = mains.begin();

		while (mi != me) {
		  restart:
			Listit li = mi->deps.begin();
			Listit le = mi->deps.end();

			while (li != le) {
				i = files.begin ();
				while (i != e) {
					if (get_base_name(li->c_str()) == i->name) {
						// std::set's iterators are const.
						String_set_type &depsref (noconst(mi->deps));
						if (add_deps (depsref, i->deps, given_files)) {
							changed = true;
							goto restart;
						}
					}
					++i;
				}
				++li;
			}
			++mi;
		}
	} while (changed);
}


void Project::list_deps (const String_set_type &target,
                         const std::string &ext, const std::string &dir,
                         std::string *res) const
{
	Listit li = target.begin ();
	Listit le = target.end ();

	res->clear();
	std::string tmp;

	while (li != le) {
		if (dir.empty()) {
			tmp = *li + ext;
		} else {
			tmp = dir + "/" + *li + ext;
		}
		cat_and_wrap (res, tmp);
		++li;
	}
}

void Project::list_pots (const String_set_type &target, std::string *res) const
{
	Listit li = target.begin ();
	Listit le = target.end ();

	res->clear();

	while (li != le) {
		cat_and_wrap (res, *li);
		++li;
	}
}


void Project::dump_main_progs (std::ostream &os) const
{
	os << "# Main programs\n";

	std::string full_targets;
	std::string objs, tmp;

	if (object_dirs.empty ()) {
		object_dirs.insert (std::string("."));
	}
	String_set_type::const_iterator db, de;
	db = object_dirs.begin ();
	de = object_dirs.end ();

	std::string empty;

	for (; db != de; ++db) {
		Mains_type::const_iterator i = mains.begin ();
		Mains_type::const_iterator e = mains.end ();

		for (; i != e; ++i) {
			if (i->target == Not_target) {
				continue;
			}
			std::string pfx;
			if (*db != ".") pfx = *db;
			list_deps (i->deps, object_ext, pfx, &objs);

			if (i->target == Main_target) {
				if (*db != ".") {
					tmp = *db + "/";
				} else {
					tmp.clear();
				}
				tmp += i->name + exe_ext;
				cat_and_wrap (&full_targets, tmp);
				os << tmp << ": \\\n   " << objs << "\n\n";

				auto abib = abis.begin();
				auto abie = abis.end();
				while (abib != abie) {
					if (*db != ".") {
						os << *db << "/";
					}
					os << i->name << "-" << *abib << exe_ext << ": \\\n   ";
					std::string abi_ext = "-";
					abi_ext.append (*abib);
					abi_ext.append (object_ext);
					list_deps (i->deps, abi_ext, pfx, &objs);
					os << objs << "\n\n";
					++abib;
				}
		   } else {
				if (*db == ".") {
					tmp.clear();
				} else {
					tmp = *db + "/";
				}
				if (starts_with (i->name, lib_prefix)) {
					tmp += i->name + ar_suffix;
				} else {
					tmp += lib_prefix + i->name + ar_suffix;
				}
				cat_and_wrap (&full_targets, tmp);
				os << tmp << ": \\\n   " << objs << "\n\n";

				bool pic_found = false;
				auto abib = abis.begin();
				auto abie = abis.end();
				while (abib != abie) {
					std::string abiext("-");
					abiext += *abib + object_ext;
					list_deps (i->deps, abiext, pfx, &objs);
					if (starts_with (i->name, lib_prefix)) {
						tmp = *db + "/";
					} else {
						tmp = *db + "/" + lib_prefix;
					}
					if (*abib == "pic") {
						pic_found = true;
						tmp += i->name + lib_suffix;
					} else {
						tmp += i->name + "-" + *abib + lib_suffix;
					}
					cat_and_wrap (&full_targets, tmp);

					os << tmp << ": \\\n   " << objs << "\n\n";
					++abib;
				}
				if (!pic_found) {
					if (*db == ".") {
						tmp.clear();
					} else {
						tmp = *db + "/";
					}
					if (starts_with (i->name, lib_prefix)) {
						tmp += i->name + lib_suffix;
					} else {
						tmp += lib_prefix + i->name + lib_suffix;
					}
					cat_and_wrap (&full_targets, tmp);
					os << tmp << ": \\\n   " << objs << "\n\n";
				}
			}

		}
	}

	if (potdeps) {
		Mains_type::const_iterator i = mains.begin ();
		Mains_type::const_iterator e = mains.end ();

		for (; i != e; ++i) {
			if (i->target == Not_target) {
				continue;
			}

			os << "pot/" << i->name << ".pot: \\\n   ";
			list_pots (i->deps_full_path, &objs);
			os << objs << "\n\n";
		}
	}

	os << "FULL_TARGETS = " << full_targets << '\n';
	os << "full_targets: " << "$(FULL_TARGETS)\n";
	os << "FULL_LIB_HEADERS = " << full_lib_headers << '\n';
}




void Project::compute_target_pchs (std::ostream &os) const
{
	Mains_type::const_iterator i = mains.begin ();
	Mains_type::const_iterator e = mains.end ();

	for (; i != e; ++i) {
		if (i->target == Not_target) {
			continue;
		}

		if (object_dirs.empty ()) {
			object_dirs.insert (std::string("."));
		}

		String_set_type::const_iterator oi, oe;
		oi = object_dirs.begin ();
		oe = object_dirs.end ();

		while (oi != oe) {
			os << *oi << "/" << i->name << "-precomp.hpp.gch ";
			auto abib = abis.begin();
			auto abie = abis.end();
			while (abib != abie) {
				os << *oi << "/" << i->name << "-precomp.hpp-" << *abib << ".gch ";
				++abib;
			}
			++oi;
		}
		os << ":\\\n   ";

		String_set_type::const_iterator di = i->deps.begin ();
		String_set_type::const_iterator de = i->deps.end ();

		String_set_type headers;
		std::string tmp;

		while (di != de) {
			Source_file sf;
			sf.name = *di;
			Source_file_set_type::const_iterator obj = files.find (sf);
			if (obj != files.end()) {
				String_set_type::const_iterator hi = obj->deps.begin();
				String_set_type::const_iterator he = obj->deps.end();
				while (hi != he) {
					clean_file_name (hi->c_str(), &tmp);
					headers.insert (tmp);
					++hi;
				}
			}
			++di;
		}
		String_set_type::const_iterator hi = headers.cbegin();
		String_set_type::const_iterator he = headers.cend();
		std::string formatted;

		tmp = i->name + "-precomp.hpp";
		std::ofstream preh (tmp.c_str());

		while (hi != he) {
			cat_and_wrap (&formatted, *hi);
			if (preh) {
				preh << "#include \"" << *hi << "\"\n";
			}
			++hi;
		}
		os << formatted << "\n\n";
	}
}

static void merge_paths (std::string *dest, const char *head, const char *tail)
{
	if (try_merge_paths (dest, head, tail) != 0) {
		throw std::runtime_error ("Merge paths failed.");
	}
}



void Project::clean_file_name (const char *name, std::string *res) const
{
	 if (!path_is_absolute (name)) {
		  merge_paths (res, curdir.c_str(), name);
	 } else {
		  *res = name;
	 }

	 if ((*res)[1] == ':') {
		  res->erase (0, 2);   // Remove colons from absolute windows paths.
	 }

	 if (strncmp (res->c_str(), curdir.c_str(), curdir.size()) == 0) {
		  res->erase (0, curdir.size() + 1);
	 } else if (res->size() > ::strlen (name)) {
		  *res = name;
	 }
}


void Project::show_levels ()
{
	Mains_type::const_iterator i = mains.begin ();
	Mains_type::const_iterator e = mains.end ();

	int ccd = 0;
	int ncomps = 0;

	for (; i != e; ++i) {
		Listit li = i->deps.begin ();
		Listit le = i->deps.end ();
		i->depcount = 1;
		while (li != le) {
			i->depcount++;
			++li;
		}
		ccd += i->depcount;
		++ncomps;
	}

	int n1 = ncomps + 1;
	double nccd = n1*( log(n1)/log(2.0) - 1 ) + 1;
	nccd = ccd/nccd;

	std::cout << "components=" << ncomps << "   ccd=" << ccd
			<< "  acd=" << double(ccd)/ncomps
			<< "  nccd=" << nccd << '\n';

	bool pending;
	int current_level = 1;
	do {
		pending = false;
		bool label = false;
		for (i = mains.begin (); i != e; ++i) {
			if (i->depcount == current_level) {
				if (!label) {
					std::cout << "\nlevel " << current_level << ": " << std::flush;
					label = true;
				}
				std::cout << i->name << " ";
			} else if (i->depcount > current_level) {
				pending = true;
			}
		}
		if (label) {
			std::cout << '\n';
		}
		++current_level;
	} while (pending);
	std::cout << '\n';
}


static void chop_last (std::string *s)
{
	size_t n = s->size();

	if (n > 0) {
		if ((*s)[n] == '/') {
			s->resize (n - 1);
		}
	}
}




void Project::compute_and_show_libs () const
{
	String_set_type udeps;
	Source_file_set_type::const_iterator b = files.begin ();
	Source_file_set_type::const_iterator e = files.end ();
	std::string cwd (gcwd), merged;

	//full_cwd (&cwd);
	chop_last (&cwd);

	while (b != e) {
		String_set_type::const_iterator db = b->deps.begin ();
		String_set_type::const_iterator de = b->deps.end ();

		while (db != de) {
			std::string s = *db;
			normalize_path (&s);
			shrink_to_dir (&s);
			chop_last (&s);
			if (try_merge_paths (&merged, cwd.c_str(), s.c_str()) == 0) {
				if (merged != cwd) {
					udeps.insert (s);
				}
			} else {
				if (s != cwd) {
					udeps.insert (s);
				}
			}
			++db;
		}
		++b;
	}

	std::cout << "potential libraries used (based on included files):\n";
	show_container (udeps);
}




static Project project;


static void process_file (const char *fn)
{
	State state;
	std::ifstream is (fn);

	if (!is) return;

	state.scan_source_file (is, fn);
	project.add (fn, state.get_deps(), state.get_target());
}

static const char needle[] = "# Generated automatically. Do not edit beyond here.";

static void transfer_header (const std::string &src, std::ostream &dest)
{
	std::ifstream is(src.c_str());

	if (!is.good()) {
		return;
	}

	std::string ln;

	while (getline (is, ln)) {
		if (!append && ln == needle) {
			break;
		}
		dest << ln << '\n';
//      sscanf (ln.c_str(), "LIBNAMEMACRO = %s", libmacroname);
	}
}

static
void remove_arg(int *argc, char **argv, int i)
{
	for (int j = i + 1; j < *argc; ++j) {
		argv[j - 1] = argv[j];
	}
	--*argc;
}

static
int hasopt(int *argcp, char **argv, const char *opts, const char **val)
{
	for (int i = 1; i < *argcp; ++i) {
		if (argv[i][0] != '-' || argv[i][1] == '-') continue;
		char *cp = argv[i] + 1;
		while (*cp) {
			for (const char *op = opts; *op; ++op) {
				if (*op == ':') continue;
				if (*op == *cp) {
					if (op[1] == ':') {
						if (cp[1]) {
							*val = cp + 1;
							*cp = 0;
							if (cp == argv[i] + 1) {
								remove_arg(argcp, argv, i);
							}
							return *op;
						} else if (i + 1 < *argcp) {
							*val = argv[i + 1];
							remove_arg(argcp, argv, i + 1);
							if (cp == argv[i] + 1) {
								remove_arg(argcp, argv, i);
							}
							return *op;
						} else {
							return -1;
						}
					} else {
						int res = *cp;
						do {
							*cp = cp[1];
							++cp;
						} while (*cp);
						if (argv[i][1] == 0) {
							remove_arg(argcp, argv, i);
						}
						return res;
					}
				}
			}
			return -1;
			++cp;
		}
	}
	return 0;
}


// Check if a flag was passed in the command line and remove it from the
// arguments.
static
bool hasopt_long(int *argc, char **argv, const char *longopt)
{
	for (int i = 1; i < *argc; ++i) {
		if (strcmp(argv[i], longopt) == 0) {
			for (int j = i + 1; j < *argc; ++j) {
				argv[j - 1] = argv[j];
			}
			--*argc;
			return true;
		}
	}
	return false;
}


// Check if an option with a value was passed in the command line and remove
// it from the arguments.
static
bool hasopt_long(int *argc, char **argv, const char *longopt, const char **val)
{
	size_t olen = strlen(longopt);

	for (int i = 1; i < *argc; ++i) {
		if (strncmp(argv[i], longopt, olen) == 0) {
			if (argv[i][olen] == '=' || argv[i][olen] == ':') {
				*val = argv[i] + olen + 1;
				for (int j = i + 1; j < *argc; ++j) {
					argv[j - 1] = argv[j];
				}
				--*argc;
				return true;
			} else if (argv[i][olen] == 0) {
				if (i + 1 < *argc) {
					*val = argv[i + 1];
					for (int j = i + 2; j < *argc; ++j) {
						argv[j - 2] = argv[j];
					}
					*argc -= 2;
					return true;
				}
			}
		}
	}
	return false;
}

static
std::string describe (const std::exception &e)
{
	std::string res("Reason: ");
	res += e.what();
	res += '\n';

	try {
		std::rethrow_if_nested (e);
	} catch (std::exception &e) {
		res += describe (e);
	} catch (...) {
		res += "Unknown exception class\n";
	}
	return res;
}

static void show_exception (std::exception &e, bool first=true)
{
	if (first) {
		std::cerr << "The program was interrupted\n";
	}
	std::cerr << "Reason: " << e.what() << '\n';

	try {
		std::rethrow_if_nested (e);
	} catch (std::exception &ne) {
		show_exception (ne, false);
	} catch (...) {
		std::cerr << "Unknown exception has been caught.\n";
	}
}

static void catch_signal (int sig)
{
	switch (sig) {
	case SIGABRT:
		std::cout << "The signal SIGABRT ";
		break;

	case SIGILL:
		std::cout << "The signal SIGILL ";
		break;

	default:
		std::cout << "The signal ";
	}

	std::cout << sig << " was caught. This may be due to overflow if compiled with -ftrapv\n";
	abort();
}

static int run_main (int argc, char **argv, int (*real_main)(int,char**))
{
	signal (SIGABRT, catch_signal);
	signal (SIGILL, catch_signal);

	try {
		return real_main (argc, argv);
	} catch (std::exception &e) {
		show_exception (e);
		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "Some unknown exception was caught.\n";
		return EXIT_FAILURE;
	}
}


static void show_help()
{
	std::cout << "mkdep [options] source_files\n";
	std::cout << "  Scan files for dependencies\n";
	std::cout << "-I <dir>            add a dir to the search path\n";
	std::cout << "-v                  verbose output\n";
	std::cout << "--trace             show file names as they are scanned\n";
	std::cout << "-d                  show defines\n";
	std::cout << "-o <ext>            set the object extension\n";
	std::cout << "-e <ext>            set the exe extension\n";
	std::cout << "--libpfx <prefix>   set the prefix for libraries\n";
	std::cout << "--libsfx <suffix>   set the suffix for shared libraries\n";
	std::cout << "-a <suffix>         set the suffix for static libraries\n";
	std::cout << "-f <makefile>       set the name of the makefile to modify\n";
	std::cout << "--odir <directory>  add an object directory\n";
	std::cout << "--abi <abiname>     add an additional ABI\n";
	std::cout << "--hpfx <prefix>     set the prefix to prepend to header names\n";
	std::cout << "--append            append to makefile instead of modifying\n";
	std::cout << "--pch               use precompiled headers for each file in gcc\n";
	std::cout << "--tch               use precompiled headers for each target in gcc\n";
	std::cout << "--potdeps           generate dependencies for C++ POT files\n";

	std::cout << "It will scan the source files, check the corresponding header files\n";
	std::cout << "and compute the dependencies. It understands #ifdefs.\n";
}


static int real_main (int argc, char **argv)
{
	bool precomp_targets = false;
	add_search_dir (".");

	// We do not change directories. We query it once and avoid repeated
	// calls to getcwd().
	full_cwd (&gcwd);

	const char *val;
	int opt;

	if (hasopt_long (&argc, argv, "--help")) {
		show_help();
		return 0;
	}
	if (hasopt_long (&argc, argv, "--trace")) {
		trace = true;
	}
	if (hasopt_long (&argc, argv, "--libpfx", &val)) {
		lib_prefix = val;
	}
	if (hasopt_long (&argc, argv, "--libsfx", &val)) {
		lib_suffix = val;
	}
	while (hasopt_long (&argc, argv, "--odir", &val)) {
		add_object_dir (val);
	}
	while (hasopt_long (&argc, argv, "--abi", &val)) {
		add_abi (val);
	}
	if (hasopt_long (&argc, argv, "--hpfx", &val)) {
		header_prefix = val;
	}
	if (hasopt_long (&argc, argv, "--append")) {
		append = true;
	}
	if (hasopt_long (&argc, argv, "--pch")) {
		precomp_headers = true;
	}
	if (hasopt_long (&argc, argv, "--tch")) {
		precomp_targets = true;
	}
	if (hasopt_long (&argc, argv, "--potdeps")) {
		potdeps = true;
	}

	while ((opt = hasopt(&argc, argv, "hI:vdo:e:a:f:", &val)) > 0) {
		switch (opt) {
		case 'h':
			show_help();
			return 0;

		case 'I':
			add_search_dir (val);
			break;

		case 'v':
			verbose = true;
			break;

		case 'd':
			show_defs = true;
			break;

		case 'o':
			object_ext = val;
			break;

		case 'e':
			exe_ext = val;
			break;

		case 'a':
			ar_suffix = val;
			break;

		case 'f':
			makefile_name = val;
			break;

		default:
			std::cerr << "Unknown option: " << char(opt) << '\n';
			return -1;
		}
	}

	if (verbose) {
		std::cout << "search path:\n";
		show_container (search_dirs);
	}

	for (int i = 1; i < argc; ++i) {
		process_file (argv[i]);
		project.add_given_file (argv[i]);
	}

	std::string tmp;
	tmp = makefile_name + ".tmp";

	std::ofstream mkfile (tmp.c_str());

	if (!mkfile.good()) {
		std::cerr << "mkdeps: can't open file '" << makefile_name << "'\n";
		return 1;
	}

	transfer_header (makefile_name, mkfile);

	if (!append) {
		mkfile << needle << "\n\n";
	}

	mkfile << "# Object dependencies.\n";
	project.dump_single_deps (mkfile);

	project.compute_and_show_libs ();

	project.compute_mains ();
	project.dump_main_progs (mkfile);

	if (precomp_targets) {
		mkfile << "\n# Precompiled headers.\n";
		project.compute_target_pchs (mkfile);
	}

	if (verbose) {
		project.show_levels ();
	}

	mkfile.close ();

	if (remove (makefile_name.c_str()) != 0) {
		if (file_exists (makefile_name.c_str())) {
			 // Complain if the file exists but we could not rename it!
			 std::cout << "removing '" << makefile_name.c_str() << "' failed\n";
		}
	}
	if (rename (tmp.c_str(), makefile_name.c_str()) != 0) {
		std::cout << "renaming '" << tmp.c_str() << "' to '" << makefile_name.c_str() << "' failed\n";
	}

	return 0;
}


int main (int argc, char **argv)
{
	return run_main (argc, argv, real_main);
}


