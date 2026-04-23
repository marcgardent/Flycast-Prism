#include "VulkanHudCompositor.h"
#include "../vulkan_context.h"
#include "../texture.h"
#include "log/Log.h"
#include <algorithm>

void VulkanHudCompositor::Init(vk::Extent2D renderViewport, int swapchainSize) {
    m_renderViewport = renderViewport;
    m_compositionBuffers.clear();

    for (int i = 0; i < swapchainSize; ++i) {
        size_t bufferSize = m_renderViewport.width * m_renderViewport.height * 4;

        // Destination only: TransferDst (receives fill and copy) / TransferSrc (sends to image)
        m_compositionBuffers.push_back(std::make_unique<BufferData>(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));
    }
}

void VulkanHudCompositor::Term() {
    m_compositionBuffers.clear();
}

void VulkanHudCompositor::UpdateViewport(vk::Extent2D renderViewport) {
    if (m_renderViewport == renderViewport && !m_compositionBuffers.empty())
        return;

    int swapchainSize = (int)m_compositionBuffers.size();
    m_renderViewport = renderViewport;

    m_compositionBuffers.clear();
    for (int i = 0; i < swapchainSize; ++i) {
        size_t bufferSize = m_renderViewport.width * m_renderViewport.height * 4;
        m_compositionBuffers.push_back(std::make_unique<BufferData>(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));
    }
}

void VulkanHudCompositor::Recompose(vk::CommandBuffer cmd,
                                   int imgIdx,
                                   const std::vector<ViewportTransform>& elements,
                                   vk::Image srcGbufferHud,
                                   vk::Image dstSwapchainImage) {

    if (imgIdx < 0 || imgIdx >= (int)m_compositionBuffers.size())
        return;

    if (elements.empty()) {
        std::vector<ViewportTransform> fallback = elements;
        ViewportTransform stub;
        stub.name = "FALLBACK_1_TO_1";
        stub.source = {0.f, 0.f, (float)m_renderViewport.width, (float)m_renderViewport.height};
        stub.destination = {0.f, 0.f, (float)m_renderViewport.width, (float)m_renderViewport.height};
        stub.zenMode = false;
        fallback.push_back(stub);
        Recompose(cmd, imgIdx,fallback, srcGbufferHud, dstSwapchainImage);
        return;
    }

    auto& compositionBuffer = m_compositionBuffers[imgIdx];

    // --- 1. CLEAR THE CANVAS ---
    cmd.fillBuffer(compositionBuffer->buffer.get(), 0, VK_WHOLE_SIZE, 0);

    // --- 2. SYNCHRONIZATION BARRIER ---
    // Secure the composition buffer: wait for fillBuffer to finish
    // before starting to write our copied regions from the image.
    vk::BufferMemoryBarrier preBarrierDst{};
    preBarrierDst.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    preBarrierDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    preBarrierDst.buffer = compositionBuffer->buffer.get();
    preBarrierDst.size = VK_WHOLE_SIZE;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        {}, nullptr, preBarrierDst, nullptr
    );

    // --- 3. DIRECT BATCHING (Major Optimization) ---
    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(elements.size()); // Only one region per element, no more "row" loop!

    for (const auto& el : elements) {
        uint32_t srcX = static_cast<uint32_t>(std::max(0.0f, el.source.x));
        uint32_t srcY = static_cast<uint32_t>(std::max(0.0f, el.source.y));
        uint32_t dstX = static_cast<uint32_t>(std::max(0.0f, el.destination.x));
        uint32_t dstY = static_cast<uint32_t>(std::max(0.0f, el.destination.y));
        uint32_t w = static_cast<uint32_t>(el.source.w);
        uint32_t h = static_cast<uint32_t>(el.source.h);

        // STRICT CLIPPING to the renderViewport
        if (srcX + w > m_renderViewport.width) w = (m_renderViewport.width > srcX) ? m_renderViewport.width - srcX : 0;
        if (srcY + h > m_renderViewport.height) h = (m_renderViewport.height > srcY) ? m_renderViewport.height - srcY : 0;
        if (dstX + w > m_renderViewport.width) w = (m_renderViewport.width > dstX) ? m_renderViewport.width - dstX : 0;
        if (dstY + h > m_renderViewport.height) h = (m_renderViewport.height > dstY) ? m_renderViewport.height - dstY : 0;

        if (w == 0 || h == 0) continue;

        vk::BufferImageCopy region{};

        // Source area read from the GBuffer texture
        region.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        region.imageOffset = vk::Offset3D{static_cast<int32_t>(srcX), static_cast<int32_t>(srcY), 0};
        region.imageExtent = vk::Extent3D{w, h, 1};

        // Destination area written to the composition buffer
        // The magic happens here: We give the driver the starting offset of the top-left corner.
        region.bufferOffset = ((dstY * m_renderViewport.width) + dstX) * 4;

        // And we specify the "width" of our linear grid (stride).
        // The driver will automatically jump to the next line in memory!
        region.bufferRowLength = m_renderViewport.width;
        region.bufferImageHeight = m_renderViewport.height;

        regions.push_back(region);
    }

    if (!regions.empty()) {
        // --- DEBUG PART 1: LOG ALL TRANSFORMS ---
        NOTICE_LOG(RENDERER, "VulkanHudCompositor::Recompose: Processing %zu elements", elements.size());
        for (size_t i = 0; i < elements.size(); ++i) {
            const auto& el = elements[i];
            NOTICE_LOG(RENDERER, "  Transform[%zu]: src={%.1f, %.1f, %.1f, %.1f} -> dst={%.1f, %.1f, %.1f, %.1f}",
                       i, el.source.x, el.source.y, el.source.w, el.source.h,
                       el.destination.x, el.destination.y, el.destination.w, el.destination.h);
        }

        // --- REAL READ: Copy HUD data to buffer ---
        cmd.copyImageToBuffer(srcGbufferHud, vk::ImageLayout::eTransferSrcOptimal, compositionBuffer->buffer.get(), regions);
    }

    // --- 4. BARRIER BEFORE FINAL TRANSFER ---
    vk::BufferMemoryBarrier postBarrier{};
    postBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    postBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    postBarrier.buffer = compositionBuffer->buffer.get();
    postBarrier.size = VK_WHOLE_SIZE;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        {}, nullptr, postBarrier, nullptr
    );

    // --- 5. TRANSFER TO SWAPCHAIN ---
    vk::BufferImageCopy dstRegion{};
    dstRegion.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
    dstRegion.imageOffset = vk::Offset3D{0, 0, 0};
    dstRegion.imageExtent = vk::Extent3D{m_renderViewport.width, m_renderViewport.height, 1};
    dstRegion.bufferOffset = 0;
    dstRegion.bufferRowLength = 0; // 0 means "use imageExtent.width" (perfect for full screen)
    dstRegion.bufferImageHeight = 0;

    cmd.copyBufferToImage(
        compositionBuffer->buffer.get(),
        dstSwapchainImage,
        vk::ImageLayout::eTransferDstOptimal,
        1, &dstRegion
    );
}