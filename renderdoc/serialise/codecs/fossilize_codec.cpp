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
