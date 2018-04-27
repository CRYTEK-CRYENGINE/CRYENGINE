// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlexibleMesh.h"

namespace Designer
{
void FlexibleMesh::Clear()
{
	vertexList.clear();
	normalList.clear();
	faceList.clear();
	matId2SubsetMap.clear();
}

int FlexibleMesh::FindMatIdFromSubsetNum(int nSubsetNum) const
{
	auto ii = matId2SubsetMap.begin();
	for (; ii != matId2SubsetMap.end(); ++ii)
	{
		if (ii->second == nSubsetNum)
			return ii->first;
	}
	return 0;
}

int FlexibleMesh::AddMatID(int nMatID)
{
	int nSubsetNumber = 0;
	if (matId2SubsetMap.find(nMatID) != matId2SubsetMap.end())
	{
		nSubsetNumber = matId2SubsetMap[nMatID];
	}
	else
	{
		nSubsetNumber = matId2SubsetMap.size();
		matId2SubsetMap[nMatID] = nSubsetNumber;
	}
	return nSubsetNumber;
}

void FlexibleMesh::Reserve(int nSize)
{
	vertexList.reserve(nSize);
	normalList.reserve(nSize);
	faceList.reserve(nSize);
}

bool FlexibleMesh::IsValid() const
{
	return !vertexList.empty() && !faceList.empty() && !normalList.empty();
}

bool FlexibleMesh::IsPassed(const BrushRay& ray, BrushFloat& outT) const
{
	for (int i = 0, iFaceCount(faceList.size()); i < iFaceCount; ++i)
	{
		const std::vector<Vertex>& v = vertexList;
		const SMeshFace& f = faceList[i];
		Vec3 vOut;
		if (Intersect::Ray_Triangle(Ray(ToVec3(ray.origin), ToVec3(ray.direction)), v[f.v[2]].pos, v[f.v[1]].pos, v[f.v[0]].pos, vOut) ||
		    Intersect::Ray_Triangle(Ray(ToVec3(ray.origin), ToVec3(ray.direction)), v[f.v[0]].pos, v[f.v[1]].pos, v[f.v[2]].pos, vOut))
		{
			outT = ray.origin.GetDistance(ToBrushVec3(vOut));
			return true;
		}
	}
	return false;
}

bool FlexibleMesh::IsPassedUV(const Ray& ray) const
{
	const std::vector<Vertex>& v = vertexList;

	for (int i = 0, iFaceCount(faceList.size()); i < iFaceCount; ++i)
	{
		const SMeshFace& f = faceList[i];
		Vec3 vOut;
		if (Intersect::Ray_Triangle(ray, v[f.v[2]].uv, v[f.v[1]].uv, v[f.v[0]].uv, vOut) ||
		    Intersect::Ray_Triangle(ray, v[f.v[0]].uv, v[f.v[1]].uv, v[f.v[2]].uv, vOut))
		{
			return true;
		}
	}
	return false;
}

bool FlexibleMesh::IsOverlappedUV(const AABB& aabb) const
{
	const std::vector<Vertex>& v = vertexList;

	for (int i = 0, iFaceCount(faceList.size()); i < iFaceCount; ++i)
	{
		const SMeshFace& f = faceList[i];

		for (int k = 0; k < 3; ++k)
		{
			if (aabb.IsContainPoint(v[f.v[k]].uv))
				return true;

			Lineseg ls(v[f.v[k]].uv, v[f.v[(k + 1) % 3]].uv);
			Vec3 out;
			if (Intersect::Lineseg_AABB(ls, aabb, out) != 0)
				return true;
		}
		;
	}
	return false;
}

void FlexibleMesh::FillIndexedMesh(IIndexedMesh* pMesh) const
{
	if (pMesh == NULL)
		return;

	int vertexCount = vertexList.size();
	int faceCount = faceList.size();

	pMesh->FreeStreams();
	pMesh->SetVertexCount(vertexCount);
	pMesh->SetFaceCount(faceCount);
	pMesh->SetIndexCount(0);
	pMesh->SetTexCoordCount(vertexCount);

	Vec3* const positions = pMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::POSITIONS);
	Vec3* const normals = pMesh->GetMesh()->GetStreamPtr<Vec3>(CMesh::NORMALS);
	SMeshTexCoord* const texcoords = pMesh->GetMesh()->GetStreamPtr<SMeshTexCoord>(CMesh::TEXCOORDS);
	SMeshFace* const faces = pMesh->GetMesh()->GetStreamPtr<SMeshFace>(CMesh::FACES);

	for (int i = 0; i < vertexCount; ++i)
	{
		positions[i] = ToVec3(vertexList[i].pos);
		normals[i] = ToVec3(normalList[i]);
		texcoords[i] = SMeshTexCoord(vertexList[i].uv);
	}

	memcpy(faces, &faceList[0], faceCount * sizeof(SMeshFace));

	if (!matId2SubsetMap.empty())
	{
		pMesh->SetSubSetCount(matId2SubsetMap.size());
		auto ii = matId2SubsetMap.begin();
		for (; ii != matId2SubsetMap.end(); ++ii)
			pMesh->SetSubsetMaterialId(ii->second, ii->first == -1 ? 0 : ii->first);
	}
	else
	{
		pMesh->SetSubSetCount(1);
		pMesh->SetSubsetMaterialId(0, 0);
	}
}

void FlexibleMesh::Join(const FlexibleMesh& mesh)
{
	int nVertexOffset = vertexList.size();

	vertexList.insert(vertexList.end(), mesh.vertexList.begin(), mesh.vertexList.end());
	normalList.insert(normalList.end(), mesh.normalList.begin(), mesh.normalList.end());

	for (int i = 0, iFaceCount(mesh.faceList.size()); i < iFaceCount; ++i)
	{
		SMeshFace f;
		for (int k = 0; k < 3; ++k)
			f.v[k] = mesh.faceList[i].v[k] + nVertexOffset;
		int nMatID = mesh.FindMatIdFromSubsetNum(mesh.faceList[i].nSubset);
		f.nSubset = AddMatID(nMatID);
		faceList.push_back(f);
	}
}

void FlexibleMesh::Invert()
{
	for (auto iNormal = normalList.begin(); iNormal != normalList.end(); ++iNormal)
		*iNormal = -*iNormal;

	for (auto iFace = faceList.begin(); iFace != faceList.end(); ++iFace)
		std::swap((*iFace).v[0], (*iFace).v[2]);
}
}

