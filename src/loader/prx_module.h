#pragma once

#include "common/types.h"
#include <string>
#include <vector>

namespace Loader {

struct PrxSegment {
    u32 type;
    u32 flags;
    u64 offset;
    u64 vaddr;
    u64 filesz;
    u64 memsz;
};

class PrxModule {
public:
    PrxModule() = default;
    ~PrxModule() = default;

    bool Load(const std::string& prx_path, uintptr_t base_address);
    bool RelocateAndLink();

    const std::string& GetName() const { return m_name; }
    uintptr_t GetBaseAddress() const { return m_base_address; }
    size_t GetExportCount() const { return m_exports_count; }

private:
    bool ParseAndMapSegments(const u8* data, size_t size);
    bool RewriteTlsInstructions(const u8* data);
    bool ApplyRelocations(const u8* data);
    bool RegisterExports(const u8* data);

    std::string m_name;
    uintptr_t m_base_address{0};
    size_t m_exports_count{0};
    std::vector<PrxSegment> m_segments;
    std::vector<u8> m_file_buffer;
};

} // namespace Loader
