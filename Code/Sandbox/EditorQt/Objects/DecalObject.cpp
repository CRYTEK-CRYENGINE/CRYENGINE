// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "EntityObject.h"
#include "Geometry\EdMesh.h"
#include "Material\MaterialManager.h"

#include <Serialization/Decorators/EditToolButton.h>
#include <Serialization/Decorators/EditorActionButton.h>

#include <Cry3DEngine/I3DEngine.h>

#include <Preferences/ViewportPreferences.h>

#include "DecalObject.h"

class CDecalObjectTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CDecalObjectTool)

	CDecalObjectTool();

	virtual string GetDisplayName() const override { return "Decal Object"; }
	virtual void   Display(DisplayContext& dc)     {};
	virtual bool   MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	virtual bool   OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	virtual void   SetUserData(const char* userKey, void* userData);

protected:
	virtual ~CDecalObjectTool();
	void DeleteThis() { delete this; };

private:
	CDecalObject* m_pDecalObj;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CDecalObjectTool, CEditTool)

CDecalObjectTool::CDecalObjectTool()
{
}

//////////////////////////////////////////////////////////////////////////
CDecalObjectTool::~CDecalObjectTool()
{
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObjectTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObjectTool::OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObjectTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (m_pDecalObj)
		m_pDecalObj->MouseCallbackImpl(view, event, point, flags);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CDecalObjectTool::SetUserData(const char* userKey, void* userData)
{
	m_pDecalObj = static_cast<CDecalObject*>(userData);
}

//////////////////////////////////////////////////////////////////////////
// CDecalObject implementation.
//////////////////////////////////////////////////////////////////////////

REGISTER_CLASS_DESC(CDecalObjectClassDesc);

IMPLEMENT_DYNCREATE(CDecalObject, CBaseObject)

CDecalObject::CDecalObject()
{
	m_renderFlags = 0;
	m_projectionType = 0;
	m_deferredDecal = false;

	m_projectionType.AddEnumItem("Planar", SDecalProperties::ePlanar);
	m_projectionType.AddEnumItem("ProjectOnStaticObjects", SDecalProperties::eProjectOnStaticObjects);
	m_projectionType.AddEnumItem("ProjectOnTerrain", SDecalProperties::eProjectOnTerrain);
	m_projectionType.AddEnumItem("ProjectOnTerrainAndStaticObjects", SDecalProperties::eProjectOnTerrainAndStaticObjects);

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(m_projectionType, "ProjectionType", functor(*this, &CDecalObject::OnParamChanged));
	m_projectionType.SetDescription("0-Planar\n1-ProjectOnStaticObjects\n2-ProjectOnTerrain\n3-ProjectOnTerrainAndStaticObjects");

	m_pVarObject->AddVariable(m_deferredDecal, "Deferred", functor(*this, &CDecalObject::OnParamChanged));

	m_viewDistRatio = 100;
	m_pVarObject->AddVariable(m_viewDistRatio, "ViewDistRatio", functor(*this, &CDecalObject::OnViewDistRatioChanged));
	m_viewDistRatio.SetLimits(0, 255);

	m_sortPrio = 16;
	m_pVarObject->AddVariable(m_sortPrio, "SortPriority", functor(*this, &CDecalObject::OnParamChanged));
	m_sortPrio.SetLimits(0, 255);

	m_depth = 1.0f;
	m_pVarObject->AddVariable(m_depth, "ProjectionDepth", functor(*this, &CDecalObject::OnParamChanged));
	m_depth.SetLimits(0.0f, 10.0f, 1.0f / 255.0f, true, true);

	m_pRenderNode = 0;
	m_pPrevRenderNode = 0;
}

void CDecalObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CDecalObject>("Decal", [](CDecalObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->m_projectionType, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_deferredDecal, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_viewDistRatio, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_sortPrio, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->m_depth, ar);

		if (ar.openBlock("decal", "<Decal"))
		{
			Serialization::SEditToolButton reorientateButton("");
			reorientateButton.SetToolClass(RUNTIME_CLASS(CDecalObjectTool), "decal", pObject);
			ar(reorientateButton, "reorientate", "^Reorientate");
			ar(Serialization::ActionButton(std::bind(&CDecalObject::UpdateProjection, pObject)), "update_projection", "^Update Projection");
			ar.closeBlock();
		}
	});
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::OnParamChanged(IVariable* pVar)
{
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::OnViewDistRatioChanged(IVariable* pVar)
{
	if (m_pRenderNode)
	{
		m_pRenderNode->SetViewDistRatio(m_viewDistRatio);

		// set matrix again to for update of view distance ratio in engine
		const Matrix34& wtm(GetWorldTM());
		m_pRenderNode->SetMatrix(wtm);
	}
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CDecalObject::GetDefaultMaterial() const
{
	CMaterial* pDefMat(GetIEditorImpl()->GetMaterialManager()->LoadMaterial("Materials/Decals/Default"));
	pDefMat->AddRef();
	return pDefMat;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObject::Init(CBaseObject* prev, const string& file)
{
	CDecalObject* pSrcDecalObj((CDecalObject*) prev);

	SetColor(RGB(127, 127, 255));

	if (IsCreateGameObjects())
	{
		if (pSrcDecalObj)
			m_pPrevRenderNode = pSrcDecalObj->m_pRenderNode;
	}

	// Must be after SetDecal call.
	bool res = CBaseObject::Init(prev, file);

	if (pSrcDecalObj)
	{
		// clone custom properties
		m_projectionType = pSrcDecalObj->m_projectionType;
		m_deferredDecal = pSrcDecalObj->m_deferredDecal;
		m_renderFlags = pSrcDecalObj->m_renderFlags;
	}
	else
	{
		// new decal, set default material
		SetMaterial(GetDefaultMaterial());
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
int CDecalObject::GetProjectionType() const
{
	return m_projectionType;
}

void CDecalObject::UpdateProjection()
{
	if (GetProjectionType() > 0)
		UpdateEngineNode();
}
//////////////////////////////////////////////////////////////////////////
bool CDecalObject::CreateGameObject()
{
	if (!m_pRenderNode)
	{
		m_pRenderNode = static_cast<IDecalRenderNode*>(GetIEditorImpl()->Get3DEngine()->CreateRenderNode(eERType_Decal));

		if (m_pRenderNode && m_pPrevRenderNode)
			m_pPrevRenderNode = 0;

		UpdateEngineNode();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	if (m_pRenderNode)
	{
		GetIEditorImpl()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}

	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::UpdateEngineNode()
{
	if (!m_pRenderNode)
		return;

	bool boIsSelected(IsSelected());

	// update basic entity render flags
	m_renderFlags = 0;

	if (boIsSelected)
		m_renderFlags |= ERF_SELECTED;

	if (IsHidden() || IsHiddenBySpec())
		m_renderFlags |= ERF_HIDDEN;

	m_pRenderNode->SetRndFlags(m_renderFlags);

	// set properties
	SDecalProperties decalProperties;
	switch (m_projectionType)
	{
	case SDecalProperties::eProjectOnTerrainAndStaticObjects:
		{
			decalProperties.m_projectionType = SDecalProperties::eProjectOnTerrainAndStaticObjects;
			break;
		}
	case SDecalProperties::eProjectOnStaticObjects:
		{
			decalProperties.m_projectionType = SDecalProperties::eProjectOnStaticObjects;
			break;
		}
	case SDecalProperties::eProjectOnTerrain:
		{
			decalProperties.m_projectionType = SDecalProperties::eProjectOnTerrain;
			break;
		}
	case SDecalProperties::ePlanar:
	default:
		{
			decalProperties.m_projectionType = SDecalProperties::ePlanar;
			break;
		}
	}

	// update world transform
	const Matrix34& wtm(GetWorldTM());

	// get normalized rotation (remove scaling)
	Matrix33 rotation(wtm);
	if (m_projectionType != SDecalProperties::ePlanar)
	{
		rotation.SetRow(0, rotation.GetRow(0).GetNormalized());
		rotation.SetRow(1, rotation.GetRow(1).GetNormalized());
		rotation.SetRow(2, rotation.GetRow(2).GetNormalized());
	}

	decalProperties.m_pos = wtm.TransformPoint(Vec3(0, 0, 0));
	decalProperties.m_normal = wtm.TransformVector(Vec3(0, 0, 1));
	decalProperties.m_pMaterialName = GetMaterialName();
	decalProperties.m_radius = m_projectionType != SDecalProperties::ePlanar ? decalProperties.m_normal.GetLength() : 1;
	decalProperties.m_explicitRightUpFront = rotation;
	decalProperties.m_sortPrio = m_sortPrio;
	decalProperties.m_deferred = m_deferredDecal;
	decalProperties.m_depth = m_depth;
	m_pRenderNode->SetDecalProperties(decalProperties);

	m_pRenderNode->SetMatrix(wtm);
	m_pRenderNode->SetMinSpec(GetMinSpec());
	m_pRenderNode->SetMaterialLayers(GetMaterialLayersMask());
	m_pRenderNode->SetViewDistRatio(m_viewDistRatio);
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::SetHidden(bool bHidden)
{
	CBaseObject::SetHidden(bHidden);
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::UpdateVisibility(bool visible)
{
	CBaseObject::UpdateVisibility(visible);
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::SetMinSpec(uint32 nSpec, bool bSetChildren)
{
	__super::SetMinSpec(nSpec, bSetChildren);
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::SetMaterialLayersMask(uint32 nLayersMask)
{
	__super::SetMinSpec(nLayersMask);
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::SetMaterial(IEditorMaterial* mtl)
{
	if (!mtl)
		mtl = GetDefaultMaterial();
	CBaseObject::SetMaterial(mtl);
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::InvalidateTM(int nWhyFlags)
{
	CBaseObject::InvalidateTM(nWhyFlags);
	UpdateEngineNode();
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::GetLocalBounds(AABB& box)
{
	box = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::MouseCallbackImpl(CViewport* view, EMouseEvent event, CPoint& point, int flags, bool callerIsMouseCreateCallback)
{
	static bool s_mousePosTracked(false);

	if ((callerIsMouseCreateCallback && !flags))
	{
		Vec3 pos(view->MapViewToCP(point, 0, true, GetCreationOffsetFromTerrain()));
		SetPos(pos);
		s_mousePosTracked = false;
	}
	else
	{
		static CPoint s_prevMousePos;

		enum EInputMode
		{
			eNone,

			eAlign,
			eScale,
			eRotate
		};

		bool altKeyPressed(0 != GetAsyncKeyState(VK_MENU));

		EInputMode eInputMode(eNone);
		if ((flags & MK_CONTROL) && !altKeyPressed)
			eInputMode = eAlign;
		else if ((flags & MK_CONTROL) && altKeyPressed)
			eInputMode = eRotate;
		else if (altKeyPressed)
			eInputMode = eScale;

		switch (eInputMode)
		{
		case eAlign:
			{
				Vec3 raySrc, rayDir;
				GetIEditorImpl()->GetActiveView()->ViewToWorldRay(point, raySrc, rayDir);
				rayDir = rayDir.GetNormalized() * 1000.0f;

				ray_hit hit;
				if (gEnv->pPhysicalWorld->RayWorldIntersection(raySrc, rayDir, ent_all, rwi_stop_at_pierceable | rwi_ignore_terrain_holes, &hit, 1) > 0)
				{
					const Matrix34& wtm(GetWorldTM());
					Vec3 x(wtm.GetColumn0().GetNormalized());
					Vec3 y(wtm.GetColumn1().GetNormalized());
					Vec3 z(hit.n.GetNormalized());

					y = z.Cross(x);
					if (y.GetLengthSquared() < 1e-4f)
						y = z.GetOrthogonal();
					y.Normalize();
					x = y.Cross(z);

					Matrix33 newOrient;
					newOrient.SetColumn(0, x);
					newOrient.SetColumn(1, y);
					newOrient.SetColumn(2, z);
					Quat q(newOrient);

					SetPos(hit.pt, eObjectUpdateFlags_DoNotInvalidate);
					SetRotation(q, eObjectUpdateFlags_DoNotInvalidate);
					InvalidateTM(eObjectUpdateFlags_PositionChanged | eObjectUpdateFlags_RotationChanged);
				}
				break;
			}
		case eRotate:
			{
				if (!s_mousePosTracked)
					break;

				float angle(-DEG2RAD(point.x - s_prevMousePos.x));
				SetRotation(GetRotation() * Quat(Matrix33::CreateRotationZ(angle)));
				break;
			}
		case eScale:
			{
				if (!s_mousePosTracked)
					break;

				float scaleDelta(0.01f * (s_prevMousePos.y - point.y));
				Vec3 newScale(GetScale() + Vec3(scaleDelta, scaleDelta, scaleDelta));
				newScale.x = max(newScale.x, 0.1f);
				newScale.y = max(newScale.y, 0.1f);
				newScale.z = max(newScale.z, 0.1f);
				SetScale(newScale);
				break;
			}
		default:
			{
				break;
			}
		}

		s_prevMousePos = point;
		s_mousePosTracked = true;
	}
}

//////////////////////////////////////////////////////////////////////////
int CDecalObject::MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLUp)
	{
		MouseCallbackImpl((CViewport*)view, event, point, flags, true);

		if (event == eMouseLDown)
		{
			return MOUSECREATE_OK;
		}

		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback(view, event, point, flags);
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	if (!gViewportDebugPreferences.showDecalObjectHelper)
		return;

	if (IsSelected())
	{
		const Matrix34& wtm(GetWorldTM());

		Vec3 x1(wtm.TransformPoint(Vec3(-1, 0, 0)));
		Vec3 x2(wtm.TransformPoint(Vec3(1, 0, 0)));
		Vec3 y1(wtm.TransformPoint(Vec3(0, -1, 0)));
		Vec3 y2(wtm.TransformPoint(Vec3(0, 1, 0)));
		Vec3 p(wtm.TransformPoint(Vec3(0, 0, 0)));
		Vec3 n(wtm.TransformPoint(Vec3(0, 0, 1)));

		if (IsSelected())
			dc.SetSelectedColor();
		else if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor(GetColor());

		dc.DrawLine(p, n);
		dc.DrawLine(x1, x2);
		dc.DrawLine(y1, y2);

		if (IsSelected())
		{
			Vec3 p0 = wtm.TransformPoint(Vec3(-1, -1, 0));
			Vec3 p1 = wtm.TransformPoint(Vec3(-1, 1, 0));
			Vec3 p2 = wtm.TransformPoint(Vec3(1, 1, 0));
			Vec3 p3 = wtm.TransformPoint(Vec3(1, -1, 0));
			dc.DrawLine(p0, p1);
			dc.DrawLine(p1, p2);
			dc.DrawLine(p2, p3);
			dc.DrawLine(p3, p0);
			dc.DrawLine(p0, p2);
			dc.DrawLine(p1, p3);
		}
	}

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
void CDecalObject::Serialize(CObjectArchive& ar)
{
	CBaseObject::Serialize(ar);

	XmlNodeRef xmlNode(ar.node);

	if (ar.bLoading)
	{
		// load attributes
		int projectionType(SDecalProperties::ePlanar);
		xmlNode->getAttr("ProjectionType", projectionType);
		m_projectionType = projectionType;

		xmlNode->getAttr("RndFlags", m_renderFlags);

		// update render node
		if (!m_pRenderNode)
			CreateGameObject();

		UpdateEngineNode();
	}
	else
	{
		// save attributes
		xmlNode->setAttr("ProjectionType", m_projectionType);
		xmlNode->setAttr("RndFlags", ERF_GET_WRITABLE(m_renderFlags));
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CDecalObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef decalNode(CBaseObject::Export(levelPath, xmlNode));
	decalNode->setAttr("ProjectionType", m_projectionType);
	return decalNode;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObject::RayToLineDistance(const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt)
{
	Vec3 pa, pb;
	float ua, ub;
	if (!LineLineIntersect(pi, pj, rayLineP1, rayLineP2, pa, pb, ua, ub))
		return false;

	// If before ray origin.
	if (ub < 0)
		return false;

	float d(0);
	if (ua < 0)
		d = PointToLineDistance(rayLineP1, rayLineP2, pi, intPnt);
	else if (ua > 1)
		d = PointToLineDistance(rayLineP1, rayLineP2, pj, intPnt);
	else
	{
		intPnt = rayLineP1 + ub * (rayLineP2 - rayLineP1);
		d = (pb - pa).GetLength();
	}
	distance = d;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObject::HitTest(HitContext& hc)
{
	// Selectable by icon.
	/*
	   const Matrix34& wtm( GetWorldTM() );

	   Vec3 x1( wtm.TransformPoint( Vec3( -1,  0, 0 ) ) );
	   Vec3 x2( wtm.TransformPoint( Vec3(  1,  0, 0 ) ) );
	   Vec3 y1( wtm.TransformPoint( Vec3(  0, -1, 0 ) ) );
	   Vec3 y2( wtm.TransformPoint( Vec3(  0,  1, 0 ) ) );
	   Vec3  p( wtm.TransformPoint( Vec3(  0,  0, 0 ) ) );
	   Vec3  n( wtm.TransformPoint( Vec3(  0,  0, 1 ) ) );

	   float minDist( FLT_MAX );
	   Vec3 ip;
	   Vec3 intPnt;
	   Vec3 rayLineP1( hc.raySrc );
	   Vec3 rayLineP2( hc.raySrc + hc.rayDir * 100000.0f );

	   bool bWasIntersection( false );

	   float d( 0 );
	   if( RayToLineDistance( rayLineP1, rayLineP2, p, n, d, ip ) && d < minDist )
	   { minDist = d; intPnt = ip; bWasIntersection = true; }
	   if( RayToLineDistance( rayLineP1, rayLineP2, x1, x2, d, ip ) && d < minDist )
	   { minDist = d; intPnt = ip; bWasIntersection = true; }
	   if( RayToLineDistance( rayLineP1, rayLineP2, y1, y2, d, ip ) && d < minDist )
	   { minDist = d; intPnt = ip; bWasIntersection = true; }

	   float fRoadCloseDistance( 0.8f * hc.view->GetScreenScaleFactor( intPnt ) * 0.01f );
	   if( bWasIntersection && minDist < fRoadCloseDistance + hc.distanceTollerance )
	   {
	   hc.dist = hc.raySrc.GetDistance( intPnt );
	   return true;
	   }
	 */

	return false;
}

