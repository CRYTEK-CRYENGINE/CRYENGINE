// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannequinUtils.h"

#include <ICryMannequin.h>

const char* mannequin::FindProcClipTypeName(const IProceduralClipFactory::THash& typeNameHash)
{
	if (gEnv && gEnv->pGameFramework)
	{
		const IProceduralClipFactory& proceduralClipFactory = gEnv->pGameFramework->GetMannequinInterface().GetProceduralClipFactory();
		const char* const typeName = proceduralClipFactory.FindTypeName(typeNameHash);
		return typeName;
	}
	return NULL;
}
