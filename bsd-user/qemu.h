/*
 *  qemu bsd user mode definition
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
#ifndef QEMU_H
#define QEMU_H

#include <sys/param.h>

#define BSD_HAVE_INO64
#define BSD_HAVE_KEVENT64

#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/units.h"
#include "exec/cpu_ldst.h"
#include "exec/exec-all.h"
#include "trace/trace-bsd_user.h"

#undef DEBUG_REMAP
#ifdef DEBUG_REMAP
#include <stdio.h>
#endif /* DEBUG_REMAP */

#include "exec/user/abitypes.h"

extern char **environ;

#include "exec/user/thunk.h"
#include "target_arch.h"
#include "syscall_defs.h"
#include "target_syscall.h"
#include "target_os_vmparam.h"
#include "target_os_signal.h"
#include "hostdep.h"
#include "exec/gdbstub.h"

/*
 * This struct is used to hold certain information about the image.  Basically,
 * it replicates in user space what would be certain task_struct fields in the
 * kernel
 */
struct image_info {
    abi_ulong load_bias;
    abi_ulong load_addr;
    abi_ulong start_code;
    abi_ulong end_code;
    abi_ulong start_data;
    abi_ulong end_data;
    abi_ulong start_brk;
    abi_ulong brk;
    abi_ulong start_mmap;
    abi_ulong mmap;
    abi_ulong rss;
    abi_ulong start_stack;
    abi_ulong entry;
    abi_ulong code_offset;
    abi_ulong data_offset;
    abi_ulong arg_start;
    abi_ulong arg_end;
    uint32_t  elf_flags;
};

struct emulated_sigtable {
    int pending; /* true if signal is pending */
    target_siginfo_t info;
};

/*
 * NOTE: we force a big alignment so that the stack stored after is aligned too
 */
typedef struct TaskState {
    pid_t ts_tid;     /* tid (or pid) of this task */

    struct TaskState *next;
    struct bsd_binprm *bprm;
    struct image_info *info;

    struct emulated_sigtable sync_signal;
    /*
     * TODO: Since we block all signals while returning to the main CPU
     * loop, this needn't be an array
     */
    struct emulated_sigtable sigtab[TARGET_NSIG];
    /*
     * Nonzero if process_pending_signals() needs to do something (either
     * handle a pending signal or unblock signals).
     * This flag is written from a signal handler so should be accessed via
     * the qatomic_read() and qatomic_set() functions. (It is not accessed
     * from multiple threads.)
     */
    int signal_pending;
    /* True if we're leaving a sigsuspend and sigsuspend_mask is valid. */
    bool in_sigsuspend;
    /*
     * This thread's signal mask, as requested by the guest program.
     * The actual signal mask of this thread may differ:
     *  + we don't let SIGSEGV and SIGBUS be blocked while running guest code
     *  + sometimes we block all signals to avoid races
     */
    sigset_t signal_mask;
    /*
     * The signal mask imposed by a guest sigsuspend syscall, if we are
     * currently in the middle of such a syscall
     */
    sigset_t sigsuspend_mask;

    /* This thread's sigaltstack, if it has one */
    struct target_sigaltstack sigaltstack_used;
} __attribute__((aligned(16))) TaskState;

void init_task_state(TaskState *ts);
void stop_all_tasks(void);
extern const char *interp_prefix;
extern const char *qemu_uname_release;

/*
 * TARGET_ARG_MAX defines the number of bytes allocated for arguments
 * and envelope for the new program. 256k should suffice for a reasonable
 * maxiumum env+arg in 32-bit environments, bump it up to 512k for !ILP32
 * platforms.
 */
#if TARGET_ABI_BITS > 32
#define TARGET_ARG_MAX (512 * 1024)
#else
#define TARGET_ARG_MAX (256 * 1024)
#endif
#define MAX_ARG_PAGES (TARGET_ARG_MAX / TARGET_PAGE_SIZE)

/*
 * This structure is used to hold the arguments that are
 * used when loading binaries.
 */
struct bsd_binprm {
        char buf[128];
        void *page[MAX_ARG_PAGES];
        abi_ulong p;
        abi_ulong stringp;
        int fd;
        int e_uid, e_gid;
        int argc, envc;
        char **argv;
        char **envp;
        char *filename;         /* (Given) Name of binary */
        char *fullpath;         /* Full path of binary */
        int (*core_dump)(int, CPUArchState *);
};

void do_init_thread(struct target_pt_regs *regs, struct image_info *infop);
abi_ulong loader_build_argptr(int envc, int argc, abi_ulong sp,
                              abi_ulong stringp);
int loader_exec(const char *filename, char **argv, char **envp,
             struct target_pt_regs *regs, struct image_info *infop,
             struct bsd_binprm *bprm);

int load_elf_binary(struct bsd_binprm *bprm, struct target_pt_regs *regs,
                    struct image_info *info);
int load_flt_binary(struct bsd_binprm *bprm, struct target_pt_regs *regs,
                    struct image_info *info);
int is_target_elf_binary(int fd);

abi_long memcpy_to_target(abi_ulong dest, const void *src,
                          unsigned long len);
void target_set_brk(abi_ulong new_brk);
abi_long do_brk(abi_ulong new_brk);
void syscall_init(void);
abi_long do_freebsd_syscall(void *cpu_env, int num, abi_long arg1,
                            abi_long arg2, abi_long arg3, abi_long arg4,
                            abi_long arg5, abi_long arg6, abi_long arg7,
                            abi_long arg8);
abi_long do_netbsd_syscall(void *cpu_env, int num, abi_long arg1,
                           abi_long arg2, abi_long arg3, abi_long arg4,
                           abi_long arg5, abi_long arg6);
abi_long do_openbsd_syscall(void *cpu_env, int num, abi_long arg1,
                            abi_long arg2, abi_long arg3, abi_long arg4,
                            abi_long arg5, abi_long arg6);
void gemu_log(const char *fmt, ...) GCC_FMT_ATTR(1, 2);
extern __thread CPUState *thread_cpu;
void cpu_loop(CPUArchState *env);
char *target_strerror(int err);
int get_osversion(void);
void fork_start(void);
void fork_end(int child);

#include "qemu/log.h"

/* strace.c */
struct syscallname {
    int nr;
    const char *name;
    const char *format;
    void (*call)(const struct syscallname *,
                 abi_long, abi_long, abi_long,
                 abi_long, abi_long, abi_long);
    void (*result)(const struct syscallname *, abi_long);
};

void
print_freebsd_syscall(int num,
                      abi_long arg1, abi_long arg2, abi_long arg3,
                      abi_long arg4, abi_long arg5, abi_long arg6);
void print_freebsd_syscall_ret(int num, abi_long ret);
void
print_netbsd_syscall(int num,
                     abi_long arg1, abi_long arg2, abi_long arg3,
                     abi_long arg4, abi_long arg5, abi_long arg6);
void print_netbsd_syscall_ret(int num, abi_long ret);
void
print_openbsd_syscall(int num,
                      abi_long arg1, abi_long arg2, abi_long arg3,
                      abi_long arg4, abi_long arg5, abi_long arg6);
void print_openbsd_syscall_ret(int num, abi_long ret);
/**
 * print_taken_signal:
 * @target_signum: target signal being taken
 * @tinfo: target_siginfo_t which will be passed to the guest for the signal
 *
 * Print strace output indicating that this signal is being taken by the guest,
 * in a format similar to:
 * --- SIGSEGV {si_signo=SIGSEGV, si_code=SI_KERNEL, si_addr=0} ---
 */
void print_taken_signal(int target_signum, const target_siginfo_t *tinfo);
extern int do_strace;

/* mmap.c */
int target_mprotect(abi_ulong start, abi_ulong len, int prot);
abi_long target_mmap(abi_ulong start, abi_ulong len, int prot,
                     int flags, int fd, off_t offset);
int target_munmap(abi_ulong start, abi_ulong len);
abi_long target_mremap(abi_ulong old_addr, abi_ulong old_size,
                       abi_ulong new_size, unsigned long flags,
                       abi_ulong new_addr);
int target_msync(abi_ulong start, abi_ulong len, int flags);
extern unsigned long last_brk;
extern abi_ulong mmap_next_start;
abi_ulong mmap_find_vma(abi_ulong start, abi_ulong size);
void mmap_fork_start(void);
void mmap_fork_end(int child);

/* main.c */
extern char qemu_proc_pathname[];
extern unsigned long target_maxtsiz;
extern unsigned long target_dfldsiz;
extern unsigned long target_maxdsiz;
extern unsigned long target_dflssiz;
extern unsigned long target_maxssiz;
extern unsigned long target_sgrowsiz;

/* syscall.c */
abi_long get_errno(abi_long ret);
bool is_error(abi_long ret);
int host_to_target_errno(int err);

/* os-proc.c */
abi_long freebsd_exec_common(abi_ulong path_or_fd, abi_ulong guest_argp,
        abi_ulong guest_envp, int do_fexec);
abi_long do_freebsd_procctl(void *cpu_env, int idtype, abi_ulong arg2,
        abi_ulong arg3, abi_ulong arg4, abi_ulong arg5, abi_ulong arg6);

/* os-sys.c */
struct target_kinfo_proc;
struct target_kinfo_file;
struct target_kinfo_vmentry;
abi_long do_sysctl_kern_getprocs(int op, int arg, size_t olen,
        struct target_kinfo_proc *tki, size_t *tlen);
abi_long do_sysctl_kern_proc_filedesc(int pid, size_t olen,
        struct target_kinfo_file *tkif, size_t *tlen);
abi_long do_sysctl_kern_proc_vmmap(int pid, size_t olen,
        struct target_kinfo_vmentry *tkve, size_t *tlen);
abi_long do_freebsd_sysctl(CPUArchState *env, abi_ulong namep, int32_t namelen,
        abi_ulong oldp, abi_ulong oldlenp, abi_ulong newp, abi_ulong newlen);
abi_long do_freebsd_sysctlbyname(CPUArchState *env, abi_ulong namep,
        int32_t namelen, abi_ulong oldp, abi_ulong oldlenp, abi_ulong newp,
        abi_ulong newlen);
abi_long do_freebsd_sysarch(void *cpu_env, abi_long arg1, abi_long arg2);

/* os-thread.c */
extern pthread_mutex_t *new_freebsd_thread_lock_ptr;
extern pthread_mutex_t *freebsd_umtx_wait_lck_ptr;
void *new_freebsd_thread_start(void *arg);
abi_long freebsd_lock_umtx(abi_ulong target_addr, abi_long tid,
        size_t tsz, void *t);
abi_long freebsd_unlock_umtx(abi_ulong target_addr, abi_long id);
abi_long freebsd_umtx_wait(abi_ulong targ_addr, abi_ulong id,
        size_t tsz, void *t);
abi_long freebsd_umtx_wake(abi_ulong target_addr, uint32_t n_wake);
abi_long freebsd_umtx_mutex_wake(abi_ulong target_addr, abi_long val);
abi_long freebsd_umtx_wait_uint(abi_ulong obj, uint32_t val, size_t tsz,
        void *t);
abi_long freebsd_umtx_wait_uint_private(abi_ulong obj, uint32_t val,
        size_t tsz, void *t);
abi_long freebsd_umtx_wake_private(abi_ulong obj, uint32_t val);
abi_long freebsd_umtx_nwake_private(abi_ulong obj, uint32_t val);
abi_long freebsd_umtx_mutex_wake2(abi_ulong obj, uint32_t val);
abi_long freebsd_umtx_sem2_wait(abi_ulong obj, size_t tsz, void *t);
abi_long freebsd_umtx_sem2_wake(abi_ulong obj);
abi_long freebsd_umtx_sem_wait(abi_ulong obj, size_t tsz, void *t);
abi_long freebsd_umtx_sem_wake(abi_ulong obj);
abi_long freebsd_lock_umutex(abi_ulong target_addr, uint32_t id,
        void *ts, size_t tsz, int mode, abi_ulong val);
abi_long freebsd_unlock_umutex(abi_ulong target_addr, uint32_t id);
abi_long freebsd_cv_wait(abi_ulong target_ucond_addr,
                abi_ulong target_umtx_addr, struct timespec *ts, int wflags);
abi_long freebsd_cv_signal(abi_ulong target_ucond_addr);
abi_long freebsd_cv_broadcast(abi_ulong target_ucond_addr);
abi_long freebsd_rw_rdlock(abi_ulong target_addr, long fflag,
        size_t tsz, void *t);
abi_long freebsd_rw_wrlock(abi_ulong target_addr, long fflag,
        size_t tsz, void *t);
abi_long freebsd_rw_unlock(abi_ulong target_addr);
abi_long freebsd_umtx_shm(abi_ulong target_addr, long fflag);
abi_long freebsd_umtx_robust_list(abi_ulong target_addr, size_t rbsize);

/* user access */

#define VERIFY_READ 0
#define VERIFY_WRITE 1 /* implies read access */

static inline int access_ok(int type, abi_ulong addr, abi_ulong size)
{
    return page_check_range((target_ulong)addr, size,
                            (type == VERIFY_READ) ? PAGE_READ : (PAGE_READ | PAGE_WRITE)) == 0;
}

/*
 * NOTE __get_user and __put_user use host pointers and don't check access.
 *
 * These are usually used to access struct data members once the struct has been
 * locked - usually with lock_user_struct().
 */

/*
 * Tricky points:
 * - Use __builtin_choose_expr to avoid type promotion from ?:,
 * - Invalid sizes result in a compile time error stemming from
 *   the fact that abort has no parameters.
 * - It's easier to use the endian-specific unaligned load/store
 *   functions than host-endian unaligned load/store plus tswapN.
 * - The pragmas are necessary only to silence a clang false-positive
 *   warning: see https://bugs.llvm.org/show_bug.cgi?id=39113 .
 * - We have to disable -Wpragmas warnings to avoid a complaint about
 *   an unknown warning type from older compilers that don't know about
 *   -Waddress-of-packed-member.
 * - gcc has bugs in its _Pragma() support in some versions, eg
 *   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83256 -- so we only
 *   include the warning-suppression pragmas for clang
 */
#ifdef __clang__
#define PRAGMA_DISABLE_PACKED_WARNING                                   \
    _Pragma("GCC diagnostic push");                                     \
    _Pragma("GCC diagnostic ignored \"-Wpragmas\"");                    \
    _Pragma("GCC diagnostic ignored \"-Waddress-of-packed-member\"")

#define PRAGMA_REENABLE_PACKED_WARNING          \
    _Pragma("GCC diagnostic pop")

#else
#define PRAGMA_DISABLE_PACKED_WARNING
#define PRAGMA_REENABLE_PACKED_WARNING
#endif

#define __put_user(x, hptr)\
({\
    PRAGMA_DISABLE_PACKED_WARNING;\
    int size = sizeof(*hptr);\
    switch (size) {\
    case 1:\
        *(uint8_t *)(hptr) = (uint8_t)(typeof(*hptr))(x);\
        break;\
    case 2:\
        *(uint16_t *)(hptr) = tswap16((typeof(*hptr))(x));\
        break;\
    case 4:\
        *(uint32_t *)(hptr) = tswap32((typeof(*hptr))(x));\
        break;\
    case 8:\
        *(uint64_t *)(hptr) = tswap64((typeof(*hptr))(x));\
        break;\
    default:\
        abort();\
    } \
    PRAGMA_REENABLE_PACKED_WARNING;\
    0;\
})

#define __get_user(x, hptr) \
({\
    PRAGMA_DISABLE_PACKED_WARNING;\
    int size = sizeof(*hptr);\
    switch (size) {\
    case 1:\
        x = (typeof(*hptr))*(uint8_t *)(hptr);\
        break;\
    case 2:\
        x = (typeof(*hptr))tswap16(*(uint16_t *)(hptr));\
        break;\
    case 4:\
        x = (typeof(*hptr))tswap32(*(uint32_t *)(hptr));\
        break;\
    case 8:\
        x = (typeof(*hptr))tswap64(*(uint64_t *)(hptr));\
        break;\
    default:\
        x = 0;\
        abort();\
    } \
    PRAGMA_REENABLE_PACKED_WARNING;\
    0;\
})

/*
 * put_user()/get_user() take a guest address and check access
 *
 * These are usually used to access an atomic data type, such as an int, that
 * has been passed by address.  These internally perform locking and unlocking
 * on the data type.
 */
#define put_user(x, gaddr, target_type)                                 \
({                                                                      \
    abi_ulong __gaddr = (gaddr);                                        \
    target_type *__hptr;                                                \
    abi_long __ret;                                                     \
    __hptr = lock_user(VERIFY_WRITE, __gaddr, sizeof(target_type), 0);  \
    if (__hptr) {                                                       \
        __ret = __put_user((x), __hptr);                                \
        unlock_user(__hptr, __gaddr, sizeof(target_type));              \
    } else                                                              \
        __ret = -TARGET_EFAULT;                                         \
    __ret;                                                              \
})

#define get_user(x, gaddr, target_type)                                 \
({                                                                      \
    abi_ulong __gaddr = (gaddr);                                        \
    target_type *__hptr;                                                \
    abi_long __ret;                                                     \
    __hptr = lock_user(VERIFY_READ, __gaddr, sizeof(target_type), 1);   \
    if (__hptr) {                                                       \
        __ret = __get_user((x), __hptr);                                \
        unlock_user(__hptr, __gaddr, 0);                                \
    } else {                                                            \
        (x) = 0;                                                        \
        __ret = -TARGET_EFAULT;                                         \
    }                                                                   \
    __ret;                                                              \
})

#define put_user_ual(x, gaddr) put_user((x), (gaddr), abi_ulong)
#define put_user_sal(x, gaddr) put_user((x), (gaddr), abi_long)
#define put_user_u64(x, gaddr) put_user((x), (gaddr), uint64_t)
#define put_user_s64(x, gaddr) put_user((x), (gaddr), int64_t)
#define put_user_u32(x, gaddr) put_user((x), (gaddr), uint32_t)
#define put_user_s32(x, gaddr) put_user((x), (gaddr), int32_t)
#define put_user_u16(x, gaddr) put_user((x), (gaddr), uint16_t)
#define put_user_s16(x, gaddr) put_user((x), (gaddr), int16_t)
#define put_user_u8(x, gaddr)  put_user((x), (gaddr), uint8_t)
#define put_user_s8(x, gaddr)  put_user((x), (gaddr), int8_t)

#define get_user_ual(x, gaddr) get_user((x), (gaddr), abi_ulong)
#define get_user_sal(x, gaddr) get_user((x), (gaddr), abi_long)
#define get_user_u64(x, gaddr) get_user((x), (gaddr), uint64_t)
#define get_user_s64(x, gaddr) get_user((x), (gaddr), int64_t)
#define get_user_u32(x, gaddr) get_user((x), (gaddr), uint32_t)
#define get_user_s32(x, gaddr) get_user((x), (gaddr), int32_t)
#define get_user_u16(x, gaddr) get_user((x), (gaddr), uint16_t)
#define get_user_s16(x, gaddr) get_user((x), (gaddr), int16_t)
#define get_user_u8(x, gaddr)  get_user((x), (gaddr), uint8_t)
#define get_user_s8(x, gaddr)  get_user((x), (gaddr), int8_t)

/*
 * copy_from_user() and copy_to_user() are usually used to copy data
 * buffers between the target and host.  These internally perform
 * locking/unlocking of the memory.
 */
abi_long copy_from_user(void *hptr, abi_ulong gaddr, size_t len);
abi_long copy_to_user(abi_ulong gaddr, void *hptr, size_t len);

/*
 * Functions for accessing guest memory.  The tget and tput functions
 * read/write single values, byteswapping as necessary.  The lock_user function
 * gets a pointer to a contiguous area of guest memory, but does not perform
 * any byteswapping.  lock_user may return either a pointer to the guest
 * memory, or a temporary buffer.
 */

/*
 * Lock an area of guest memory into the host.  If copy is true then the
 * host area will have the same contents as the guest.
 */
static inline void *lock_user(int type, abi_ulong guest_addr, long len,
                              int copy)
{
    if (!access_ok(type, guest_addr, len)) {
        return NULL;
    }
#ifdef DEBUG_REMAP
    {
        void *addr;
        addr = g_malloc(len);
        if (copy) {
            memcpy(addr, g2h_untagged(guest_addr), len);
        } else {
            memset(addr, 0, len);
        }
        return addr;
    }
#else
    return g2h_untagged(guest_addr);
#endif
}

/*
 * Unlock an area of guest memory.  The first LEN bytes must be flushed back to
 * guest memory. host_ptr = NULL is explicitly allowed and does nothing.
 */
static inline void unlock_user(void *host_ptr, abi_ulong guest_addr,
                               long len)
{

#ifdef DEBUG_REMAP
    if (!host_ptr) {
        return;
    }
    if (host_ptr == g2h_untagged(guest_addr)) {
        return;
    }
    if (len > 0) {
        memcpy(g2h_untagged(guest_addr), host_ptr, len);
    }
    g_free(host_ptr);
#endif
}

/*
 * Return the length of a string in target memory or -TARGET_EFAULT if access
 * error.
 */
abi_long target_strlen(abi_ulong gaddr);

/* Like lock_user but for null terminated strings.  */
static inline void *lock_user_string(abi_ulong guest_addr)
{
    abi_long len;
    len = target_strlen(guest_addr);
    if (len < 0) {
        return NULL;
    }
    return lock_user(VERIFY_READ, guest_addr, (long)(len + 1), 1);
}

/* Helper macros for locking/unlocking a target struct.  */
#define lock_user_struct(type, host_ptr, guest_addr, copy)      \
    (host_ptr = lock_user(type, guest_addr, sizeof(*host_ptr), copy))
#define unlock_user_struct(host_ptr, guest_addr, copy)          \
    unlock_user(host_ptr, guest_addr, (copy) ? sizeof(*host_ptr) : 0)

#if TARGET_ABI_BITS == 32
static inline uint64_t
target_arg64(uint32_t word0, uint32_t word1)
{
#ifdef TARGET_WORDS_BIGENDIAN
    return ((uint64_t)word0 << 32) | word1;
#else
    return ((uint64_t)word1 << 32) | word0;
#endif
}
#else /* TARGET_ABI_BITS != 32 */
static inline uint64_t
target_arg64(uint64_t word0, uint64_t word1)
{
    return word0;
}
#endif /* TARGET_ABI_BITS != 32 */

/* ARM EABI and 32-bit powerpc have aligned even on pairs of registers */
static inline int regpairs_aligned(void *cpu_env) {
#if TARGET_ABI_BITS == 32 && !defined(TARGET_I386)
    return 1;
#else
    return 0;
#endif
}

#include <pthread.h>

#include "user/safe-syscall.h"

#endif /* QEMU_H */
