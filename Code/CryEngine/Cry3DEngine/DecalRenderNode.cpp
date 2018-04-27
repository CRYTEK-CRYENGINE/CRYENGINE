// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DecalRenderNode.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "MatMan.h"
#include "terrain.h"

int CDecalRenderNode::m_nFillBigDecalIndicesCounter = 0;

CDecalRenderNode::CDecalRenderNode()
	: m_pos(0, 0, 0)
	, m_localBounds(Vec3(-1, -1, -1), Vec3(1, 1, 1))
	, m_pOverrideMaterial(NULL)
	, m_pMaterial(NULL)
	, m_updateRequested(false)
	, m_decalProperties()
	, m_decals()
	, m_nLastRenderedFrameId(0)
	, m_nLayerId(0)
	, m_physEnt(0)
{
	GetInstCount(GetRenderNodeType())++;

	m_Matrix.SetIdentity();
}

CDecalRenderNode::~CDecalRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	DeleteDecals();
	Get3DEngine()->FreeRenderNodeState(this);
	if (m_physEnt)
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_physEnt);
}

const SDecalProperties* CDecalRenderNode::GetDecalProperties() const
{
	return &m_decalProperties;
}

void CDecalRenderNode::DeleteDecals()
{
	for (size_t i(0); i < m_decals.size(); ++i)
		delete m_decals[i];

	m_decals.resize(0);
}

void CDecalRenderNode::CreatePlanarDecal()
{
	CryEngineDecalInfo decalInfo;

	// necessary params
	decalInfo.vPos = m_decalProperties.m_pos;
	decalInfo.vNormal = m_decalProperties.m_normal;
	decalInfo.fSize = m_decalProperties.m_radius;
	decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
	cry_strcpy(decalInfo.szMaterialName, m_pMaterial->GetName());
	decalInfo.sortPrio = m_decalProperties.m_sortPrio;

	// default for all other
	decalInfo.pIStatObj = NULL;
	decalInfo.ownerInfo.pRenderNode = NULL;
	decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
	decalInfo.vHitDirection = Vec3(0, 0, 0);
	decalInfo.fGrowTime = 0;
	decalInfo.preventDecalOnGround = true;
	decalInfo.fAngle = 0;

	CDecal* pDecal(new CDecal);
	if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
		m_decals.push_back(pDecal);
	else
		delete pDecal;
}

void CDecalRenderNode::CreateDecalOnStaticObjects()
{
	CTerrain* pTerrain(GetTerrain());
	CVisAreaManager* pVisAreaManager(GetVisAreaManager());
	PodArray<SRNInfo> decalReceivers;

	if (pTerrain && m_pOcNode && !m_pOcNode->GetVisArea())
		pTerrain->GetObjectsAround(m_decalProperties.m_pos, m_decalProperties.m_radius, &decalReceivers, true, false);
	else if (pVisAreaManager && m_pOcNode && m_pOcNode->GetVisArea())
		pVisAreaManager->GetObjectsAround(m_decalProperties.m_pos, m_decalProperties.m_radius, &decalReceivers, true);

	// delete vegetations
	for (int nRecId(0); nRecId < decalReceivers.Count(); ++nRecId)
	{
		EERType eType = decalReceivers[nRecId].pNode->GetRenderNodeType();
		if (eType != eERType_Brush && eType != eERType_MovableBrush)
		{
			decalReceivers.DeleteFastUnsorted(nRecId);
			nRecId--;
		}
	}

	for (int nRecId(0); nRecId < decalReceivers.Count(); ++nRecId)
	{
		CryEngineDecalInfo decalInfo;

		// necessary params
		decalInfo.vPos = m_decalProperties.m_pos;
		decalInfo.vNormal = m_decalProperties.m_normal;
		decalInfo.vHitDirection = -m_decalProperties.m_normal;
		decalInfo.fSize = m_decalProperties.m_radius;
		decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
		cry_strcpy(decalInfo.szMaterialName, m_pMaterial->GetName());
		decalInfo.sortPrio = m_decalProperties.m_sortPrio;

		if (GetCVars()->e_DecalsMerge)
			decalInfo.ownerInfo.pDecalReceivers = &decalReceivers;
		else
			decalInfo.ownerInfo.pRenderNode = decalReceivers[nRecId].pNode;

		decalInfo.ownerInfo.nRenderNodeSlotId = 0;
		decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = -1;

		// default for all other
		decalInfo.pIStatObj = NULL;
		decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
		decalInfo.fGrowTime = 0;
		decalInfo.preventDecalOnGround = false;
		decalInfo.fAngle = 0;

		IStatObj* pMainStatObj = decalInfo.ownerInfo.pRenderNode->GetEntityStatObj();
		if (!pMainStatObj)
			continue;

		// in case of multi-sub-objects, spawn decal on each visible sub-object
		int nSubObjCount = pMainStatObj->GetSubObjectCount();
		if (!GetCVars()->e_DecalsMerge && nSubObjCount)
		{
			for (int nSubObjId = 0; nSubObjId < nSubObjCount; nSubObjId++)
			{
				CStatObj::SSubObject* const __restrict pSubObj = (CStatObj::SSubObject*)pMainStatObj->GetSubObject(nSubObjId);
				if (CStatObj* const __restrict pSubStatObj = (CStatObj*)pSubObj->pStatObj)
					if (pSubObj->nType == STATIC_SUB_OBJECT_MESH && !pSubObj->bHidden && !(pSubStatObj->m_nFlags & STATIC_OBJECT_HIDDEN))
					{
						CDecal* pDecal(new CDecal);
						decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = nSubObjId;

						if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
						{
							m_decals.push_back(pDecal);
							assert(!GetCVars()->e_DecalsMerge || pDecal->m_eDecalType == eDecalType_WS_Merged);
						}
						else
						{
							delete pDecal;
						}
					}
			}
		}
		else
		{
			CDecal* pDecal(new CDecal);
			if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
			{
				m_decals.push_back(pDecal);
				assert(!GetCVars()->e_DecalsMerge || pDecal->m_eDecalType == eDecalType_WS_Merged);
			}
			else
			{
				delete pDecal;
			}
		}

		if (GetCVars()->e_DecalsMerge)
			break;
	}
}

void CDecalRenderNode::CreateDecalOnTerrain()
{
	float terrainHeight(GetTerrain()->GetZApr(m_decalProperties.m_pos.x, m_decalProperties.m_pos.y));
	float terrainDelta(m_decalProperties.m_pos.z - terrainHeight);
	if (terrainDelta < m_decalProperties.m_radius && terrainDelta > -0.5f)
	{
		CryEngineDecalInfo decalInfo;

		// necessary params
		decalInfo.vPos = Vec3(m_decalProperties.m_pos.x, m_decalProperties.m_pos.y, terrainHeight);
		decalInfo.vNormal = Vec3(0, 0, 1);
		decalInfo.vHitDirection = Vec3(0, 0, -1);
		decalInfo.fSize = m_decalProperties.m_radius;// - terrainDelta;
		decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
		cry_strcpy(decalInfo.szMaterialName, m_pMaterial->GetName());
		decalInfo.sortPrio = m_decalProperties.m_sortPrio;

		// default for all other
		decalInfo.pIStatObj = NULL;
		decalInfo.ownerInfo.pRenderNode = 0;
		decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
		decalInfo.fGrowTime = 0;
		decalInfo.preventDecalOnGround = false;
		decalInfo.fAngle = 0;

		CDecal* pDecal(new CDecal);
		if (m_p3DEngine->CreateDecalInstance(decalInfo, pDecal))
			m_decals.push_back(pDecal);
		else
			delete pDecal;
	}
}

void CDecalRenderNode::CreateDecals()
{
	DeleteDecals();

	if (m_decalProperties.m_deferred)
		return;

	IMaterial* pMaterial(GetMaterial());

	assert(0 != pMaterial && "CDecalRenderNode::CreateDecals() -- Invalid Material!");
	if (!pMaterial)
		return;

	switch (m_decalProperties.m_projectionType)
	{
	case SDecalProperties::ePlanar:
		{
			CreatePlanarDecal();
			break;
		}
	case SDecalProperties::eProjectOnStaticObjects:
		{
			CreateDecalOnStaticObjects();
			break;
		}
	case SDecalProperties::eProjectOnTerrain:
		{
			CreateDecalOnTerrain();
			break;
		}
	case SDecalProperties::eProjectOnTerrainAndStaticObjects:
		{
			CreateDecalOnStaticObjects();
			CreateDecalOnTerrain();
			break;
		}
	default:
		{
			assert(!"CDecalRenderNode::CreateDecals() : Unsupported decal projection type!");
			break;
		}
	}
}

void CDecalRenderNode::ProcessUpdateRequest()
{
	if (!m_updateRequested || m_nFillBigDecalIndicesCounter >= GetCVars()->e_DecalsMaxUpdatesPerFrame)
		return;

	CreateDecals();
	m_updateRequested = false;
}

void CDecalRenderNode::UpdateAABBFromRenderMeshes()
{
	if (m_decalProperties.m_projectionType == SDecalProperties::eProjectOnStaticObjects ||
	    m_decalProperties.m_projectionType == SDecalProperties::eProjectOnTerrain ||
	    m_decalProperties.m_projectionType == SDecalProperties::eProjectOnTerrainAndStaticObjects)
	{
		AABB WSBBox;
		WSBBox.Reset();
		for (size_t i(0); i < m_decals.size(); ++i)
		{
			CDecal* pDecal(m_decals[i]);
			if (pDecal && pDecal->m_pRenderMesh && pDecal->m_eDecalType != eDecalType_OS_OwnersVerticesUsed)
			{
				AABB aabb;
				pDecal->m_pRenderMesh->GetBBox(aabb.min, aabb.max);
				if (pDecal->m_eDecalType == eDecalType_WS_Merged || pDecal->m_eDecalType == eDecalType_WS_OnTheGround)
				{
					aabb.min += pDecal->m_vPos;
					aabb.max += pDecal->m_vPos;
				}
				WSBBox.Add(aabb);
			}
		}

		if (!WSBBox.IsReset())
			m_WSBBox = WSBBox;
	}
}

//special check for def decals forcing
bool CDecalRenderNode::CheckForceDeferred()
{
	if (m_pMaterial != NULL)
	{
		SShaderItem& sItem = m_pMaterial->GetShaderItem(0);
		if (sItem.m_pShaderResources != NULL)
		{
			float fCosA = m_decalProperties.m_normal.GetNormalized().Dot(Vec3(0, 0, 1));
			if (fCosA > 0.5f)
				return false;

			if (SEfResTexture* pEnvRes0 = sItem.m_pShaderResources->GetTexture(EFTT_ENV))
			{
				if (pEnvRes0->m_Sampler.m_pITex == NULL)
				{
					m_decalProperties.m_projectionType = SDecalProperties::ePlanar;
					m_decalProperties.m_deferred = true;
					return true;
				}
			}
			else
			{
				m_decalProperties.m_projectionType = SDecalProperties::ePlanar;
				m_decalProperties.m_deferred = true;
				return true;
			}
		}
	}
	return false;
}

void CDecalRenderNode::SetDecalProperties(const SDecalProperties& properties)
{
	// update bounds
	m_localBounds = AABB(-properties.m_radius * Vec3(1, 1, 1), properties.m_radius * Vec3(1, 1, 1));

	// register material
	m_pMaterial = GetMatMan()->LoadMaterial(properties.m_pMaterialName, false);

	// copy decal properties
	m_decalProperties = properties;
	m_decalProperties.m_pMaterialName = 0; // reset this as it's assumed to be a temporary pointer only, refer to m_materialID to get material

	// request update
	m_updateRequested = true;

	bool bForced = 0;

	// TOOD: Revive support for planar decals in new pipeline
	if (properties.m_deferred || (GetCVars()->e_DecalsDeferredStatic
	                              /*&& (m_decalProperties.m_projectionType != SDecalProperties::ePlanar && m_decalProperties.m_projectionType != SDecalProperties::eProjectOnTerrain)*/))
	{
		m_decalProperties.m_projectionType = SDecalProperties::ePlanar;
		m_decalProperties.m_deferred = true;
	}

	if (GetCVars()->e_DecalsForceDeferred)
	{
		if (CheckForceDeferred())
			bForced = true;
	}

	// set matrix
	m_Matrix.SetRotation33(m_decalProperties.m_explicitRightUpFront);
	Matrix33 matScale;
	if (bForced && !properties.m_deferred)
		matScale.SetScale(Vec3(properties.m_radius, properties.m_radius, properties.m_radius * 0.05f));
	else
		matScale.SetScale(Vec3(properties.m_radius, properties.m_radius, properties.m_radius * properties.m_depth));

	m_Matrix = m_Matrix * matScale;
	m_Matrix.SetTranslation(properties.m_pos);

	bool hasPhys = false;
	if (m_pMaterial && m_pMaterial->GetSurfaceTypeId() &&
	    (properties.m_projectionType == SDecalProperties::eProjectOnTerrainAndStaticObjects || properties.m_projectionType == SDecalProperties::eProjectOnTerrain))
	{
		IGeometry* pGeom = 0;
		IGeomManager* pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
		const float thickness = 0.01f;
		geom_world_data gwd[2];
		gwd[0].offset = m_Matrix.GetTranslation();
		gwd[0].v = m_Matrix.TransformVector(Vec3(0, 0, 1));
		primitives::cylinder cyl;

		Vec2_tpl<uint16> sz;
		int* idAtlas;
		if (m_pMaterial->GetFlags() & MTL_FLAG_TRACEABLE_TEXTURE && m_pMaterial->GetShaderItem().m_pShaderResources)
			if (SEfResTexture* pTex = m_pMaterial->GetShaderItem().m_pShaderResources->GetTexture(EFTT_DIFFUSE))
				if (ITexture* pITex = pTex->m_Sampler.m_pITex)
					if (ColorB* data = (ColorB*)pITex->GetLowResSystemCopy(sz.x, sz.y, &idAtlas))
					{
						const int lp = 5;
						Vec2i szKrn((int)sz.x / lp, (int)sz.y / lp), rszKrn((1 << 23) / szKrn.x, (1 << 23) / szKrn.y);
						Vec2 rsz(1.0f / sz.x, 1.0f / sz.y);

						auto RunningSum = [](uint8* psrc, uint8* pdst, int stride, int w, int wk, int rwk, int rwkShift)
						{
							int i, sum;
							uint8* p0 = psrc, * p1 = psrc;
							for (i = 0, sum = 0; i*2 < wk + 1; sum += *p1, p1 += stride, ++i)
								;
							for (; i < wk; sum += *p1, p1 += stride, pdst += stride, ++i)
								*pdst = sum * rwk >> rwkShift;
							for (; i < w; (sum += *p1) -= *p0, p0 += stride, p1 += stride, pdst += stride, ++i)
								*pdst = sum * rwk >> rwkShift;
							for (; i*2 < w * 2 + wk; sum -= *p0, p0 += stride, pdst += stride, ++i)
								*pdst = sum * rwk >> rwkShift;
						};

						uint8* buf0 = new uint8[sz.x * sz.y], * buf1 = new uint8[sz.x * sz.y], * buf2 = new uint8[sz.x * sz.y];
						for (int i = sz.x * sz.y - 1; i >= 0; i--)
							buf0[i] = data[i].a;
						for (int iy = 0; iy < sz.y; iy++)	// low-pass along x
							RunningSum(buf0 + iy * sz.x, buf1 + iy * sz.x, 1, sz.x, szKrn.x, rszKrn.x, 23);
						for (int ix = 0; ix < sz.x; ix++)	// low-pass along y, set to 1 if resulting alpha>0.5
							RunningSum(buf1 + ix, buf0 + ix, sz.x, sz.y, szKrn.y, rszKrn.y, 23 + 7);
						for (int iy = 0; iy < sz.y; iy++)	// count neighbors (+self) along x
							RunningSum(buf0 + iy * sz.x, buf1 + iy * sz.x, 1, sz.x, 3, 1, 0);
						for (int ix = 0; ix < sz.x; ix++)	// ..repeat along y
							RunningSum(buf1 + ix, buf2 + ix, sz.x, sz.y, 3, 1, 0);
						int nVtx = 0;
						for (int i = sz.x * sz.y - 1; i >= 0; nVtx += buf0[i] & isneg((int)buf2[i] - 6), i--)
							;	// count filled points with less than 5 filled neighbors
						ptitem2d* pts = new ptitem2d[nVtx];
						for (int iy = 0, i = 0, j = 0; iy < sz.y; iy++)
							for (int ix = 0; ix < sz.x; ix++, i++)
								if (buf0[i] && buf2[i] < 6)
									pts[j++].pt.set((ix + 0.5f) * 2 * rsz.x - 1, 1 - (iy + 0.5f) * 2 * rsz.y);

						edgeitem* edges = new edgeitem[nVtx * 2], * pedge;
						int nEdges = gEnv->pPhysicalWorld->GetPhysUtils()->qhull2d(pts, nVtx, edges, 16);
						for (pedge = edges; nEdges && !pedge->next; pedge++)
							;
						Vec3* vtx = new Vec3[nEdges * 2];
						Vec3_tpl<uint16>* tri = new Vec3_tpl<uint16>[(nEdges - 1) * 4];
						for (int i = 0; i < nEdges; i++, pedge = pedge->next)
						{
							Vec3 pt = m_Matrix.TransformVector(Vec3(pedge->pvtx->pt));
							vtx[i] = pt - gwd[0].v * thickness;
							vtx[i + nEdges] = pt + gwd[0].v * thickness;
						}
						for (int i = 0; i < nEdges - 2; i++)
						{
							tri[i].Set(0, i + 2, i + 1);
							tri[i + nEdges - 2].Set(nEdges, nEdges + i + 1, nEdges + i + 2);
						}
						for (int i = 0; i < nEdges; i++)
						{
							int i1 = i + 1 & (i + 1 - nEdges) >> 31;
							tri[(nEdges - 2) * 2 + i * 2].Set(i, i1, i + nEdges);
							tri[(nEdges - 2) * 2 + i * 2 + 1].Set(nEdges + i1, nEdges + i, i1);
						}
						pGeom = pGeoman->CreateMesh(vtx, &tri[0].x, 0, 0, (nEdges - 1) * 4, mesh_SingleBB | mesh_no_filter | mesh_no_vtx_merge, 0);

						delete[] tri, delete[] vtx, delete[] edges, delete[] pts;
						delete[] buf2, delete[] buf1, delete[] buf0;
					}

		if (!pGeom)
		{
			cyl.center.zero();
			cyl.axis = gwd[0].v;
			cyl.hh = thickness;
			cyl.r = m_Matrix.TransformVector(Vec3(1, 0, 0)).len();
			pGeom = pGeoman->CreatePrimitive(primitives::cylinder::type, &cyl);
		}
		phys_geometry* pgeom = pGeoman->RegisterGeometry(pGeom, m_pMaterial->GetSurfaceType()->GetId());
		pGeom->Release();
		--pgeom->nRefCount;

		primitives::box bbox;
		pGeom->GetBBox(&bbox);
		Matrix33 Rabs;
		Vec3 center = gwd[0].offset + bbox.center, size = (Rabs = bbox.Basis.T()).Fabs() * bbox.size;
		IPhysicalEntity* pentBuf[16], ** pents = pentBuf;
		int useStatics = properties.m_projectionType == SDecalProperties::eProjectOnTerrainAndStaticObjects;
		int nEnts = gEnv->pPhysicalWorld->GetEntitiesInBox(center - size, center + size, pents, ent_terrain | ent_static & - useStatics, 16);
		float t[2] = { 0, 0 }, tmax = m_Matrix.TransformVector(Vec3(0, 0, 1)).len();

		pe_status_pos sp;
		for (int i = 0; i < nEnts; i++)
			for (sp.ipart = 0; pents[i]->GetStatus(&sp); sp.ipart++)
				if (sp.flagsOR & geom_colltype0)
				{
					gwd[1].R = Matrix33(sp.q);
					gwd[1].offset = sp.pos;
					gwd[1].scale = sp.scale;
					for (int j = 0; j < 2; j++)
					{
						geom_contact* pcont;
						if (pGeom->Intersect(sp.pGeom, gwd, gwd + 1, 0, pcont))
							t[j] = min(tmax, max(t[j], (float)pcont->t));
						gwd[0].v.Flip();
					}
				}
		if (pGeom->GetType() == GEOM_TRIMESH)
		{
			mesh_data* md = (mesh_data*)pGeom->GetData();
			for (int i = 0; i*2 < md->nVertices; i++)
			{
				md->pVertices[i] -= gwd[0].v * t[0];
				md->pVertices[i + (md->nVertices >> 1)] += gwd[0].v * t[1];
			}
			pGeom->SetData(md);
		}
		else
		{
			cyl.hh += (t[0] + t[1]) * 0.5f;
			cyl.center += gwd[0].v * ((t[1] - t[0]) * 0.5f);
			pGeom->SetData(&cyl);
		}

		pe_params_pos pp;
		pp.pos = gwd[0].offset;
		pe_params_flags pf;
		pf.flagsOR = pef_disabled; // disabled ents are moved to the end of the cell list - needed to check mat updates the last
		if (!m_physEnt)
			m_physEnt = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_STATIC, &pf);
		pe_action_remove_all_parts arap;
		m_physEnt->Action(&arap);
		m_physEnt->SetParams(&pp);
		pe_geomparams gp;
		gp.flags = geom_mat_substitutor;
		gp.flagsCollider = 1 + useStatics * 2;
		m_physEnt->AddGeometry(pgeom, &gp);
		pf.flagsAND = ~pef_traceable;
		m_physEnt->SetParams(&pf);
		pf.flagsOR = pef_traceable;
		m_physEnt->SetParams(&pf); // triggers list update according to pef_disabled
		hasPhys = true;
	}
	if (m_physEnt && !hasPhys)
	{
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_physEnt);
		m_physEnt = 0;
	}
}

IRenderNode* CDecalRenderNode::Clone() const
{
	CDecalRenderNode* pDestDecal = new CDecalRenderNode();

	// CDecalRenderNode member vars
	pDestDecal->m_pos = m_pos;
	pDestDecal->m_localBounds = m_localBounds;
	pDestDecal->m_pMaterial = m_pMaterial;
	pDestDecal->m_pOverrideMaterial = m_pOverrideMaterial;
	pDestDecal->m_updateRequested = true;
	pDestDecal->m_decalProperties = m_decalProperties;
	pDestDecal->m_WSBBox = m_WSBBox;
	pDestDecal->m_Matrix = m_Matrix;
	pDestDecal->m_nLayerId = m_nLayerId;

	//IRenderNode member vars
	//	We cannot just copy over due to issues with the linked list of IRenderNode objects
	CopyIRenderNodeData(pDestDecal);

	return pDestDecal;
}

void CDecalRenderNode::SetMatrix(const Matrix34& mat)
{
	Vec3 translation = mat.GetTranslation();
	if (m_pos == translation)
		return;

	m_pos = translation;

	if (m_decalProperties.m_projectionType == SDecalProperties::ePlanar)
		m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 0.5f), Vec3(1, 1, 0.5f)));
	else
		m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 1), Vec3(1, 1, 1)));

	Get3DEngine()->RegisterEntity(this);
}

void CDecalRenderNode::SetMatrixFull(const Matrix34& mat)
{
	if (m_Matrix == mat)
		return;

	m_Matrix = mat;
	m_pos = mat.GetTranslation();

	if (m_decalProperties.m_projectionType == SDecalProperties::ePlanar)
		m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 0.5f), Vec3(1, 1, 0.5f)));
	else
		m_WSBBox.SetTransformedAABB(m_Matrix, AABB(-Vec3(1, 1, 1), Vec3(1, 1, 1)));
}

const char* CDecalRenderNode::GetEntityClassName() const
{
	return "Decal";
}

const char* CDecalRenderNode::GetName() const
{
	return "Decal";
}

void CDecalRenderNode::Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!passInfo.RenderDecals())
		return; // false;

	// Calculate distance fading once as it's the same for
	// all decals belonging to this node, regardless of type
	float fDistFading = SATURATE((1.f - rParam.fDistance / m_fWSMaxViewDist) * DIST_FADING_FACTOR);

	if (m_decalProperties.m_deferred)
	{
		if (passInfo.IsShadowPass())
			return; // otherwise causing flickering with GI

		SDeferredDecal newItem;
		newItem.fAlpha = fDistFading;
		newItem.pMaterial = m_pOverrideMaterial ? m_pOverrideMaterial : m_pMaterial;
		newItem.projMatrix = m_Matrix;
		newItem.nSortOrder = m_decalProperties.m_sortPrio;
		newItem.nFlags = DECAL_STATIC;
		GetRenderer()->EF_AddDeferredDecal(newItem, passInfo);
		return;
	}

	// update last rendered frame id
	m_nLastRenderedFrameId = passInfo.GetMainFrameID();

	bool bUpdateAABB = m_updateRequested;

	if (passInfo.IsGeneralPass())
		ProcessUpdateRequest();

	float waterLevel(m_p3DEngine->GetWaterLevel());
	for (size_t i(0); i < m_decals.size(); ++i)
	{
		CDecal* pDecal(m_decals[i]);
		if (pDecal && 0 != pDecal->m_pMaterial)
		{
			pDecal->m_vAmbient.x = rParam.AmbientColor.r;
			pDecal->m_vAmbient.y = rParam.AmbientColor.g;
			pDecal->m_vAmbient.z = rParam.AmbientColor.b;
			bool bAfterWater = CObjManager::IsAfterWater(pDecal->m_vWSPos, passInfo.GetCamera().GetPosition(), passInfo, waterLevel);
			pDecal->Render(0, bAfterWater, fDistFading, rParam.fDistance, passInfo);
		}
	}

	// terrain decal meshes are created only during rendering so only after that bbox can be computed
	if (bUpdateAABB)
		UpdateAABBFromRenderMeshes();

	//	return true;
}

IPhysicalEntity* CDecalRenderNode::GetPhysics() const
{
	return 0;
}

void CDecalRenderNode::SetPhysics(IPhysicalEntity*)
{
}

void CDecalRenderNode::SetMaterial(IMaterial* pMat)
{
	m_pOverrideMaterial = pMat;

	for (size_t i(0); i < m_decals.size(); ++i)
	{
		CDecal* pDecal(m_decals[i]);
		if (pDecal)
			pDecal->m_pMaterial = m_pOverrideMaterial ? m_pOverrideMaterial : m_pMaterial;
	}

	//special check for def decals forcing
	if (GetCVars()->e_DecalsForceDeferred)
	{
		CheckForceDeferred();
	}

}

void CDecalRenderNode::Precache()
{
	ProcessUpdateRequest();
}

void CDecalRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "DecalNode");
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_decals);
}

void CDecalRenderNode::CleanUpOldDecals()
{
	if (m_nLastRenderedFrameId != 0 && // was rendered at least once
	    (int)GetRenderer()->GetFrameID(false) > (int)m_nLastRenderedFrameId + GetCVars()->e_DecalsMaxValidFrames)
	{
		DeleteDecals();
		m_nLastRenderedFrameId = 0;
		m_updateRequested = true; // make sure if rendered again, that the decal is recreated
	}
}

void CDecalRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_pos += delta;
	m_WSBBox.Move(delta);
	m_Matrix.SetTranslation(m_Matrix.GetTranslation() + delta);
}

void CDecalRenderNode::FillBBox(AABB& aabb)
{
	aabb = CDecalRenderNode::GetBBox();
}

EERType CDecalRenderNode::GetRenderNodeType()
{
	return eERType_Decal;
}

float CDecalRenderNode::GetMaxViewDist()
{
	float fMatScale = m_Matrix.GetColumn0().GetLength();

	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, fMatScale * 0.75f * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return(max(GetCVars()->e_ViewDistMin, fMatScale * 0.75f * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized()));
}

Vec3 CDecalRenderNode::GetPos(bool bWorldOnly) const
{
	return m_pos;
}

IMaterial* CDecalRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pOverrideMaterial ? m_pOverrideMaterial : m_pMaterial;
}
