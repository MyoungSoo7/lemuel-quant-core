#include "judge/sandbox.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux__
#include "judge/sandbox_linux.hpp"
#endif

namespace judge {

std::string_view to_string(Verdict v) {
    switch (v) {
        case Verdict::Accepted: return "AC";
        case Verdict::WrongAnswer: return "WA";
        case Verdict::TimeLimitExceeded: return "TLE";
        case Verdict::MemoryLimitExceeded: return "MLE";
        case Verdict::OutputLimitExceeded: return "OLE";
        case Verdict::RuntimeError: return "RE";
        case Verdict::CompileError: return "CE";
        case Verdict::InternalError: return "IE";
    }
    return "??";
}

Sandbox::Sandbox(SandboxLimits limits) : limits_(limits) {}

#ifdef __linux__

// Linux production path: fork → cgroup attach → setrlimit → seccomp → execve.
// stdin via pipe, stdout via pipe, stderr captured separately.
ExecutionResult Sandbox::run(const std::filesystem::path& executable,
                             std::string_view stdin_data) {
    ExecutionResult result;

    int in_pipe[2]{-1, -1};
    int out_pipe[2]{-1, -1};
    if (::pipe(in_pipe) < 0 || ::pipe(out_pipe) < 0) {
        result.verdict = Verdict::InternalError;
        return result;
    }

    namespace ls = judge::linux_sandbox;
    ls::CgroupSpec cspec;
    cspec.memory_max_bytes = limits_.memory_bytes;
    cspec.cpu_quota_us = limits_.cpu_time.count() * 1000;
    const std::string run_id =
        "run-" + std::to_string(::getpid()) + "-" +
        std::to_string(std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count());

    std::filesystem::path cgroup_leaf;
    try {
        cgroup_leaf = ls::create_cgroup(cspec, run_id);
    } catch (...) {
        // Soft-fail: continue with rlimits only.
    }

    const auto start = std::chrono::steady_clock::now();
    const pid_t pid = ::fork();
    if (pid < 0) {
        result.verdict = Verdict::InternalError;
        return result;
    }
    if (pid == 0) {
        ::dup2(in_pipe[0], 0);
        ::dup2(out_pipe[1], 1);
        ::dup2(out_pipe[1], 2);
        ::close(in_pipe[1]);
        ::close(out_pipe[0]);
        ls::apply_rlimits(limits_.cpu_time, limits_.memory_bytes,
                          limits_.output_bytes, limits_.stack_bytes);
        ls::install_seccomp_whitelist();
        char* argv[] = {const_cast<char*>(executable.c_str()), nullptr};
        ::execv(executable.c_str(), argv);
        std::_Exit(127);
    }

    if (!cgroup_leaf.empty()) {
        try { ls::attach_pid_to_cgroup(cgroup_leaf, pid); } catch (...) {}
    }

    ::close(in_pipe[0]);
    ::close(out_pipe[1]);
    if (!stdin_data.empty()) {
        ::write(in_pipe[1], stdin_data.data(), stdin_data.size());
    }
    ::close(in_pipe[1]);

    std::string captured;
    char buf[4096];
    ssize_t n;
    while ((n = ::read(out_pipe[0], buf, sizeof(buf))) > 0) {
        captured.append(buf, static_cast<std::size_t>(n));
        if (captured.size() > limits_.output_bytes) {
            ::kill(pid, SIGKILL);
            result.verdict = Verdict::OutputLimitExceeded;
            ::close(out_pipe[0]);
            int st;
            ::waitpid(pid, &st, 0);
            return result;
        }
    }
    ::close(out_pipe[0]);

    int status = 0;
    ::waitpid(pid, &status, 0);
    const auto end = std::chrono::steady_clock::now();
    result.wall_time_used =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.stdout_data = std::move(captured);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (WIFSIGNALED(status)) {
        const int sig = WTERMSIG(status);
        if (sig == SIGXCPU)        result.verdict = Verdict::TimeLimitExceeded;
        else if (sig == SIGKILL)   result.verdict = Verdict::MemoryLimitExceeded;
        else if (sig == SIGSEGV ||
                 sig == SIGBUS  ||
                 sig == SIGFPE)    result.verdict = Verdict::RuntimeError;
        else                       result.verdict = Verdict::RuntimeError;
    } else if (result.wall_time_used > limits_.wall_time) {
        result.verdict = Verdict::TimeLimitExceeded;
    } else if (result.exit_code != 0) {
        result.verdict = Verdict::RuntimeError;
    } else {
        result.verdict = Verdict::Accepted;
    }
    return result;
}

#else  // !__linux__

// Dev path (macOS/BSD): fork+execv with pipes — no shell interpretation,
// no isolation but no command injection either. The Linux path provides
// the real sandbox; this branch exists so dev iteration works.
ExecutionResult Sandbox::run(const std::filesystem::path& executable,
                             std::string_view stdin_data) {
    ExecutionResult result;

    int in_pipe[2]{-1, -1};
    int out_pipe[2]{-1, -1};
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
        result.verdict = Verdict::InternalError;
        return result;
    }

    const auto start = std::chrono::steady_clock::now();
    const pid_t pid = fork();
    if (pid < 0) {
        result.verdict = Verdict::InternalError;
        return result;
    }
    if (pid == 0) {
        dup2(in_pipe[0], 0);
        dup2(out_pipe[1], 1);
        dup2(out_pipe[1], 2);
        close(in_pipe[1]);
        close(out_pipe[0]);
        char* argv[] = {const_cast<char*>(executable.c_str()), nullptr};
        execv(executable.c_str(), argv);
        std::_Exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);
    if (!stdin_data.empty()) {
        const auto written = write(in_pipe[1], stdin_data.data(),
                                    stdin_data.size());
        (void)written;
    }
    close(in_pipe[1]);

    char buf[4096];
    std::string captured;
    ssize_t n;
    while ((n = read(out_pipe[0], buf, sizeof(buf))) > 0) {
        captured.append(buf, static_cast<std::size_t>(n));
        if (captured.size() > limits_.output_bytes) {
            kill(pid, SIGKILL);
            result.verdict = Verdict::OutputLimitExceeded;
            close(out_pipe[0]);
            int st;
            waitpid(pid, &st, 0);
            return result;
        }
    }
    close(out_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    const auto end = std::chrono::steady_clock::now();
    result.wall_time_used =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.stdout_data = std::move(captured);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (WIFSIGNALED(status)) {
        result.verdict = Verdict::RuntimeError;
    } else if (result.wall_time_used > limits_.wall_time) {
        result.verdict = Verdict::TimeLimitExceeded;
    } else if (result.exit_code != 0) {
        result.verdict = Verdict::RuntimeError;
    } else {
        result.verdict = Verdict::Accepted;
    }
    return result;
}

#endif

}  // namespace judge
