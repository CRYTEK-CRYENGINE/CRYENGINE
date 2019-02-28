// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OpticsFactory.h"

#include "RootOpticsElement.h"
#include "OpticsElement.h"
#include "Ghost.h"
#include "Glow.h"
#include "ChromaticRing.h"
#include "CameraOrbs.h"
#include "IrisShafts.h"
#include "Streaks.h"
#include "ImageSpaceShafts.h"
#include "OpticsReference.h"
#include "OpticsProxy.h"
#include "OpticsPredef.hpp"

IOpticsElementBase* COpticsFactory::Create(EFlareType type) const
{
	switch (type)
	{
	case eFT_Root:
		return new RootOpticsElement;
	case eFT_Group:
		return new COpticsGroup("[Group]");
	case eFT_Ghost:
		return new CLensGhost("Ghost");
	case eFT_MultiGhosts:
		return new CMultipleGhost("Multi Ghost");
	case eFT_Glow:
		return new Glow("Glow");
	case eFT_IrisShafts:
		return new IrisShafts("Iris Shafts");
	case eFT_ChromaticRing:
		return new ChromaticRing("Chromatic Ring");
	case eFT_CameraOrbs:
		return new CameraOrbs("Orbs");
	case eFT_ImageSpaceShafts:
		return new ImageSpaceShafts("Vol Shafts");
	case eFT_Streaks:
		return new Streaks("Streaks");
	case eFT_Reference:
		return new COpticsReference("Reference");
	case eFT_Proxy:
		return new COpticsProxy("Proxy");
	default:
		return NULL;
	}
}
