#include "dart/disclosure_store.hpp"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

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

}  // namespace

std::unique_ptr<DisclosureStore> DisclosureStore::make_in_memory() {
    return std::make_unique<InMemoryStore>();
}

}  // namespace dart
