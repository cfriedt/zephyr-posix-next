/*
 * Copyright (c) 2024 Marvin Ouma <pancakesdeath@protonmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>

#include <zephyr/ztest.h>

static void before(void *arg)
{
    ARG_UNUSED(arg);

    /* re-initialize getopt(). This should also be done on a per-thread basis when testing for thread-safety */
    optind = 1;
}

ZTEST_SUITE(posix_c_lib_ext, NULL, NULL, before, NULL, NULL);
