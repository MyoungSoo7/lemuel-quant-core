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
    auto ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (!ctx) return false;

    static const int allow[] = {
        SCMP_SYS(read),       SCMP_SYS(write),     SCMP_SYS(exit),
        SCMP_SYS(exit_group), SCMP_SYS(brk),       SCMP_SYS(mmap),
        SCMP_SYS(munmap),     SCMP_SYS(mprotect),  SCMP_SYS(rt_sigaction),
        SCMP_SYS(rt_sigreturn), SCMP_SYS(rt_sigprocmask),
        SCMP_SYS(fstat),      SCMP_SYS(lseek),     SCMP_SYS(ioctl),
        SCMP_SYS(uname),      SCMP_SYS(arch_prctl), SCMP_SYS(set_tid_address),
        SCMP_SYS(set_robust_list), SCMP_SYS(prlimit64), SCMP_SYS(getrandom),
        SCMP_SYS(readv),      SCMP_SYS(writev),    SCMP_SYS(close),
        SCMP_SYS(futex),      SCMP_SYS(clock_gettime), SCMP_SYS(gettimeofday),
        SCMP_SYS(nanosleep),  SCMP_SYS(getpid),    SCMP_SYS(gettid),
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
