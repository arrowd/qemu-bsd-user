/*
 * PowerPC specific prototypes for bsd-user
 *
 *  Copyright (c) 2015 Justin Hibbits
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TARGET_ARCH_H_
#define _TARGET_ARCH_H_

#include "qemu.h"

/* target_arch_cpu.c */
extern bool bsd_ppc_is_elfv1(CPUPPCState *env);

#endif /* !_TARGET_ARCH_H_ */
