#ifdef __linux__

#include "judge/sandbox_linux.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef LQC_HAS_SECCOMP
#include <seccomp.h>
#endif

namespace judge::linux_sandbox {

namespace {

void write_to(const std::filesystem::path& p, const std::string& s) {
    std::ofstream f(p);
    if (!f) {
        throw std::runtime_error("cgroup write failed: " + p.string() +
                                 " (" + std::strerror(errno) + ")");
    }
    f << s;
}

}  // namespace

std::filesystem::path create_cgroup(const CgroupSpec& spec,
                                    const std::string& run_id) {
    const auto root = spec.mount_point / spec.slice;
    std::filesystem::create_directories(root);

    // Enable controllers on the slice.
    write_to(root / "cgroup.subtree_control", "+memory +cpu +pids");

    const auto leaf = root / run_id;
    std::filesystem::create_directories(leaf);

    if (spec.memory_max_bytes > 0) {
        write_to(leaf / "memory.max",
                 std::to_string(spec.memory_max_bytes));
    }
    if (spec.cpu_quota_us > 0) {
        write_to(leaf / "cpu.max",
                 std::to_string(spec.cpu_quota_us) + " " +
                     std::to_string(spec.cpu_period_us));
    }
    write_to(leaf / "pids.max", "32");
    return leaf;
}

void attach_pid_to_cgroup(const std::filesystem::path& cgroup_path,
                          int pid) {
    write_to(cgroup_path / "cgroup.procs", std::to_string(pid));
}

void apply_rlimits(std::chrono::milliseconds cpu_time,
                   std::size_t memory_bytes,
                   std::size_t output_bytes,
                   std::size_t stack_bytes) {
    auto set = [](int res, rlim_t v) {
        rlimit r{v, v};
        ::setrlimit(res, &r);
    };
    const auto cpu_sec =
        static_cast<rlim_t>((cpu_time.count() + 999) / 1000);
    set(RLIMIT_CPU, cpu_sec);
    set(RLIMIT_AS, static_cast<rlim_t>(memory_bytes));
    set(RLIMIT_DATA, static_cast<rlim_t>(memory_bytes));
    set(RLIMIT_STACK, static_cast<rlim_t>(stack_bytes));
    set(RLIMIT_FSIZE, static_cast<rlim_t>(output_bytes));
    set(RLIMIT_NPROC, 32);
    set(RLIMIT_CORE, 0);
}

bool install_seccomp_whitelist() {
#ifdef LQC_HAS_SECCOMP
    if (const char* dis = std::getenv("LQC_DISABLE_SECCOMP"); dis && *dis) {
        return false;
    }
    auto ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (!ctx) return false;

    // Expanded for glibc 2.35+ (Ubuntu 22.04+ / 24.04). Modern glibc startup
    // performs clone3 + rseq + getrlimit, and inlines many fstatat/openat
    // calls under exec — keep the list permissive enough that compiled
    // C/C++ binaries run, but still block network/filesystem mutation.
    static const int allow[] = {
        // execve is needed to actually launch the user binary (parent
        // installs the filter before calling execve itself). After exec,
        // user code generally doesn't need it again, but allow execveat
        // for static-pie / libc style entry.
        SCMP_SYS(execve),    SCMP_SYS(execveat),
        // I/O
        SCMP_SYS(read),      SCMP_SYS(write),    SCMP_SYS(readv),
        SCMP_SYS(writev),    SCMP_SYS(close),    SCMP_SYS(lseek),
        SCMP_SYS(pread64),   SCMP_SYS(pwrite64),
        SCMP_SYS(openat),    SCMP_SYS(open),     SCMP_SYS(access),
        SCMP_SYS(faccessat), SCMP_SYS(faccessat2),
        SCMP_SYS(dup),       SCMP_SYS(dup2),     SCMP_SYS(dup3),
        SCMP_SYS(pipe),      SCMP_SYS(pipe2),    SCMP_SYS(fcntl),
        SCMP_SYS(ioctl),
        // Memory
        SCMP_SYS(brk),       SCMP_SYS(mmap),     SCMP_SYS(munmap),
        SCMP_SYS(mremap),    SCMP_SYS(mprotect), SCMP_SYS(madvise),
        // Process / thread (compiled binary may use threads via libstdc++)
        SCMP_SYS(exit),      SCMP_SYS(exit_group),
        SCMP_SYS(getpid),    SCMP_SYS(gettid),   SCMP_SYS(tgkill),
        SCMP_SYS(clone),     SCMP_SYS(clone3),   SCMP_SYS(rseq),
        SCMP_SYS(set_tid_address), SCMP_SYS(set_robust_list),
        SCMP_SYS(arch_prctl), SCMP_SYS(prctl),
        SCMP_SYS(getrlimit), SCMP_SYS(setrlimit), SCMP_SYS(prlimit64),
        SCMP_SYS(uname),
        // Signals
        SCMP_SYS(rt_sigaction),  SCMP_SYS(rt_sigreturn),
        SCMP_SYS(rt_sigprocmask),SCMP_SYS(rt_sigpending),
        SCMP_SYS(sigaltstack),
        // Time / random
        SCMP_SYS(clock_gettime), SCMP_SYS(clock_nanosleep),
        SCMP_SYS(gettimeofday),  SCMP_SYS(nanosleep),
        SCMP_SYS(getrandom),
        // Stat (read-only)
        SCMP_SYS(fstat),     SCMP_SYS(newfstatat), SCMP_SYS(statx),
        SCMP_SYS(stat),      SCMP_SYS(lstat),
        SCMP_SYS(getdents64),
        // Sync
        SCMP_SYS(futex),     SCMP_SYS(futex_waitv),
        SCMP_SYS(membarrier),
    };
    for (int sc : allow) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, sc, 0) < 0) {
            seccomp_release(ctx);
            return false;
        }
    }
    const bool ok = seccomp_load(ctx) == 0;
    seccomp_release(ctx);
    return ok;
#else
    return false;  // libseccomp not linked at build time
#endif
}

bool drop_privileges(const std::filesystem::path& chroot_dir,
                     const std::string& /*user*/) {
    if (!chroot_dir.empty()) {
        if (::chroot(chroot_dir.c_str()) != 0) return false;
        if (::chdir("/") != 0) return false;
    }
    // Production: lookup uid/gid for `user` via getpwnam_r and setresgid/uid.
    // The MVP relies on the parent already running as a low-priv account.
    return true;
}

}  // namespace judge::linux_sandbox

#endif  // __linux__
