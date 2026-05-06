#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "judge.grpc.pb.h"
#include "judge/runner.hpp"

namespace pb = lqc::judge::v1;

namespace {

pb::Verdict to_pb(judge::Verdict v) {
    using enum judge::Verdict;
    switch (v) {
        case Accepted:            return pb::AC;
        case WrongAnswer:         return pb::WA;
        case TimeLimitExceeded:   return pb::TLE;
        case MemoryLimitExceeded: return pb::MLE;
        case OutputLimitExceeded: return pb::OLE;
        case RuntimeError:        return pb::RE;
        case CompileError:        return pb::CE;
        case InternalError:       return pb::IE;
    }
    return pb::UNKNOWN;
}

judge::Submission from_pb(const pb::SubmitRequest& req) {
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

void fill_case(pb::CaseResult* out, const judge::CaseResult& in) {
    out->set_verdict(to_pb(in.verdict));
    out->set_wall_time_ms(static_cast<std::uint32_t>(in.wall_time_used.count()));
    out->set_memory_bytes(in.memory_used);
    out->set_actual_output(in.actual_output);
}

class JudgeService final : public pb::Judge::Service {
public:
    explicit JudgeService(std::filesystem::path workdir)
        : runner_(std::move(workdir)) {}

    grpc::Status Submit(grpc::ServerContext*,
                        const pb::SubmitRequest* req,
                        pb::SubmitResponse* resp) override {
        const auto r = runner_.judge(from_pb(*req));
        resp->set_overall(to_pb(r.overall));
        resp->set_compile_log(r.compile_log);
        for (const auto& c : r.per_case) fill_case(resp->add_cases(), c);
        return grpc::Status::OK;
    }

    grpc::Status Stream(grpc::ServerContext*,
                        const pb::SubmitRequest* req,
                        grpc::ServerWriter<pb::CaseUpdate>* writer) override {
        const auto r = runner_.judge(from_pb(*req));
        for (std::size_t i = 0; i < r.per_case.size(); ++i) {
            pb::CaseUpdate u;
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
