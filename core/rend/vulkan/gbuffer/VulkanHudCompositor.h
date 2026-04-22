#pragma once

#include "../vulkan.h"
#include "../buffer.h"
#include "HudCompositor.h"
#include <vector>
#include <memory>

/**
 * @brief Handles high-performance HUD recomposition using DMA transfers.
 */
class VulkanHudCompositor {
public:
    VulkanHudCompositor() = default;
    ~VulkanHudCompositor() { Term(); }

    void Init(vk::Extent2D viewport, int swapchainSize);
    void Term();

    void UpdateViewport(vk::Extent2D viewport);

    /**
     * @brief Recomposes the HUD from a source image to a destination image.
     * 
     * @param cmd Command buffer to record transfers.
     * @param imgIdx Index of the current swapchain image (for multi-buffering).
     * @param elements List of transformed HUD elements from HudCompositor.
     * @param srcImage Source HUD image (640x480).
     * @param dstImage Destination HUD image (viewport size).
     */
    void Recompose(vk::CommandBuffer cmd, 
                  int imgIdx,
                  const std::vector<TransformedHudElement>& elements,
                  vk::Image srcImage, 
                  vk::Image dstImage);

private:
    vk::Extent2D m_viewport {0, 0};
    std::vector<std::unique_ptr<BufferData>> m_sourceBuffers;
    std::vector<std::unique_ptr<BufferData>> m_compositionBuffers;

    static constexpr uint32_t VIRT_W = 640;
    static constexpr uint32_t VIRT_H = 480;
};
