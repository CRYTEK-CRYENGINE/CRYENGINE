// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ----------------------------------------------------------------------------------------
//  File name:   MaterialEffectsDebug.h
//  Description: MaterialEffects debug utility class
//
////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _MATERIAL_EFFECTS_DEBUG_H_
#define _MATERIAL_EFFECTS_DEBUG_H_

#pragma once

#include <CryAction/IMaterialEffects.h>

class CMaterialEffects;

namespace MaterialEffectsUtils
{

#ifdef MATERIAL_EFFECTS_DEBUG

class CVisualDebug
{
private:

	struct SDebugVisualEntry
	{
		SDebugVisualEntry()
			: lifeTime(0.0f)
			, fxId(InvalidEffectId)
			, materialName1("")
			, materialName2("")
		{

		}

		CryFixedStringT<32> materialName1;
		CryFixedStringT<32> materialName2;

		Vec3                fxPosition;
		Vec3                fxDirection;

		float               lifeTime;

		TMFXEffectId        fxId;
	};

	struct SLastSearchHint
	{
		SLastSearchHint()
		{
			Reset();
		}

		void Reset()
		{
			materialName1 = "";
			materialName2 = "";
			fxId = InvalidEffectId;
		}

		CryFixedStringT<32> materialName1;
		CryFixedStringT<32> materialName2;

		TMFXEffectId        fxId;
	};

public:

	CVisualDebug()
		: m_nextHit(0)
	{

	}

	void AddLastSearchHint(const TMFXEffectId effectId, const int surfaceIndex1, const int surfaceIndex2);
	void AddLastSearchHint(const TMFXEffectId effectId, const char* customName, const int surfaceIndex2);
	void AddLastSearchHint(const TMFXEffectId effectId, const IEntityClass* pEntityClass, const int surfaceIndex2);

	void AddEffectDebugVisual(const TMFXEffectId effectId, const SMFXRunTimeEffectParams& runtimeParams);
	void Update(const CMaterialEffects& materialEffects, const float frameTime);

private:

	const static uint32 kMaxDebugVisualMfxEntries = 48;

	SLastSearchHint     m_lastSearchHint;

	SDebugVisualEntry   m_effectList[kMaxDebugVisualMfxEntries];
	uint32              m_nextHit;
};

#endif //MATERIAL_EFFECTS_DEBUG

}

#endif // _MATERIAL_EFFECTS_DEBUG_H_
