// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DesignerObject.h"
#include "Core/ModelCompiler.h"
#include "Material/Material.h"
#include "Material/MaterialManager.h"
#include "DesignerEditor.h"
#include "Util/Converter.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "ObjectCreateTool.h"
#include <CryCore/Base64.h>
#include "GameEngine.h"
#include "ToolFactory.h"
#include "Core/Helper.h"
#include "Util/Display.h"
#include "Controls/DynamicPopupMenu.h"
#include <Serialization/Decorators/EditorActionButton.h>
#include "Tools/ToolCommon.h"
#include "Util/PrimitiveShape.h"
#include "Core/Common.h"
#include "DesignerSession.h"
#include "Util/Undo.h"
#include <Preferences/ViewportPreferences.h>

namespace Designer
{
IMPLEMENT_DYNCREATE(DesignerObject, CBaseObject)

REGISTER_CLASS_DESC(BoxObjectClassDesc)
REGISTER_CLASS_DESC(SphereObjectClassDesc)
REGISTER_CLASS_DESC(ConeObjectClassDesc)
REGISTER_CLASS_DESC(CylinderObjectClassDesc)

REGISTER_CLASS_DESC(DesignerObjectClassDesc)

REGISTER_CLASS_DESC(SolidObjectClassDesc)

DesignerObject::DesignerObject()
{
	m_pCompiler = new ModelCompiler(eCompiler_General);
	m_nBrushUniqFileId = s_nGlobalBrushDesignerFileId;
	m_EngineFlags.m_pObj = this;
	++s_nGlobalBrushDesignerFileId;
	m_bSupportsBoxHighlight = false;
}

void GeneratePrimitiveShape(PolygonList& polygonList, string primitiveType)
{
	PrimitiveShape bp;

	BrushVec3 min(0.0, 0.0, 0.0);
	BrushVec3 max(1.0, 1.0, 1.0);

	if (primitiveType == "DesignerSphere")
	{
		min -= BrushVec3(0.0, 0.0, 0.5);
		max -= BrushVec3(0.0, 0.0, 0.5);

		bp.CreateSphere(
		  min,
		  max,
		  16,
		  &polygonList);
	}
	else if (primitiveType == "DesignerBox")
	{
		bp.CreateBox(
		  min,
		  max,
		  &polygonList);
	}
	else if (primitiveType == "DesignerCylinder")
	{
		bp.CreateCylinder(
		  min,
		  max,
		  16,
		  &polygonList);
	}
	else if (primitiveType == "DesignerCone")
	{
		bp.CreateCone(
		  min,
		  max,
		  16,
		  &polygonList);
	}
}

bool DesignerObject::Init(CBaseObject* prev, const string& file)
{
	SetMaterial(BrushDesignerDefaultMaterial);

	if (prev)
	{
		DesignerObject* pGeomObj = static_cast<DesignerObject*>(prev);

		Model* pModel = pGeomObj->GetModel();
		if (pModel)
		{
			Model* pClonedModel = new Model(*pModel);
			SetModel(pClonedModel);
		}

		ModelCompiler* pFrontBrush = pGeomObj->GetCompiler();
		if (pFrontBrush)
		{
			ModelCompiler* pClonedCompilier = new ModelCompiler(*pFrontBrush);
			SetCompiler(pClonedCompilier);
			ApplyPostProcess(GetMainContext(), ePostProcess_Mesh);
		}
	}

	bool ret = CBaseObject::Init(prev, file);

	// Finished initialization, now generate geometry for this object type.
	// Do this only for new objects, or we risk duplicating lots of cubes
	if (ret && !prev && (!file.IsEmpty()))
	{
		PolygonList polygonList;

		polygonList.clear();

		GeneratePrimitiveShape(polygonList, file);

		MODEL_SHELF_RECONSTRUCTOR(GetModel());

		GetModel()->SetShelf(eShelf_Base);
		GetModel()->Clear();
		for (int i = 0, iPolygonCount(polygonList.size()); i < iPolygonCount; ++i)
		{
			polygonList[i]->SetSubMatID(0);
			GetModel()->AddUnionPolygon(polygonList[i]);
		}

		ApplyPostProcess(GetMainContext(), ePostProcess_Mesh);
	}

	return ret;
}

void DesignerObject::Display(CObjectRenderHelper& objRenderHelper)
{
	const CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	bool bDisplay2D = dc.flags & DISPLAY_2D;
	if (bDisplay2D)
	{
		dc.PushMatrix(GetWorldTM());
		Display::DisplayModel(dc, GetModel(), NULL, eShelf_Any, 2, IsSelected() ? kSelectedColor : ColorB(0, 0, 0));
		dc.PopMatrix();
	}
	else
	{
		DrawDefault(dc);
	}

	if (!bDisplay2D)
		DrawOpenPolygons(dc);
}

void DesignerObject::DrawOpenPolygons(DisplayContext& dc)
{
	dc.PushMatrix(GetWorldTM());

	ColorB oldColor = dc.GetColor();
	int oldLineWidth = dc.GetLineWidth();

	dc.SetColor(ColorB(0, 0, 2));
	dc.SetLineWidth(2);
	for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr pPolygon = GetModel()->GetPolygon(i);
		if (!pPolygon->IsOpen())
			continue;
		Display::DisplayPolygon(dc, pPolygon);
	}
	dc.SetColor(oldColor);
	dc.SetLineWidth(oldLineWidth);

	dc.PopMatrix();
}

void DesignerObject::GetBoundBox(AABB& box)
{
	if (GetModel())
	{
		for (int i = eShelf_Base; i < cShelfMax; ++i)
		{
			if (!GetModel()->IsEmpty(static_cast<ShelfID>(i)))
			{
				box.SetTransformedAABB(GetWorldTM(), GetModel()->GetBoundBox(static_cast<ShelfID>(i)));
				return;
			}
		}
	}

	BrushVec3 vWorldPos = GetWorldTM().GetTranslation();
	box = AABB(vWorldPos, vWorldPos);
}

void DesignerObject::GetLocalBounds(AABB& box)
{
	if (GetModel())
	{
		box = GetModel()->GetBoundBox(eShelf_Base);
	}
	else
	{
		box = AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
	}
}

bool DesignerObject::HitTest(HitContext& hc)
{
	BrushRay ray;
	GetLocalViewRay(GetWorldTM(), hc.view, hc.point2d, ray);

	if (Designer::HitTest(ray, GetMainContext(), hc))
	{
		hc.object = this;
		return true;
	}
	else if (GetModel())
	{
		BrushMatrix34 invMat = GetWorldTM().GetInverted();
		for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr pPolygon = GetModel()->GetPolygon(i);
			if (!pPolygon->IsOpen())
				continue;
			for (int k = 0, iEdgeCount(pPolygon->GetEdgeCount()); k < iEdgeCount; ++k)
			{
				BrushEdge3D edge = pPolygon->GetEdge(k);
				if (HitTestEdge(ray, pPolygon->GetPlane().Normal(), edge, hc.view))
				{
					hc.object = this;
					return true;
				}
			}
		}
	}
	return false;
}

IPhysicalEntity* DesignerObject::GetCollisionEntity() const
{
	if (GetCompiler() == NULL)
		return NULL;
	if (GetCompiler()->GetRenderNode())
		return GetCompiler()->GetRenderNode()->GetPhysics();
	return NULL;
}

void DesignerObject::OnContextMenu(CPopupMenuItem* menu)
{
	// Add button to enter edit mode
	menu->Add("Edit", [=]()
		{
			BeginUndoAndEnsureSelection();
			GetIEditor()->GetIUndoManager()->Accept("Select Object");
			GetIEditor()->ExecuteCommand("general.open_pane 'Modeling'");
	  });
	// then show the rest of object properties
	CBaseObject::OnContextMenu(menu);
}

void DesignerObject::Serialize(CObjectArchive& ar)
{
	CBaseObject::Serialize(ar);

	XmlNodeRef xmlNode = ar.node;

	if (ar.bLoading)
	{
		string typeName("Designer");
		xmlNode->getAttr("Type", typeName);

		bool bDesignerBinary = false;
		xmlNode->getAttr("DesignerBinary", bDesignerBinary);

		XmlNodeRef brushNode = xmlNode->findChild("Brush");
		if (brushNode)
		{
			bool bDesignerObject = true;
			if (!brushNode->haveAttr("DesignerModeFlags"))
				bDesignerObject = false;

			if (!GetCompiler())
				SetCompiler(new ModelCompiler(eCompiler_General));

			if (!GetModel())
				SetModel(new Model);

			if (bDesignerObject)
			{
				if (ar.bUndo || !bDesignerBinary)
					GetModel()->Serialize(brushNode, ar.bLoading, ar.bUndo);
			}
			else
			{
				bool bConvertSuccess = Converter::ConvertSolidXMLToDesignerObject(brushNode, this);
				DESIGNER_ASSERT(bConvertSuccess);
			}
		}

		if (ar.bUndo)
		{
			if (GetCompiler())
				GetCompiler()->DeleteAllRenderNodes();
		}

		if (GetCompiler())
		{
			int nRenderFlag = ERF_HAS_CASTSHADOWMAPS | ERF_CASTSHADOWMAPS;
			ar.node->getAttr("RndFlags", nRenderFlag);
			if (nRenderFlag & ERF_CASTSHADOWMAPS)
				nRenderFlag |= ERF_HAS_CASTSHADOWMAPS;
			GetCompiler()->SetRenderFlags(nRenderFlag);
			int nStatObjFlag = 0;
			if (ar.node->getAttr("StaticObjFlags", nStatObjFlag))
				GetCompiler()->SetStaticObjFlags(nStatObjFlag);
			int nViewDistRadio = kDefaultViewDist;
			ar.node->getAttr("ViewDistRatio", nViewDistRadio);
			GetCompiler()->SetViewDistRatio(nViewDistRadio);

			XmlNodeRef meshNode = ar.node->findChild("Mesh");
			if (meshNode)
			{
				const char* encodedStr;
				int nVersion = 0;
				meshNode->getAttr("Version", nVersion);
				if (meshNode->getAttr("BinaryData", &encodedStr))
				{
					int nLength = strlen(encodedStr);
					if (nLength > 0)
					{
						int nDestBufferLen = Base64::decodedsize_base64(nLength);
						std::vector<char> buffer;
						buffer.resize(nDestBufferLen, 0);
						Base64::decode_base64(&buffer[0], encodedStr, nLength, false);
						GetCompiler()->LoadMesh(nVersion, buffer, this, GetModel());
						bDesignerBinary = true;
					}
				}
			}

			m_EngineFlags.Set();
		}

		if (ar.bUndo || !bDesignerBinary)
		{
			if (GetModel()->GetSubdivisionLevel() > 0)
				GetModel()->ResetDB(eDBRF_Vertex);
			ApplyPostProcess(GetMainContext(), ePostProcess_Mesh);
		}
	}
	else
	{
		if (GetModel())
		{
			XmlNodeRef brushNode = xmlNode->newChild("Brush");
			GetModel()->Serialize(brushNode, ar.bLoading, ar.bUndo);
		}
		if (m_pCompiler)
		{
			ar.node->setAttr("RndFlags", ERF_GET_WRITABLE(GetCompiler()->GetRenderFlags()));
			ar.node->setAttr("StaticObjFlags", ERF_GET_WRITABLE(GetCompiler()->GetStaticObjFlags()));
			ar.node->setAttr("ViewDistRatio", GetCompiler()->GetViewDistRatio());
			if (!GetCompiler()->IsValid())
				GetCompiler()->Compile(this, GetModel());

			if (GetCompiler()->IsValid() && !ar.bUndo)
			{
				XmlNodeRef meshNode = xmlNode->newChild("Mesh");
				std::vector<char> meshBuffer;
				const int nMeshVersion = 2;
				if (GetCompiler()->SaveMesh(nMeshVersion, meshBuffer, this, GetModel()))
				{
					unsigned int meshBufferSize = meshBuffer.size();
					std::vector<char> encodedStr(Base64::encodedsize_base64(meshBufferSize) + 1);
					int nReturnedSize = Base64::encode_base64(&encodedStr[0], &meshBuffer[0], meshBufferSize, true);
					meshNode->setAttr("Version", nMeshVersion);
					meshNode->setAttr("BinaryData", &encodedStr[0]);
				}
			}
		}
	}
}

// Get a list of incompatible subtools with current Designer Object type
std::vector<EDesignerTool> DesignerObject::GetIncompatibleSubtools()
{
	return std::vector<EDesignerTool>();
}

void DesignerObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<DesignerObject>("Designer", [](DesignerObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		pObject->m_EngineFlags.Serialize(ar);

		if (ar.openBlock("Edit", "<Edit"))
		{
			ar(Serialization::ActionButton([=]
			{
				GetIEditor()->ExecuteCommand("general.open_pane 'Modeling'");
			})
				, "edit", "^Edit");
			ar.closeBlock();
		}

		if (ar.isInput())
		{
			pObject->UpdateGameResource();
		}
	});
}

void DesignerObject::SetMaterial(IEditorMaterial* mtl)
{
	bool bAssignedSubmaterial = false;

	if (mtl == NULL)
		mtl = GetIEditor()->GetMaterialManager()->LoadMaterial(BrushDesignerDefaultMaterial);

	CMaterial* pParentMat = mtl ? ((CMaterial*)mtl)->GetParent() : NULL;
	if (pParentMat && pParentMat == GetMaterial())
	{
		DesignerSession* pSession = DesignerSession::GetInstance();
		Model* pModel = GetModel();

		for (int i = 0, iSubMatCount(pParentMat->GetSubMaterialCount()); i < iSubMatCount; ++i)
		{
			if (pParentMat->GetSubMaterial(i) != mtl)
				continue;

			if (pSession->GetIsActive())
			{
				CBaseObject* pObject = pSession->GetBaseObject();

				if (CUndo::IsRecording() && pObject)
					CUndo::Record(new UndoTextureMapping(pObject, "Designer : SetSubMatID"));

				ElementSet* pSelected = pSession->GetSelectedElements();
				if (pSelected->IsEmpty())
					pModel->SetSubMatID(i);
				else
					AssignMatIDToSelectedPolygons(i);

				pSession->SetCurrentSubMatID(i);
			}
			else
				pModel->SetSubMatID(i);

			GetCompiler()->Compile(this, GetModel());
			bAssignedSubmaterial = true;
			break;
		}
	}

	if (!bAssignedSubmaterial)
	{
		if (!pParentMat)
		{
			CBaseObject::SetMaterial(mtl);
		}
		else if (pParentMat != GetMaterial())
		{
			CBaseObject::SetMaterial(pParentMat);
			SetMaterial(mtl);
		}
	}

	UpdateEngineNode();
}

void DesignerObject::SetMaterial(const string& materialName)
{
	CMaterial* pMaterial = NULL;
	pMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial(materialName);
	if (!pMaterial || (pMaterial && pMaterial->IsDummy()))
	{
		IMaterial* pMatInfo = GetIEditor()->Get3DEngine()->GetMaterialManager()->LoadMaterial(materialName, false);
		if (pMatInfo)
		{
			GetIEditor()->GetMaterialManager()->OnCreateMaterial(pMatInfo);
			pMaterial = GetIEditor()->GetMaterialManager()->FromIMaterial(pMatInfo);
		}
	}
	CBaseObject::SetMaterial(pMaterial);

	const string materialNameWithoutExtension = PathUtil::RemoveExtension(materialName);
	if (pMaterial->GetName() != materialNameWithoutExtension)
		m_materialNameOverride = materialNameWithoutExtension;
	else
		m_materialNameOverride.Empty();

	UpdateEngineNode();
}

string DesignerObject::GetMaterialName() const
{
	if (!m_materialNameOverride.IsEmpty())
		return m_materialNameOverride;

	return CBaseObject::GetMaterialName();
}

void DesignerObject::SetMaterialLayersMask(uint32 nLayersMask)
{
	CBaseObject::SetMaterialLayersMask(nLayersMask);
	UpdateEngineNode();
}

void DesignerObject::SetMinSpec(uint32 nSpec, bool bSetChildren)
{
	__super::SetMinSpec(nSpec, bSetChildren);
	if (GetCompiler() && GetCompiler()->GetRenderNode())
		GetCompiler()->GetRenderNode()->SetMinSpec(GetMinSpec());
}

bool DesignerObject::IsSimilarObject(CBaseObject* pObject) const
{
	if (pObject->GetClassDesc() == GetClassDesc() && GetRuntimeClass() == pObject->GetRuntimeClass())
		return true;
	return false;
}

void DesignerObject::OnEvent(ObjectEvent event)
{
	CBaseObject::OnEvent(event);
	switch (event)
	{
	case EVENT_INGAME:
		{
			if (CheckFlags(OBJFLAG_SUBOBJ_EDITING))
				EndSubObjectSelection();
			DesignerEditor* pDesignerTool = GetDesigner();
			if (pDesignerTool && !GetIEditor()->GetGameEngine()->GetSimulationMode())
				pDesignerTool->LeaveCurrentTool();
		}
		break;
	case EVENT_OUTOFGAME:
		{
			DesignerEditor* pDesignerTool = GetDesigner();
			if (pDesignerTool)
				pDesignerTool->EnterCurrentTool();
		}
		break;
	case EVENT_HIDE_HELPER:
		if (IsSelected() && GetCompiler() && IsVisible())
		{
			_smart_ptr<IStatObj> obj = NULL;
			if (GetCompiler()->GetIStatObj(&obj))
			{
				int flag = obj->GetFlags();
				flag &= ~STATIC_OBJECT_HIDDEN;
				obj->SetFlags(flag);
			}
		}
		break;
	}
}

void DesignerObject::WorldToLocalRay(Vec3& raySrc, Vec3& rayDir) const
{
	raySrc = m_invertTM.TransformPoint(raySrc);
	rayDir = m_invertTM.TransformVector(rayDir).GetNormalized();
}

void DesignerObject::InvalidateTM(int nWhyFlags)
{
	CBaseObject::InvalidateTM(nWhyFlags);
	UpdateEngineNode();
	m_invertTM = GetWorldTM();
	m_invertTM.Invert();
}

void DesignerObject::GenerateGameFilename(string& generatedFileName) const
{
	generatedFileName.Format("%%level%%/Brush/designer_%d.%s", m_nBrushUniqFileId, CRY_GEOMETRY_FILE_EXT);
}

void DesignerObject::DrawDimensions(DisplayContext& dc, AABB* pMergedBoundBox)
{
	if (!HasMeasurementAxis() || GetDesigner() || !gDesignerSettings.bDisplayDimensionHelper)
		return;
	AABB localBoundBox;
	GetLocalBounds(localBoundBox);
	DrawDimensionsImpl(dc, localBoundBox, pMergedBoundBox);
}

IRenderNode* DesignerObject::GetEngineNode() const
{
	if (!GetCompiler() || !GetCompiler()->GetRenderNode())
		return NULL;
	return GetCompiler()->GetRenderNode();
}

DesignerObjectFlags::DesignerObjectFlags() : m_pObj(NULL)
{
	outdoor = false;
	castShadows = true;
	giMode = true;
	supportSecVisArea = false;
	rainOccluder = false;
	hideable = false;
	ratioViewDist = 100;
	excludeFromTriangulation = false;
	noDynWater = false;
	noStaticDecals = false;
	excludeCollision = false;
	occluder = false;
}

void DesignerObjectFlags::Set()
{
	ratioViewDist = m_pObj->GetCompiler()->GetViewDistRatio();
	int flags = m_pObj->GetCompiler()->GetRenderFlags();
	int statobjFlags = m_pObj->GetCompiler()->GetStaticObjFlags();
	outdoor = (flags & ERF_OUTDOORONLY) != 0;
	rainOccluder = (flags & ERF_RAIN_OCCLUDER) != 0;
	castShadows = (flags & ERF_CASTSHADOWMAPS) != 0;
	giMode = (flags & ERF_GI_MODE_BIT0) != 0;
	supportSecVisArea = (flags & ERF_REGISTER_BY_BBOX) != 0;
	occluder = (flags & ERF_GOOD_OCCLUDER) != 0;
	hideable = (flags & ERF_HIDABLE) != 0;
	noDynWater = (flags & ERF_NODYNWATER) != 0;
	noStaticDecals = (flags & ERF_NO_DECALNODE_DECALS) != 0;
	excludeFromTriangulation = (flags & ERF_EXCLUDE_FROM_TRIANGULATION) != 0;
	excludeCollision = (statobjFlags & STATIC_OBJECT_NO_PLAYER_COLLIDE) != 0;
}

void DesignerObjectFlags::Serialize(Serialization::IArchive& ar)
{
	ar(castShadows, "castShadows", "Cast Shadows");
	ar(giMode, "globalIllumination", "Global Illumination");
	ar(supportSecVisArea, "supportSecVisArea", "Support Second Visarea");
	ar(outdoor, "outdoor", "Outdoor");
	ar(rainOccluder, "rainOccluder", "Rain Occluder");
	ar(ratioViewDist, "ratioViewDist", "View Distance Ratio");
	ar(excludeFromTriangulation, "excludeFromTriangulation", "AI Exclude From Triangulation");
	ar(hideable, "hideable", "AI Hideable");
	ar(noDynWater, "noDynWater", "No Dynamic Water");
	ar(noStaticDecals, "noStaticDecals", "No Static Decal");
	ar(excludeCollision, "excludeCollision", "Exclude Collision");
	ar(occluder, "occluder", "Occluder");
	if (ar.isInput())
		Update();
}

namespace
{
void ModifyFlag(int& nFlags, int flag, int clearFlag, bool var)
{
	nFlags = (var) ? (nFlags | flag) : (nFlags & (~clearFlag));
}
void ModifyFlag(int& nFlags, int flag, bool var)
{
	ModifyFlag(nFlags, flag, flag, var);
}
}

void DesignerObjectFlags::Update()
{
	int nFlags = m_pObj->GetCompiler()->GetRenderFlags();
	int statobjFlags = m_pObj->GetCompiler()->GetStaticObjFlags();

	ModifyFlag(nFlags, ERF_OUTDOORONLY, outdoor);
	ModifyFlag(nFlags, ERF_RAIN_OCCLUDER, rainOccluder);
	ModifyFlag(nFlags, ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS, ERF_CASTSHADOWMAPS, castShadows);
	ModifyFlag(nFlags, ERF_GI_MODE_BIT0, giMode);
	ModifyFlag(nFlags, ERF_REGISTER_BY_BBOX, supportSecVisArea);
	ModifyFlag(nFlags, ERF_HIDABLE, hideable);
	ModifyFlag(nFlags, ERF_EXCLUDE_FROM_TRIANGULATION, excludeFromTriangulation);
	ModifyFlag(nFlags, ERF_NODYNWATER, noDynWater);
	ModifyFlag(nFlags, ERF_NO_DECALNODE_DECALS, noStaticDecals);
	ModifyFlag(nFlags, ERF_GOOD_OCCLUDER, occluder);
	ModifyFlag(statobjFlags, STATIC_OBJECT_NO_PLAYER_COLLIDE, excludeCollision);

	m_pObj->GetCompiler()->SetViewDistRatio(ratioViewDist);
	m_pObj->GetCompiler()->SetRenderFlags(nFlags);
	m_pObj->GetCompiler()->SetStaticObjFlags(statobjFlags);
	m_pObj->GetCompiler()->Compile(m_pObj, m_pObj->GetModel(), eShelf_Base, true);
	ApplyPostProcess(m_pObj->GetMainContext(), ePostProcess_SyncPrefab);
}

}

