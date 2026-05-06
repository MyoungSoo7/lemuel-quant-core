#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "judge.grpc.pb.h"
#include "judge/runner.hpp"

namespace lqcv1 = lqc::judge::v1;

namespace {

lqcv1::Verdict to_pb(judge::Verdict v) {
    using enum judge::Verdict;
    switch (v) {
        case Accepted:            return lqcv1::AC;
        case WrongAnswer:         return lqcv1::WA;
        case TimeLimitExceeded:   return lqcv1::TLE;
        case MemoryLimitExceeded: return lqcv1::MLE;
        case OutputLimitExceeded: return lqcv1::OLE;
        case RuntimeError:        return lqcv1::RE;
        case CompileError:        return lqcv1::CE;
        case InternalError:       return lqcv1::IE;
    }
    return lqcv1::UNKNOWN;
}

judge::Submission from_pb(const lqcv1::SubmitRequest& req) {
    judge::Submission s;
    s.source = std::string(req.source().begin(), req.source().end());
    s.language = req.language().empty() ? "cpp" : req.language();
    if (req.has_limits()) {
        if (req.limits().wall_time_ms())
            s.limits.wall_time =
                std::chrono::milliseconds(req.limits().wall_time_ms());
        if (req.limits().cpu_time_ms())
            s.limits.cpu_time =
                std::chrono::milliseconds(req.limits().cpu_time_ms());
        if (req.limits().memory_bytes())
            s.limits.memory_bytes = req.limits().memory_bytes();
        if (req.limits().output_bytes())
            s.limits.output_bytes = req.limits().output_bytes();
    }
    s.cases.reserve(req.cases_size());
    for (const auto& c : req.cases()) {
        s.cases.push_back({
            std::string(c.input().begin(), c.input().end()),
            std::string(c.expected_output().begin(), c.expected_output().end()),
        });
    }
    return s;
}

void fill_case(lqcv1::CaseResult* out, const judge::CaseResult& in) {
    out->set_verdict(to_pb(in.verdict));
    out->set_wall_time_ms(static_cast<std::uint32_t>(in.wall_time_used.count()));
    out->set_memory_bytes(in.memory_used);
    out->set_actual_output(in.actual_output);
}

class JudgeService final : public lqcv1::Judge::Service {
public:
    explicit JudgeService(std::filesystem::path workdir)
        : runner_(std::move(workdir)) {}

    grpc::Status Submit(grpc::ServerContext*,
                        const lqcv1::SubmitRequest* req,
                        lqcv1::SubmitResponse* resp) override {
        const auto r = runner_.judge(from_pb(*req));
        resp->set_overall(to_pb(r.overall));
        resp->set_compile_log(r.compile_log);
        for (const auto& c : r.per_case) fill_case(resp->add_cases(), c);
        return grpc::Status::OK;
    }

    grpc::Status Stream(grpc::ServerContext*,
                        const lqcv1::SubmitRequest* req,
                        grpc::ServerWriter<lqcv1::CaseUpdate>* writer) override {
        const auto r = runner_.judge(from_pb(*req));
        for (std::size_t i = 0; i < r.per_case.size(); ++i) {
            lqcv1::CaseUpdate u;
            u.set_case_index(static_cast<std::uint32_t>(i));
            fill_case(u.mutable_result(), r.per_case[i]);
            u.set_is_last(i + 1 == r.per_case.size());
            if (!writer->Write(u)) break;
        }
        return grpc::Status::OK;
    }

private:
    judge::Runner runner_;
};

}  // namespace

int main(int argc, char** argv) {
    const std::string addr = (argc > 1) ? argv[1] : "0.0.0.0:50051";
    const auto workdir =
        std::filesystem::temp_directory_path() / "lqc-judge-grpc";

    JudgeService service(workdir);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    auto server = builder.BuildAndStart();
    std::cout << "judge-engine gRPC listening on " << addr << "\n";
    server->Wait();
    return 0;
}
