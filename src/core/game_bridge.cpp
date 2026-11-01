#include "core/game_bridge.h"

#include <windows.h>

#include <cstring>

#include "common/log.h"
#include "common/rtti.h"
#include "common/valve_html.h"

namespace nc {

namespace {

// cl_enginefunc_t slots (NextClient goldsrc_abi/client_api.h); 0x87 slots in total.
constexpr int kEngineSlots = 0x87;
constexpr int kGetCvarPointer = 72, kClientCmd = 20, kGetPlayerInfo = 21, kGetLocalPlayer = 51,
              kGetLevelName = 74, kNetApi = 86;
constexpr int kNetApiStatus = 1;      // net_api_t: InitNetworking, Status, ...
constexpr int kGameUiIsActive = 11;   // IGameUI : IBaseInterface (destructor at slot 0)

struct cvar_t {
    char* name;
    char* string;
    int flags;
    float value;
    cvar_t* next;
};
struct hud_player_info_t {
    char* name;
    short ping;
    uint8_t thisplayer, spectator, packetloss;
    char* model;
    short topcolor, bottomcolor;
    uint64_t steam_id;
};
struct netadr_t {
    int type;
    unsigned char ip[4];
    unsigned char ipx[10];
    unsigned short port;  // network byte order
};
struct net_status_t {
    int connected;
    netadr_t local_address;
    netadr_t remote_address;
    int packet_loss;
    double latency, connection_time, rate;
};

// client.dll's Initialize starts with `mov edi, offset gEngfuncs; ... mov ecx, 87h; rep movsd`.
void** find_client_engine_table() {
    HMODULE client = GetModuleHandleW(L"client.dll");
    auto* init = client ? reinterpret_cast<const uint8_t*>(GetProcAddress(client, "Initialize")) : nullptr;
    if (!init || !readable(init, 0x60)) return nullptr;
    bool count_ok = false;
    for (int i = 0; i + 5 <= 0x60; ++i)
        if (init[i] == 0xB9 && *reinterpret_cast<const uint32_t*>(init + i + 1) == kEngineSlots) count_ok = true;
    if (!count_ok) return nullptr;
    for (int i = 0; i + 7 <= 0x60; ++i) {
        if (init[i] != 0xBF || init[i + 5] != 0xF3 || init[i + 6] != 0xA5) continue;
        auto** table = *reinterpret_cast<void** const*>(init + i + 1);
        if (!readable(table, sizeof(void*) * kEngineSlots)) return nullptr;
        for (int s : {kGetCvarPointer, kClientCmd, kGetPlayerInfo, kGetLocalPlayer, kGetLevelName})
            if (!in_module_image(table[s])) return nullptr;
        return table;
    }
    return nullptr;
}

template <class Fn>
Fn slot(void** table, int i) {
    return reinterpret_cast<Fn>(table[i]);
}

}  // namespace

void GameBridge::attach() {
    if (!engine_) {
        engine_ = find_client_engine_table();
        if (engine_) log("game bridge: engine functions at %p", engine_);
        else if (attempts_++ == 0) log("game bridge: engine functions not found yet (client.dll Initialize)");
    }
    if (!gameui_) {
        HMODULE m = GetModuleHandleW(L"GameUI.dll");
        auto create = m ? reinterpret_cast<CreateInterfaceFn>(GetProcAddress(m, "CreateInterface")) : nullptr;
        gameui_ = create ? create("GameUI007", nullptr) : nullptr;
    }
}

std::optional<std::string> GameBridge::cvar(const std::string& name) {
    if (!engine_) return std::nullopt;
    auto* c = slot<cvar_t* (*)(const char*)>(engine_, kGetCvarPointer)(name.c_str());
    if (!c || !c->string) return std::nullopt;
    return std::string(c->string);
}

void GameBridge::client_cmd(const std::string& cmd) {
    if (!engine_) return;
    std::string copy = cmd;  // pfnClientCmd takes a char*
    slot<int (*)(char*)>(engine_, kClientCmd)(copy.data());
}

std::optional<ConnectedHost> GameBridge::host() {
    if (!engine_) return std::nullopt;
    auto** net_api = static_cast<void**>(engine_[kNetApi]);
    if (!readable(net_api, sizeof(void*) * 2)) return std::nullopt;
    net_status_t st{};
    slot<void (*)(net_status_t*)>(net_api, kNetApiStatus)(&st);
    if (!st.connected) return std::nullopt;

    ConnectedHost h;
    const unsigned char* ip = st.remote_address.ip;
    char addr[32];
    std::snprintf(addr, sizeof(addr), "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3],
                  static_cast<unsigned>(((st.remote_address.port & 0xFF) << 8) | (st.remote_address.port >> 8)));
    h.address = addr;
    const char* level = slot<const char* (*)()>(engine_, kGetLevelName)();
    h.joined = slot<void* (*)()>(engine_, kGetLocalPlayer)() != nullptr && level && *level;
    for (int i = 1; i <= 32; ++i) {
        hud_player_info_t info{};
        slot<void (*)(int, hud_player_info_t*)>(engine_, kGetPlayerInfo)(i, &info);
        if (!info.name || !*info.name) continue;
        h.players.push_back({info.name, info.steam_id, info.thisplayer != 0});
    }
    return h;
}

bool GameBridge::gameui_shown() {
    if (!gameui_) return false;
    return slot<bool(__thiscall*)(void*)>(*static_cast<void***>(gameui_), kGameUiIsActive)(gameui_);
}

std::vector<GameEvent> GameBridge::poll() {
    std::vector<GameEvent> events;
    if (!engine_) return events;
    std::optional<ConnectedHost> h = host();
    bool connected = h.has_value(), joined = h && h->joined;
    std::string address = h ? h->address : last_address_;
    std::string detail = "{\"address\":" + json_string(address) + "}";
    if (connected && !was_connected_) events.push_back({"nextclient", "connect", detail});
    if (joined && !was_joined_) events.push_back({"nextclient", "putinserver", detail});
    if (!connected && was_connected_) events.push_back({"nextclient", "disconnect", detail});
    was_connected_ = connected;
    was_joined_ = joined;
    if (connected) last_address_ = address;

    bool shown = gameui_shown();
    if (shown != was_shown_) events.push_back({"nextclient.gameUi", shown ? "show" : "hide", "null"});
    was_shown_ = shown;
    return events;
}

}  // namespace nc
