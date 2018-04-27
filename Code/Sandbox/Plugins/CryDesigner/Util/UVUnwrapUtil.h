// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"
#include "Core/UVIslandManager.h"
#include "Tools/ToolCommon.h"

namespace Designer
{
class Model;

namespace UnwrapUtil
{
void  ApplyPlanarProjection(const BrushPlane& plane, PolygonPtr polygon);
void  ApplySphereProjection(const BrushVec3& origin, float radius, PolygonPtr polygon);
void  ApplyCylinderProjection(const BrushVec3& bottom_origin, float bottom_radius, float height, PolygonPtr polygon);
void  ApplyCameraProjection(const CCamera& camera, const BrushMatrix34& worldTM, PolygonPtr polygon);
void  PackUVIslands(UVIslandManager* pUVIslandMgr, std::vector<UVIslandPtr>& uvIslands, EPlaneAxis projectAxis);
bool  Project(const CCamera& camera, const Vec3& p, Vec3& outPos);
void  CalcTextureBasis(const SBrushPlane<float>& p, Vec3& u, Vec3& v);
void  CalcTexCoords(const SBrushPlane<float>& p, const Vec3& pos, float& tu, float& tv);
void  FitTexture(const SBrushPlane<float>& plane, const Vec3& vMin, const Vec3& vMax, float tileU, float tileV, TexInfo& outTexInfo);
void  MakeUVIslandsFromSelectedElements(std::vector<UVIslandPtr>& outUVIslands);
float CalculateSphericalRadius(Model* pModel);
void  CalculateCylinderRadiusHeight(Model* pModel, float& outRadius, float& outHeight);
}
}

