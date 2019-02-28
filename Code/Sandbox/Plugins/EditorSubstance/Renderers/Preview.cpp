// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Preview.h"

#include "SubstanceCommon.h"

namespace EditorSubstance
{
	namespace Renderers
	{


		CPreviewRenderer::CPreviewRenderer() : CInstanceRenderer()
		{

		}

		void CPreviewRenderer::FillVirtualOutputRenderData(const ISubstancePreset* preset, const SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData)
{
	ProcessOutput(preset, output, renderData, true);
}



void CPreviewRenderer::FillOriginalOutputRenderData(const ISubstancePreset* preset, SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData)
{
	ProcessOutput(preset, output, renderData, false);
}

void CPreviewRenderer::ProcessComputedOutputs()
{

	OutputsSet currentOutputs = GetComputedOutputs();
	for (auto itr = currentOutputs.begin(); itr != currentOutputs.end(); ++itr) {
		SubstanceAir::OutputInstance::Result result((*itr)->grabResult());
		if (result.get() != NULL)
		{
			outputComputed(result.get(), (SPreviewGeneratedOutputData*)(*itr)->mUserData);
		}
	}
}

void CPreviewRenderer::RemovePresetRenderData(ISubstancePreset* preset)
{
	if (m_PresetOutputDataMap.count(preset->GetInstanceID()))
	{
		m_PresetOutputDataMap.erase(preset->GetInstanceID());
	}
}

void CPreviewRenderer::ProcessOutput(const ISubstancePreset* preset, const SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData, bool isVirtual)
{
	renderData.resize(1);
	SSubstanceRenderData& outRenderData = renderData[0];
	outRenderData.format = Substance_PF_RGBA | Substance_PF_8I;
	outRenderData.name = output.name;
	outRenderData.useMips = false;
	outRenderData.skipAlpha = false;
	outRenderData.swapRG = false;
	outRenderData.output = output;
	outRenderData.output.enabled = true;
	outRenderData.output.resolution = SSubstanceOutput::Original;
	AttachOutputUserData(preset, outRenderData.output, outRenderData, isVirtual);

}

void CPreviewRenderer::AttachOutputUserData(const ISubstancePreset* preset, const SSubstanceOutput& output, SSubstanceRenderData& renderData, bool isVirtual)
{
	string cacheName = renderData.name + (isVirtual ? "_virtual" : "_original");

	if (!m_PresetOutputDataMap.count(preset->GetInstanceID()) || !m_PresetOutputDataMap[preset->GetInstanceID()].count(cacheName))
	{
		std::shared_ptr<SPreviewGeneratedOutputData> generatedData = std::make_shared<SPreviewGeneratedOutputData>();
		generatedData->outputName = renderData.name;
		generatedData->isVirtual = isVirtual;
		generatedData->presetInstanceID = preset->GetInstanceID();
		m_PresetOutputDataMap[preset->GetInstanceID()][cacheName] = generatedData;
	}
	SPreviewGeneratedOutputData* generatedData = m_PresetOutputDataMap[preset->GetInstanceID()][cacheName].get();
	renderData.customData = (size_t)generatedData;
}

} // END namespace Renderers
} // END n
