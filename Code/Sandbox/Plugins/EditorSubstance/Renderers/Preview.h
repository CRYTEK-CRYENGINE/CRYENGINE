// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Base.h"
#include "CrySandbox/CrySignal.h"

namespace SubstanceAir
{
	struct RenderResult;
}

namespace EditorSubstance
{
namespace Renderers
{

struct SPreviewGeneratedOutputData
{
	string outputName;
	bool isVirtual;
	SubstanceAir::UInt presetInstanceID;
};

typedef std::map<string, std::shared_ptr<SPreviewGeneratedOutputData>> PreviewOutputDataMap;
typedef std::unordered_map<SubstanceAir::UInt, PreviewOutputDataMap> PresetPreviewOutputsDataMap;

class CPreviewRenderer : public CInstanceRenderer
{

public:

	CPreviewRenderer();

	virtual void FillVirtualOutputRenderData(const ISubstancePreset* preset, const SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData) override;
	virtual void FillOriginalOutputRenderData(const ISubstancePreset* preset, SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData) override;
	virtual void ProcessComputedOutputs() override;
	virtual bool SupportsOriginalOutputs() { return true; }
	virtual void RemovePresetRenderData(ISubstancePreset* preset) override;
	CCrySignal <void(SubstanceAir::RenderResult* result, SPreviewGeneratedOutputData* data)> outputComputed;
protected:

private:
	void ProcessOutput(const ISubstancePreset* preset, const SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData, bool isVirtual);
	void AttachOutputUserData(const ISubstancePreset* preset, const SSubstanceOutput& output, SSubstanceRenderData& renderData, bool isVirtual);
	PresetPreviewOutputsDataMap m_PresetOutputDataMap;
};


} // END namespace Renderers
} // END namespace EditorSubstance





