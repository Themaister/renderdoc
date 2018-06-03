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

static inline SDObject * const *get_array_or_null(const SDObject *arg)
{
	if (arg->type.basetype == SDBasic::Null)
		return nullptr;
	else if (arg->data.children.empty())
		return nullptr;
	else
		return arg->begin();
}

#define SKIP_IF(args, id) do { if ((*args)->name == #id) args++; } while(0)
#define GET_U32_EXPLICIT(args) (*args++)->AsUInt32()
#define GET_SIZE_EXPLICIT(args) VkDeviceSize((*args++)->AsUInt64())
#define GET_F32_EXPLICIT(args) (*args++)->AsFloat()
#define GET_ENUM_EXPLICIT(args, T) (*args++)->AsEnum<T>()
#define GET_HANDLE_EXPLICIT(args, T) (T)((*args++)->AsUInt64())
#define GET_ARRAY_EXPLICIT(args) (*args++)->begin()
#define GET_NULLABLE_ARRAY_EXPLICIT(args) get_array_or_null(*args++)

#define GET_U32() (*args++)->AsUInt32()
#define GET_SIZE() VkDeviceSize((*args++)->AsUInt64())
#define GET_F32() (*args++)->AsFloat()
#define GET_ENUM(T) (*args++)->AsEnum<T>()
#define GET_HANDLE(T) (T)((*args++)->AsUInt64())
#define GET_ARRAY() (*args++)->begin()
#define GET_NULLABLE_ARRAY() get_array_or_null(*args++)

static bool serialise_sampler(StateRecorder &recorder, const SDObject *create_info, const SDObject *id)
{
	const SDObject * const *args = create_info->begin();
	VkSamplerCreateInfo info = {};
	info.sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
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
	recorder.set_sampler_handle(index, (VkSampler)id->AsUInt64());

	return true;
}

static void serialise_descriptor_set_binding(StateRecorder &recorder,
                                             VkDescriptorSetLayoutBinding &binding,
                                             const SDObject *arg)
{
	const SDObject * const *args = arg->begin();
	binding.binding = GET_U32();
	binding.descriptorType = GET_ENUM(VkDescriptorType);
	binding.descriptorCount = GET_U32();
	binding.stageFlags = GET_U32();

	const SDObject *immutable = *args;
	VkSampler *immutable_samplers = recorder.get_allocator().allocate_n<VkSampler>(binding.descriptorCount);
	if (!immutable->data.children.empty())
	{
		binding.pImmutableSamplers = immutable_samplers;
		const SDObject * const *handles = immutable->begin();
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
	const SDObject * const *args = arg->begin();
	for (size_t i = 0; i < count; i++)
		serialise_descriptor_set_binding(recorder, binding[i], *args++);

	return true;
}

static bool serialise_descriptor_set_layout(StateRecorder &recorder, const SDObject *create_info,
                                            const SDObject *id)
{
	const SDObject * const *args = create_info->begin();
	VkDescriptorSetLayoutCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
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
	recorder.set_descriptor_set_layout_handle(index, (VkDescriptorSetLayout)id->AsUInt64());
	return true;
}

static bool serialise_pipeline_layout(StateRecorder &recorder, const SDObject *create_info,
                                      const SDObject *id)
{
	const SDObject * const *args = create_info->begin();
	VkPipelineLayoutCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
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
			const SDObject * const *range = ranges[i]->begin();
			r.stageFlags = GET_U32_EXPLICIT(range);
			r.offset = GET_U32_EXPLICIT(range);
			r.size = GET_U32_EXPLICIT(range);
		}
	}

	unsigned index = recorder.register_pipeline_layout(
			Hashing::compute_hash_pipeline_layout(recorder, info), info);
	recorder.set_pipeline_layout_handle(index, (VkPipelineLayout)id->AsUInt64());
	return true;
}

static bool serialise_shader_module(StateRecorder &recorder, const StructuredBufferList &buffers,
                                    const SDObject *create_info, const SDObject *id)
{
	const SDObject * const *args = create_info->begin();
	VkShaderModuleCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return false;
	info.flags = GET_U32();
	info.codeSize = GET_SIZE();

	uint32_t buffer_index = GET_U32();
	info.pCode = reinterpret_cast<const uint32_t *>(buffers[buffer_index]->data());
	if (buffers[buffer_index]->size() != info.codeSize)
		return false;

	unsigned index = recorder.register_shader_module(
			Hashing::compute_hash_shader_module(recorder, info), info);
	recorder.set_shader_module_handle(index, (VkShaderModule)id->AsUInt64());
	return true;
}

static VkSpecializationInfo *clone_spec_info(StateRecorder &recorder,
                                             const StructuredBufferList &buffers,
                                             const SDObject *spec_info)
{
	VkSpecializationInfo *info = recorder.get_allocator().allocate_cleared<VkSpecializationInfo>();

	const SDObject * const *args = spec_info->begin();
	info->mapEntryCount = GET_U32();
	if (info->mapEntryCount)
	{
		VkSpecializationMapEntry *map_entries =
				recorder.get_allocator().allocate_n_cleared<VkSpecializationMapEntry>(info->mapEntryCount);
		info->pMapEntries = map_entries;

		const SDObject * const *entries = GET_ARRAY();
		for (uint32_t i = 0; i < info->mapEntryCount; i++)
		{
			const SDObject * const *map_entry = entries[i]->begin();

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
	const SDObject * const *args = create_info->begin();

	VkComputePipelineCreateInfo info = {};
	info.sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return false;
	info.flags = GET_U32();

	{
		const SDObject * const *stage = GET_ARRAY();
		info.stage.sType = GET_ENUM_EXPLICIT(stage, VkStructureType);
		if (GET_NULLABLE_ARRAY_EXPLICIT(stage))
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
	recorder.set_compute_pipeline_handle(index, (VkPipeline)id->AsUInt64());
	return true;
}

static bool serialise_render_pass(StateRecorder &recorder, const SDObject *create_info,
                                  const SDObject *id)
{
	const SDObject * const *args = create_info->begin();
	VkRenderPassCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
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
			const SDObject * const *a = att[i]->begin();
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
			const SDObject * const *s = sub[i]->begin();
			subpasses[i].flags = GET_U32_EXPLICIT(s);
			subpasses[i].pipelineBindPoint = GET_ENUM_EXPLICIT(s, VkPipelineBindPoint);

			subpasses[i].inputAttachmentCount = GET_U32_EXPLICIT(s);
			const SDObject * const *inputs = GET_ARRAY_EXPLICIT(s);
			subpasses[i].colorAttachmentCount = GET_U32_EXPLICIT(s);
			const SDObject * const *colors = GET_ARRAY_EXPLICIT(s);

			if ((*s)->data.children.size() != 0)
			{
				const SDObject * const *res = (*s)->begin();
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
				const SDObject * const *depth_stencil = (*s)->begin();
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
			const SDObject * const *d = dep[i]->begin();
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
	recorder.set_render_pass_handle(index, (VkRenderPass)id->AsUInt64());
	return true;
}

static VkPipelineShaderStageCreateInfo *parse_shader_stages(StateRecorder &recorder,
                                                            const StructuredBufferList &buffers,
                                                            const SDObject * const *stages,
                                                            uint32_t stage_count)
{
	VkPipelineShaderStageCreateInfo *infos =
			recorder.get_allocator().allocate_n_cleared<VkPipelineShaderStageCreateInfo>(stage_count);

	for (uint32_t i = 0; i < stage_count; i++)
	{
		const SDObject *const *args = stages[i]->begin();
		VkPipelineShaderStageCreateInfo &info = infos[i];

		info.sType = GET_ENUM(VkStructureType);
		if (GET_NULLABLE_ARRAY())
			return nullptr;
		info.flags = GET_U32();
		info.stage = GET_ENUM(VkShaderStageFlagBits);
		info.module = GET_HANDLE(VkShaderModule);
		info.pName = (*args++)->data.str.c_str();

		if ((*args)->type.basetype != SDBasic::Null)
		{
			info.pSpecializationInfo = clone_spec_info(recorder, buffers, *args);
			if (!info.pSpecializationInfo)
				return nullptr;
		}
	}

	return infos;
}

static VkPipelineVertexInputStateCreateInfo *parse_vertex_input(StateRecorder &recorder,
                                                                const SDObject * const *args)
{
	VkPipelineVertexInputStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineVertexInputStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();
	info->vertexBindingDescriptionCount = GET_U32();
	const SDObject * const *bindings = GET_ARRAY();
	info->vertexAttributeDescriptionCount = GET_U32();
	const SDObject * const *attribs = GET_ARRAY();

	if (info->vertexBindingDescriptionCount)
	{
		VkVertexInputBindingDescription *binds =
				recorder.get_allocator().allocate_n_cleared<VkVertexInputBindingDescription>(info->vertexBindingDescriptionCount);
		info->pVertexBindingDescriptions = binds;

		for (uint32_t i = 0; i < info->vertexBindingDescriptionCount; i++)
		{
			const SDObject * const *bind = bindings[i]->begin();
			binds[i].binding = GET_U32_EXPLICIT(bind);
			binds[i].stride = GET_U32_EXPLICIT(bind);
			binds[i].inputRate = GET_ENUM_EXPLICIT(bind, VkVertexInputRate);
		}
	}

	if (info->vertexAttributeDescriptionCount)
	{
		VkVertexInputAttributeDescription *attrs =
				recorder.get_allocator().allocate_n_cleared<VkVertexInputAttributeDescription>(info->vertexAttributeDescriptionCount);
		info->pVertexAttributeDescriptions = attrs;

		for (uint32_t i = 0; i < info->vertexAttributeDescriptionCount; i++)
		{
			const SDObject * const *attr = attribs[i]->begin();
			attrs[i].location = GET_U32_EXPLICIT(attr);
			attrs[i].binding = GET_U32_EXPLICIT(attr);
			attrs[i].format = GET_ENUM_EXPLICIT(attr, VkFormat);
			attrs[i].offset = GET_U32_EXPLICIT(attr);
		}
	}

	return info;
}

static VkPipelineInputAssemblyStateCreateInfo *parse_input_assembly(StateRecorder &recorder,
                                                                    const SDObject * const *args)
{
	VkPipelineInputAssemblyStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineInputAssemblyStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();
	info->topology = GET_ENUM(VkPrimitiveTopology);
	info->primitiveRestartEnable = GET_U32();

	return info;
}

static VkPipelineTessellationStateCreateInfo *parse_tessellation_state(StateRecorder &recorder,
                                                                       const SDObject * const *args)
{
	VkPipelineTessellationStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineTessellationStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();
	info->patchControlPoints = GET_U32();

	return info;
}

static VkPipelineViewportStateCreateInfo *parse_viewport_state(StateRecorder &recorder,
                                                               const SDObject * const *args)
{
	VkPipelineViewportStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineViewportStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();
	info->viewportCount = GET_U32();
	const SDObject * const *viewports = GET_NULLABLE_ARRAY();
	info->scissorCount = GET_U32();
	const SDObject * const *scissors = GET_NULLABLE_ARRAY();

	if (info->viewportCount && viewports)
	{
		VkViewport *views = recorder.get_allocator().allocate_n_cleared<VkViewport>(info->viewportCount);
		info->pViewports = views;

		for (uint32_t i = 0; i < info->viewportCount; i++)
		{
			const SDObject * const *view = viewports[i]->begin();
			views[i].x = GET_F32_EXPLICIT(view);
			views[i].y = GET_F32_EXPLICIT(view);
			views[i].width = GET_F32_EXPLICIT(view);
			views[i].height = GET_F32_EXPLICIT(view);
			views[i].minDepth = GET_F32_EXPLICIT(view);
			views[i].maxDepth = GET_F32_EXPLICIT(view);
		}
	}

	if (info->scissorCount && scissors)
	{
		VkRect2D *sci = recorder.get_allocator().allocate_n_cleared<VkRect2D>(info->scissorCount);
		info->pScissors = sci;

		for (uint32_t i = 0; i < info->viewportCount; i++)
		{
			const SDObject * const *s = scissors[i]->begin();
			sci[i].offset.x = s[0]->begin()[0]->AsUInt32();
			sci[i].offset.y = s[0]->begin()[1]->AsUInt32();
			sci[i].extent.width = s[1]->begin()[0]->AsUInt32();
			sci[i].extent.height = s[1]->begin()[1]->AsUInt32();
		}
	}

	return info;
}

static VkPipelineRasterizationStateCreateInfo *parse_rasterization_state(StateRecorder &recorder,
                                                                         const SDObject * const *args)
{
	VkPipelineRasterizationStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineRasterizationStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();

	info->depthClampEnable = GET_U32();
	info->rasterizerDiscardEnable = GET_U32();
	info->polygonMode = GET_ENUM(VkPolygonMode);
	info->cullMode = GET_U32();
	info->frontFace = GET_ENUM(VkFrontFace);
	info->depthBiasEnable = GET_U32();
	info->depthBiasConstantFactor = GET_F32();
	info->depthBiasClamp = GET_F32();
	info->depthBiasSlopeFactor = GET_F32();
	info->lineWidth = GET_F32();

	return info;
}

static VkPipelineMultisampleStateCreateInfo *parse_multisample_state(StateRecorder &recorder,
                                                                     const SDObject * const *args)
{
	VkPipelineMultisampleStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineMultisampleStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();

	info->rasterizationSamples = GET_ENUM(VkSampleCountFlagBits);
	info->sampleShadingEnable = GET_U32();
	info->minSampleShading = GET_F32();
	const SDObject * const *sample_mask = GET_NULLABLE_ARRAY();
	if (sample_mask)
	{
		uint32_t count = (info->rasterizationSamples + 31) / 32;
		uint32_t *samples = recorder.get_allocator().allocate_n<uint32_t>(count);
		info->pSampleMask = samples;
		for (uint32_t i = 0; i < count; i++)
			samples[i] = sample_mask[i]->AsUInt32();
	}
	info->alphaToCoverageEnable = GET_U32();
	info->alphaToOneEnable = GET_U32();

	return info;
}

static VkPipelineDepthStencilStateCreateInfo *parse_depth_stencil_state(StateRecorder &recorder,
                                                                        const SDObject * const *args)
{
	VkPipelineDepthStencilStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineDepthStencilStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();
	info->depthTestEnable = GET_U32();
	info->depthWriteEnable = GET_U32();
	info->depthCompareOp = GET_ENUM(VkCompareOp);
	info->depthBoundsTestEnable = GET_U32();
	info->stencilTestEnable = GET_U32();
	const SDObject * const *front = GET_ARRAY();
	const SDObject * const *back = GET_ARRAY();

	info->front.failOp = GET_ENUM_EXPLICIT(front, VkStencilOp);
	info->front.passOp = GET_ENUM_EXPLICIT(front, VkStencilOp);
	info->front.depthFailOp = GET_ENUM_EXPLICIT(front, VkStencilOp);
	info->front.compareOp = GET_ENUM_EXPLICIT(front, VkCompareOp);
	info->front.compareMask = GET_U32_EXPLICIT(front);
	info->front.writeMask = GET_U32_EXPLICIT(front);
	info->front.reference = GET_U32_EXPLICIT(front);

	info->back.failOp = GET_ENUM_EXPLICIT(back, VkStencilOp);
	info->back.passOp = GET_ENUM_EXPLICIT(back, VkStencilOp);
	info->back.depthFailOp = GET_ENUM_EXPLICIT(back, VkStencilOp);
	info->back.compareOp = GET_ENUM_EXPLICIT(back, VkCompareOp);
	info->back.compareMask = GET_U32_EXPLICIT(back);
	info->back.writeMask = GET_U32_EXPLICIT(back);
	info->back.reference = GET_U32_EXPLICIT(back);

	info->minDepthBounds = GET_F32();
	info->maxDepthBounds = GET_F32();

	return info;
}

static VkPipelineColorBlendStateCreateInfo *parse_blend_state(StateRecorder &recorder,
                                                              const SDObject * const *args)
{
	VkPipelineColorBlendStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineColorBlendStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();
	info->logicOpEnable = GET_U32();
	info->logicOp = GET_ENUM(VkLogicOp);
	info->attachmentCount = GET_U32();
	const SDObject * const *atts = GET_NULLABLE_ARRAY();

	if (atts)
	{
		VkPipelineColorBlendAttachmentState *attachments =
				recorder.get_allocator().allocate_n_cleared<VkPipelineColorBlendAttachmentState>(info->attachmentCount);
		info->pAttachments = attachments;

		for (uint32_t i = 0; i < info->attachmentCount; i++)
		{
			const SDObject * const *a = atts[i]->begin();
			attachments[i].blendEnable = GET_U32_EXPLICIT(a);
			attachments[i].srcColorBlendFactor = GET_ENUM_EXPLICIT(a, VkBlendFactor);
			attachments[i].dstColorBlendFactor = GET_ENUM_EXPLICIT(a, VkBlendFactor);
			attachments[i].colorBlendOp = GET_ENUM_EXPLICIT(a, VkBlendOp);
			attachments[i].srcAlphaBlendFactor = GET_ENUM_EXPLICIT(a, VkBlendFactor);
			attachments[i].dstAlphaBlendFactor = GET_ENUM_EXPLICIT(a, VkBlendFactor);
			attachments[i].alphaBlendOp = GET_ENUM_EXPLICIT(a, VkBlendOp);
			attachments[i].colorWriteMask = GET_U32_EXPLICIT(a);
		}
	}

	const SDObject * const *constants = GET_ARRAY();
	for (uint32_t i = 0; i < 4; i++)
		info->blendConstants[i] = constants[i]->AsFloat();

	return info;
}

static VkPipelineDynamicStateCreateInfo *parse_dynamic_state(StateRecorder &recorder,
                                                             const SDObject * const *args)
{
	VkPipelineDynamicStateCreateInfo *info =
			recorder.get_allocator().allocate_cleared<VkPipelineDynamicStateCreateInfo>();

	info->sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return nullptr;
	info->flags = GET_U32();
	info->dynamicStateCount = GET_U32();
	const SDObject * const *dyn = GET_NULLABLE_ARRAY();
	if (info->dynamicStateCount)
	{
		VkDynamicState *states = recorder.get_allocator().allocate_n<VkDynamicState>(info->dynamicStateCount);
		info->pDynamicStates = states;
		for (uint32_t i = 0; i < info->dynamicStateCount; i++)
			states[i] = dyn[i]->AsEnum<VkDynamicState>();
	}

	return info;
}

static bool serialise_graphics_pipeline(StateRecorder &recorder, const StructuredBufferList &buffers,
                                        const SDObject *create_info, const SDObject *id)
{
	const SDObject * const *args = create_info->begin();
	VkGraphicsPipelineCreateInfo info = {};

	info.sType = GET_ENUM(VkStructureType);
	if (GET_NULLABLE_ARRAY())
		return false;
	info.flags = GET_U32();
	info.stageCount = GET_U32();
	const SDObject * const *stages = GET_ARRAY();
	const SDObject * const *vertex_input = GET_NULLABLE_ARRAY();
	const SDObject * const *input_assembly = GET_NULLABLE_ARRAY();
	const SDObject * const *tessellation = GET_NULLABLE_ARRAY();
	const SDObject * const *viewport = GET_NULLABLE_ARRAY();
	const SDObject * const *rasterization = GET_NULLABLE_ARRAY();
	const SDObject * const *multisample = GET_NULLABLE_ARRAY();
	const SDObject * const *depth_stencil = GET_NULLABLE_ARRAY();
	const SDObject * const *blend_state = GET_NULLABLE_ARRAY();
	const SDObject * const *dynamic_state = GET_NULLABLE_ARRAY();

	if (info.stageCount)
	{
		info.pStages = parse_shader_stages(recorder, buffers, stages, info.stageCount);
		if (!info.pStages)
			return false;
	}

	if (vertex_input)
	{
		info.pVertexInputState = parse_vertex_input(recorder, vertex_input);
		if (!info.pVertexInputState)
			return false;
	}

	if (input_assembly)
	{
		info.pInputAssemblyState = parse_input_assembly(recorder, input_assembly);
		if (!info.pInputAssemblyState)
			return false;
	}

	if (tessellation)
	{
		info.pTessellationState = parse_tessellation_state(recorder, tessellation);
		if (!info.pTessellationState)
			return false;
	}

	if (viewport)
	{
		info.pViewportState = parse_viewport_state(recorder, viewport);
		if (!info.pViewportState)
			return false;
	}

	if (rasterization)
	{
		info.pRasterizationState = parse_rasterization_state(recorder, rasterization);
		if (!info.pRasterizationState)
			return false;
	}

	if (multisample)
	{
		info.pMultisampleState = parse_multisample_state(recorder, multisample);
		if (!info.pMultisampleState)
			return false;
	}

	if (depth_stencil)
	{
		info.pDepthStencilState = parse_depth_stencil_state(recorder, depth_stencil);
		if (!info.pDepthStencilState)
			return false;
	}

	if (blend_state)
	{
		info.pColorBlendState = parse_blend_state(recorder, blend_state);
		if (!info.pColorBlendState)
			return false;
	}

	if (dynamic_state)
	{
		info.pDynamicState = parse_dynamic_state(recorder, dynamic_state);
		if (!info.pDynamicState)
			return false;
	}

	info.layout = GET_HANDLE(VkPipelineLayout);
	info.renderPass = GET_HANDLE(VkRenderPass);
	info.subpass = GET_U32();
	info.basePipelineHandle = GET_HANDLE(VkPipeline);
	info.basePipelineIndex = GET_U32();

	unsigned index = recorder.register_graphics_pipeline(
			Hashing::compute_hash_graphics_pipeline(recorder, info), info);
	recorder.set_graphics_pipeline_handle(index, (VkPipeline)id->AsUInt64());
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
		else if (chunk->name == "vkCreateGraphicsPipelines")
		{
			if (!serialise_graphics_pipeline(recorder, structData.buffers,
			                                 chunk->data.children[3],
			                                 chunk->data.children[5]))
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
