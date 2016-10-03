/* cmdline.c: a reentrant version of getopt(). Written 2006 by Brian
 * Raiter. This code is in the public domain.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"cmdline.h"

#define	docallback(opt, val) \
	    do { if ((r = callback(opt, val, data)) != 0) return r; } while (0)

/* Parse the given cmdline arguments.
 */
int readoptions(option const* list, int argc, char **argv,
		int (*callback)(int, char const*, void*), void *data)
{
    char		argstring[] = "--";
    option const       *opt;
    char const	       *val;
    char const	       *p;
    int			stop = 0;
    int			argi, len, r;

    if (!list || !callback)
	return -1;

    for (argi = 1 ; argi < argc ; ++argi)
    {
	/* First, check for "--", which forces all remaining arguments
	 * to be treated as non-options.
	 */
	if (!stop && argv[argi][0] == '-' && argv[argi][1] == '-'
					  && argv[argi][2] == '\0') {
	    stop = 1;
	    continue;
	}

	/* Arguments that do not begin with '-' (or are only "-") are
	 * not options.
	 */
	if (stop || argv[argi][0] != '-' || argv[argi][1] == '\0') {
	    docallback(0, argv[argi]);
	    continue;
	}

	if (argv[argi][1] == '-')
	{
	    /* Arguments that begin with a double-dash are long
	     * options.
	     */
	    p = argv[argi] + 2;
	    val = strchr(p, '=');
	    if (val)
		len = val++ - p;
	    else
		len = strlen(p);

	    /* Is it on the list of valid options? If so, does it
	     * expect a parameter?
	     */
	    for (opt = list ; opt->optval ; ++opt)
		if (opt->name && !strncmp(p, opt->name, len)
			      && !opt->name[len])
		    break;
	    if (!opt->optval) {
		docallback('?', argv[argi]);
	    } else if (!val && opt->arg == 1) {
		docallback(':', argv[argi]);
	    } else if (val && opt->arg == 0) {
		docallback('=', argv[argi]);
	    } else {
		docallback(opt->optval, val);
	    }
	}
	else
	{
	    /* Arguments that begin with a single dash contain one or
	     * more short options. Each character in the argument is
	     * examined in turn, unless a parameter consumes the rest
	     * of the argument (or possibly even the following
	     * argument).
	     */
	    for (p = argv[argi] + 1 ; *p ; ++p) {
		for (opt = list ; opt->optval ; ++opt)
		    if (opt->chname == *p)
			break;
		if (!opt->optval) {
		    argstring[1] = *p;
		    docallback('?', argstring);
		    continue;
		} else if (opt->arg == 0) {
		    docallback(opt->optval, NULL);
		    continue;
		} else if (p[1]) {
		    docallback(opt->optval, p + 1);
		    break;
		} else if (argi + 1 < argc && strcmp(argv[argi + 1], "--")) {
		    ++argi;
		    docallback(opt->optval, argv[argi]);
		    break;
		} else if (opt->arg == 2) {
		    docallback(opt->optval, NULL);
		    continue;
		} else {
		    argstring[1] = *p;
		    docallback(':', argstring);
		    break;
		}
	    }
	}
    }
    return 0;
}

/* Verify that str points to an ASCII zero or one (optionally with
 * whitespace) and return the value present, or -1 if str's contents
 * are anything else.
 */
static int readboolvalue(char const *str)
{
    char	d;

    while (isspace(*str))
	++str;
    if (!*str)
	return -1;
    d = *str++;
    while (isspace(*str))
	++str;
    if (*str)
	return -1;
    if (d == '0')
	return 0;
    else if (d == '1')
	return 1;
    else
	return -1;
}

/* Parse a configuration file.
 */
int readcfgfile(option const* list, FILE *fp,
		int (*callback)(int, char const*, void*), void *data)
{
    char		buf[1024];
    option const       *opt;
    char	       *name, *val, *p;
    int			len, f, r;

    while (fgets(buf, sizeof buf, fp) != NULL)
    {
	/* Strip off the trailing newline and any leading whitespace.
	 * If the line begins with a hash sign, skip it entirely.
	 */
	len = strlen(buf);
	if (len && buf[len - 1] == '\n')
	    buf[--len] = '\0';
	for (p = buf ; isspace(*p) ; ++p) ;
	if (!*p || *p == '#')
	    continue;

	/* Find the end of the option's name and the beginning of the
	 * parameter, if any.
	 */
	for (name = p ; *p && *p != '=' && !isspace(*p) ; ++p) ;
	len = p - name;
	for ( ; *p == '=' || isspace(*p) ; ++p) ;
	val = p;

	/* Is it on the list of valid options? Does it take a
	 * full parameter, or just an optional boolean?
	 */
	for (opt = list ; opt->optval ; ++opt)
	    if (opt->name && !strncmp(name, opt->name, len)
			  && !opt->name[len])
		    break;
	if (!opt->optval) {
	    docallback('?', name);
	} else if (!*val && opt->arg == 1) {
	    docallback(':', name);
	} else if (*val && opt->arg == 0) {
	    f = readboolvalue(val);
	    if (f < 0)
		docallback('=', name);
	    else if (f == 1)
		docallback(opt->optval, NULL);
	} else {
	    docallback(opt->optval, val);
	}
    }
    return ferror(fp) ? -1 : 0;
}

/* Turn a string containing a cmdline into an argc-argv pair.
 */
int makecmdline(char const *cmdline, int *argcp, char ***argvp)
{
    char      **argv;
    int		argc;
    char const *s;
    int		n, quoted;

    if (!cmdline)
	return 0;

    /* Calcuate argc by counting the number of "clumps" of non-spaces.
     */
    for (s = cmdline ; isspace(*s) ; ++s) ;
    if (!*s) {
	*argcp = 1;
	if (argvp) {
	    *argvp = malloc(2 * sizeof(char*));
	    if (!*argvp)
		return 0;
	    (*argvp)[0] = NULL;
	    (*argvp)[1] = NULL;
	}
	return 1;
    }
    for (argc = 2, quoted = 0 ; *s ; ++s) {
	if (quoted == '"') {
	    if (*s == '"')
		quoted = 0;
	    else if (*s == '\\' && s[1])
		++s;
	} else if (quoted == '\'') {
	    if (*s == '\'')
		quoted = 0;
	} else {
	    if (isspace(*s)) {
		for ( ; isspace(s[1]) ; ++s) ;
		if (!s[1])
		    break;
		++argc;
	    } else if (*s == '"' || *s == '\'') {
		quoted = *s;
	    }
	}
    }

    *argcp = argc;
    if (!argvp)
	return 1;

    /* Allocate space for all the arguments and their pointers.
     */
    argv = malloc((argc + 1) * sizeof(char*) + strlen(cmdline) + 1);
    *argvp = argv;
    if (!argv)
	return 0;
    argv[0] = NULL;
    argv[1] = (char*)(argv + argc + 1);

    /* Copy the string into the allocated memory immediately after the
     * argv array. Where spaces immediately follows a nonspace,
     * replace it with a \0. Where a nonspace immediately follows
     * spaces, store a pointer to it. (Except, of course, when the
     * space-nonspace transitions occur within quotes.)
     */
    for (s = cmdline ; isspace(*s) ; ++s) ;
    for (argc = 1, n = 0, quoted = 0 ; *s ; ++s) {
	if (quoted == '"') {
	    if (*s == '"') {
		quoted = 0;
	    } else {
		if (*s == '\\' && s[1])
		    ++s;
		argv[argc][n++] = *s;
	    }
	} else if (quoted == '\'') {
	    if (*s == '\'')
		quoted = 0;
	    else
		argv[argc][n++] = *s;
	} else {
	    if (isspace(*s)) {
		argv[argc][n] = '\0';
		for ( ; isspace(s[1]) ; ++s) ;
		if (!s[1])
		    break;
		argv[argc + 1] = argv[argc] + n + 1;
		++argc;
		n = 0;
	    } else {
		if (*s == '"' || *s == '\'')
		    quoted = *s;
		else
		    argv[argc][n++] = *s;
	    }
	}
    }
    argv[argc + 1] = NULL;
    return 1;
}
