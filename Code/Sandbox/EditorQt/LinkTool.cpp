// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "LinkTool.h"
#include "Viewport.h"
#include "Objects/EntityObject.h"
#include "Objects/MiscEntities.h"
#include <Objects/IObjectLayer.h>
#include "Controls/QuestionDialog.h"

#include "QtUtil.h"
#include <QMenu>

class CLinkToPicker : public IPickObjectCallback
{
public:
	//! Called when object picked.
	virtual void OnPick(CBaseObject* picked)
	{
		IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();

		// Perform link
		pObjectManager->LinkSelectionTo(picked);

		delete this;
	}
	//! Called when pick mode cancelled.
	virtual void OnCancelPick()
	{
		delete this;
	}
	//! Return true if specified object is pickable.
	virtual bool OnPickFilter(CBaseObject* pLinkTo)
	{
		const CSelectionGroup* pSelectionGroup = GetIEditorImpl()->GetSelection();
		if (!pSelectionGroup)
			return false;

		pSelectionGroup->FilterParents();

		for (int i = 0; i < pSelectionGroup->GetFilteredCount(); i++)
		{
			if (!pSelectionGroup->GetFilteredObject(i)->CanLinkTo(pLinkTo))
				return false;
		}

		return true;
	}
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CLinkTool, CEditTool)

namespace
{
const float kGeomCacheNodePivotSizeScale = 0.0025f;
}

//////////////////////////////////////////////////////////////////////////
CLinkTool::CLinkTool()
	: m_pChild(nullptr)
	, m_nodeName(nullptr)
	, m_pGeomCacheRenderNode(nullptr)
	, m_hLinkCursor(AfxGetApp()->LoadCursor(IDC_POINTER_LINK))
	, m_hLinkNowCursor(AfxGetApp()->LoadCursor(IDC_POINTER_LINKNOW))
	, m_hCurrCursor(m_hLinkCursor)
{
}

//////////////////////////////////////////////////////////////////////////
CLinkTool::~CLinkTool()
{
}

//////////////////////////////////////////////////////////////////////////
bool CLinkTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	view->SetCursorString("");

	m_hCurrCursor = m_hLinkCursor;
	if (event == eMouseLDown)
	{
		HitContext hitInfo;
		view->HitTest(point, hitInfo);
		m_pChild = hitInfo.object;

		if (m_pChild)
			m_StartDrag = m_pChild->GetWorldPos();
	}
	else if (event == eMouseLUp)
	{
		HitContext hitInfo;
		view->HitTest(point, hitInfo);
		CBaseObject* pLinkTo = hitInfo.object;
		GetIEditorImpl()->GetObjectManager()->Link(m_pChild, pLinkTo, hitInfo.name);
		
		m_pChild = nullptr;
	}
	else if (event == eMouseMove)
	{
		// If there's no valid child then don't proceed with looking for what we're linking to
		if (!m_pChild)
			return false;

		m_EndDrag = view->ViewToWorld(point);
		m_nodeName = nullptr;
		m_pGeomCacheRenderNode = nullptr;

		HitContext hitInfo;
		if (view->HitTest(point, hitInfo))
			m_EndDrag = hitInfo.raySrc + hitInfo.rayDir * hitInfo.dist;

		CBaseObject* pLinkTo = hitInfo.object;

		// If not a valid pick than don't proceed
		if (!m_pChild->CanLinkTo(pLinkTo))
			return false;

		string name = pLinkTo->GetName();

		if (hitInfo.name)
			name += string("\n  ") + hitInfo.name;

		// Set Cursors.
		view->SetCursorString(name);
		m_hCurrCursor = m_hLinkNowCursor;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLinkTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		// Cancel selection.
		GetIEditorImpl()->SetEditTool(0);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CLinkTool::Display(DisplayContext& dc)
{
	if (m_pChild && m_EndDrag != Vec3(ZERO))
	{
		ColorF lineColor = (m_hCurrCursor == m_hLinkNowCursor) ? ColorF(0, 1, 0) : ColorF(1, 0, 0);
		dc.DrawLine(m_StartDrag, m_EndDrag, lineColor, lineColor);
	}
}

//////////////////////////////////////////////////////////////////////////
void CLinkTool::DrawObjectHelpers(CBaseObject* pObject, DisplayContext& dc)
{
	if (!m_pChild)
	{
		return;
	}

#if defined(USE_GEOM_CACHES)
	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntityObject = (CEntityObject*)pObject;

		IEntity* pEntity = pEntityObject->GetIEntity();
		if (pEntity)
		{
			IGeomCacheRenderNode* pGeomCacheRenderNode = pEntity->GetGeomCacheRenderNode(0);
			if (pGeomCacheRenderNode)
			{
				if (QtUtil::IsModifierKeyDown(Qt::ShiftModifier))
				{
					const Matrix34& worldTM = pEntity->GetWorldTM();

					dc.DepthTestOff();

					const uint nodeCount = pGeomCacheRenderNode->GetNodeCount();
					for (uint i = 0; i < nodeCount; ++i)
					{
						const char* pNodeName = pGeomCacheRenderNode->GetNodeName(i);
						Vec3 nodePosition = worldTM.TransformPoint(pGeomCacheRenderNode->GetNodeTransform(i).GetTranslation());

						AABB entityWorldBounds;
						pEntity->GetWorldBounds(entityWorldBounds);
						Vec3 aabbSize = entityWorldBounds.GetSize();
						float aabbMinSide = std::min(std::min(aabbSize.x, aabbSize.y), aabbSize.z);

						Vec3 boxSizeHalf(kGeomCacheNodePivotSizeScale * aabbMinSide);

						ColorB color = ColorB(0, 0, 255, 255);
						if (pNodeName == m_nodeName && pGeomCacheRenderNode == m_pGeomCacheRenderNode)
						{
							color = ColorB(0, 255, 0, 255);
						}

						dc.SetColor(color);
						dc.DrawWireBox(nodePosition - boxSizeHalf, nodePosition + boxSizeHalf);
					}

					dc.DepthTestOn();
				}

				if (pGeomCacheRenderNode == m_pGeomCacheRenderNode)
				{
					SGeometryDebugDrawInfo dd;
					dd.tm = pEntity->GetSlotWorldTM(0);
					dd.bDrawInFront = true;
					dd.color = ColorB(250, 0, 250, 30);
					dd.lineColor = ColorB(255, 255, 0, 160);

					pGeomCacheRenderNode->DebugDraw(dd, m_hitNodeIndex);
				}
			}
		}
	}
#endif
}

void CLinkTool::PickObject()
{
	CLinkToPicker* pCallback = new CLinkToPicker;
	GetIEditorImpl()->PickObject(pCallback);
}

//////////////////////////////////////////////////////////////////////////
bool CLinkTool::HitTest(CBaseObject* pObject, HitContext& hc)
{
	if (!m_pChild)
	{
		return false;
	}

	bool bHit = false;

	m_hitNodeIndex = 0;

#if defined(USE_GEOM_CACHES)
	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntityObject = (CEntityObject*)pObject;

		IEntity* pEntity = pEntityObject->GetIEntity();
		if (pEntity)
		{
			IGeomCacheRenderNode* pGeomCacheRenderNode = pEntity->GetGeomCacheRenderNode(0);
			if (pGeomCacheRenderNode)
			{
				float nearestDist = std::numeric_limits<float>::max();

				if (QtUtil::IsModifierKeyDown(Qt::ShiftModifier))
				{
					Ray ray(hc.raySrc, hc.rayDir);
					const Matrix34& worldTM = pEntity->GetWorldTM();

					const uint nodeCount = pGeomCacheRenderNode->GetNodeCount();
					for (uint i = 0; i < nodeCount; ++i)
					{
						Vec3 nodePosition = worldTM.TransformPoint(pGeomCacheRenderNode->GetNodeTransform(i).GetTranslation());

						AABB entityWorldBounds;
						pEntity->GetWorldBounds(entityWorldBounds);
						Vec3 aabbSize = entityWorldBounds.GetSize();
						float aabbMinSide = std::min(std::min(aabbSize.x, aabbSize.y), aabbSize.z);

						Vec3 boxSizeHalf(kGeomCacheNodePivotSizeScale * aabbMinSide);

						AABB nodeAABB(nodePosition - boxSizeHalf, nodePosition + boxSizeHalf);

						Vec3 hitPoint;
						if (Intersect::Ray_AABB(ray, nodeAABB, hitPoint))
						{
							const float hitDist = hitPoint.GetDistance(hc.raySrc);

							if (hitDist < nearestDist)
							{
								nearestDist = hitDist;
								bHit = true;

								m_hitNodeIndex = i;
								hc.object = pObject;
								hc.dist = 0.0f;
								hc.name = pGeomCacheRenderNode->GetNodeName(i);
								m_nodeName = hc.name;
								m_pGeomCacheRenderNode = pGeomCacheRenderNode;
							}
						}
					}
				}
				else
				{
					SRayHitInfo hitInfo;
					ZeroStruct(hitInfo);
					hitInfo.inReferencePoint = hc.raySrc;
					hitInfo.inRay = Ray(hitInfo.inReferencePoint, hc.rayDir.GetNormalized());
					hitInfo.bInFirstHit = false;
					hitInfo.bUseCache = false;

					if (pGeomCacheRenderNode->RayIntersection(hitInfo, NULL, &m_hitNodeIndex))
					{
						nearestDist = hitInfo.fDistance;
						bHit = true;

						hc.object = pObject;
						hc.dist = 0.0f;
						hc.name = pGeomCacheRenderNode->GetNodeName(m_hitNodeIndex);
						m_nodeName = hc.name;
						m_pGeomCacheRenderNode = pGeomCacheRenderNode;
					}
				}
			}
		}
	}
#endif

	return bHit;
}

