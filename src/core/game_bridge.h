// Game state for window.nextclient, read on the game's main thread through the engine's
// cl_enginefunc_t (the copy client.dll keeps) and GameUI's IGameUI.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/api_router.h"
#include "core/js_api.h"

namespace nc {

struct GameEvent {
    std::string target;  // "nextclient" or "nextclient.gameUi"
    std::string type;
    std::string detail_json;
};

class GameBridge final : public GameApi {
public:
    // Locates the engine function table and GameUI; safe to call more than once.
    void attach();
    bool attached() const { return engine_ != nullptr; }

    std::optional<std::string> cvar(const std::string& name) override;
    void client_cmd(const std::string& cmd) override;
    std::optional<ConnectedHost> host() override;
    bool gameui_shown() override;
    // Compares with the previous frame and returns connect/disconnect/putinserver/show/hide.
    std::vector<GameEvent> poll();

private:
    void** engine_ = nullptr;  // cl_enginefunc_t as an array of slots
    void* gameui_ = nullptr;   // IGameUI
    bool was_connected_ = false, was_joined_ = false, was_shown_ = false;
    std::string last_address_;
    int attempts_ = 0;
};

}  // namespace nc
