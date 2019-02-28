// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SubstanceCommon.h"
#include "CrySubstanceAPI.h"

class ISubstanceInstanceRenderer;


class CRY_SUBSTANCE_API ISubstanceManager
{

	public:
		static ISubstanceManager* Instance();

		virtual void CreateInstance(const string& archiveName, const string& instanceName, const string& instanceGraph, const std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution) = 0;
		virtual bool GetArchiveContents(const string& archiveName, std::map<string, std::vector<string>>& contents) = 0;
		virtual bool IsRenderPending(const SubstanceAir::UInt id) const = 0;
		virtual void RegisterInstanceRenderer(ISubstanceInstanceRenderer* renderer) = 0;
		
		virtual void GenerateOutputs(ISubstancePreset* preset, ISubstanceInstanceRenderer* renderer) = 0;
		virtual SubstanceAir::UInt GenerateOutputsAsync(ISubstancePreset* preset, ISubstanceInstanceRenderer* renderer) = 0;
		virtual SubstanceAir::UInt GenerateOutputsAsync(const std::vector<ISubstancePreset*>& preset, ISubstanceInstanceRenderer* renderer) = 0;
};



