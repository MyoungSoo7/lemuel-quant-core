#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "judge/runner.hpp"
#include "lqc/version.hpp"

namespace {

std::string slurp(const std::filesystem::path& p) {
    std::ifstream f(p);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "lemuel-quant-core judge_engine " << lqc::kVersion << "\n"
                  << "usage: " << argv[0]
                  << " <source.cpp> <input.txt> <expected.txt>\n";
        return 2;
    }

    judge::Submission sub;
    sub.source = slurp(argv[1]);
    sub.cases.push_back({slurp(argv[2]), slurp(argv[3])});

    judge::Runner runner(std::filesystem::temp_directory_path() / "lqc-judge");
    const auto result = runner.judge(sub);

    std::cout << "verdict: " << judge::to_string(result.overall) << "\n";
    if (result.overall == judge::Verdict::CompileError) {
        std::cout << "compile_log:\n" << result.compile_log;
        return 1;
    }
    for (std::size_t i = 0; i < result.per_case.size(); ++i) {
        const auto& cr = result.per_case[i];
        std::cout << "case[" << i << "]: " << judge::to_string(cr.verdict)
                  << "  time=" << cr.wall_time_used.count() << "ms\n";
    }
    return result.overall == judge::Verdict::Accepted ? 0 : 1;
}
