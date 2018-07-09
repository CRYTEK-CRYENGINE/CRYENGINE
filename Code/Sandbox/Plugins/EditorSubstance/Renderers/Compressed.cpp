// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Compressed.h"
#include "EditorSubstanceManager.h"
#include "IEditor.h"
#include "CryRenderer/IRenderer.h"
#include "Cry3DEngine/ImageExtensionHelper.h"
#include "CryRenderer/IImage.h"

namespace EditorSubstance
{
	namespace Renderers
	{

		CCompressedRenderer::CCompressedRenderer() : CInstanceRenderer()
		{

		}

		void CCompressedRenderer::FillVirtualOutputRenderData(const ISubstancePreset* preset, const SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData)
		{
			STexturePreset presetConfig = CManager::Instance()->GetConfigurationForPreset(output.preset);
			if (presetConfig.format == EditorSubstance::ePixelFormat_Unsupported)
				return;

			int flags = 0;
			if (presetConfig.colorSpace == EditorSubstance::eColorSpaceSRGB)
				flags |= EditorSubstance::TF_SRGB;
			if (presetConfig.decal)
				flags |= EditorSubstance::TF_DECAL;
			if (presetConfig.formatAlpha != EditorSubstance::ePixelFormat_Unsupported)
				flags |= EditorSubstance::TF_HAS_ATTACHED_ALPHA;
			if (output.name.find("ddn") == 0)
				flags |= EditorSubstance::TF_NORMALMAP;

			if (flags & EditorSubstance::TF_HAS_ATTACHED_ALPHA)
			{
				renderData.resize(2);
				int alphaFlags = 0;
				if (flags & EditorSubstance::TF_SRGB)
					alphaFlags |= EditorSubstance::TF_SRGB;
				if (flags & EditorSubstance::TF_DECAL)
					alphaFlags |= EditorSubstance::TF_DECAL;

				alphaFlags |= EditorSubstance::TF_ALPHA;
				SSubstanceOutput alphaOutput(output);
				alphaOutput.SetAllSources(-1);
				alphaOutput.channels[0].sourceId = 0; // R channel will use just first source.
				alphaOutput.sourceOutputs[0] = output.sourceOutputs[output.channels[3].sourceId];
				alphaOutput.channels[0].channel = output.channels[3].channel;

				SSubstanceRenderData& alphaRenderData = renderData[1];
				alphaRenderData.format = GetOutputCompressedFormat(presetConfig.formatAlpha, alphaFlags & EditorSubstance::TF_SRGB);
				alphaRenderData.name = alphaOutput.name + "_attachedAlpha";
				alphaRenderData.useMips = presetConfig.mipmaps;
				alphaRenderData.skipAlpha = true;
				alphaRenderData.output = alphaOutput;
				AttachOutputUserData(preset, alphaOutput, alphaRenderData, alphaFlags);
			}
			else
			{
				renderData.resize(1);
			}
			SSubstanceRenderData& outRenderData = renderData[0];
			outRenderData.format = GetOutputCompressedFormat(presetConfig.format, flags & EditorSubstance::TF_SRGB);
			outRenderData.name = output.name;
			outRenderData.useMips = presetConfig.mipmaps;
			outRenderData.skipAlpha = flags & EditorSubstance::TF_HAS_ATTACHED_ALPHA;
			outRenderData.swapRG = (flags & EditorSubstance::TF_NORMALMAP) > 0;
			outRenderData.output = output;
			AttachOutputUserData(preset, output, outRenderData, flags);
		}

		unsigned int CCompressedRenderer::GetOutputCompressedFormat(const EditorSubstance::EPixelFormat& format, bool useSRGB)
		{
			unsigned int res = 0;
			switch (format)
			{
			case(EditorSubstance::ePixelFormat_BC1):
				res = Substance_PF_BC1;
				break;
			case(EditorSubstance::ePixelFormat_BC1a):
				res = Substance_PF_BC1;
				break;
			case(EditorSubstance::ePixelFormat_BC3):
				res = Substance_PF_BC3;
				break;
			case(EditorSubstance::ePixelFormat_BC4):
				res = Substance_PF_BC4;
				break;
			case(EditorSubstance::ePixelFormat_BC5):
				res = Substance_PF_BC5;
				break;
			case(EditorSubstance::ePixelFormat_BC5s):
				res = Substance_PF_BC5;
				break;
			case(EditorSubstance::ePixelFormat_BC7):
				res = Substance_PF_BC3;
				break;
			case(EditorSubstance::ePixelFormat_BC7t):
				res = Substance_PF_BC3;
				break;
			case(EditorSubstance::ePixelFormat_A8R8G8B8):
				res = Substance_PF_RGBA | Substance_PF_8I;
				break;
			}
			if (useSRGB)
				res |= Substance_PF_sRGB;

			return res;
		}

		void CCompressedRenderer::AttachOutputUserData(const ISubstancePreset* preset, const SSubstanceOutput& output, SSubstanceRenderData& renderData, const int& flags)
		{
			if (!m_PresetOutputDataMap.count(preset->GetInstanceID()) || !m_PresetOutputDataMap[preset->GetInstanceID()].count(renderData.name))
			{
				std::shared_ptr<SGeneratedOutputData> generatedData = std::make_shared<SGeneratedOutputData>();
				generatedData->path = preset->GetOutputPath(&output);
				generatedData->suffix = output.name;
				m_PresetOutputDataMap[preset->GetInstanceID()][renderData.name] = generatedData;
			}
			SGeneratedOutputData* generatedData = m_PresetOutputDataMap[preset->GetInstanceID()][renderData.name].get();
			generatedData->texturePreset = output.preset;
			generatedData->flags = flags;
			renderData.customData = (size_t)generatedData;
		}

		void CCompressedRenderer::RemovePresetRenderData(ISubstancePreset* preset)
		{
			if (m_PresetOutputDataMap.count(preset->GetInstanceID()))
			{
				m_PresetOutputDataMap.erase(preset->GetInstanceID());
			}
		}

		void CCompressedRenderer::ProcessComputedOutputs()
		{
			OutputsSet currentOutputs = GetComputedOutputs();
			for (auto itr = currentOutputs.begin(); itr != currentOutputs.end(); ++itr) {
				SubstanceAir::OutputInstance::Result result((*itr)->grabResult());
				if (result.get() != NULL)
				{
					UpdateTexture(result.get(), (SGeneratedOutputData*)(*itr)->mUserData);
				}
			}

		}

		void CCompressedRenderer::UpdateTexture(SubstanceAir::RenderResult* result, SGeneratedOutputData* data)
		{
			string textureName = data->path + ".dds";
			int FIM_textureFlags = 0;
			int FT_textureFlags = 0;
			if (data->flags & EditorSubstance::TF_SRGB)
				FIM_textureFlags |= FIM_SRGB_READ;
			if (data->flags & EditorSubstance::TF_DECAL)
				FIM_textureFlags |= FIM_DECAL;
			if (data->flags & EditorSubstance::TF_NORMALMAP)
				FIM_textureFlags |= FIM_NORMALMAP;
			if (data->flags & EditorSubstance::TF_ALPHA)
				FT_textureFlags |= FT_ALPHA;
			if (data->flags & EditorSubstance::TF_HAS_ATTACHED_ALPHA)
				FT_textureFlags |= FT_HAS_ATTACHED_ALPHA;

			ITexture* const pTargetTexture = GetIEditor()->GetSystem()->GetIRenderer()->EF_GetTextureByName(textureName, FT_textureFlags);
			if (!pTargetTexture)
			{
				return;
			}

			const SubstanceTexture& texture = result->getTexture();

			ETEX_Format resultFormat;
			int formatFlag = texture.pixelFormat & 0x1F;
			if (formatFlag == Substance_PF_BC5)
			{
				resultFormat = ETEX_Format::eTF_BC5U;
			}
			else if (formatFlag == Substance_PF_BC4)
			{
				resultFormat = ETEX_Format::eTF_BC4U;
			}
			else if (formatFlag == Substance_PF_BC3)
			{
				resultFormat = ETEX_Format::eTF_BC3;
			}
			else if (formatFlag == Substance_PF_BC1)
			{
				resultFormat = ETEX_Format::eTF_BC1;
			}
			else if (formatFlag == Substance_PF_RGBA)
			{
				resultFormat = ETEX_Format::eTF_R8G8B8A8;
			}
			else
			{
				return;
			}

			STexData sd;
			sd.m_nHeight = texture.level0Height;
			sd.m_nWidth = texture.level0Width;
			sd.m_nMips = texture.mipmapCount;
			sd.m_pFilePath = textureName;
			sd.m_eFormat = resultFormat;
			sd.m_nFlags = FIM_textureFlags;
			sd.m_pData[0] = (byte*)texture.buffer;
			uint32 targetFlags = pTargetTexture->GetFlags();
			targetFlags &= ~(FT_ALPHA);
			targetFlags &= ~(FT_HAS_ATTACHED_ALPHA);
			if (FT_textureFlags & FT_ALPHA)
				targetFlags |= FT_ALPHA;
			if (FT_textureFlags & FT_HAS_ATTACHED_ALPHA)
				targetFlags |= FT_HAS_ATTACHED_ALPHA;


			if (sd.m_nFlags & FIM_NORMALMAP)
			{
				if (sd.m_eFormat == eTF_BC5U)
				{
					unsigned char *blocks = sd.m_pData[0];
					unsigned char *end = sd.m_pData[0] + TextureDataSize(sd.m_nWidth, sd.m_nHeight, sd.m_nDepth, sd.m_nMips, sd.m_eFormat);
					while (blocks < end)
					{
						// Cheapest transform, always rounds towards negative infinity (because the input is entirely positive)
						// Better transforms with less error need to be done in floating-point and truncation (which rounds the signed number towards 0)

						blocks[0] = char((float(blocks[0]) * 2.0f / 255 - 1.0f) * 127);
						blocks[1] = char((float(blocks[1]) * 2.0f / 255 - 1.0f) * 127);

						blocks += 4;
					}
					sd.m_eFormat = eTF_BC5S;
				}


			}
			pTargetTexture->UpdateData(sd, targetFlags);
		}

		uint32 CCompressedRenderer::TextureDataSize(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, const ETEX_Format eTF) const
		{
			if (eTF == eTF_Unknown)
				return 0;


			const bool bIsBlockCompressed = CImageExtensionHelper::IsBlockCompressed(eTF);
			const uint32 nBytesPerBlock = bIsBlockCompressed ? CImageExtensionHelper::BytesPerBlock(eTF) : CImageExtensionHelper::BitsPerPixel(eTF) / 8;
			uint32 nSize = 0;
			if (bIsBlockCompressed)
			{
				nWidth = max(1U, nWidth);
				nHeight = max(1U, nHeight);
				nWidth = ((nWidth + 3) / 4);
				nHeight = ((nHeight + 3) / 4);
			}

			while ((nWidth || nHeight || nDepth) && nMips)
			{
				nWidth = max(1U, nWidth);
				nHeight = max(1U, nHeight);
				nDepth = max(1U, nDepth);

				nSize += nWidth * nHeight * nDepth * nBytesPerBlock;

				nWidth >>= 1;
				nHeight >>= 1;
				nDepth >>= 1;
				--nMips;
			}

			return nSize;
		
		}
	}
}
