#include "dart/disclosure_store.hpp"

#include <algorithm>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef LQC_HAS_LIBPQXX
#include <pqxx/pqxx>
#endif

namespace dart {

namespace {

class InMemoryStore : public DisclosureStore {
public:
    bool insert(const Disclosure& d) override {
        std::lock_guard lk(m_);
        if (!by_id_.emplace(d.rcept_no, d).second) return false;
        order_.push_back(d.rcept_no);
        return true;
    }
    std::optional<Disclosure> find(const std::string& rcept_no) override {
        std::lock_guard lk(m_);
        const auto it = by_id_.find(rcept_no);
        if (it == by_id_.end()) return std::nullopt;
        return it->second;
    }
    std::vector<Disclosure> recent(int limit) override {
        std::lock_guard lk(m_);
        std::vector<Disclosure> out;
        const auto count = std::min<std::size_t>(
            order_.size(), static_cast<std::size_t>(std::max(0, limit)));
        out.reserve(count);
        for (auto it = order_.rbegin();
             it != order_.rend() && out.size() < count; ++it) {
            out.push_back(by_id_[*it]);
        }
        return out;
    }

private:
    std::mutex m_;
    std::unordered_map<std::string, Disclosure> by_id_;
    std::vector<std::string> order_;
};

#ifdef LQC_HAS_LIBPQXX

class PostgresStore : public DisclosureStore {
public:
    explicit PostgresStore(const std::string& dsn) : conn_(dsn) {
        ensure_schema();
    }

    bool insert(const Disclosure& d) override {
        try {
            pqxx::work tx(conn_);
            const auto r = tx.exec_params(
                "INSERT INTO dart_disclosure "
                "(rcept_no, corp_code, corp_name, stock_code, corp_cls, "
                " report_nm, flr_nm, rcept_dt, rm) "
                "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9) "
                "ON CONFLICT (rcept_no) DO NOTHING",
                d.rcept_no, d.corp_code, d.corp_name, d.stock_code,
                d.corp_cls, d.report_nm, d.flr_nm, d.rcept_dt, d.rm);
            tx.commit();
            return r.affected_rows() > 0;
        } catch (const std::exception& e) {
            std::cerr << "[dart-pg] insert failed: " << e.what() << "\n";
            return false;
        }
    }

    std::optional<Disclosure> find(const std::string& rcept_no) override {
        try {
            pqxx::nontransaction tx(conn_);
            const auto r = tx.exec_params(
                "SELECT rcept_no,corp_code,corp_name,stock_code,corp_cls,"
                "       report_nm,flr_nm,rcept_dt,rm "
                "FROM dart_disclosure WHERE rcept_no = $1", rcept_no);
            if (r.empty()) return std::nullopt;
            return row_to_disclosure(r[0]);
        } catch (const std::exception& e) {
            std::cerr << "[dart-pg] find failed: " << e.what() << "\n";
            return std::nullopt;
        }
    }

    std::vector<Disclosure> recent(int limit) override {
        std::vector<Disclosure> out;
        try {
            pqxx::nontransaction tx(conn_);
            const auto r = tx.exec_params(
                "SELECT rcept_no,corp_code,corp_name,stock_code,corp_cls,"
                "       report_nm,flr_nm,rcept_dt,rm "
                "FROM dart_disclosure "
                "ORDER BY rcept_no DESC LIMIT $1", limit);
            for (const auto& row : r) out.push_back(row_to_disclosure(row));
        } catch (const std::exception& e) {
            std::cerr << "[dart-pg] recent failed: " << e.what() << "\n";
        }
        return out;
    }

private:
    void ensure_schema() {
        pqxx::work tx(conn_);
        tx.exec0(
            "CREATE TABLE IF NOT EXISTS dart_disclosure ("
            "  rcept_no  TEXT PRIMARY KEY,"
            "  corp_code TEXT, corp_name TEXT, stock_code TEXT,"
            "  corp_cls  TEXT, report_nm TEXT, flr_nm TEXT,"
            "  rcept_dt  TEXT, rm TEXT,"
            "  inserted_at TIMESTAMPTZ DEFAULT now()"
            ")");
        tx.exec0(
            "CREATE INDEX IF NOT EXISTS idx_dart_disclosure_stock "
            "ON dart_disclosure(stock_code, rcept_dt DESC)");
        tx.commit();
    }

    static Disclosure row_to_disclosure(const pqxx::row& r) {
        Disclosure d;
        d.rcept_no   = r[0].as<std::string>("");
        d.corp_code  = r[1].as<std::string>("");
        d.corp_name  = r[2].as<std::string>("");
        d.stock_code = r[3].as<std::string>("");
        d.corp_cls   = r[4].as<std::string>("");
        d.report_nm  = r[5].as<std::string>("");
        d.flr_nm     = r[6].as<std::string>("");
        d.rcept_dt   = r[7].as<std::string>("");
        d.rm         = r[8].as<std::string>("");
        return d;
    }

    pqxx::connection conn_;
};

#endif  // LQC_HAS_LIBPQXX

}  // namespace

std::unique_ptr<DisclosureStore> DisclosureStore::make_in_memory() {
    return std::make_unique<InMemoryStore>();
}

std::unique_ptr<DisclosureStore> DisclosureStore::make_postgres(
    const std::string& dsn) {
#ifdef LQC_HAS_LIBPQXX
    try {
        return std::make_unique<PostgresStore>(dsn);
    } catch (const std::exception& e) {
        std::cerr << "[dart-pg] connect failed: " << e.what() << "\n";
        return nullptr;
    }
#else
    (void)dsn;
    std::cerr << "[dart-pg] libpqxx not linked; falling back to in-memory.\n";
    return nullptr;
#endif
}

}  // namespace dart
