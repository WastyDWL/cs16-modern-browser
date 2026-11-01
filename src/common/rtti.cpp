#include "common/rtti.h"

#include <cstring>

namespace nc {

void** find_vtable(HMODULE m, const char* type_name, uint32_t offset) {
    auto* base = reinterpret_cast<uint8_t*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const uint32_t size = nt->OptionalHeader.SizeOfImage;
    const size_t len = std::strlen(type_name);
    uint8_t* td = nullptr;  // TypeDescriptor: { vftable, spare, name[] }
    for (uint32_t i = 8; i + len < size; ++i)
        if (std::memcmp(base + i, type_name, len + 1) == 0) {
            td = base + i - 8;
            break;
        }
    if (!td) return nullptr;
    for (uint32_t i = 0; i + 20 <= size; i += 4) {
        auto* col = reinterpret_cast<uint32_t*>(base + i);  // { signature, offset, cdOffset, pTD, pCHD }
        if (col[0] != 0 || col[1] != offset || col[3] != reinterpret_cast<uint32_t>(td)) continue;
        for (uint32_t j = 0; j + 8 <= size; j += 4)
            if (*reinterpret_cast<uint32_t*>(base + j) == reinterpret_cast<uint32_t>(col))
                return reinterpret_cast<void**>(base + j + 4);
    }
    return nullptr;
}

bool readable(const void* p, size_t size) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!p || !VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    auto end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    return static_cast<const uint8_t*>(p) + size <= end;
}

bool in_module_image(const void* p) {
    HMODULE m = nullptr;
    return p && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   static_cast<LPCWSTR>(p), &m);
}

}  // namespace nc
