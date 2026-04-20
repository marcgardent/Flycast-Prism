/*
 *  Created on: Oct 3, 2019

        Copyright 2019 flyinghead

        This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "shaders.h"
#include "compiler.h"
#include "gbuffer/gbuffer_constants.h"
#include "utils.h"
#include "vulkan.h"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(flycast);

static std::string loadShaderSource(const std::string &path) {
  auto fs = cmrc::flycast::get_filesystem();
  auto file = fs.open(path);
  return std::string(file.begin(), file.end());
}

vk::UniqueShaderModule
ShaderManager::compileShader(const VertexShaderParams &params) {
  VulkanSource src;
  if (!params.naomi2) {
    src.addConstant("pp_Gouraud", (int)params.gouraud)
        .addConstant("DIV_POS_Z", (int)params.divPosZ)
        .addSource(GouraudSource)
        .addSource(loadShaderSource("shaders/vulkan_main.vert"));
  } else {
    src.addConstant("pp_Gouraud", (int)params.gouraud)
        .addConstant("pp_Texture", (int)params.texture)
        .addSource(GouraudSource)
        .addSource(loadShaderSource("shaders/vulkan_n2_light.glsl"))
        .addSource(loadShaderSource("shaders/vulkan_n2.vert"));
  }
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex,
                                 src.generate());
}

vk::UniqueShaderModule
ShaderManager::compileShader(const FragmentShaderParams &params) {
  VulkanSource src;
  src.addConstant("cp_AlphaTest", (int)params.alphaTest)
      .addConstant("pp_ClipInside", (int)params.insideClipTest)
      .addConstant("pp_UseAlpha", (int)params.useAlpha)
      .addConstant("pp_Texture", (int)params.texture)
      .addConstant("pp_IgnoreTexA", (int)params.ignoreTexAlpha)
      .addConstant("pp_ShadInstr", params.shaderInstr)
      .addConstant("pp_Offset", (int)params.offset)
      .addConstant("pp_FogCtrl", params.fog)
      .addConstant("pp_Gouraud", (int)params.gouraud)
      .addConstant("pp_BumpMap", (int)params.bumpmap)
      .addConstant("ColorClamping", (int)params.clamping)
      .addConstant("pp_TriLinear", (int)params.trilinear)
      .addConstant("pp_Palette", params.palette)
      .addConstant("DIV_POS_Z", (int)params.divPosZ)
      .addConstant("DITHERING", (int)params.dithering)
      .addConstant("ShowDepth", (int)params.showDepth)
      .addConstant("IS_TRANSLUCENT", (int)params.isTranslucent)
      .addConstant("ShowNormals", (int)params.showNormals)
      .addConstant("ShowMaterial", (int)params.showMaterial)
      .addConstant("GBUFFER", (int)params.gbuffer)
      .addConstant("GBUFFER_ALBEDO_INDEX", (int)GBUFFER_ALBEDO_INDEX)
      .addConstant("GBUFFER_NORMAL_INDEX", (int)GBUFFER_NORMAL_INDEX)
      .addConstant("GBUFFER_MATERIAL_INDEX", (int)GBUFFER_MATERIAL_INDEX)
      .addConstant("GBUFFER_MOTION_INDEX", (int)GBUFFER_MOTION_INDEX)
      .addConstant("GBUFFER_HUD_INDEX", (int)GBUFFER_HUD_INDEX)
      .addConstant("EnableSSAO", (int)params.enableSSAO)
      .addConstant("ShowSSAO", (int)params.showSSAO)
      .addConstant("IS_HUD", (int)params.isHud)

      .addSource(GouraudSource)
      .addSource(loadShaderSource("shaders/vulkan_top.frag"))
      .addSource(loadShaderSource("shaders/vulkan_common.frag"))
      .addSource(loadShaderSource("shaders/vulkan_main.frag"));
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
                                 src.generate());
}

vk::UniqueShaderModule
ShaderManager::compileShader(const ModVolShaderParams &params) {
  return ShaderCompiler::Compile(
      vk::ShaderStageFlagBits::eVertex,
      VulkanSource()
          .addConstant("DIV_POS_Z", (int)params.divPosZ)
          .addSource(loadShaderSource(params.naomi2
                                          ? "shaders/vulkan_n2_modvol.vert"
                                          : "shaders/vulkan_modvol.vert"))
          .generate());
}

vk::UniqueShaderModule
ShaderManager::compileModVolFragmentShader(bool divPosZ) {
  return ShaderCompiler::Compile(
      vk::ShaderStageFlagBits::eFragment,
      VulkanSource()
          .addConstant("DIV_POS_Z", (int)divPosZ)
          .addSource(loadShaderSource("shaders/vulkan_modvol.frag"))
          .generate());
}

vk::UniqueShaderModule ShaderManager::compileQuadVertexShader(bool rotate) {
  VulkanSource src;
  src.addConstant("ROTATE", (int)rotate)
      .addSource(loadShaderSource("shaders/vulkan_quad.vert"));
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex,
                                 src.generate());
}

vk::UniqueShaderModule
ShaderManager::compileQuadFragmentShader(bool ignoreTexAlpha) {
  VulkanSource src;
  src.addConstant("IGNORE_TEX_ALPHA", (int)ignoreTexAlpha)
      .addSource(loadShaderSource("shaders/vulkan_quad.frag"));
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
                                 src.generate());
}

vk::UniqueShaderModule ShaderManager::compileSSAOFragmentShader() {
  VulkanSource src;
  src.addSource(loadShaderSource("shaders/vulkan_ssao.frag"));
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
                                 src.generate());
}

vk::UniqueShaderModule ShaderManager::compileDoFFragmentShader() {
  VulkanSource src;
  src.addSource(loadShaderSource("shaders/vulkan_dof.frag"));
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
                                 src.generate());
}

vk::UniqueShaderModule ShaderManager::compileMaterialFragmentShader() {
  VulkanSource src;
  src.addSource(loadShaderSource("shaders/vulkan_material.frag"));
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
                                 src.generate());
}



vk::UniqueShaderModule ShaderManager::compileGBuffer3DResolveFragmentShader() {
  VulkanSource src;
  src.addSource(loadShaderSource("shaders/vulkan_gbuffer_3d_resolve.frag"));
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
                                 src.generate());
}

vk::UniqueShaderModule ShaderManager::compileGBufferHUDOverlayFragmentShader() {
  VulkanSource src;
  src.addSource(loadShaderSource("shaders/vulkan_gbuffer_hud_overlay.frag"));
  return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
                                 src.generate());
}
