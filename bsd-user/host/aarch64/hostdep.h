/*
 * hostdep.h : things which are dependent on the host architecture
 *
 *  * Written by Peter Maydell <peter.maydell@linaro.org>
 *
 * Copyright (C) 2016 Linaro Limited
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef AARCH64_HOSTDEP_H
#define AARCH64_HOSTDEP_H

/* We have a safe-syscall.inc.S */
#define HAVE_SAFE_SYSCALL

#define ADJUST_SYSCALL_RETCODE \
    b.cc 2f;                   \
    neg x0, x0;                \
    2:

#endif
