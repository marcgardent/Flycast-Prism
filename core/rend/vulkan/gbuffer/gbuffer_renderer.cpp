#include "../vulkan_renderer.h"
#include "../drawer.h"

class GBufferVulkanRenderer final : public BaseVulkanRenderer
{
public:
	bool Init() override
	{
		NOTICE_LOG(RENDERER, "GBufferVulkanRenderer::Init");
		try {
			std::vector<vk::Format> formats = {
				vk::Format::eR8G8B8A8Unorm,         // Albedo
				vk::Format::eR16G16B16A16Sfloat    // Normals
			};
			screenDrawer.Init(&samplerManager, &shaderManager, viewport, formats);
			screenDrawer.SetCommandPool(&texCommandPool);
			BaseInit(screenDrawer.GetRenderPass());
			
			return true;
		} catch (const vk::SystemError& err) {
			ERROR_LOG(RENDERER, "Vulkan system error: %s", err.what());
			return false;
		}
	}

	void Term() override
	{
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Term");
		GetContext()->WaitIdle();
		texCommandPool.Term();
		screenDrawer.Term();
		shaderManager.term();
		samplerManager.term();
		BaseVulkanRenderer::Term();
	}

	void Process(TA_context* ctx) override
	{
		BaseVulkanRenderer::Process(ctx);
	}

	bool Render() override
	{
		resize(rendContext->framebufferWidth, rendContext->framebufferHeight);

		screenDrawer.setRendContext(rendContext);
		screenDrawer.Draw(fogTexture.get(), paletteTexture.get());
		
		return true;
	}

	bool Present() override
	{
		int attachmentIndex = 0;
		if (config::ShowNormals || config::ShowSSAO)
			attachmentIndex = 1;
		
		return screenDrawer.PresentFrame(attachmentIndex);
	}

protected:
	void resize(int w, int h) override
	{
		if ((u32)w == viewport.width && (u32)h == viewport.height)
			return;
		BaseVulkanRenderer::resize(w, h);
		GetContext()->WaitIdle();
		std::vector<vk::Format> formats = {
			vk::Format::eR8G8B8A8Unorm,         // Albedo
			vk::Format::eR16G16B16A16Sfloat    // Normals
		};
		screenDrawer.Init(&samplerManager, &shaderManager, viewport, formats);
	}

private:
	SamplerManager samplerManager;
	ScreenDrawer screenDrawer;
};

Renderer* rend_GBufferVulkan()
{
	return new GBufferVulkanRenderer();
}
