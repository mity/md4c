/* cmdline.h: a reentrant version of getopt(). Written 2006 by Brian
 * Raiter. This code is in the public domain.
 */

#ifndef	_cmdline_h_
#define	_cmdline_h_

/* The information specifying a single cmdline option.
 */
typedef struct option {
    char const *name;		/* the option's long name, or "" if none */
    char	chname;		/* a single-char name, or zero if none */
    int		optval;		/* a unique value representing this option */
    int		arg;		/* 0 = no arg, 1 = arg req'd, 2 = optional */
} option;

/* Parse the given cmdline arguments. list is an array of option
 * structs, each entry specifying a valid option. The last struct in
 * the array must have name set to NULL. argc and argv give the
 * cmdline to parse. callback is the function to call for each option
 * and non-option found on the cmdline. data is a pointer that is
 * passed to each invocation of callback. The return value of callback
 * should be zero to continue processing the cmdline, or any other
 * value to abort. The return value of readoptions() is the value
 * returned from the last callback, or zero if no arguments were
 * found, or -1 if an error occurred.
 *
 * When readoptions() encounters a regular cmdline argument (i.e. a
 * non-option argument), callback() is invoked with opt equal to zero
 * and val pointing to the argument. When an option is found,
 * callback() is invoked with opt equal to the optval field in the
 * option struct corresponding to that option, and val points to the
 * option's paramter, or is NULL if the option does not take a
 * parameter. If readoptions() finds an option that does not appear in
 * the list of valid options, callback() is invoked with opt equal to
 * '?'. If readoptions() encounters an option that is missing its
 * required parameter, callback() is invoked with opt equal to ':'. If
 * readoptions() finds a parameter on a long option that does not
 * admit a parameter, callback() is invoked with opt equal to '='. In
 * each of these cases, val will point to the erroneous option
 * argument.
 */
extern int readoptions(option const* list, int argc, char **argv,
		       int (*callback)(int opt, char const *val, void *data),
		       void *data);

/* Parse the given file. list is an array of option structs, in the
 * same form as taken by readoptions(). fp is a pointer to an open
 * text file. callback is the function to call for each line found in
 * the configuration file. data is a pointer that is passed to each
 * invocation of callback. The return value of readcfgfile() is the
 * value returned from the last callback, or zero if no arguments were
 * found, or -1 if an error occurred while reading the file.
 *
 * The function will ignore lines that contain only whitespace, or
 * lines that begin with a hash sign. All other lines should be of the
 * form "OPTION=VALUE", where OPTION is one of the long options in
 * list. Whitespace around the equal sign is permitted. An option that
 * takes no arguments can either have a VALUE of 0 or 1, or omit the
 * "=VALUE" entirely. (A VALUE of 0 will behave the same as if the
 * line was not present.)
 */
extern int readcfgfile(option const* list, FILE *fp,
		       int (*callback)(int opt, char const *val, void *data),
		       void *data);


/* Create an argc-argv pair from a string containing a command line.
 * cmdline is the string to be parsed. argcp points to the variable to
 * receive the argc value, and argvp points to the variable to receive
 * the argv value. argvp can be NULL if the caller just wants to get
 * argc. Zero is returned on failure. This function allocates memory
 * on behalf of the caller. The memory is allocated as a single block,
 * so it is sufficient to simply free() the pointer returned through
 * argvp. Note that argv[0] will always be initialized to NULL; the
 * first argument will be stored in argv[1]. The string is parsed by
 * separating arguments on whitespace boundaries. Space within
 * substrings enclosed in single-quotes is ignored. A substring
 * enclosed in double-quotes is treated the same, except that the
 * backslash is recognized as an escape character within such a
 * substring. Enclosing quotes and escaping backslashes are not copied
 * into the argv values.
 */
extern int makecmdline(char const *cmdline, int *argcp, char ***argvp);

#endif
