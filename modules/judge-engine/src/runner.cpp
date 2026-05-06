#include "judge/runner.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace judge {

namespace {

std::string normalize(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\r') continue;
        out.push_back(c);
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) {
        out.pop_back();
    }
    return out;
}

}  // namespace

Runner::Runner(std::filesystem::path workdir) : workdir_(std::move(workdir)) {
    std::filesystem::create_directories(workdir_);
}

JudgeResult Runner::judge(const Submission& submission) {
    JudgeResult result;
    result.overall = Verdict::InternalError;

    const auto src_path = workdir_ / "main.cpp";
    const auto bin_path = workdir_ / "main";

    {
        std::ofstream src(src_path);
        src << submission.source;
    }

    std::ostringstream compile_cmd;
    compile_cmd << "c++ -std=c++20 -O2 -static-libstdc++ "
                << src_path.string() << " -o " << bin_path.string()
                << " 2>&1";
    std::string log;
    if (std::FILE* pipe = ::popen(compile_cmd.str().c_str(), "r")) {
        char buf[1024];
        while (std::fgets(buf, sizeof(buf), pipe)) log.append(buf);
        const int rc = ::pclose(pipe);
        result.compile_log = log;
        if (rc != 0) {
            result.overall = Verdict::CompileError;
            return result;
        }
    } else {
        return result;
    }

    Sandbox sandbox(submission.limits);
    Verdict overall = Verdict::Accepted;

    for (const auto& tc : submission.cases) {
        auto exec = sandbox.run(bin_path, tc.input);
        CaseResult cr{
            .verdict = exec.verdict,
            .wall_time_used = exec.wall_time_used,
            .memory_used = exec.memory_used,
            .actual_output = exec.stdout_data,
        };
        if (cr.verdict == Verdict::Accepted &&
            normalize(exec.stdout_data) != normalize(tc.expected_output)) {
            cr.verdict = Verdict::WrongAnswer;
        }
        if (cr.verdict != Verdict::Accepted && overall == Verdict::Accepted) {
            overall = cr.verdict;
        }
        result.per_case.push_back(std::move(cr));
    }

    result.overall = overall;
    return result;
}

}  // namespace judge
