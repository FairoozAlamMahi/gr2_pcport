#pragma once

#include "common/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Loader {

class NidTable {
public:
    static NidTable& Instance();

    void Initialize();

    // Register a function pointer under an 11-character NID hash
    void RegisterSymbol(const std::string& nid_prefix, uintptr_t func_addr, const std::string& name = "");

    // Resolve an 11-character NID prefix to a function address
    uintptr_t Resolve(const std::string& nid_prefix, const std::string& full_symbol = "");

    bool HasSymbol(const std::string& nid_prefix) const {
        return m_symbols.find(nid_prefix) != m_symbols.end();
    }

    // Register host implementations for all known system modules
    void RegisterHostSubsystems();

    size_t GetRegisteredCount() const { return m_symbols.size(); }
    const std::string& GetStubName(size_t stub_id) const {
        static const std::string s_unknown = "UnknownStub";
        if (stub_id < m_stub_entries.size()) return m_stub_entries[stub_id];
        return s_unknown;
    }

private:
    NidTable() = default;
    std::unordered_map<std::string, uintptr_t> m_symbols;
    std::unordered_map<std::string, std::string> m_names;
    std::vector<std::string> m_stub_entries;
    std::unordered_map<std::string, uintptr_t> m_stub_cache;
    u8* m_stub_arena{nullptr};
};

} // namespace Loader
