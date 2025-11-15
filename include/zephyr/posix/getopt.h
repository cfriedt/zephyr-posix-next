/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_POSIX_GETOPT_H_
#define ZEPHYR_INCLUDE_POSIX_GETOPT_H_

#if defined(_GNU_SOURCE) || defined(__DOXYGEN__)

#if (!defined(_OPTION_DECLARED) && !defined(__option_defined)) || defined(__DOXYGEN__)
struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};
#define _OPTION_DECLARED
#define __option_defined
#endif

int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts,
		int *longindex);

int getopt_long_only(int argc, char *const argv[], const char *optstring,
		     const struct option *longopts, int *longindex);

#endif /* defined(_GNU_SOURCE) || defined(__DOXYGEN__) */

#if defined(_ZEPHYR_SOURCE) || defined(__DOXYGEN__)

int getopt_r(int argc, char *const argv[], const char *optstring, char **optarg, int *optind,
	     int *opterr, int *optopt, int *optwhere);

int getopt_long_r(int argc, char *const argv[], const char *shortopts,
		  const struct option *longopts, int *longind, char **optarg, int *optind,
		  int *opterr, int *optopt, int *optwhere);

int getopt_long_only_r(int argc, char *const argv[], const char *shortopts,
		       const struct option *longopts, int *longind, char **optarg, int *optind,
		       int *opterr, int *optopt, int *optwhere);

#endif /* defined(_ZEPHYR_SOURCE) || defined(__DOXYGEN__) */

#endif /* ZEPHYR_INCLUDE_POSIX_GETOPT_H_ */
