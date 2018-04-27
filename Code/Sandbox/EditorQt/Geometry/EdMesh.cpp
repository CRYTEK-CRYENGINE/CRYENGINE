// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EdMesh.h"
#include "Objects\DisplayContext.h"
#include "Objects\ObjectLoader.h"
#include "Gizmos/ITransformManipulator.h"
#include "Viewport.h"
#include "Util\PakFile.h"
#include "Grid.h"

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/IChunkFile.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <Cry3DEngine/CGF/CGFContent.h>

IMPLEMENT_DYNAMIC(CEdMesh, CEdGeometry)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Undo object for Editable Mesh.
class CUndoEdMesh : public IUndoObject
{
public:
	CUndoEdMesh(CEdMesh* pEdMesh, int nCopyFlags, const char* undoDescription)
	{
		// Stores the current state of this object.
		assert(pEdMesh != 0);
		m_nCopyFlags = nCopyFlags;
		m_undoDescription = undoDescription;
		m_pEdMesh = pEdMesh;
		pEdMesh->CopyToMesh(undoMesh, nCopyFlags);
	}
protected:

	virtual const char* GetDescription() { return m_undoDescription; };

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_pEdMesh->CopyToMesh(redoMesh, m_nCopyFlags);
		}
		// Undo object state.
		m_pEdMesh->CopyFromMesh(undoMesh, m_nCopyFlags, bUndo);
	}
	virtual void Redo()
	{
		m_pEdMesh->CopyFromMesh(redoMesh, m_nCopyFlags, true);
	}

private:
	string             m_undoDescription;
	int                 m_nCopyFlags;
	_smart_ptr<CEdMesh> m_pEdMesh;
	CTriMesh            undoMesh;
	CTriMesh            redoMesh;
};

//////////////////////////////////////////////////////////////////////////
// Static member of CEdMesh.
//////////////////////////////////////////////////////////////////////////
CEdMesh::MeshMap CEdMesh::m_meshMap;

//////////////////////////////////////////////////////////////////////////
CEdMesh::CEdMesh()
{
	m_pStatObj = 0;
	m_pSubObjCache = 0;
	m_nUserCount = 0;
}

//////////////////////////////////////////////////////////////////////////
CEdMesh::CEdMesh(IStatObj* pGeom)
{
	assert(pGeom);
	if (pGeom)
	{
		m_pStatObj = pGeom;
		m_pStatObj->AddRef();
	}
	m_pSubObjCache = 0;
	m_nUserCount = 0;
}

//////////////////////////////////////////////////////////////////////////
CEdMesh::~CEdMesh()
{
	for (auto ppIndexedMeshes = m_tempIndexedMeshes.begin(); ppIndexedMeshes != m_tempIndexedMeshes.end(); ++ppIndexedMeshes)
	{
		SAFE_RELEASE(*ppIndexedMeshes);
	}

	SAFE_RELEASE(m_pStatObj);
	// Remove this object from map.
	m_meshMap.erase(m_filename);
	if (m_pSubObjCache)
	{
		if (m_pSubObjCache->pTriMesh)
			delete m_pSubObjCache->pTriMesh;
		delete m_pSubObjCache;
	}
}

//////////////////////////////////////////////////////////////////////////
// CEdMesh implementation.
//////////////////////////////////////////////////////////////////////////
CEdMesh* CEdMesh::LoadMesh(const char* filename)
{
	if (strlen(filename) == 0)
		return 0;

	// If object created see if its not yet registered.
	CEdMesh* pMesh = stl::find_in_map(m_meshMap, filename, (CEdMesh*)0);
	if (pMesh)
	{
		// Found, return it.
		return pMesh;
	}

	// Make new.
	IStatObj* pGeom = GetIEditorImpl()->Get3DEngine()->LoadStatObj(filename);
	if (!pGeom)
		return 0;

	// Not found, Make new.
	pMesh = new CEdMesh(pGeom);
	pMesh->m_filename = filename;
	m_meshMap[filename] = pMesh;
	return pMesh;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::AddUser()
{
	m_nUserCount++;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::RemoveUser()
{
	m_nUserCount--;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::ReloadAllGeometries()
{
	for (MeshMap::iterator it = m_meshMap.begin(); it != m_meshMap.end(); ++it)
	{
		CEdMesh* pMesh = it->second;
		if (pMesh)
			pMesh->ReloadGeometry();
	}
}

void CEdMesh::ReleaseAll()
{
	m_meshMap.clear();
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::ReloadGeometry()
{
	// Reload mesh.
	if (m_pStatObj)
	{
		m_pStatObj->Refresh(FRO_GEOMETRY);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::IsSameObject(const char* filename)
{
	return stricmp(m_filename, filename) == 0;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::GetBounds(AABB& box)
{
	assert(m_pStatObj);

	if (m_pStatObj)
	{
		box.min = m_pStatObj->GetBoxMin();
		box.max = m_pStatObj->GetBoxMax();
	}
}

//////////////////////////////////////////////////////////////////////////
CEdGeometry* CEdMesh::Clone()
{
	if (m_pStatObj)
	{
		// Clone StatObj.
		IStatObj* pStatObj = m_pStatObj->Clone(true, true, false);
		pStatObj->AddRef();
		CEdMesh* pNewMesh = new CEdMesh(pStatObj);
		return pNewMesh;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::SetFilename(const string& filename)
{
	if (!m_filename.IsEmpty())
		m_meshMap.erase(m_filename);
	m_filename = PathUtil::MakeGamePath(filename.GetString());
	m_meshMap[m_filename] = this;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::Render(SRendParams& rp, const SRenderingPassInfo& passInfo)
{
	if (m_pStatObj)
		m_pStatObj->Render(rp, passInfo);
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::IsDefaultObject()
{
	if (m_pStatObj)
		return m_pStatObj->IsDefaultObject();
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IIndexedMesh* CEdMesh::GetIndexedMesh(size_t idx)
{
	if (m_tempIndexedMeshes.size() == 0 && m_pStatObj)
	{
		if (m_pStatObj->GetIndexedMesh())
		{
			if (idx == 0)
				return m_pStatObj->GetIndexedMesh();

			return nullptr;
		}
		else
		{
			// Load from CGF.
			string sFilename = m_pStatObj->GetFilePath();
			CContentCGF cgf(sFilename);
			if (gEnv->p3DEngine->LoadChunkFileContent(&cgf, sFilename))
			{
				for (int i = 0; i < cgf.GetNodeCount(); ++i)
				{
					CNodeCGF* pNode = cgf.GetNode(i);
					if (pNode->type == CNodeCGF::NODE_MESH)
					{
						CMesh* pMesh = pNode->pMesh;
						if (pMesh)
						{
							IIndexedMesh* pTempIndexedMesh = GetIEditorImpl()->Get3DEngine()->CreateIndexedMesh();
							pTempIndexedMesh->SetMesh(*pMesh);
							m_tempIndexedMeshes.push_back(pTempIndexedMesh);

							Matrix34 tm = pNode->localTM;
							CNodeCGF* pParent = pNode->pParent;
							while (pParent)
							{
								tm = pParent->localTM * tm;
								pParent = pParent->pParent;
							}
							m_tempMatrices.push_back(tm);
						}
					}
				}
			}
		}
	}

	if (idx < m_tempIndexedMeshes.size())
		return m_tempIndexedMeshes[idx];

	return nullptr;
}

void CEdMesh::GetTM(Matrix34* pTM, size_t idx)
{
	if (idx < m_tempMatrices.size())
		*pTM = m_tempMatrices[idx];
	else
		pTM->SetIdentity();
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::UpdateIndexedMeshFromCache(bool bFast)
{
	// Implement
	if (m_pSubObjCache)
	{
		if (bFast)
		{
			if (g_SubObjSelOptions.displayType == SO_DISPLAY_GEOMETRY)
			{
				m_pSubObjCache->pTriMesh->UpdateIndexedMesh(GetIndexedMesh());
				if (m_pStatObj)
					m_pStatObj->Invalidate();
			}
		}
		else
		{
			m_pSubObjCache->pTriMesh->UpdateIndexedMesh(GetIndexedMesh());
			if (m_pStatObj)
				m_pStatObj->Invalidate();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::StartSubObjSelection(const Matrix34& nodeWorldTM, int elemType, int nFlags)
{
	IIndexedMesh* pIndexedMesh = GetIndexedMesh();
	if (!pIndexedMesh)
		return false;
	CMesh& mesh = *pIndexedMesh->GetMesh();

	if (!m_pSubObjCache)
		m_pSubObjCache = new SubObjCache;
	m_pSubObjCache->worldTM = nodeWorldTM;
	m_pSubObjCache->invWorldTM = nodeWorldTM.GetInverted();

	if (!m_pSubObjCache->pTriMesh)
	{
		m_pSubObjCache->pTriMesh = new CTriMesh;
		m_pSubObjCache->pTriMesh->SetFromMesh(mesh);
	}
	UpdateSubObjCache();
	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;
	triMesh.selectionType = elemType;

	m_pSubObjCache->bNoDisplay = false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::UpdateSubObjCache()
{
	Matrix34& wtm = m_pSubObjCache->worldTM;

	SetWorldTM(wtm);
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::EndSubObjSelection()
{
	if (!m_pSubObjCache)
		return;

	UpdateIndexedMeshFromCache(false);

	if (m_pSubObjCache->pTriMesh)
		delete m_pSubObjCache->pTriMesh;
	delete m_pSubObjCache;
	m_pSubObjCache = 0;

	if (m_pStatObj)
	{
		// Clear hidden flag from geometry.
		m_pStatObj->SetFlags(m_pStatObj->GetFlags() & (~STATIC_OBJECT_HIDDEN));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::Display(DisplayContext& dc)
{
	if (!m_pSubObjCache || m_pSubObjCache->bNoDisplay)
		return;

	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;
	if (!triMesh.pWSVertices)
		return;

	if (m_pStatObj)
	{
		int nStatObjFlags = m_pStatObj->GetFlags();
		if (g_SubObjSelOptions.displayType == SO_DISPLAY_GEOMETRY)
			nStatObjFlags &= ~STATIC_OBJECT_HIDDEN;
		else
			nStatObjFlags |= STATIC_OBJECT_HIDDEN;
		m_pStatObj->SetFlags(nStatObjFlags);
	}

	const Matrix34& worldTM = m_pSubObjCache->worldTM;
	Vec3 vWSCameraVector = m_pSubObjCache->worldTM.GetTranslation() - dc.view->GetViewTM().GetTranslation();
	Vec3 vOSCameraVector = m_pSubObjCache->invWorldTM.TransformVector(vWSCameraVector).GetNormalized(); // Object space camera vector.

	// Render geometry vertices.
	uint32 nPrevState = dc.GetState();

	//////////////////////////////////////////////////////////////////////////
	// Calculate front facing vertices.
	//////////////////////////////////////////////////////////////////////////
	triMesh.frontFacingVerts.resize(triMesh.GetVertexCount());
	triMesh.frontFacingVerts.clear();
	for (int i = 0; i < triMesh.GetFacesCount(); i++)
	{
		CTriFace& face = triMesh.pFaces[i];
		if (vOSCameraVector.Dot(face.normal) < 0)
		{
			triMesh.frontFacingVerts[face.v[0]] = true;
			triMesh.frontFacingVerts[face.v[1]] = true;
			triMesh.frontFacingVerts[face.v[2]] = true;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Display flat shaded object.
	//////////////////////////////////////////////////////////////////////////
	if (g_SubObjSelOptions.displayType == SO_DISPLAY_FLAT)
	{
		ColorB faceColor(0, 250, 250, 255);
		ColorB col = faceColor;
		dc.SetDrawInFrontMode(false);
		dc.SetFillMode(e_FillModeSolid);
		dc.CullOn();
		for (int i = 0; i < triMesh.GetFacesCount(); i++)
		{
			CTriFace& face = triMesh.pFaces[i];
			if (triMesh.selectionType != SO_ELEM_FACE || !triMesh.faceSel[i])
			{
				ColorB col = faceColor;
				float dt = -face.normal.Dot(vOSCameraVector);
				dt = max(0.4f, dt);
				dt = min(1.0f, dt);
				col.r = int(faceColor.r * dt);
				col.g = int(faceColor.g * dt);
				col.b = int(faceColor.b * dt);
				col.a = faceColor.a;
				dc.pRenderAuxGeom->DrawTriangle(
				  triMesh.pWSVertices[face.v[0]], col,
				  triMesh.pWSVertices[face.v[1]], col,
				  triMesh.pWSVertices[face.v[2]], col
				  );
			}
		}
	}

	// Draw selected triangles.
	ColorB edgeColor(255, 255, 255, 155);
	if (triMesh.StreamHaveSelection(CTriMesh::FACES))
	{
		if (g_SubObjSelOptions.bDisplayBackfacing)
			dc.CullOff();
		else
			dc.CullOn();
		dc.SetDrawInFrontMode(true);
		dc.SetFillMode(e_FillModeWireframe);
		// Draw triangles.
		//dc.pRenderAuxGeom->DrawTriangles( triMesh.pVertices,triMesh.GetVertexCount(),	mesh.m_pIndices,mesh.GetIndexCount(),edgeColor );
		for (int i = 0; i < triMesh.GetFacesCount(); i++)
		{
			CTriFace& face = triMesh.pFaces[i];
			if (!triMesh.faceSel[i])
			{
				dc.pRenderAuxGeom->DrawTriangle(
				  triMesh.pWSVertices[face.v[0]], edgeColor,
				  triMesh.pWSVertices[face.v[1]], edgeColor,
				  triMesh.pWSVertices[face.v[2]], edgeColor
				  );
			}
		}
	}

	if (g_SubObjSelOptions.bDisplayNormals)
	{
		for (int i = 0; i < triMesh.GetFacesCount(); i++)
		{
			CTriFace& face = triMesh.pFaces[i];
			Vec3 p1 = triMesh.pWSVertices[face.v[0]];
			Vec3 p2 = triMesh.pWSVertices[face.v[1]];
			Vec3 p3 = triMesh.pWSVertices[face.v[2]];
			Vec3 midp = (p1 + p2 + p3) * (1.0f / 3.0f);
			dc.pRenderAuxGeom->DrawLine(midp, edgeColor, midp + worldTM.TransformVector(face.normal) * g_SubObjSelOptions.fNormalsLength, edgeColor);
		}
	}

	if (triMesh.selectionType == SO_ELEM_VERTEX || triMesh.StreamHaveSelection(CTriMesh::VERTICES))
	{
		ColorB pointColor(0, 255, 255, 255);

		float fClrAdd = (g_SubObjSelOptions.bSoftSelection) ? 0 : 1;
		for (int i = 0; i < triMesh.GetVertexCount(); i++)
		{
			bool bSelected = triMesh.vertSel[i] || triMesh.pWeights[i] != 0;
			if (bSelected)
			{
				int clr = (triMesh.pWeights[i] + fClrAdd) * 255;
				dc.pRenderAuxGeom->DrawPoint(triMesh.pWSVertices[i], ColorB(clr, 255 - clr, 255 - clr, 255), 8);
			}
			else if (!g_SubObjSelOptions.bDisplayBackfacing || triMesh.frontFacingVerts[i])
			{
				dc.pRenderAuxGeom->DrawPoint(triMesh.pWSVertices[i], pointColor, 5);
			}
		}
	}

	// Draw edges.
	if (triMesh.selectionType == SO_ELEM_EDGE || triMesh.StreamHaveSelection(CTriMesh::EDGES))
	{
		ColorB edgeColor(200, 255, 200, 255);
		ColorB selEdgeColor(255, 0, 0, 255);

		// Draw selected edges.
		for (int i = 0; i < triMesh.GetEdgeCount(); i++)
		{
			CTriEdge& edge = triMesh.pEdges[i];
			if (triMesh.edgeSel[i])
			{
				const Vec3& p1 = triMesh.pWSVertices[edge.v[0]];
				const Vec3& p2 = triMesh.pWSVertices[edge.v[1]];
				dc.pRenderAuxGeom->DrawLine(p1, selEdgeColor, p2, selEdgeColor, 6);
			}
			else if (!g_SubObjSelOptions.bDisplayBackfacing || (
			           triMesh.frontFacingVerts[edge.v[0]] && triMesh.frontFacingVerts[edge.v[1]]))
			{
				const Vec3& p1 = triMesh.pWSVertices[edge.v[0]];
				const Vec3& p2 = triMesh.pWSVertices[edge.v[1]];
				dc.pRenderAuxGeom->DrawLine(p1, edgeColor, p2, edgeColor);
			}
		}
	}

	if (triMesh.selectionType == SO_ELEM_FACE)
	{
		ColorB pointColor(0, 255, 255, 255);
		ColorB selFaceColor(255, 0, 0, 180);

		// Draw selected faces and face points.
		dc.CullOff();
		dc.SetFillMode(e_FillModeSolid);

		for (int i = 0; i < triMesh.GetFacesCount(); i++)
		{
			CTriFace& face = triMesh.pFaces[i];
			const Vec3& p1 = triMesh.pWSVertices[face.v[0]];
			const Vec3& p2 = triMesh.pWSVertices[face.v[1]];
			const Vec3& p3 = triMesh.pWSVertices[face.v[2]];
			if (triMesh.faceSel[i])
			{
				dc.pRenderAuxGeom->DrawTriangle(p1, selFaceColor, p2, selFaceColor, p3, selFaceColor);
			}

			if (!g_SubObjSelOptions.bDisplayBackfacing && vOSCameraVector.Dot(face.normal) > 0)
				continue;   // Backfacing.

			Vec3 midp = (p1 + p2 + p3) * (1.0f / 3.0f);
			dc.pRenderAuxGeom->DrawPoint(midp, pointColor, 4);
		}
	}
	else if (triMesh.StreamHaveSelection(CTriMesh::FACES))
	{
		ColorB pointColor(0, 255, 255, 255);
		ColorB selFaceColor(255, 0, 0, 180);

		// Draw selected faces and face points.
		dc.CullOff();
		dc.SetFillMode(e_FillModeSolid);

		for (int i = 0; i < triMesh.GetFacesCount(); i++)
		{
			CTriFace& face = triMesh.pFaces[i];
			const Vec3& p1 = triMesh.pWSVertices[face.v[0]];
			const Vec3& p2 = triMesh.pWSVertices[face.v[1]];
			const Vec3& p3 = triMesh.pWSVertices[face.v[2]];
			if (triMesh.faceSel[i])
			{
				dc.pRenderAuxGeom->DrawTriangle(p1, selFaceColor, p2, selFaceColor, p3, selFaceColor);
			}
		}
	}

	dc.SetState(nPrevState); // Restore render state.
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::HitTestVertex(HitContext& hit, SSubObjHitTestEnvironment& env, SSubObjHitTestResult& result)
{
	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;

	// This make sure that bit array size matches num vertices, front facing should be calculated in Display method.
	triMesh.frontFacingVerts.resize(triMesh.GetVertexCount());

	float minDist = FLT_MAX;
	int closestElem = -1;

	for (int i = 0; i < triMesh.GetVertexCount(); i++)
	{
		if (env.bIgnoreBackfacing && !triMesh.frontFacingVerts[i])
			continue;
		CPoint p = hit.view->WorldToView(triMesh.pWSVertices[i]);
		if (p.x >= hit.rect.left && p.x <= hit.rect.right &&
		    p.y >= hit.rect.top && p.y <= hit.rect.bottom)
		{
			if (env.bHitTestNearest)
			{
				float dist = env.vWSCameraPos.GetDistance(triMesh.pWSVertices[i]);
				if (dist < minDist)
				{
					closestElem = i;
					minDist = dist;
				}
			}
			else
			{
				result.elems.push_back(i);
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
	if (closestElem >= 0)
	{
		result.minDistance = minDist;
		result.elems.push_back(closestElem);
	}
	return !result.elems.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::HitTestEdge(HitContext& hit, SSubObjHitTestEnvironment& env, SSubObjHitTestResult& result)
{
	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;
	// This make sure that bit array size matches num vertices, front facing should be calculated in Display method.
	triMesh.frontFacingVerts.resize(triMesh.GetVertexCount());

	float minDist = FLT_MAX;
	int closestElem = -1;

	for (int i = 0; i < triMesh.GetEdgeCount(); i++)
	{
		CTriEdge& edge = triMesh.pEdges[i];
		if (!env.bIgnoreBackfacing ||
		    (triMesh.frontFacingVerts[edge.v[0]] && triMesh.frontFacingVerts[edge.v[1]]))
		{
			if (hit.view->HitTestLine(triMesh.pWSVertices[edge.v[0]], triMesh.pWSVertices[edge.v[1]], hit.point2d, 5))
			{
				if (env.bHitTestNearest)
				{
					float dist = env.vWSCameraPos.GetDistance(triMesh.pWSVertices[edge.v[0]]);
					if (dist < minDist)
					{
						closestElem = i;
						minDist = dist;
					}
				}
				else
				{
					result.elems.push_back(i);
				}
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
	if (closestElem >= 0)
	{
		result.minDistance = minDist;
		result.elems.push_back(closestElem);
	}
	return !result.elems.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::HitTestFace(HitContext& hit, SSubObjHitTestEnvironment& env, SSubObjHitTestResult& result)
{
	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;
	float minDist = FLT_MAX;
	int closestElem = -1;

	Vec3 vOut(0, 0, 0);
	Ray hitRay(hit.raySrc, hit.rayDir);

	for (int i = 0; i < triMesh.GetFacesCount(); i++)
	{
		CTriFace& face = triMesh.pFaces[i];

		if (env.bIgnoreBackfacing && env.vOSCameraVector.Dot(face.normal) > 0)
			continue; // Back facing.

		Vec3 p1 = triMesh.pWSVertices[face.v[0]];
		Vec3 p2 = triMesh.pWSVertices[face.v[1]];
		Vec3 p3 = triMesh.pWSVertices[face.v[2]];

		if (!env.bHitTestNearest)
		{
			// Hit test face middle point in rectangle.
			Vec3 midp = (p1 + p2 + p3) * (1.0f / 3.0f);

			CPoint p = hit.view->WorldToView(midp);
			if (p.x >= hit.rect.left && p.x <= hit.rect.right &&
			    p.y >= hit.rect.top && p.y <= hit.rect.bottom)
			{
				result.elems.push_back(i);
			}
		}
		else
		{
			// Hit test ray/triangle.
			if (Intersect::Ray_Triangle(hitRay, p1, p3, p2, vOut))
			{
				float dist = hitRay.origin.GetSquaredDistance(vOut);
				if (dist < minDist)
				{
					closestElem = i;
					minDist = dist;
				}
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
	if (closestElem >= 0)
	{
		result.minDistance = (float)sqrt(minDist);
		result.elems.push_back(closestElem);
	}
	return !result.elems.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::SelectSubObjElements(SSubObjHitTestEnvironment& env, SSubObjHitTestResult& result)
{
	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;

	bool bSelChanged = false;
	if (env.bSelectOnHit && !result.elems.empty())
	{
		CBitArray* streamSel = triMesh.GetStreamSelection(result.stream);
		if (streamSel)
		{
			// Select on hit.
			for (int i = 0, num = result.elems.size(); i < num; i++)
			{
				int elem = result.elems[i];
				if ((*streamSel)[elem] != env.bSelectValue)
				{
					bSelChanged = true;
					(*streamSel)[elem] = env.bSelectValue;
				}
			}
			if (bSelChanged)
			{
				if (env.bSelectValue)
					triMesh.streamSelMask |= (1 << result.stream);
				else if (!env.bSelectValue && streamSel->is_zero())
					triMesh.streamSelMask &= ~(1 << result.stream);
			}
		}
	}
	return bSelChanged;
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::IsHitTestResultSelected(SSubObjHitTestResult& result)
{
	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;

	if (!result.elems.empty())
	{
		CBitArray* streamSel = triMesh.GetStreamSelection(result.stream);
		if (streamSel)
		{
			// check if first result element is selected.
			if ((*streamSel)[result.elems[0]])
				return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::HitTest(HitContext& hit)
{
	if (hit.nSubObjFlags & SO_HIT_NO_EDIT)
	// This is for a 'move-by-face-normal'. Prepare the mesh and set the 'bNoDisplay'to true
	// so that the normal rendering happens instead of the edit-mode rendering.
	{
		StartSubObjSelection(hit.object->GetWorldTM(), SO_ELEM_FACE, 0);
		m_pSubObjCache->bNoDisplay = true;
	}

	if (!m_pSubObjCache)
		return false;

	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;

	SSubObjHitTestEnvironment env;
	env.vWSCameraPos = hit.view->GetViewTM().GetTranslation();
	env.vWSCameraVector = m_pSubObjCache->worldTM.GetTranslation() - hit.view->GetViewTM().GetTranslation();
	env.vOSCameraVector = m_pSubObjCache->invWorldTM.TransformVector(env.vWSCameraVector).GetNormalized(); // Object space camera vector.

	env.bHitTestNearest = hit.nSubObjFlags & SO_HIT_POINT;
	env.bHitTestSelected = hit.nSubObjFlags & SO_HIT_TEST_SELECTED;
	env.bSelectOnHit = hit.nSubObjFlags & SO_HIT_SELECT;
	env.bAdd = hit.nSubObjFlags & SO_HIT_SELECT_ADD;
	env.bRemove = hit.nSubObjFlags & SO_HIT_SELECT_REMOVE;
	env.bSelectValue = !env.bRemove;
	env.bHighlightOnly = hit.nSubObjFlags & SO_HIT_HIGHLIGHT_ONLY;
	env.bIgnoreBackfacing = g_SubObjSelOptions.bIgnoreBackfacing && !env.bHitTestNearest;

	int nHitTestWhat = (hit.nSubObjFlags & SO_HIT_ELEM_ALL);
	if (nHitTestWhat == 0)
	{
		if (g_SubObjSelOptions.bSelectByVertex)
		{
			nHitTestWhat |= SO_HIT_ELEM_VERTEX;
		}
		switch (triMesh.selectionType)
		{
		case SO_ELEM_VERTEX:
			nHitTestWhat |= SO_HIT_ELEM_VERTEX;
			break;
		case SO_ELEM_EDGE:
			nHitTestWhat |= SO_HIT_ELEM_EDGE;
			break;
		case SO_ELEM_FACE:
			nHitTestWhat |= SO_HIT_ELEM_FACE;
			break;
		case SO_ELEM_POLYGON:
			nHitTestWhat |= SO_HIT_ELEM_POLYGON;
			break;
		}
	}

	IUndoObject* pUndoObj = NULL;
	if (env.bSelectOnHit)
	{
		if (CUndo::IsRecording() && !(hit.nSubObjFlags & SO_HIT_NO_EDIT))
		{
			switch (triMesh.selectionType)
			{
			case SO_ELEM_VERTEX:
				pUndoObj = new CUndoEdMesh(this, CTriMesh::COPY_VERT_SEL | CTriMesh::COPY_WEIGHTS, "Select Vertex(s)");
				break;
			case SO_ELEM_EDGE:
				pUndoObj = new CUndoEdMesh(this, CTriMesh::COPY_EDGE_SEL | CTriMesh::COPY_WEIGHTS, "Select Edge(s)");
				break;
			case SO_ELEM_FACE:
				pUndoObj = new CUndoEdMesh(this, CTriMesh::COPY_FACE_SEL | CTriMesh::COPY_WEIGHTS, "Select Face(s)");
				break;
			}
		}
	}

	bool bSelChanged = false;
	bool bAnyHit = false;
	//////////////////////////////////////////////////////////////////////////
	if (env.bSelectOnHit && !env.bAdd && !env.bRemove)
	{
		bSelChanged = triMesh.ClearSelection();
	}
	//////////////////////////////////////////////////////////////////////////

	SSubObjHitTestResult result[4];
	result[0].stream = CTriMesh::VERTICES;
	result[1].stream = CTriMesh::EDGES;
	result[2].stream = CTriMesh::FACES;

	if (nHitTestWhat & SO_HIT_ELEM_VERTEX)
	{
		if (HitTestVertex(hit, env, result[0]))
			bAnyHit = true;
	}

	if (nHitTestWhat & SO_HIT_ELEM_EDGE)
	{
		if (HitTestEdge(hit, env, result[1]))
			bAnyHit = true;
	}

	if (nHitTestWhat & SO_HIT_ELEM_FACE)
	{
		if (HitTestFace(hit, env, result[2]))
			bAnyHit = true;
	}

	if (bAnyHit && !env.bSelectOnHit && !env.bHitTestSelected)
	{
		// Return distance to the first hit element.
		hit.dist = min(min(result[0].minDistance, result[1].minDistance), result[2].minDistance);
		return true;
	}
	if (bAnyHit && !env.bSelectOnHit && env.bHitTestSelected)
	{
		// check if we hit selected item.
		if (IsHitTestResultSelected(result[0]) ||
		    IsHitTestResultSelected(result[1]) ||
		    IsHitTestResultSelected(result[2]))
		{
			hit.dist = min(min(result[0].minDistance, result[1].minDistance), result[2].minDistance);
			return true;
		}
		// If not hit selected.
		return false;
	}
	if (bAnyHit)
	{
		// Find closest hit.
		int n = 0;
		if (!result[0].elems.empty())
			n = 0;
		else if (!result[1].elems.empty())
			n = 1;
		else if (!result[2].elems.empty())
			n = 2;

		hit.dist = result[n].minDistance;

		if (env.bSelectOnHit &&
		    g_SubObjSelOptions.bSelectByVertex &&
		    !result[0].elems.empty() &&
		    !env.bHighlightOnly &&
		    triMesh.selectionType != SO_HIT_ELEM_VERTEX)
		{
			// When selecting elements by vertex.
			switch (triMesh.selectionType)
			{
			case SO_ELEM_EDGE:
				n = 1;
				triMesh.GetEdgesByVertex(result[0].elems, result[1].elems);
				break;
			case SO_ELEM_FACE:
				n = 2;
				triMesh.GetFacesByVertex(result[0].elems, result[2].elems);
				break;
			case SO_ELEM_POLYGON:
				n = 2;
				triMesh.GetFacesByVertex(result[0].elems, result[2].elems);
				break;
			}
		}
		if (env.bSelectOnHit && SelectSubObjElements(env, result[n]))
			bSelChanged = true;
	}
	if (bSelChanged)
		hit.nSubObjFlags |= SO_HIT_SELECTION_CHANGED;
	else
		hit.nSubObjFlags &= ~SO_HIT_SELECTION_CHANGED;

	bool bSelectionNotEmpty = false;
	if (env.bSelectOnHit && bSelChanged && !env.bHighlightOnly)
	{
		if (CUndo::IsRecording() && pUndoObj)
			CUndo::Record(pUndoObj);

		bSelectionNotEmpty = triMesh.UpdateSelection();
		if (g_SubObjSelOptions.bSoftSelection)
			triMesh.SoftSelection(g_SubObjSelOptions);
	}
	else
	{
		if (pUndoObj)
			pUndoObj->Release();
	}

	return bSelectionNotEmpty;
}

//////////////////////////////////////////////////////////////////////////
bool CEdMesh::GetSelectionReferenceFrame(Matrix34& refFrame)
{
	if (!m_pSubObjCache)
		return false;

	bool bAnySelected = false;

	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;

	Vec3 normal(0, 0, 0);

	refFrame.SetIdentity();

	if (triMesh.selectionType == SO_ELEM_VERTEX)
	{
		// Average all selected vertex normals.
		int numNormals = 0;
		int nFaces = triMesh.GetFacesCount();
		for (int i = 0; i < triMesh.GetVertexCount(); i++)
		{
			if (triMesh.vertSel[i])
			{
				bAnySelected = true;
				int nVertexIndex = i;
				for (int j = 0; j < nFaces; j++)
				{
					CTriFace& face = triMesh.pFaces[j];
					for (int k = 0; k < 3; k++)
					{
						if (face.v[k] == nVertexIndex)
						{
							normal += face.n[k];
							numNormals++;
						}
					}
				}
			}
		}
		if (numNormals > 0)
		{
			normal = normal / numNormals;
			if (!normal.IsZero())
				normal.Normalize();
		}
	}
	else if (triMesh.selectionType == SO_ELEM_EDGE)
	{
		int nNormals = 0;
		// Average face normals of a the selected edges.
		for (int i = 0; i < triMesh.GetEdgeCount(); i++)
		{
			if (triMesh.edgeSel[i])
			{
				bAnySelected = true;
				CTriEdge& edge = triMesh.pEdges[i];
				for (int j = 0; j < 2; j++)
					if (edge.face[j] >= 0)
					{
						normal = normal + triMesh.pFaces[edge.face[j]].normal;
						nNormals++;
					}
			}
		}
		if (nNormals > 0)
		{
			normal = normal / nNormals;
			if (!normal.IsZero())
				normal.Normalize();
		}
	}
	else if (triMesh.selectionType == SO_ELEM_FACE)
	{
		// Average all face normals.
		int nNormals = 0;
		for (int i = 0; i < triMesh.GetFacesCount(); i++)
		{
			if (triMesh.faceSel[i])
			{
				bAnySelected = true;
				CTriFace& face = triMesh.pFaces[i];
				normal = normal + face.normal;
				nNormals++;
			}
		}
		if (nNormals > 0)
		{
			normal = normal / nNormals;
			if (!normal.IsZero())
				normal.Normalize();
		}
	}

	if (bAnySelected)
	{
		Vec3 pos(0, 0, 0);
		int numSel = 0;
		for (int i = 0; i < triMesh.GetVertexCount(); i++)
		{
			if (triMesh.pWeights[i] == 1.0f)
			{
				pos = pos + triMesh.pVertices[i].pos;
				numSel++;
			}
		}
		if (numSel > 0)
			pos = pos / numSel; // Average position.
		refFrame.SetTranslation(pos);

		if (!normal.IsZero())
		{
			Vec3 xAxis(1, 0, 0), yAxis(0, 1, 0), zAxis(0, 0, 1);
			if (normal.IsEquivalent(zAxis) || normal.IsEquivalent(-zAxis))
				zAxis = xAxis;
			xAxis = normal.Cross(zAxis).GetNormalized();
			yAxis = xAxis.Cross(normal).GetNormalized();
			refFrame.SetFromVectors(xAxis, yAxis, normal, pos);
		}
	}

	return bAnySelected;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::CopyToMesh(CTriMesh& toMesh, int nCopyFlags)
{
	if (!m_pSubObjCache)
		return;

	toMesh.Copy(*m_pSubObjCache->pTriMesh, nCopyFlags);
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::CopyFromMesh(CTriMesh& fromMesh, int nCopyFlags, bool bUndo)
{
	if (m_pSubObjCache)
	{
		m_pSubObjCache->pTriMesh->Copy(fromMesh, nCopyFlags);
	}

	if (bUndo)
	{
		UpdateIndexedMeshFromCache(true);
	}

	if (m_pSubObjCache)
		UpdateSubObjCache();
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::SaveToCGF(const char* sFilename, CPakFile* pPakFile, IMaterial* pMaterial)
{
	if (m_pStatObj)
	{
		// Save this EdMesh to CGF file.
		m_filename = PathUtil::MakeGamePath(sFilename);

		_smart_ptr<IMaterial> pOriginalMaterial = m_pStatObj->GetMaterial();
		if (pMaterial)
			m_pStatObj->SetMaterial(pMaterial);

		if (!pPakFile)
			m_pStatObj->SaveToCGF(sFilename);
		else
		{
			IChunkFile* pChunkFile = NULL;
			if (m_pStatObj->SaveToCGF(sFilename, &pChunkFile))
			{
				void* pMemFile = NULL;
				int nFileSize = 0;

				pChunkFile->WriteToMemoryBuffer(&pMemFile, &nFileSize);
				pPakFile->UpdateFile(sFilename, pMemFile, nFileSize, true, ICryArchive::LEVEL_FASTER);
				pChunkFile->Release();
			}
		}

		// Restore original material.
		if (pMaterial)
			m_pStatObj->SetMaterial(pOriginalMaterial);
	}
}

//////////////////////////////////////////////////////////////////////////
CTriMesh* CEdMesh::GetMesh()
{
	if (m_pSubObjCache)
		return m_pSubObjCache->pTriMesh;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CEdMesh* CEdMesh::CreateMesh(const char* name)
{
	IStatObj* pStatObj = gEnv->p3DEngine->CreateStatObj();
	if (pStatObj)
	{
		CEdMesh* pEdMesh = new CEdMesh;
		pEdMesh->m_pStatObj = pStatObj;
		pEdMesh->m_pStatObj->AddRef();

		// Force create of indexed mesh.
		pEdMesh->m_pStatObj->GetIndexedMesh(true);

		// Not found, Make new.
		pEdMesh->m_filename = name;

		if (!pEdMesh->m_pSubObjCache)
		{
			pEdMesh->m_pSubObjCache = new SubObjCache;
			pEdMesh->m_pSubObjCache->pTriMesh = new CTriMesh;
			pEdMesh->m_pSubObjCache->worldTM.SetIdentity();
			pEdMesh->m_pSubObjCache->invWorldTM.SetIdentity();
		}
		return pEdMesh;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::InvalidateMesh()
{
	if (m_pSubObjCache)
		UpdateIndexedMeshFromCache(false);
	if (m_pStatObj)
		m_pStatObj->Invalidate(true);
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::SetWorldTM(const Matrix34& worldTM)
{
	if (!m_pSubObjCache)
		return;

	m_pSubObjCache->worldTM = worldTM;
	m_pSubObjCache->invWorldTM = worldTM.GetInverted();

	//////////////////////////////////////////////////////////////////////////
	// Transform vertices and normals to world space and store in cached mesh.
	//////////////////////////////////////////////////////////////////////////
	CTriMesh& triMesh = *m_pSubObjCache->pTriMesh;
	int nVerts = triMesh.GetVertexCount();
	triMesh.ReallocStream(CTriMesh::WS_POSITIONS, nVerts);
	for (int i = 0; i < nVerts; i++)
	{
		triMesh.pWSVertices[i] = worldTM.TransformPoint(triMesh.pVertices[i].pos);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEdMesh::DebugDraw(const SGeometryDebugDrawInfo& info)
{
	if (m_pStatObj)
	{
		m_pStatObj->DebugDraw(info);
	}
}

