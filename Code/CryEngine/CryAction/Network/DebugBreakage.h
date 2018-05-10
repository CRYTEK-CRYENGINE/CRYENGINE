// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DEBUG_NET_BREAKAGE__H__
#define __DEBUG_NET_BREAKAGE__H__

#pragma once

// Enable logging of breakage events and breakage serialisation
#define DEBUG_NET_BREAKAGE 0

#ifndef XYZ
	#define XYZ(v) (v).x, (v).y, (v).z
#endif

#if !defined(_RELEASE) && DEBUG_NET_BREAKAGE

	#include <CryString/StringUtils.h>

	#define LOGBREAK(...) CryLogAlways("brk: " __VA_ARGS__)

static void LOGBREAK_STATOBJ(IStatObj* pObj)
{
	if (pObj)
	{
		unsigned int breakable = pObj->GetBreakableByGame();
		int idBreakable = pObj->GetIDMatBreakable();
		Vec3 bbmin = pObj->GetBoxMin();
		Vec3 bbmax = pObj->GetBoxMax();
		Vec3 vegCentre = pObj->GetVegCenter();
		const char* filePath = pObj->GetFilePath();
		const char* geoName = pObj->GetGeoName();
		int subObjCount = pObj->GetSubObjectCount();
		LOGBREAK("StatObj: bbmin = (%f, %f, %f) bbmax = (%f, %f, %f)", XYZ(bbmin), XYZ(bbmax));
		LOGBREAK("StatObj: vegCentre = (%f, %f, %f)", XYZ(vegCentre));
		LOGBREAK("StatObj: breakable = %d, idBreakable = %d, subObjCount = %d", breakable, idBreakable, subObjCount);
		LOGBREAK("StatObj: filePath = '%s', geoName = '%s'", filePath, geoName);
		LOGBREAK("StatObj: filePathHash = %d", CryStringUtils::CalculateHash(filePath));
	}
}

#else

	#define LOGBREAK(...)
	#define LOGBREAK_STATOBJ(x)

#endif

#endif // __DEBUG_NET_BREAKAGE__H__
