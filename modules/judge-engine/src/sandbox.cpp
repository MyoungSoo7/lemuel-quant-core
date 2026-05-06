#include "judge/sandbox.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef __linux__
#include <sys/wait.h>
#include <unistd.h>
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

// Dev/macOS path: popen + temp stdin file. No real isolation.
ExecutionResult Sandbox::run(const std::filesystem::path& executable,
                             std::string_view stdin_data) {
    ExecutionResult result;

    const auto stdin_path = std::filesystem::temp_directory_path() /
                            "lqc-judge-stdin.txt";
    {
        std::ofstream sin(stdin_path);
        sin.write(stdin_data.data(),
                  static_cast<std::streamsize>(stdin_data.size()));
    }

    std::string cmd = executable.string() + " < " + stdin_path.string() +
                      " 2>&1";

    const auto start = std::chrono::steady_clock::now();
    std::FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) {
        result.verdict = Verdict::InternalError;
        return result;
    }

    char buf[4096];
    std::string captured;
    while (std::size_t n = std::fread(buf, 1, sizeof(buf), pipe)) {
        captured.append(buf, n);
        if (captured.size() > limits_.output_bytes) {
            result.verdict = Verdict::OutputLimitExceeded;
            ::pclose(pipe);
            return result;
        }
    }

    const int status = ::pclose(pipe);
    const auto end = std::chrono::steady_clock::now();
    result.wall_time_used =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.exit_code = status;
    result.stdout_data = std::move(captured);
    result.verdict = (status == 0) ? Verdict::Accepted : Verdict::RuntimeError;
    if (result.wall_time_used > limits_.wall_time) {
        result.verdict = Verdict::TimeLimitExceeded;
    }
    return result;
}

#endif

}  // namespace judge
