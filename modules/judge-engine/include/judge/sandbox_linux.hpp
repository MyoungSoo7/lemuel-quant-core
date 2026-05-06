#pragma once

// Linux-specific sandbox hardening primitives. Compiled-in only on Linux.
// Provides:
//   - seccomp-bpf syscall whitelist
//   - cgroup v2 memory + CPU limits
//   - setrlimit fallbacks (always available)
//   - non-root drop + chroot helper

#ifdef __linux__

#include <chrono>
#include <filesystem>
#include <string>
#include <sys/resource.h>

namespace judge::linux_sandbox {

struct CgroupSpec {
    std::filesystem::path mount_point{"/sys/fs/cgroup"};
    std::string slice{"lqc-judge"};
    std::size_t memory_max_bytes{0};
    std::uint64_t cpu_quota_us{0};   // per 100ms period
    std::uint64_t cpu_period_us{100000};
};

// Create or reuse a cgroup v2 leaf for this judge run.
// Returns the cgroup path. Throws on failure.
std::filesystem::path create_cgroup(const CgroupSpec& spec,
                                    const std::string& run_id);

// Move the calling process (or pid) into the given cgroup.
void attach_pid_to_cgroup(const std::filesystem::path& cgroup_path,
                          int pid);

// Apply setrlimit limits (CPU seconds, memory, output, stack).
// Call from the child process *before* execve.
void apply_rlimits(std::chrono::milliseconds cpu_time,
                   std::size_t memory_bytes,
                   std::size_t output_bytes,
                   std::size_t stack_bytes);

// Install a seccomp-bpf filter restricting syscalls to a safe whitelist
// for compiled binaries. Call from the child *after* execve preparation
// but *before* actually running user code.
//
// Returns true on success. Soft-fails to false if libseccomp not linked.
bool install_seccomp_whitelist();

// Drop privileges to nobody and chroot to a readonly rootfs.
// Returns true on success.
bool drop_privileges(const std::filesystem::path& chroot_dir,
                     const std::string& user = "nobody");

}  // namespace judge::linux_sandbox

#endif  // __linux__
