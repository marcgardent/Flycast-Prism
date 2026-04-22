#include "VulkanHudCompositor.h"
#include "../vulkan_context.h"
#include <algorithm>

void VulkanHudCompositor::Init(vk::Extent2D viewport, int swapchainSize) {
    m_viewport = viewport;
    
    m_sourceBuffers.clear();
    m_compositionBuffers.clear();
    
    for (int i = 0; i < swapchainSize; ++i) {
        // Source buffer: fixed 640x480 RGBA8
        m_sourceBuffers.push_back(std::make_unique<BufferData>(
            VIRT_W * VIRT_H * 4,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));
        
        // Destination buffer: viewport-sized RGBA8
        m_compositionBuffers.push_back(std::make_unique<BufferData>(
            m_viewport.width * m_viewport.height * 4,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));
    }
}

void VulkanHudCompositor::Term() {
    m_sourceBuffers.clear();
    m_compositionBuffers.clear();
}

void VulkanHudCompositor::UpdateViewport(vk::Extent2D viewport) {
    if (m_viewport == viewport && !m_compositionBuffers.empty())
        return;
        
    int swapchainSize = (int)m_compositionBuffers.size();
    m_viewport = viewport;
    
    m_compositionBuffers.clear();
    for (int i = 0; i < swapchainSize; ++i) {
        m_compositionBuffers.push_back(std::make_unique<BufferData>(
            m_viewport.width * m_viewport.height * 4,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));
    }
}

void VulkanHudCompositor::Recompose(vk::CommandBuffer cmd, 
                                   int imgIdx,
                                   const std::vector<TransformedHudElement>& elements,
                                   vk::Image srcImage, 
                                   vk::Image dstImage) {
    if (imgIdx < 0 || imgIdx >= (int)m_sourceBuffers.size() || elements.empty())
        return;

    auto& sourceBuffer = m_sourceBuffers[imgIdx];
    auto& compositionBuffer = m_compositionBuffers[imgIdx];

    // 1. Copy srcImage to sourceBuffer
    vk::BufferImageCopy srcRegion(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {0, 0, 0}, {VIRT_W, VIRT_H, 1}
    );
    cmd.copyImageToBuffer(srcImage, vk::ImageLayout::eTransferSrcOptimal, sourceBuffer->buffer.get(), srcRegion);

    // 2. Clear compositionBuffer (alpha = 0)
    cmd.fillBuffer(compositionBuffer->buffer.get(), 0, VK_WHOLE_SIZE, 0);

    // Barrier to ensure clearing and copy to source buffer are finished before recomposing
    vk::BufferMemoryBarrier barrier{};
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite;
    barrier.buffer = sourceBuffer->buffer.get();
    barrier.size = VK_WHOLE_SIZE;
    
    vk::BufferMemoryBarrier barrierDst{};
    barrierDst.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrierDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrierDst.buffer = compositionBuffer->buffer.get();
    barrierDst.size = VK_WHOLE_SIZE;
    
    std::array<vk::BufferMemoryBarrier, 2> barriers = {barrier, barrierDst};
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, barriers, nullptr);

    // 3. Generate VkBufferCopy regions for recomposition (Batching)
    std::vector<vk::BufferCopy> regions;
    for (const auto& el : elements) {
        // We use the real coordinates provided by HudCompositor (no re-calculation needed)
        uint32_t srcX = static_cast<uint32_t>(std::max(0.0f, el.sourceRect.x));
        uint32_t srcY = static_cast<uint32_t>(std::max(0.0f, el.sourceRect.y));
        uint32_t dstX = static_cast<uint32_t>(std::max(0.0f, el.viewportRect.x));
        uint32_t dstY = static_cast<uint32_t>(std::max(0.0f, el.viewportRect.y));
        uint32_t w = static_cast<uint32_t>(el.sourceRect.w);
        uint32_t h = static_cast<uint32_t>(el.sourceRect.h);

        // Clip to avoid out of bounds
        if (srcX + w > VIRT_W) w = VIRT_W - srcX;
        if (srcY + h > VIRT_H) h = VIRT_H - srcY;
        if (dstX + w > m_viewport.width) w = m_viewport.width - dstX;
        if (dstY + h > m_viewport.height) h = m_viewport.height - dstY;

        for (uint32_t row = 0; row < h; ++row) {
            vk::BufferCopy region{};
            region.srcOffset = ((srcY + row) * VIRT_W + srcX) * 4;
            region.dstOffset = ((dstY + row) * m_viewport.width + dstX) * 4;
            region.size = w * 4;
            regions.push_back(region);
        }
    }

    if (!regions.empty()) {
        cmd.copyBuffer(sourceBuffer->buffer.get(), compositionBuffer->buffer.get(), regions);
    }

    // Barrier before copying back to image
    vk::BufferMemoryBarrier postBarrier{};
    postBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    postBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    postBarrier.buffer = compositionBuffer->buffer.get();
    postBarrier.size = VK_WHOLE_SIZE;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, postBarrier, nullptr);

    // 4. Copy compositionBuffer back to dstImage
    vk::BufferImageCopy dstRegion(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {0, 0, 0}, {m_viewport.width, m_viewport.height, 1}
    );
    cmd.copyBufferToImage(compositionBuffer->buffer.get(), dstImage, vk::ImageLayout::eTransferDstOptimal, dstRegion);
}
