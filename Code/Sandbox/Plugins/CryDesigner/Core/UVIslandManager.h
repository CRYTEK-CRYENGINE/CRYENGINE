// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UVIsland.h"

namespace Designer
{
class UVIslandManager : public _i_reference_target_t
{
public:
	void             AddUVIsland(UVIslandPtr pUVIsland);

	bool             FindUVIslandsHavingPolygon(PolygonPtr polygon, std::vector<UVIslandPtr>& OutUVIslands);
	void             RemoveEmptyUVIslands();

	int              GetCount() const           { return m_UVIslands.size(); }
	UVIslandPtr      GetUVIsland(int idx) const { return m_UVIslands[idx]; }
	UVIslandPtr      FindUVIsland(const CryGUID& guid) const;
	void             Reset(Model* pModel);

	void             Join(UVIslandManager* pUVIslandManager);
	void             ConvertIsolatedAreasToIslands();

	void             Clear() { m_UVIslands.clear(); }
	void             Clean(Model* pModel);
	void             Invalidate();
	bool             HasSubMatID(int nSubMatID) const;

	void             Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo, Model* pModel);

	UVIslandManager& operator=(const UVIslandManager& uvIslandMgr);
	UVIslandManager* Clone() const;

private:
	std::vector<UVIslandPtr> m_UVIslands;
};
}

