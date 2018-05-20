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

#define SKIP_IF(args, id) do { if ((*args)->name == #id) args++; } while(0)
#define GET_U32_EXPLICIT(args) uint32_t((*args++)->data.basic.u)
#define GET_SIZE_EXPLICIT(args) VkDeviceSize((*args++)->data.basic.u)
#define GET_F32_EXPLICIT(args) float((*args++)->data.basic.d)
#define GET_ENUM_EXPLICIT(args, T) static_cast<T>((*args++)->data.basic.u)
#define GET_HANDLE_EXPLICIT(args, T) (T)((*args++)->data.basic.u)
#define GET_ARRAY_EXPLICIT(args) (*args++)->data.children.data()

#define GET_U32() uint32_t((*args++)->data.basic.u)
#define GET_SIZE() VkDeviceSize((*args++)->data.basic.u)
#define GET_F32() float((*args++)->data.basic.d)
#define GET_ENUM(T) static_cast<T>((*args++)->data.basic.u)
#define GET_HANDLE(T) (T)((*args++)->data.basic.u)
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

static void serialise_descriptor_set_binding(StateRecorder &recorder,
                                             VkDescriptorSetLayoutBinding &binding,
                                             const SDObject *arg)
{
	const SDObject * const *args = arg->data.children.data();
	binding.binding = GET_U32();
	binding.descriptorType = GET_ENUM(VkDescriptorType);
	binding.descriptorCount = GET_U32();
	binding.stageFlags = GET_U32();

	const SDObject *immutable = *args;
	VkSampler *immutable_samplers = recorder.get_allocator().allocate_n<VkSampler>(binding.descriptorCount);
	if (!immutable->data.children.empty())
	{
		binding.pImmutableSamplers = immutable_samplers;
		const SDObject * const *handles = immutable->data.children.data();
		for (uint32_t i = 0; i < binding.descriptorCount; i++)
			immutable_samplers[i] = (VkSampler)handles[i]->data.basic.u;
	}
	else
		binding.pImmutableSamplers = NULL;
}

static bool serialise_descriptor_set_bindings(StateRecorder &recorder,
                                              VkDescriptorSetLayoutBinding *binding, size_t count,
                                              const SDObject *arg)
{
	const SDObject * const *args = arg->data.children.data();
	for (size_t i = 0; i < count; i++)
		serialise_descriptor_set_binding(recorder, binding[i], *args++);

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

	VkDescriptorSetLayoutBinding *bindings =
			recorder.get_allocator().allocate_n<VkDescriptorSetLayoutBinding>(info.bindingCount);
	if (info.bindingCount)
	{
		info.pBindings = bindings;
		serialise_descriptor_set_bindings(recorder, bindings, info.bindingCount, *args++);
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

	VkDescriptorSetLayout *layouts = recorder.get_allocator().allocate_n<VkDescriptorSetLayout>(info.setLayoutCount);
	const SDObject * const *set_layouts = GET_ARRAY();
	if (info.setLayoutCount)
	{
		info.pSetLayouts = layouts;
		for (uint32_t i = 0; i < info.setLayoutCount; i++)
			layouts[i] = (VkDescriptorSetLayout)set_layouts[i]->data.basic.u;
	}
	else
		info.pSetLayouts = NULL;

	info.pushConstantRangeCount = GET_U32();
	const SDObject * const *ranges = GET_ARRAY();
	VkPushConstantRange *push_ranges = recorder.get_allocator().allocate_n<VkPushConstantRange>(info.pushConstantRangeCount);
	if (info.pushConstantRangeCount)
	{
		info.pPushConstantRanges = push_ranges;
		for (uint32_t i = 0; i < info.pushConstantRangeCount; i++)
		{
			VkPushConstantRange &r = push_ranges[i];
			const SDObject * const *range = ranges[i]->data.children.data();
			r.stageFlags = GET_U32_EXPLICIT(range);
			r.offset = GET_U32_EXPLICIT(range);
			r.size = GET_U32_EXPLICIT(range);
		}
	}

	unsigned index = recorder.register_pipeline_layout(
			Hashing::compute_hash_pipeline_layout(recorder, info), info);
	recorder.set_pipeline_layout_handle(index, (VkPipelineLayout)id->data.basic.u);
	return true;
}

static bool serialise_shader_module(StateRecorder &recorder, const StructuredBufferList &buffers,
                                    const SDObject *create_info, const SDObject *id)
{
	const SDObject * const *args = create_info->data.children.data();
	VkShaderModuleCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if ((*args++)->type.basetype != SDBasic::Null)
		return false;
	info.flags = GET_U32();
	info.codeSize = GET_SIZE();

	uint32_t buffer_index = GET_U32();
	info.pCode = reinterpret_cast<const uint32_t *>(buffers[buffer_index]->data());
	if (buffers[buffer_index]->size() != info.codeSize)
		return false;

	unsigned index = recorder.register_shader_module(
			Hashing::compute_hash_shader_module(recorder, info), info);
	recorder.set_shader_module_handle(index, (VkShaderModule)id->data.basic.u);
	return true;
}

static VkSpecializationInfo *clone_spec_info(StateRecorder &recorder,
                                             const StructuredBufferList &buffers,
                                             const SDObject *spec_info)
{
	VkSpecializationInfo *info = recorder.get_allocator().allocate_cleared<VkSpecializationInfo>();

	const SDObject * const *args = spec_info->data.children.data();
	info->mapEntryCount = GET_U32();
	if (info->mapEntryCount)
	{
		VkSpecializationMapEntry *map_entries =
				recorder.get_allocator().allocate_n_cleared<VkSpecializationMapEntry>(info->mapEntryCount);
		info->pMapEntries = map_entries;

		const SDObject * const *entries = GET_ARRAY();
		for (uint32_t i = 0; i < info->mapEntryCount; i++)
		{
			const SDObject * const *map_entry = entries[i]->data.children.data();

			// XXX: RenderDoc copy-paste bug in serialisation, but works fine.
			map_entries[i].constantID = GET_U32_EXPLICIT(map_entry);
			map_entries[i].offset = GET_U32_EXPLICIT(map_entry);
			SKIP_IF(map_entry, constantID);
			map_entries[i].size = GET_U32_EXPLICIT(map_entry);
		}

		info->dataSize = GET_U32();
		uint32_t index = GET_U32();
		if (buffers[index]->size() != info->dataSize)
			return nullptr;
		info->pData = buffers[index]->data();
	}

	return info;
}

static bool serialise_compute_pipeline(StateRecorder &recorder, const StructuredBufferList &buffers,
                                       const SDObject *create_info, const SDObject *id)
{
	const SDObject * const *args = create_info->data.children.data();

	VkComputePipelineCreateInfo info = {};
	info.sType = GET_ENUM(VkStructureType);
	if ((*args++)->type.basetype != SDBasic::Null)
		return false;
	info.flags = GET_U32();

	{
		const SDObject * const *stage = GET_ARRAY();
		info.stage.sType = GET_ENUM_EXPLICIT(stage, VkStructureType);
		if ((*stage++)->type.basetype != SDBasic::Null)
			return false;
		info.stage.flags = GET_U32_EXPLICIT(stage);
		info.stage.stage = GET_ENUM_EXPLICIT(stage, VkShaderStageFlagBits);
		info.stage.module = GET_HANDLE_EXPLICIT(stage, VkShaderModule);
		info.stage.pName = (*stage++)->data.str.c_str();

		if ((*stage)->type.basetype != SDBasic::Null)
		{
			info.stage.pSpecializationInfo = clone_spec_info(recorder, buffers, *stage);
			if (!info.stage.pSpecializationInfo)
				return false;
		}
	}

	info.layout = GET_HANDLE(VkPipelineLayout);
	info.basePipelineHandle = GET_HANDLE(VkPipeline);
	info.basePipelineIndex = GET_U32();

	unsigned index = recorder.register_compute_pipeline(
			Hashing::compute_hash_compute_pipeline(recorder, info), info);
	recorder.set_compute_pipeline_handle(index, (VkPipeline)id->data.basic.u);
	return true;
}

static bool serialise_render_pass(StateRecorder &recorder, const SDObject *create_info,
                                  const SDObject *id)
{
	const SDObject * const *args = create_info->data.children.data();
	VkRenderPassCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if ((*args++)->type.basetype != SDBasic::Null)
		return false;
	info.flags = GET_U32();
	info.attachmentCount = GET_U32();
	const SDObject * const *att = GET_ARRAY();
	info.subpassCount = GET_U32();
	const SDObject * const *sub = GET_ARRAY();
	info.dependencyCount = GET_U32();
	const SDObject * const *dep = GET_ARRAY();

	if (info.attachmentCount)
	{
		VkAttachmentDescription *attachments =
				recorder.get_allocator().allocate_n_cleared<VkAttachmentDescription>(info.attachmentCount);
		info.pAttachments = attachments;
		for (uint32_t i = 0; i < info.attachmentCount; i++)
		{
			const SDObject * const *a = att[i]->data.children.data();
			attachments[i].flags = GET_U32_EXPLICIT(a);
			attachments[i].format = GET_ENUM_EXPLICIT(a, VkFormat);
			attachments[i].samples = GET_ENUM_EXPLICIT(a, VkSampleCountFlagBits);
			attachments[i].loadOp = GET_ENUM_EXPLICIT(a, VkAttachmentLoadOp);
			attachments[i].storeOp = GET_ENUM_EXPLICIT(a, VkAttachmentStoreOp);
			attachments[i].stencilLoadOp = GET_ENUM_EXPLICIT(a, VkAttachmentLoadOp);
			attachments[i].stencilStoreOp = GET_ENUM_EXPLICIT(a, VkAttachmentStoreOp);
			attachments[i].initialLayout = GET_ENUM_EXPLICIT(a, VkImageLayout);
			attachments[i].finalLayout = GET_ENUM_EXPLICIT(a, VkImageLayout);
		}
	}

	if (info.subpassCount)
	{
		VkSubpassDescription *subpasses =
				recorder.get_allocator().allocate_n_cleared<VkSubpassDescription>(info.subpassCount);
		info.pSubpasses = subpasses;

		for (uint32_t i = 0; i < info.subpassCount; i++)
		{
			const SDObject * const *s = sub[i]->data.children.data();
			subpasses[i].flags = GET_U32_EXPLICIT(s);
			subpasses[i].pipelineBindPoint = GET_ENUM_EXPLICIT(s, VkPipelineBindPoint);

			subpasses[i].inputAttachmentCount = GET_U32_EXPLICIT(s);
			const SDObject * const *inputs = GET_ARRAY_EXPLICIT(s);
			subpasses[i].colorAttachmentCount = GET_U32_EXPLICIT(s);
			const SDObject * const *colors = GET_ARRAY_EXPLICIT(s);

			if ((*s)->data.children.size() != 0)
			{
				const SDObject * const *res = (*s)->data.children.data();
				VkAttachmentReference *resolves =
						recorder.get_allocator().allocate_n<VkAttachmentReference>(subpasses[i].colorAttachmentCount);
				subpasses[i].pResolveAttachments = resolves;

				for (uint32_t j = 0; j < subpasses[i].colorAttachmentCount; j++)
				{
					const SDObject * const *r = GET_ARRAY_EXPLICIT(res);
					resolves[j].attachment = GET_U32_EXPLICIT(r);
					resolves[j].layout = GET_ENUM_EXPLICIT(r, VkImageLayout);
				}
			}
			s++;

			if ((*s)->type.basetype != SDBasic::Null)
			{
				const SDObject * const *depth_stencil = (*s)->data.children.data();
				VkAttachmentReference *ds =
						recorder.get_allocator().allocate_cleared<VkAttachmentReference>();
				subpasses[i].pDepthStencilAttachment = ds;

				ds->attachment = GET_U32_EXPLICIT(depth_stencil);
				ds->layout = GET_ENUM_EXPLICIT(depth_stencil, VkImageLayout);
			}
			s++;

			subpasses[i].preserveAttachmentCount = GET_U32_EXPLICIT(s);
			const SDObject * const *preserves = GET_ARRAY_EXPLICIT(s);

			if (subpasses[i].inputAttachmentCount)
			{
				VkAttachmentReference *att =
						recorder.get_allocator().allocate_n<VkAttachmentReference>(subpasses[i].inputAttachmentCount);
				subpasses[i].pInputAttachments = att;
				for (uint32_t j = 0; j < subpasses[i].inputAttachmentCount; j++)
				{
					const SDObject * const *i = GET_ARRAY_EXPLICIT(inputs);
					att[j].attachment = GET_U32_EXPLICIT(i);
					att[j].layout = GET_ENUM_EXPLICIT(i, VkImageLayout);
				}
			}

			if (subpasses[i].colorAttachmentCount)
			{
				VkAttachmentReference *att =
						recorder.get_allocator().allocate_n<VkAttachmentReference>(subpasses[i].colorAttachmentCount);
				subpasses[i].pColorAttachments = att;
				for (uint32_t j = 0; j < subpasses[i].colorAttachmentCount; j++)
				{
					const SDObject * const *c = GET_ARRAY_EXPLICIT(colors);
					att[j].attachment = GET_U32_EXPLICIT(c);
					att[j].layout = GET_ENUM_EXPLICIT(c, VkImageLayout);
				}
			}

			if (subpasses[i].preserveAttachmentCount)
			{
				uint32_t *att =
						recorder.get_allocator().allocate_n<uint32_t>(subpasses[i].preserveAttachmentCount);
				subpasses[i].pPreserveAttachments = att;
				for (uint32_t j = 0; j < subpasses[i].preserveAttachmentCount; j++)
					att[j] = GET_U32_EXPLICIT(preserves);
			}
		}
	}

	if (info.dependencyCount)
	{
		VkSubpassDependency *deps =
				recorder.get_allocator().allocate_n_cleared<VkSubpassDependency>(info.dependencyCount);
		info.pDependencies = deps;

		for (uint32_t i = 0; i < info.dependencyCount; i++)
		{
			const SDObject * const *d = dep[i]->data.children.data();
			deps[i].srcSubpass = GET_U32_EXPLICIT(d);
			deps[i].dstSubpass = GET_U32_EXPLICIT(d);
			deps[i].srcStageMask = GET_U32_EXPLICIT(d);
			deps[i].dstStageMask = GET_U32_EXPLICIT(d);
			deps[i].srcAccessMask = GET_U32_EXPLICIT(d);
			deps[i].dstAccessMask = GET_U32_EXPLICIT(d);
			deps[i].dependencyFlags = GET_U32_EXPLICIT(d);
		}
	}

	unsigned index = recorder.register_render_pass(
			Hashing::compute_hash_render_pass(recorder, info), info);
	recorder.set_render_pass_handle(index, (VkRenderPass)id->data.basic.u);
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
		else if (chunk->name == "vkCreateShaderModule")
		{
			if (!serialise_shader_module(recorder, structData.buffers,
			                             chunk->data.children[1],
			                             chunk->data.children[3]))
				return ReplayStatus::APIIncompatibleVersion;
		}
		else if (chunk->name == "vkCreateComputePipelines")
		{
			if (!serialise_compute_pipeline(recorder, structData.buffers,
			                                chunk->data.children[3],
			                                chunk->data.children[5]))
				return ReplayStatus::APIIncompatibleVersion;
		}
		else if (chunk->name == "vkCreateRenderPass")
		{
			if (!serialise_render_pass(recorder, chunk->data.children[1],
			                           chunk->data.children[3]))
				return ReplayStatus::APIIncompatibleVersion;
		}
	}

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
        true,
    });
