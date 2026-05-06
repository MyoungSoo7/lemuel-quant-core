#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dart/dart_client.hpp"

namespace dart {

// PostgreSQL 저장소 추상화. 실제 구현은 libpqxx (find_package PostgreSQL).
// 인터페이스만 두고 in-memory 백엔드를 디폴트로 끼워 dev 빌드 보장.
class DisclosureStore {
public:
    virtual ~DisclosureStore() = default;

    // rcept_no 가 이미 저장되어 있으면 false 반환 (중복 무시).
    virtual bool insert(const Disclosure& d) = 0;

    virtual std::optional<Disclosure> find(const std::string& rcept_no) = 0;

    virtual std::vector<Disclosure> recent(int limit = 50) = 0;

    static std::unique_ptr<DisclosureStore> make_in_memory();
    // dsn 예: "postgres://lqc:pw@127.0.0.1:5432/lqc"
    // libpqxx 미링크 시 nullptr 반환.
    static std::unique_ptr<DisclosureStore> make_postgres(
        const std::string& dsn);
};

}  // namespace dart
