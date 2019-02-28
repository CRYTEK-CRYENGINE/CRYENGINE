// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "CrySubstanceAPI.h"
#include "SubstanceCommon.h"
#include "substance/framework/graph.h"
#include "substance/framework/inputimage.h"


class CRY_SUBSTANCE_API ISubstanceInstanceRenderer
{
public:
	virtual void OnOutputAvailable(SubstanceAir::UInt runUid, const SubstanceAir::GraphInstance *graphInstance,
		SubstanceAir::OutputInstanceBase * outputInstance) = 0;
	virtual void FillOriginalOutputRenderData(const ISubstancePreset* preset, SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData) {};
	virtual void FillVirtualOutputRenderData(const ISubstancePreset* preset, const SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData) = 0;
	virtual bool SupportsOriginalOutputs() { return false; }
	virtual SubstanceAir::InputImage::SPtr GetInputImage(const ISubstancePreset* preset, const string& path) = 0;
};
