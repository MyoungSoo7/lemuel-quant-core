#include "judge/sandbox.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

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

// MVP stub: feeds stdin via a temp file, captures stdout via popen.
// Linux production path will use fork + seccomp-bpf + cgroup v2 + setrlimit.
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

}  // namespace judge
