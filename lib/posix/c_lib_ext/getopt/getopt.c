/*	$NetBSD: getopt.c,v 1.26 2003/08/07 16:43:40 agc Exp $	*/
/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* for getopt_long(), getopt_long_only() visibility in getopt.h */
#define _GNU_SOURCE
/* for getopt_r(), getopt_long_r(), getopt_long_only_r() visibility in getopt.h */
#define _ZEPHYR_SOURCE

#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(getopt);

#define GNU_COMPATIBLE /* Be more compatible, configure's use us! */

#define PRINT_ERROR ((opterr) && (*(options) != ':'))

#define FLAG_PERMUTE  0x01 /* permute non-options to the end of argv */
#define FLAG_ALLARGS  0x02 /* treat non-options as args to option "-1" */
#define FLAG_LONGONLY 0x04 /* operate as getopt_long_only */

#ifdef GNU_COMPATIBLE
#define NO_PREFIX (-1)
#define D_PREFIX  0
#define DD_PREFIX 1
#define W_PREFIX  2
#endif

#define BADCH  (int)'?'
#define BADARG (int)':'
#define EMSG   ""

int opterr = 1,            /* if error message should be printed */
	optind = 1,        /* index into parent argv vector */
	optopt;            /* character checked for validity */
char *optarg;              /* argument associated with option */
static char *place = EMSG; /* option letter processing */

/*
 * getopt --
 *	Parse argc/argv argument vector.
 */
int getopt_r(int nargc, char *const nargv[], const char *ostr, char **optarg, int *optind,
	     int *opterr, int *optopt, int *optwhere)
{
	char *oli; /* option letter list index */

	if (*optind >= nargc) {
		return -1;
	}

	if ((*optind == 1) || (*place == 0)) { /* update scanning pointer */
		place = nargv[*optind];
		if ((*optind >= nargc) || (*place++ != '-')) {
			/* Argument is absent or is not an option */
			place = EMSG;
			return -1;
		}
		*optopt = *place++;
		if ((*optopt == '-') && (*place == 0)) {
			/* "--" => end of options */
			++*optind;
			place = EMSG;
			return -1;
		}
		if (*optopt == 0) {
			/* Solitary '-', treat as a '-' option
			   if the program (eg su) is looking for it. */
			place = EMSG;
			if (strchr(ostr, '-') == NULL) {
				return -1;
			}
			*optopt = '-';
		}
	} else {
		*optopt = *place++;
	}

	/* See if option letter is one the caller wanted... */
	if ((*optopt == ':') || (oli = strchr(ostr, *optopt)) == NULL) {
		if (*place == 0) {
			++*optind;
		}
		if (*opterr && *ostr != ':') {
			LOG_DBG("illegal option -- %c\n", *optopt);
		}
		return (BADCH);
	}

	/* Does this option need an argument? */
	if (oli[1] != ':') {
		/* don't need argument */
		*optarg = NULL;
		if (*place == 0) {
			++*optind;
		}
	} else {
		/* Option-argument is either the rest of this argument or the
		   entire next argument. */
		if (*place) {
			*optarg = place;
		} else if (nargc > ++*optind) {
			*optarg = nargv[*optind];
		} else {
			/* option-argument absent */
			place = EMSG;
			if (*ostr == ':') {
				return (BADARG);
			}
			if (*opterr) {
				LOG_DBG(" option requires an argument -- %c\n", optopt);
			}
			return (BADCH);
		}
		place = EMSG;
		++*optind;
	}
	return *optopt; /* return option letter */
}

int getopt(int nargc, char *const nargv[], const char *optstring)
{
	return getopt_r(nargc, nargv, optstring, NULL, NULL, FLAG_PERMUTE);
}

#if defined(CONFIG_GETOPT_LONG)

#undef BADARG
#define BADARG  ((*options == ':') ? (int)':' : (int)'?')
#define INORDER 1

static int getopt_internal(int nargc, char *const *nargv, const char *options,
			   const struct option *long_options, int *idx, int flags);
static int parse_long_options(char *const *, const char *, const struct option *, int *, int, int);
static int gcd(int, int);
static void permute_args(int, int, int, char *const *);

/* Error messages */
#define RECARGCHAR "option requires an argument -- %c"
#define ILLOPTCHAR "illegal option -- %c" /* From P1003.2 */
#ifdef GNU_COMPATIBLE
static int dash_prefix = NO_PREFIX;
#define GNUOPTCHAR "invalid option -- %c"

#define RECARGSTRING "option `%s%s' requires an argument"
#define AMBIG        "option `%s%.*s' is ambiguous"
#define NOARG        "option `%s%.*s' doesn't allow an argument"
#define ILLOPTSTRING "unrecognized option `%s%s'"
#else
#define RECARGSTRING "option requires an argument -- %s"
#define AMBIG        "ambiguous option -- %.*s"
#define NOARG        "option doesn't take an argument -- %.*s"
#define ILLOPTSTRING "unknown option -- %s"
#endif

/*
 * Compute the greatest common divisor of a and b.
 */
static int gcd(int a, int b)
{
	int c;

	c = a % b;
	while (c != 0) {
		a = b;
		b = c;
		c = a % b;
	}

	return b;
}

/*
 * Exchange the block from nonopt_start to nonopt_end with the block
 * from nonopt_end to opt_end (keeping the same order of arguments
 * in each block).
 */
static void permute_args(int panonopt_start, int panonopt_end, int opt_end, char *const *nargv)
{
	int cstart, cyclelen, i, j, ncycle, nnonopts, nopts, pos;
	char *swap;

	/*
	 * compute lengths of blocks and number and size of cycles
	 */
	nnonopts = panonopt_end - panonopt_start;
	nopts = opt_end - panonopt_end;
	ncycle = gcd(nnonopts, nopts);
	cyclelen = (opt_end - panonopt_start) / ncycle;

	for (i = 0; i < ncycle; i++) {
		cstart = panonopt_end + i;
		pos = cstart;
		for (j = 0; j < cyclelen; j++) {
			if (pos >= panonopt_end) {
				pos -= nnonopts;
			} else {
				pos += nopts;
			}
			swap = nargv[pos];
			/* LINTED const cast */
			((char **)nargv)[pos] = nargv[cstart];
			/* LINTED const cast */
			((char **)nargv)[cstart] = swap;
		}
	}
}

/*
 * parse_long_options --
 *	Parse long options in argc/argv argument vector.
 * Returns -1 if short_too is set and the option does not match long_options.
 */
static int parse_long_options(char *const *nargv, const char *options,
			      const struct option *long_options, int *idx, int short_too, int flags)
{
	char *place = nargv[optind];
	char *current_argv, *has_equal;
#ifdef GNU_COMPATIBLE
	char *current_dash = "";
#endif
	size_t current_argv_len;
	int i, match, exact_match, second_partial_match;

	current_argv = place;
#ifdef GNU_COMPATIBLE
	if (PRINT_ERROR) {
		switch (dash_prefix) {
		case D_PREFIX:
			current_dash = "-";
			break;
		case DD_PREFIX:
			current_dash = "--";
			break;
		case W_PREFIX:
			current_dash = "-W ";
			break;
		default:
			break;
		}
	}
#endif
	match = -1;
	exact_match = 0;
	second_partial_match = 0;

	optind++;

	has_equal = strchr(current_argv, '=');
	if (has_equal != NULL) {
		/* argument found (--option=arg) */
		current_argv_len = has_equal - current_argv;
		has_equal++;
	} else {
		current_argv_len = strlen(current_argv);
	}

	for (i = 0; long_options[i].name; i++) {
		/* find matching long option */
		if (strncmp(current_argv, long_options[i].name, current_argv_len)) {
			continue;
		}

		if (strlen(long_options[i].name) == current_argv_len) {
			/* exact match */
			match = i;
			exact_match = 1;
			break;
		}
		/*
		 * If this is a known short option, don't allow
		 * a partial match of a single character.
		 */
		if (short_too && current_argv_len == 1) {
			continue;
		}

		if (match == -1) { /* first partial match */
			match = i;
		} else if ((flags & FLAG_LONGONLY) ||
			   long_options[i].has_arg != long_options[match].has_arg ||
			   long_options[i].flag != long_options[match].flag ||
			   long_options[i].val != long_options[match].val) {
			second_partial_match = 1;
		}
	}
	if (!exact_match && second_partial_match) {
		/* ambiguous abbreviation */
		if (PRINT_ERROR) {
			LOG_WRN(AMBIG,
#ifdef GNU_COMPATIBLE
				current_dash,
#endif
				(int)current_argv_len, current_argv);
		}
		optopt = 0;
		return BADCH;
	}
	if (match != -1) { /* option found */
		if (long_options[match].has_arg == no_argument && has_equal) {
			if (PRINT_ERROR) {
				LOG_WRN(NOARG,
#ifdef GNU_COMPATIBLE
					current_dash,
#endif
					(int)current_argv_len, current_argv);
			}
			/*
			 * XXX: GNU sets optopt to val regardless of flag
			 */
			if (long_options[match].flag == NULL) {
				optopt = long_options[match].val;
			} else {
				optopt = 0;
			}
#ifdef GNU_COMPATIBLE
			return BADCH;
#else
			return BADARG;
#endif
		}
		if (long_options[match].has_arg == required_argument ||
		    long_options[match].has_arg == optional_argument) {
			if (has_equal) {
				optarg = has_equal;
			} else if (long_options[match].has_arg == required_argument) {
				/*
				 * optional argument doesn't use next nargv
				 */
				optarg = nargv[optind++];
			}
		}
		if ((long_options[match].has_arg == required_argument) && (optarg == NULL)) {
			/*
			 * Missing argument; leading ':' indicates no error
			 * should be generated.
			 */
			if (PRINT_ERROR) {
				LOG_WRN(RECARGSTRING,
#ifdef GNU_COMPATIBLE
					current_dash,
#endif
					current_argv);
			}
			/*
			 * XXX: GNU sets optopt to val regardless of flag
			 */
			if (long_options[match].flag == NULL) {
				optopt = long_options[match].val;
			} else {
				optopt = 0;
			}
			--optind;
			return BADARG;
		}
	} else { /* unknown option */
		if (short_too) {
			--optind;
			return -1;
		}
		if (PRINT_ERROR) {
			LOG_WRN(ILLOPTSTRING,
#ifdef GNU_COMPATIBLE
				current_dash,
#endif
				current_argv);
		}
		optopt = 0;
		return BADCH;
	}
	if (idx) {
		*idx = match;
	}
	if (long_options[match].flag) {
		*long_options[match].flag = long_options[match].val;
		return 0;
	} else {
		return long_options[match].val;
	}
}

/*
 * getopt_internal --
 *	Parse argc/argv argument vector.  Called by user level routines.
 */
static int getopt_internal(int nargc, char *const *nargv, const char *options,
			   const struct option *long_options, int *idx, int flags)
{
	char *oli; /* option letter list index */
	char *place;
	int optchar, short_too;
	int nonopt_start, nonopt_end;

	if (options == NULL) {
		return -1;
	}

	/*
	 * Disable GNU extensions if options string begins with a '+'.
	 */
#ifdef GNU_COMPATIBLE
	if (*options == '-') {
		flags |= FLAG_ALLARGS;
	} else if (*options == '+') {
		flags &= ~FLAG_PERMUTE;
	}
#else
	if (*options == '+') {
		flags &= ~FLAG_PERMUTE;
	} else if (*options == '-') {
		flags |= FLAG_ALLARGS;
	}
#endif
	if (*options == '+' || *options == '-') {
		options++;
	}

	/*
	 * XXX Some GNU programs (like cvs) set optind to 0 instead of
	 * XXX using optreset.  Work around this braindamage.
	 */
	if (optind == 0) {
		optind = 1;
	}

	optarg = NULL;
	if (optind == 1) {
		nonopt_start = nonopt_end = -1;
	}
start:
	if (!*(place)) {               /* update scanning pointer */
		if (optind >= nargc) { /* end of argument vector */
			place = EMSG;
			if (nonopt_end != -1) {
				/* do permutation, if we have to */
				permute_args(nonopt_start, nonopt_end, optind, nargv);
				optind -= nonopt_end - nonopt_start;
			} else if (nonopt_start != -1) {
				/*
				 * If we skipped non-options, set optind
				 * to the first of them.
				 */
				optind = nonopt_start;
			}
			nonopt_start = nonopt_end = -1;
			return -1;
		}
		place = nargv[optind];
		if (*(place) != '-' ||
#ifdef GNU_COMPATIBLE
		    place[1] == '\0') {
#else
		    (place[1] == '\0' && strchr(options, '-') == NULL)) {
#endif
			place = EMSG; /* found non-option */
			if (flags & FLAG_ALLARGS) {
				/*
				 * GNU extension:
				 * return non-option as argument to option 1
				 */
				optarg = nargv[optind++];
				return INORDER;
			}
			if (!(flags & FLAG_PERMUTE)) {
				/*
				 * If no permutation wanted, stop parsing
				 * at first non-option.
				 */
				return -1;
			}
			/* do permutation */
			if (nonopt_start == -1) {
				nonopt_start = optind;
			} else if (nonopt_end != -1) {
				permute_args(nonopt_start, nonopt_end, optind, nargv);
				nonopt_start = optind - (nonopt_end - nonopt_start);
				nonopt_end = -1;
			}
			optind++;
			/* process next argument */
			goto start;
		}
		if (nonopt_start != -1 && nonopt_end == -1) {
			nonopt_end = optind;
		}

		/*
		 * If we have "-" do nothing, if "--" we are done.
		 */
		if (place[1] != '\0' && *++(place) == '-' && place[1] == '\0') {
			optind++;
			place = EMSG;
			/*
			 * We found an option (--), so if we skipped
			 * non-options, we have to permute.
			 */
			if (nonopt_end != -1) {
				permute_args(nonopt_start, nonopt_end, optind, nargv);
				optind -= nonopt_end - nonopt_start;
			}
			nonopt_start = nonopt_end = -1;
			return -1;
		}
	}

	/*
	 * Check long options if:
	 *  1) we were passed some
	 *  2) the arg is not just "-"
	 *  3) either the arg starts with -- we are getopt_long_only()
	 */
	if (long_options != NULL && place != nargv[optind] &&
	    (*(place) == '-' || (flags & FLAG_LONGONLY))) {
		short_too = 0;
#ifdef GNU_COMPATIBLE
		dash_prefix = D_PREFIX;
#endif
		if (*(place) == '-') {
			place++; /* --foo long option */
#ifdef GNU_COMPATIBLE
			dash_prefix = DD_PREFIX;
#endif
		} else if (*(place) != ':' && strchr(options, *(place)) != NULL) {
			short_too = 1; /* could be short option too */
		}

		optchar = parse_long_options(nargv, options, long_options, idx, short_too, flags);
		if (optchar != -1) {
			place = EMSG;
			return optchar;
		}
	}
	optchar = (int)*(place)++;
	oli = strchr(options, optchar);
	if (optchar == (int)':' || (optchar == (int)'-' && *(place) != '\0') || oli == NULL) {
		/*
		 * If the user specified "-" and  '-' isn't listed in
		 * options, return -1 (non-option) as per POSIX.
		 * Otherwise, it is an unknown option character (or ':').
		 */
		if (optchar == (int)'-' && *(place) == '\0') {
			return -1;
		}
		if (!*(place)) {
			++optind;
		}
#ifdef GNU_COMPATIBLE
		if (PRINT_ERROR) {
			LOG_WRN(GNUOPTCHAR, optchar);
		}
#else
		if (PRINT_ERROR) {
			LOG_WRN(ILLOPTCHAR, optchar);
		}
#endif
		optopt = optchar;
		return BADCH;
	}
	if (long_options != NULL && optchar == 'W' && oli[1] == ';') {
		/* -W long-option */
		if (*(place)) {                   /* no space */
			;                         /* NOTHING */
		} else if (++(optind) >= nargc) { /* no arg */
			place = EMSG;
			if (PRINT_ERROR) {
				LOG_WRN(RECARGCHAR, optchar);
			}
			optopt = optchar;
			return BADARG;
		} else if ((optind) < nargc) {
			place = nargv[optind];
		}
#ifdef GNU_COMPATIBLE
		dash_prefix = W_PREFIX;
#endif
		optchar = parse_long_options(nargv, options, long_options, idx, 0, flags);
		place = EMSG;
		return optchar;
	}
	if (*++oli != ':') { /* doesn't take argument */
		if (!*(place)) {
			++optind;
		}
	} else { /* takes (optional) argument */
		optarg = NULL;
		if (*(place)) { /* no white space */
			optarg = place;
		} else if (oli[1] != ':') {      /* arg not optional */
			if (++optind >= nargc) { /* no arg */
				place = EMSG;
				if (PRINT_ERROR) {
					LOG_WRN(RECARGCHAR, optchar);
				}
				optopt = optchar;
				return BADARG;
			}
			optarg = nargv[optind];
		}
		place = EMSG;
		++optind;
	}
	/* dump back option letter */
	return optchar;
}

/*
 * getopt_long --
 *	Parse argc/argv argument vector.
 */
int getopt_long(int nargc, char *const *nargv, const char *options,
		const struct option *long_options, int *idx)
{
	return getopt_internal(nargc, nargv, options, long_options, idx, FLAG_PERMUTE);
}

/*
 * getopt_long_only --
 *	Parse argc/argv argument vector.
 */
int getopt_long_only(int nargc, char *const *nargv, const char *options,
		     const struct option *long_options, int *idx)
{
	return getopt_internal(nargc, nargv, options, long_options, idx,
			       FLAG_PERMUTE | FLAG_LONGONLY);
}

#endif /* CONFIG_GETOPT_LONG */
