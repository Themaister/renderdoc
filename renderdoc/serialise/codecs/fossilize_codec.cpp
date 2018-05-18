/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
 * Copyright (c) 2018 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <utility>
#include "common/common.h"
#include "serialise/rdcfile.h"
#include "fossilize/fossilize.hpp"

using namespace Fossilize;

#define GET_U32() uint32_t((*args++)->data.basic.u)
#define GET_F32() float((*args++)->data.basic.d)
#define GET_ENUM(T) static_cast<T>((*args++)->data.basic.u)
#define GET_HANDLE(t) (T)((*args++)->data.basic.u)
#define GET_ARRAY() (*args++)->data.children.data()

static bool serialise_sampler(StateRecorder &recorder, const SDObject *create_info, const SDObject *id)
{
	const SDObject * const *args = create_info->data.children.data();
	VkSamplerCreateInfo info = {};
	info.sType = GET_ENUM(VkStructureType);
	if ((*args++)->type.basetype != SDBasic::Null)
		return false;
	info.flags = GET_U32();
	info.magFilter = GET_ENUM(VkFilter);
	info.minFilter = GET_ENUM(VkFilter);
	info.mipmapMode = GET_ENUM(VkSamplerMipmapMode);
	info.addressModeU = GET_ENUM(VkSamplerAddressMode);
	info.addressModeV = GET_ENUM(VkSamplerAddressMode);
	info.addressModeW = GET_ENUM(VkSamplerAddressMode);
	info.mipLodBias = GET_F32();
	info.anisotropyEnable = GET_U32();
	info.maxAnisotropy = GET_F32();
	info.compareEnable = GET_U32();
	info.compareOp = GET_ENUM(VkCompareOp);
	info.minLod = GET_F32();
	info.maxLod = GET_F32();
	info.borderColor = GET_ENUM(VkBorderColor);
	info.unnormalizedCoordinates = GET_U32();

	unsigned index = recorder.register_sampler(Hashing::compute_hash_sampler(recorder, info), info);
	recorder.set_sampler_handle(index, (VkSampler)id->data.basic.u);

	return true;
}

static void serialise_descriptor_set_binding(VkDescriptorSetLayoutBinding &binding,
                                             const SDObject *arg)
{
	const SDObject * const *args = arg->data.children.data();
	binding.binding = GET_U32();
	binding.descriptorType = GET_ENUM(VkDescriptorType);
	binding.descriptorCount = GET_U32();
	binding.stageFlags = GET_U32();

	const SDObject *immutable = *args;
	vector<VkSampler> immutable_samplers(binding.descriptorCount);
	if (!immutable->data.children.empty())
	{
		binding.pImmutableSamplers = immutable_samplers.data();
		const SDObject * const *handles = immutable->data.children.data();
		for (uint32_t i = 0; i < binding.descriptorCount; i++)
			immutable_samplers[i] = (VkSampler)handles[i]->data.basic.u;
	}
	else
		binding.pImmutableSamplers = NULL;
}

static bool serialise_descriptor_set_bindings(VkDescriptorSetLayoutBinding *binding, size_t count,
                                              const SDObject *arg)
{
	const SDObject * const *args = arg->data.children.data();
	for (size_t i = 0; i < count; i++)
		serialise_descriptor_set_binding(binding[i], *args++);

	return true;
}

static bool serialise_descriptor_set_layout(StateRecorder &recorder, const SDObject *create_info,
                                            const SDObject *id)
{
	const SDObject * const *args = create_info->data.children.data();
	VkDescriptorSetLayoutCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if ((*args++)->type.basetype != SDBasic::Null)
		return false;
	info.flags = GET_U32();
	info.bindingCount = GET_U32();

	vector<VkDescriptorSetLayoutBinding> bindings(info.bindingCount);
	if (info.bindingCount)
	{
		info.pBindings = bindings.data();
		serialise_descriptor_set_bindings(bindings.data(), bindings.size(), *args++);
	}

	unsigned index = recorder.register_descriptor_set_layout(
			Hashing::compute_hash_descriptor_set_layout(recorder, info), info);
	recorder.set_descriptor_set_layout_handle(index, (VkDescriptorSetLayout)id->data.basic.u);
	return true;
}

static bool serialise_pipeline_layout(StateRecorder &recorder, const SDObject *create_info,
                                      const SDObject *id)
{
	const SDObject * const *args = create_info->data.children.data();
	VkPipelineLayoutCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if ((*args++)->type.basetype != SDBasic::Null)
		return false;
	info.flags = GET_U32();
	info.setLayoutCount = GET_U32();

	vector<VkDescriptorSetLayout> layouts(info.setLayoutCount);
	const SDObject * const *set_layouts = GET_ARRAY();
	if (info.setLayoutCount)
	{
		info.pSetLayouts = layouts.data();
		for (uint32_t i = 0; i < info.setLayoutCount; i++)
			layouts[i] = (VkDescriptorSetLayout)set_layouts[i]->data.basic.u;
	}
	else
		info.pSetLayouts = NULL;

	info.pushConstantRangeCount = GET_U32();
	const SDObject * const *ranges = GET_ARRAY();
	vector<VkPushConstantRange> push_ranges(info.pushConstantRangeCount);
	if (info.pushConstantRangeCount)
	{
		info.pPushConstantRanges = push_ranges.data();
		for (uint32_t i = 0; i < info.pushConstantRangeCount; i++)
		{
			VkPushConstantRange &r = push_ranges[i];
			const SDObject * const *args = ranges[i]->data.children.data();
			r.stageFlags = GET_U32();
			r.offset = GET_U32();
			r.size = GET_U32();
		}
	}

	unsigned index = recorder.register_pipeline_layout(
			Hashing::compute_hash_pipeline_layout(recorder, info), info);
	recorder.set_pipeline_layout_handle(index, (VkPipelineLayout)id->data.basic.u);
	return true;
}

ReplayStatus export_fossilize(const char *filename, const RDCFile &rdc, const SDFile &structData,
                              RENDERDOC_ProgressCallback progress)
{
	if (rdc.GetDriver() != RDCDriver::Vulkan)
		return ReplayStatus::APIIncompatibleVersion;
	StateRecorder recorder;

	for (SDChunk *chunk : structData.chunks)
	{
		if (chunk->name == "vkCreateSampler")
		{
			if (!serialise_sampler(recorder, chunk->data.children[1], chunk->data.children[3]))
				return ReplayStatus::APIIncompatibleVersion;
		}
		else if (chunk->name == "vkCreateDescriptorSetLayout")
		{
			if (!serialise_descriptor_set_layout(recorder,
			                                     chunk->data.children[1],
			                                     chunk->data.children[3]))
				return ReplayStatus::APIIncompatibleVersion;
		}
		else if (chunk->name == "vkCreatePipelineLayout")
		{
			if (!serialise_pipeline_layout(recorder,
			                               chunk->data.children[1],
			                               chunk->data.children[3]))
				return ReplayStatus::APIIncompatibleVersion;
		}
	}

	if (progress)
		progress(0.1f);

	vector<uint8_t> serialized = recorder.serialize();

	FILE *file = FileIO::fopen(filename, "wb");
	if (!file)
		return ReplayStatus::FileIOFailed;

	FileIO::fwrite(serialized.data(), 1, serialized.size(), file);
	FileIO::fclose(file);

	if (progress)
		progress(1.0f);
	return ReplayStatus::Succeeded;
}

static ConversionRegistration FossilizeConversionRegistration(
    &export_fossilize,
    {
        "fossilize.json", "Fossilize state exporter",
        R"(Exports Vulkan state for various persistent objects.)",
        false,
    });
