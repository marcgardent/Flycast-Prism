#pragma once

#include "hw/pvr/Renderer_if.h"
#include "flycast_plugin_api.h"

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

    void* plugin_handle = nullptr;
    const FlycastPluginVTable* vtable = nullptr;
};

} // namespace rend
