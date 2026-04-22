#include "VulkanHudCompositor.h"
#include "../vulkan_context.h"
#include <algorithm>

/**
 * @brief Initializes the composition resources.
 * Using buffers (rather than textures) avoids layout transition overheads
 * and facilitates direct memory access for BitBlt-like operations.
 */
void VulkanHudCompositor::Init(vk::Extent2D viewport, int swapchainSize) {
    m_viewport = viewport;

    m_sourceBuffers.clear();
    m_compositionBuffers.clear();

    for (int i = 0; i < swapchainSize; ++i) {
        // Both buffers have the exact size of the viewport (the final destination)
        size_t bufferSize = m_viewport.width * m_viewport.height * 4;

        // Allocate buffers in pairs for double-buffered composition
        m_sourceBuffers.push_back(std::make_unique<BufferData>(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));

        m_compositionBuffers.push_back(std::make_unique<BufferData>(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));
    }
}

void VulkanHudCompositor::Term() {
    m_sourceBuffers.clear();
    m_compositionBuffers.clear();
}

/**
 * @brief Updates buffer dimensions if the viewport changes.
 * State of the art: Cleanly free and reallocate both sets of buffers.
 */
void VulkanHudCompositor::UpdateViewport(vk::Extent2D viewport) {
    if (m_viewport == viewport && !m_compositionBuffers.empty())
        return;

    int swapchainSize = (int)m_compositionBuffers.size();
    m_viewport = viewport;

    // Complete reset to ensure size consistency
    m_sourceBuffers.clear();
    m_compositionBuffers.clear();

    for (int i = 0; i < swapchainSize; ++i) {
        // Maintain sizes based solely on the viewport
        size_t bufferSize = m_viewport.width * m_viewport.height * 4;

        m_sourceBuffers.push_back(std::make_unique<BufferData>(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));

        m_compositionBuffers.push_back(std::make_unique<BufferData>(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        ));
    }
}

/**
 * @brief Recomposes HUD elements by moving memory blocks.
 * @note This implementation is "exclusive": each destination zone is treated
 * as independent, allowing massive batching of copy commands.
 */
void VulkanHudCompositor::Recompose(vk::CommandBuffer cmd,
                                   int imgIdx,
                                   const std::vector<TransformedHudElement>& elements,
                                   vk::Image srcImage,
                                   vk::Image dstImage) {

    if (imgIdx < 0 || imgIdx >= (int)m_sourceBuffers.size() || elements.empty())
        return;

    auto& sourceBuffer = m_sourceBuffers[imgIdx];
    auto& compositionBuffer = m_compositionBuffers[imgIdx];

    // --- 1. CAPTURE: Image to linear buffer ---
    // Bring the image into a buffered memory space for 2D-in-1D manipulation
    vk::BufferImageCopy srcRegion(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {0, 0, 0}, {m_viewport.width, m_viewport.height, 1}
    );
    cmd.copyImageToBuffer(srcImage, vk::ImageLayout::eTransferSrcOptimal, sourceBuffer->buffer.get(), srcRegion);

    // --- 2. CLEAR: Destination buffer initialization ---
    // Fill the composition buffer with zeros (alpha = 0).
    // fillBuffer is extremely fast as it's a DMA operation.
    cmd.fillBuffer(compositionBuffer->buffer.get(), 0, VK_WHOLE_SIZE, 0);

    // --- 3. SYNCHRONIZATION BARRIER ---
    // Wait for write operations (Fill and CopyImageToBuffer) to finish before reading/writing for composition.
    vk::BufferMemoryBarrier preBarrierSrc{};
    preBarrierSrc.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    preBarrierSrc.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    preBarrierSrc.buffer = sourceBuffer->buffer.get();
    preBarrierSrc.size = VK_WHOLE_SIZE;

    vk::BufferMemoryBarrier preBarrierDst{};
    preBarrierDst.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    preBarrierDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    preBarrierDst.buffer = compositionBuffer->buffer.get();
    preBarrierDst.size = VK_WHOLE_SIZE;

    std::array<vk::BufferMemoryBarrier, 2> preBarriers = {preBarrierSrc, preBarrierDst};
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, preBarriers, nullptr);

    // --- 4. COMPOSITION (Region batching) ---
    // Given the very low number of elements (< 10), generating copy commands
    // on the CPU is immediate. This is the most optimal approach (a shader would be counter-productive).
    std::vector<vk::BufferCopy> regions;

    // Optimization: Generously pre-allocate the vector (e.g., estimating 512 lines per element).
    // For < 10 elements, this definitively avoids any costly dynamic reallocation during the loop.
    regions.reserve(elements.size() * 512);

    for (const auto& el : elements) {
        // Source and destination coordinates
        uint32_t srcX = static_cast<uint32_t>(std::max(0.0f, el.sourceRect.x));
        uint32_t srcY = static_cast<uint32_t>(std::max(0.0f, el.sourceRect.y));
        uint32_t dstX = static_cast<uint32_t>(std::max(0.0f, el.viewportRect.x));
        uint32_t dstY = static_cast<uint32_t>(std::max(0.0f, el.viewportRect.y));
        uint32_t w = static_cast<uint32_t>(el.sourceRect.w);
        uint32_t h = static_cast<uint32_t>(el.sourceRect.h);

        // CLIPPING: Critical safety check to prevent out-of-bounds buffer access
        // Source and Destination now share the same size limits (the viewport)
        if (srcX + w > m_viewport.width) w = (m_viewport.width > srcX) ? m_viewport.width - srcX : 0;
        if (srcY + h > m_viewport.height) h = (m_viewport.height > srcY) ? m_viewport.height - srcY : 0;
        if (dstX + w > m_viewport.width) w = (m_viewport.width > dstX) ? m_viewport.width - dstX : 0;
        if (dstY + h > m_viewport.height) h = (m_viewport.height > dstY) ? m_viewport.height - dstY : 0;

        if (w == 0 || h == 0) continue;

        // Split the rectangle into single-line regions (since it's a 1D buffer)
        for (uint32_t row = 0; row < h; ++row) {
            vk::BufferCopy region{};
            // The "stride" (row width) is m_viewport.width for BOTH source and destination
            region.srcOffset = ((srcY + row) * m_viewport.width + srcX) * 4;
            region.dstOffset = ((dstY + row) * m_viewport.width + dstX) * 4;
            region.size = w * 4;
            regions.push_back(region);
        }
    }

    if (!regions.empty()) {
        // Massive submission to the GPU's transfer engine (DMA)
        cmd.copyBuffer(sourceBuffer->buffer.get(), compositionBuffer->buffer.get(), regions);
    }

    // --- 5. FINISH: Buffer to final image ---
    // Barrier to ensure composition is finished before copying back to the image
    vk::BufferMemoryBarrier postBarrier{};
    postBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    postBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    postBarrier.buffer = compositionBuffer->buffer.get();
    postBarrier.size = VK_WHOLE_SIZE;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, postBarrier, nullptr);

    vk::BufferImageCopy dstRegion(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {0, 0, 0}, {m_viewport.width, m_viewport.height, 1}
    );
    cmd.copyBufferToImage(compositionBuffer->buffer.get(), dstImage, vk::ImageLayout::eTransferDstOptimal, dstRegion);
}