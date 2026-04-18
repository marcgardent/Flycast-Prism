#include "../drawer.h"
#include "../quad.h"
#include "../texture.h"
#include "../vulkan_renderer.h"
#include "gbuffer_constants.h"

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1

extern "C" {
typedef unsigned char Bytef;
typedef unsigned long uLong;
#define Z_OK 0
int stbi_zlib_decode_buffer(char *obuffer, int olen, const char *ibuffer,
                            int ilen);
unsigned char *stbi_zlib_compress(unsigned char *data, int data_len,
                                  int *out_len, int quality);

inline uLong compressBound(uLong sourceLen) {
  return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) +
         13;
}

inline int compress(unsigned char *dest, uLong *destLen,
                    const unsigned char *source, uLong sourceLen) {
  int out_len;
  unsigned char *compressed = stbi_zlib_compress(
      const_cast<unsigned char *>(source), (int)sourceLen, &out_len, 8);
  if (!compressed)
    return -1;
  if ((uLong)out_len > *destLen) {
    free(compressed);
    return -1;
  }
  memcpy(dest, compressed, out_len);
  *destLen = (uLong)out_len;
  free(compressed);
  return Z_OK;
}

inline int uncompress(unsigned char *dest, uLong *destLen,
                      const unsigned char *source, uLong sourceLen) {
  int ret = stbi_zlib_decode_buffer((char *)dest, (int)*destLen,
                                    (const char *)source, (int)sourceLen);
  if (ret < 0)
    return -1;
  *destLen = (uLong)ret;
  return Z_OK;
}
}

#include "../../deps/tinyexr.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

class SSAOPass {
public:
  // albedoViews : une ImageView par image de swap chain (albedo attachment)
  void Init(ShaderManager *shaderManager, vk::Extent2D viewport,
            const std::vector<vk::ImageView> &albedoViews) {
    NOTICE_LOG(RENDERER, "SSAOPass::Init start (%dx%d, %zu views)",
               viewport.width, viewport.height, albedoViews.size());
    this->shaderManager = shaderManager;
    this->viewport = viewport;

    VulkanContext *ctx = VulkanContext::Instance();

    // Generation du kernel de 64 echantillons
    std::uniform_real_distribution<float> randomFD(0.0, 1.0);
    std::default_random_engine generator;
    kernelSamples.clear();
    for (int i = 0; i < 64; ++i) {
      glm::vec3 sample(randomFD(generator) * 2.0 - 1.0,
                       randomFD(generator) * 2.0 - 1.0, randomFD(generator));
      sample = glm::normalize(sample);
      sample *= randomFD(generator);
      float scale = (float)i / 64.0f;
      scale = 0.1f + (scale * scale) * (1.0f - 0.1f); // lerp
      sample *= scale;
      kernelSamples.push_back(glm::vec4(sample, 0.0f));
    }

    // Uniform buffer pour le kernel
    kernelBuffer =
        std::make_unique<BufferData>(kernelSamples.size() * sizeof(glm::vec4),
                                     vk::BufferUsageFlagBits::eUniformBuffer);
    kernelBuffer->upload(kernelSamples.size() * sizeof(glm::vec4),
                         kernelSamples.data());

    // Generation du bruit 4x4 dans un Uniform Buffer (evite tout probleme de
    // tiling/staging)
    std::vector<glm::vec4> noiseValues;
    for (int i = 0; i < 16; i++)
      noiseValues.push_back(glm::vec4(randomFD(generator) * 2.0f - 1.0f,
                                      randomFD(generator) * 2.0f - 1.0f, 0.0f,
                                      0.0f));
    noiseBuffer =
        std::make_unique<BufferData>(noiseValues.size() * sizeof(glm::vec4),
                                     vk::BufferUsageFlagBits::eUniformBuffer);
    noiseBuffer->upload(noiseValues.size() * sizeof(glm::vec4),
                        noiseValues.data());

    // Descriptor set layout : binding 0 = depth, binding 1 = normals, binding 2
    // = noise UBO, binding 3 = kernel UBO
    std::array<vk::DescriptorSetLayoutBinding, 4> bindings = {
        vk::DescriptorSetLayoutBinding(
            0, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(
            1, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1,
                                       vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1,
                                       vk::ShaderStageFlagBits::eFragment),
    };
    descSetLayout = ctx->GetDevice().createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(),
                                          bindings));

    // Push constants : resolution + near/far + bias + radius + debug
    vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0,
                                       sizeof(float) * 8);
    pipelineLayout = ctx->GetDevice().createPipelineLayoutUnique(
        vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(),
                                     *descSetLayout, pushConstant));

    // Samplers
    sampler = ctx->GetDevice().createSamplerUnique(vk::SamplerCreateInfo(
        vk::SamplerCreateFlags(), vk::Filter::eNearest, vk::Filter::eNearest,
        vk::SamplerMipmapMode::eNearest, vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge, 0.0f, false, 1.0f, false,
        vk::CompareOp::eNever, 0.0f, vk::LodClampNone,
        vk::BorderColor::eFloatOpaqueBlack));

    depthSampler = ctx->GetDevice().createSamplerUnique(vk::SamplerCreateInfo(
        vk::SamplerCreateFlags(), vk::Filter::eNearest, vk::Filter::eNearest,
        vk::SamplerMipmapMode::eNearest, vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge, 0.0f, false, 1.0f, false,
        vk::CompareOp::eNever, 0.0f, vk::LodClampNone,
        vk::BorderColor::eFloatOpaqueBlack));

    // Quad vertex buffer
    quadBuffer = std::make_unique<QuadBuffer>();

    // Buffers SSAO separés (R8Unorm) pour capturer la valeur AO brute avant
    // blending
    ssaoBuffers.clear();
    ssaoImageViews.clear();
    ssaoImages.clear();
    for (size_t i = 0; i < albedoViews.size(); ++i) {
      auto buf = std::make_unique<FramebufferAttachment>(
          ctx->GetPhysicalDevice(), ctx->GetDevice());
      buf->Init(viewport.width, viewport.height, vk::Format::eR8Unorm,
                vk::ImageUsageFlagBits::eColorAttachment |
                    vk::ImageUsageFlagBits::eTransferSrc |
                    vk::ImageUsageFlagBits::eSampled);
      ssaoImageViews.push_back(buf->GetImageView());
      ssaoImages.push_back(buf->GetImage());
      ssaoBuffers.push_back(std::move(buf));
    }

    // Renderpass SSAO : attachment 0 = ssao buffer (Clear/Store)
    vk::AttachmentDescription attachDesc(
        vk::AttachmentDescriptionFlags(), vk::Format::eR8Unorm,
        vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::AttachmentReference colorRef(0,
                                     vk::ImageLayout::eColorAttachmentOptimal);
    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
                                   vk::PipelineBindPoint::eGraphics, nullptr,
                                   colorRef, nullptr, nullptr);
    vk::SubpassDependency dep1(
        VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eFragmentShader,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eShaderRead,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::DependencyFlagBits::eByRegion);
    vk::SubpassDependency dep2(
        0, VK_SUBPASS_EXTERNAL,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eShaderRead, vk::DependencyFlagBits::eByRegion);
    std::array<vk::SubpassDependency, 2> deps = {dep1, dep2};
    renderPass =
        ctx->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(
            vk::RenderPassCreateFlags(), attachDesc, subpass, deps));

    // Framebuffers : un par image, avec seulement le ssao buffer
    framebuffers.clear();
    for (size_t i = 0; i < ssaoImageViews.size(); ++i) {
      framebuffers.push_back(ctx->GetDevice().createFramebufferUnique(
          vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
                                    ssaoImageViews[i], viewport.width,
                                    viewport.height, 1)));
    }

    // Pipeline
    CreatePipeline(false);
    CreatePipeline(true);
    DEBUG_LOG(RENDERER, "SSAOPass::Init end");
  }

  void Term() {
    DEBUG_LOG(RENDERER, "SSAOPass::Term start");
    pipeline.reset();
    debugPipeline.reset();
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

  vk::ImageView GetSSAOImageView(int index) const {
    if (index >= 0 && index < (int)ssaoImageViews.size())
      return ssaoImageViews[index];
    return {};
  }
  vk::Image GetSSAOImage(int index) const {
    if (index >= 0 && index < (int)ssaoImages.size())
      return ssaoImages[index];
    return {};
  }

  // Execute le pass SSAO sur l'image courante
  void Draw(vk::CommandBuffer cmdBuffer, int imageIndex,
            vk::ImageView depthView, vk::ImageView normalView, bool showSSAO) {
    if (imageIndex < 0 || imageIndex >= (int)framebuffers.size()) {
      ERROR_LOG(RENDERER,
                "SSAOPass::Draw: Invalid imageIndex %d (framebuffers size: "
                "%zu). Skipping SSAO.",
                imageIndex, framebuffers.size());
      return;
    }
    DEBUG_LOG(RENDERER, "SSAOPass::Draw(imageIndex=%d, showSSAO=%d)",
              imageIndex, showSSAO);
    VulkanContext *ctx = VulkanContext::Instance();

    if ((int)descriptorSets.size() <= imageIndex)
      descriptorSets.resize(imageIndex + 1);

    auto &descSet = descriptorSets[imageIndex];
    if (!descSet) {
      descSet = std::move(
          ctx->GetDevice()
              .allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(
                  ctx->GetDescriptorPool(), *descSetLayout))
              .front());
    }

    // Mise a jour des descriptors
    vk::DescriptorImageInfo depthInfo(
        *depthSampler, depthView,
        vk::ImageLayout::eDepthStencilReadOnlyOptimal);
    vk::DescriptorImageInfo normalInfo(*sampler, normalView,
                                       vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorBufferInfo noiseInfo(*noiseBuffer->buffer, 0,
                                       16 * sizeof(glm::vec4));
    vk::DescriptorBufferInfo kernelInfo(
        *kernelBuffer->buffer, 0, kernelSamples.size() * sizeof(glm::vec4));

    std::array<vk::WriteDescriptorSet, 4> writes = {
        vk::WriteDescriptorSet(*descSet, 0, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               depthInfo),
        vk::WriteDescriptorSet(*descSet, 1, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               normalInfo),
        vk::WriteDescriptorSet(
            *descSet, 2, 0, vk::DescriptorType::eUniformBuffer, {}, noiseInfo),
        vk::WriteDescriptorSet(
            *descSet, 3, 0, vk::DescriptorType::eUniformBuffer, {}, kernelInfo),
    };
    ctx->GetDevice().updateDescriptorSets(writes, nullptr);

    // Debut du renderpass SSAO (1 clear value : ssao buffer cleared a 1.0)
    vk::ClearValue clearValue(
        vk::ClearColorValue(std::array<float, 4>{1.f, 1.f, 1.f, 1.f}));
    cmdBuffer.beginRenderPass(
        vk::RenderPassBeginInfo(*renderPass, *framebuffers[imageIndex],
                                vk::Rect2D({0, 0}, viewport), clearValue),
        vk::SubpassContents::eInline);

    cmdBuffer.setViewport(0, vk::Viewport(0.f, 0.f, (float)viewport.width,
                                          (float)viewport.height, 0.f, 1.f));
    cmdBuffer.setScissor(0, vk::Rect2D({0, 0}, viewport));

    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                           showSSAO ? *debugPipeline : *pipeline);
    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                 *pipelineLayout, 0, *descSet, nullptr);

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
    cmdBuffer.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eFragment,
                            0, sizeof(pc), &pc);

    // Full-screen quad
    quadBuffer->Update(nullptr);
    quadBuffer->Bind(cmdBuffer);
    quadBuffer->Draw(cmdBuffer);

    cmdBuffer.endRenderPass();
  }

private:
  void CreatePipeline(bool debug) {
    VulkanContext *ctx = VulkanContext::Instance();

    vk::PipelineVertexInputStateCreateInfo vertexInput =
        GetQuadInputStateCreateInfo(true);
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        vk::PipelineInputAssemblyStateCreateFlags(),
        vk::PrimitiveTopology::eTriangleStrip);
    vk::PipelineViewportStateCreateInfo viewportState(
        vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.lineWidth = 1.0f;
    vk::PipelineMultisampleStateCreateInfo multisample;
    vk::PipelineDepthStencilStateCreateInfo depthStencil;

    // Output uniquement sur le SSAO buffer (R8Unorm) : ecriture directe sans
    // blending
    vk::PipelineColorBlendAttachmentState aoRawAttachment(
        false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR);

    vk::PipelineColorBlendStateCreateInfo colorBlend(
        vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
        aoRawAttachment, {{1.f, 1.f, 1.f, 1.f}});

    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState(
        vk::PipelineDynamicStateCreateFlags(), dynamicStates);

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eVertex,
            shaderManager->GetQuadVertexShader(false), "main"),
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eFragment,
            shaderManager->GetSSAOFragmentShader(), "main"),
    };

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        vk::PipelineCreateFlags(), stages, &vertexInput, &inputAssembly,
        nullptr, &viewportState, &rasterization, &multisample, &depthStencil,
        &colorBlend, &dynamicState, *pipelineLayout, *renderPass, 0);

    if (debug)
      debugPipeline = ctx->GetDevice()
                          .createGraphicsPipelineUnique(ctx->GetPipelineCache(),
                                                        pipelineInfo)
                          .value;
    else
      pipeline = ctx->GetDevice()
                     .createGraphicsPipelineUnique(ctx->GetPipelineCache(),
                                                   pipelineInfo)
                     .value;
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

class DoFPass {
public:
  void Init(ShaderManager *shaderManager, vk::Extent2D viewport,
            const std::vector<vk::ImageView> &albedoViews,
            const std::vector<vk::Image> &albedoImages) {
    NOTICE_LOG(RENDERER, "DoFPass::Init start (%dx%d)", viewport.width,
               viewport.height);
    this->shaderManager = shaderManager;
    this->viewport = viewport;
    this->albedoImages = albedoImages;

    VulkanContext *ctx = VulkanContext::Instance();

    // Binding 0: albedo source, Binding 1: depth
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
        vk::DescriptorSetLayoutBinding(
            0, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(
            1, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
    };
    descSetLayout = ctx->GetDevice().createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(),
                                          bindings));

    vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0,
                                       sizeof(float) * 4);
    pipelineLayout = ctx->GetDevice().createPipelineLayoutUnique(
        vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(),
                                     *descSetLayout, pushConstant));

    sampler = ctx->GetDevice().createSamplerUnique(vk::SamplerCreateInfo(
        vk::SamplerCreateFlags(), vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge, 0.0f, false, 1.0f, false,
        vk::CompareOp::eNever, 0.0f, vk::LodClampNone,
        vk::BorderColor::eFloatOpaqueBlack));

    quadBuffer = std::make_unique<QuadBuffer>();

    // Creer les images intermediaires (une par swap index) pour eviter le
    // conflit read/write Le DoF lit l'albedo source et ecrit dans dofBuffers,
    // puis on blit vers l'albedo
    int swapSize = (int)albedoViews.size();
    dofBuffers.clear();
    dofBuffers.resize(swapSize);
    dofImageViews.clear();
    for (int i = 0; i < swapSize; ++i) {
      dofBuffers[i] = std::make_unique<FramebufferAttachment>(
          ctx->GetPhysicalDevice(), ctx->GetDevice());
      dofBuffers[i]->Init(viewport.width, viewport.height,
                          vk::Format::eR16G16B16A16Sfloat,
                          vk::ImageUsageFlagBits::eColorAttachment |
                              vk::ImageUsageFlagBits::eTransferSrc);
      dofImageViews.push_back(dofBuffers[i]->GetImageView());
    }

    // Renderpass : ecriture dans l'image intermediaire (Undefined ->
    // ColorAttachmentOptimal -> TransferSrcOptimal)
    vk::AttachmentDescription dofDesc(
        vk::AttachmentDescriptionFlags(), vk::Format::eR16G16B16A16Sfloat,
        vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference dofRef(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
                                   vk::PipelineBindPoint::eGraphics, nullptr,
                                   dofRef, nullptr, nullptr);

    std::array<vk::SubpassDependency, 2> deps = {
        vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0,
                              vk::PipelineStageFlagBits::eFragmentShader,
                              vk::PipelineStageFlagBits::eColorAttachmentOutput,
                              vk::AccessFlagBits::eShaderRead,
                              vk::AccessFlagBits::eColorAttachmentWrite,
                              vk::DependencyFlagBits::eByRegion),
        vk::SubpassDependency(0, VK_SUBPASS_EXTERNAL,
                              vk::PipelineStageFlagBits::eColorAttachmentOutput,
                              vk::PipelineStageFlagBits::eTransfer,
                              vk::AccessFlagBits::eColorAttachmentWrite,
                              vk::AccessFlagBits::eTransferRead,
                              vk::DependencyFlagBits::eByRegion)};

    renderPass =
        ctx->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(
            vk::RenderPassCreateFlags(), dofDesc, subpass, deps));

    framebuffers.clear();
    for (vk::ImageView dofView : dofImageViews) {
      framebuffers.push_back(ctx->GetDevice().createFramebufferUnique(
          vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
                                    dofView, viewport.width, viewport.height,
                                    1)));
    }

    CreatePipeline();
  }

  void Term() {
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
            vk::ImageView albedoView, vk::ImageView depthView) {
    VulkanContext *ctx = VulkanContext::Instance();
    if ((int)descriptorSets.size() <= imageIndex)
      descriptorSets.resize(imageIndex + 1);

    auto &descSet = descriptorSets[imageIndex];
    if (!descSet) {
      descSet = std::move(
          ctx->GetDevice()
              .allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(
                  ctx->GetDescriptorPool(), *descSetLayout))
              .front());
    }

    // Lire l'albedo source (ShaderReadOnlyOptimal apres le SSAO ou le G-Buffer)
    vk::DescriptorImageInfo albedoInfo(*sampler, albedoView,
                                       vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo depthInfo(
        *sampler, depthView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
    std::array<vk::WriteDescriptorSet, 2> writes = {
        vk::WriteDescriptorSet(*descSet, 0, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               albedoInfo),
        vk::WriteDescriptorSet(*descSet, 1, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               depthInfo),
    };
    ctx->GetDevice().updateDescriptorSets(writes, nullptr);

    // Render pass : ecriture dans l'image intermediaire dofBuffers[imageIndex]
    vk::ClearValue clearValue;
    cmdBuffer.beginRenderPass(
        vk::RenderPassBeginInfo(*renderPass, *framebuffers[imageIndex],
                                vk::Rect2D({0, 0}, viewport), clearValue),
        vk::SubpassContents::eInline);

    cmdBuffer.setViewport(0, vk::Viewport(0.f, 0.f, (float)viewport.width,
                                          (float)viewport.height, 0.f, 1.f));
    cmdBuffer.setScissor(0, vk::Rect2D({0, 0}, viewport));
    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                 *pipelineLayout, 0, *descSet, nullptr);

    struct {
      float resolution[2];
      float focus;
      float intensity;
    } pc;
    pc.resolution[0] = (float)viewport.width;
    pc.resolution[1] = (float)viewport.height;
    pc.focus = config::DoFFocus;
    pc.intensity = config::DoFBokehIntensity;
    cmdBuffer.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eFragment,
                            0, sizeof(pc), &pc);

    quadBuffer->Update(nullptr);
    quadBuffer->Bind(cmdBuffer);
    quadBuffer->Draw(cmdBuffer);

    cmdBuffer.endRenderPass();

    // Transition dofBuffer : ColorAttachmentOptimal -> TransferSrcOptimal
    vk::ImageMemoryBarrier dofToSrc(
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eTransferRead,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, dofBuffers[imageIndex]->GetImage(),
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    // Transition albedo : ShaderReadOnlyOptimal -> TransferDstOptimal
    vk::ImageMemoryBarrier albedoToDst(
        vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, albedoImages[imageIndex],
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    std::array<vk::ImageMemoryBarrier, 2> toTransfer = {dofToSrc, albedoToDst};
    cmdBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
            vk::PipelineStageFlagBits::eFragmentShader,
        vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, toTransfer);

    // Blit du dofBuffer vers l'albedo
    vk::ImageBlit blitRegion(
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {vk::Offset3D(0, 0, 0),
         vk::Offset3D((int)viewport.width, (int)viewport.height, 1)},
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {vk::Offset3D(0, 0, 0),
         vk::Offset3D((int)viewport.width, (int)viewport.height, 1)});
    cmdBuffer.blitImage(
        dofBuffers[imageIndex]->GetImage(),
        vk::ImageLayout::eTransferSrcOptimal, albedoImages[imageIndex],
        vk::ImageLayout::eTransferDstOptimal, blitRegion, vk::Filter::eNearest);

    // Retransition albedo : TransferDstOptimal -> ShaderReadOnlyOptimal
    vk::ImageMemoryBarrier albedoToShader(
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, albedoImages[imageIndex],
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                              vk::PipelineStageFlagBits::eFragmentShader, {},
                              nullptr, nullptr, albedoToShader);
  }

private:
  void CreatePipeline() {
    VulkanContext *ctx = VulkanContext::Instance();
    vk::PipelineVertexInputStateCreateInfo vertexInput =
        GetQuadInputStateCreateInfo(true);
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        vk::PipelineInputAssemblyStateCreateFlags(),
        vk::PrimitiveTopology::eTriangleStrip);
    vk::PipelineViewportStateCreateInfo viewportState(
        vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.lineWidth = 1.0f;
    vk::PipelineMultisampleStateCreateInfo multisample;
    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    vk::PipelineColorBlendAttachmentState blendAttachment(
        false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo colorBlend(
        vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
        blendAttachment, {{1.f, 1.f, 1.f, 1.f}});
    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState(
        vk::PipelineDynamicStateCreateFlags(), dynamicStates);

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eVertex,
            shaderManager->GetQuadVertexShader(false), "main"),
        vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(),
                                          vk::ShaderStageFlagBits::eFragment,
                                          shaderManager->GetDoFFragmentShader(),
                                          "main"),
    };

    pipeline =
        ctx->GetDevice()
            .createGraphicsPipelineUnique(
                ctx->GetPipelineCache(),
                vk::GraphicsPipelineCreateInfo(
                    vk::PipelineCreateFlags(), stages, &vertexInput,
                    &inputAssembly, nullptr, &viewportState, &rasterization,
                    &multisample, &depthStencil, &colorBlend, &dynamicState,
                    *pipelineLayout, *renderPass, 0))
            .value;
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

class GBuffer3DResolvePass {
public:
  void Init(ShaderManager *shaderManager, vk::Extent2D viewport,
            const std::vector<vk::ImageView> &albedoViews,
            const std::vector<vk::ImageView> &normalViews,
            const std::vector<vk::ImageView> &depthViews,
            const std::vector<vk::ImageView> &materialViews,
            const std::vector<vk::ImageView> &motionViews,
            const std::vector<vk::ImageView> &ssaoViews) {
    this->shaderManager = shaderManager;
    this->viewport = viewport;
    this->albedoViews = albedoViews;
    this->normalViews = normalViews;
    this->depthViews = depthViews;
    this->materialViews = materialViews;
    this->motionViews = motionViews;
    this->ssaoViews = ssaoViews;

    VulkanContext *ctx = VulkanContext::Instance();

    std::array<vk::DescriptorSetLayoutBinding, 6> bindings = {
        vk::DescriptorSetLayoutBinding(
            0, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(
            1, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(
            2, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(
            3, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(
            4, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(
            5, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
    };
    descSetLayout = ctx->GetDevice().createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(),
                                          bindings));

    vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0,
                                       sizeof(int));
    pipelineLayout = ctx->GetDevice().createPipelineLayoutUnique(
        vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(),
                                     *descSetLayout, pushConstant));

    sampler = ctx->GetDevice().createSamplerUnique(vk::SamplerCreateInfo(
        vk::SamplerCreateFlags(), vk::Filter::eNearest, vk::Filter::eNearest,
        vk::SamplerMipmapMode::eNearest, vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge, 0.0f, false, 1.0f, false,
        vk::CompareOp::eNever, 0.0f, vk::LodClampNone,
        vk::BorderColor::eFloatOpaqueBlack));

    quadBuffer = std::make_unique<QuadBuffer>();

    accumulationBuffers.clear();
    accumulationViews.clear();
    for (size_t i = 0; i < albedoViews.size(); ++i) {
      auto buf = std::make_unique<FramebufferAttachment>(
          ctx->GetPhysicalDevice(), ctx->GetDevice());
      buf->Init(viewport.width, viewport.height,
                vk::Format::eR16G16B16A16Sfloat,
                vk::ImageUsageFlagBits::eColorAttachment |
                    vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eTransferSrc);
      accumulationViews.push_back(buf->GetImageView());
      accumulationBuffers.push_back(std::move(buf));
    }

    vk::AttachmentDescription colorAttachment(
        vk::AttachmentDescriptionFlags(), vk::Format::eR16G16B16A16Sfloat,
        vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::AttachmentReference colorReference(
        0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
                                   vk::PipelineBindPoint::eGraphics, nullptr,
                                   colorReference, nullptr, nullptr);

    vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eColorAttachmentWrite);

    renderPass =
        ctx->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(
            vk::RenderPassCreateFlags(), colorAttachment, subpass, dependency));

    framebuffers.clear();
    for (auto &view : accumulationViews) {
      framebuffers.push_back(ctx->GetDevice().createFramebufferUnique(
          vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
                                    view, viewport.width, viewport.height, 1)));
    }

    CreatePipeline();
  }

  void Term() {
    pipeline.reset();
    pipelineLayout.reset();
    descSetLayout.reset();
    sampler.reset();
    quadBuffer.reset();
    framebuffers.clear();
    renderPass.reset();
    descriptorSets.clear();
    accumulationViews.clear();
    accumulationBuffers.clear();
  }

  bool IsInitialized() const { return (bool)pipeline; }

  vk::ImageView GetAccumulationImageView(int index) const {
    if (index >= 0 && index < (int)accumulationViews.size())
      return accumulationViews[index];
    return {};
  }

  FramebufferAttachment *GetAccumulationAttachment(int index) const {
    if (index >= 0 && index < (int)accumulationBuffers.size())
      return accumulationBuffers[index].get();
    return nullptr;
  }

  void Draw(vk::CommandBuffer cmdBuffer, int imageIndex, int viewMode) {
    if (imageIndex < 0 || imageIndex >= (int)framebuffers.size())
      return;

    VulkanContext *ctx = VulkanContext::Instance();
    if ((int)descriptorSets.size() <= imageIndex)
      descriptorSets.resize(imageIndex + 1);

    auto &descSet = descriptorSets[imageIndex];
    if (!descSet) {
      descSet = std::move(
          ctx->GetDevice()
              .allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(
                  ctx->GetDescriptorPool(), *descSetLayout))
              .front());
    }

    vk::DescriptorImageInfo albedoInfo(*sampler, albedoViews[imageIndex],
                                       vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo normalInfo(*sampler, normalViews[imageIndex],
                                       vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo depthInfo(
        *sampler, depthViews[imageIndex],
        vk::ImageLayout::eDepthStencilReadOnlyOptimal);
    vk::DescriptorImageInfo materialInfo(
        *sampler, materialViews[imageIndex],
        vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo motionInfo(*sampler, motionViews[imageIndex],
                                       vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo ssaoInfo(*sampler, ssaoViews[imageIndex],
                                     vk::ImageLayout::eShaderReadOnlyOptimal);

    std::array<vk::WriteDescriptorSet, 6> writes = {
        vk::WriteDescriptorSet(*descSet, 0, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               albedoInfo),
        vk::WriteDescriptorSet(*descSet, 1, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               normalInfo),
        vk::WriteDescriptorSet(*descSet, 2, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               depthInfo),
        vk::WriteDescriptorSet(*descSet, 3, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               materialInfo),
        vk::WriteDescriptorSet(*descSet, 4, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               motionInfo),
        vk::WriteDescriptorSet(*descSet, 5, 0,
                               vk::DescriptorType::eCombinedImageSampler,
                               ssaoInfo),
    };
    ctx->GetDevice().updateDescriptorSets(writes, nullptr);

    vk::ClearValue clearValue(
        vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f}));
    cmdBuffer.beginRenderPass(
        vk::RenderPassBeginInfo(*renderPass, *framebuffers[imageIndex],
                                vk::Rect2D({0, 0}, viewport), clearValue),
        vk::SubpassContents::eInline);
    cmdBuffer.setViewport(0, vk::Viewport(0.f, 0.f, (float)viewport.width,
                                          (float)viewport.height, 0.f, 1.f));
    cmdBuffer.setScissor(0, vk::Rect2D({0, 0}, viewport));
    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                 *pipelineLayout, 0, *descSet, nullptr);
    cmdBuffer.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eFragment,
                            0, sizeof(int), &viewMode);
    quadBuffer->Update(nullptr);
    quadBuffer->Bind(cmdBuffer);
    quadBuffer->Draw(cmdBuffer);
    cmdBuffer.endRenderPass();
  }

private:
  void CreatePipeline() {
    VulkanContext *ctx = VulkanContext::Instance();
    vk::PipelineVertexInputStateCreateInfo vertexInput =
        GetQuadInputStateCreateInfo(true);
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        vk::PipelineInputAssemblyStateCreateFlags(),
        vk::PrimitiveTopology::eTriangleStrip);
    vk::PipelineViewportStateCreateInfo viewportState(
        vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.lineWidth = 1.0f;
    vk::PipelineMultisampleStateCreateInfo multisample;
    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    vk::PipelineColorBlendAttachmentState blendAttachment(
        false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo colorBlend(
        vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
        blendAttachment, {{1.f, 1.f, 1.f, 1.f}});
    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState(
        vk::PipelineDynamicStateCreateFlags(), dynamicStates);
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eVertex,
            shaderManager->GetQuadVertexShader(false), "main"),
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eFragment,
            shaderManager->GetGBuffer3DResolveFragmentShader(), "main"),
    };
    pipeline =
        ctx->GetDevice()
            .createGraphicsPipelineUnique(
                ctx->GetPipelineCache(),
                vk::GraphicsPipelineCreateInfo(
                    vk::PipelineCreateFlags(), stages, &vertexInput,
                    &inputAssembly, nullptr, &viewportState, &rasterization,
                    &multisample, &depthStencil, &colorBlend, &dynamicState,
                    *pipelineLayout, *renderPass, 0))
            .value;
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
  std::vector<vk::ImageView> albedoViews, normalViews, depthViews,
      materialViews, motionViews, ssaoViews;
  std::vector<vk::ImageView> accumulationViews;
  std::vector<std::unique_ptr<FramebufferAttachment>> accumulationBuffers;
};

class GBufferHUDOverlayPass {
public:
  void Init(ShaderManager *shaderManager, vk::Extent2D viewport,
            const std::vector<vk::ImageView> &accumulationViews,
            const std::vector<vk::ImageView> &hudViews) {
    this->shaderManager = shaderManager;
    this->viewport = viewport;
    this->accumulationViews = accumulationViews;
    this->hudViews = hudViews;

    VulkanContext *ctx = VulkanContext::Instance();

    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        vk::DescriptorSetLayoutBinding(
            0, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(
            1, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment),
    };
    descSetLayout = ctx->GetDevice().createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(),
                                          bindings));

    vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0,
                                       sizeof(int));
    pipelineLayout = ctx->GetDevice().createPipelineLayoutUnique(
        vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(),
                                     *descSetLayout, pushConstant));

    sampler = ctx->GetDevice().createSamplerUnique(vk::SamplerCreateInfo(
        vk::SamplerCreateFlags(), vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eNearest, vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge, 0.0f, false, 1.0f, false,
        vk::CompareOp::eNever, 0.0f, vk::LodClampNone,
        vk::BorderColor::eFloatOpaqueBlack));

    quadBuffer = std::make_unique<QuadBuffer>();

    finalBuffers.clear();
    finalViews.clear();
    for (size_t i = 0; i < accumulationViews.size(); ++i) {
      auto buf = std::make_unique<FramebufferAttachment>(
          ctx->GetPhysicalDevice(), ctx->GetDevice());
      buf->Init(viewport.width, viewport.height, vk::Format::eR8G8B8A8Unorm,
                vk::ImageUsageFlagBits::eColorAttachment |
                    vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eTransferSrc);
      finalViews.push_back(buf->GetImageView());
      finalBuffers.push_back(std::move(buf));
    }

    vk::AttachmentDescription colorAttachment(
        vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm,
        vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::AttachmentReference colorReference(
        0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
                                   vk::PipelineBindPoint::eGraphics, nullptr,
                                   colorReference, nullptr, nullptr);

    vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eColorAttachmentWrite);

    renderPass =
        ctx->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(
            vk::RenderPassCreateFlags(), colorAttachment, subpass, dependency));

    framebuffers.clear();
    for (auto &view : finalViews) {
      framebuffers.push_back(ctx->GetDevice().createFramebufferUnique(
          vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
                                    view, viewport.width, viewport.height, 1)));
    }

    CreatePipeline();
  }

  void Term() {
    pipeline.reset();
    pipelineLayout.reset();
    descSetLayout.reset();
    sampler.reset();
    quadBuffer.reset();
    framebuffers.clear();
    renderPass.reset();
    descriptorSets.clear();
    finalViews.clear();
    finalBuffers.clear();
  }

  bool IsInitialized() const { return (bool)pipeline; }

  vk::ImageView GetFinalImageView(int index) const {
    if (index >= 0 && index < (int)finalViews.size())
      return finalViews[index];
    return {};
  }

  FramebufferAttachment *GetFinalAttachment(int index) const {
    if (index >= 0 && index < (int)finalBuffers.size())
      return finalBuffers[index].get();
    return nullptr;
  }

  void Draw(vk::CommandBuffer cmdBuffer, int imageIndex, int viewMode) {
    if (imageIndex < 0 || imageIndex >= (int)framebuffers.size())
      return;

    VulkanContext *ctx = VulkanContext::Instance();
    if ((int)descriptorSets.size() <= imageIndex)
      descriptorSets.resize(imageIndex + 1);

    auto &descSet = descriptorSets[imageIndex];
    if (!descSet) {
      descSet = std::move(
          ctx->GetDevice()
              .allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(
                  ctx->GetDescriptorPool(), *descSetLayout))
              .front());
    }

    vk::DescriptorImageInfo accInfo(*sampler, accumulationViews[imageIndex],
                                    vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo hudInfo(*sampler, hudViews[imageIndex],
                                    vk::ImageLayout::eShaderReadOnlyOptimal);

    std::array<vk::WriteDescriptorSet, 2> writes = {
        vk::WriteDescriptorSet(
            *descSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, accInfo),
        vk::WriteDescriptorSet(
            *descSet, 1, 0, vk::DescriptorType::eCombinedImageSampler, hudInfo),
    };
    ctx->GetDevice().updateDescriptorSets(writes, nullptr);

    vk::ClearValue clearValue(
        vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f}));
    cmdBuffer.beginRenderPass(
        vk::RenderPassBeginInfo(*renderPass, *framebuffers[imageIndex],
                                vk::Rect2D({0, 0}, viewport), clearValue),
        vk::SubpassContents::eInline);
    cmdBuffer.setViewport(0, vk::Viewport(0.f, 0.f, (float)viewport.width,
                                          (float)viewport.height, 0.f, 1.f));
    cmdBuffer.setScissor(0, vk::Rect2D({0, 0}, viewport));
    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                 *pipelineLayout, 0, *descSet, nullptr);
    cmdBuffer.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eFragment,
                            0, sizeof(int), &viewMode);
    quadBuffer->Update(nullptr);
    quadBuffer->Bind(cmdBuffer);
    quadBuffer->Draw(cmdBuffer);
    cmdBuffer.endRenderPass();
  }

private:
  void CreatePipeline() {
    VulkanContext *ctx = VulkanContext::Instance();
    vk::PipelineVertexInputStateCreateInfo vertexInput =
        GetQuadInputStateCreateInfo(true);
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        vk::PipelineInputAssemblyStateCreateFlags(),
        vk::PrimitiveTopology::eTriangleStrip);
    vk::PipelineViewportStateCreateInfo viewportState(
        vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.lineWidth = 1.0f;
    vk::PipelineMultisampleStateCreateInfo multisample;
    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    vk::PipelineColorBlendAttachmentState blendAttachment(
        false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo colorBlend(
        vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
        blendAttachment, {{1.f, 1.f, 1.f, 1.f}});
    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState(
        vk::PipelineDynamicStateCreateFlags(), dynamicStates);
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eVertex,
            shaderManager->GetQuadVertexShader(false), "main"),
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eFragment,
            shaderManager->GetGBufferHUDOverlayFragmentShader(), "main"),
    };
    pipeline =
        ctx->GetDevice()
            .createGraphicsPipelineUnique(
                ctx->GetPipelineCache(),
                vk::GraphicsPipelineCreateInfo(
                    vk::PipelineCreateFlags(), stages, &vertexInput,
                    &inputAssembly, nullptr, &viewportState, &rasterization,
                    &multisample, &depthStencil, &colorBlend, &dynamicState,
                    *pipelineLayout, *renderPass, 0))
            .value;
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
  std::vector<vk::ImageView> accumulationViews, hudViews;
  std::vector<vk::ImageView> finalViews;
  std::vector<std::unique_ptr<FramebufferAttachment>> finalBuffers;
};

class GBufferVulkanRenderer final : public BaseVulkanRenderer {
public:
  GBufferVulkanRenderer() {}

  bool Init() override {
    NOTICE_LOG(RENDERER, "GBufferVulkanRenderer::Init");
    try {
      std::vector<vk::Format> formats;
      formats.resize(5);
      formats[GBUFFER_ALBEDO_INDEX] = vk::Format::eR8G8B8A8Unorm;
      formats[GBUFFER_NORMAL_INDEX] = vk::Format::eR16G16B16A16Sfloat;
      formats[GBUFFER_MATERIAL_INDEX] = vk::Format::eR8Uint;
      formats[GBUFFER_MOTION_INDEX] = vk::Format::eR16G16Sfloat;
      formats[GBUFFER_HUD_INDEX] = vk::Format::eR8G8B8A8Unorm;
      screenDrawer.Init(&samplerManager, &shaderManager, viewport, formats);
      screenDrawer.SetCommandPool(&texCommandPool);
      BaseInit(screenDrawer.GetRenderPass());

      initSSAO();
      init3DResolve();
      initDoF();
      initHUDOverlay();

      return true;
    } catch (const vk::SystemError &err) {
      ERROR_LOG(RENDERER, "Vulkan system error: %s", err.what());
      return false;
    }
  }

  void Term() override {
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Term");
    GetContext()->WaitIdle();
    ssaoPass.Term();
    hudOverlayPass.Term();
    dofPass.Term();
    resolve3DPass.Term();
    texCommandPool.Term();
    screenDrawer.Term();
    shaderManager.term();
    samplerManager.term();
    BaseVulkanRenderer::Term();
  }

  void Process(TA_context *ctx) override { BaseVulkanRenderer::Process(ctx); }

  bool Render() override {
    resize(rendContext->framebufferWidth, rendContext->framebufferHeight);

    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Render screenDrawer.Draw");
    screenDrawer.setRendContext(rendContext);
    screenDrawer.Draw(fogTexture.get(), paletteTexture.get());

    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Render end");

    return true;
  }

  bool Present() override {
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Present start");

    int imgIdx = screenDrawer.GetCurrentImageIndex();
    FramebufferAttachment *depthAtt = screenDrawer.GetDepthAttachment();
    FramebufferAttachment *normalAtt =
        screenDrawer.GetColorAttachment(imgIdx, GBUFFER_NORMAL_INDEX);

    // Initialisation lazy
    if ((config::EnableSSAO || config::ShowSSAO) && !ssaoPass.IsInitialized())
      initSSAO();
    if (config::EnableDoF && !dofPass.IsInitialized())
      initDoF();
    else if (!config::EnableDoF && dofPass.IsInitialized())
      dofPass.Term();
    if (!resolve3DPass.IsInitialized())
      init3DResolve();
    if (!hudOverlayPass.IsInitialized())
      initHUDOverlay();

    bool doSSAO = (config::EnableSSAO || config::ShowSSAO) &&
                  ssaoPass.IsInitialized() && depthAtt && normalAtt;
    bool doDoF = config::EnableDoF && dofPass.IsInitialized() && depthAtt &&
                 resolve3DPass.GetAccumulationAttachment(imgIdx);

    vk::CommandBuffer cmdBuf = screenDrawer.EndRenderPassOnly();
    int viewMode = 0; // Final
    if (cmdBuf) {
      static const float scopeColor[4] = {0.25f, 0.25f, 0.25f, 0.25f};
      CommandBufferDebugScope _(cmdBuf, "GBuffer Render", scopeColor);

      // Sélection de la vue debug
      if (config::ShowAlbedo)
        viewMode = 1;
      else if (config::ShowNormals)
        viewMode = 2;
      else if (config::ShowDepth)
        viewMode = 3;
      else if (config::ShowMaterial)
        viewMode = 4;
      else if (config::ShowMotion)
        viewMode = 5;
      else if (config::ShowSSAO)
        viewMode = 6;
      else if (config::ShowHUD)
        viewMode = 7;

      if (doSSAO && (viewMode == 0 || viewMode == 6))
        ssaoPass.Draw(cmdBuf, imgIdx, depthAtt->GetImageView(),
                      normalAtt->GetImageView(), viewMode == 6);

      resolve3DPass.Draw(cmdBuf, imgIdx, viewMode);

      if (doDoF && viewMode == 0)
        dofPass.Draw(cmdBuf, imgIdx,
                     resolve3DPass.GetAccumulationImageView(imgIdx),
                     depthAtt->GetImageView());

      if (viewMode == 0 || viewMode == 7)
        hudOverlayPass.Draw(cmdBuf, imgIdx, viewMode);
    }

    bool ret = screenDrawer.PresentFrame(
        viewMode == 0 || viewMode == 7
            ? hudOverlayPass.GetFinalAttachment(imgIdx)
            : resolve3DPass.GetAccumulationAttachment(imgIdx));
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::Present end (%s)",
              ret ? "success" : "skipped");
    return ret;
  }

  void ExportGBuffer() override;

protected:
  void resize(int w, int h) override {
    if ((u32)w == viewport.width && (u32)h == viewport.height)
      return;
    NOTICE_LOG(RENDERER, "GBufferVulkanRenderer::resize(%d, %d)", w, h);
    BaseVulkanRenderer::resize(w, h);
    GetContext()->WaitIdle();
    std::vector<vk::Format> formats = {
        vk::Format::eR8G8B8A8Unorm,      // Albedo (0)
        vk::Format::eR16G16B16A16Sfloat, // Normals (1)
        vk::Format::eR8Uint,             // Material ID (2)
        vk::Format::eR16G16Sfloat,       // Motion (velocity) (3)
        vk::Format::eR8G8B8A8Unorm       // HUD (4)
    };
    screenDrawer.Init(&samplerManager, &shaderManager, viewport, formats);
    initSSAO();
    init3DResolve();
    initDoF();
    initHUDOverlay();
  }

private:
  void initSSAO() {
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO start");
    if (!config::EnableSSAO && !config::ShowSSAO) {
      DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO: SSAO disabled");
      ssaoPass.Term();
      return;
    }

    // Collecte les albedo views pour tous les indices de swap chain
    int swapSize = (int)screenDrawer.GetSwapChainCount();
    std::vector<vk::ImageView> albedoViews;
    albedoViews.reserve(swapSize);
    for (int i = 0; i < swapSize; ++i) {
      FramebufferAttachment *att =
          screenDrawer.GetColorAttachment(i, GBUFFER_ALBEDO_INDEX);
      if (att)
        albedoViews.push_back(att->GetImageView());
      else
        DEBUG_LOG(RENDERER,
                  "GBufferVulkanRenderer::initSSAO: Missing albedo attachment "
                  "for swap index %d",
                  i);
    }

    if (!albedoViews.empty()) {
      DEBUG_LOG(RENDERER,
                "GBufferVulkanRenderer::initSSAO calling ssaoPass.Init with "
                "%zu views",
                albedoViews.size());
      ssaoPass.Init(&shaderManager, viewport, albedoViews);
    } else {
      ERROR_LOG(RENDERER,
                "GBufferVulkanRenderer::initSSAO: No albedo views found!");
    }
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initSSAO end");
  }

  void initDoF() {
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initDoF start");
    if (!config::EnableDoF) {
      DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initDoF: DoF disabled");
      dofPass.Term();
      return;
    }

    int swapSize = (int)screenDrawer.GetSwapChainCount();
    std::vector<vk::ImageView> albedoViews;
    std::vector<vk::Image> albedoImages;
    albedoViews.reserve(swapSize);
    albedoImages.reserve(swapSize);
    for (int i = 0; i < swapSize; ++i) {
      FramebufferAttachment *att = resolve3DPass.GetAccumulationAttachment(i);
      if (att) {
        albedoViews.push_back(att->GetImageView());
        albedoImages.push_back(att->GetImage());
      }
    }

    if (!albedoViews.empty()) {
      dofPass.Init(&shaderManager, viewport, albedoViews, albedoImages);
    }
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initDoF end");
  }

  void init3DResolve() {
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::init3DResolve start");
    int swapSize = (int)screenDrawer.GetSwapChainCount();
    std::vector<vk::ImageView> albedoViews, normalViews, depthViews,
        materialViews, motionViews, ssaoViews;
    for (int i = 0; i < swapSize; i++) {
      albedoViews.push_back(
          screenDrawer.GetColorAttachment(i, GBUFFER_ALBEDO_INDEX)
              ->GetImageView());
      normalViews.push_back(
          screenDrawer.GetColorAttachment(i, GBUFFER_NORMAL_INDEX)
              ->GetImageView());
      depthViews.push_back(screenDrawer.GetDepthAttachment()->GetImageView());
      materialViews.push_back(
          screenDrawer.GetColorAttachment(i, GBUFFER_MATERIAL_INDEX)
              ->GetImageView());
      motionViews.push_back(
          screenDrawer.GetColorAttachment(i, GBUFFER_MOTION_INDEX)
              ->GetImageView());
      ssaoViews.push_back(ssaoPass.IsInitialized()
                              ? ssaoPass.GetSSAOImageView(i)
                              : vk::ImageView{});
    }
    resolve3DPass.Init(&shaderManager, viewport, albedoViews, normalViews,
                       depthViews, materialViews, motionViews, ssaoViews);
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::init3DResolve end");
  }

  void initHUDOverlay() {
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initHUDOverlay start");
    int swapSize = (int)screenDrawer.GetSwapChainCount();
    std::vector<vk::ImageView> accumulationViews;
    std::vector<vk::ImageView> hudViews;
    accumulationViews.reserve(swapSize);
    hudViews.reserve(swapSize);
    for (int i = 0; i < swapSize; ++i) {
      accumulationViews.push_back(resolve3DPass.GetAccumulationImageView(i));
      hudViews.push_back(screenDrawer.GetColorAttachment(i, GBUFFER_HUD_INDEX)
                             ->GetImageView());
    }
    hudOverlayPass.Init(&shaderManager, viewport, accumulationViews, hudViews);
    DEBUG_LOG(RENDERER, "GBufferVulkanRenderer::initHUDOverlay end");
  }

  SamplerManager samplerManager;
  ScreenDrawer screenDrawer;
  SSAOPass ssaoPass;
  DoFPass dofPass;
  GBuffer3DResolvePass resolve3DPass;
  GBufferHUDOverlayPass hudOverlayPass;
};

void GBufferVulkanRenderer::ExportGBuffer() {
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
    ERROR_LOG(RENDERER, "ExportGBuffer: Invalid viewport dimensions %ux%u",
              width, height);
    return;
  }

  int imgIdx = screenDrawer.GetCurrentImageIndex();

  // 1. Définition des sources de données
  auto albedoAtt =
      screenDrawer.GetColorAttachment(imgIdx, GBUFFER_ALBEDO_INDEX);
  auto normalAtt =
      screenDrawer.GetColorAttachment(imgIdx, GBUFFER_NORMAL_INDEX);
  auto materialAtt =
      screenDrawer.GetColorAttachment(imgIdx, GBUFFER_MATERIAL_INDEX);
  auto motionAtt =
      screenDrawer.GetColorAttachment(imgIdx, GBUFFER_MOTION_INDEX);
  auto hudAtt = screenDrawer.GetColorAttachment(imgIdx, GBUFFER_HUD_INDEX);
  auto depthAtt = screenDrawer.GetDepthAttachment();
  vk::Image ssaoImage =
      ssaoPass.IsInitialized() ? ssaoPass.GetSSAOImage(imgIdx) : vk::Image{};

  if (!albedoAtt || !normalAtt || !depthAtt) {
    ERROR_LOG(RENDERER, "ExportGBuffer: Missing critical attachments");
    return;
  }

  vk::Format depthFmt = screenDrawer.GetDepthFormat();

  // 2. Allouer les staging buffers (Copie brute du GPU)
  BufferData stageA(width * height * 4,
                    vk::BufferUsageFlagBits::eTransferDst); // RGBA8
  BufferData stageN(width * height * 8,
                    vk::BufferUsageFlagBits::eTransferDst); // RGBA16F
  BufferData stageD(width * height * 4,
                    vk::BufferUsageFlagBits::eTransferDst); // D32F or D24S8
  std::unique_ptr<BufferData> stageMat =
      materialAtt ? std::make_unique<BufferData>(
                        width * height, vk::BufferUsageFlagBits::eTransferDst)
                  : nullptr;
  std::unique_ptr<BufferData> stageSSAO =
      ssaoImage ? std::make_unique<BufferData>(
                      width * height, vk::BufferUsageFlagBits::eTransferDst)
                : nullptr;
  std::unique_ptr<BufferData> stageMotion =
      motionAtt ? std::make_unique<BufferData>(
                      width * height * 4, vk::BufferUsageFlagBits::eTransferDst)
                : nullptr;
  std::unique_ptr<BufferData> stageHUD =
      hudAtt ? std::make_unique<BufferData>(
                   width * height * 4, vk::BufferUsageFlagBits::eTransferDst)
             : nullptr;

  try {
    texCommandPool.BeginFrame();
    vk::CommandBuffer cmd = texCommandPool.Allocate(true);
    cmd.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    // Transitions vers TransferSrc
    std::vector<vk::ImageMemoryBarrier> barriers;
    auto addBarrier = [&](vk::Image img, vk::ImageAspectFlags aspect,
                          vk::ImageLayout oldLayout) {
      if (img) {
        vk::AccessFlags srcAccess =
            vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite |
            vk::AccessFlagBits::eColorAttachmentWrite |
            vk::AccessFlagBits::eDepthStencilAttachmentWrite |
            vk::AccessFlagBits::eTransferWrite;
        barriers.push_back(vk::ImageMemoryBarrier(
            srcAccess, vk::AccessFlagBits::eTransferRead, oldLayout,
            vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED, img,
            vk::ImageSubresourceRange(aspect, 0, 1, 0, 1)));
      }
    };

    addBarrier(albedoAtt->GetImage(), vk::ImageAspectFlagBits::eColor,
               vk::ImageLayout::eShaderReadOnlyOptimal);
    addBarrier(normalAtt->GetImage(), vk::ImageAspectFlagBits::eColor,
               vk::ImageLayout::eShaderReadOnlyOptimal);
    addBarrier(depthAtt->GetImage(), vk::ImageAspectFlagBits::eDepth,
               vk::ImageLayout::eDepthStencilReadOnlyOptimal);
    if (materialAtt)
      addBarrier(materialAtt->GetImage(), vk::ImageAspectFlagBits::eColor,
                 vk::ImageLayout::eShaderReadOnlyOptimal);
    if (ssaoImage)
      addBarrier(ssaoImage, vk::ImageAspectFlagBits::eColor,
                 vk::ImageLayout::eShaderReadOnlyOptimal);
    if (motionAtt)
      addBarrier(motionAtt->GetImage(), vk::ImageAspectFlagBits::eColor,
                 vk::ImageLayout::eShaderReadOnlyOptimal);
    if (hudAtt)
      addBarrier(hudAtt->GetImage(), vk::ImageAspectFlagBits::eColor,
                 vk::ImageLayout::eShaderReadOnlyOptimal);

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllGraphics,
                        vk::PipelineStageFlagBits::eTransfer, {}, nullptr,
                        nullptr, barriers);

    // Copies Image -> Buffer
    vk::BufferImageCopy region(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {0, 0, 0}, {width, height, 1});
    cmd.copyImageToBuffer(albedoAtt->GetImage(),
                          vk::ImageLayout::eTransferSrcOptimal,
                          stageA.buffer.get(), region);
    cmd.copyImageToBuffer(normalAtt->GetImage(),
                          vk::ImageLayout::eTransferSrcOptimal,
                          stageN.buffer.get(), region);
    if (stageMat)
      cmd.copyImageToBuffer(materialAtt->GetImage(),
                            vk::ImageLayout::eTransferSrcOptimal,
                            stageMat->buffer.get(), region);
    if (stageSSAO)
      cmd.copyImageToBuffer(ssaoImage, vk::ImageLayout::eTransferSrcOptimal,
                            stageSSAO->buffer.get(), region);
    if (stageMotion)
      cmd.copyImageToBuffer(motionAtt->GetImage(),
                            vk::ImageLayout::eTransferSrcOptimal,
                            stageMotion->buffer.get(), region);
    if (stageHUD)
      cmd.copyImageToBuffer(hudAtt->GetImage(),
                            vk::ImageLayout::eTransferSrcOptimal,
                            stageHUD->buffer.get(), region);

    vk::BufferImageCopy dRegion(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eDepth, 0, 0, 1),
        {0, 0, 0}, {width, height, 1});
    cmd.copyImageToBuffer(depthAtt->GetImage(),
                          vk::ImageLayout::eTransferSrcOptimal,
                          stageD.buffer.get(), dRegion);

    // Transition retour
    for (auto &b : barriers) {
      std::swap(b.oldLayout, b.newLayout);
      std::swap(b.srcAccessMask, b.dstAccessMask);
    }
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eAllGraphics, {}, nullptr,
                        nullptr, barriers);

    cmd.end();
    texCommandPool.EndFrameAndWait();

    // 3. Préparation des canaux EXR (Mapping & Tri)
    struct ChannelData {
      std::string name;
      std::vector<float> dataFloat;
      std::vector<u32> dataUint;
      std::vector<u16> dataHalf;
      int pixelType; // TINYEXR_PIXELTYPE_*
    };
    std::vector<ChannelData> channels;

    // --- Extraction Albedo (RGBA8) ---
    u8 *ptrA = (u8 *)stageA.MapMemory();
    if (ptrA) {
      channels.push_back({"R",
                          std::vector<float>(width * height),
                          {},
                          {},
                          TINYEXR_PIXELTYPE_FLOAT});
      channels.push_back({"G",
                          std::vector<float>(width * height),
                          {},
                          {},
                          TINYEXR_PIXELTYPE_FLOAT});
      channels.push_back({"B",
                          std::vector<float>(width * height),
                          {},
                          {},
                          TINYEXR_PIXELTYPE_FLOAT});
      channels.push_back({"A",
                          std::vector<float>(width * height),
                          {},
                          {},
                          TINYEXR_PIXELTYPE_FLOAT});
      for (u32 i = 0; i < width * height; ++i) {
        channels[channels.size() - 4].dataFloat[i] =
            (float)ptrA[i * 4 + 0] / 255.0f;
        channels[channels.size() - 3].dataFloat[i] =
            (float)ptrA[i * 4 + 1] / 255.0f;
        channels[channels.size() - 2].dataFloat[i] =
            (float)ptrA[i * 4 + 2] / 255.0f;
        channels[channels.size() - 1].dataFloat[i] =
            (float)ptrA[i * 4 + 3] / 255.0f;
      }
      stageA.UnmapMemory();
    }

    // --- Extraction Normales (Pass-through Half) ---
    u16 *ptrN = (u16 *)stageN.MapMemory();
    if (ptrN) {
      channels.push_back({"Normal.X",
                          {},
                          {},
                          std::vector<u16>(width * height),
                          TINYEXR_PIXELTYPE_HALF});
      channels.push_back({"Normal.Y",
                          {},
                          {},
                          std::vector<u16>(width * height),
                          TINYEXR_PIXELTYPE_HALF});
      channels.push_back({"Normal.Z",
                          {},
                          {},
                          std::vector<u16>(width * height),
                          TINYEXR_PIXELTYPE_HALF});
      for (u32 i = 0; i < width * height; ++i) {
        channels[channels.size() - 3].dataHalf[i] = ptrN[i * 4 + 0];
        channels[channels.size() - 2].dataHalf[i] = ptrN[i * 4 + 1];
        channels[channels.size() - 1].dataHalf[i] = ptrN[i * 4 + 2];
      }
      stageN.UnmapMemory();
    }

    // --- Extraction Depth (Pass-through if D32F) ---
    void *ptrD = stageD.MapMemory();
    if (ptrD) {
      channels.push_back({"Depth.Z",
                          std::vector<float>(width * height),
                          {},
                          {},
                          TINYEXR_PIXELTYPE_FLOAT});
      // On accepte D32F et D32F_S8 (Vulkan compacte l'aspect depth en 32-bit
      // float lors de la copie)
      if (depthFmt == vk::Format::eD32Sfloat ||
          depthFmt == vk::Format::eD32SfloatS8Uint) {
        memcpy(channels.back().dataFloat.data(), ptrD, width * height * 4);
      } else {
        // Cas D24_S8 ou autre format entier
        u32 *src = (u32 *)ptrD;
        for (u32 i = 0; i < width * height; ++i)
          channels.back().dataFloat[i] = (src[i] & 0xFFFFFF) / 16777215.0f;
      }
      stageD.UnmapMemory();
    }

    // --- Extraction Material ID (UINT) ---
    if (stageMat) {
      u8 *ptrM = (u8 *)stageMat->MapMemory();
      if (ptrM) {
        channels.push_back({"Material.ID",
                            {},
                            std::vector<u32>(width * height),
                            {},
                            TINYEXR_PIXELTYPE_UINT});
        for (u32 i = 0; i < width * height; ++i)
          channels.back().dataUint[i] = (u32)ptrM[i];

        stageMat->UnmapMemory();
      }
    }

    // --- Extraction SSAO ---
    if (stageSSAO) {
      u8 *ptrS = (u8 *)stageSSAO->MapMemory();
      if (ptrS) {
        channels.push_back({"SSAO.AO",
                            std::vector<float>(width * height),
                            {},
                            {},
                            TINYEXR_PIXELTYPE_FLOAT});
        for (u32 i = 0; i < width * height; ++i)
          channels.back().dataFloat[i] = ptrS[i] / 255.f;
        stageSSAO->UnmapMemory();
      }
    }

    // --- Extraction Motion (Pass-through Half) ---
    if (stageMotion) {
      u16 *ptrMo = (u16 *)stageMotion->MapMemory();
      if (ptrMo) {
        channels.push_back({"Motion.X",
                            {},
                            {},
                            std::vector<u16>(width * height),
                            TINYEXR_PIXELTYPE_HALF});
        channels.push_back({"Motion.Y",
                            {},
                            {},
                            std::vector<u16>(width * height),
                            TINYEXR_PIXELTYPE_HALF});
        for (u32 i = 0; i < width * height; ++i) {
          channels[channels.size() - 2].dataHalf[i] = ptrMo[i * 2 + 0];
          channels[channels.size() - 1].dataHalf[i] = ptrMo[i * 2 + 1];
        }
        stageMotion->UnmapMemory();
      }
    }

    // --- Extraction HUD ---
    if (stageHUD) {
      u8 *ptrH = (u8 *)stageHUD->MapMemory();
      if (ptrH) {
        channels.push_back({"HUD.R",
                            std::vector<float>(width * height),
                            {},
                            {},
                            TINYEXR_PIXELTYPE_FLOAT});
        channels.push_back({"HUD.G",
                            std::vector<float>(width * height),
                            {},
                            {},
                            TINYEXR_PIXELTYPE_FLOAT});
        channels.push_back({"HUD.B",
                            std::vector<float>(width * height),
                            {},
                            {},
                            TINYEXR_PIXELTYPE_FLOAT});
        channels.push_back({"HUD.A",
                            std::vector<float>(width * height),
                            {},
                            {},
                            TINYEXR_PIXELTYPE_FLOAT});
        for (u32 i = 0; i < width * height; ++i) {
          channels[channels.size() - 4].dataFloat[i] = ptrH[i * 4 + 0] / 255.f;
          channels[channels.size() - 3].dataFloat[i] = ptrH[i * 4 + 1] / 255.f;
          channels[channels.size() - 2].dataFloat[i] = ptrH[i * 4 + 2] / 255.f;
          channels[channels.size() - 1].dataFloat[i] = ptrH[i * 4 + 3] / 255.f;
        }
        stageHUD->UnmapMemory();
      }
    }

    // 4. TRI ALPHABÉTIQUE (Crucial pour TinyEXR)
    std::sort(channels.begin(), channels.end(),
              [](const ChannelData &a, const ChannelData &b) {
                return a.name < b.name;
              });

    // 5. Configuration finale TinyEXR
    EXRHeader header;
    InitEXRHeader(&header);
    EXRImage image;
    InitEXRImage(&image);

    header.num_channels = (int)channels.size();
    header.channels =
        (EXRChannelInfo *)malloc(sizeof(EXRChannelInfo) * header.num_channels);
    header.pixel_types = (int *)malloc(sizeof(int) * header.num_channels);
    header.requested_pixel_types =
        (int *)malloc(sizeof(int) * header.num_channels);

    unsigned char *image_ptr[64]; // Max 64 channels
    for (int i = 0; i < header.num_channels; i++) {
      strncpy(header.channels[i].name, channels[i].name.c_str(), 255);
      header.channels[i].name[255] = '\0';
      header.pixel_types[i] = channels[i].pixelType;
      // On préserve le type original (FLOAT 32 ou HALF 16) pour éviter les
      // pertes de précision en Deep Learning
      header.requested_pixel_types[i] = channels[i].pixelType;

      if (channels[i].pixelType == TINYEXR_PIXELTYPE_FLOAT)
        image_ptr[i] = (unsigned char *)channels[i].dataFloat.data();
      else if (channels[i].pixelType == TINYEXR_PIXELTYPE_UINT)
        image_ptr[i] = (unsigned char *)channels[i].dataUint.data();
      else
        image_ptr[i] = (unsigned char *)channels[i].dataHalf.data();
    }

    image.num_channels = header.num_channels;
    image.images = image_ptr;
    image.width = width;
    image.height = height;

    // Sauvegarde
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << "gbuffer_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".exr";
    std::string fullPath = hostfs::getScreenshotsPath() + "/" + oss.str();

    const char *err = nullptr;
    if (SaveEXRImageToFile(&image, &header, fullPath.c_str(), &err) !=
        TINYEXR_SUCCESS) {
      ERROR_LOG(RENDERER, "SaveEXR failed: %s", err);
      if (err)
        FreeEXRErrorMessage(err);
    } else {
      NOTICE_LOG(RENDERER, "G-Buffer exported to %s (%d channels)",
                 fullPath.c_str(), header.num_channels);
    }

    free(header.channels);
    free(header.pixel_types);
    free(header.requested_pixel_types);
  } catch (const std::exception &e) {
    ERROR_LOG(RENDERER, "ExportGBuffer exception: %s", e.what());
  }
}

Renderer *rend_GBufferVulkan() { return new GBufferVulkanRenderer(); }
