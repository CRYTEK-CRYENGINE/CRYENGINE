// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CGF/CGFSaver.h"

class CSkinSaver
{
public:
	static bool SaveUncompiledSkinningInfo(CSaverCGF& cgfSaver, const CSkinningInfo& skinningInfo, const bool bSwapEndian, const float unitSizeInCentimeters);

	static int SaveBoneInitialMatrices(CSaverCGF& cgfSaver, const DynArray<CryBoneDescData>& bonesDesc, const bool bSwapEndian, const float unitSizeInCentimeters);
	static int SaveBoneNames(CSaverCGF& cgfSaver, const DynArray<CryBoneDescData>& bonesDesc, const bool bSwapEndian);
};


