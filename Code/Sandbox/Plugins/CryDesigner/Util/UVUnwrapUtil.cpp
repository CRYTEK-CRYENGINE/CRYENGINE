// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UVUnwrapUtil.h"
#include "Core/Model.h"
#include "Util/ElementSet.h"
#include "DesignerEditor.h"
#include "DesignerSession.h"

namespace Designer {
namespace UnwrapUtil
{

void ApplyPlanarProjection(const BrushPlane& plane, PolygonPtr polygon)
{
	for (int i = 0, iVertexCount(polygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		BrushVec2 uv = plane.W2P(polygon->GetPos(i));
		Vec2 inv_uv((float)uv.y, (float)uv.x);
		polygon->SetUV(i, inv_uv);
		polygon->SetTexInfo(TexInfo());
	}
}

void ApplySphereProjection(const BrushVec3& origin, float radius, PolygonPtr polygon)
{
	int iVertexCount = polygon->GetVertexCount();
	for (int i = 0; i < iVertexCount; ++i)
	{
		Vec3 pos = ToVec3(polygon->GetPos(i) - origin) / radius;
		float u = 0.5f + std::atan2(pos.y, pos.x) / (PI * 2);
		float v = 0.5f - std::asin(pos.z) / PI;
		polygon->SetUV(i, Vec2(u, v));
	}
}

void ApplyCylinderProjection(const BrushVec3& bottom_origin, float bottom_radius, float height, PolygonPtr polygon)
{
	int iVertexCount = polygon->GetVertexCount();
	const BrushPlane& p = polygon->GetPlane();
	// Only apply cylinder projection to polygons not parallel to z plane. 
	// May make sense to filter based on 45 degree angle (0.5*sqrt(2)) even, but for now just apply to exceptional case
	if (1.0f - abs(p.Normal().Dot(BrushVec3(0.0, 0.0, 1.0))) > 0.0001f)
	{
		// probably unnecessary check, but better be safe than sorry
		if (iVertexCount > 0)
		{
			Vec3 pos = ToVec3(polygon->GetPos(0) - bottom_origin) / bottom_radius;
			float origTanval = std::atan2(pos.y, pos.x);

			for (int i = 0; i < iVertexCount; ++i)
			{
				pos = ToVec3(polygon->GetPos(i) - bottom_origin) / bottom_radius;
				float tanVal = std::atan2(pos.y, pos.x);

				if (abs(origTanval - tanVal) > PI)
				{
					// coordinates will wrap around the cylinder, to prevent that either add or subract 2 * PI
					tanVal += PI * ((origTanval < 0.0f) ? -2.0f : 2.0f);
				}
				float u = 0.5f + (0.5f / PI) * tanVal;
				float v = (float)(polygon->GetPos(i).z - bottom_origin.z) / height;
				polygon->SetUV(i, Vec2(u, v));
			}
		}
	}
	else
	{
		// Polygon is parallel to z plane, just keep its x y coordinates as UVs and scale accordingly
		for (int i = 0; i < iVertexCount; ++i)
		{
			Vec3 pos = ToVec3(polygon->GetPos(i) - bottom_origin) / bottom_radius;
			float u = 0.5f + 0.5f * pos.x;
			float v = 0.5f + 0.5f * pos.y;
			polygon->SetUV(i, Vec2(u, v));
		}
	}
}

void ApplyCameraProjection(const CCamera& camera, const BrushMatrix34& worldTM, PolygonPtr polygon)
{
	for (int i = 0, iVertexCount(polygon->GetVertexCount()); i < iVertexCount; ++i)
	{
		Vec3 projected;
		Project(camera, ToVec3(worldTM.TransformPoint(polygon->GetPos(i))), projected);
		projected.x = projected.x * 0.5f + 0.5f;
		projected.y = projected.y * 0.5f + 0.5f;
		polygon->SetUV(i, Vec2(projected.x, projected.y));
		polygon->SetTexInfo(TexInfo());
	}
}

void PackUVIslands(UVIslandManager* pUVIslandMgr, std::vector<UVIslandPtr>& uvIslands, EPlaneAxis projectAxis)
{
	int iUVIslandCount(uvIslands.size());
	int columnRowCount = static_cast<int>(sqrt(iUVIslandCount));

	// in the case we have an exact root, do not add one
	if (columnRowCount * columnRowCount < iUVIslandCount)
	{
		++columnRowCount;
	}

	float unitSize = 1.0f / columnRowCount;

	for (int i = 0; i < iUVIslandCount; ++i)
	{
		UVIslandPtr pUVIsland = uvIslands[i];

		BrushVec3 average_normal(0, 0, 0);
		BrushVec3 average_pos(0, 0, 0);

		if (projectAxis == ePlaneAxis_Average)
		{
			for (int k = 0, iCount(pUVIsland->GetCount()); k < iCount; ++k)
				average_normal += pUVIsland->GetPolygon(k)->GetPlane().Normal();
			if (average_normal.IsZero(kDesignerEpsilon))
				average_normal = BrushVec3(0, 0, 1);
			else
				average_normal.Normalize();
		}
		else if (projectAxis == ePlaneAxis_X)
			average_normal = BrushVec3(1, 0, 0);
		else if (projectAxis == ePlaneAxis_Y)
			average_normal = BrushVec3(0, 1, 0);
		else if (projectAxis == ePlaneAxis_Z)
			average_normal = BrushVec3(0, 0, 1);

		for (int k = 0, iCount(pUVIsland->GetCount()); k < iCount; ++k)
			average_pos += pUVIsland->GetPolygon(k)->GetRepresentativePosition();
		average_pos /= pUVIsland->GetCount();

		BrushPlane plane(average_normal, -average_normal.Dot(average_pos));
		for (int k = 0, iPolygonCount(pUVIsland->GetCount()); k < iPolygonCount; ++k)
			ApplyPlanarProjection(plane, pUVIsland->GetPolygon(k));

		int row = i / columnRowCount;
		int column = i % columnRowCount;

		AABB range(Vec2(column * unitSize, row * unitSize), Vec2((column + 1) * unitSize, (row + 1) * unitSize));
		range.Expand(Vec2(-0.001f, -0.001f));
		range.min.z = range.max.z = 0;
		pUVIsland->Normalize(range);

		pUVIslandMgr->AddUVIsland(pUVIsland);
	}
}

bool Project(const CCamera& camera, const Vec3& p, Vec3& outPos)
{
	Matrix44A mProj, mView;
	Vec4 transformed, projected;

	mathMatrixPerspectiveFov(&mProj, camera.GetFov(), camera.GetProjRatio(), camera.GetNearPlane(), camera.GetFarPlane());
	mathMatrixLookAt(&mView, camera.GetPosition(), camera.GetPosition() + camera.GetViewdir(), camera.GetUp());

	int pViewport[4] = { 0, 0, camera.GetViewSurfaceX(), camera.GetViewSurfaceZ() };

	transformed = Vec4(p, 1) * mView;

	bool visible = transformed.z < 0.0f;

	projected = transformed * mProj;

	if (projected.w == 0.0f)
	{
		outPos = Vec3(0.f, 0.f, 0.f);
		return false;
	}

	projected.x /= projected.w;
	projected.y /= projected.w;
	projected.z /= projected.w;

	outPos = Vec3(projected.x, projected.y, projected.z);
	return true;
}

void CalcTextureBasis(const SBrushPlane<float>& p, Vec3& u, Vec3& v)
{
	Vec3 pvecs[2];

	u.Set(0, 0, 0);
	v.Set(0, 0, 0);

	p.CalcTextureAxis(pvecs[0], pvecs[1]);

	u = pvecs[0];
	v = pvecs[1];

	int sv, tv;
	if (pvecs[0].x)
		sv = 0;
	else if (pvecs[0].y)
		sv = 1;
	else
		sv = 2;

	if (pvecs[1].x)
		tv = 0;
	else if (pvecs[1].y)
		tv = 1;
	else
		tv = 2;

	for (int i = 0; i < 2; i++)
	{
		float ns = pvecs[i][sv];
		float nt = pvecs[i][tv];
		if (!i)
		{
			u[sv] = ns;
			u[tv] = nt;
		}
		else
		{
			v[sv] = ns;
			v[tv] = nt;
		}
	}
}

void CalcTexCoords(const SBrushPlane<float>& p, const Vec3& pos, float& tu, float& tv)
{
	Vec3 tVecx, tVecy;
	CalcTextureBasis(p, tVecx, tVecy);

	tu = pos.Dot(tVecx);
	tv = pos.Dot(tVecy);
}

void FitTexture(const SBrushPlane<float>& plane, const Vec3& vMin, const Vec3& vMax, float tileU, float tileV, TexInfo& outTexInfo)
{
	int i;
	float width, height;
	float rot_width, rot_height;
	float cosv, sinv, ang;
	float min_t, min_s, max_t, max_s;
	float s, t;
	Vec3 vecs[2];
	Vec3 coords[4];

	ang = DEG2RAD(outTexInfo.rotate);
	sinv = sinf(ang);
	cosv = cosf(ang);

	plane.CalcTextureAxis(vecs[0], vecs[1]);

	min_s = vMin | vecs[0];
	min_t = vMin | vecs[1];
	max_s = vMax | vecs[0];
	max_t = vMax | vecs[1];

	width = max_s - min_s;
	height = max_t - min_t;

	coords[0][0] = min_s;
	coords[0][1] = min_t;
	coords[1][0] = max_s;
	coords[1][1] = min_t;
	coords[2][0] = min_s;
	coords[2][1] = max_t;
	coords[3][0] = max_s;
	coords[3][1] = max_t;

	min_s = min_t = 99999;
	max_s = max_t = -99999;

	for (i = 0; i < 4; i++)
	{
		s = cosv * coords[i][0] - sinv * coords[i][1];
		t = sinv * coords[i][0] + cosv * coords[i][1];
		if (i & 1)
		{
			if (s > max_s)
				max_s = s;
		}
		else
		{
			if (s < min_s)
				min_s = s;

			if (i < 2)
			{
				if (t < min_t)
					min_t = t;
			}
			else
			{
				if (t > max_t)
					max_t = t;
			}
		}
	}

	rot_width = (max_s - min_s);
	rot_height = (max_t - min_t);

	float lastTileU = GetAdjustedFloatToAvoidZero(tileU);
	float lastTileV = GetAdjustedFloatToAvoidZero(tileV);

	outTexInfo.scale[0] = -rot_width / lastTileU;
	outTexInfo.scale[1] = -rot_height / lastTileV;

	float lastScaleU = GetAdjustedFloatToAvoidZero(outTexInfo.scale[0]);
	float lastScaleV = GetAdjustedFloatToAvoidZero(outTexInfo.scale[1]);

	outTexInfo.shift[0] = min_s / lastScaleU;
	outTexInfo.shift[1] = min_t / lastScaleV;
}

float CalculateSphericalRadius(Model* pModel)
{
	BrushVec3 center = pModel->GetBoundBox().GetCenter();
	float radius = 0;

	MODEL_SHELF_RECONSTRUCTOR(pModel);
	for (int a = eShelf_Base; a < cShelfMax; ++a)
	{
		pModel->SetShelf(static_cast<ShelfID>(a));
		for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr polygon = pModel->GetPolygon(i);
			for (int k = 0, iVertexCount(polygon->GetVertexCount()); k < iVertexCount; ++k)
			{
				float candidateRadius = (float)polygon->GetPos(k).GetDistance(center);
				if (candidateRadius > radius)
					radius = candidateRadius;
			}
		}
	}

	radius = std::max(0.0001f, radius);
	return radius;
}

void CalculateCylinderRadiusHeight(Model* pModel, float& outRadius, float& outHeight)
{
	AABB aabb = pModel->GetBoundBox();
	Vec3 bottom_center = (aabb.min + Vec3(aabb.max.x, aabb.max.y, aabb.min.z)) * 0.5f;
	outHeight = aabb.max.z - aabb.min.z;
	outRadius = 0;

	MODEL_SHELF_RECONSTRUCTOR(pModel);
	for (int a = eShelf_Base; a < cShelfMax; ++a)
	{
		pModel->SetShelf(static_cast<ShelfID>(a));
		for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr polygon = pModel->GetPolygon(i);
			for (int k = 0, iVertexCount(polygon->GetVertexCount()); k < iVertexCount; ++k)
			{
				Vec3 pos = ToVec3(polygon->GetPos(k));
				float candidateRadius = bottom_center.GetDistance(Vec3(pos.x, pos.y, bottom_center.z));
				if (candidateRadius > outRadius)
					outRadius = candidateRadius;
			}
		}
	}

	outHeight = std::max(0.0001f, outHeight);
	outRadius = std::max(0.0001f, outRadius);
}

void MakeUVIslandsFromSelectedElements(std::vector<UVIslandPtr>& outUVIslands)
{
	DesignerSession* pSession = DesignerSession::GetInstance();

	ElementSet* pSelected = pSession->GetSelectedElements();
	std::vector<PolygonPtr> polygons = pSelected->GetAllPolygons();

	if (polygons.empty())
		return;

	AssignMatIDToSelectedPolygons(pSession->GetCurrentSubMatID());

	std::set<int> usedPolygonIndices;

	int i = 0;
	int iPolygonCount = polygons.size();
	bool bExpanded = false;

	while (usedPolygonIndices.size() < iPolygonCount)
	{
		if (usedPolygonIndices.find(i) == usedPolygonIndices.end())
		{
			UVIslandPtr pUVIsland = new UVIsland;
			pUVIsland->AddPolygon(polygons[i]);
			usedPolygonIndices.insert(i);

			do
			{
				bExpanded = false;
				for (int k = 0; k < iPolygonCount; ++k)
				{
					if (i == k || usedPolygonIndices.find(k) != usedPolygonIndices.end())
						continue;

					if (pUVIsland->HasOverlappedEdges(polygons[k]))
					{
						pUVIsland->AddPolygon(polygons[k]);
						usedPolygonIndices.insert(k);
						bExpanded = true;
					}
				}
			}
			while (bExpanded);

			outUVIslands.push_back(pUVIsland);
		}
		++i;
	}
}
}
}

