// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "HalfEdgeMesh.h"

namespace Designer
{
struct SubdivisionContext
{
	_smart_ptr<HalfEdgeMesh> fullPatches;
	_smart_ptr<HalfEdgeMesh> transitionPatches;
};

class Subdivision : public _i_reference_target_t
{
public:
	SubdivisionContext CreateSubdividedMesh(Model* pModel, int nSubdivisionLevel);

private:
	SubdivisionContext CreateSubdividedMesh(SubdivisionContext& sc);

	struct Position
	{
		enum EVertexType
		{
			eVT_Valid  = BIT(0),
			eVT_Corner = BIT(1)
		};
		explicit Position(int _pos_index, const Vec2& _uv, char _flag = eVT_Valid) : pos_index(_pos_index), uv(_uv), flag(_flag) {}
		int  pos_index;
		char flag;
		Vec2 uv;
		bool IsValid() const  { return flag & eVT_Valid; }
		bool IsCorner() const { return flag & eVT_Corner; }
	};

	void CalculateNextLocations(
	  HalfEdgeMesh* pHalfEdgeMesh,
	  std::vector<HE_Position>& outNextPosList,
	  std::vector<Position>& outNextFaceLocations,
	  std::vector<Position>& outNextEdgeLocations,
	  std::vector<Position>& outNextVertexLocations);

};
}

