#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <memory>

#include "HudCompositor.h"

// Forward declaration de BufferData (selon ton architecture interne)
struct BufferData;

/**
 * @class VulkanHudCompositor
 * @brief Moteur de composition HUD optimisé pour un workflow GBuffer.
 * * * ARCHITECTURE (GBuffer) :
 * 1. Ton fragment shader (main.frag) dessine le HUD dans une texture de ton GBuffer (srcGbufferHud).
 * 2. Le compositeur prend cette texture source en entrée.
 * 3. Il extrait et place précisément les rectangles demandés dans un buffer de composition.
 * 4. Il copie le résultat final vers l'image de la swapchain.
 */
class VulkanHudCompositor {
public:
    VulkanHudCompositor() = default;
    ~VulkanHudCompositor() = default;

    /**
     * @brief Initialise les buffers de composition.
     * @param renderViewport La résolution de rendu INTERNE (taille du GBuffer).
     * @param swapchainSize  Le nombre d'images dans la swapchain.
     */
    void Init(vk::Extent2D renderViewport, int swapchainSize);

    void Term();
    void UpdateViewport(vk::Extent2D renderViewport);

    /**
     * @brief Assemble le HUD depuis le GBuffer vers l'image finale.
     * * * PRÉREQUIS AVANT APPEL :
     * 1. `srcGbufferHud` DOIT être transitionnée en `vk::ImageLayout::eTransferSrcOptimal`.
     * 2. `dstSwapchainImage` DOIT être transitionnée en `vk::ImageLayout::eTransferDstOptimal`.
     * * @param cmd               Command Buffer en cours.
     * @param imgIdx            Index de la frame (swapchain).
     * @param elements          Liste des zones à déplacer.
     * @param srcGbufferHud     L'attachement image de ton GBuffer (HUD).
     * @param dstSwapchainImage L'image finale (Swapchain).
     */
    void Recompose(vk::CommandBuffer cmd,
                   int imgIdx,
                   const std::vector<ViewportTransform>& elements,
                   vk::Image srcGbufferHud,
                   vk::Image dstSwapchainImage);

private:
    vk::Extent2D m_renderViewport;

    // Seuls les buffers de composition sont conservés. La source est le Gbuffer externe.
    std::vector<std::unique_ptr<BufferData>> m_compositionBuffers;
};