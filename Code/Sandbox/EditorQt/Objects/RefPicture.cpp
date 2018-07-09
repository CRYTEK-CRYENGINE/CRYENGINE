// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RefPicture.h"
#include "Geometry/EdMesh.h"
#include "../Material/MaterialManager.h"
#include "Viewport.h"
#include "Objects/InspectorWidgetCreator.h"
#include <Preferences/ViewportPreferences.h>

REGISTER_CLASS_DESC(CRefPictureClassDesc);

#define REFERENCE_PICTURE_MATERAIL_TEMPLATE "%EDITOR%/Materials/refpicture.mtl"
#define REFERENCE_PICTURE_BASE_OBJECT       "%EDITOR%/Objects/refpicture.cgf"

#define DISPLAY_2D_SELECT_LINEWIDTH         2
#define DISPLAY_2D_SELECT_COLOR             RGB(225, 0, 0)
#define DISPLAY_GEOM_SELECT_COLOR           ColorB(250, 0, 250, 30)
#define DISPLAY_GEOM_SELECT_LINE_COLOR      ColorB(255, 255, 0, 160)

_smart_ptr<CMaterial> g_RefPictureMaterialTemplate = 0;

//-----------------------------------------------------------------------------
IMPLEMENT_DYNCREATE(CRefPicture, CBaseObject)

//-----------------------------------------------------------------------------
IMaterial * GetRefPictureMaterial()
{
	if (!g_RefPictureMaterialTemplate)
	{
		string filename = PathUtil::MakeGamePath(REFERENCE_PICTURE_MATERAIL_TEMPLATE);

		XmlNodeRef mtlNode = GetISystem()->LoadXmlFromFile(filename);
		if (mtlNode)
		{
			// Create a template material without registering DB.
			g_RefPictureMaterialTemplate = GetIEditorImpl()->GetMaterialManager()->CreateMaterial("RefPicture", mtlNode, MTL_FLAG_UIMATERIAL);

			// Create MatInfo by calling it here for the first time
			g_RefPictureMaterialTemplate->GetMatInfo();
			g_RefPictureMaterialTemplate->Release();
		}
	}

	IMaterial* pMat = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->CloneMaterial(g_RefPictureMaterialTemplate->GetMatInfo());

	return pMat;
}

//-----------------------------------------------------------------------------
CRefPicture::CRefPicture()
	: m_pMaterial(0)
	, m_pRenderNode(0)
	, m_pGeometry(0)
	, m_aspectRatio(1.f)
{
	mv_scale.Set(true);

	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(mv_picFilepath, "File", functor(*this, &CRefPicture::OnVariableChanged), IVariable::DT_FILE);
	m_pVarObject->AddVariable(mv_scale, "Scale", "Keep Aspect On Scale", functor(*this, &CRefPicture::OnVariableChanged));

	m_pMaterial = GetRefPictureMaterial();
}

//-----------------------------------------------------------------------------
void CRefPicture::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CRefPicture>("Reference Picture", [](CRefPicture* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_picFilepath, ar);
		pObject->m_pVarObject->SerializeVariable(&pObject->mv_scale, ar);
	});
}

//-----------------------------------------------------------------------------
void CRefPicture::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	if (dc.flags & DISPLAY_2D)
	{
		if (IsSelected())
		{
			dc.SetLineWidth(DISPLAY_2D_SELECT_LINEWIDTH);
			dc.SetColor(DISPLAY_2D_SELECT_COLOR);
		}
		else
		{
			dc.SetColor(GetColor());
		}

		dc.PushMatrix(GetWorldTM());
		dc.DrawWireBox(m_bbox.min, m_bbox.max);

		dc.PopMatrix();

		if (IsSelected())
			dc.SetLineWidth(0);

		if (m_pGeometry)
			m_pGeometry->Display(dc);
		return;
	}

	if (m_pGeometry)
	{
		if (!IsHighlighted())
		{
			m_pGeometry->Display(dc);
		}
		else
		{
			SGeometryDebugDrawInfo debugDrawInfo;
			debugDrawInfo.tm = GetWorldTM();
			debugDrawInfo.color = DISPLAY_GEOM_SELECT_COLOR;
			debugDrawInfo.lineColor = DISPLAY_GEOM_SELECT_LINE_COLOR;
			debugDrawInfo.bDrawInFront = true;
			m_pGeometry->DebugDraw(debugDrawInfo);
		}
	}

	DrawDefault(dc);
}

//-----------------------------------------------------------------------------
bool CRefPicture::HitTest(HitContext& hc)
{
	Vec3 localHit;
	Vec3 localRaySrc = hc.raySrc;
	Vec3 localRayDir = hc.rayDir;

	localRaySrc = m_invertTM.TransformPoint(localRaySrc);
	localRayDir = m_invertTM.TransformVector(localRayDir).GetNormalized();

	if (Intersect::Ray_AABB(localRaySrc, localRayDir, m_bbox, localHit))
	{
		if (hc.b2DViewport)
		{
			// World space distance.
			hc.dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(localHit));
			return true;
		}

		if (m_pRenderNode)
		{
			IStatObj* pStatObj = m_pRenderNode->GetEntityStatObj(0);
			if (pStatObj)
			{
				SRayHitInfo hi;
				hi.inReferencePoint = localRaySrc;
				hi.inRay = Ray(localRaySrc, localRayDir);
				if (pStatObj->RayIntersection(hi))
				{
					hc.dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(hi.vHitPos));
					return true;
				}
				return false;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
void CRefPicture::GetLocalBounds(AABB& box)
{
	box = m_bbox;
}

//-----------------------------------------------------------------------------
XmlNodeRef CRefPicture::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	// Don't export this. Only relevant for editor.
	return 0;
}

void CRefPicture::SerializeGeneralProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	CBaseObject::SerializeGeneralProperties(ar, bMultiEdit);

	Vec3 scale = GetScale();

	// Note, constrained scale will only work well for in place editing, not dragging, because old scale may still get overwritten
	// during serialization
	if (ar.isInput())
	{
		ApplyScale();
	}
}

//-----------------------------------------------------------------------------
bool CRefPicture::CreateGameObject()
{
	// Load geometry
	if (!m_pGeometry)
	{
		m_pGeometry = CEdMesh::LoadMesh(REFERENCE_PICTURE_BASE_OBJECT);
		assert(m_pGeometry);

		if (!m_pGeometry)
			return false;
	}

	// Create render node
	if (!m_pRenderNode)
	{
		m_pRenderNode = GetIEditorImpl()->Get3DEngine()->CreateRenderNode(eERType_Brush);

		if (m_pGeometry)
		{
			m_pGeometry->GetBounds(m_bbox);
			Matrix34A tm = GetWorldTM();
			m_pRenderNode->SetEntityStatObj(m_pGeometry->GetIStatObj(), &tm);
			if (m_pGeometry->GetIStatObj())
				m_pGeometry->GetIStatObj()->SetMaterial(m_pMaterial);
		}
	}

	// Apply material & reference picture image

	UpdateImage(mv_picFilepath);

	return true;
}

//-----------------------------------------------------------------------------
void CRefPicture::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	if (m_pRenderNode)
	{
		GetIEditorImpl()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}

	if (m_pGeometry)
		m_pGeometry = 0;

	if (m_pMaterial)
		m_pMaterial = 0;

	CBaseObject::Done();
}

//-----------------------------------------------------------------------------
void CRefPicture::InvalidateTM(int nWhyFlags)
{
	CBaseObject::InvalidateTM(nWhyFlags);

	if (!(nWhyFlags & eObjectUpdateFlags_RestoreUndo)) // Can skip updating game object when restoring undo.
	{
		if (m_pRenderNode && m_pGeometry)
		{
			Matrix34A tm = GetWorldTM();
			m_pRenderNode->SetEntityStatObj(m_pGeometry->GetIStatObj(), &tm);
		}
	}

	m_invertTM = GetWorldTM();
	m_invertTM.Invert();
}

//-----------------------------------------------------------------------------
void CRefPicture::OnVariableChanged(IVariable* piVariable)
{
	if (!strcmp(piVariable->GetName(), "File"))
		UpdateImage(mv_picFilepath);
	else if (!strcmp(piVariable->GetName(), "Scale"))
	{
		if (mv_scale)
		{
			ApplyScale();
		}
	}
}

//-----------------------------------------------------------------------------
void CRefPicture::UpdateImage(const string& picturePath)
{
	if (!m_pMaterial || !m_pRenderNode)
		return;

	string sgamePath = picturePath;
	SShaderItem& si(m_pMaterial->GetShaderItem());
	SInputShaderResourcesPtr isr = gEnv->pRenderer->EF_CreateInputShaderResource(si.m_pShaderResources);
	isr->m_Textures[EFTT_DIFFUSE].m_Name = PathUtil::MakeGamePath(sgamePath);

	SShaderItem siDst(GetIEditorImpl()->GetRenderer()->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, isr, si.m_pShader->GetGenerationMask()));
	m_pMaterial->AssignShaderItem(siDst);

	m_pRenderNode->SetMaterial(m_pMaterial);

	CImageEx image;
	if (CImageUtil::LoadImage(picturePath, image))
	{
		float w = (float)image.GetWidth();
		float h = (float)image.GetHeight();

		m_aspectRatio = (h != 0.f ? w / h : 1.f);
	}
	else
	{
		m_aspectRatio = 1.f;
	}

	// first load always keeps perspective
	ApplyScale();
}

//-----------------------------------------------------------------------------
void CRefPicture::ApplyScale(bool bHeight)
{
	Vec3 scale = GetScale();
	float scaleHeight;
	float scaleWidth;

	if (bHeight)
	{
		scaleHeight = scale.z;
		scaleWidth = scaleHeight * m_aspectRatio;
	}
	else
	{
		scaleWidth = scale.y;
		scaleHeight = scaleWidth / m_aspectRatio;
	}

	CBaseObject::SetScale(Vec3(1.0f, scaleWidth, scaleHeight));
}

