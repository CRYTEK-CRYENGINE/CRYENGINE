// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "3dEngine.h"
#include "PolygonClipContext.h"
#include "RoadRenderNode.h"
#include "terrain.h"
#include "ObjMan.h"
#include "MeshCompiler/MeshCompiler.h"

#include "CryCore/TypeInfo_impl.h"

const float fRoadAreaZRange = 2.5f;
const float fRoadTerrainZOffset = 0.06f;

// tmp buffers used during road mesh creation
PodArray<Vec3> CRoadRenderNode::s_tempVertexPositions;
PodArray<vtx_idx> CRoadRenderNode::s_tempIndices;
PodArray<SPipTangents> CRoadRenderNode::s_tempTangents;
PodArray<SVF_P3F_C4B_T2S> CRoadRenderNode::s_tempVertices;
CPolygonClipContext CRoadRenderNode::s_tmpClipContext;
ILINE Vec3 max(const Vec3& v0, const Vec3& v1) { return Vec3(max(v0.x, v1.x), max(v0.y, v1.y), max(v0.z, v1.z)); }
ILINE Vec3 min(const Vec3& v0, const Vec3& v1) { return Vec3(min(v0.x, v1.x), min(v0.y, v1.y), min(v0.z, v1.z)); }

STRUCT_INFO_BEGIN(CRoadRenderNode::SData)
STRUCT_VAR_INFO(arrTexCoors, TYPE_ARRAY(2, TYPE_INFO(float)))
STRUCT_VAR_INFO(arrTexCoorsGlobal, TYPE_ARRAY(2, TYPE_INFO(float)))

STRUCT_VAR_INFO(worldSpaceBBox, TYPE_INFO(AABB))

STRUCT_VAR_INFO(numVertices, TYPE_INFO(uint32))
STRUCT_VAR_INFO(numIndices, TYPE_INFO(uint32))
STRUCT_VAR_INFO(numTangents, TYPE_INFO(uint32))

STRUCT_VAR_INFO(physicsGeometryCount, TYPE_INFO(uint32))

STRUCT_VAR_INFO(sourceVertexCount, TYPE_INFO(uint32))
STRUCT_INFO_END(CRoadRenderNode::SData)

STRUCT_INFO_BEGIN(CRoadRenderNode::SPhysicsGeometryParams)
STRUCT_VAR_INFO(size, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(pos, TYPE_INFO(Vec3))
STRUCT_VAR_INFO(q, TYPE_INFO(Quat))
STRUCT_INFO_END(CRoadRenderNode::SPhysicsGeometryParams)

CRoadRenderNode::CRoadRenderNode()
	: m_bRebuildFull(false)
{
	m_pRenderMesh = NULL;
	m_pMaterial = NULL;
	m_serializedData.arrTexCoors[0] = m_serializedData.arrTexCoorsGlobal[0] = 0;
	m_serializedData.arrTexCoors[1] = m_serializedData.arrTexCoorsGlobal[1] = 1;
	m_pPhysEnt = NULL;
	m_sortPrio = 0;
	m_nLayerId = 0;
	m_bIgnoreTerrainHoles = false;
	m_bPhysicalize = false;

	GetInstCount(GetRenderNodeType())++;
}

CRoadRenderNode::~CRoadRenderNode()
{
	Dephysicalize();
	m_pRenderMesh = NULL;
	Get3DEngine()->FreeRenderNodeState(this);

	Get3DEngine()->m_lstRoadRenderNodesForUpdate.Delete(this);

	GetInstCount(GetRenderNodeType())--;
}

// Sets vertices, texture coordinates and updates the bbox.
// Only called when actually modifying the road, should never be called in Launcher normally.
void CRoadRenderNode::SetVertices(const Vec3* pVertsAll, int nVertsNumAll,
                                  float fTexCoordBegin, float fTexCoordEnd,
                                  float fTexCoordBeginGlobal, float fTexCoordEndGlobal)
{
	if (pVertsAll != m_arrVerts.GetElements())
	{
		m_arrVerts.Clear();
		m_arrVerts.AddList((Vec3*)pVertsAll, nVertsNumAll);

		// work around for cracks between road segments
		if (m_arrVerts.Count() >= 4)
		{
			if (fTexCoordBegin != fTexCoordBeginGlobal)
			{
				Vec3 m0 = (m_arrVerts[0] + m_arrVerts[1]) * .5f;
				Vec3 m1 = (m_arrVerts[2] + m_arrVerts[3]) * .5f;
				Vec3 vDir = (m0 - m1).GetNormalized() * 0.01f;
				m_arrVerts[0] += vDir;
				m_arrVerts[1] += vDir;
			}

			if (fTexCoordEnd != fTexCoordEndGlobal)
			{
				int n = m_arrVerts.Count();
				Vec3 m0 = (m_arrVerts[n - 1] + m_arrVerts[n - 2]) * .5f;
				Vec3 m1 = (m_arrVerts[n - 3] + m_arrVerts[n - 4]) * .5f;
				Vec3 vDir = (m0 - m1).GetNormalized() * 0.01f;
				m_arrVerts[n - 1] += vDir;
				m_arrVerts[n - 2] += vDir;
			}
		}
	}

	// Adjust uv coords to smaller range to avoid f16 precision errors.
	// Need to adjust global ranges too to keep relative fades at ends of spline
	const float fNodeStart = static_cast<float>(static_cast<int>(fTexCoordBegin));
	const float fNodeOffset = fTexCoordBegin - fNodeStart;

	m_serializedData.arrTexCoors[0] = fNodeOffset;
	m_serializedData.arrTexCoors[1] = fNodeOffset + (fTexCoordEnd - fTexCoordBegin);

	m_serializedData.arrTexCoorsGlobal[0] = fTexCoordBeginGlobal - fNodeStart;
	m_serializedData.arrTexCoorsGlobal[1] = fTexCoordBeginGlobal + (fTexCoordEndGlobal - fTexCoordBeginGlobal) - fNodeStart;

	m_serializedData.worldSpaceBBox.Reset();
	for (int i = 0; i < nVertsNumAll; i++)
		m_serializedData.worldSpaceBBox.Add(m_arrVerts[i]);

	m_serializedData.worldSpaceBBox.min -= Vec3(0.1f, 0.1f, fRoadAreaZRange);
	m_serializedData.worldSpaceBBox.max += Vec3(0.1f, 0.1f, fRoadAreaZRange);

	// Query rebuild, results in CRoadRenderNode::Compile being called
	// In this case the road vertices were changed, schedule a full rebuild.
	ScheduleRebuild(true);
}

void CRoadRenderNode::Compile() PREFAST_SUPPRESS_WARNING(6262) //function uses > 32k stack space
{
	LOADING_TIME_PROFILE_SECTION;

	// free old object and mesh
	m_pRenderMesh = NULL;

	// Make sure to dephysicalize first
	Dephysicalize();

	// The process of generating the render mesh is very slow, only perform if the road changed!
	if (m_bRebuildFull)
	{
		Plane arrPlanes[6];
		float arrTexCoors[2];

		int nVertsNumAll = m_arrVerts.Count();

		assert(!(nVertsNumAll & 1));

		if (nVertsNumAll < 4)
			return;

		m_serializedData.worldSpaceBBox.Reset();
		for (int i = 0; i < nVertsNumAll; i++)
		{
			Vec3 vTmp(m_arrVerts[i].x, m_arrVerts[i].y, Get3DEngine()->GetTerrainElevation(m_arrVerts[i].x, m_arrVerts[i].y) + fRoadTerrainZOffset);
			m_serializedData.worldSpaceBBox.Add(vTmp);
		}

		// prepare arrays for final mesh
		const int nMaxVerticesToMerge = 1024 * 32; // limit memory usage
		m_dynamicData.vertices.PreAllocate(nMaxVerticesToMerge, 0);
		m_dynamicData.indices.PreAllocate(nMaxVerticesToMerge * 6, 0);
		m_dynamicData.tangents.PreAllocate(nMaxVerticesToMerge, 0);

		float fChunksNum = (float)((nVertsNumAll - 2) / 2);
		float fTexStep = (m_serializedData.arrTexCoors[1] - m_serializedData.arrTexCoors[0]) / fChunksNum;

		// for every trapezoid
		for (int nVertId = 0; nVertId <= nVertsNumAll - 4; nVertId += 2)
		{
			const Vec3* pVerts = &m_arrVerts[nVertId];

			if (pVerts[0] == pVerts[1] ||
			    pVerts[1] == pVerts[2] ||
			    pVerts[2] == pVerts[3] ||
			    pVerts[3] == pVerts[0])
				continue;

			// get texture coordinates range
			arrTexCoors[0] = m_serializedData.arrTexCoors[0] + fTexStep * (nVertId / 2);
			arrTexCoors[1] = m_serializedData.arrTexCoors[0] + fTexStep * (nVertId / 2 + 1);

			GetClipPlanes(&arrPlanes[0], 4, nVertId);

			// make trapezoid 2d bbox
			AABB WSBBox;
			WSBBox.Reset();
			for (int i = 0; i < 4; i++)
			{
				Vec3 vTmp(pVerts[i].x, pVerts[i].y, pVerts[i].z);
				WSBBox.Add(vTmp);
			}

			// make vert array
			float unitSize = GetTerrain()->GetHeightMapUnitSize();

			float x1 = std::floor(WSBBox.min.x / unitSize) * unitSize;
			float x2 = std::floor(WSBBox.max.x / unitSize) * unitSize + unitSize;
			float y1 = std::floor(WSBBox.min.y / unitSize) * unitSize;
			float y2 = std::floor(WSBBox.max.y / unitSize) * unitSize + unitSize;

			// make arrays of verts and indices used in trapezoid area
			s_tempVertexPositions.Clear();
			s_tempIndices.Clear();

			for (float x = x1; x <= x2; x += unitSize)
			{
				for (float y = y1; y <= y2; y += unitSize)
				{
					s_tempVertexPositions.Add(Vec3((float)x, (float)y, GetTerrain()->GetZ(x, y, true)));
				}
			}

			// make indices
			int dx = int((x2 - x1) / unitSize);
			int dy = int((y2 - y1) / unitSize);

			for (int x = 0; x < dx; x++)
			{
				for (int y = 0; y < dy; y++)
				{
					int nIdx0 = (x * (dy + 1) + y);
					int nIdx1 = (x * (dy + 1) + y + (dy + 1));
					int nIdx2 = (x * (dy + 1) + y + 1);
					int nIdx3 = (x * (dy + 1) + y + 1 + (dy + 1));

					float X_in_meters = float(x1) + float(x) * unitSize;
					float Y_in_meters = float(y1) + float(y) * unitSize;

					CTerrain* pTerrain = GetTerrain();

					if (m_bIgnoreTerrainHoles || (pTerrain && !pTerrain->GetHole(X_in_meters, Y_in_meters)))
					{
						if (pTerrain && pTerrain->IsMeshQuadFlipped(X_in_meters, Y_in_meters, unitSize))
						{
							s_tempIndices.Add(nIdx0);
							s_tempIndices.Add(nIdx1);
							s_tempIndices.Add(nIdx3);

							s_tempIndices.Add(nIdx0);
							s_tempIndices.Add(nIdx3);
							s_tempIndices.Add(nIdx2);
						}
						else
						{
							s_tempIndices.Add(nIdx0);
							s_tempIndices.Add(nIdx1);
							s_tempIndices.Add(nIdx2);

							s_tempIndices.Add(nIdx1);
							s_tempIndices.Add(nIdx3);
							s_tempIndices.Add(nIdx2);
						}
					}
				}
			}

			// clip triangles
			int nOrigCount = s_tempIndices.Count();
			for (int i = 0; i < nOrigCount; i += 3)
			{
				if (ClipTriangle(s_tempVertexPositions, s_tempIndices, i, arrPlanes))
				{
					i -= 3;
					nOrigCount -= 3;
				}
			}

			if (s_tempIndices.Count() < 3 || s_tempVertexPositions.Count() < 3)
				continue;

			if (m_bPhysicalize)
			{
				Vec3 axis = (pVerts[2] + pVerts[3] - pVerts[0] - pVerts[1]).normalized(), n = (pVerts[1] - pVerts[0] ^ pVerts[2] - pVerts[0]) + (pVerts[2] - pVerts[3] ^ pVerts[2] - pVerts[3]);
				(n -= axis * (n * axis)).normalize();

				SPhysicsGeometryParams physParams;

				Vec3 bbox[2];
				bbox[0] = VMAX;
				bbox[1] = VMIN;

				physParams.q = Quat(Matrix33::CreateFromVectors(axis, n ^ axis, n));

				for (int j = 0; j < s_tempIndices.Count(); j++)
				{
					Vec3 ptloc = s_tempVertexPositions[s_tempIndices[j]] * physParams.q;
					bbox[0] = min(bbox[0], ptloc);
					bbox[1] = max(bbox[1], ptloc);
				}

				physParams.pos = physParams.q * (bbox[1] + bbox[0]) * 0.5f;
				physParams.size = (bbox[1] - bbox[0]) * 0.5f;

				m_dynamicData.physicsGeometry.Add(physParams);
			}

			// allocate tangent array
			s_tempTangents.Clear();
			s_tempTangents.PreAllocate(s_tempVertexPositions.Count(), s_tempVertexPositions.Count());

			Vec3 vWSBoxCenter = m_serializedData.worldSpaceBBox.GetCenter(); //vWSBoxCenter.z=0;

			// make real vertex data
			s_tempVertices.Clear();
			for (int i = 0; i < s_tempVertexPositions.Count(); i++)
			{
				SVF_P3F_C4B_T2S tmp;

				Vec3 vWSPos = s_tempVertexPositions[i];

				tmp.xyz = (vWSPos - vWSBoxCenter);

				// do texgen
				float d0 = arrPlanes[0].DistFromPlane(vWSPos);
				float d1 = arrPlanes[1].DistFromPlane(vWSPos);
				float d2 = arrPlanes[2].DistFromPlane(vWSPos);
				float d3 = arrPlanes[3].DistFromPlane(vWSPos);

				float t = fabsf(d0 + d1) < FLT_EPSILON ? 0.0f : d0 / (d0 + d1);
				tmp.st = Vec2f16((1 - t) * fabs(arrTexCoors[0]) + t * fabs(arrTexCoors[1]), fabsf(d2 + d3) < FLT_EPSILON ? 0.0f : d2 / (d2 + d3));

				// calculate alpha value
				float fAlpha = 1.f;
				if (fabs(arrTexCoors[0] - m_serializedData.arrTexCoorsGlobal[0]) < 0.01f)
					fAlpha = CLAMP(t, 0, 1.f);
				else if (fabs(arrTexCoors[1] - m_serializedData.arrTexCoorsGlobal[1]) < 0.01f)
					fAlpha = CLAMP(1.f - t, 0, 1.f);

				tmp.color.bcolor[0] = 255;
				tmp.color.bcolor[1] = 255;
				tmp.color.bcolor[2] = 255;
				tmp.color.bcolor[3] = uint8(255.f * fAlpha);
				SwapEndian(tmp.color.dcolor, eLittleEndian);

				s_tempVertices.Add(tmp);

				Vec3 vNormal = GetTerrain()->GetTerrainSurfaceNormal(vWSPos, 0.25f);

				Vec3 vBiTang = pVerts[1] - pVerts[0];
				vBiTang.Normalize();

				Vec3 vTang = pVerts[2] - pVerts[0];
				vTang.Normalize();

				vBiTang = -vNormal.Cross(vTang);
				vTang = vNormal.Cross(vBiTang);

				s_tempTangents[i] = SPipTangents(vTang, vBiTang, -1);
			}

			// shift indices
			for (int i = 0; i < s_tempIndices.Count(); i++)
				s_tempIndices[i] += m_dynamicData.vertices.size();

			if (m_dynamicData.vertices.size() + s_tempVertices.Count() > nMaxVerticesToMerge)
			{
				Warning("Road object is too big, has to be split into several smaller parts (pos=%d,%d,%d)", (int)m_serializedData.worldSpaceBBox.GetCenter().x, (int)m_serializedData.worldSpaceBBox.GetCenter().y, (int)m_serializedData.worldSpaceBBox.GetCenter().z);
				return;
			}

			m_dynamicData.indices.AddList(s_tempIndices);
			m_dynamicData.vertices.AddList(s_tempVertices);
			m_dynamicData.tangents.AddList(s_tempTangents);
		}

		PodArray<SPipNormal> dummyNormals;

		mesh_compiler::CMeshCompiler meshCompiler;
		meshCompiler.WeldPos_VF_P3X(m_dynamicData.vertices, m_dynamicData.tangents, dummyNormals, m_dynamicData.indices, VEC_EPSILON, GetBBox());
	}

	// make render mesh
	if (m_dynamicData.indices.Count() && GetRenderer())
	{
		m_pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
		  m_dynamicData.vertices.GetElements(), m_dynamicData.vertices.Count(), EDefaultInputLayouts::P3F_C4B_T2S,
		  m_dynamicData.indices.GetElements(), m_dynamicData.indices.Count(), prtTriangleList,
		  "RoadRenderNode", GetName(), eRMT_Static, 1, 0, NULL, NULL, false, true, m_dynamicData.tangents.GetElements());

		// Calculate the texel area density
		float texelAreaDensity = 1.0f;

		const size_t indexCount = m_dynamicData.indices.size();
		const size_t vertexCount = m_dynamicData.vertices.size();

		if ((indexCount > 0) && (vertexCount > 0))
		{
			float posArea;
			float texArea;
			const char* errorText = "";

			const bool ok = CMeshHelpers::ComputeTexMappingAreas(
			  indexCount, &m_dynamicData.indices[0],
			  vertexCount,
			  &m_dynamicData.vertices[0].xyz, sizeof(m_dynamicData.vertices[0]),
			  &m_dynamicData.vertices[0].st, sizeof(m_dynamicData.vertices[0]),
			  posArea, texArea, errorText);

			if (ok)
			{
				texelAreaDensity = texArea / posArea;
			}
			else
			{
				gEnv->pLog->LogError("Failed to compute texture mapping density for mesh '%s': %s", GetName(), errorText);
			}
		}

		m_pRenderMesh->SetChunk((m_pMaterial != NULL) ? (IMaterial*)m_pMaterial : gEnv->p3DEngine->GetMaterialManager()->GetDefaultMaterial(),
		                        0, m_dynamicData.vertices.Count(), 0, m_dynamicData.indices.Count(), texelAreaDensity);
		Vec3 vWSBoxCenter = m_serializedData.worldSpaceBBox.GetCenter(); //vWSBoxCenter.z=0;
		AABB OSBBox(m_serializedData.worldSpaceBBox.min - vWSBoxCenter, m_serializedData.worldSpaceBBox.max - vWSBoxCenter);
		m_pRenderMesh->SetBBox(OSBBox.min, OSBBox.max);

		if (m_bPhysicalize)
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("RoadPhysicalization");

			IGeomManager* pGeoman = GetPhysicalWorld()->GetGeomManager();
			primitives::box abox;
			pe_geomparams gp;
			int matid = 0;
			abox.center.zero();
			abox.Basis.SetIdentity();
			gp.flags = geom_mat_substitutor;
			pe_params_flags pf;
			pf.flagsAND = ~pef_traceable;
			pf.flagsOR = pef_parts_traceable;
			m_pPhysEnt = GetPhysicalWorld()->CreatePhysicalEntity(PE_STATIC, &pf);

			if (m_pMaterial && m_pPhysEnt)
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("RoadPhysicalizationParts");

				ISurfaceType* psf;
				if ((psf = m_pMaterial->GetSurfaceType()) || m_pMaterial->GetSubMtl(0) && (psf = m_pMaterial->GetSubMtl(0)->GetSurfaceType()))
					matid = psf->GetId();

				phys_geometry* pPhysGeom;

				for (int i = 0, numParts = m_dynamicData.physicsGeometry.Count(); i < numParts; i++)
				{
					gp.q = m_dynamicData.physicsGeometry[i].q;
					gp.pos = m_dynamicData.physicsGeometry[i].pos;

					abox.size = m_dynamicData.physicsGeometry[i].size;

					pPhysGeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::box::type, &abox), matid);
					pPhysGeom->pGeom->Release();
					m_pPhysEnt->AddGeometry(pPhysGeom, &gp);
					pGeoman->UnregisterGeometry(pPhysGeom);
				}
			}
		}
	}

	// activate rendering
	Get3DEngine()->RegisterEntity(this);
}

void CRoadRenderNode::SetSortPriority(uint8 sortPrio)
{
	m_sortPrio = sortPrio;
}

void CRoadRenderNode::SetIgnoreTerrainHoles(bool bVal)
{
	m_bIgnoreTerrainHoles = bVal;
}

void CRoadRenderNode::Render(const SRendParams& RendParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!passInfo.RenderRoads())
		return; // false;

	// Prepare object model matrix
	Vec3 vWSBoxCenter = m_serializedData.worldSpaceBBox.GetCenter();
	vWSBoxCenter.z += 0.01f;
	const auto objMat = Matrix34::CreateTranslationMat(vWSBoxCenter);

	CRenderObject* pObj = nullptr;
	if (GetObjManager()->AddOrCreatePersistentRenderObject(m_pTempData.load(), pObj, nullptr, objMat, passInfo))
		return;

	pObj->m_pRenderNode = this;
	pObj->m_ObjFlags |= RendParams.dwFObjFlags;
	pObj->SetAmbientColor(RendParams.AmbientColor, passInfo);
	pObj->m_editorSelectionID = m_nEditorSelectionID;

	//RendParams.nRenderList = EFSLIST_DECAL;

	pObj->m_nSort = m_sortPrio;

	//	if (RendParams.pShadowMapCasters)
	pObj->m_ObjFlags |= (FOB_DECAL | FOB_INSHADOW | FOB_NO_FOG | FOB_TRANS_TRANSLATE);

	if (RendParams.pTerrainTexInfo && (RendParams.dwFObjFlags & (FOB_BLEND_WITH_TERRAIN_COLOR /* | FOB_AMBIENT_OCCLUSION*/)))
	{
		SRenderObjData* pOD = pObj->GetObjData();
		pOD->m_pTerrainSectorTextureInfo = RendParams.pTerrainTexInfo;
		pOD->m_fMaxViewDistance = m_fWSMaxViewDist;

		pObj->m_nTextureID = RendParams.pTerrainTexInfo->nTex0;
		//pObj->m_nTextureID1 = _RendParams.pTerrainTexInfo->nTex1;
		pOD->m_fTempVars[0] = RendParams.pTerrainTexInfo->fTexOffsetX;
		pOD->m_fTempVars[1] = RendParams.pTerrainTexInfo->fTexOffsetY;
		pOD->m_fTempVars[2] = RendParams.pTerrainTexInfo->fTexScale;
		pOD->m_fTempVars[3] = 0;
		pOD->m_fTempVars[4] = 0;
	}

	if (m_pRenderMesh)
	{
		pObj->m_pCurrMaterial = m_pMaterial;
		m_pRenderMesh->Render(pObj, passInfo);
	}
}

bool CRoadRenderNode::ClipTriangle(PodArray<Vec3>& lstVerts, PodArray<vtx_idx>& lstInds, int nStartIdxId, Plane* pPlanes)
{
	const PodArray<Vec3>& clipped = s_tmpClipContext.Clip(
	  lstVerts[lstInds[nStartIdxId + 0]],
	  lstVerts[lstInds[nStartIdxId + 1]],
	  lstVerts[lstInds[nStartIdxId + 2]],
	  pPlanes, 4);

	if (clipped.Count() < 3)
	{
		lstInds.Delete(nStartIdxId, 3);
		return true; // entire triangle is clipped away
	}

	if (clipped.Count() == 3)
		if (clipped[0].IsEquivalent(lstVerts[lstInds[nStartIdxId + 0]]))
			if (clipped[1].IsEquivalent(lstVerts[lstInds[nStartIdxId + 1]]))
				if (clipped[2].IsEquivalent(lstVerts[lstInds[nStartIdxId + 2]]))
					return false;
	// entire triangle is in

	// replace old triangle with several new triangles
	int nStartId = lstVerts.Count();
	lstVerts.AddList(clipped);

	// put first new triangle into position of original one
	lstInds[nStartIdxId + 0] = nStartId + 0;
	lstInds[nStartIdxId + 1] = nStartId + 1;
	lstInds[nStartIdxId + 2] = nStartId + 2;

	// put others in the end
	for (int i = 1; i < clipped.Count() - 2; i++)
	{
		lstInds.Add(nStartId + 0);
		lstInds.Add(nStartId + i + 1);
		lstInds.Add(nStartId + i + 2);
	}

	return false;
}

void CRoadRenderNode::SetMaterial(IMaterial* pMat)
{
	m_pMaterial = pMat;
}

void CRoadRenderNode::Dephysicalize(bool bKeepIfReferenced)
{
	if (m_pPhysEnt)
		GetPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt);
	m_pPhysEnt = NULL;
}

void CRoadRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "Road");
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_arrVerts);

	pSizer->AddObject(m_dynamicData.vertices);
	pSizer->AddObject(m_dynamicData.indices);
	pSizer->AddObject(m_dynamicData.tangents);
}

void CRoadRenderNode::ScheduleRebuild(bool bFullRebuild)
{
	m_bRebuildFull = bFullRebuild;

	if (Get3DEngine()->m_lstRoadRenderNodesForUpdate.Find(this) < 0)
		Get3DEngine()->m_lstRoadRenderNodesForUpdate.Add(this);
}

void CRoadRenderNode::OnTerrainChanged()
{
	if (!m_pRenderMesh)
		return;

	int nPosStride = 0;

	IRenderMesh::ThreadAccessLock lock(m_pRenderMesh);

	byte* pPos = m_pRenderMesh->GetPosPtr(nPosStride, FSL_SYSTEM_UPDATE);

	Vec3 vWSBoxCenter = m_serializedData.worldSpaceBBox.GetCenter(); //vWSBoxCenter.z=0;

	for (int i = 0, nVertsNum = m_pRenderMesh->GetVerticesCount(); i < nVertsNum; i++)
	{
		Vec3& vPos = *(Vec3*)&pPos[i * nPosStride];
		vPos.z = GetTerrain()->GetZApr(vWSBoxCenter.x + vPos.x, vWSBoxCenter.y + vPos.y) + 0.01f - vWSBoxCenter.z;
	}
	m_pRenderMesh->UnlockStream(VSF_GENERAL);

	// Terrain changed, schedule a full rebuild of the road to match
	ScheduleRebuild(true);
}

void CRoadRenderNode::GetTexCoordInfo(float* pTexCoordInfo)
{
	pTexCoordInfo[0] = m_serializedData.arrTexCoors[0];
	pTexCoordInfo[1] = m_serializedData.arrTexCoors[1];
	pTexCoordInfo[2] = m_serializedData.arrTexCoorsGlobal[0];
	pTexCoordInfo[3] = m_serializedData.arrTexCoorsGlobal[1];
}

void CRoadRenderNode::GetClipPlanes(Plane* pPlanes, int nPlanesNum, int nVertId)
{
	const Vec3* pVerts = &m_arrVerts[nVertId];

	if (
	  pVerts[0] == pVerts[1] ||
	  pVerts[1] == pVerts[2] ||
	  pVerts[2] == pVerts[3] ||
	  pVerts[3] == pVerts[0])
		return;

	assert(nPlanesNum == 4 || nPlanesNum == 6);

	// define 6 clip planes
	pPlanes[0].SetPlane(pVerts[0], pVerts[1], pVerts[1] + Vec3(0, 0, 1));
	pPlanes[1].SetPlane(pVerts[2], pVerts[3], pVerts[3] + Vec3(0, 0, -1));
	pPlanes[2].SetPlane(pVerts[0], pVerts[2], pVerts[2] + Vec3(0, 0, -1));
	pPlanes[3].SetPlane(pVerts[1], pVerts[3], pVerts[3] + Vec3(0, 0, 1));

	if (nPlanesNum == 6)
	{
		Vec3 vHeight(0, 0, fRoadAreaZRange);
		pPlanes[4].SetPlane(pVerts[0] - vHeight, pVerts[1] - vHeight, pVerts[2] - vHeight);
		pPlanes[5].SetPlane(pVerts[1] + vHeight, pVerts[0] + vHeight, pVerts[2] + vHeight);
	}
}

void CRoadRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_serializedData.worldSpaceBBox.Move(delta);
}

///////////////////////////////////////////////////////////////////////////////
void CRoadRenderNode::FillBBox(AABB& aabb)
{
	aabb = CRoadRenderNode::GetBBox();
}

EERType CRoadRenderNode::GetRenderNodeType()
{
	return eERType_Road;
}

float CRoadRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, CRoadRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_ViewDistMin, CRoadRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized());
}

Vec3 CRoadRenderNode::GetPos(bool) const
{
	return m_serializedData.worldSpaceBBox.GetCenter();
}

IMaterial* CRoadRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}

bool CRoadRenderNode::CanExecuteRenderAsJob()
{
	return !gEnv->IsEditor() && GetCVars()->e_ExecuteRenderAsJobMask & BIT(GetRenderNodeType());
}
