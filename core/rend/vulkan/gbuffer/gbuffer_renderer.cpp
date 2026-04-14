#include "../vulkan_renderer.h"
#include "../drawer.h"
#include "../quad.h"
#include "../texture.h"

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1

extern "C" {
	typedef unsigned char Bytef;
	typedef unsigned long uLong;
	#define Z_OK 0
	int stbi_zlib_decode_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);
	unsigned char *stbi_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality);

	inline uLong compressBound(uLong sourceLen) {
		return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) + 13;
	}

	inline int compress(unsigned char* dest, uLong* destLen, const unsigned char* source, uLong sourceLen) {
		int out_len;
		unsigned char* compressed = stbi_zlib_compress(const_cast<unsigned char*>(source), (int)sourceLen, &out_len, 8);
		if (!compressed) return -1;
		if ((uLong)out_len > *destLen) { free(compressed); return -1; }
		memcpy(dest, compressed, out_len);
		*destLen = (uLong)out_len;
		free(compressed);
		return Z_OK;
	}

	inline int uncompress(unsigned char* dest, uLong* destLen, const unsigned char* source, uLong sourceLen) {
		int ret = stbi_zlib_decode_buffer((char*)dest, (int)*destLen, (const char*)source, (int)sourceLen);
		if (ret < 0) return -1;
		*destLen = (uLong)ret;
		return Z_OK;
	}
}

#include "../../deps/tinyexr.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <random>

class SSAOPass
{
public:
	// albedoViews : une ImageView par image de swap chain (albedo attachment)
	void Init(ShaderManager *shaderManager, vk::Extent2D viewport,
		const std::vector<vk::ImageView> &albedoViews)
	{
		NOTICE_LOG(RENDERER, "SSAOPass::Init start (%dx%d, %zu views)", viewport.width, viewport.height, albedoViews.size());
		this->shaderManager = shaderManager;
		this->viewport = viewport;

		VulkanContext *ctx = VulkanContext::Instance();

		// Generation du kernel de 64 echantillons
		std::uniform_real_distribution<float> randomFD(0.0, 1.0);
		std::default_random_engine generator;
		kernelSamples.clear();
		for (int i = 0; i < 64; ++i) {
			glm::vec3 sample(randomFD(generator) * 2.0 - 1.0, randomFD(generator) * 2.0 - 1.0, randomFD(generator));
			sample = glm::normalize(sample);
			sample *= randomFD(generator);
			float scale = (float)i / 64.0f;
			scale = 0.1f + (scale * scale) * (1.0f - 0.1f); // lerp
			sample *= scale;
			kernelSamples.push_back(glm::vec4(sample, 0.0f));
		}

		// Uniform buffer pour le kernel
		kernelBuffer = std::make_unique<BufferData>(kernelSamples.size() * sizeof(glm::vec4), vk::BufferUsageFlagBits::eUniformBuffer);
		kernelBuffer->upload(kernelSamples.size() * sizeof(glm::vec4), kernelSamples.data());

		// Generation du bruit 4x4 dans un Uniform Buffer (evite tout probleme de tiling/staging)
		std::vector<glm::vec4> noiseValues;
		for (int i = 0; i < 16; i++)
			noiseValues.push_back(glm::vec4(randomFD(generator) * 2.0f - 1.0f, randomFD(generator) * 2.0f - 1.0f, 0.0f, 0.0f));
		noiseBuffer = std::make_unique<BufferData>(noiseValues.size() * sizeof(glm::vec4), vk::BufferUsageFlagBits::eUniformBuffer);
		noiseBuffer->upload(noiseValues.size() * sizeof(glm::vec4), noiseValues.data());

		// Descriptor set layout : binding 0 = depth, binding 1 = normals, binding 2 = noise UBO, binding 3 = kernel UBO
		std::array<vk::DescriptorSetLayoutBinding, 4> bindings = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),
		};
		descSetLayout = ctx->GetDevice().createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), bindings));

		// Push constants : resolution + near/far + bias + radius + debug
		vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(float) * 8);
		pipelineLayout = ctx->GetDevice().createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), *descSetLayout, pushConstant));

		// Samplers
		sampler = ctx->GetDevice().createSamplerUnique(
			vk::SamplerCreateInfo(vk::SamplerCreateFlags(),
				vk::Filter::eNearest, vk::Filter::eNearest,
				vk::SamplerMipmapMode::eNearest,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				0.0f, false, 1.0f, false, vk::CompareOp::eNever,
				0.0f, vk::LodClampNone, vk::BorderColor::eFloatOpaqueBlack));

		depthSampler = ctx->GetDevice().createSamplerUnique(
			vk::SamplerCreateInfo(vk::SamplerCreateFlags(),
				vk::Filter::eNearest, vk::Filter::eNearest,
				vk::SamplerMipmapMode::eNearest,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				0.0f, false, 1.0f, false, vk::CompareOp::eNever,
				0.0f, vk::LodClampNone, vk::BorderColor::eFloatOpaqueBlack));
		
		// Quad vertex buffer
		quadBuffer = std::make_unique<QuadBuffer>();

		// Buffers SSAO separés (R8Unorm) pour capturer la valeur AO brute avant blending
		ssaoBuffers.clear();
		ssaoImageViews.clear();
		ssaoImages.clear();
		for (size_t i = 0; i < albedoViews.size(); ++i)
		{
			auto buf = std::make_unique<FramebufferAttachment>(ctx->GetPhysicalDevice(), ctx->GetDevice());
			buf->Init(viewport.width, viewport.height, vk::Format::eR8Unorm,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);
			ssaoImageViews.push_back(buf->GetImageView());
			ssaoImages.push_back(buf->GetImage());
			ssaoBuffers.push_back(std::move(buf));
		}

		// Renderpass SSAO : attachment 0 = albedo (Load/Store), attachment 1 = ssao buffer (Clear/Store)
		std::array<vk::AttachmentDescription, 2> attachDescs = {
			vk::AttachmentDescription(
				vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::AttachmentDescription(
				vk::AttachmentDescriptionFlags(), vk::Format::eR8Unorm, vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal),
		};
		std::array<vk::AttachmentReference, 2> colorRefs = {
			vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
			vk::AttachmentReference(1, vk::ImageLayout::eColorAttachmentOptimal),
		};
		vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
			nullptr, colorRefs, nullptr, nullptr);
		vk::SubpassDependency dep1(VK_SUBPASS_EXTERNAL, 0,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::AccessFlagBits::eShaderRead,
			vk::AccessFlagBits::eColorAttachmentWrite,
			vk::DependencyFlagBits::eByRegion);
		vk::SubpassDependency dep2(0, VK_SUBPASS_EXTERNAL,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::AccessFlagBits::eColorAttachmentWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::DependencyFlagBits::eByRegion);
		std::array<vk::SubpassDependency, 2> deps = { dep1, dep2 };
		renderPass = ctx->GetDevice().createRenderPassUnique(
			vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), attachDescs, subpass, deps));

		// Framebuffers : un par image, avec albedo + ssao buffer
		framebuffers.clear();
		for (size_t i = 0; i < albedoViews.size(); ++i)
		{
			std::array<vk::ImageView, 2> views = { albedoViews[i], ssaoImageViews[i] };
			framebuffers.push_back(ctx->GetDevice().createFramebufferUnique(
				vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
					views, viewport.width, viewport.height, 1)));
		}

		// Pipeline
		CreatePipeline(false); CreatePipeline(true);
		DEBUG_LOG(RENDERER, "SSAOPass::Init end");
	}

	void Term()
	{
		DEBUG_LOG(RENDERER, "SSAOPass::Term start");
		pipeline.reset(); debugPipeline.reset();
		pipelineLayout.reset();
		descSetLayout.reset();
		sampler.reset();
		depthSampler.reset();
		noiseBuffer.reset();
		kernelBuffer.reset();
		quadBuffer.reset();
		framebuffers.clear();
		renderPass.reset();
		descriptorSets.clear();
		ssaoBuffers.clear();
		ssaoImageViews.clear();
		ssaoImages.clear();
		DEBUG_LOG(RENDERER, "SSAOPass::Term end");
	}

	bool IsInitialized() const { return pipeline && debugPipeline && quadBuffer; }

	vk::ImageView GetSSAOImageView(int index) const
	{
		if (index >= 0 && index < (int)ssaoImageViews.size()) return ssaoImageViews[index];
		return {};
	}
	vk::Image GetSSAOImage(int index) const
	{
		if (index >= 0 && index < (int)ssaoImages.size()) return ssaoImages[index];
		return {};
	}

	// Execute le pass SSAO sur l'image courante
	void Draw(vk::CommandBuffer cmdBuffer, int imageIndex,
		vk::ImageView depthView, vk::ImageView normalView, bool showSSAO)
	{
		if (imageIndex < 0 || imageIndex >= (int)framebuffers.size())
		{
			ERROR_LOG(RENDERER, "SSAOPass::Draw: Invalid imageIndex %d (framebuffers size: %zu). Skipping SSAO.", imageIndex, framebuffers.size());
			return;
		}
		DEBUG_LOG(RENDERER, "SSAOPass::Draw(imageIndex=%d, showSSAO=%d)", imageIndex, showSSAO);
		VulkanContext *ctx = VulkanContext::Instance();

		if ((int)descriptorSets.size() <= imageIndex)
			descriptorSets.resize(imageIndex + 1);

		auto &descSet = descriptorSets[imageIndex];
		if (!descSet)
		{
			descSet = std::move(ctx->GetDevice().allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(ctx->GetDescriptorPool(), *descSetLayout)).front());
		}

		// Mise a jour des descriptors
		vk::DescriptorImageInfo depthInfo(*depthSampler, depthView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
		vk::DescriptorImageInfo normalInfo(*sampler, normalView, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::DescriptorBufferInfo noiseInfo(*noiseBuffer->buffer, 0, 16 * sizeof(glm::vec4));
		vk::DescriptorBufferInfo kernelInfo(*kernelBuffer->buffer, 0, kernelSamples.size() * sizeof(glm::vec4));
		
		std::array<vk::WriteDescriptorSet, 4> writes = {
			vk::WriteDescriptorSet(*descSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, depthInfo),
			vk::WriteDescriptorSet(*descSet, 1, 0, vk::DescriptorType::eCombinedImageSampler, normalInfo),
			vk::WriteDescriptorSet(*descSet, 2, 0, vk::DescriptorType::eUniformBuffer, {}, noiseInfo),
			vk::WriteDescriptorSet(*descSet, 3, 0, vk::DescriptorType::eUniformBuffer, {}, kernelInfo),
		};
		ctx->GetDevice().updateDescriptorSets(writes, nullptr);

		// Debut du renderpass SSAO (2 clear values : albedo non cleared, ssao buffer cleared a 1.0)
		std::array<vk::ClearValue, 2> clearValues = {
			vk::ClearValue(vk::ClearColorValue(std::array<float,4>{0.f,0.f,0.f,1.f})),
			vk::ClearValue(vk::ClearColorValue(std::array<float,4>{1.f,1.f,1.f,1.f})),
		};
		cmdBuffer.beginRenderPass(
			vk::RenderPassBeginInfo(*renderPass, *framebuffers[imageIndex],
				vk::Rect2D({0,0}, viewport), clearValues),
			vk::SubpassContents::eInline);

		cmdBuffer.setViewport(0, vk::Viewport(0.f, 0.f, (float)viewport.width, (float)viewport.height, 0.f, 1.f));
		cmdBuffer.setScissor(0, vk::Rect2D({0,0}, viewport));

		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, showSSAO ? *debugPipeline : *pipeline);
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descSet, nullptr);

		// Push constants : resolution, near, far, bias, radius, showSSAO
		struct PushConstants {
			float resolution[2];
			float nearPlane;
			float farPlane;
			float bias;
			float radius;
			int showSSAO;
			int pad[1];
		} pc;
		memset(&pc, 0, sizeof(pc));
		pc.resolution[0] = (float)viewport.width;
		pc.resolution[1] = (float)viewport.height;
		pc.nearPlane = 0.001f;
		pc.farPlane = 100000.f;
		pc.bias = config::SSAOBias;
		pc.radius = config::SSAORadius;
		pc.showSSAO = showSSAO ? 1 : 0;
		cmdBuffer.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(pc), &pc);

		// Full-screen quad
		quadBuffer->Update(nullptr);
		quadBuffer->Bind(cmdBuffer);
		quadBuffer->Draw(cmdBuffer);

		cmdBuffer.endRenderPass();
	}

private:
void CreatePipeline(bool debug)
	{
		VulkanContext *ctx = VulkanContext::Instance();

		vk::PipelineVertexInputStateCreateInfo vertexInput = GetQuadInputStateCreateInfo(true);
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly(vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleStrip);
		vk::PipelineViewportStateCreateInfo viewportState(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);
		vk::PipelineRasterizationStateCreateInfo rasterization;
		rasterization.lineWidth = 1.0f;
		vk::PipelineMultisampleStateCreateInfo multisample;
		vk::PipelineDepthStencilStateCreateInfo depthStencil;

		// Blending multiplicatif : albedo *= ao
		// Attachment 1 (AoRaw) : ecriture directe sans blending
		vk::PipelineColorBlendAttachmentState aoRawAttachment(false,
			vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
			vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
			vk::ColorComponentFlagBits::eR);

		vk::PipelineColorBlendAttachmentState blendAttachment;
		if (debug) {
			blendAttachment = vk::PipelineColorBlendAttachmentState(false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		} else {
			// Blending multiplicatif : albedo.rgb *= ao (ao est dans le canal alpha du fragment SSAO)
			// src = vec4(1,1,1,ao), dst = albedo
			// RGB_final = src.rgb * 0 + dst.rgb * src.a  => albedo.rgb * ao
			// A_final   = src.a   * 0 + dst.a   * 1      => albedo.a inchange
			blendAttachment = vk::PipelineColorBlendAttachmentState(true, vk::BlendFactor::eZero, vk::BlendFactor::eSrcAlpha, vk::BlendOp::eAdd, vk::BlendFactor::eZero, vk::BlendFactor::eOne, vk::BlendOp::eAdd, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		}
		std::array<vk::PipelineColorBlendAttachmentState, 2> blendAttachments = { blendAttachment, aoRawAttachment };
		vk::PipelineColorBlendStateCreateInfo colorBlend(
			vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
			blendAttachments, { { 1.f, 1.f, 1.f, 1.f } });

		std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicState(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

		std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex,
				shaderManager->GetQuadVertexShader(false), "main"),
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment,
				shaderManager->GetSSAOFragmentShader(), "main"),
		};

		vk::GraphicsPipelineCreateInfo pipelineInfo(
			vk::PipelineCreateFlags(), stages,
			&vertexInput, &inputAssembly, nullptr, &viewportState,
			&rasterization, &multisample, &depthStencil, &colorBlend, &dynamicState,
			*pipelineLayout, *renderPass, 0);

if (debug)
debugPipeline = ctx->GetDevice().createGraphicsPipelineUnique(ctx->GetPipelineCache(), pipelineInfo).value;
else
pipeline = ctx->GetDevice().createGraphicsPipelineUnique(ctx->GetPipelineCache(), pipelineInfo).value;
	}

	ShaderManager *shaderManager = nullptr;
	vk::Extent2D viewport;
	vk::UniqueRenderPass renderPass;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	vk::UniquePipeline pipeline, debugPipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout descSetLayout;
	vk::UniqueSampler sampler;
	vk::UniqueSampler depthSampler;
	std::unique_ptr<BufferData> noiseBuffer;
	std::vector<glm::vec4> kernelSamples;
	std::unique_ptr<BufferData> kernelBuffer;
	std::unique_ptr<QuadBuffer> quadBuffer;
	std::vector<vk::UniqueDescriptorSet> descriptorSets;
	// Buffers SSAO separés pour capturer la valeur AO brute (R8Unorm)
	std::vector<std::unique_ptr<FramebufferAttachment>> ssaoBuffers;
	std::vector<vk::ImageView> ssaoImageViews;
	std::vector<vk::Image> ssaoImages;
};

class DoFPass
{
public:
	void Init(ShaderManager *shaderManager, vk::Extent2D viewport,
		const std::vector<vk::ImageView> &albedoViews,
		const std::vector<vk::Image> &albedoImages)
	{
		NOTICE_LOG(RENDERER, "DoFPass::Init start (%dx%d)", viewport.width, viewport.height);
		this->shaderManager = shaderManager;
		this->viewport = viewport;
		this->albedoImages = albedoImages;

		VulkanContext *ctx = VulkanContext::Instance();

		// Binding 0: albedo source, Binding 1: depth
		std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
		};
		descSetLayout = ctx->GetDevice().createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), bindings));

		vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(float) * 4);
		pipelineLayout = ctx->GetDevice().createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), *descSetLayout, pushConstant));

		sampler = ctx->GetDevice().createSamplerUnique(
			vk::SamplerCreateInfo(vk::SamplerCreateFlags(),
				vk::Filter::eLinear, vk::Filter::eLinear,
				vk::SamplerMipmapMode::eLinear,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				0.0f, false, 1.0f, false, vk::CompareOp::eNever,
				0.0f, vk::LodClampNone, vk::BorderColor::eFloatOpaqueBlack));

		quadBuffer = std::make_unique<QuadBuffer>();

		// Creer les images intermediaires (une par swap index) pour eviter le conflit read/write
		// Le DoF lit l'albedo source et ecrit dans dofBuffers, puis on blit vers l'albedo
		int swapSize = (int)albedoViews.size();
		dofBuffers.clear();
		dofBuffers.resize(swapSize);
		dofImageViews.clear();
		for (int i = 0; i < swapSize; ++i)
		{
			dofBuffers[i] = std::make_unique<FramebufferAttachment>(ctx->GetPhysicalDevice(), ctx->GetDevice());
			dofBuffers[i]->Init(viewport.width, viewport.height, vk::Format::eR8G8B8A8Unorm,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
			dofImageViews.push_back(dofBuffers[i]->GetImageView());
		}

		// Renderpass : ecriture dans l'image intermediaire (Undefined -> ColorAttachmentOptimal -> TransferSrcOptimal)
		vk::AttachmentDescription dofDesc(
			vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
		vk::AttachmentReference dofRef(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
			nullptr, dofRef, nullptr, nullptr);

		std::array<vk::SubpassDependency, 2> deps = {
			vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion),
			vk::SubpassDependency(0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead, vk::DependencyFlagBits::eByRegion)
		};

		renderPass = ctx->GetDevice().createRenderPassUnique(
			vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), dofDesc, subpass, deps));

		framebuffers.clear();
		for (vk::ImageView dofView : dofImageViews)
		{
			framebuffers.push_back(ctx->GetDevice().createFramebufferUnique(
				vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
					dofView, viewport.width, viewport.height, 1)));
		}

		CreatePipeline();
	}

	void Term()
	{
		pipeline.reset();
		pipelineLayout.reset();
		descSetLayout.reset();
		sampler.reset();
		quadBuffer.reset();
		framebuffers.clear();
		dofImageViews.clear();
		dofBuffers.clear();
		renderPass.reset();
		descriptorSets.clear();
		albedoImages.clear();
	}

	bool IsInitialized() const { return pipeline && quadBuffer; }

	void Draw(vk::CommandBuffer cmdBuffer, int imageIndex,
		vk::ImageView albedoView, vk::ImageView depthView)
	{
		VulkanContext *ctx = VulkanContext::Instance();
		if ((int)descriptorSets.size() <= imageIndex)
			descriptorSets.resize(imageIndex + 1);

		auto &descSet = descriptorSets[imageIndex];
		if (!descSet)
		{
			descSet = std::move(ctx->GetDevice().allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(ctx->GetDescriptorPool(), *descSetLayout)).front());
		}

		// Lire l'albedo source (ShaderReadOnlyOptimal apres le SSAO ou le G-Buffer)
		vk::DescriptorImageInfo albedoInfo(*sampler, albedoView, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::DescriptorImageInfo depthInfo(*sampler, depthView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
		std::array<vk::WriteDescriptorSet, 2> writes = {
			vk::WriteDescriptorSet(*descSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, albedoInfo),
			vk::WriteDescriptorSet(*descSet, 1, 0, vk::DescriptorType::eCombinedImageSampler, depthInfo),
		};
		ctx->GetDevice().updateDescriptorSets(writes, nullptr);

		// Render pass : ecriture dans l'image intermediaire dofBuffers[imageIndex]
		vk::ClearValue clearValue;
		cmdBuffer.beginRenderPass(
			vk::RenderPassBeginInfo(*renderPass, *framebuffers[imageIndex],
				vk::Rect2D({0,0}, viewport), clearValue),
			vk::SubpassContents::eInline);

		cmdBuffer.setViewport(0, vk::Viewport(0.f, 0.f, (float)viewport.width, (float)viewport.height, 0.f, 1.f));
		cmdBuffer.setScissor(0, vk::Rect2D({0,0}, viewport));
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descSet, nullptr);

		struct {
			float resolution[2];
			float focus;
			float intensity;
		} pc;
		pc.resolution[0] = (float)viewport.width;
		pc.resolution[1] = (float)viewport.height;
		pc.focus = config::DoFFocus;
		pc.intensity = config::DoFBokehIntensity;
		cmdBuffer.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(pc), &pc);

		quadBuffer->Update(nullptr);
		quadBuffer->Bind(cmdBuffer);
		quadBuffer->Draw(cmdBuffer);

		cmdBuffer.endRenderPass();

		// Transition dofBuffer : ColorAttachmentOptimal -> TransferSrcOptimal
		vk::ImageMemoryBarrier dofToSrc(
			vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
			vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			dofBuffers[imageIndex]->GetImage(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		// Transition albedo : ShaderReadOnlyOptimal -> TransferDstOptimal
		vk::ImageMemoryBarrier albedoToDst(
			vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			albedoImages[imageIndex],
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		std::array<vk::ImageMemoryBarrier, 2> toTransfer = { dofToSrc, albedoToDst };
		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eFragmentShader,
			vk::PipelineStageFlagBits::eTransfer,
			{}, nullptr, nullptr, toTransfer);

		// Blit du dofBuffer vers l'albedo
		vk::ImageBlit blitRegion(
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
			{ vk::Offset3D(0,0,0), vk::Offset3D((int)viewport.width, (int)viewport.height, 1) },
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
			{ vk::Offset3D(0,0,0), vk::Offset3D((int)viewport.width, (int)viewport.height, 1) });
		cmdBuffer.blitImage(
			dofBuffers[imageIndex]->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
			albedoImages[imageIndex], vk::ImageLayout::eTransferDstOptimal,
			blitRegion, vk::Filter::eNearest);

		// Retransition albedo : TransferDstOptimal -> ShaderReadOnlyOptimal
		vk::ImageMemoryBarrier albedoToShader(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			albedoImages[imageIndex],
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{}, nullptr, nullptr, albedoToShader);
	}

private:
	void CreatePipeline()
	{
		VulkanContext *ctx = VulkanContext::Instance();
		vk::PipelineVertexInputStateCreateInfo vertexInput = GetQuadInputStateCreateInfo(true);
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly(vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleStrip);
		vk::PipelineViewportStateCreateInfo viewportState(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);
		vk::PipelineRasterizationStateCreateInfo rasterization;
		rasterization.lineWidth = 1.0f;
		vk::PipelineMultisampleStateCreateInfo multisample;
		vk::PipelineDepthStencilStateCreateInfo depthStencil;
		vk::PipelineColorBlendAttachmentState blendAttachment(false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		vk::PipelineColorBlendStateCreateInfo colorBlend(vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp, blendAttachment, {{1.f, 1.f, 1.f, 1.f}});
		std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicState(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

		std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, shaderManager->GetQuadVertexShader(false), "main"),
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, shaderManager->GetDoFFragmentShader(), "main"),
		};

		pipeline = ctx->GetDevice().createGraphicsPipelineUnique(ctx->GetPipelineCache(),
			vk::GraphicsPipelineCreateInfo(vk::PipelineCreateFlags(), stages, &vertexInput, &inputAssembly, nullptr, &viewportState, &rasterization, &multisample, &depthStencil, &colorBlend, &dynamicState, *pipelineLayout, *renderPass, 0)).value;
	}

	ShaderManager *shaderManager = nullptr;
	vk::Extent2D viewport;
	vk::UniqueRenderPass renderPass;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	vk::UniquePipeline pipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout descSetLayout;
	vk::UniqueSampler sampler;
	std::unique_ptr<QuadBuffer> quadBuffer;
	std::vector<vk::UniqueDescriptorSet> descriptorSets;
	// Images intermediaires (ping-pong) pour eviter le conflit read/write
	std::vector<std::unique_ptr<FramebufferAttachment>> dofBuffers;
	std::vector<vk::ImageView> dofImageViews;
	std::vector<vk::Image> albedoImages;
};

class MaterialPass
{
public:
	void Init(ShaderManager *shaderManager, vk::Extent2D viewport,
		const std::vector<vk::ImageView> &albedoViews,
		const std::vector<vk::ImageView> &materialViews)
	{
		NOTICE_LOG(RENDERER, "MaterialPass::Init start (%dx%d, %zu views)", viewport.width, viewport.height, albedoViews.size());
		this->shaderManager = shaderManager;
		this->viewport = viewport;

		VulkanContext *ctx = VulkanContext::Instance();

		// Descriptor set layout : binding 0 = material ID (usampler2D)
		vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
		descSetLayout = ctx->GetDevice().createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), binding));

		pipelineLayout = ctx->GetDevice().createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), *descSetLayout));

		sampler = ctx->GetDevice().createSamplerUnique(
			vk::SamplerCreateInfo(vk::SamplerCreateFlags(),
				vk::Filter::eNearest, vk::Filter::eNearest,
				vk::SamplerMipmapMode::eNearest,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				vk::SamplerAddressMode::eClampToEdge,
				0.0f, false, 1.0f, false, vk::CompareOp::eNever,
    0.0f, vk::LodClampNone, vk::BorderColor::eIntOpaqueBlack));

		quadBuffer = std::make_unique<QuadBuffer>();

		// Renderpass : ecrit sur l'albedo attachment (overwrite complet)
		vk::AttachmentDescription albedoDesc(
			vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::AttachmentReference albedoRef(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
			nullptr, albedoRef, nullptr, nullptr);
		vk::SubpassDependency dep1(VK_SUBPASS_EXTERNAL, 0,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::AccessFlagBits::eShaderRead,
			vk::AccessFlagBits::eColorAttachmentWrite,
			vk::DependencyFlagBits::eByRegion);
		vk::SubpassDependency dep2(0, VK_SUBPASS_EXTERNAL,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::AccessFlagBits::eColorAttachmentWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::DependencyFlagBits::eByRegion);
		std::array<vk::SubpassDependency, 2> deps = { dep1, dep2 };
		renderPass = ctx->GetDevice().createRenderPassUnique(
			vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), albedoDesc, subpass, deps));

		// Framebuffers : un par image, pointant sur l'albedo attachment
		framebuffers.clear();
		for (vk::ImageView albedoView : albedoViews)
		{
			framebuffers.push_back(ctx->GetDevice().createFramebufferUnique(
				vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
					albedoView, viewport.width, viewport.height, 1)));
		}

		this->materialViews = materialViews;

		CreatePipeline();
		NOTICE_LOG(RENDERER, "MaterialPass::Init end");
	}

	void Term()
	{
		pipeline.reset();
		pipelineLayout.reset();
		descSetLayout.reset();
		sampler.reset();
		quadBuffer.reset();
		framebuffers.clear();
		renderPass.reset();
		descriptorSets.clear();
		materialViews.clear();
	}

	bool IsInitialized() const { return (bool)pipeline; }

	void Draw(vk::CommandBuffer cmdBuffer, int imageIndex)
	{
		if (imageIndex < 0 || imageIndex >= (int)framebuffers.size())
		{
			ERROR_LOG(RENDERER, "MaterialPass::Draw: Invalid imageIndex %d. Skipping.", imageIndex);
			return;
		}
		DEBUG_LOG(RENDERER, "MaterialPass::Draw(imageIndex=%d)", imageIndex);
		VulkanContext *ctx = VulkanContext::Instance();

		if ((int)descriptorSets.size() <= imageIndex)
			descriptorSets.resize(imageIndex + 1);

		auto &descSet = descriptorSets[imageIndex];
		if (!descSet)
		{
			descSet = std::move(ctx->GetDevice().allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(ctx->GetDescriptorPool(), *descSetLayout)).front());
		}

		vk::DescriptorImageInfo matInfo(*sampler, materialViews[imageIndex], vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::WriteDescriptorSet write(*descSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, matInfo);
		ctx->GetDevice().updateDescriptorSets(write, nullptr);

		vk::ClearValue clearColor(vk::ClearColorValue(std::array<float,4>{0.f,0.f,0.f,1.f}));
		cmdBuffer.beginRenderPass(
			vk::RenderPassBeginInfo(*renderPass, *framebuffers[imageIndex],
				vk::Rect2D({0,0}, viewport), clearColor),
			vk::SubpassContents::eInline);

		cmdBuffer.setViewport(0, vk::Viewport(0.f, 0.f, (float)viewport.width, (float)viewport.height, 0.f, 1.f));
		cmdBuffer.setScissor(0, vk::Rect2D({0,0}, viewport));

		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descSet, nullptr);

		quadBuffer->Update(nullptr);
		quadBuffer->Bind(cmdBuffer);
		quadBuffer->Draw(cmdBuffer);

		cmdBuffer.endRenderPass();
	}

private:
	void CreatePipeline()
	{
		VulkanContext *ctx = VulkanContext::Instance();

		vk::PipelineVertexInputStateCreateInfo vertexInput = GetQuadInputStateCreateInfo(true);
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly(vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleStrip);
		vk::PipelineViewportStateCreateInfo viewportState(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);
		vk::PipelineRasterizationStateCreateInfo rasterization;
		rasterization.lineWidth = 1.0f;
		vk::PipelineMultisampleStateCreateInfo multisample;
		vk::PipelineDepthStencilStateCreateInfo depthStencil;

		vk::ColorComponentFlags colorFlags = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		vk::PipelineColorBlendAttachmentState blendAttachment(false,
			vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
			vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
			colorFlags);
		vk::PipelineColorBlendStateCreateInfo colorBlend(
			vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
			blendAttachment, { { 1.f, 1.f, 1.f, 1.f } });

		std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicState(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

		std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex,
				shaderManager->GetQuadVertexShader(false), "main"),
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment,
				shaderManager->GetMaterialFragmentShader(), "main"),
		};

		pipeline = ctx->GetDevice().createGraphicsPipelineUnique(ctx->GetPipelineCache(),
			vk::GraphicsPipelineCreateInfo(vk::PipelineCreateFlags(), stages,
				&vertexInput, &inputAssembly, nullptr, &viewportState,
				&rasterization, &multisample, &depthStencil, &colorBlend, &dynamicState,
				*pipelineLayout, *renderPass, 0)).value;
	}

	ShaderManager *shaderManager = nullptr;
	vk::Extent2D viewport;
	vk::UniqueRenderPass renderPass;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	vk::UniquePipeline pipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout descSetLayout;
	vk::UniqueSampler sampler;
	std::unique_ptr<QuadBuffer> quadBuffer;
	std::vector<vk::UniqueDescriptorSet> descriptorSets;
	std::vector<vk::ImageView> materialViews;
};

class GBufferVulkanRenderer final : public BaseVulkanRenderer
{
public:
	bool Init() override
	{
		NOTICE_LOG(RENDERER, "GBufferVulkanRenderer::Init");
		try {
			std::vector<vk::Format> formats = {
				vk::Format::eR8G8B8A8Unorm,         // Albedo (0)
				vk::Format::eR16G16B16A16Sfloat,    // Normals (1)
				vk::Format::eR8Uint,                // Material ID (2)
				vk::Format::eR16G16Sfloat           // Motion (velocity) (3)
			};
			screenDrawer.Init(&samplerManager, &shaderManager, viewport, formats);
			screenDrawer.SetCommandPool(&texCommandPool);
			BaseInit(screenDrawer.GetRenderPass());

			initSSAO();
			initDoF();
			initMaterial();

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
		ssaoPass.Term();
		dofPass.Term();
		materialPass.Term();
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
		NOTICE_LOG(RENDERER, "GBufferVulkanRenderer::Render start");
		resize(rendContext->framebufferWidth, rendContext->framebufferHeight);

		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Render screenDrawer.Draw");
		screenDrawer.setRendContext(rendContext);
		screenDrawer.Draw(fogTexture.get(), paletteTexture.get());

		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Render end");

		return true;
	}

	bool Present() override
	{
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Present start");

		int imgIdx = screenDrawer.GetCurrentImageIndex();
		FramebufferAttachment *depthAtt = screenDrawer.GetDepthAttachment();
		FramebufferAttachment *normalAtt = screenDrawer.GetColorAttachment(imgIdx, 1);
		FramebufferAttachment *albedoAtt = screenDrawer.GetColorAttachment(imgIdx, 0);

		// Initialisation lazy du SSAO si necessaire
		if ((config::EnableSSAO || config::ShowSSAO) && !ssaoPass.IsInitialized())
			initSSAO();

		// Initialisation lazy du DoF si necessaire
		if (config::EnableDoF && !dofPass.IsInitialized())
			initDoF();
		else if (!config::EnableDoF && dofPass.IsInitialized())
			dofPass.Term();

		// Initialisation lazy du Material si necessaire
		if (config::ShowMaterial && !materialPass.IsInitialized())
			initMaterial();

		bool doSSAO = (config::EnableSSAO || config::ShowSSAO) && ssaoPass.IsInitialized() && depthAtt && normalAtt;
		bool doDoF  = config::EnableDoF && dofPass.IsInitialized() && depthAtt && albedoAtt;
		FramebufferAttachment *materialAtt = screenDrawer.GetColorAttachment(imgIdx, 2);
		bool doMaterial = config::ShowMaterial && materialPass.IsInitialized() && materialAtt;

		if (doSSAO || doDoF || doMaterial)
		{
			// Fermer le render pass G-Buffer sans soumettre le command buffer
			// Les attachments passent en ShaderReadOnlyOptimal via la transition implicite du render pass
			vk::CommandBuffer cmdBuf = screenDrawer.EndRenderPassOnly();
			if (cmdBuf)
			{
				// SSAO : enregistre dans le meme command buffer, apres le render pass G-Buffer
				if (doSSAO)
				{
					DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Present ssaoPass.Draw(imgIdx=%d)", imgIdx);
					ssaoPass.Draw(cmdBuf, imgIdx,
						depthAtt->GetImageView(),
						normalAtt->GetImageView(),
						config::ShowSSAO);
				}

				// DoF : enregistre dans le meme command buffer, apres le SSAO
				if (doDoF)
				{
					DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Present dofPass.Draw(imgIdx=%d)", imgIdx);
					dofPass.Draw(cmdBuf, imgIdx,
						albedoAtt->GetImageView(),
						depthAtt->GetImageView());
				}
				// Material : enregistre dans le meme command buffer, apres le render pass G-Buffer
				if (doMaterial)
				{
					DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Present materialPass.Draw(imgIdx=%d)", imgIdx);
					materialPass.Draw(cmdBuf, imgIdx);
				}
				// Le command buffer sera soumis par PresentFrame() -> EndRenderPass()
			}
		}

		// ALT+1 (Depth) et ALT+2 (Normals) : attachment 1
		// ALT+0 (Normal) et ALT+3 (SSAO) et ALT+5 (Material) : attachment 0 (Albedo)
		// ALT+4 (Motion) : attachment 3
		int attachmentIndex = config::ShowMotion ? 3
			: (config::ShowNormals || config::ShowDepth) ? 1 : 0;

		bool ret = screenDrawer.PresentFrame(attachmentIndex);
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Present end (%s)", ret ? "success" : "skipped");
		return ret;
	}

	void ExportGBuffer() override;

protected:
	void resize(int w, int h) override
	{
		if ((u32)w == viewport.width && (u32)h == viewport.height)
			return;
		NOTICE_LOG(RENDERER, "GBufferVulkanRenderer::resize(%d, %d)", w, h);
		BaseVulkanRenderer::resize(w, h);
		GetContext()->WaitIdle();
		std::vector<vk::Format> formats = {
			vk::Format::eR8G8B8A8Unorm,         // Albedo (0)
			vk::Format::eR16G16B16A16Sfloat,    // Normals (1)
			vk::Format::eR8Uint,                // Material ID (2)
			vk::Format::eR16G16Sfloat           // Motion (velocity) (3)
		};
		screenDrawer.Init(&samplerManager, &shaderManager, viewport, formats);
		initSSAO();
		initDoF();
		initMaterial();
	}

private:
	void initSSAO()
	{
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO start");
		if (!config::EnableSSAO && !config::ShowSSAO)
		{
			DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO: SSAO disabled");
			ssaoPass.Term();
			return;
		}

		// Collecte les albedo views pour tous les indices de swap chain
		int swapSize = (int)screenDrawer.GetSwapChainCount();
		std::vector<vk::ImageView> albedoViews;
		albedoViews.reserve(swapSize);
		for (int i = 0; i < swapSize; ++i)
		{
			FramebufferAttachment *att = screenDrawer.GetColorAttachment(i, 0);
			if (att)
				albedoViews.push_back(att->GetImageView());
			else
				DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO: Missing albedo attachment for swap index %d", i);
		}

		if (!albedoViews.empty())
		{
			DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO calling ssaoPass.Init with %zu views", albedoViews.size());
			ssaoPass.Init(&shaderManager, viewport, albedoViews);
		} else {
			ERROR_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO: No albedo views found!");
		}
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO end");
	}

	void initDoF()
	{
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initDoF start");
		if (!config::EnableDoF)
		{
			DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initDoF: DoF disabled");
			dofPass.Term();
			return;
		}

		int swapSize = (int)screenDrawer.GetSwapChainCount();
		std::vector<vk::ImageView> albedoViews;
		std::vector<vk::Image> albedoImages;
		albedoViews.reserve(swapSize);
		albedoImages.reserve(swapSize);
		for (int i = 0; i < swapSize; ++i)
		{
			FramebufferAttachment *att = screenDrawer.GetColorAttachment(i, 0);
			if (att)
			{
				albedoViews.push_back(att->GetImageView());
				albedoImages.push_back(att->GetImage());
			}
		}

		if (!albedoViews.empty())
		{
			dofPass.Init(&shaderManager, viewport, albedoViews, albedoImages);
		}
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initDoF end");
	}

	void initMaterial()
	{
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initMaterial start");
		int swapSize = (int)screenDrawer.GetSwapChainCount();
		std::vector<vk::ImageView> albedoViews;
		std::vector<vk::ImageView> materialViews;
		albedoViews.reserve(swapSize);
		materialViews.reserve(swapSize);
		for (int i = 0; i < swapSize; ++i)
		{
			FramebufferAttachment *albedo = screenDrawer.GetColorAttachment(i, 0);
			FramebufferAttachment *mat    = screenDrawer.GetColorAttachment(i, 2);
			if (albedo && mat)
			{
				albedoViews.push_back(albedo->GetImageView());
				materialViews.push_back(mat->GetImageView());
			}
		}
		if (!albedoViews.empty())
			materialPass.Init(&shaderManager, viewport, albedoViews, materialViews);
		else
			ERROR_LOG(RENDERER, "GBufferVulkanRenderer::initMaterial: No views found!");
		DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initMaterial end");
	}

	SamplerManager samplerManager;
	ScreenDrawer screenDrawer;
	SSAOPass ssaoPass;
	DoFPass dofPass;
	MaterialPass materialPass;
};

void GBufferVulkanRenderer::ExportGBuffer()
{
	INFO_LOG(RENDERER, "ExportGBuffer: Initializing...");
	VulkanContext *ctx = GetContext();
	if (!ctx) {
		ERROR_LOG(RENDERER, "ExportGBuffer: Context is null!");
		return;
	}
	ctx->WaitIdle();

	u32 width = viewport.width;
	u32 height = viewport.height;
	if (width == 0 || height == 0) {
		ERROR_LOG(RENDERER, "ExportGBuffer: Invalid viewport dimensions %ux%u", width, height);
		return;
	}

	int imgIdx = screenDrawer.GetCurrentImageIndex();
	INFO_LOG(RENDERER, "ExportGBuffer: Viewport %ux%u, image index %d", width, height, imgIdx);

	auto albedoAtt = screenDrawer.GetColorAttachment(imgIdx, 0);
	auto normalAtt = screenDrawer.GetColorAttachment(imgIdx, 1);
	auto materialAtt = screenDrawer.GetColorAttachment(imgIdx, 2);
	auto depthAtt = screenDrawer.GetDepthAttachment();
	vk::Image ssaoImage = ssaoPass.IsInitialized() ? ssaoPass.GetSSAOImage(imgIdx) : vk::Image{};

	if (!albedoAtt || !normalAtt || !depthAtt) {
		ERROR_LOG(RENDERER, "ExportGBuffer: Missing attachments (A: %p, N: %p, D: %p)", albedoAtt, normalAtt, depthAtt);
		return;
	}

	// Les color attachments sont en eShaderReadOnlyOptimal et la depth en eDepthStencilReadOnlyOptimal
	// après le render pass (défini dans drawer.cpp lignes 693 et 703).
	vk::Format depthFmt = screenDrawer.GetDepthFormat();
	INFO_LOG(RENDERER, "ExportGBuffer: Depth format: %s", vk::to_string(depthFmt).c_str());

	// Allouer les staging buffers avant le command buffer
	BufferData stageA(width * height * 4, vk::BufferUsageFlagBits::eTransferDst);
	BufferData stageN(width * height * 8, vk::BufferUsageFlagBits::eTransferDst);
	// La depth D32 = 4 bytes/pixel, D24S8 = 4 bytes/pixel
	BufferData stageD(width * height * 4, vk::BufferUsageFlagBits::eTransferDst);
	// Material : R8Uint = 1 byte/pixel ; SSAO : R8Unorm = 1 byte/pixel
	std::unique_ptr<BufferData> stageMat = materialAtt
		? std::make_unique<BufferData>(width * height * 1, vk::BufferUsageFlagBits::eTransferDst) : nullptr;
	std::unique_ptr<BufferData> stageSSAO = ssaoImage
		? std::make_unique<BufferData>(width * height * 1, vk::BufferUsageFlagBits::eTransferDst) : nullptr;

	try {
		// Un seul BeginFrame + un seul command buffer pour toutes les opérations GPU
		texCommandPool.BeginFrame();

		vk::CommandBuffer cmd = texCommandPool.Allocate(true);
		if (!cmd) {
			ERROR_LOG(RENDERER, "ExportGBuffer: Failed to allocate command buffer");
			texCommandPool.EndFrame();
			return;
		}
		cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

			// --- Transition vers TransferSrcOptimal ---
			INFO_LOG(RENDERER, "ExportGBuffer: Transitioning to TransferSrc...");
			{
				std::vector<vk::ImageMemoryBarrier> barriers;
				barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferRead,
					vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					albedoAtt->GetImage(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
				barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferRead,
					vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					normalAtt->GetImage(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
				barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eDepthStencilAttachmentRead, vk::AccessFlagBits::eTransferRead,
					vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					depthAtt->GetImage(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1)));
				if (materialAtt)
					barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferRead,
						vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
						materialAtt->GetImage(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
				if (ssaoImage)
					barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferRead,
						vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
						ssaoImage, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eLateFragmentTests,
					vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barriers);
			}

			// --- Copies image → staging buffers ---
			INFO_LOG(RENDERER, "ExportGBuffer: Copying to staging buffers...");
			{
				vk::BufferImageCopy region(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), { 0, 0, 0 }, { width, height, 1 });
				cmd.copyImageToBuffer(albedoAtt->GetImage(), vk::ImageLayout::eTransferSrcOptimal, stageA.buffer.get(), region);
				cmd.copyImageToBuffer(normalAtt->GetImage(), vk::ImageLayout::eTransferSrcOptimal, stageN.buffer.get(), region);
				if (materialAtt && stageMat)
					cmd.copyImageToBuffer(materialAtt->GetImage(), vk::ImageLayout::eTransferSrcOptimal, stageMat->buffer.get(), region);
				if (ssaoImage && stageSSAO)
					cmd.copyImageToBuffer(ssaoImage, vk::ImageLayout::eTransferSrcOptimal, stageSSAO->buffer.get(), region);

				vk::BufferImageCopy dRegion(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eDepth, 0, 0, 1), { 0, 0, 0 }, { width, height, 1 });
				cmd.copyImageToBuffer(depthAtt->GetImage(), vk::ImageLayout::eTransferSrcOptimal, stageD.buffer.get(), dRegion);
			}

		// --- Transition retour vers les layouts originaux ---
		INFO_LOG(RENDERER, "ExportGBuffer: Transitioning back to original layouts...");
		{
			std::vector<vk::ImageMemoryBarrier> barriers;
			barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead,
				vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				albedoAtt->GetImage(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
			barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead,
				vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				normalAtt->GetImage(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
				barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eDepthStencilAttachmentRead,
					vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eDepthStencilReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					depthAtt->GetImage(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1)));
				if (materialAtt)
					barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead,
						vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
						materialAtt->GetImage(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
				if (ssaoImage)
					barriers.push_back(vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead,
						vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
						ssaoImage, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eLateFragmentTests, {}, nullptr, nullptr, barriers);
			}

		cmd.end();

		// Soumettre et attendre la fin de l'exécution GPU avant de lire les staging buffers
		texCommandPool.EndFrameAndWait();
		INFO_LOG(RENDERER, "ExportGBuffer: GPU work done.");

		INFO_LOG(RENDERER, "ExportGBuffer: Processing data on CPU...");

		std::vector<float> r(width * height), g(width * height), b(width * height);
		std::vector<float> nx(width * height), ny(width * height), nz(width * height);
		std::vector<float> depth(width * height);
		std::vector<float> matF(width * height, 0.f);
		std::vector<float> aoF(width * height, 1.f);

		u8 *ptrA = (u8 *)stageA.MapMemory();
		if (ptrA) {
			for (u32 i = 0; i < width * height; ++i) {
				r[i] = ptrA[i * 4 + 0] / 255.0f;
				g[i] = ptrA[i * 4 + 1] / 255.0f;
				b[i] = ptrA[i * 4 + 2] / 255.0f;
			}
			stageA.UnmapMemory();
		}

		u16 *ptrN = (u16 *)stageN.MapMemory();
		if (ptrN) {
			auto h2f = [](u16 h) {
				u32 sign = (h >> 15) & 1; u32 exp = (h >> 10) & 0x1f; u32 mant = h & 0x3ff;
				if (exp == 0) return (sign ? -1.0f : 1.0f) * std::pow(2.0f, -14.0f) * (mant / 1024.0f);
				if (exp == 31) return 0.0f;
				return (sign ? -1.0f : 1.0f) * std::pow(2.0f, (float)exp - 15.0f) * (1.0f + mant / 1024.0f);
			};
			for (u32 i = 0; i < width * height; ++i) {
				nx[i] = h2f(ptrN[i * 4 + 0]);
				ny[i] = h2f(ptrN[i * 4 + 1]);
				nz[i] = h2f(ptrN[i * 4 + 2]);
			}
			stageN.UnmapMemory();
		}

		void *ptrD = stageD.MapMemory();
		if (ptrD) {
			if (depthFmt == vk::Format::eD24UnormS8Uint) {
				u32 *src = (u32 *)ptrD;
				for (u32 i = 0; i < width * height; ++i) depth[i] = (src[i] & 0xFFFFFF) / 16777215.0f;
			} else {
				float *src = (float *)ptrD;
				for (u32 i = 0; i < width * height; ++i) depth[i] = src[i];
			}
			stageD.UnmapMemory();
		}

		if (stageMat) {
			u8 *ptrM = (u8 *)stageMat->MapMemory();
			if (ptrM) {
				for (u32 i = 0; i < width * height; ++i)
					matF[i] = ptrM[i] / 255.0f;
				stageMat->UnmapMemory();
			}
		}

		if (stageSSAO) {
			u8 *ptrS = (u8 *)stageSSAO->MapMemory();
			if (ptrS) {
				for (u32 i = 0; i < width * height; ++i)
					aoF[i] = ptrS[i] / 255.0f;
				stageSSAO->UnmapMemory();
			}
		}

		INFO_LOG(RENDERER, "ExportGBuffer: Data conversion done. Initializing EXR...");

		EXRHeader header;
		InitEXRHeader(&header);
		EXRImage image;
		InitEXRImage(&image);

		// Canaux EXR : Albedo RGB, Normal XYZ, Depth, Material, SSAO
		const int numChannels = 9;
		const char *channel_names[] = {
			"Albedo.B", "Albedo.G", "Albedo.R",
			"Normal.Z", "Normal.Y", "Normal.X",
			"Depth.Z",
			"Material.ID",
			"SSAO.AO"
		};
		float* image_ptr[numChannels];
		image_ptr[0] = b.data(); image_ptr[1] = g.data(); image_ptr[2] = r.data();
		image_ptr[3] = nz.data(); image_ptr[4] = ny.data(); image_ptr[5] = nx.data();
		image_ptr[6] = depth.data();
		image_ptr[7] = matF.data();
		image_ptr[8] = aoF.data();

		image.num_channels = numChannels;
		image.images = (unsigned char **)image_ptr;
		image.width = width;
		image.height = height;

		header.num_channels = numChannels;
		header.channels = (EXRChannelInfo *)malloc(sizeof(EXRChannelInfo) * numChannels);
		header.pixel_types = (int *)malloc(sizeof(int) * numChannels);
		header.requested_pixel_types = (int *)malloc(sizeof(int) * numChannels);
		for (int i = 0; i < numChannels; i++) {
			strncpy(header.channels[i].name, channel_names[i], 255);
			header.channels[i].name[255] = '\0';
			header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
			header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF;
			INFO_LOG(RENDERER, "ExportGBuffer: Channel %d: %s", i, channel_names[i]);
		}

		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);
		std::ostringstream oss;
		oss << "gbuffer_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".exr";
		std::string filename = oss.str();
		std::string fullPath = hostfs::getScreenshotsPath() + "/" + filename;

		INFO_LOG(RENDERER, "ExportGBuffer: Saving EXR to %s", fullPath.c_str());

		const char *err = nullptr;
		int ret = SaveEXRImageToFile(&image, &header, fullPath.c_str(), &err);
		if (ret != TINYEXR_SUCCESS) {
			ERROR_LOG(RENDERER, "SaveEXR failed: %s", err);
			if (err) FreeEXRErrorMessage(err);
		} else {
			NOTICE_LOG(RENDERER, "G-Buffer exported to %s", fullPath.c_str());
		}
		free(header.channels);
		free(header.pixel_types);
		free(header.requested_pixel_types);
	} catch (const std::exception& e) {
		ERROR_LOG(RENDERER, "ExportGBuffer: Exception caught: %s", e.what());
	} catch (...) {
		ERROR_LOG(RENDERER, "ExportGBuffer: Unknown exception caught");
	}

	INFO_LOG(RENDERER, "ExportGBuffer finished");
}

Renderer* rend_GBufferVulkan()
{
	return new GBufferVulkanRenderer();
}
