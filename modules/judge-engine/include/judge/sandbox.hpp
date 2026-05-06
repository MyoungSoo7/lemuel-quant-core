#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace judge {

struct SandboxLimits {
    std::chrono::milliseconds wall_time{2000};
    std::chrono::milliseconds cpu_time{1000};
    std::size_t memory_bytes{256 * 1024 * 1024};
    std::size_t output_bytes{64 * 1024};
    std::size_t stack_bytes{64 * 1024 * 1024};
};

enum class Verdict {
    Accepted,
    WrongAnswer,
    TimeLimitExceeded,
    MemoryLimitExceeded,
    OutputLimitExceeded,
    RuntimeError,
    CompileError,
    InternalError,
};

struct ExecutionResult {
    Verdict verdict{Verdict::InternalError};
    std::chrono::milliseconds wall_time_used{0};
    std::chrono::milliseconds cpu_time_used{0};
    std::size_t memory_used{0};
    int exit_code{0};
    std::string stdout_data;
    std::string stderr_data;
};

class Sandbox {
public:
    explicit Sandbox(SandboxLimits limits);

    ExecutionResult run(const std::filesystem::path& executable,
                        std::string_view stdin_data);

private:
    SandboxLimits limits_;
};

std::string_view to_string(Verdict v);

}  // namespace judge
