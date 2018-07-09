// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"

namespace Designer
{
class UVIsland : public _i_reference_target_t
{
public:
	typedef _smart_ptr<UVIsland> UVIslandPtr;

public:
	UVIsland();
	~UVIsland();

	UVIsland(const UVIsland& uvIsland);
	UVIsland&   operator=(const UVIsland& uvIsland);

	void        Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bUndo, Model* pModel);
	UVIsland*   Clone() const;

	void        AddPolygon(PolygonPtr polygon);
	void        DeletePolygon(PolygonPtr polygon);
	bool        HasPolygon(PolygonPtr polygon);
	void        Clear()                                 { m_Polygons.clear(); Invalidate(); }
	void        Normalize(const AABB& range = AABB(Vec2(0, 0), Vec2(1.0f, 1.0f)));
	int         GetCount() const                        { return m_Polygons.size(); }
	PolygonPtr  GetPolygon(int idx) const               { return m_Polygons[idx]; }
	void        SetPolygon(int idx, PolygonPtr polygon) { m_Polygons[idx] = polygon; }
	bool        HasOverlappedEdges(PolygonPtr polygon) const;
	void        Transform(const Matrix33& tm);
	void        Join(UVIsland* pUVIsland);
	int         GetSubMatID() const;
	bool        HasSubMatID(int nSubMatID) const;
	const CryGUID& GetGUID() const { return m_Guid; }
	bool        FindIsolatedAreas(std::vector<UVIslandPtr>& outIsolatedAreas);

	bool        IsPicked(const Ray& ray) const;
	bool        IsOverlapped(const AABB& aabb) const;
	bool        IsEmpty() const { return m_Polygons.empty(); }

	const AABB& GetUVBoundBox() const;
	void        Invalidate() const;
	void        ResetPolygonsInModel(Model* pModel);
	void        ResetPolygonsInUVIslands(Model* pModel);

	void        Clean(Model* pModel);

private:
	CryGUID                    m_Guid;
	std::vector<PolygonPtr> m_Polygons;
	mutable AABB*           m_pUVBoundBox;
};
typedef UVIsland::UVIslandPtr UVIslandPtr;
}

