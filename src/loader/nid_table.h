#pragma once

#include "common/types.h"
#include <string>
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
    uintptr_t Resolve(const std::string& nid_prefix, const std::string& full_symbol = "") const;

    // Register host implementations for all known system modules
    void RegisterHostSubsystems();

    size_t GetRegisteredCount() const { return m_symbols.size(); }

private:
    NidTable() = default;
    std::unordered_map<std::string, uintptr_t> m_symbols;
    std::unordered_map<std::string, std::string> m_names;
};

} // namespace Loader
