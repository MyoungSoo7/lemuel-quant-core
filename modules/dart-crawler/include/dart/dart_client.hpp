#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace dart {

// DART OpenAPI 공시 레코드. /api/list.json 응답의 핵심 필드만 보관.
struct Disclosure {
    std::string corp_code;          // 8자리 고유번호
    std::string corp_name;
    std::string stock_code;         // 6자리 종목코드 (상장사 한정)
    std::string corp_cls;           // Y(유가) / K(코스닥) / N(코넥스) / E(기타)
    std::string report_nm;          // 공시 제목
    std::string rcept_no;           // 접수번호 (PK)
    std::string flr_nm;             // 제출인
    std::string rcept_dt;           // YYYYMMDD
    std::string rm;                 // 비고 (정정/철회 등)
};

struct ListQuery {
    std::string corp_code;          // 종목 한정시
    std::string bgn_de;             // YYYYMMDD
    std::string end_de;             // YYYYMMDD
    std::string pblntf_ty;          // 공시유형 (A/B/...)
    int page_no{1};
    int page_count{100};
};

class DartClient {
public:
    explicit DartClient(std::string api_key,
                        std::string base_url = "https://opendart.fss.or.kr");
    ~DartClient();

    // 동기 1회 조회. 반환은 페이지 결과.
    std::vector<Disclosure> list(const ListQuery& q);

    // 폴링 모드. 마지막 본 rcept_no를 기억하여 새 공시만 콜백으로 흘림.
    void poll_loop(std::chrono::seconds interval,
                   std::function<void(const Disclosure&)> on_new,
                   std::function<bool()> keep_running);

private:
    struct Impl;
    std::unique_ptr<struct Impl> impl_;
};

}  // namespace dart
