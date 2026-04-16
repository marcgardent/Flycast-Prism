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
#include "vulkan.h"
#include "shaders.h"
#include "compiler.h"
#include "utils.h"

static const char VertexShaderSource[] = 
#include "shaders/vulkan_main.vert"
;

static const char FragmentShaderTop[] = 
#include "shaders/vulkan_top.frag"
;

const char *FragmentShaderCommon = 
#include "shaders/vulkan_common.frag"
;

static const char FragmentShaderMain[] = 
#include "shaders/vulkan_main.frag"
;

extern const char ModVolVertexShaderSource[] = 
#include "shaders/vulkan_modvol.vert"
;

static const char ModVolFragmentShaderSource[] = 
#include "shaders/vulkan_modvol.frag"
;

static const char QuadVertexShaderSource[] = 
#include "shaders/vulkan_quad.vert"
;

static const char QuadFragmentShaderSource[] = 
#include "shaders/vulkan_quad.frag"
;

static const char SSAOFragmentShaderSource[] = 
#include "shaders/vulkan_ssao.frag"
;

static const char DoFFragmentShaderSource[] = 
#include "shaders/vulkan_dof.frag"
;

static const char MaterialFragmentShaderSource[] = 
#include "shaders/vulkan_material.frag"
;

static const char HUDCompositeFragmentShaderSource[] = 
#include "shaders/vulkan_hud_composite.frag"
;

extern const char N2LightShaderSource[] = 
#include "shaders/vulkan_n2_light.glsl"
;

static const char N2VertexShaderSource[] = 
#include "shaders/vulkan_n2.vert"
;

extern const char N2ModVolVertexShaderSource[] = 
#include "shaders/vulkan_n2_modvol.vert"
;

vk::UniqueShaderModule ShaderManager::compileShader(const VertexShaderParams& params)
{
	VulkanSource src;
	if (!params.naomi2)
	{
		src.addConstant("pp_Gouraud", (int)params.gouraud)
				.addConstant("DIV_POS_Z", (int)params.divPosZ)
				.addSource(GouraudSource)
				.addSource(VertexShaderSource);
	}
	else
	{
		src.addConstant("pp_Gouraud", (int)params.gouraud)
				.addConstant("pp_Texture", (int)params.texture)
				.addSource(GouraudSource)
				.addSource(N2LightShaderSource)
				.addSource(N2VertexShaderSource);
	}
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileShader(const FragmentShaderParams& params)
{
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
		.addConstant("EnableSSAO", (int)params.enableSSAO)
		.addConstant("ShowSSAO", (int)params.showSSAO)
		.addSource(GouraudSource)
		.addSource(FragmentShaderTop)
		.addSource(FragmentShaderCommon)
		.addSource(FragmentShaderMain);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileShader(const ModVolShaderParams& params)
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex,
			VulkanSource().addConstant("DIV_POS_Z", (int)params.divPosZ)
				.addSource(params.naomi2 ? N2ModVolVertexShaderSource : ModVolVertexShaderSource).generate());
}

vk::UniqueShaderModule ShaderManager::compileModVolFragmentShader(bool divPosZ)
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
			VulkanSource().addConstant("DIV_POS_Z", (int)divPosZ)
				.addSource(ModVolFragmentShaderSource).generate());
}

vk::UniqueShaderModule ShaderManager::compileQuadVertexShader(bool rotate)
{
	VulkanSource src;
	src.addConstant("ROTATE", (int)rotate)
			.addSource(QuadVertexShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileQuadFragmentShader(bool ignoreTexAlpha)
{
	VulkanSource src;
	src.addConstant("IGNORE_TEX_ALPHA", (int)ignoreTexAlpha)
			.addSource(QuadFragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,src.generate());
}

vk::UniqueShaderModule ShaderManager::compileSSAOFragmentShader()
{
	VulkanSource src;
	src.addSource(SSAOFragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileDoFFragmentShader()
{
	VulkanSource src;
	src.addSource(DoFFragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileMaterialFragmentShader()
{
	VulkanSource src;
	src.addSource(MaterialFragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileHUDFragmentShader()
{
	VulkanSource src;
	src.addSource(HUDCompositeFragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}
