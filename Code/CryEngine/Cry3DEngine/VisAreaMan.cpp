// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjmandraw.cpp
//  Version:     v1.00
//  Created:     18/12/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Visibility areas
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>

#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "terrain.h"
#include <CryMath/AABBSV.h>
#include "3dEngine.h"

#define DEFAULT_INITIAL_PORTALS   1
#define DEFAULT_INITIAL_VISAREAS  1
#define DEFAULT_INITIAL_OCCLAREAS 1

CVisAreaManager::CVisAreaManager()
{
	m_pCurPortal = m_pCurArea = 0;
	m_bOutdoorVisible = false;
	m_bSkyVisible = false;
	m_bSunIsNeeded = false;
	m_bOceanVisible = false;
	m_pAABBTree = NULL;

	m_visAreas.PreAllocate(DEFAULT_INITIAL_VISAREAS);
	m_visAreaColdData.PreAllocate(DEFAULT_INITIAL_VISAREAS);

	m_portals.PreAllocate(DEFAULT_INITIAL_PORTALS);
	m_portalColdData.PreAllocate(DEFAULT_INITIAL_PORTALS);

	m_occlAreas.PreAllocate(DEFAULT_INITIAL_OCCLAREAS);
	m_occlAreaColdData.PreAllocate(DEFAULT_INITIAL_OCCLAREAS);

	m_segVisAreas.Clear();
	m_segPortals.Clear();
	m_segOcclAreas.Clear();
}

void CVisAreaManager::DeleteAllVisAreas()
{
	for (int i = 0; i < m_lstVisAreas.Count(); i++)
	{
		if (m_visAreas.Find(m_lstVisAreas[i]) >= 0)
		{
			delete m_lstVisAreas[i];
		}
		else
		{
			delete m_lstVisAreas[i]->GetColdData();
			delete m_lstVisAreas[i];
		}
	}

	m_visAreas.Clear();
	m_visAreaColdData.Clear();
	m_lstVisAreas.Clear();

	for (int i = 0; i < m_lstPortals.Count(); i++)
	{
		if (m_portals.Find(m_lstPortals[i]) >= 0)
		{
			delete m_lstPortals[i];
		}
		else
		{
			delete m_lstPortals[i]->GetColdData();
			delete m_lstPortals[i];
		}
	}

	m_portals.Clear();
	m_portalColdData.Clear();
	m_lstPortals.Clear();

	for (int i = 0; i < m_lstOcclAreas.Count(); i++)
	{
		if (m_occlAreas.Find(m_lstOcclAreas[i]) >= 0)
		{
			delete m_lstOcclAreas[i];
		}
		else
		{
			delete m_lstOcclAreas[i]->GetColdData();
			delete m_lstOcclAreas[i];
		}
	}

	m_occlAreas.Clear();
	m_occlAreaColdData.Clear();
	m_lstOcclAreas.Clear();

	stl::free_container(CVisArea::m_lUnavailableAreas);
}

void SAABBTreeNode::OffsetPosition(const Vec3& delta)
{
	nodeBox.Move(delta);
	if (!nodeAreas.Count())
	{
		for (int i = 0; i < 2; i++)
		{
			if (arrChilds[i])
				arrChilds[i]->OffsetPosition(delta);
		}
	}
}

CVisAreaManager::~CVisAreaManager()
{
	DeleteAllVisAreas();

	delete m_pAABBTree;
	m_pAABBTree = NULL;
}

SAABBTreeNode::SAABBTreeNode(PodArray<CVisArea*>& lstAreas, AABB box, int nRecursion)
{
	memset(this, 0, sizeof(*this));

	nodeBox = box;

	nRecursion++;
	if (nRecursion > 8 || lstAreas.Count() < 8)
	{
		nodeAreas.AddList(lstAreas);
		return;
	}

	PodArray<CVisArea*> lstAreas0, lstAreas1;
	Vec3 vSize = nodeBox.GetSize();
	Vec3 vCenter = nodeBox.GetCenter();

	AABB nodeBox0 = nodeBox;
	AABB nodeBox1 = nodeBox;

	if (vSize.x >= vSize.y && vSize.x >= vSize.z)
	{
		nodeBox0.min.x = vCenter.x;
		nodeBox1.max.x = vCenter.x;
	}
	else if (vSize.y >= vSize.x && vSize.y >= vSize.z)
	{
		nodeBox0.min.y = vCenter.y;
		nodeBox1.max.y = vCenter.y;
	}
	else
	{
		nodeBox0.min.z = vCenter.z;
		nodeBox1.max.z = vCenter.z;
	}

	for (int i = 0; i < lstAreas.Count(); i++)
	{
		if (Overlap::AABB_AABB(nodeBox0, *lstAreas[i]->GetAABBox()))
			lstAreas0.Add(lstAreas[i]);

		if (Overlap::AABB_AABB(nodeBox1, *lstAreas[i]->GetAABBox()))
			lstAreas1.Add(lstAreas[i]);
	}

	if (lstAreas0.Count())
		arrChilds[0] = new SAABBTreeNode(lstAreas0, nodeBox0, nRecursion);

	if (lstAreas1.Count())
		arrChilds[1] = new SAABBTreeNode(lstAreas1, nodeBox1, nRecursion);
}

SAABBTreeNode::~SAABBTreeNode()
{
	delete arrChilds[0];
	delete arrChilds[1];
}

SAABBTreeNode* SAABBTreeNode::GetTopNode(const AABB& box, void** pNodeCache)
{
	AABB boxClip = box;
	boxClip.ClipToBox(nodeBox);

	SAABBTreeNode* pNode = this;
	if (pNodeCache)
	{
		pNode = (SAABBTreeNode*)*pNodeCache;
		if (!pNode || !pNode->nodeBox.ContainsBox(boxClip))
			pNode = this;
	}

	// Find top node containing box.
	for (;; )
	{
		int i;
		for (i = 0; i < 2; i++)
		{
			if (pNode->arrChilds[i] && pNode->arrChilds[i]->nodeBox.ContainsBox(boxClip))
			{
				pNode = pNode->arrChilds[i];
				break;
			}
		}
		if (i == 2)
			break;
	}

	if (pNodeCache)
		*(SAABBTreeNode**)pNodeCache = pNode;
	return pNode;
}

bool SAABBTreeNode::IntersectsVisAreas(const AABB& box)
{
	if (nodeBox.IsIntersectBox(box))
	{
		if (nodeAreas.Count())
		{
			// leaf
			for (int i = 0; i < nodeAreas.Count(); i++)
			{
				if (nodeAreas[i]->m_bActive && nodeAreas[i]->m_boxArea.IsIntersectBox(box))
					return true;
			}
		}
		else
		{
			// node
			for (int i = 0; i < 2; i++)
				if (arrChilds[i])
					if (arrChilds[i]->IntersectsVisAreas(box))
						return true;
		}
	}
	return false;
}

int SAABBTreeNode::ClipOutsideVisAreas(Sphere& sphere, Vec3 const& vNormal)
{
	int nClipped = 0;

	if (sphere.radius > FLT_MAX * 0.01f || Overlap::Sphere_AABB(sphere, nodeBox))
	{
		if (nodeAreas.Count())
		{
			// leaf
			for (int i = 0; i < nodeAreas.Count(); i++)
			{
				if (nodeAreas[i]->m_bActive && Overlap::Sphere_AABB(sphere, nodeAreas[i]->m_boxArea))
					nClipped += nodeAreas[i]->ClipToVisArea(false, sphere, vNormal);
			}
		}
		else
		{
			// node
			for (int i = 0; i < 2; i++)
				if (arrChilds[i])
					nClipped += arrChilds[i]->ClipOutsideVisAreas(sphere, vNormal);
		}
	}

	return nClipped;
}

void CVisAreaManager::UpdateAABBTree()
{
	delete m_pAABBTree;
	PodArray<CVisArea*> lstAreas;
	lstAreas.AddList(m_lstPortals);
	lstAreas.AddList(m_lstVisAreas);

	AABB nodeBox;
	nodeBox.min = Vec3(1000000, 1000000, 1000000);
	nodeBox.max = -nodeBox.min;
	for (int i = 0; i < lstAreas.Count(); i++)
		nodeBox.Add(*lstAreas[i]->GetAABBox());

	m_pAABBTree = new SAABBTreeNode(lstAreas, nodeBox);
}

bool CVisAreaManager::IsEntityVisible(IRenderNode* pEnt)
{
	if (GetCVars()->e_Portals == 3)
		return true;

	if (!pEnt->GetEntityVisArea())
		return IsOutdoorAreasVisible();

	return true;
}

void CVisAreaManager::SetCurAreas(const SRenderingPassInfo& passInfo)
{
	m_pCurArea = 0;
	m_pCurPortal = 0;

	if (!GetCVars()->e_Portals)
		return;

	if (!m_pAABBTree)
		UpdateAABBTree();

	CVisArea* pFound = m_pAABBTree->FindVisarea(passInfo.GetCamera().GetOccPos());

#ifdef _DEBUG

	// find camera portal id
	for (int v = 0; v < m_lstPortals.Count(); v++)
		if (m_lstPortals[v]->m_bActive && m_lstPortals[v]->IsPointInsideVisArea(passInfo.GetCamera().GetOccPos()))
		{
			m_pCurPortal = m_lstPortals[v];
			break;
		}

	// if not inside any portal - try to find area
	if (!m_pCurPortal)
	{
		//		int nFoundAreasNum = 0;

		// find camera area
		for (int nVolumeId = 0; nVolumeId < m_lstVisAreas.Count(); nVolumeId++)
		{
			if (m_lstVisAreas[nVolumeId]->IsPointInsideVisArea(passInfo.GetCamera().GetOccPos()))
			{
				//			nFoundAreasNum++;
				m_pCurArea = m_lstVisAreas[nVolumeId];
				break;
			}
		}

		//		if(nFoundAreasNum>1) // if more than one area found - set cur area to undefined
		{
			// todo: try to set joining portal as current
			//	m_pCurArea = 0;
		}
	}

	assert(pFound == m_pCurArea || pFound == m_pCurPortal);

#endif // _DEBUG

	if (pFound)
	{
		if (pFound->IsPortal())
			m_pCurPortal = pFound;
		else
			m_pCurArea = pFound;
	}

	// camera is in outdoors
	m_lstActiveEntransePortals.Clear();
	if (!m_pCurArea && !m_pCurPortal)
		MakeActiveEntransePortalsList(&passInfo.GetCamera(), m_lstActiveEntransePortals, 0, passInfo);

	/*
	   if(m_pCurArea)
	   {
	    IVisArea * arrAreas[8];
	    int nres = m_pCurArea->GetVisAreaConnections(arrAreas, 8);
	    nres=nres;
	   }
	   DefineTrees();*/

	/*	if(GetCVars()->e_Portals == 4)
	   {
	    if(m_pCurPortal)
	    {
	      IVisArea * arrAreas[64];
	      int nConnections = m_pCurPortal->GetVisAreaConnections(arrAreas,64);
	      PrintMessage("CurPortal = %s, nConnections = %d", m_pCurPortal->m_sName, nConnections);
	    }

	    if(m_pCurArea)
	    {
	      IVisArea * arrAreas[64];
	      int nConnections = m_pCurArea->GetVisAreaConnections(arrAreas,64);
	      PrintMessage("CurArea = %s, nRes = %d", m_pCurArea->m_sName, nConnections);
	    }
	   }*/
}

bool CVisAreaManager::IsSkyVisible()
{
	return m_bSkyVisible;
}

bool CVisAreaManager::IsOceanVisible()
{
	return m_bOceanVisible;
}

bool CVisAreaManager::IsOutdoorAreasVisible()
{
	if (!m_pCurArea && !m_pCurPortal)
	{
		m_bOutdoorVisible = true;
		return m_bOutdoorVisible; // camera not in the areas
	}

	if (m_pCurPortal && m_pCurPortal->m_lstConnections.Count() == 1)
	{
		m_bOutdoorVisible = true;
		return m_bOutdoorVisible; // camera is in exit portal
	}

	if (m_bOutdoorVisible)
		return true; // exit is visible

	// note: outdoor camera is no modified in this case
	return false;
}

/*void CVisAreaManager::SetAreaFogVolume(CTerrain * pTerrain, CVisArea * pVisArea)
   {
   pVisArea->m_pFogVolume=0;
   for(int f=0; f<Get3DEngine()->GetFogVolumes().Count(); f++)
   {
    const Vec3 & v1Min = Get3DEngine()->GetFogVolumes()[f].box.min;
    const Vec3 & v1Max = Get3DEngine()->GetFogVolumes()[f].box.max;
    const Vec3 & v2Min = pVisArea->m_boxArea.min;
    const Vec3 & v2Max = pVisArea->m_boxArea.max;

    if(v1Max.x>v2Min.x && v2Max.x>v1Min.x)
    if(v1Max.y>v2Min.y && v2Max.y>v1Min.y)
    if(v1Max.z>v2Min.z && v2Max.z>v1Min.z)
    if(!Get3DEngine()->GetFogVolumes()[f].bOcean)
    {
      Vec3 arrVerts3d[8] =
      {
        Vec3(v1Min.x,v1Min.y,v1Min.z),
        Vec3(v1Min.x,v1Max.y,v1Min.z),
        Vec3(v1Max.x,v1Min.y,v1Min.z),
        Vec3(v1Max.x,v1Max.y,v1Min.z),
        Vec3(v1Min.x,v1Min.y,v1Max.z),
        Vec3(v1Min.x,v1Max.y,v1Max.z),
        Vec3(v1Max.x,v1Min.y,v1Max.z),
        Vec3(v1Max.x,v1Max.y,v1Max.z)
      };

      bool bIntersect = false;
      for(int i=0; i<8; i++)
        if(pVisArea->IsPointInsideVisArea(arrVerts3d[i]))
        {
          bIntersect = true;
          break;
        }

      if(!bIntersect)
        if(pVisArea->IsPointInsideVisArea((v1Min+v1Max)*0.5f))
          bIntersect = true;

      if(!bIntersect)
      {
        for(int i=0; i<pVisArea->m_lstShapePoints.Count(); i++)
          if(Get3DEngine()->GetFogVolumes()[f].IsInsideBBox(pVisArea->m_lstShapePoints[i]))
          {
            bIntersect = true;
            break;
          }
      }

      if(!bIntersect)
      {
        Vec3 vCenter = (pVisArea->m_boxArea.min+pVisArea->m_boxArea.max)*0.5f;
        if(Get3DEngine()->GetFogVolumes()[f].IsInsideBBox(vCenter))
          bIntersect = true;
      }

      if(bIntersect)
      {
        pVisArea->m_pFogVolume = &Get3DEngine()->GetFogVolumes()[f];
        Get3DEngine()->GetFogVolumes()[f].bIndoorOnly = true;
        pTerrain->UnregisterFogVolumeFromOutdoor(&Get3DEngine()->GetFogVolumes()[f]);
        break;
      }
    }
   }
   }*/

void CVisAreaManager::PortalsDrawDebug()
{
	UpdateConnections();
	/*
	   if(m_pCurArea)
	   {
	    for(int p=0; p<m_pCurArea->m_lstConnections.Count(); p++)
	    {
	      CVisArea * pPortal = m_pCurArea->m_lstConnections[p];
	      float fBlink = gEnv->pTimer->GetFrameStartTime().GetPeriodicFraction(1.0f)>0.5f ? 1.0f : 0.0f;
	      float fError = pPortal->IsPortalValid() ? 1.0f : fBlink;
	      GetRenderer()->SetMaterialColor(fError,fError*(pPortal->m_lstConnections.Count()<2),0,0.25f);
	      DrawBBox(pPortal->m_boxArea.min, pPortal->m_boxArea.max, DPRIM_SOLID_BOX);
	      GetRenderer()->DrawLabel((pPortal->m_boxArea.min+ pPortal->m_boxArea.max)*0.5f,
	        2,pPortal->m_sName);
	    }
	   }
	   else*/
	{
		// debug draw areas
		Vec3 oneVec(1, 1, 1);
		for (int v = 0; v < m_lstVisAreas.Count(); v++)
		{
			DrawBBox(m_lstVisAreas[v]->m_boxArea.min, m_lstVisAreas[v]->m_boxArea.max, ColorB(0, 255, 0, 64)); //, DPRIM_SOLID_BOX);
			IRenderAuxText::DrawLabelEx((m_lstVisAreas[v]->m_boxArea.min + m_lstVisAreas[v]->m_boxArea.max) * 0.5f, 1, (float*)&oneVec, 0, 1, m_lstVisAreas[v]->GetName());
			DrawBBox(m_lstVisAreas[v]->m_boxStatics, Col_LightGray);
		}

		// debug draw portals
		for (int v = 0; v < m_lstPortals.Count(); v++)
		{
			CVisArea* pPortal = m_lstPortals[v];

			float fBlink = gEnv->pTimer->GetFrameStartTime().GetPeriodicFraction(1.0f) > 0.5f ? 1.0f : 0.0f;
			float fError = pPortal->IsPortalValid() ? 1.f : fBlink;

			ColorB col(
			  (int)clamp_tpl(fError * 255.0f, 0.0f, 255.0f),
			  (int)clamp_tpl(fError * (pPortal->m_lstConnections.Count() < 2) * 255.0f, 0.0f, 255.0f),
			  0,
			  64);
			DrawBBox(pPortal->m_boxArea.min, pPortal->m_boxArea.max, col);

			IRenderAuxText::DrawLabelEx((pPortal->m_boxArea.min + pPortal->m_boxArea.max) * 0.5f, 1, (float*)&oneVec, 0, 1, pPortal->GetName());

			Vec3 vCenter = (pPortal->m_boxArea.min + pPortal->m_boxArea.max) * 0.5f;
			DrawBBox(vCenter - Vec3(0.1f, 0.1f, 0.1f), vCenter + Vec3(0.1f, 0.1f, 0.1f));

			int nConnections = pPortal->m_lstConnections.Count();
			if (nConnections == 1)
				col = ColorB(0, 255, 0, 64);
			else
				col = ColorB(0, 0, 255, 64);

			for (int i = 0; i < nConnections && i < 2; i++)
				DrawLine(vCenter, vCenter + pPortal->m_vConnNormals[i], col);

			DrawBBox(pPortal->m_boxStatics.min, pPortal->m_boxStatics.max, col);
		}

		/*
		    // debug draw area shape
		    GetRenderer()->SetMaterialColor(0,0,1,0.25f);
		    for(int v=0; v<m_lstVisAreas.Count(); v++)
		    for(int p=0; p<m_lstVisAreas[v]->m_lstShapePoints.Count(); p++)
		      GetRenderer()->DrawLabel(m_lstVisAreas[v]->m_lstShapePoints[p], 2,"%d", p);
		    for(int v=0; v<m_lstPortals.Count(); v++)
		    for(int p=0; p<m_lstPortals[v]->m_lstShapePoints.Count(); p++)
		      GetRenderer()->DrawLabel(m_lstPortals[v]->m_lstShapePoints[p], 2,"%d", p);*/
	}
}

void CVisAreaManager::DrawVisibleSectors(const SRenderingPassInfo& passInfo, uint32 passCullMask)
{
	FUNCTION_PROFILER_3DENGINE;

	for (int i = 0; i < m_lstVisibleAreas.Count(); i++)
	{
		CVisArea* pArea = m_lstVisibleAreas[i];
		Vec3 vAmbColor = pArea->GetFinalAmbientColor();
		if (pArea->IsObjectsTreeValid())
		{
			for (int c = 0; c < pArea->m_lstCurCamerasLen; c++)
			{
				passInfo.GetRendItemSorter().IncreaseOctreeCounter();
				// create a new RenderingPassInfo object, which a camera matching the visarea
				pArea->GetObjectsTree()->Render_Object_Nodes(false, OCTREENODE_RENDER_FLAG_OBJECTS, vAmbColor, passCullMask,
				                                             SRenderingPassInfo::CreateTempRenderingInfo(CVisArea::s_tmpCameras[pArea->m_lstCurCamerasIdx + c], passInfo));
			}
		}
	}

	passInfo.GetRendItemSorter().IncreaseGroupCounter();
}

void CVisAreaManager::PhysicalizeInBox(const AABB& bbox)
{
	for (int i = 0; i < m_lstVisAreas.Count(); i++)
	{
		CVisArea* pArea = m_lstVisAreas[i];
		if (pArea && pArea->IsObjectsTreeValid() && Overlap::AABB_AABB(*pArea->GetAABBox(), bbox))
		{
			pArea->GetObjectsTree()->PhysicalizeInBox(bbox);
		}
	}
	for (int i = 0; i < m_lstPortals.Count(); i++)
	{
		CVisArea* pArea = m_lstPortals[i];
		if (pArea && pArea->IsObjectsTreeValid() && Overlap::AABB_AABB(*pArea->GetAABBox(), bbox))
		{
			pArea->GetObjectsTree()->PhysicalizeInBox(bbox);
		}
	}
}
void CVisAreaManager::DephysicalizeInBox(const AABB& bbox)
{
	for (int i = 0; i < m_lstVisAreas.Count(); i++)
	{
		CVisArea* pArea = m_lstVisAreas[i];
		if (pArea && pArea->IsObjectsTreeValid() && Overlap::AABB_AABB(*pArea->GetAABBox(), bbox))
		{
			pArea->GetObjectsTree()->DephysicalizeInBox(bbox);
		}
	}
	for (int i = 0; i < m_lstPortals.Count(); i++)
	{
		CVisArea* pArea = m_lstPortals[i];
		if (pArea && pArea->IsObjectsTreeValid() && Overlap::AABB_AABB(*pArea->GetAABBox(), bbox))
		{
			pArea->GetObjectsTree()->DephysicalizeInBox(bbox);
		}
	}
}

void CVisAreaManager::CheckVis(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (passInfo.IsGeneralPass())
	{
		m_bOutdoorVisible = false;
		m_bSkyVisible = false;
		m_bOceanVisible = false;
		CVisArea::s_tmpCameras.Clear();
	}

	m_lstOutdoorPortalCameras.Clear();
	m_lstVisibleAreas.Clear();
	m_bSunIsNeeded = false;

	SetCurAreas(passInfo);

	CCamera camRoot = passInfo.GetCamera();

	camRoot.m_ScissorInfo.x1 = 0;
	camRoot.m_ScissorInfo.y1 = 0;
	camRoot.m_ScissorInfo.x2 = camRoot.GetViewSurfaceX();
	camRoot.m_ScissorInfo.y2 = camRoot.GetViewSurfaceZ();

	if (GetCVars()->e_Portals == 3)
	{
		// draw everything for debug
		for (int i = 0; i < m_lstVisAreas.Count(); i++)
			if (camRoot.IsAABBVisible_F(AABB(m_lstVisAreas[i]->m_boxArea.min, m_lstVisAreas[i]->m_boxArea.max)))
				m_lstVisAreas[i]->PreRender(0, camRoot, 0, m_pCurPortal, &m_bOutdoorVisible, &m_lstOutdoorPortalCameras, &m_bSkyVisible, &m_bOceanVisible, m_lstVisibleAreas, passInfo);

		for (int i = 0; i < m_lstPortals.Count(); i++)
			if (camRoot.IsAABBVisible_F(AABB(m_lstPortals[i]->m_boxArea.min, m_lstPortals[i]->m_boxArea.max)))
				m_lstPortals[i]->PreRender(0, camRoot, 0, m_pCurPortal, &m_bOutdoorVisible, &m_lstOutdoorPortalCameras, &m_bSkyVisible, &m_bOceanVisible, m_lstVisibleAreas, passInfo);
	}
	else
	{
		if (passInfo.IsRecursivePass())
		{
			// use another starting point for reflections
			CVisArea* pVisArea = (CVisArea*)GetVisAreaFromPos(camRoot.GetOccPos());
			if (pVisArea)
				pVisArea->PreRender(3, camRoot, 0, m_pCurPortal, &m_bOutdoorVisible, &m_lstOutdoorPortalCameras, &m_bSkyVisible, &m_bOceanVisible, m_lstVisibleAreas, passInfo);
		}
		else if (m_pCurArea)
		{
			// camera inside some sector
			m_pCurArea->PreRender(GetCVars()->e_PortalsMaxRecursion, camRoot, 0, m_pCurPortal, &m_bOutdoorVisible, &m_lstOutdoorPortalCameras, &m_bSkyVisible, &m_bOceanVisible, m_lstVisibleAreas, passInfo);

			for (int ii = 0; ii < m_lstOutdoorPortalCameras.Count(); ii++) // process all exit portals
			{
				// for each portal build list of potentially visible entrances into other areas
				MakeActiveEntransePortalsList(&m_lstOutdoorPortalCameras[ii], m_lstActiveEntransePortals, (CVisArea*)m_lstOutdoorPortalCameras[ii].m_pPortal, passInfo);
				for (int i = 0; i < m_lstActiveEntransePortals.Count(); i++) // entrance into another building is visible
					m_lstActiveEntransePortals[i]->PreRender(i == 0 ? 5 : 1,
					                                         m_lstOutdoorPortalCameras[ii], 0, m_pCurPortal, 0, 0, 0, 0, m_lstVisibleAreas, passInfo);
			}

			// reset scissor if skybox is visible also thru skyboxonly portal
			if (m_bSkyVisible && m_lstOutdoorPortalCameras.Count() == 1)
				m_lstOutdoorPortalCameras[0].m_ScissorInfo.x1 =
				  m_lstOutdoorPortalCameras[0].m_ScissorInfo.x2 =
				    m_lstOutdoorPortalCameras[0].m_ScissorInfo.y1 =
				      m_lstOutdoorPortalCameras[0].m_ScissorInfo.y2 = 0;
		}
		else if (m_pCurPortal)
		{
			// camera inside some portal
			m_pCurPortal->PreRender(GetCVars()->e_PortalsMaxRecursion - 1, camRoot, 0, m_pCurPortal, &m_bOutdoorVisible, &m_lstOutdoorPortalCameras, &m_bSkyVisible, &m_bOceanVisible, m_lstVisibleAreas, passInfo);

			if (m_pCurPortal->m_lstConnections.Count() == 1)
				m_lstOutdoorPortalCameras.Clear(); // camera in outdoor

			if (m_pCurPortal->m_lstConnections.Count() == 1 || m_lstOutdoorPortalCameras.Count())
			{
				// if camera is in exit portal or exit is visible
				MakeActiveEntransePortalsList(m_lstOutdoorPortalCameras.Count() ? &m_lstOutdoorPortalCameras[0] : &camRoot,
				                              m_lstActiveEntransePortals,
				                              m_lstOutdoorPortalCameras.Count() ? (CVisArea*)m_lstOutdoorPortalCameras[0].m_pPortal : m_pCurPortal, passInfo);
				for (int i = 0; i < m_lstActiveEntransePortals.Count(); i++) // entrance into another building is visible
					m_lstActiveEntransePortals[i]->PreRender(i == 0 ? 5 : 1,
					                                         m_lstOutdoorPortalCameras.Count() ? m_lstOutdoorPortalCameras[0] : camRoot, 0, m_pCurPortal, 0, 0, 0, 0, m_lstVisibleAreas, passInfo);
				//				m_lstOutdoorPortalCameras.Clear();  // otherwise ocean in fleet was not scissored
			}
		}
		else if (m_lstActiveEntransePortals.Count())
		{
			// camera in outdoors - process visible entrance portals
			for (int i = 0; i < m_lstActiveEntransePortals.Count(); i++)
				m_lstActiveEntransePortals[i]->PreRender(5, camRoot, 0, m_lstActiveEntransePortals[i], &m_bOutdoorVisible, &m_lstOutdoorPortalCameras, &m_bSkyVisible, &m_bOceanVisible, m_lstVisibleAreas, passInfo);
			m_lstActiveEntransePortals.Clear();

			// do not recurse to another building since we already processed all potential entrances
			m_lstOutdoorPortalCameras.Clear(); // use default camera
			m_bOutdoorVisible = true;
		}
	}

	if (GetCVars()->e_Portals == 2)
		PortalsDrawDebug();
}

void CVisAreaManager::ActivatePortal(const Vec3& vPos, bool bActivate, const char* szEntityName)
{
	//  bool bFound = false;

	for (int v = 0; v < m_lstPortals.Count(); v++)
	{
		AABB aabb;
		aabb.min = m_lstPortals[v]->m_boxArea.min - Vec3(0.5f, 0.5f, 0.1f);
		aabb.max = m_lstPortals[v]->m_boxArea.max + Vec3(0.5f, 0.5f, 0.0f);

		if (Overlap::Point_AABB(vPos, aabb))
		{
			m_lstPortals[v]->m_bActive = bActivate;

			// switch to PrintComment once portals activation is working stable
			PrintMessage("I3DEngine::ActivatePortal(): Portal %s is %s by entity %s at position(%.1f,%.1f,%.1f)",
			             m_lstPortals[v]->GetName(), bActivate ? "Enabled" : "Disabled", szEntityName, vPos.x, vPos.y, vPos.z);

			//      bFound = true;
		}
	}

	/*
	   if(!bFound)
	   {
	   PrintComment("I3DEngine::ActivatePortal(): Portal not found for entity %s at position(%.1f,%.1f,%.1f)",
	    szEntityName, vPos.x, vPos.y, vPos.z);
	   }
	 */
}

void CVisAreaManager::ActivateOcclusionAreas(IVisAreaTestCallback* pTest, bool bActivate)
{
	for (int v = 0; v < m_lstOcclAreas.Count(); v++)
	{
		if (pTest->TestVisArea(m_lstOcclAreas[v]))
		{
			m_lstOcclAreas[v]->m_bActive = bActivate;
		}
	}
}

bool CVisAreaManager::IsValidVisAreaPointer(CVisArea* pVisArea)
{
	if (m_lstVisAreas.Find(pVisArea) < 0 &&
	    m_lstPortals.Find(pVisArea) < 0 &&
	    m_lstOcclAreas.Find(pVisArea) < 0)
		return false;

	return true;
}

//This is only called from the editor, so pVisArea will not be pool allocated by type
bool CVisAreaManager::DeleteVisArea(CVisArea* pVisArea)
{
	bool bFound = false;
	if (m_lstVisAreas.Delete(pVisArea) || m_lstPortals.Delete(pVisArea) || m_lstOcclAreas.Delete(pVisArea))
	{
		delete pVisArea;
		bFound = true;
	}

	m_lstActiveOcclVolumes.Delete(pVisArea);
	m_lstIndoorActiveOcclVolumes.Delete(pVisArea);
	m_lstActiveEntransePortals.Delete(pVisArea);

	m_pCurArea = 0;
	m_pCurPortal = 0;
	UpdateConnections();

	delete m_pAABBTree;
	m_pAABBTree = NULL;

	return bFound;
}

/*void CVisAreaManager::LoadVisAreaShapeFromXML(XmlNodeRef pDoc)
   {
   for(int i=0; i<m_lstVisAreas.Count(); i++)
   {
    delete m_lstVisAreas[i];
    m_lstVisAreas.Delete(i);
    i--;
   }

   for(int i=0; i<m_lstPortals.Count(); i++)
   {
    delete m_lstPortals[i];
    m_lstPortals.Delete(i);
    i--;
   }

   // fill list of volumes of shape points
   XmlNodeRef pObjectsNode = pDoc->findChild("Objects");
   if (pObjectsNode)
   {
    for (int i = 0; i < pObjectsNode->getChildCount(); i++)
    {
      XmlNodeRef pNode = pObjectsNode->getChild(i);
      if (pNode->isTag("Object"))
      {
        const char * pType = pNode->getAttr("Type");
        if (strstr(pType,"OccluderArea") || strstr(pType,"VisArea") || strstr(pType,"Portal"))
        {
          CVisArea * pArea = new CVisArea();

          pArea->m_boxArea.max=SetMinBB();
          pArea->m_boxArea.min=SetMaxBB();

          // set name
          strcpy(pArea->m_sName, pNode->getAttr("Name"));
          strlwr(pArea->m_sName);

          // set height
          pNode->getAttr("Height",pArea->m_fHeight);

          // set ambient color
          pNode->getAttr("AmbientColor", pArea->m_vAmbColor);

          // set dynamic ambient color
   //					pNode->getAttr("DynAmbientColor", pArea->m_vDynAmbColor);

          // set SkyOnly flag
          pNode->getAttr("SkyOnly", pArea->m_bSkyOnly);

          // set AfectedByOutLights flag
          pNode->getAttr("AffectedBySun", pArea->m_bAfectedByOutLights);

          // set ViewDistRatio
          pNode->getAttr("ViewDistRatio", pArea->m_fViewDistRatio);

          // set DoubleSide flag
          pNode->getAttr("DoubleSide", pArea->m_bDoubleSide);

          // set UseInIndoors flag
          pNode->getAttr("UseInIndoors", pArea->m_bUseInIndoors);

          if(strstr(pType, "OccluderArea"))
            m_lstOcclAreas.Add(pArea);
          else if(strstr(pArea->m_sName,"portal") || strstr(pType,"Portal"))
            m_lstPortals.Add(pArea);
          else
            m_lstVisAreas.Add(pArea);

          // load vertices
          XmlNodeRef pPointsNode = pNode->findChild("Points");
          if (pPointsNode)
          for (int i = 0; i < pPointsNode->getChildCount(); i++)
          {
            XmlNodeRef pPointNode = pPointsNode->getChild(i);

            Vec3 vPos;
            if (pPointNode->isTag("Point") && pPointNode->getAttr("Pos", vPos))
            {
              pArea->m_lstShapePoints.Add(vPos);
              pArea->m_boxArea.max.CheckMax(vPos);
              pArea->m_boxArea.min.CheckMin(vPos);
              pArea->m_boxArea.max.CheckMax(vPos+Vec3(0,0,pArea->m_fHeight));
              pArea->m_boxArea.min.CheckMin(vPos+Vec3(0,0,pArea->m_fHeight));
            }
          }
          pArea->UpdateGeometryBBox();
        }
      }
    }
   }

   // load area boxes to support old way
   //	LoadVisAreaBoxFromXML(pDoc);
   }*/

//THIS SHOULD ONLY BE CALLED BY THE EDITOR
void CVisAreaManager::UpdateVisArea(CVisArea* pArea, const Vec3* pPoints, int nCount, const char* szName, const SVisAreaInfo& info)
{
	// on first update there will be nothing to delete, area will be added into list only in this function
	m_lstPortals.Delete(pArea);
	m_lstVisAreas.Delete(pArea);
	m_lstOcclAreas.Delete(pArea);

	SGenericColdData* pColdData = pArea->GetColdData();
	if (pColdData != NULL)
	{
		delete pArea->GetColdData();
		pArea->SetColdDataPtr(NULL);
	}

	SGenericColdData* pColdDataPtr = NULL;

	char sTemp[64];
	cry_strcpy(sTemp, szName);
	_strlwr_s(sTemp, sizeof(sTemp));
	strlwr(sTemp);

	bool bPortal = false;
	bool bVisArea = false;
	bool bOcclArea = false;

	//TODO: Refactor with code below so it's not horrible
	if (strstr(sTemp, "portal"))
	{
		pColdDataPtr = new SPortalColdData();
		bPortal = true;
	}
	else if (strstr(sTemp, "visarea"))
	{
		pColdDataPtr = new SGenericColdData();
		bVisArea = true;
	}
	else if (strstr(sTemp, "occlarea"))
	{
		pColdDataPtr = new SGenericColdData();
		bOcclArea = true;
	}
	else
	{
		pColdDataPtr = new SGenericColdData();
	}

	assert(pColdDataPtr);
	pArea->SetColdDataPtr(pColdDataPtr);

	pArea->Update(pPoints, nCount, sTemp, info);

	if (bPortal)
	{
		if (pArea->m_lstConnections.Count() == 1)
			pArea->UpdateGeometryBBox();

		m_lstPortals.Add(pArea);
	}
	else if (bVisArea)
	{
		m_lstVisAreas.Add(pArea);
	}
	else if (bOcclArea)
	{
		m_lstOcclAreas.Add(pArea);
	}

	UpdateConnections();

	// disable terrain culling for tunnels
	pArea->UpdateOcclusionFlagInTerrain();

	delete m_pAABBTree;
	m_pAABBTree = NULL;
}

void CVisAreaManager::UpdateConnections()
{
	// Reset connectivity
	for (int p = 0; p < m_lstPortals.Count(); p++)
		m_lstPortals[p]->m_lstConnections.Clear();

	for (int v = 0; v < m_lstVisAreas.Count(); v++)
		m_lstVisAreas[v]->m_lstConnections.Clear();

	// Init connectivity - check intersection of all areas and portals
	for (int p = 0; p < m_lstPortals.Count(); p++)
	{
		for (int v = 0; v < m_lstVisAreas.Count(); v++)
		{
			if (m_lstVisAreas[v]->IsPortalIntersectAreaInValidWay(m_lstPortals[p]))
			{
				// if bboxes intersect
				m_lstVisAreas[v]->m_lstConnections.Add(m_lstPortals[p]);
				m_lstPortals[p]->m_lstConnections.Add(m_lstVisAreas[v]);

				// set portal direction
				Vec3 vNormal = m_lstVisAreas[v]->GetConnectionNormal(m_lstPortals[p]);
				if (m_lstPortals[p]->m_lstConnections.Count() <= 2)
					m_lstPortals[p]->m_vConnNormals[m_lstPortals[p]->m_lstConnections.Count() - 1] = vNormal;
			}
		}
	}
}

void CVisAreaManager::MoveObjectsIntoList(PodArray<SRNInfo>* plstVisAreasEntities, const AABB* boxArea, bool bRemoveObjects)
{
	for (int p = 0; p < m_lstPortals.Count(); p++)
	{
		if (m_lstPortals[p]->IsObjectsTreeValid() && (!boxArea || Overlap::AABB_AABB(m_lstPortals[p]->m_boxArea, *boxArea)))
		{
			m_lstPortals[p]->GetObjectsTree()->MoveObjectsIntoList(plstVisAreasEntities, boxArea, bRemoveObjects);
		}
	}

	for (int v = 0; v < m_lstVisAreas.Count(); v++)
	{
		if (m_lstVisAreas[v]->IsObjectsTreeValid() && (!boxArea || Overlap::AABB_AABB(m_lstVisAreas[v]->m_boxArea, *boxArea)))
		{
			m_lstVisAreas[v]->GetObjectsTree()->MoveObjectsIntoList(plstVisAreasEntities, boxArea, bRemoveObjects);
		}
	}
}

void CVisAreaManager::CleanUpTrees()
{
	for (int p = 0; p < m_lstPortals.Count(); p++)
	{
		if (m_lstPortals[p]->IsObjectsTreeValid())
		{
			m_lstPortals[p]->GetObjectsTree()->CleanUpTree();
		}
	}

	for (int v = 0; v < m_lstVisAreas.Count(); v++)
	{
		if (m_lstVisAreas[v]->IsObjectsTreeValid())
		{
			m_lstVisAreas[v]->GetObjectsTree()->CleanUpTree();
		}
	}
}

bool CVisAreaManager::IntersectsVisAreas(const AABB& box, void** pNodeCache)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pAABBTree)
		UpdateAABBTree();
	SAABBTreeNode* pTopNode = m_pAABBTree->GetTopNode(box, pNodeCache);
	return pTopNode->IntersectsVisAreas(box);
}

bool CVisAreaManager::ClipOutsideVisAreas(Sphere& sphere, Vec3 const& vNormal, void* pNodeCache)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pAABBTree)
		UpdateAABBTree();
	AABB box(sphere.center - Vec3(sphere.radius), sphere.center + Vec3(sphere.radius));
	SAABBTreeNode* pTopNode = m_pAABBTree->GetTopNode(box, &pNodeCache);
	return pTopNode->ClipOutsideVisAreas(sphere, vNormal) > 0;
}

//This is used by the editor. Use the visareas pool for all areas, so prefetching
//	is still safe.
CVisArea* CVisAreaManager::CreateVisArea(VisAreaGUID visGUID)
{
	return new CVisArea(visGUID);
}

bool CVisAreaManager::IsEntityVisAreaVisibleReqursive(CVisArea* pVisArea, int nMaxReqursion, PodArray<CVisArea*>* pUnavailableAreas, const SRenderLight* pLight, const SRenderingPassInfo& passInfo)
{
	int nAreaId = pUnavailableAreas->Count();
	pUnavailableAreas->Add(pVisArea);

	bool bFound = false;
	if (pVisArea)
	{
		// check is lsource area was rendered in prev frame
		if (abs(pVisArea->m_nRndFrameId - passInfo.GetFrameID()) > 2)
		{
			if (nMaxReqursion > 1)
				for (int n = 0; n < pVisArea->m_lstConnections.Count(); n++)
				{
					// loop other sectors
					CVisArea* pNeibArea = (CVisArea*)pVisArea->m_lstConnections[n];
					if (-1 == pUnavailableAreas->Find(pNeibArea) &&
					    (!pLight || Overlap::Sphere_AABB(Sphere(pLight->m_Origin, pLight->m_fRadius), *pNeibArea->GetAABBox())))
						if (IsEntityVisAreaVisibleReqursive(pNeibArea, nMaxReqursion - 1, pUnavailableAreas, pLight, passInfo))
						{
							bFound = true;
							break;
						} //if visible
				}     // for
		}
		else
			bFound = true;
	}
	else if (IsOutdoorAreasVisible())                  //Indirect - outdoor can be a problem!
		bFound = true;

	pUnavailableAreas->Delete(nAreaId);
	return bFound;
}

bool CVisAreaManager::IsEntityVisAreaVisible(IRenderNode* pEnt, int nMaxReqursion, const SRenderLight* pLight, const SRenderingPassInfo& passInfo)
{
	if (!pEnt)
		return false;

	PodArray<CVisArea*>& lUnavailableAreas = m_tmpLstUnavailableAreas;
	lUnavailableAreas.Clear();

	lUnavailableAreas.PreAllocate(nMaxReqursion, 0);

	return IsEntityVisAreaVisibleReqursive((CVisArea*)pEnt->GetEntityVisArea(), nMaxReqursion, &lUnavailableAreas, pLight, passInfo);
	/*
	   if(pEnt->GetEntityVisArea())
	   {
	    if(pEnt->GetEntityVisArea())//->IsPortal())
	    { // check is lsource area was rendered in prev frame
	      CVisArea * pVisArea = pEnt->GetEntityVisArea();
	      int nRndFrameId = passInfo.GetFrameID();
	      if(abs(pVisArea->m_nRndFrameId - nRndFrameId)>2)
	      {
	        if(!nCheckNeighbors)
	          return false; // this area is not visible

	        // try neibhour areas
	        bool bFound = false;
	        if(pEnt->GetEntityVisArea()->IsPortal())
	        {
	          CVisArea * pPort = pEnt->GetEntityVisArea();
	          for(int n=0; n<pPort->m_lstConnections.Count(); n++)
	          { // loop other sectors
	            CVisArea * pNeibArea = (CVisArea*)pPort->m_lstConnections[n];
	            if(abs(pNeibArea->m_nRndFrameId - passInfo.GetFrameID())<=2)
	            {
	              bFound=true;
	              break;
	            }
	          }
	        }
	        else
	        {
	          for(int t=0; !bFound && t<pVisArea->m_lstConnections.Count(); t++)
	          { // loop portals
	            CVisArea * pPort = (CVisArea*)pVisArea->m_lstConnections[t];
	            if(abs(pPort->m_nRndFrameId - passInfo.GetFrameID())<=2)
	            {
	              bFound=true;
	              break;
	            }

	            for(int n=0; n<pPort->m_lstConnections.Count(); n++)
	            { // loop other sectors
	              CVisArea * pNeibArea = (CVisArea*)pPort->m_lstConnections[n];
	              if(abs(pNeibArea->m_nRndFrameId - passInfo.GetFrameID())<=2)
	              {
	                bFound=true;
	                break;
	              }
	            }
	          }
	        }

	        if(!bFound)
	          return false;

	        return true;
	      }
	    }
	    else
	      return false; // not visible
	   }
	   else if(!IsOutdoorAreasVisible())
	    return false;

	   return true;
	 */
}

int __cdecl CVisAreaManager__CmpDistToPortal(const void* v1, const void* v2)
{
	CVisArea* p1 = *((CVisArea**)v1);
	CVisArea* p2 = *((CVisArea**)v2);

	if (!p1 || !p2)
		return 0;

	if (p1->m_fDistance > p2->m_fDistance)
		return 1;
	else if (p1->m_fDistance < p2->m_fDistance)
		return -1;

	return 0;
}

void CVisAreaManager::MakeActiveEntransePortalsList(const CCamera* pCamera, PodArray<CVisArea*>& lstActiveEntransePortals, CVisArea* pThisPortal, const SRenderingPassInfo& passInfo)
{
	lstActiveEntransePortals.Clear();
	float fZoomFactor = passInfo.GetZoomFactor();

	for (int nPortalId = 0; nPortalId < m_lstPortals.Count(); nPortalId++)
	{
		CVisArea* pPortal = m_lstPortals[nPortalId];

		if (pPortal->m_lstConnections.Count() == 1 && pPortal != pThisPortal && pPortal->IsActive() && !pPortal->m_bSkyOnly)
		{
			if (!pCamera || pCamera->IsAABBVisible_F(pPortal->m_boxStatics))
			{
				Vec3 vNormal = pPortal->m_lstConnections[0]->GetConnectionNormal(pPortal);
				Vec3 vCenter = (pPortal->m_boxArea.min + pPortal->m_boxArea.max) * 0.5f;
				if (vNormal.Dot(vCenter - (pCamera ? pCamera->GetPosition() : passInfo.GetCamera().GetPosition())) < 0)
					continue;
				/*
				        if(pCurPortal)
				        {
				          vNormal = pCurPortal->m_vConnNormals[0];
				          if(vNormal.Dot(vCenter - curCamera.GetPosition())<0)
				            continue;
				        }
				 */
				pPortal->m_fDistance = pPortal->m_boxArea.GetDistance(pCamera ? pCamera->GetPosition() : passInfo.GetCamera().GetPosition());

				float fRadius = (pPortal->m_boxArea.max - pPortal->m_boxArea.min).GetLength() * 0.5f;
				if (pPortal->m_fDistance * fZoomFactor > fRadius * pPortal->m_fViewDistRatio * GetFloatCVar(e_ViewDistRatioPortals) / 60.f)
					continue;

				SPortalColdData* pColdData = static_cast<SPortalColdData*>(pPortal->GetColdData());

				// test occlusion
				if (GetObjManager()->IsBoxOccluded(pPortal->m_boxStatics, pPortal->m_fDistance, &pColdData->m_occlusionTestClient, false, eoot_PORTAL, passInfo))
					continue;

				lstActiveEntransePortals.Add(pPortal);

				//				if(GetCVars()->e_Portals==3)
				//				DrawBBox(pPortal->m_boxStatics.min, pPortal->m_boxStatics.max);
			}
		}
	}

	// sort by distance
	if (lstActiveEntransePortals.Count())
	{
		qsort(&lstActiveEntransePortals[0], lstActiveEntransePortals.Count(),
		      sizeof(lstActiveEntransePortals[0]), CVisAreaManager__CmpDistToPortal);
		//		m_pCurPortal = lstActiveEntransePortals[0];
	}
}

void CVisAreaManager::MergeCameras(CCamera& cam1, const CCamera& cam2, const SRenderingPassInfo& passInfo)
{
	assert(0); // under development
	/*	{
	    float fDotLR1 =  cam1.GetFrustumPlane(FR_PLANE_LEFT )->n.Dot(cam1.GetFrustumPlane(FR_PLANE_RIGHT)->n);
	    float fDotRL2 =  cam2.GetFrustumPlane(FR_PLANE_RIGHT)->n.Dot(cam2.GetFrustumPlane(FR_PLANE_LEFT)->n);
	    int y=0;
	   }
	 */
	// left-right
	float fDotLR = cam1.GetFrustumPlane(FR_PLANE_LEFT)->n.Dot(cam2.GetFrustumPlane(FR_PLANE_RIGHT)->n);
	float fDotRL = cam1.GetFrustumPlane(FR_PLANE_RIGHT)->n.Dot(cam2.GetFrustumPlane(FR_PLANE_LEFT)->n);
	if (fabs(fDotLR) < fabs(fDotRL))
	{
		cam1.SetFrustumPlane(FR_PLANE_RIGHT, *cam2.GetFrustumPlane(FR_PLANE_RIGHT));
		cam1.SetPPVertex(2, cam2.GetPPVertex(2));
		cam1.SetPPVertex(3, cam2.GetPPVertex(3));
	}
	else
	{
		cam1.SetFrustumPlane(FR_PLANE_LEFT, *cam2.GetFrustumPlane(FR_PLANE_LEFT));
		cam1.SetPPVertex(0, cam2.GetPPVertex(0));
		cam1.SetPPVertex(1, cam2.GetPPVertex(1));
	}

	/*
	   {
	   float fDotLR1 =  cam1.GetFrustumPlane(FR_PLANE_LEFT )->n.Dot(cam1.GetFrustumPlane(FR_PLANE_RIGHT)->n);
	   float fDotRL2 =  cam2.GetFrustumPlane(FR_PLANE_RIGHT)->n.Dot(cam2.GetFrustumPlane(FR_PLANE_LEFT)->n);
	   int y=0;
	   }*/
	/*
	   // top-bottom
	   float fDotTB =  cam1.GetFrustumPlane(FR_PLANE_TOP   )->n.Dot(cam2.GetFrustumPlane(FR_PLANE_BOTTOM)->n);
	   float fDotBT =  cam1.GetFrustumPlane(FR_PLANE_BOTTOM)->n.Dot(cam2.GetFrustumPlane(FR_PLANE_TOP   )->n);

	   if(fDotTB>fDotBT)
	    cam1.SetFrustumPlane(FR_PLANE_BOTTOM, *cam2.GetFrustumPlane(FR_PLANE_BOTTOM));
	   else
	    cam1.SetFrustumPlane(FR_PLANE_TOP,    *cam2.GetFrustumPlane(FR_PLANE_TOP));
	 */
	cam1.SetFrustumPlane(FR_PLANE_NEAR, *passInfo.GetCamera().GetFrustumPlane(FR_PLANE_NEAR));
	cam1.SetFrustumPlane(FR_PLANE_FAR, *passInfo.GetCamera().GetFrustumPlane(FR_PLANE_FAR));

	cam1.SetFrustumPlane(FR_PLANE_TOP, *passInfo.GetCamera().GetFrustumPlane(FR_PLANE_TOP));
	cam1.SetFrustumPlane(FR_PLANE_BOTTOM, *passInfo.GetCamera().GetFrustumPlane(FR_PLANE_BOTTOM));

	//cam1.UpdateFrustum();
	/*
	   if(GetCVars()->e_Portals==4)
	   {
	    GetRenderer()->SetMaterialColor(1,1,1,1);
	    DrawBBox(pVerts[0],pVerts[1],DPRIM_LINE);
	    GetRenderer()->DrawLabel(pVerts[0],2,"0");
	    DrawBBox(pVerts[1],pVerts[2],DPRIM_LINE);
	    GetRenderer()->DrawLabel(pVerts[1],2,"1");
	    DrawBBox(pVerts[2],pVerts[3],DPRIM_LINE);
	    GetRenderer()->DrawLabel(pVerts[2],2,"2");
	    DrawBBox(pVerts[3],pVerts[0],DPRIM_LINE);
	    GetRenderer()->DrawLabel(pVerts[3],2,"3");
	   }*/
}

void CVisAreaManager::DrawOcclusionAreasIntoCBuffer(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	m_lstActiveOcclVolumes.Clear();
	m_lstIndoorActiveOcclVolumes.Clear();

	float fDistRatio = GetFloatCVar(e_OcclusionVolumesViewDistRatio) * passInfo.GetInverseZoomFactor();

	if (GetCVars()->e_OcclusionVolumes)
		for (int i = 0; i < m_lstOcclAreas.Count(); i++)
		{
			CVisArea* pArea = m_lstOcclAreas[i];
			if (pArea->m_bActive && passInfo.GetCamera().IsAABBVisible_E(pArea->m_boxArea))
			{
				float fRadius = (pArea->m_boxArea.min - pArea->m_boxArea.max).GetLength();
				Vec3 vPos = (pArea->m_boxArea.min + pArea->m_boxArea.max) * 0.5f;
				float fDist = passInfo.GetCamera().GetPosition().GetDistance(vPos);
				if (fDist < fRadius * pArea->m_fViewDistRatio * fDistRatio && pArea->m_lstShapePoints.Count() >= 2)
				{
					int nRecursiveLevel = passInfo.GetRecursiveLevel();
					if (!pArea->m_arrOcclCamera[nRecursiveLevel])
						pArea->m_arrOcclCamera[nRecursiveLevel] = new CCamera;
					*pArea->m_arrOcclCamera[nRecursiveLevel] = passInfo.GetCamera();

					SActiveVerts activeVerts;

					if (pArea->m_lstShapePoints.Count() == 4)
					{
						activeVerts.arrvActiveVerts[0] = pArea->m_lstShapePoints[0];
						activeVerts.arrvActiveVerts[1] = pArea->m_lstShapePoints[1];
						activeVerts.arrvActiveVerts[2] = pArea->m_lstShapePoints[2];
						activeVerts.arrvActiveVerts[3] = pArea->m_lstShapePoints[3];
					}
					else
					{
						CRY_ASSERT_MESSAGE(pArea->m_lstShapePoints.Count() == 2, "Occlusion area only supports planes or quads");

						activeVerts.arrvActiveVerts[0] = pArea->m_lstShapePoints[0];
						activeVerts.arrvActiveVerts[1] = pArea->m_lstShapePoints[0] + Vec3(0, 0, pArea->m_fHeight);
						activeVerts.arrvActiveVerts[2] = pArea->m_lstShapePoints[1] + Vec3(0, 0, pArea->m_fHeight);
						activeVerts.arrvActiveVerts[3] = pArea->m_lstShapePoints[1];
					}

					Plane plane;
					plane.SetPlane(activeVerts.arrvActiveVerts[0], activeVerts.arrvActiveVerts[2], activeVerts.arrvActiveVerts[1]);

					if (plane.DistFromPlane(passInfo.GetCamera().GetPosition()) < 0)
					{
						std::swap(activeVerts.arrvActiveVerts[0], activeVerts.arrvActiveVerts[3]);
						std::swap(activeVerts.arrvActiveVerts[1], activeVerts.arrvActiveVerts[2]);
					}
					else if (!pArea->m_bDoubleSide)
						continue;

					//				GetRenderer()->SetMaterialColor(1,0,0,1);
					pArea->UpdatePortalCameraPlanes(*pArea->m_arrOcclCamera[passInfo.GetRecursiveLevel()], activeVerts.arrvActiveVerts, false, passInfo);

					// make far plane never clip anything

					Plane newNearPlane;
					newNearPlane.SetPlane(activeVerts.arrvActiveVerts[0], activeVerts.arrvActiveVerts[2], activeVerts.arrvActiveVerts[1]);
					pArea->m_arrOcclCamera[passInfo.GetRecursiveLevel()]->SetFrustumPlane(FR_PLANE_NEAR, newNearPlane);

					Plane newFarPlane;
					newFarPlane.SetPlane(Vec3(0, 1, -1024), Vec3(1, 0, -1024), Vec3(0, 0, -1024));
					pArea->m_arrOcclCamera[passInfo.GetRecursiveLevel()]->SetFrustumPlane(FR_PLANE_FAR, newFarPlane);
					//pArea->m_arrOcclCamera[m_nRenderStackLevel]->UpdateFrustum();

					m_lstActiveOcclVolumes.Add(pArea);
					pArea->m_fDistance = fDist;
				}
			}
		}

	if (m_lstActiveOcclVolumes.Count())
	{
		// sort occluders by distance to the camera
		qsort(&m_lstActiveOcclVolumes[0], m_lstActiveOcclVolumes.Count(),
		      sizeof(m_lstActiveOcclVolumes[0]), CVisAreaManager__CmpDistToPortal);

		// remove occluded occluders
		for (int i = m_lstActiveOcclVolumes.Count() - 1; i >= 0; i--)
		{
			CVisArea* pArea = m_lstActiveOcclVolumes[i];
			AABB extrudedBox = pArea->m_boxStatics;
			extrudedBox.min -= Vec3(VEC_EPSILON, VEC_EPSILON, VEC_EPSILON);
			extrudedBox.max += Vec3(VEC_EPSILON, VEC_EPSILON, VEC_EPSILON);
			if (IsOccludedByOcclVolumes(extrudedBox, passInfo))
				m_lstActiveOcclVolumes.Delete(i);
		}

		// put indoor occluders into separate list
		for (int i = m_lstActiveOcclVolumes.Count() - 1; i >= 0; i--)
		{
			CVisArea* pArea = m_lstActiveOcclVolumes[i];
			if (pArea->m_bUseInIndoors)
				m_lstIndoorActiveOcclVolumes.Add(pArea);
		}

		if (GetCVars()->e_Portals == 4)
		{
			// show really active occluders
			for (int i = 0; i < m_lstActiveOcclVolumes.Count(); i++)
			{
				CVisArea* pArea = m_lstActiveOcclVolumes[i];
				DrawBBox(pArea->m_boxStatics.min, pArea->m_boxStatics.max, ColorB(0, 255, 0));
			}
		}
	}
}

/*
   void CVisAreaManager::CompileObjects()
   {
   // areas
   for(int v=0; v<m_lstVisAreas.Count(); v++)
    m_lstVisAreas[v]->CompileObjects(STATIC_OBJECTS);

   // portals
   for(int v=0; v<m_lstPortals.Count(); v++)
    m_lstPortals[v]->CompileObjects(STATIC_OBJECTS);
   }
 */
/*
   void CVisAreaManager::CheckUnload()
   {
   m_nLoadedSectors=0;

   // areas
   for(int v=0; v<m_lstVisAreas.Count(); v++)
    m_nLoadedSectors += m_lstVisAreas[v]->CheckUnload();

   // portals
   for(int v=0; v<m_lstPortals.Count(); v++)
    m_nLoadedSectors += m_lstPortals[v]->CheckUnload();
   }
 */
void CVisAreaManager::GetStreamingStatus(int& nLoadedSectors, int& nTotalSectors)
{
	nLoadedSectors = 0;//m_nLoadedSectors;
	nTotalSectors = m_lstPortals.Count() + m_lstVisAreas.Count();
}

void CVisAreaManager::GetMemoryUsage(ICrySizer* pSizer) const
{
	// areas
	for (int v = 0; v < m_lstVisAreas.Count(); v++)
		m_lstVisAreas[v]->GetMemoryUsage(pSizer);

	// portals
	for (int v = 0; v < m_lstPortals.Count(); v++)
		m_lstPortals[v]->GetMemoryUsage(pSizer);

	// occl areas
	for (int v = 0; v < m_lstOcclAreas.Count(); v++)
		m_lstOcclAreas[v]->GetMemoryUsage(pSizer);

	pSizer->AddObject(this, sizeof(*this));
}

void CVisAreaManager::PrecacheLevel(bool bPrecacheAllVisAreas, Vec3* pPrecachePoints, int nPrecachePointsNum)
{
	CryLog("Precaching the level ...");
	//  gEnv->pLog->UpdateLoadingScreen(0);

	float fPrecacheTimeStart = GetTimer()->GetAsyncCurTime();

	GetRenderer()->EnableSwapBuffers((GetCVars()->e_PrecacheLevel >= 2) ? true : false);

	uint32 dwPrecacheLocations = 0;

	Vec3 arrCamDir[6] =
	{
		Vec3(1,  0,  0),
		Vec3(-1, 0,  0),
		Vec3(0,  1,  0),
		Vec3(0,  -1, 0),
		Vec3(0,  0,  1),
		Vec3(0,  0,  -1)
	};

	//loop over all sectors and place a light in the middle of the sector
	for (int v = 0; v < m_lstVisAreas.Count() && bPrecacheAllVisAreas; v++)
	{
		GetRenderer()->EF_Query(EFQ_IncrementFrameID);

		++dwPrecacheLocations;

		// find real geometry bbox
		/*		bool bGeomFound = false;
		    Vec3 vBoxMin(100000.f,100000.f,100000.f);
		    Vec3 vBoxMax(-100000.f,-100000.f,-100000.f);
		    for(int s=0; s<ENTITY_LISTS_NUM; s++)
		    for(int e=0; e<m_lstVisAreas[v]->m_lstEntities[s].Count(); e++)
		    {
		      AABB aabbBox = m_lstVisAreas[v]->m_lstEntities[s][e].aabbBox;
		      vBoxMin.CheckMin(aabbBox.min);
		      vBoxMax.CheckMax(aabbBox.max);
		      bGeomFound = true;
		    }*/

		Vec3 vAreaCenter = m_lstVisAreas[v]->m_boxArea.GetCenter();
		CryLog("  Precaching VisArea %s", m_lstVisAreas[v]->GetName());

		//place camera in the middle of a sector and render sector from different directions
		for (int i = 0; i < 6 /*&& bGeomFound*/; i++)
		{
			// setup camera
			CCamera cam = gEnv->pSystem->GetViewCamera();
			Matrix33 mat = Matrix33::CreateRotationVDir(arrCamDir[i], 0);
			cam.SetMatrix(mat);
			cam.SetPosition(vAreaCenter);
			cam.SetFrustum(GetRenderer()->GetOverlayWidth(), GetRenderer()->GetOverlayHeight(), gf_PI / 2, cam.GetNearPlane(), cam.GetFarPlane());
			//	Get3DEngine()->SetupCamera(cam);

			GetRenderer()->BeginFrame({});
			Get3DEngine()->RenderWorld(SHDF_ZPASS | SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS | SHDF_ALLOW_WATER | SHDF_ALLOW_AO, SRenderingPassInfo::CreateGeneralPassRenderingInfo(cam), "PrecacheVisAreas");
			GetRenderer()->RenderDebug();
			GetRenderer()->EndFrame();

			if (GetCVars()->e_PrecacheLevel >= 2)
				CrySleep(200);
		}
	}

	CryLog("Precached %u visarea sectors", dwPrecacheLocations);

	//--------------------------------------------------------------------------------------
	//----                  PRE-FETCHING OF RENDER-DATA IN OUTDOORS                     ----
	//--------------------------------------------------------------------------------------

	// loop over all cam-position in the level and render this part of the level from 6 different directions
	for (int p = 0; pPrecachePoints && p < nPrecachePointsNum; p++) //loop over outdoor-camera position
	{
		CryLog("  Precaching PrecacheCamera point %d of %d", p, nPrecachePointsNum);
		for (int i = 0; i < 6; i++) //loop over 6 camera orientations
		{
			// setup camera
			CCamera cam = gEnv->pSystem->GetViewCamera();
			Matrix33 mat = Matrix33::CreateRotationVDir(arrCamDir[i], 0);
			cam.SetMatrix(mat);
			cam.SetPosition(pPrecachePoints[p]);
			cam.SetFrustum(GetRenderer()->GetOverlayWidth(), GetRenderer()->GetOverlayHeight(), gf_PI / 2, cam.GetNearPlane(), cam.GetFarPlane());

			GetRenderer()->BeginFrame({});
			Get3DEngine()->RenderWorld(SHDF_ZPASS | SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS | SHDF_ALLOW_WATER | SHDF_ALLOW_AO, SRenderingPassInfo::CreateGeneralPassRenderingInfo(cam), "PrecacheOutdoor");
			GetRenderer()->RenderDebug();
			GetRenderer()->EndFrame();

			if (GetCVars()->e_PrecacheLevel >= 2)
				CrySleep(1000);
		}
	}

	CryLog("Precached %d PrecacheCameraXX points", nPrecachePointsNum);

	GetRenderer()->EnableSwapBuffers(true);

	float fPrecacheTime = GetTimer()->GetAsyncCurTime() - fPrecacheTimeStart;
	CryLog("Level Precache finished in %.2f seconds", fPrecacheTime);
}

void CVisAreaManager::GetObjectsAround(Vec3 vExploPos, float fExploRadius, PodArray<SRNInfo>* pEntList, bool bSkip_ERF_NO_DECALNODE_DECALS, bool bSkipDynamicObjects)
{
	AABB aabbBox(vExploPos - Vec3(fExploRadius, fExploRadius, fExploRadius), vExploPos + Vec3(fExploRadius, fExploRadius, fExploRadius));

	CVisArea* pVisArea = (CVisArea*)GetVisAreaFromPos(vExploPos);

	if (pVisArea && pVisArea->IsObjectsTreeValid())
	{
		pVisArea->GetObjectsTree()->MoveObjectsIntoList(pEntList, &aabbBox, false, true, bSkip_ERF_NO_DECALNODE_DECALS, bSkipDynamicObjects);
	}

	/*
	   // find static objects around
	   for(int i=0; pVisArea && i<pVisArea->m_lstEntities[STATIC_OBJECTS].Count(); i++)
	   {
	    IRenderNode * pRenderNode =	pVisArea->m_lstEntities[STATIC_OBJECTS][i].pNode;
	    if(bSkip_ERF_NO_DECALNODE_DECALS && pRenderNode->GetRndFlags()&ERF_NO_DECALNODE_DECALS)
	      continue;

	    if(pRenderNode->GetRenderNodeType() == eERType_Decal)
	      continue;

	    if(Overlap::Sphere_AABB(Sphere(vExploPos,fExploRadius), pRenderNode->GetBBox()))
	      if(pEntList->Find(pRenderNode)<0)
	        pEntList->Add(pRenderNode);
	   }*/
}

void CVisAreaManager::IntersectWithBox(const AABB& aabbBox, PodArray<CVisArea*>* plstResult, bool bOnlyIfVisible)
{
	for (int p = 0; p < m_lstPortals.Count(); p++)
	{
		if (m_lstPortals[p]->m_boxArea.min.x<aabbBox.max.x&& m_lstPortals[p]->m_boxArea.max.x> aabbBox.min.x &&
		    m_lstPortals[p]->m_boxArea.min.y<aabbBox.max.y&& m_lstPortals[p]->m_boxArea.max.y> aabbBox.min.y)
		{
			plstResult->Add(m_lstPortals[p]);
		}
	}

	for (int v = 0; v < m_lstVisAreas.Count(); v++)
	{
		if (m_lstVisAreas[v]->m_boxArea.min.x<aabbBox.max.x&& m_lstVisAreas[v]->m_boxArea.max.x> aabbBox.min.x &&
		    m_lstVisAreas[v]->m_boxArea.min.y<aabbBox.max.y&& m_lstVisAreas[v]->m_boxArea.max.y> aabbBox.min.y)
		{
			plstResult->Add(m_lstVisAreas[v]);
		}
	}
}

void CVisAreaManager::AddLightSourceReqursive(SRenderLight* pLight, CVisArea* pArea, const int32 nDeepness, const SRenderingPassInfo& passInfo)
{
	if (pArea->IsObjectsTreeValid())
	{
		pArea->GetObjectsTree()->AddLightSource(pLight, passInfo);
	}

	if (1 < nDeepness)
	{
		for (int v = 0; v < pArea->m_lstConnections.Count(); v++)
		{
			CVisArea* pConArea = pArea->m_lstConnections[v];
			AddLightSourceReqursive(pLight, pConArea, nDeepness - 1, passInfo);
		}
	}
}

void CVisAreaManager::AddLightSource(SRenderLight* pLight, const SRenderingPassInfo& passInfo)
{
	CVisArea* pLightArea = (CVisArea*)GetVisAreaFromPos(pLight->m_Origin);

	bool bThisAreaInly = (pLight->m_Flags & DLF_THIS_AREA_ONLY) != 0;

	if (pLightArea)
	{
		int nPortal = pLightArea->IsPortal() ? 1 : 0;
		AddLightSourceReqursive(pLight, pLightArea, nPortal + (bThisAreaInly ? 2 : 3), passInfo);
	}
	else if (!bThisAreaInly)
	{
		AABB lightBox(
		  pLight->m_Origin - Vec3(pLight->m_fRadius, pLight->m_fRadius, pLight->m_fRadius),
		  pLight->m_Origin + Vec3(pLight->m_fRadius, pLight->m_fRadius, pLight->m_fRadius));
		PodArray<CVisArea*>& arrAreas = m_tmpLstLightBoxAreas;
		arrAreas.Clear();
		m_pVisAreaManager->IntersectWithBox(lightBox, &arrAreas, true);
		for (int i = 0; i < arrAreas.Count(); i++)
			if (arrAreas[i]->IsPortal() && arrAreas[i]->IsConnectedToOutdoor())
			{
				pLightArea = arrAreas[i];
				int nPortal = pLightArea->IsPortal() ? 1 : 0;
				AddLightSourceReqursive(pLight, pLightArea, nPortal + (bThisAreaInly ? 2 : 3), passInfo);
			}
	}
}

int CVisAreaManager::GetNumberOfVisArea() const
{
	return m_lstPortals.size() + m_lstVisAreas.size();
}

IVisArea* CVisAreaManager::GetVisAreaById(int nID) const
{
	if (nID < 0)
		return NULL;

	if (nID < (int)m_lstPortals.size())
	{
		return m_lstPortals[nID];
	}
	nID -= m_lstPortals.size();
	if (nID < (int)m_lstVisAreas.size())
	{
		return m_lstVisAreas[nID];
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaManager::AddListener(IVisAreaCallback* pListener)
{
	if (m_lstCallbacks.Find(pListener) < 0)
		m_lstCallbacks.Add(pListener);
}

//////////////////////////////////////////////////////////////////////////
void CVisAreaManager::RemoveListener(IVisAreaCallback* pListener)
{
	m_lstCallbacks.Delete(pListener);
}

void CVisAreaManager::CloneRegion(const AABB& region, const Vec3& offset, float zRotation)
{
	PodArray<Vec3> points;

	PodArray<CVisArea*> visAreas;
	IntersectWithBox(region, &visAreas, false);

	Vec3 localOrigin = region.GetCenter();
	Matrix34 l2w(Matrix33::CreateRotationZ(zRotation));
	l2w.SetTranslation(offset);

	int numAreas = visAreas.size();
	for (int i = 0; i < numAreas; ++i)
	{
		CVisArea* pSrcArea = visAreas[i];
		CVisArea* pCloneArea = CreateVisArea(0);

		SVisAreaInfo info;
		info.fHeight = pSrcArea->m_fHeight;
		info.vAmbientColor = pSrcArea->m_vAmbientColor;
		info.bAffectedByOutLights = pSrcArea->m_bAffectedByOutLights;
		info.bIgnoreSkyColor = pSrcArea->m_bIgnoreSky;
		info.bSkyOnly = pSrcArea->m_bSkyOnly;
		info.fViewDistRatio = pSrcArea->m_fViewDistRatio;
		info.bDoubleSide = pSrcArea->m_bDoubleSide;
		info.bUseDeepness = pSrcArea->m_bUseDeepness;
		info.bUseInIndoors = pSrcArea->m_bUseInIndoors;
		info.bOceanIsVisible = pSrcArea->m_bOceanVisible;
		info.bIgnoreGI = pSrcArea->m_bIgnoreGI;
		info.bIgnoreOutdoorAO = pSrcArea->m_bIgnoreOutdoorAO;

		points = pSrcArea->m_lstShapePoints;
		int numPoints = points.size();
		for (int p = 0; p < numPoints; ++p)
		{
			Vec3& point = points[p];

			point -= localOrigin;
			point = l2w * point;
		}

		const char* pName = pSrcArea->m_pVisAreaColdData->m_sName;

		UpdateVisArea(pCloneArea, &points[0], numPoints, pName, info);
	}
}

void CVisAreaManager::ClearRegion(const AABB& region)
{
	PodArray<CVisArea*> visAreas;
	IntersectWithBox(region, &visAreas, false);

	bool updated = false;

	// What we're doing here is basically just what's done in DeleteVisArea, but this
	// should be a pooled vis area, so we don't want to actually delete it.  Instead we
	// just unregister them and let the pool cleanup actually destruct them.
	int numAreas = visAreas.size();
	for (int i = 0; i < numAreas; ++i)
	{
		CVisArea* pVisArea = visAreas[i];

		// IntersectWithBox only checks x and y, but we want to also make sure it's in the z
		if (pVisArea->m_boxArea.min.z < region.max.z && pVisArea->m_boxArea.max.z > region.min.z)
		{
			bool deletedVis = m_lstVisAreas.Delete(pVisArea);
			bool deletedPortal = m_lstPortals.Delete(pVisArea);
			bool deletedOccluder = m_lstOcclAreas.Delete(pVisArea);

			CRY_ASSERT_MESSAGE(!deletedVis || (m_visAreas.Find(pVisArea) >= 0), "Should only clear pooled vis areas, going to leak");
			CRY_ASSERT_MESSAGE(!deletedPortal || (m_portals.Find(pVisArea) >= 0), "Should only clear pooled portals, going to leak");
			CRY_ASSERT_MESSAGE(!deletedOccluder || (m_occlAreas.Find(pVisArea) >= 0), "Should only clear pooled occluders, going to leak");

			if (deletedVis || deletedPortal || deletedOccluder)
			{
				updated = true;
			}

			m_lstActiveOcclVolumes.Delete(pVisArea);
			m_lstIndoorActiveOcclVolumes.Delete(pVisArea);
			m_lstActiveEntransePortals.Delete(pVisArea);
		}
	}

	if (updated)
	{
		m_pCurArea = NULL;
		m_pCurPortal = NULL;
		UpdateConnections();

		delete m_pAABBTree;
		m_pAABBTree = NULL;
	}
}

namespace
{
inline void HelperMarkAllSectorsAsUncompiled(PodArray<CVisArea*>& arrayVisArea)
{
	int s = arrayVisArea.Count();
	for (int i = 0; i < s; ++i)
	{
		if (arrayVisArea[i]->IsObjectsTreeValid())
		{
			arrayVisArea[i]->GetObjectsTree()->MarkAsUncompiled();
		}
	}
}

inline void HelperActivateObjectsLayer(PodArray<CVisArea*>& arrayVisArea, uint16 nLayerId, bool bActivate, bool bPhys, IGeneralMemoryHeap* pHeap, const AABB& layerBox)
{
	int s = arrayVisArea.Count();
	for (int i = 0; i < s; ++i)
	{
		if (arrayVisArea[i]->IsObjectsTreeValid())
		{
			arrayVisArea[i]->GetObjectsTree()->ActivateObjectsLayer(nLayerId, bActivate, bPhys, pHeap, layerBox);
		}
	}
}

inline void HelperGetObjects(PodArray<CVisArea*>& arrayVisArea, PodArray<IRenderNode*>& lstObjects, const AABB* pBBox)
{
	int s = arrayVisArea.Count();
	for (int i = 0; i < s; ++i)
	{
		if (arrayVisArea[i]->IsObjectsTreeValid())
		{
			arrayVisArea[i]->GetObjectsTree()->GetObjects(lstObjects, pBBox);
		}
	}
}

inline void HelperGetObjectsByFlags(PodArray<CVisArea*>& arrayVisArea, uint dwFlags, PodArray<IRenderNode*>& lstObjects)
{
	int s = arrayVisArea.Count();
	for (int i = 0; i < s; ++i)
	{
		if (arrayVisArea[i]->IsObjectsTreeValid())
		{
			arrayVisArea[i]->GetObjectsTree()->GetObjectsByFlags(dwFlags, lstObjects);
		}
	}
}

inline void HelperGenerateStatObjAndMatTables(PodArray<CVisArea*>& arrayVisArea, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, SHotUpdateInfo* pExportInfo)
{
	int s = arrayVisArea.Count();
	for (int i = 0; i < s; ++i)
	{
		if (arrayVisArea[i]->IsObjectsTreeValid())
		{
			arrayVisArea[i]->GetObjectsTree()->GenerateStatObjAndMatTables(pStatObjTable, pMatTable, pStatInstGroupTable, pExportInfo);
		}
	}
}
}

void CVisAreaManager::MarkAllSectorsAsUncompiled()
{
	HelperMarkAllSectorsAsUncompiled(m_lstPortals);
	HelperMarkAllSectorsAsUncompiled(m_lstVisAreas);
}

void CVisAreaManager::ActivateObjectsLayer(uint16 nLayerId, bool bActivate, bool bPhys, IGeneralMemoryHeap* pHeap, const AABB& layerBox)
{
	HelperActivateObjectsLayer(m_lstVisAreas, nLayerId, bActivate, bPhys, pHeap, layerBox);
	HelperActivateObjectsLayer(m_lstPortals, nLayerId, bActivate, bPhys, pHeap, layerBox);
}

void CVisAreaManager::GetObjects(PodArray<IRenderNode*>& lstObjects, const AABB* pBBox)
{
	HelperGetObjects(m_lstVisAreas, lstObjects, pBBox);
	HelperGetObjects(m_lstPortals, lstObjects, pBBox);
}

void CVisAreaManager::GetObjectsByFlags(uint dwFlags, PodArray<IRenderNode*>& lstObjects)
{
	HelperGetObjectsByFlags(m_lstVisAreas, dwFlags, lstObjects);
	HelperGetObjectsByFlags(m_lstPortals, dwFlags, lstObjects);
}

void CVisAreaManager::GenerateStatObjAndMatTables(std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, SHotUpdateInfo* pExportInfo)
{
	HelperGenerateStatObjAndMatTables(m_lstVisAreas, pStatObjTable, pMatTable, pStatInstGroupTable, pExportInfo);
	HelperGenerateStatObjAndMatTables(m_lstPortals, pStatObjTable, pMatTable, pStatInstGroupTable, pExportInfo);
}

bool CVisAreaManager::IsAABBVisibleFromPoint(AABB& box, Vec3 pos)
{
	CVisArea* pAreaBox = (CVisArea*)GetVisAreaFromPos(box.GetCenter());
	CVisArea* pAreaPos = (CVisArea*)GetVisAreaFromPos(pos);

	if (!pAreaBox && !pAreaPos)
		return true; // no indoors involved

	PodArray<CVisArea*> arrPortals;
	int nRecursion = 0;
	Shadowvolume sv;
	NAABB_SV::AABB_ReceiverShadowVolume(pos, box, sv);

	bool bRes = FindShortestPathToVisArea(pAreaPos, pAreaBox, arrPortals, nRecursion, sv);

	IRenderAuxText::DrawLabelF(box.GetCenter(), 2, "-%s-", bRes ? "Y" : "N");
	IRenderAuxText::DrawLabel(pos, 2, "-X-");
	DrawLine(pos, box.GetCenter());
	DrawBBox(box, bRes ? Col_White : Col_NavyBlue);

	return bRes;
}

bool CVisAreaManager::FindShortestPathToVisArea(CVisArea* pThisArea, CVisArea* pTargetArea, PodArray<CVisArea*>& arrVisitedAreas, int& nRecursion, const Shadowvolume& sv)
{
	// skip double processing
	if (arrVisitedAreas.Find(pThisArea) >= 0)
		return false;

	// check if point to box frustum intersects pThisArea visarea
	if (pThisArea && !NAABB_SV::Is_AABB_In_ShadowVolume(sv, *pThisArea->GetAABBox()))
		return false;

	// check if box visarea reached
	if (pThisArea == pTargetArea)
		return true;

	// register as already processed
	arrVisitedAreas.Add(pThisArea);

	// recurse to connections
	if (pThisArea)
	{
		for (int p = 0; p < pThisArea->m_lstConnections.Count(); p++)
			if (FindShortestPathToVisArea(pThisArea->m_lstConnections[p], pTargetArea, arrVisitedAreas, nRecursion, sv))
				return true;

		if (pThisArea->IsPortal() && pThisArea->m_lstConnections.Count() == 1 && !pThisArea->m_bSkyOnly)
			if (FindShortestPathToVisArea(NULL, pTargetArea, arrVisitedAreas, nRecursion, sv))
				return true;
	}
	else
	{
		for (int p = 0; p < m_lstPortals.Count(); p++)
			if (m_lstPortals[p]->IsPortal() && m_lstPortals[p]->m_lstConnections.Count() == 1 && !m_lstPortals[p]->m_bSkyOnly)
				if (FindShortestPathToVisArea(m_lstPortals[p], pTargetArea, arrVisitedAreas, nRecursion, sv))
					return true;
	}

	return false;
}

CVisArea* CVisAreaManager::CreateTypeVisArea()
{
	CVisArea* pNewVisArea = new CVisArea();
	SGenericColdData* pColdData = &m_visAreaColdData.AddNew();

	m_visAreas.Add(pNewVisArea);
	pColdData->ResetGenericData();
	pNewVisArea->SetColdDataPtr(pColdData);

	return pNewVisArea;
}

CVisArea* CVisAreaManager::CreateTypePortal()
{
	CVisArea* pNewPortal = new CVisArea();
	SPortalColdData* pColdData = &m_portalColdData.AddNew();

	m_portals.Add(pNewPortal);
	pColdData->ResetPortalData();
	pNewPortal->SetColdDataPtr(pColdData);

	return pNewPortal;
}

CVisArea* CVisAreaManager::CreateTypeOcclArea()
{
	CVisArea* pNewOcclArea = new CVisArea();
	SGenericColdData* pColdData = &m_occlAreaColdData.AddNew();

	m_occlAreas.Add(pNewOcclArea);
	pColdData->ResetGenericData();
	pNewOcclArea->SetColdDataPtr(pColdData);

	return pNewOcclArea;
}

void CVisAreaManager::InitAABBTree()
{
	IF (!m_pAABBTree, 0)
	{
		UpdateAABBTree();
	}
}

CVisArea* CVisAreaManager::FindVisAreaByGuid(VisAreaGUID guid, PodArray<CVisArea*>& lstVisAreas)
{
	if (!guid)
	{
		for (int i = 0; i < lstVisAreas.Count(); i++)
		{
			if (lstVisAreas[i] && guid == lstVisAreas[i]->m_nVisGUID)
				return lstVisAreas[i];
		}
	}

	return NULL;
}

void CVisAreaManager::OffsetPosition(const Vec3& delta)
{
	for (int i = 0; i < m_lstVisAreas.Count(); i++)
		m_lstVisAreas[i]->OffsetPosition(delta);

	for (int i = 0; i < m_lstPortals.Count(); i++)
		m_lstPortals[i]->OffsetPosition(delta);

	for (int i = 0; i < m_lstOcclAreas.Count(); i++)
		m_lstOcclAreas[i]->OffsetPosition(delta);

	if (m_pAABBTree)
		m_pAABBTree->OffsetPosition(delta);
}
