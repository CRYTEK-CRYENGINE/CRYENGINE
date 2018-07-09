// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ENGINETERRAINMODIFICATIONS_H__
#define __ENGINETERRAINMODIFICATIONS_H__

#pragma once

#include <CryNetwork/ISerialize.h>            // TSerialize

// helper class to serialize (load/save) terrain heighmap changes and limit the modification
// also to remove some vegetation for the affected area
class CTerrainModifications : public Cry3DEngineBase
{
public:
	// constructor
	CTerrainModifications();

	void SetTerrain(CTerrain& rTerrain);

	void FreeData();

	// Arguments:
	//   fRadius - needs to be >0
	// Returns:
	//   true = modification was accepted, false=modifiction was rejected
	bool PushModification(const Vec3& vPos, const float fRadius);

	// needed for savegames
	void SerializeTerrainState(TSerialize ser);

private: // -------------------------------------------------------------------

	struct STerrainMod
	{
		// default constructor (needed to serialize the vector)
		STerrainMod()
			: m_vPos(0, 0, 0), m_fHeight(0), m_fRadius(0)
		{
		}

		// constructor
		STerrainMod(const Vec3& vPos, const float fRadius, const float fHeight)
			: m_vPos(vPos), m_fRadius(fRadius), m_fHeight(fHeight)
		{
		}

		Vec3  m_vPos;             // midpoint
		float m_fHeight;          // old height at (m_vPos.x,m_vPos.y)  (explosion might not be at the ground)
		float m_fRadius;          //

		void  Serialize(TSerialize ser)
		{
			ser.Value("pos", m_vPos);
			ser.Value("r", m_fRadius);
			ser.Value("height", m_fHeight);
		}
	};

	// ---------------------------------------------------------------------------

	std::vector<STerrainMod> m_TerrainMods;               //
	CTerrain*                m_pModifiedTerrain;          //

	// ---------------------------------------------------------------------------

	void MakeCrater(const STerrainMod& ref, const uint32 dwCheckExistingMods);

	// O(n), n=registered modifications and that should not be a huge number but that might need improvement
	float ComputeMaxDepthAt(const float fX, const float fY, const uint32 dwCheckExistingMods) const;
};

#endif // __ENGINETERRAINMODIFICATIONS_H__
