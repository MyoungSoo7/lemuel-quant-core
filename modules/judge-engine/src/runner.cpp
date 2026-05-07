#include "judge/runner.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

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

// Per-submission unique subdir token. Combines pid + monotonic counter +
// 64-bit random — uniqueness even under heavy concurrency, no fs probes.
std::string make_run_id() {
    static std::atomic<std::uint64_t> counter{0};
    const auto seq = counter.fetch_add(1, std::memory_order_relaxed);
    static thread_local std::mt19937_64 rng{
        std::random_device{}() ^ static_cast<std::uint64_t>(::getpid())
        ^ static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count())};
    std::ostringstream os;
    os << std::hex << ::getpid() << "-" << seq << "-" << rng();
    return os.str();
}

// fork+execv compile. Pipes stderr+stdout into log. No shell.
struct CompileOutcome { int exit_code; std::string log; bool spawned; };
CompileOutcome compile_via_execv(const std::string& src,
                                  const std::string& bin) {
    CompileOutcome out{0, {}, false};
    int pipefd[2]{-1, -1};
    if (pipe(pipefd) < 0) return out;

    const pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return out; }
    out.spawned = true;
    if (pid == 0) {
        dup2(pipefd[1], 1);
        dup2(pipefd[1], 2);
        close(pipefd[0]);
        close(pipefd[1]);
        const char* argv[] = {
            "c++",
            "-std=c++20", "-O2",
            src.c_str(), "-o", bin.c_str(),
            nullptr,
        };
        execvp("c++", const_cast<char* const*>(argv));
        std::_Exit(127);
    }
    close(pipefd[1]);
    char buf[1024];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        out.log.append(buf, static_cast<std::size_t>(n));
    }
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    out.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return out;
}

}  // namespace

Runner::Runner(std::filesystem::path workdir) : workdir_(std::move(workdir)) {
    std::filesystem::create_directories(workdir_);
}

JudgeResult Runner::judge(const Submission& submission) {
    JudgeResult result;
    result.overall = Verdict::InternalError;

    // Per-submission isolated subdir — no race between concurrent gRPC
    // requests sharing the same Runner instance.
    const auto run_dir = workdir_ / make_run_id();
    std::error_code ec;
    if (!std::filesystem::create_directories(run_dir, ec) && ec) {
        return result;
    }
    struct Cleanup {
        std::filesystem::path p;
        ~Cleanup() { std::error_code e; std::filesystem::remove_all(p, e); }
    } cleanup{run_dir};

    const auto src_path = run_dir / "main.cpp";
    const auto bin_path = run_dir / "main";

    {
        std::ofstream src(src_path);
        src << submission.source;
    }

    const auto compile = compile_via_execv(src_path.string(),
                                            bin_path.string());
    result.compile_log = compile.log;
    if (!compile.spawned) {
        return result;   // InternalError
    }
    if (compile.exit_code != 0) {
        result.overall = Verdict::CompileError;
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
