/*
 *  RISC-V thread support
 *
 *  Copyright (c) 2019 Mark Corbin
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

#ifndef _TARGET_ARCH_THREAD_H_
#define _TARGET_ARCH_THREAD_H_

/* Compare with cpu_set_upcall() in riscv/riscv/vm_machdep.c */
static inline void target_thread_set_upcall(CPURISCVState *regs,
    abi_ulong entry, abi_ulong arg, abi_ulong stack_base,
    abi_ulong stack_size)
{
    abi_ulong sp;

    sp = (abi_ulong)(stack_base + stack_size) & ~(16 - 1);

    regs->gpr[xSP] = sp;
    regs->pc = entry;
    regs->gpr[xA0] = arg;
}

/* Compare with exec_setregs() in riscv/riscv/machdep.c */
static inline void target_thread_init(struct target_pt_regs *regs,
    struct image_info *infop)
{
    regs->sepc = infop->entry & ~0x03;
    regs->regs[xRA] = infop->entry & ~0x03;
    regs->regs[10] = infop->start_stack;               /* a0 */
    regs->regs[xSP] = infop->start_stack & ~(16 - 1);
}

#endif /* !_TARGET_ARCH_THREAD_H_ */
