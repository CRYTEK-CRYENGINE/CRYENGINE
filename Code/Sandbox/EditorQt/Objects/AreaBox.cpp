// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AreaBox.h"
#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include "Controls/PropertiesPanel.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include <CryEntitySystem/IEntity.h>

#include <Serialization/Decorators/EntityLink.h>
#include <Serialization/Decorators/EditorActionButton.h>
#include <Serialization/Decorators/EditToolButton.h>

#include "PickObjectTool.h"

REGISTER_CLASS_DESC(CAreaBoxClassDesc);

class AreaTargetTool : public CPickObjectTool
{
	DECLARE_DYNCREATE(AreaTargetTool)

	struct SAreaTargetPicker : IPickObjectCallback
	{
		SAreaTargetPicker()
		{
		}

		void OnPick(CBaseObject* pObj) override
		{
			if (m_owner)
			{
				CUndo undo("Add target link");
				m_owner->AddEntity(pObj);
			}
		}

		void OnCancelPick() override
		{
		}

		CAreaObjectBase* m_owner;
	};

public:
	AreaTargetTool()
		: CPickObjectTool(&m_picker)
	{
	}

	virtual void SetUserData(const char* key, void* userData) override
	{
		m_picker.m_owner = static_cast<CAreaObjectBase*>(userData);
	}

private:
	SAreaTargetPicker m_picker;
};

IMPLEMENT_DYNCREATE(AreaTargetTool, CPickObjectTool)

//////////////////////////////////////////////////////////////////////////
CAreaObjectBase::CAreaObjectBase()
{
	m_entities.RegisterEventCallback(functor(*this, &CAreaObjectBase::OnObjectEvent));
}

//////////////////////////////////////////////////////////////////////////
CAreaObjectBase::~CAreaObjectBase()
{
	m_entities.UnregisterEventCallback(functor(*this, &CAreaObjectBase::OnObjectEvent));
}

//////////////////////////////////////////////////////////////////////////
void CAreaObjectBase::SerializeEntityTargets(Serialization::IArchive& ar, bool bMultiEdit)
{
	std::vector<Serialization::EntityTarget> links;
	for (size_t i = 0; i < m_entities.GetCount(); i++)
	{
		links.emplace_back(m_entities.Get(i)->GetId(), (LPCTSTR)m_entities.Get(i)->GetName());
	}

	ar(links, "attached_entities", "!Attached Entities");
	if (ar.isInput())
	{
		size_t linkCount = links.size();
		bool linksChanged = m_entities.GetCount() != linkCount;

		if (!linksChanged)
		{
			for (size_t i = 0; i < m_entities.GetCount(); i++)
			{
				if (m_entities.Get(i)->GetId() != links[i].guid)
				{
					linksChanged = true;
				}
			}
		}

		if (linksChanged)
		{
			while (!m_entities.IsEmpty())
			{
				RemoveEntity(0);
			}

			for (size_t i = 0; i < links.size(); i++)
			{
				CBaseObject* obj = GetIEditorImpl()->GetObjectManager()->FindObject(links[i].guid);
				if (obj)
				{
					AddEntity(obj);
				}
			}
		}
	}

	if (ar.isEdit())
	{
		if (ar.openBlock("linktools", "Target Tools"))
		{
			Serialization::SEditToolButton pickToolButton("");
			pickToolButton.SetToolClass(RUNTIME_CLASS(AreaTargetTool), nullptr, this);

			ar(pickToolButton, "picker", "^Pick");
			ar(Serialization::ActionButton([ = ] {
				CUndo undo("Clear entity links");
				while (!m_entities.IsEmpty())
				{
				  RemoveEntity(0);
				}
			}), "picker", "^Clear");

			ar.closeBlock();
		}
	}
}

void CAreaObjectBase::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CEntityObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CAreaObjectBase>("Area", &CAreaObjectBase::SerializeEntityTargets);
}

//////////////////////////////////////////////////////////////////////////
void CAreaObjectBase::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	__super::PostClone(pFromObject, ctx);

	CAreaObjectBase const* const pFrom = static_cast<CAreaObjectBase*>(pFromObject);
	// Clone event targets.
	if (!pFrom->m_entities.IsEmpty())
	{
		size_t const numEntities = pFrom->m_entities.GetCount();

		for (size_t i = 0; i < numEntities; i++)
		{
			CBaseObject* const __restrict pBaseObject = pFrom->m_entities.Get(i);
			CBaseObject* const __restrict pClonedBaseObject = ctx.FindClone(pBaseObject);

			if (pClonedBaseObject != nullptr)
			{
				m_entities.Add(pClonedBaseObject);
			}
			else
			{
				m_entities.Add(pBaseObject);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaObjectBase::OnObjectEvent(CBaseObject* const pBaseObject, int const event)
{
	if (event == OBJECT_ON_PREDELETE)
	{
		if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject const* const pEntityObject = static_cast<CEntityObject*>(pBaseObject);

			if (pEntityObject != nullptr && pEntityObject->GetIEntity() != nullptr)
			{
				OnEntityRemoved(pEntityObject->GetIEntity());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaObjectBase::AddEntity(CBaseObject* const pBaseObject)
{
	CRY_ASSERT(pBaseObject != nullptr);
	StoreUndo("Add Entity");
	m_entities.Add(pBaseObject);

	if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		OnEntityAdded(static_cast<CEntityObject*>(pBaseObject)->GetIEntity());
	}

	UpdateUIVars();
}

//////////////////////////////////////////////////////////////////////////
void CAreaObjectBase::RemoveEntity(size_t const index)
{
	CRY_ASSERT(index >= 0 && index < m_entities.GetCount());

	if (index < m_entities.GetCount())
	{
		StoreUndo("Remove Entity");
		CBaseObject* const pBaseObject = m_entities.Get(index);
		CEntityObject const* const pEntity = GetEntity(index);

		if (pEntity != nullptr)
		{
			OnEntityRemoved(pEntity->GetIEntity());
		}

		m_entities.Remove(pBaseObject);

		UpdateUIVars();
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityObject* CAreaObjectBase::GetEntity(size_t const index)
{
	CRY_ASSERT(index >= 0 && index < m_entities.GetCount());
	CBaseObject* const pBaseObject = m_entities.Get(index);

	if (pBaseObject != nullptr && pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		return static_cast<CEntityObject*>(pBaseObject);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAreaObjectBase::Serialize(CObjectArchive& ar)
{
	__super::Serialize(ar);

	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		m_entities.Clear();
		XmlNodeRef const entities = xmlNode->findChild("Entities");

		if (entities)
		{
			int const numChildren = entities->getChildCount();

			for (int i = 0; i < numChildren; ++i)
			{
				XmlNodeRef const ent = entities->getChild(i);
				CryGUID entityId;

				if (ent->getAttr("Id", entityId))
				{
					ar.SetResolveCallback(this, entityId, functor(m_entities, &CSafeObjectsArray::Add));
				}
			}
		}
	}
	else
	{
		if (!m_entities.IsEmpty())
		{
			XmlNodeRef const nodes = xmlNode->newChild("Entities");
			size_t const numEntities = m_entities.GetCount();

			for (size_t i = 0; i < numEntities; i++)
			{
				XmlNodeRef entNode = nodes->newChild("Entity");

				if (m_entities.Get(i) != nullptr)
				{
					entNode->setAttr("Id", m_entities.Get(i)->GetId());
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaObjectBase::DrawEntityLinks(DisplayContext& dc)
{
	if (!m_entities.IsEmpty())
	{
		Vec3 const vcol = Vec3(1.0f, 1.0f, 1.0f);
		size_t const numEntities = m_entities.GetCount();

		for (size_t i = 0; i < numEntities; ++i)
		{
			CBaseObject const* const pBaseObject = GetEntity(i);

			if (pBaseObject != nullptr)
			{
				dc.DrawLine(GetWorldPos(), pBaseObject->GetWorldPos(), ColorF(vcol.x, vcol.y, vcol.z, 0.7f), ColorF(1, 1, 1, 0.7f));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CAreaBox, CEntityObject)

//////////////////////////////////////////////////////////////////////////
CAreaBox::CAreaBox()
	: m_innerFadeDistance(0.0f)
{
	m_areaId = -1;
	m_edgeWidth = 0;
	mv_width = 4;
	mv_length = 4;
	mv_height = 1;
	mv_groupId = 0;
	mv_priority = 0;
	mv_filled = false;
	mv_displaySoundInfo = false;
	m_entityClass = "AreaBox";

	// Resize for 6 sides
	m_abObstructSound.resize(6);

	m_box.min = Vec3(-mv_width / 2, -mv_length / 2, 0);
	m_box.max = Vec3(mv_width / 2, mv_length / 2, mv_height);

	m_bIgnoreGameUpdate = 1;

	m_pOwnSoundVarBlock = new CVarBlock;
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::InitVariables()
{
	if (m_pVarObject == nullptr)
	{
		m_pVarObject = stl::make_unique<CVarObject>();
	}

	m_pVarObject->AddVariable(m_innerFadeDistance, "InnerFadeDistance", functor(*this, &CAreaBox::OnAreaChange));
	m_pVarObject->AddVariable(m_areaId, "AreaId", functor(*this, &CAreaBox::OnAreaChange));
	m_pVarObject->AddVariable(m_edgeWidth, "FadeInZone", functor(*this, &CAreaBox::OnSizeChange));
	m_pVarObject->AddVariable(mv_width, "Width", functor(*this, &CAreaBox::OnSizeChange));
	m_pVarObject->AddVariable(mv_length, "Length", functor(*this, &CAreaBox::OnSizeChange));
	m_pVarObject->AddVariable(mv_height, "Height", functor(*this, &CAreaBox::OnSizeChange));
	m_pVarObject->AddVariable(mv_groupId, "GroupId", functor(*this, &CAreaBox::OnAreaChange));
	m_pVarObject->AddVariable(mv_priority, "Priority", functor(*this, &CAreaBox::OnAreaChange));
	m_pVarObject->AddVariable(mv_filled, "DisplayFilled");
	m_pVarObject->AddVariable(mv_displaySoundInfo, "DisplaySoundInfo", functor(*this, &CAreaBox::OnSoundParamsChange));
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::Done()
{
	m_pOwnSoundVarBlock->DeleteAllVariables();

	CEntityObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CAreaBox::Init(CBaseObject* prev, const string& file)
{
	m_bIgnoreGameUpdate = 1;

	SetColor(RGB(0, 0, 255));
	bool res = CEntityObject::Init(prev, file);

	if (m_pEntity != nullptr)
	{
		m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();
	}

	if (prev)
	{
		m_abObstructSound = ((CAreaBox*)prev)->m_abObstructSound;
		CRY_ASSERT(m_abObstructSound.size() == 6);
	}

	m_bIgnoreGameUpdate = 0;

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::GetLocalBounds(AABB& box)
{
	box = m_box;
}

//////////////////////////////////////////////////////////////////////////
bool CAreaBox::HitTest(HitContext& hc)
{
	Vec3 p;
	/*BBox box;
	   box = m_box;
	   box.Transform( GetWorldTM() );
	   float tr = hc.distanceTollerance/2;
	   box.min -= Vec3(tr,tr,tr);
	   box.max += Vec3(tr,tr,tr);
	   if (box.IsIntersectRay(hc.raySrc,hc.rayDir,p ))
	   {
	   hc.dist = Vec3(hc.raySrc - p).Length();
	   return true;
	   }*/
	Matrix34 invertWTM = GetWorldTM();
	Vec3 worldPos = invertWTM.GetTranslation();
	invertWTM.Invert();

	Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
	Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir).GetNormalized();

	float epsilonDist = max(.1f, hc.view->GetScreenScaleFactor(worldPos) * 0.01f);
	epsilonDist *= max(0.0001f, min(invertWTM.GetColumn0().GetLength(), min(invertWTM.GetColumn1().GetLength(), invertWTM.GetColumn2().GetLength())));
	float hitDist;

	float tr = hc.distanceTolerance / 2 + 1;
	AABB box;
	box.min = m_box.min - Vec3(tr + epsilonDist, tr + epsilonDist, tr + epsilonDist);
	box.max = m_box.max + Vec3(tr + epsilonDist, tr + epsilonDist, tr + epsilonDist);

	if (Intersect::Ray_AABB(xformedRaySrc, xformedRayDir, box, p))
	{
		if (Intersect::Ray_AABBEdge(xformedRaySrc, xformedRayDir, m_box, epsilonDist, hitDist, p))
		{
			hc.dist = xformedRaySrc.GetDistance(p);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CAreaObjectBase::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CAreaBox>("Area Box", &CAreaBox::SerializeProperties);
}

void CAreaBox::SerializeProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	m_pVarObject->SerializeVariable(&m_innerFadeDistance, ar);
	m_pVarObject->SerializeVariable(&m_areaId, ar);
	m_pVarObject->SerializeVariable(&m_edgeWidth, ar);
	m_pVarObject->SerializeVariable(&mv_width, ar);
	m_pVarObject->SerializeVariable(&mv_length, ar);
	m_pVarObject->SerializeVariable(&mv_height, ar);
	m_pVarObject->SerializeVariable(&mv_groupId, ar);
	m_pVarObject->SerializeVariable(&mv_priority, ar);
	m_pVarObject->SerializeVariable(&mv_filled, ar);
	m_pVarObject->SerializeVariable(&mv_displaySoundInfo, ar);

	if (!m_abObstructSound.empty() && !m_bIgnoreGameUpdate && mv_displaySoundInfo && !bMultiEdit)
	{
		if (!ar.isInput() || m_pOwnSoundVarBlock->IsEmpty())
		{
			if (!m_pOwnSoundVarBlock->IsEmpty())
				m_pOwnSoundVarBlock->DeleteAllVariables();
			// If the shape hasn't got a height subtract 2 for roof and floor
			CRY_ASSERT(m_abObstructSound.size() == 6);
			size_t const nVarCount = m_abObstructSound.size() - ((mv_height > 0.0f) ? 0 : 2);
			for (size_t i = 0; i < nVarCount; ++i)
			{
				CVariable<bool>* const pvTemp = new CVariable<bool>;
				pvTemp->Set(m_abObstructSound[i]);
				pvTemp->AddOnSetCallback(functor(*this, &CAreaBox::OnPointChange));

				string cTemp;
				cTemp.Format("Side:%d", i + 1);

				pvTemp->SetName(cTemp);
				m_pOwnSoundVarBlock->AddVariable(pvTemp);
			}
		}

		ar(*m_pOwnSoundVarBlock, "SoundObstruction", "|Sound Obstruction");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::Reload(bool bReloadScript /* = false */)
{
	__super::Reload(bReloadScript);

	// During reloading the entity+proxies get completely removed.
	// Hence we need to completely recreate the proxy and update all data to it.
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::OnAreaChange(IVariable* pVariable)
{
	if (m_bIgnoreGameUpdate == 0 && m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetID(m_areaId);
			pArea->SetGroup(mv_groupId);
			pArea->SetPriority(mv_priority);
			pArea->SetInnerFadeDistance(m_innerFadeDistance);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::OnSizeChange(IVariable* pVariable)
{
	Vec3 size(0, 0, 0);
	size.x = mv_width;
	size.y = mv_length;
	size.z = mv_height;

	m_box.min = -size / 2;
	m_box.max = size / 2;
	// Make volume position bottom of bounding box.
	m_box.min.z = 0;
	m_box.max.z = size.z;

	InvalidateWorldBox();

	if (m_bIgnoreGameUpdate == 0 && m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetBox(m_box.min, m_box.max, nullptr, 0);
			pArea->SetProximity(m_edgeWidth);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	if (!gViewportDebugPreferences.showAreaObjectHelper)
		return;

	if (!mv_displaySoundInfo)
	{
		COLORREF wireColor, solidColor;
		float wireOffset = 0;
		float alpha = 0.3f;
		if (IsSelected())
		{
			wireColor = dc.GetSelectedColor();
			solidColor = GetColor();
			wireOffset = -0.1f;
		}
		else
		{
			wireColor = GetColor();
			solidColor = GetColor();
		}

		//dc.renderer->SetCullMode( R_CULL_DISABLE );

		dc.PushMatrix(GetWorldTM());
		AABB box = m_box;

		bool bFrozen = IsFrozen();

		if (bFrozen)
			dc.SetFreezeColor();
		//////////////////////////////////////////////////////////////////////////
		if (!bFrozen)
			dc.SetColor(solidColor, alpha);

		if (IsSelected())
		{
			dc.DepthWriteOff();
			dc.DrawSolidBox(box.min, box.max);
			dc.DepthWriteOn();
		}

		if (!bFrozen)
			dc.SetColor(wireColor, 1);

		if (IsSelected())
		{
			dc.SetLineWidth(3.0f);
			dc.DrawWireBox(box.min, box.max);
			dc.SetLineWidth(0);
		}
		else
			dc.DrawWireBox(box.min, box.max);
		if (m_edgeWidth)
		{
			float fFadeScale = m_edgeWidth / 200.0f;
			AABB InnerBox = box;
			Vec3 EdgeDist = InnerBox.max - InnerBox.min;
			InnerBox.min.x += EdgeDist.x * fFadeScale;
			InnerBox.max.x -= EdgeDist.x * fFadeScale;
			InnerBox.min.y += EdgeDist.y * fFadeScale;
			InnerBox.max.y -= EdgeDist.y * fFadeScale;
			InnerBox.min.z += EdgeDist.z * fFadeScale;
			InnerBox.max.z -= EdgeDist.z * fFadeScale;
			dc.DrawWireBox(InnerBox.min, InnerBox.max);
		}
		if (mv_filled)
		{
			dc.SetAlpha(0.2f);
			dc.DrawSolidBox(box.min, box.max);
		}
		//////////////////////////////////////////////////////////////////////////

		dc.PopMatrix();

		DrawEntityLinks(dc);
		DrawDefault(dc);
	}
	else
	{
		ColorB const oObstructionFilled(255, 0, 0, 120);
		ColorB const oObstructionFilledNotSelected(255, 0, 0, 30);
		ColorB const oObstructionNotFilled(255, 0, 0, 10);
		ColorB const oNoObstructionFilled(0, 255, 0, 120);
		ColorB const oNoObstructionFilledNotSelected(0, 255, 0, 30);
		ColorB const oNoObstructionNotFilled(0, 255, 0, 10);

		COLORREF oWireColor;
		if (IsSelected())
			oWireColor = dc.GetSelectedColor();
		else
			oWireColor = GetColor();

		dc.SetColor(oWireColor);

		dc.PushMatrix(GetWorldTM());

		if (mv_height > 0.0f)
			dc.DrawWireBox(m_box.min, m_box.max);

		Vec3 const v3BoxMin = m_box.min;
		Vec3 const v3BoxMax = m_box.max;

		// Now build the 6 segments
		float const fLength = v3BoxMax.x - v3BoxMin.x;
		float const fWidth = v3BoxMax.y - v3BoxMin.y;
		float const fHeight = v3BoxMax.z - v3BoxMin.z;

		Vec3 const v0(v3BoxMin);
		Vec3 const v1(Vec3(v3BoxMin.x + fLength, v3BoxMin.y, v3BoxMin.z));
		Vec3 const v2(Vec3(v3BoxMin.x + fLength, v3BoxMin.y + fWidth, v3BoxMin.z));
		Vec3 const v3(Vec3(v3BoxMin.x, v3BoxMin.y + fWidth, v3BoxMin.z));
		Vec3 const v4(Vec3(v3BoxMin.x, v3BoxMin.y, v3BoxMin.z + fHeight));
		Vec3 const v5(Vec3(v3BoxMin.x + fLength, v3BoxMin.y, v3BoxMin.z + fHeight));
		Vec3 const v6(Vec3(v3BoxMin.x + fLength, v3BoxMin.y + fWidth, v3BoxMin.z + fHeight));
		Vec3 const v7(Vec3(v3BoxMin.x, v3BoxMin.y + fWidth, v3BoxMin.z + fHeight));

		// Describe the box faces
		SBoxFace const aoBoxFaces[6] =
		{
			SBoxFace(&v0, &v4, &v5, &v1),
			SBoxFace(&v1, &v5, &v6, &v2),
			SBoxFace(&v2, &v6, &v7, &v3),
			SBoxFace(&v3, &v7, &v4, &v0),
			SBoxFace(&v4, &v5, &v6, &v7),
			SBoxFace(&v0, &v1, &v2, &v3)
		};

		// Draw each side
		unsigned int const nSideCount = ((mv_height > 0.0f) ? 6 : 4);
		for (unsigned int i = 0; i < nSideCount; ++i)
		{
			if (mv_height == 0.0f)
			{
				if (IsSelected())
				{
					if (m_abObstructSound[i])
						dc.SetColor(oObstructionFilled);
					else
						dc.SetColor(oNoObstructionFilled);
				}
				else
					dc.SetColor(oWireColor);

				dc.DrawLine(*(aoBoxFaces[i].pP1), *(aoBoxFaces[i].pP4));
			}

			if (mv_height > 0.0f)
			{
				// Manipulate the color so it looks a little thicker and redder
				if (mv_filled || gViewportPreferences.fillSelectedShapes)
				{
					ColorB const nColor = dc.GetColor();

					if (m_abObstructSound[i])
						dc.SetColor(IsSelected() ? oObstructionFilled : oObstructionFilledNotSelected);
					else
						dc.SetColor(IsSelected() ? oNoObstructionFilled : oNoObstructionFilledNotSelected);

					dc.CullOff();
					dc.DepthWriteOff();

					dc.DrawQuad(*(aoBoxFaces[i].pP1), *(aoBoxFaces[i].pP2), *(aoBoxFaces[i].pP3), *(aoBoxFaces[i].pP4));

					dc.DepthWriteOn();
					dc.CullOn();
					dc.SetColor(nColor);
				}
				else if (IsSelected())
				{
					ColorB const nColor = dc.GetColor();

					if (m_abObstructSound[i])
						dc.SetColor(oObstructionNotFilled);
					else
						dc.SetColor(oNoObstructionNotFilled);

					dc.CullOff();
					dc.DepthWriteOff();

					dc.DrawQuad(*(aoBoxFaces[i].pP1), *(aoBoxFaces[i].pP2), *(aoBoxFaces[i].pP3), *(aoBoxFaces[i].pP4));

					dc.DepthWriteOn();
					dc.CullOn();
					dc.SetColor(nColor);
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		dc.PopMatrix();

		DrawEntityLinks(dc);
		DrawDefault(dc);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::InvalidateTM(int nWhyFlags)
{
	CEntityObject::InvalidateTM(nWhyFlags);
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::Serialize(CObjectArchive& ar)
{
	m_bIgnoreGameUpdate = 1;
	__super::Serialize(ar);
	m_bIgnoreGameUpdate = 0;
	CVarBlockPtr soundVarBlock = new CVarBlock;

	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		AABB box;
		xmlNode->getAttr("BoxMin", box.min);
		xmlNode->getAttr("BoxMax", box.max);

		SetAreaId(m_areaId);
		SetBox(box);
	}
	else
	{
		// Saving.
		//		xmlNode->setAttr( "AreaId",m_areaId );
		xmlNode->setAttr("BoxMin", m_box.min);
		xmlNode->setAttr("BoxMax", m_box.max);
	}

	if (ar.bLoading)
	{
		for (size_t i = 0; i < ((mv_height > 0.0f) ? 6 : 4); ++i)
		{
			CVariable<bool>* const pvTemp = new CVariable<bool>;

			stack_string cTemp;
			cTemp.Format("Side%d", i + 1);

			// And add each to the block
			soundVarBlock->AddVariable(pvTemp, cTemp);
		}

		// Now read in the data
		XmlNodeRef pSoundDataNode = xmlNode->findChild("SoundData");
		if (pSoundDataNode)
		{
			// Make sure we stay backwards compatible and also find the old "Side:" formatting
			// This can go out once every level got saved with the new formatting
			char const* const pcTemp = pSoundDataNode->getAttr("Side:1");
			if (pcTemp && pcTemp[0])
			{
				// We have the old formatting
				soundVarBlock->DeleteAllVariables();

				for (size_t i = 0; i < ((mv_height > 0.0f) ? 6 : 4); ++i)
				{
					CVariable<bool>* const pvTempOld = new CVariable<bool>;

					stack_string cTempOld;
					cTempOld.Format("Side:%d", i + 1);

					// And add each to the block
					soundVarBlock->AddVariable(pvTempOld, cTempOld);
				}
			}

			soundVarBlock->Serialize(pSoundDataNode, true);
		}

		// Copy the data to the "bools" list
		// First clear the remains out
		m_abObstructSound.clear();

		unsigned int const nVarCount = soundVarBlock->GetNumVariables();
		for (unsigned int i = 0; i < nVarCount; ++i)
		{
			IVariable const* const pTemp = soundVarBlock->GetVariable(i);
			if (pTemp)
			{
				bool bTemp = false;
				pTemp->Get(bTemp);
				m_abObstructSound.push_back(bTemp);

				if (i >= nVarCount - 2)
				{
					// Here check for backwards compatibility reasons if "ObstructRoof" and "ObstructFloor" still exists
					bool bTemp = false;
					if (i == nVarCount - 2)
					{
						if (xmlNode->getAttr("ObstructRoof", bTemp))
							m_abObstructSound[i] = bTemp;
					}
					else
					{
						if (xmlNode->getAttr("ObstructFloor", bTemp))
							m_abObstructSound[i] = bTemp;
					}
				}
			}
		}

		// Since the var block is a static object clear it for the next object to be empty
		soundVarBlock->DeleteAllVariables();
	}
	else
	{
		XmlNodeRef pSoundDataNode = xmlNode->newChild("SoundData");
		if (pSoundDataNode)
		{
			// TODO: What if mv_height or/and mv_width or/and mv_length == 0 ??
			// For now just serialize all data
			// First clear the remains out
			soundVarBlock->DeleteAllVariables();

			// Create the variables
			CRY_ASSERT(m_abObstructSound.size() == 6);

			size_t nIndex = 0;
			tSoundObstruction::const_iterator Iter(m_abObstructSound.begin());
			tSoundObstruction::const_iterator const IterEnd(m_abObstructSound.end());

			for (; Iter != IterEnd; ++Iter)
			{
				bool const bObstructed = (bool)(*Iter);

				CVariable<bool>* const pvTemp = new CVariable<bool>;
				pvTemp->Set(bObstructed);
				stack_string cTemp;
				cTemp.Format("Side%d", ++nIndex);

				// And add each to the block
				soundVarBlock->AddVariable(pvTemp, cTemp);
			}

			soundVarBlock->Serialize(pSoundDataNode, false);
			soundVarBlock->DeleteAllVariables();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CAreaBox::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef objNode = CEntityObject::Export(levelPath, xmlNode);
	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::SetAreaId(int nAreaId)
{
	m_areaId = nAreaId;

	if (m_bIgnoreGameUpdate == 0 && m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetID(m_areaId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CAreaBox::GetAreaId()
{
	return m_areaId;
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::SetBox(AABB box)
{
	m_box = box;

	if (m_bIgnoreGameUpdate == 0 && m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->SetBox(m_box.min, m_box.max, nullptr, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
AABB CAreaBox::GetBox()
{
	return m_box;
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::PostLoad(CObjectArchive& ar)
{
	__super::PostLoad(ar);
	UpdateGameArea();
	UpdateAttachedEntities();
}

//////////////////////////////////////////////////////////////////////////
bool CAreaBox::CreateGameObject()
{
	bool const bSuccess = __super::CreateGameObject();

	if (bSuccess)
	{
		UpdateGameArea();
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::OnEntityAdded(IEntity const* const pIEntity)
{
	if (m_bIgnoreGameUpdate == 0 && m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->AddEntity(pIEntity->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::OnEntityRemoved(IEntity const* const pIEntity)
{
	if (m_bIgnoreGameUpdate == 0 && m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->RemoveEntity(pIEntity->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::UpdateGameArea()
{
	if (m_bIgnoreGameUpdate == 0 && m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			bool abObstructSound[6] = { false };

			size_t nIndex = 0;
			tSoundObstruction::const_iterator Iter(m_abObstructSound.begin());
			tSoundObstruction::const_iterator const IterEnd(m_abObstructSound.end());

			for (; Iter != IterEnd; ++Iter)
			{
				// Here we "unpack" the data! (1 bit*nPointsCount to 1 byte*nPointsCount)
				bool const bObstructed = (bool)(*Iter);
				abObstructSound[nIndex] = bObstructed;
				++nIndex;
			}

			UpdateSoundPanelParams();
			pArea->SetBox(m_box.min, m_box.max, &abObstructSound[0], 6);
			pArea->SetProximity(m_edgeWidth);
			pArea->SetID(m_areaId);
			pArea->SetGroup(mv_groupId);
			pArea->SetPriority(mv_priority);
			pArea->SetInnerFadeDistance(m_innerFadeDistance);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::UpdateAttachedEntities()
{
	if (m_bIgnoreGameUpdate == 0 && m_pEntity != nullptr)
	{
		IEntityAreaComponent* pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

		if (pArea)
		{
			pArea->RemoveEntities();
			size_t const numEntities = GetEntityCount();

			for (size_t i = 0; i < numEntities; ++i)
			{
				CEntityObject* const pEntity = GetEntity(i);

				if (pEntity != nullptr && pEntity->GetIEntity() != nullptr)
				{
					pArea->AddEntity(pEntity->GetIEntity()->GetId());
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::OnSoundParamsChange(IVariable* var)
{
	if (!m_bIgnoreGameUpdate && mv_displaySoundInfo)
	{
		// Refresh inspector
		UpdateUIVars();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::OnPointChange(IVariable* var)
{
	CRY_ASSERT(m_abObstructSound.size() == 6);
	size_t const numVariables = static_cast<size_t>(m_pOwnSoundVarBlock->GetNumVariables());

	for (size_t i = 0; i < numVariables; ++i)
	{
		if (m_pOwnSoundVarBlock->GetVariable(i) == var)
		{
			if (m_pEntity != nullptr)
			{
				IEntityAreaComponent* const pArea = m_pEntity->GetOrCreateComponent<IEntityAreaComponent>();

				if (pArea)
				{
					bool bValue = false;
					var->Get(bValue);
					pArea->SetSoundObstructionOnAreaFace(i, bValue);
					m_abObstructSound[i] = bValue;
				}
			}

			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaBox::UpdateSoundPanelParams()
{
	if (!m_bIgnoreGameUpdate && mv_displaySoundInfo)
	{
		UpdateUIVars();
	}
}

