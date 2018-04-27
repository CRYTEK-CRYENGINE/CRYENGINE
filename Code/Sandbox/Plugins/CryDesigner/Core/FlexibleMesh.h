// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer
{
class FlexibleMesh : public _i_reference_target_t
{
public:

	FlexibleMesh(){}
	FlexibleMesh(const FlexibleMesh& mesh)
	{
		vertexList = mesh.vertexList;
		normalList = mesh.normalList;
		faceList = mesh.faceList;
		matId2SubsetMap = mesh.matId2SubsetMap;
	}

	void Clear();
	int  FindMatIdFromSubsetNum(int nSubsetNum) const;
	int  AddMatID(int nMatID);
	void Reserve(int nSize);
	bool IsValid() const;
	bool IsPassed(const BrushRay& ray, BrushFloat& outT) const;
	bool IsPassedUV(const Ray& ray) const;
	bool IsOverlappedUV(const AABB& aabb) const;
	void FillIndexedMesh(IIndexedMesh* pMesh) const;
	void Join(const FlexibleMesh& mesh);
	void Invert();

	std::vector<Vertex>    vertexList;
	std::vector<BrushVec3> normalList;
	std::vector<SMeshFace> faceList;
	std::map<int, int>     matId2SubsetMap;
};
typedef _smart_ptr<FlexibleMesh> FlexibleMeshPtr;
}

