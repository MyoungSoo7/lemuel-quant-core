#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "judge/sandbox.hpp"

namespace judge {

struct TestCase {
    std::string input;
    std::string expected_output;
};

struct Submission {
    std::string source;
    std::string language{"cpp"};
    SandboxLimits limits{};
    std::vector<TestCase> cases;
};

struct CaseResult {
    Verdict verdict;
    std::chrono::milliseconds wall_time_used;
    std::size_t memory_used;
    std::string actual_output;
};

struct JudgeResult {
    Verdict overall;
    std::vector<CaseResult> per_case;
    std::string compile_log;
};

class Runner {
public:
    explicit Runner(std::filesystem::path workdir);

    JudgeResult judge(const Submission& submission);

private:
    std::filesystem::path workdir_;
};

}  // namespace judge
