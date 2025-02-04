/*
 *  powerpc cpu init and loop
 *
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

#ifndef _TARGET_ARCH_CPU_H_
#define _TARGET_ARCH_CPU_H_

#include "target_arch.h"

#if defined(TARGET_PPC64) && !defined(TARGET_ABI32)
#if defined(TARGET_WORDS_BIGENDIAN)
#define TARGET_DEFAULT_CPU_MODEL "ppc64"
#else
/* LE is restricted to POWER8 and up. */
#define TARGET_DEFAULT_CPU_MODEL "power8"
#endif
#else
#define TARGET_DEFAULT_CPU_MODEL "ppc"
#endif

static inline void target_cpu_set_tls(CPUPPCState *env, target_ulong newtls)
{
#if defined(TARGET_PPC64)
    /* The kernel checks TIF_32BIT here; we don't support loading 32-bit
       binaries on PPC64 yet. */
    env->gpr[13] = newtls + 0x7010;
#else
    env->gpr[2] = newtls + 0x7008;
#endif
}

static inline void target_cpu_init(CPUPPCState *env,
        struct target_pt_regs *regs)
{
    int i;

#ifdef TARGET_PPC64
    env->msr |= (target_ulong)1 << MSR_SF;
#endif
    for (i = 0; i < 32; i++) {
        env->gpr[i] = regs->gpr[i];
    }
    env->nip = regs->nip;
}

static inline void target_cpu_loop(CPUPPCState *env)
{
    CPUState *cs = env_cpu(env);
    int trapnr;
    target_ulong ret;
    int32_t signo, code;

    for(;;) {
        bool arch_interrupt;

        cpu_exec_start(cs);
        trapnr = cpu_exec(cs);
        cpu_exec_end(cs);
        process_queued_cpu_work(cs);

        arch_interrupt = true;
        switch(trapnr) {
        case POWERPC_EXCP_NONE:
            /* Just go on */
            break;
        case POWERPC_EXCP_CRITICAL: /* Critical input                        */
            cpu_abort(cs, "Critical interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_MCHECK:   /* Machine check exception               */
            cpu_abort(cs, "Machine check exception while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_DSI:      /* Data storage exception                */
            fprintf(stderr, "Invalid data memory access: 0x" TARGET_FMT_lx "\n",
                      env->spr[SPR_DAR]);
            /* XXX: check this. Seems bugged */
            if (env->error_code & 0x40000000) {
                signo = TARGET_SIGSEGV;
                code = TARGET_SEGV_MAPERR;
            } else if (env->error_code & 0x04000000) {
                signo = TARGET_SIGILL;
                code = TARGET_ILL_ILLADR;
            } else if (env->error_code & 0x08000000) {
                signo = TARGET_SIGSEGV;
                code = TARGET_SEGV_ACCERR;
            } else {
                /* Let's send a regular segfault... */
                fprintf(stderr, "Invalid segfault errno (%02x)\n",
                          env->error_code);
                signo = TARGET_SIGSEGV;
                code = TARGET_SEGV_MAPERR;
            }
            force_sig_fault(signo, code, env->nip);
            break;
        case POWERPC_EXCP_ISI:      /* Instruction storage exception         */
            fprintf(stderr, "Invalid instruction fetch: 0x\n" TARGET_FMT_lx
                      "\n", env->spr[SPR_SRR0]);
            /* XXX: check this */
            switch (env->error_code & 0xFF000000) {
            case 0x40000000:
                signo = TARGET_SIGSEGV;
                code = TARGET_SEGV_MAPERR;
                break;
            case 0x10000000:
            case 0x08000000:
                signo = TARGET_SIGSEGV;
                code = TARGET_SEGV_ACCERR;
                break;
            default:
                /* Let's send a regular segfault... */
                fprintf(stderr, "Invalid segfault errno (%02x)\n",
                          env->error_code);
                signo = TARGET_SIGSEGV;
                code = TARGET_SEGV_MAPERR;
                break;
            }
            force_sig_fault(signo, code, env->nip - 4);
            break;
        case POWERPC_EXCP_EXTERNAL: /* External input                        */
            cpu_abort(cs, "External interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_ALIGN:    /* Alignment exception                   */
            fprintf(stderr, "Unaligned memory access\n");
            /* XXX: check this */
            signo = TARGET_SIGBUS;
            code = TARGET_BUS_ADRALN;
            force_sig_fault(signo, code, env->nip - 4);
            break;
        case POWERPC_EXCP_PROGRAM:  /* Program exception                     */
        case POWERPC_EXCP_HV_EMU:   /* HV emulation                          */
            /* XXX: check this */
            switch (env->error_code & ~0xF) {
            case POWERPC_EXCP_FP:
                fprintf(stderr, "Floating point program exception\n");
                signo = TARGET_SIGFPE;
                switch (env->error_code & 0xF) {
                case POWERPC_EXCP_FP_OX:
                    code = TARGET_FPE_FLTOVF;
                    break;
                case POWERPC_EXCP_FP_UX:
                    code = TARGET_FPE_FLTUND;
                    break;
                case POWERPC_EXCP_FP_ZX:
                case POWERPC_EXCP_FP_VXZDZ:
                    code = TARGET_FPE_FLTDIV;
                    break;
                case POWERPC_EXCP_FP_XX:
                    code = TARGET_FPE_FLTRES;
                    break;
                case POWERPC_EXCP_FP_VXSOFT:
                    code = TARGET_FPE_FLTINV;
                    break;
                case POWERPC_EXCP_FP_VXSNAN:
                case POWERPC_EXCP_FP_VXISI:
                case POWERPC_EXCP_FP_VXIDI:
                case POWERPC_EXCP_FP_VXIMZ:
                case POWERPC_EXCP_FP_VXVC:
                case POWERPC_EXCP_FP_VXSQRT:
                case POWERPC_EXCP_FP_VXCVI:
                    code = TARGET_FPE_FLTSUB;
                    break;
                default:
                    fprintf(stderr, "Unknown floating point exception (%02x)\n",
                              env->error_code);
                    break;
                }
                break;
            case POWERPC_EXCP_INVAL:
                fprintf(stderr, "Invalid instruction\n");
                signo = TARGET_SIGILL;
                switch (env->error_code & 0xF) {
                case POWERPC_EXCP_INVAL_INVAL:
                    code = TARGET_ILL_ILLOPC;
                    break;
                case POWERPC_EXCP_INVAL_LSWX:
                    code = TARGET_ILL_ILLOPN;
                    break;
                case POWERPC_EXCP_INVAL_SPR:
                    code = TARGET_ILL_PRVREG;
                    break;
                case POWERPC_EXCP_INVAL_FP:
                    code = TARGET_ILL_COPROC;
                    break;
                default:
                    fprintf(stderr, "Unknown invalid operation (%02x)\n",
                              env->error_code & 0xF);
                    code = TARGET_ILL_ILLADR;
                    break;
                }
                break;
            case POWERPC_EXCP_PRIV:
                fprintf(stderr, "Privilege violation\n");
                signo = TARGET_SIGILL;
                switch (env->error_code & 0xF) {
                case POWERPC_EXCP_PRIV_OPC:
                    code = TARGET_ILL_PRVOPC;
                    break;
                case POWERPC_EXCP_PRIV_REG:
                    code = TARGET_ILL_PRVREG;
                    break;
                default:
                    fprintf(stderr, "Unknown privilege violation (%02x)\n",
                              env->error_code & 0xF);
                    code = TARGET_ILL_PRVOPC;
                    break;
                }
                break;
            case POWERPC_EXCP_TRAP:
                cpu_abort(cs, "Tried to call a TRAP\n");
                break;
            default:
                /* Should not happen ! */
                cpu_abort(cs, "Unknown program exception (%02x)\n",
                          env->error_code);
                break;
            }
            force_sig_fault(signo, code, env->nip - 4);
            break;
        case POWERPC_EXCP_FPU:      /* Floating-point unavailable exception  */
            env->msr |= (1 << MSR_FP);
            env->nip -= 4;
#if 0
            fprintf(stderr, "No floating point allowed\n");
            signo = TARGET_SIGILL;
            code = TARGET_ILL_COPROC;
            force_sig_fault(signo, code, env->nip - 4);
#endif
            break;
        case POWERPC_EXCP_SYSCALL:  /* System call exception                 */
            cpu_abort(cs, "Syscall exception while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_APU:      /* Auxiliary processor unavailable       */
            fprintf(stderr, "No APU instruction allowed\n");
            signo = TARGET_SIGILL;
            code = TARGET_ILL_COPROC;
            force_sig_fault(signo, code, env->nip - 4);
            break;
        case POWERPC_EXCP_DECR:     /* Decrementer exception                 */
            cpu_abort(cs, "Decrementer interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_FIT:      /* Fixed-interval timer interrupt        */
            cpu_abort(cs, "Fix interval timer interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_WDT:      /* Watchdog timer interrupt              */
            cpu_abort(cs, "Watchdog timer interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_DTLB:     /* Data TLB error                        */
            cpu_abort(cs, "Data TLB exception while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_ITLB:     /* Instruction TLB error                 */
            cpu_abort(cs, "Instruction TLB exception while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_SPEU:     /* SPE/embedded floating-point unavail.  */
            fprintf(stderr, "No SPE/floating-point instruction allowed\n");
            signo = TARGET_SIGILL;
            code = TARGET_ILL_COPROC;
            force_sig_fault(signo, code, env->nip - 4);
            break;
        case POWERPC_EXCP_EFPDI:    /* Embedded floating-point data IRQ      */
            cpu_abort(cs, "Embedded floating-point data IRQ not handled\n");
            break;
        case POWERPC_EXCP_EFPRI:    /* Embedded floating-point round IRQ     */
            cpu_abort(cs, "Embedded floating-point round IRQ not handled\n");
            break;
        case POWERPC_EXCP_EPERFM:   /* Embedded performance monitor IRQ      */
            cpu_abort(cs, "Performance monitor exception not handled\n");
            break;
        case POWERPC_EXCP_DOORI:    /* Embedded doorbell interrupt           */
            cpu_abort(cs, "Doorbell interrupt while in user mode. "
                       "Aborting\n");
            break;
        case POWERPC_EXCP_DOORCI:   /* Embedded doorbell critical interrupt  */
            cpu_abort(cs, "Doorbell critical interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_RESET:    /* System reset exception                */
            cpu_abort(cs, "Reset interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_DSEG:     /* Data segment exception                */
            cpu_abort(cs, "Data segment exception while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_ISEG:     /* Instruction segment exception         */
            cpu_abort(cs, "Instruction segment exception "
                      "while in user mode. Aborting\n");
            break;
        /* PowerPC 64 with hypervisor mode support */
        case POWERPC_EXCP_HDECR:    /* Hypervisor decrementer exception      */
            cpu_abort(cs, "Hypervisor decrementer interrupt "
                      "while in user mode. Aborting\n");
            break;
        case POWERPC_EXCP_TRACE:    /* Trace exception                       */
            /* Nothing to do:
             * we use this exception to emulate step-by-step execution mode.
             */
            break;
        /* PowerPC 64 with hypervisor mode support */
        case POWERPC_EXCP_HDSI:     /* Hypervisor data storage exception     */
            cpu_abort(cs, "Hypervisor data storage exception "
                      "while in user mode. Aborting\n");
            break;
        case POWERPC_EXCP_HISI:     /* Hypervisor instruction storage excp   */
            cpu_abort(cs, "Hypervisor instruction storage exception "
                      "while in user mode. Aborting\n");
            break;
        case POWERPC_EXCP_HDSEG:    /* Hypervisor data segment exception     */
            cpu_abort(cs, "Hypervisor data segment exception "
                      "while in user mode. Aborting\n");
            break;
        case POWERPC_EXCP_HISEG:    /* Hypervisor instruction segment excp   */
            cpu_abort(cs, "Hypervisor instruction segment exception "
                      "while in user mode. Aborting\n");
            break;
        case POWERPC_EXCP_VPU:      /* Vector unavailable exception          */
            env->msr |= (1 << MSR_VR);
            env->nip -= 4;
#if 0
            fprintf(stderr, "No Altivec instructions allowed\n");
            signo = TARGET_SIGILL;
            code = TARGET_ILL_COPROC;
            force_sig_fault(signo, code, env->nip - 4);
#endif
            break;
        case POWERPC_EXCP_PIT:      /* Programmable interval timer IRQ       */
            cpu_abort(cs, "Programmable interval timer interrupt "
                      "while in user mode. Aborting\n");
            break;
        case POWERPC_EXCP_EMUL:     /* Emulation trap exception              */
            cpu_abort(cs, "Emulation trap exception not handled\n");
            break;
        case POWERPC_EXCP_IFTLB:    /* Instruction fetch TLB error           */
            cpu_abort(cs, "Instruction fetch TLB exception "
                      "while in user-mode. Aborting");
            break;
        case POWERPC_EXCP_DLTLB:    /* Data load TLB miss                    */
            cpu_abort(cs, "Data load TLB exception while in user-mode. "
                      "Aborting");
            break;
        case POWERPC_EXCP_DSTLB:    /* Data store TLB miss                   */
            cpu_abort(cs, "Data store TLB exception while in user-mode. "
                      "Aborting");
            break;
        case POWERPC_EXCP_FPA:      /* Floating-point assist exception       */
            cpu_abort(cs, "Floating-point assist exception not handled\n");
            break;
        case POWERPC_EXCP_IABR:     /* Instruction address breakpoint        */
            cpu_abort(cs, "Instruction address breakpoint exception "
                      "not handled\n");
            break;
        case POWERPC_EXCP_SMI:      /* System management interrupt           */
            cpu_abort(cs, "System management interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_THERM:    /* Thermal interrupt                     */
            cpu_abort(cs, "Thermal interrupt interrupt while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_PERFM:   /* Embedded performance monitor IRQ      */
            cpu_abort(cs, "Performance monitor exception not handled\n");
            break;
        case POWERPC_EXCP_VPUA:     /* Vector assist exception               */
            cpu_abort(cs, "Vector assist exception not handled\n");
            break;
        case POWERPC_EXCP_SOFTP:    /* Soft patch exception                  */
            cpu_abort(cs, "Soft patch exception not handled\n");
            break;
        case POWERPC_EXCP_MAINT:    /* Maintenance exception                 */
            cpu_abort(cs, "Maintenance exception while in user mode. "
                      "Aborting\n");
            break;
        case POWERPC_EXCP_SYSCALL_USER:
            /* system call in user-mode emulation */
            /* WARNING:
             * PPC ABI uses overflow flag in cr0 to signal an error
             * in syscalls.
             */
            env->crf[0] &= ~0x1;
            env->nip += 4;
            ret = do_freebsd_syscall(env, env->gpr[0], env->gpr[3], env->gpr[4],
                             env->gpr[5], env->gpr[6], env->gpr[7],
                             env->gpr[8], env->gpr[9], env->gpr[10]);
            if (ret == (target_ulong)(-TARGET_EJUSTRETURN)) {
                /* Returning from a successful sigreturn syscall.
                   Avoid corrupting register state.  */
                break;
            } else if (ret == (target_ulong)(-TARGET_ERESTART)) {
                env->nip -= 4;
                break;
            }
            if (ret > (target_ulong)(-515)) {
                env->crf[0] |= 0x1;
                ret = -ret;
            }
            env->gpr[3] = ret;
            break;
        case EXCP_DEBUG:
            {
                signo = TARGET_SIGTRAP;
                code = TARGET_TRAP_BRKPT;
                force_sig_fault(signo, code, env->nip);
            }
            break;
        case EXCP_ATOMIC:
            cpu_exec_step_atomic(cs);
            arch_interrupt = false;
            break;
        case EXCP_INTERRUPT:
            /* just indicate that signals should be handled asap */
            break;
        default:
            cpu_abort(cs, "Unknown exception 0x%x. Aborting\n", trapnr);
            break;
        }
        process_pending_signals(env);

        /* Most of the traps imply a transition through kernel mode,
         * which implies an REI instruction has been executed.  Which
         * means that RX and LOCK_ADDR should be cleared.  But there
         * are a few exceptions for traps internal to QEMU.
         */
        if (arch_interrupt) {
            env->reserve_addr = -1;
        }
    }
}

static inline void target_cpu_clone_regs(CPUPPCState *env, target_ulong newsp)
{
    if (newsp)
        env->gpr[1] = newsp;
    env->gpr[3] = 0;
}

static inline void target_cpu_reset(CPUArchState *env)
{
    cpu_reset(env_cpu(env));
}

#endif /* ! _TARGET_ARCH_CPU_H_ */
