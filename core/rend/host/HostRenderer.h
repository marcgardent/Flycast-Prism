#pragma once

#include "hw/pvr/Renderer_if.h"
#include "flycast_plugin_api.h"
#include "PluginAssetCache.h"
#include <map>
#include <vector>
#include <stdint.h>

namespace rend {

    class HostRenderer : public Renderer {
    public:
        HostRenderer();
        virtual ~HostRenderer();

        bool Init() override;
        void Term() override;

        void Process(TA_context *ctx) override;
        bool Render() override;
        void RenderFramebuffer(const FramebufferInfo& info) override;
        bool Present() override;

    private:
        bool loadPlugin();
        void unloadPlugin();
        FlycastWindowHandle getWindowHandle();
        void checkResize();

        // Host interface implementation
        static const char* GetGameId(FlycastHostHandle host);
        static const char* GetHostName(FlycastHostHandle host);
        static const char* GetHostVersion(FlycastHostHandle host);
        static void LogMessage(FlycastHostHandle host, FlycastLogLevel level, const char* message);

        void* plugin_handle = nullptr;
        const FlycastPluginVTable* vtable = nullptr;
        FlycastHostInterface host_interface;
        uint32_t last_width = 0;
        uint32_t last_height = 0;

        // State Tracking
        uint32_t last_palette_crc = 0;
        uint32_t last_fog_crc = 0;

        PluginAssetCache asset_cache;

        // Mega-batch command buffer reused per list to avoid allocations
        std::vector<FlycastDrawCommand> mega_commands;
    };

} // namespace rend