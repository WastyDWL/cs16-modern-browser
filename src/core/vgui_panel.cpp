#include "core/vgui_panel.h"

#include <windows.h>

#include <cstring>

#include "common/rtti.h"

namespace nc {

namespace {

// MSVC x86 RTTI records.
struct TypeDescriptor {
    const void* vftable;
    void* spare;
    char name[1];
};
struct BaseClassDescriptor {
    TypeDescriptor* type;
    uint32_t contained, mdisp, pdisp, vdisp, attributes;
};
struct ClassHierarchyDescriptor {
    uint32_t signature, attributes, num_bases;
    BaseClassDescriptor** bases;
};
struct CompleteObjectLocator {
    uint32_t signature, offset, cd_offset;
    TypeDescriptor* type;
    ClassHierarchyDescriptor* hierarchy;
};

constexpr char kPanelType[] = ".?AVPanel@vgui2@@";

bool derives_from_panel(const CompleteObjectLocator* col) {
    const ClassHierarchyDescriptor* chd = col->hierarchy;
    if (!readable(chd, sizeof(*chd)) || chd->num_bases > 256 ||
        !readable(chd->bases, sizeof(void*) * chd->num_bases))
        return false;
    for (uint32_t i = 0; i < chd->num_bases; ++i) {
        const BaseClassDescriptor* b = chd->bases[i];
        if (readable(b, sizeof(*b)) && readable(b->type, sizeof(TypeDescriptor) + sizeof(kPanelType)) &&
            std::strcmp(b->type->name, kPanelType) == 0)
            return true;
    }
    return false;
}

}  // namespace

uint32_t vgui_vpanel_of(IHTMLResponses_HL1* target) {
    if (!readable(target, sizeof(void*))) return 0;
    void** vtbl = *reinterpret_cast<void***>(target);
    if (!readable(vtbl - 1, sizeof(void*))) return 0;
    auto* col = static_cast<const CompleteObjectLocator*>(vtbl[-1]);
    if (!readable(col, sizeof(*col)) || col->signature != 0 || !derives_from_panel(col)) return 0;
    // vgui2::Panel starts at the full object; its first base is IClientPanel, slot 0 = GetVPanel.
    void* full = reinterpret_cast<char*>(target) - col->offset;
    using GetVPanel_t = uint32_t(__thiscall*)(void*);
    return (*reinterpret_cast<GetVPanel_t**>(full))[0](full);
}

bool vgui_panel_size(IHTMLResponses_HL1* target, int& wide, int& tall) {
    static void* ipanel = [] {
        HMODULE vgui2 = GetModuleHandleW(L"vgui2.dll");
        auto create = vgui2 ? reinterpret_cast<CreateInterfaceFn>(GetProcAddress(vgui2, "CreateInterface")) : nullptr;
        return create ? create("VGUI_Panel007", nullptr) : nullptr;
    }();
    if (!ipanel) return false;
    uint32_t vpanel = vgui_vpanel_of(target);
    if (!vpanel) return false;
    // IPanel : IBaseInterface — slot 0 is IBaseInterface's virtual destructor, then Init, SetPos,
    // GetPos, SetSize, GetSize. (Slot 4 is SetSize: calling it with int& resizes the panel.)
    constexpr int kGetSizeSlot = 5;
    using GetSize_t = void(__thiscall*)(void*, uint32_t, int&, int&);
    int w = -1, h = -1;
    (*reinterpret_cast<GetSize_t**>(ipanel))[kGetSizeSlot](ipanel, vpanel, w, h);
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) return false;
    wide = w;
    tall = h;
    return true;
}

}  // namespace nc
