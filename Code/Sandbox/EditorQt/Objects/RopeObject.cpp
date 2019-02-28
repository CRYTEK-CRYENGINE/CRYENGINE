// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "RopeObject.h"
#include <Cry3DEngine/I3DEngine.h>
#include "Material/Material.h"
#include "Controls/PropertiesPanel.h"
#include "Controls/PropertyCtrl.h"
#include <Preferences/ViewportPreferences.h>
#include "Objects/DisplayContext.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ResourcesAudio.h>

REGISTER_CLASS_DESC(CRopeObjectClassDesc);

IMPLEMENT_DYNCREATE(CRopeObject, CShapeObject)

//////////////////////////////////////////////////////////////////////////

#define LINE_CONNECTED_COLOR    RGB(0, 255, 0)
#define LINE_DISCONNECTED_COLOR RGB(255, 255, 0)
#define ROPE_PHYS_SEGMENTS_MAX  100

SERIALIZATION_ENUM_BEGIN_NESTED(CryAudio, EOcclusionType, "OcclusionType")
SERIALIZATION_ENUM(CryAudio::EOcclusionType::Ignore, "ignore_state_name", "Ignore");
SERIALIZATION_ENUM(CryAudio::EOcclusionType::Adaptive, "adaptive_state_name", "Adaptive");
SERIALIZATION_ENUM(CryAudio::EOcclusionType::Low, "low_state_name", "Low");
SERIALIZATION_ENUM(CryAudio::EOcclusionType::Medium, "medium_state_name", "Medium");
SERIALIZATION_ENUM(CryAudio::EOcclusionType::High, "high_state_name", "High");
SERIALIZATION_ENUM_END();

inline void RopeParamsToXml(IRopeRenderNode::SRopeParams& rp, XmlNodeRef& node, bool bLoad)
{
	if (bLoad)
	{
		// Load.
		node->getAttr("flags", rp.nFlags);
		node->getAttr("radius", rp.fThickness);
		node->getAttr("anchor_radius", rp.fAnchorRadius);
		node->getAttr("num_seg", rp.nNumSegments);
		node->getAttr("num_sides", rp.nNumSides);
		node->getAttr("radius", rp.fThickness);
		node->getAttr("texu", rp.fTextureTileU);
		node->getAttr("texv", rp.fTextureTileV);
		node->getAttr("ph_num_seg", rp.nPhysSegments);
		node->getAttr("ph_sub_vtxs", rp.nMaxSubVtx);
		node->getAttr("mass", rp.mass);
		node->getAttr("friction", rp.friction);
		node->getAttr("friction_pull", rp.frictionPull);
		node->getAttr("wind", rp.wind);
		node->getAttr("wind_var", rp.windVariance);
		node->getAttr("air_resist", rp.airResistance);
		node->getAttr("water_resist", rp.waterResistance);
		node->getAttr("joint_lim", rp.jointLimit);
		node->getAttr("max_force", rp.maxForce);
		node->getAttr("tension", rp.tension);
		node->getAttr("max_iters", rp.nMaxIters);
		node->getAttr("max_time_step", rp.maxTimeStep);
		node->getAttr("stiffness", rp.stiffness);
		node->getAttr("hardness", rp.hardness);
		node->getAttr("damping", rp.damping);
		node->getAttr("sleepSpeed", rp.sleepSpeed);
	}
	else
	{
		// Save.
		node->setAttr("flags", rp.nFlags);
		node->setAttr("radius", rp.fThickness);
		node->setAttr("anchor_radius", rp.fAnchorRadius);
		node->setAttr("num_seg", rp.nNumSegments);
		node->setAttr("num_sides", rp.nNumSides);
		node->setAttr("radius", rp.fThickness);
		node->setAttr("texu", rp.fTextureTileU);
		node->setAttr("texv", rp.fTextureTileV);
		node->setAttr("ph_num_seg", rp.nPhysSegments);
		node->setAttr("ph_sub_vtxs", rp.nMaxSubVtx);
		node->setAttr("mass", rp.mass);
		node->setAttr("friction", rp.friction);
		node->setAttr("friction_pull", rp.frictionPull);
		node->setAttr("wind", rp.wind);
		node->setAttr("wind_var", rp.windVariance);
		node->setAttr("air_resist", rp.airResistance);
		node->setAttr("water_resist", rp.waterResistance);
		node->setAttr("joint_lim", rp.jointLimit);
		node->setAttr("max_force", rp.maxForce);
		node->setAttr("tension", rp.tension);
		node->setAttr("max_iters", rp.nMaxIters);
		node->setAttr("max_time_step", rp.maxTimeStep);
		node->setAttr("stiffness", rp.stiffness);
		node->setAttr("hardness", rp.hardness);
		node->setAttr("damping", rp.damping);
		node->setAttr("sleepSpeed", rp.sleepSpeed);
	}
}

//////////////////////////////////////////////////////////////////////////
CRopeObject::CRopeObject()
{
	mv_closed = false;
	//mv_castShadow = true;
	//mv_recvShadow = true;

	UseMaterialLayersMask(true);

	m_bPerVertexHeight = false;
	SetColor(LINE_DISCONNECTED_COLOR);

	m_entityClass = "RopeEntity";

	ZeroStruct(m_ropeParams);

	m_ropeParams.nFlags = IRopeRenderNode::eRope_BindEndPoints | IRopeRenderNode::eRope_CheckCollisinos;
	m_ropeParams.fThickness = 0.02f;
	m_ropeParams.fAnchorRadius = 0.1f;
	m_ropeParams.nNumSegments = 8;
	m_ropeParams.nNumSides = 4;
	m_ropeParams.nMaxSubVtx = 3;
	m_ropeParams.nPhysSegments = 8;
	m_ropeParams.mass = 1.0f;
	m_ropeParams.friction = 2;
	m_ropeParams.frictionPull = 2;
	m_ropeParams.wind.Set(0, 0, 0);
	m_ropeParams.windVariance = 0;
	m_ropeParams.waterResistance = 0;
	m_ropeParams.jointLimit = 0;
	m_ropeParams.maxForce = 0;
	m_ropeParams.airResistance = 0;
	m_ropeParams.fTextureTileU = 1.0f;
	m_ropeParams.fTextureTileV = 10.0f;
	m_ropeParams.nMaxIters = 650;
	m_ropeParams.maxTimeStep = 0.02f;
	m_ropeParams.stiffness = 10.0f;
	m_ropeParams.hardness = 20.0f;
	m_ropeParams.damping = 0.2f;
	m_ropeParams.sleepSpeed = 0.04f;

	memset(&m_linkedObjects, 0, sizeof(m_linkedObjects));

	m_endLinksDisplayUpdateCounter = 0;
	m_bskipInversionTools = true;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::InitVariables()
{
	//CEntity::InitVariables();
}

///////////////////////////////////////////////////////	///////////////////
bool CRopeObject::Init(CBaseObject* prev, const string& file)
{
	bool bResult =
	  __super::Init(prev, file);
	if (bResult && prev)
	{
		m_ropeParams = ((CRopeObject*)prev)->m_ropeParams;
	}
	else
	{
	}
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::Done()
{
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CRopeObject::CreateGameObject()
{
	__super::CreateGameObject();
	if (GetIEntity())
	{
		m_bAreaModified = true;
		UpdateGameArea();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::InvalidateTM(int nWhyFlags)
{
	__super::InvalidateTM(nWhyFlags);

	m_endLinksDisplayUpdateCounter = 0;

	if (nWhyFlags & eObjectUpdateFlags_MoveTool)
	{
		GetRenderNode()->LinkEndPoints();
		UpdateRopeLinks();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::SetMaterial(IEditorMaterial* mtl)
{
	__super::SetMaterial(mtl);
	IRopeRenderNode* pRopeNode = GetRenderNode();
	if (pRopeNode != nullptr)
	{
		if (mtl)
			((CMaterial*)mtl)->AssignToEntity(pRopeNode);
		else
			pRopeNode->SetMaterial(nullptr);
		uint8 nMaterialLayersFlags = GetMaterialLayersMask();
		pRopeNode->SetMaterialLayers(nMaterialLayersFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	bool bPrevShowIcons = gViewportPreferences.showIcons;
	const Matrix34& wtm = GetWorldTM();

	bool bLineWidth = 0;

	if (m_points.size() > 1)
	{
		IRopeRenderNode* pRopeNode = GetRenderNode();
		int nLinkedMask = 0;
		if (pRopeNode)
			nLinkedMask = pRopeNode->GetLinkedEndsMask();

		m_endLinksDisplayUpdateCounter++;
		if (pRopeNode && m_endLinksDisplayUpdateCounter > 10 && gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep == 0.0f)
		{
			m_endLinksDisplayUpdateCounter = 0;

			IRopeRenderNode::SEndPointLink links[2];
			pRopeNode->GetEndPointLinks(links);

			float s = m_ropeParams.fAnchorRadius;

			bool bCalcBBox = false;
			for (int i = 0; i < 2; i++)
			{
				if (links[i].pPhysicalEntity && m_linkedObjects[i].m_nPhysEntityId == 0)
					bCalcBBox = true;
				else if (m_linkedObjects[i].m_nPhysEntityId && links[i].pPhysicalEntity)
				{
					pe_status_pos ppos;
					int nPointIndex = (i == 0) ? 0 : GetPointCount() - 1;
					IPhysicalEntity* pPhysEnt = gEnv->pPhysicalWorld->GetPhysicalEntityById(m_linkedObjects[i].m_nPhysEntityId);
					if (pPhysEnt != links[i].pPhysicalEntity)
					{
						links[i].pPhysicalEntity->GetStatus(&ppos);
						bCalcBBox = true;
					}
					else
					{
						pPhysEnt->GetStatus(&ppos);
						if (!IsEquivalent(m_linkedObjects[i].object_pos, ppos.pos) || !Quat::IsEquivalent(m_linkedObjects[i].object_q, ppos.q))
						{
							Matrix34 tm(ppos.q);
							tm.SetTranslation(ppos.pos);
							m_points[nPointIndex] = wtm.GetInverted().TransformPoint(tm.TransformPoint(m_linkedObjects[i].offset));
							bCalcBBox = true;
						}
					}
					if (bCalcBBox)
					{
						m_linkedObjects[i].object_pos = ppos.pos;
						m_linkedObjects[i].object_q = ppos.q;
					}
				}
				else
				{
					m_linkedObjects[i].m_nPhysEntityId = 0;
				}
			}
			if (bCalcBBox)
				CalcBBox();
		}

		if (IsSelected())
		{
			gViewportPreferences.showIcons = false; // Selected Ropes hide icons.

			Vec3 p0 = wtm.TransformPoint(m_points[0]);
			Vec3 p1 = wtm.TransformPoint(m_points[m_points.size() - 1]);

			float s = m_ropeParams.fAnchorRadius;

			if (nLinkedMask & 0x01)
				dc.SetColor(ColorB(0, 255, 0));
			else
				dc.SetColor(ColorB(255, 0, 0));
			dc.DrawWireBox(p0 - Vec3(s, s, s), p0 + Vec3(s, s, s));

			if (nLinkedMask & 0x02)
				dc.SetColor(ColorB(0, 255, 0));
			else
				dc.SetColor(ColorB(255, 0, 0));
			dc.DrawWireBox(p1 - Vec3(s, s, s), p1 + Vec3(s, s, s));
		}

		if ((nLinkedMask & 3) == 3)
			SetColor(LINE_CONNECTED_COLOR);
		else
			SetColor(LINE_DISCONNECTED_COLOR);
	}

	__super::Display(objRenderHelper);
	gViewportPreferences.showIcons = bPrevShowIcons;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::UpdateGameArea()
{
	if (!m_bAreaModified)
		return;

	IRopeRenderNode* pRopeNode = GetRenderNode();
	if (pRopeNode)
	{
		std::vector<Vec3> points;
		if (GetPointCount() > 1)
		{
			const Matrix34& wtm = GetWorldTM();
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = GetPoint(i);
			}
			if (!m_points[0].IsEquivalent(m_points[1]))
			{
				pRopeNode->SetMatrix(GetWorldTM());

				pRopeNode->SetName(GetName());
				CMaterial* mtl = (CMaterial*)GetMaterial();
				if (mtl)
					mtl->AssignToEntity(pRopeNode);

				uint8 nMaterialLayersFlags = GetMaterialLayersMask();
				pRopeNode->SetMaterialLayers(nMaterialLayersFlags);

				pRopeNode->SetParams(m_ropeParams);

				pRopeNode->SetPoints(&points[0], points.size());

				UpdateRopeLinks();
			}
		}

		UpdateAudioData();

		m_bAreaModified = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::UpdateRopeLinks()
{
	IRopeRenderNode::SEndPointLink links[2];
	GetRenderNode()->GetEndPointLinks(links);
	for (int i = 0; i < 2; i++)
	{
		m_linkedObjects[i].offset = links[i].offset;
		m_linkedObjects[i].m_nPhysEntityId = gEnv->pPhysicalWorld->GetPhysicalEntityId(links[i].pPhysicalEntity);
		if (links[i].pPhysicalEntity)
		{
			pe_status_pos ppos;
			links[i].pPhysicalEntity->GetStatus(&ppos);
			m_linkedObjects[i].object_pos = ppos.pos;
			m_linkedObjects[i].object_q = ppos.q;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::OnParamChange(IVariable* var)
{
	if (!m_bIgnoreGameUpdate)
	{
		m_bAreaModified = true;
		UpdateGameArea();
	}
}

static void SerializeBitflag(Serialization::IArchive& ar, int& flag, int field, const char* name, const char* label)
{
	bool isset = (flag & field) != 0;
	bool oldisset = isset;

	ar(isset, name, label);

	if (ar.isInput())
	{
		if (oldisset != isset)
		{
			if (isset)
			{
				flag |= field;
			}
			else
			{
				flag &= ~field;
			}
		}
	}

}


//////////////////////////////////////////////////////////////////////////
void CRopeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CShapeObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CRopeObject>("Rope", &CRopeObject::SerializeProperties);
}

void CRopeObject::SerializeProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	if (ar.openBlock("rope_properties", "Rope Properties"))
	{
		ar(Serialization::Range(m_ropeParams.fThickness, 0.0f, 10000.0f), "radius", "Radius");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_Smooth, "smooth", "Smooth");
		ar(Serialization::Range(m_ropeParams.nNumSegments, 2, 1000), "num_segments", "Num Segments");
		ar(Serialization::Range(m_ropeParams.nNumSides, 2, 100), "num_sides", "Num Sides");

		ar(m_ropeParams.fTextureTileU, "tex_u_tile", "Texture U Tiling");
		ar(m_ropeParams.fTextureTileV, "tex_v_tile", "Texture V Tiling");

		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_CastShadows, "cast_shadows", "Cast Shadows");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_BindEndPoints, "bind_end_radius", "Bind End Radius");

		ar(m_ropeParams.fAnchorRadius, "bind_radius", "Bind Radius");

		ar.closeBlock();
	}

	if (ar.openBlock("physics", "Physics Properties"))
	{
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_Subdivide, "subdivide", "Subdivide");
		ar(Serialization::Range(m_ropeParams.nMaxSubVtx, 1, 100), "max_subd_verts", "Max Subdiv Verts");
		ar(Serialization::Range(m_ropeParams.nPhysSegments, 1, ROPE_PHYS_SEGMENTS_MAX), "phys_segments", "Physical Segments");
		ar(m_ropeParams.tension, "tension", "Tension");
		ar(m_ropeParams.friction, "friction", "Friction");
		ar(m_ropeParams.wind, "wind", "Wind");
		ar(m_ropeParams.windVariance, "wind_var", "Wind Variation");
		ar(m_ropeParams.airResistance, "air_res", "Air Resistance");
		ar(m_ropeParams.waterResistance, "water_res", "Water Resistance");

		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_CheckCollisinos, "check_collisions", "Check Collisions");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_NoAttachmentCollisions, "no_attach_collision", "Ignore Attachment Collisions");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_NoPlayerCollisions, "no_player_collision", "Ignore Player Collisions");

		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_Nonshootable, "non_shootable", "Non-shootable");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_Disabled, "disabled", "Disabled");

		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_StaticAttachStart, "static_attach_start", "Static Attach Start");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_StaticAttachEnd, "static_attach_end", "Static Attach End");

		ar.closeBlock();
	}

	if (ar.openBlock("physics_advanced", "Advanced Physics"))
	{
		ar(m_ropeParams.mass, "mass", "Mass");
		ar(m_ropeParams.frictionPull, "friction_pull", "Friction Pull");
		ar(m_ropeParams.maxForce, "max_force", "Max Force");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_Awake, "awake", "Awake");
		ar(m_ropeParams.nMaxIters, "solver_iter", "Solver Iterations");
		ar(m_ropeParams.maxTimeStep, "max_timestep", "Max Timestep");
		ar(m_ropeParams.stiffness, "stiffness", "Stiffness");
		ar(m_ropeParams.hardness, "hardness", "Contact Hardness");
		ar(m_ropeParams.damping, "damping", "Damping");
		ar(m_ropeParams.sleepSpeed, "sleep_speed", "Sleep Speed");

		ar.closeBlock();
	}

	if (ar.openBlock("audio_prop", "Audio Properties"))
	{
		ar(Serialization::AudioTrigger(m_ropeAudioData.startTriggerName), "start_trigger_name", "Start Trigger");
		ar(Serialization::AudioTrigger(m_ropeAudioData.stopTriggerName), "stop_trigger_name", "Stop Trigger");
		ar(Serialization::AudioRTPC(m_ropeAudioData.angleParameterName), "angle_parameter_name", "Angle Parameter");
		ar(m_ropeAudioData.occlusionType, "OcclusionType", "Occlusion Type");
		ar(Serialization::Range(m_ropeAudioData.segementToAttachTo, 1, m_ropeParams.nPhysSegments), "segment", "Segment");
		ar(Serialization::Range(m_ropeAudioData.offset, 0.0f, 1.0f), "pos_offset", "Position Offset");

		if (ar.isInput())
		{
			UpdateAudioData();
		}

		ar.closeBlock();
	}

	if (ar.isInput())
	{
		m_bAreaModified = true;
		UpdateGameArea();

		if (bMultiEdit)
		{
			const CSelectionGroup* grp = GetIEditorImpl()->GetSelection();
			for (int i = 0; i < grp->GetCount(); i++)
			{
				grp->GetObject(i)->SetLayerModified();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::OnEvent(ObjectEvent event)
{
	__super::OnEvent(event);
	switch (event)
	{
	case EVENT_INGAME:
	case EVENT_OUTOFGAME:
		{
			IRopeRenderNode* const pIRopeRenderNode = GetRenderNode();

			if (pIRopeRenderNode != nullptr)
			{
				pIRopeRenderNode->ResetPoints();

				if (event == EVENT_OUTOFGAME)
				{
					pIRopeRenderNode->DisableAudio();
				}
			}
		}
		break;
	case EVENT_RELOAD_ENTITY:
		m_bAreaModified = true;
		UpdateGameArea();
		break;
	}
}

void CRopeObject::Reload(bool bReloadScript)
{
	__super::Reload(bReloadScript);
	m_bAreaModified = true;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::Serialize(CObjectArchive& ar)
{
	if (ar.bLoading)
	{
		// Loading.
		XmlNodeRef xmlNodeRope = ar.node->findChild("Rope");

		if (xmlNodeRope)
		{
			RopeParamsToXml(m_ropeParams, xmlNodeRope, ar.bLoading);

			XmlNodeRef const xmlNodeAudio = xmlNodeRope->findChild("Audio");

			if (xmlNodeAudio)
			{
				xmlNodeAudio->getAttr("StartTrigger", m_ropeAudioData.startTriggerName);
				xmlNodeAudio->getAttr("StopTrigger", m_ropeAudioData.stopTriggerName);
				xmlNodeAudio->getAttr("AngleParameter", m_ropeAudioData.angleParameterName);
				std::underlying_type<CryAudio::EOcclusionType>::type tempOcclusionType;
				xmlNodeAudio->getAttr("OcclusionType", tempOcclusionType);
				m_ropeAudioData.occlusionType = static_cast<CryAudio::EOcclusionType>(tempOcclusionType);
				xmlNodeAudio->getAttr("SegmentToAttachTo", m_ropeAudioData.segementToAttachTo);
				xmlNodeAudio->getAttr("Offset", m_ropeAudioData.offset);
			}
		}
	}
	else
	{
		// Saving.
		XmlNodeRef xmlNodeRope = ar.node->newChild("Rope");
		RopeParamsToXml(m_ropeParams, xmlNodeRope, ar.bLoading);

		if (!m_ropeAudioData.startTriggerName.IsEmpty() || !m_ropeAudioData.stopTriggerName.IsEmpty())
		{
			XmlNodeRef const xmlNodeAudio = xmlNodeRope->newChild("Audio");
			xmlNodeAudio->setAttr("StartTrigger", m_ropeAudioData.startTriggerName);
			xmlNodeAudio->setAttr("StopTrigger", m_ropeAudioData.stopTriggerName);
			xmlNodeAudio->setAttr("AngleParameter", m_ropeAudioData.angleParameterName);
			xmlNodeAudio->setAttr("OcclusionType", IntegralValue(m_ropeAudioData.occlusionType));
			xmlNodeAudio->setAttr("SegmentToAttachTo", m_ropeAudioData.segementToAttachTo);
			xmlNodeAudio->setAttr("Offset", m_ropeAudioData.offset);
		}
	}

	__super::Serialize(ar);
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::PostLoad(CObjectArchive& ar)
{
	__super::PostLoad(ar);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CRopeObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	XmlNodeRef objNode = __super::Export(levelPath, xmlNode);

	if (objNode)
	{
		// Export Rope params.
		XmlNodeRef xmlNodeRope = objNode->newChild("Rope");
		RopeParamsToXml(m_ropeParams, xmlNodeRope, false);

		// Export Points
		if (!m_points.empty())
		{
			const Matrix34& wtm = GetWorldTM();
			XmlNodeRef points = xmlNodeRope->newChild("Points");

			for (auto const& point : m_points)
			{
				XmlNodeRef pnt = points->newChild("Point");
				pnt->setAttr("Pos", point);
			}
		}

		if (!m_ropeAudioData.startTriggerName.IsEmpty() || !m_ropeAudioData.stopTriggerName.IsEmpty())
		{
			XmlNodeRef const xmlNodeAudio = xmlNodeRope->newChild("Audio");
			xmlNodeAudio->setAttr("StartTrigger", m_ropeAudioData.startTriggerName);
			xmlNodeAudio->setAttr("StopTrigger", m_ropeAudioData.stopTriggerName);
			xmlNodeAudio->setAttr("AngleParameter", m_ropeAudioData.angleParameterName);
			xmlNodeAudio->setAttr("OcclusionType", IntegralValue(m_ropeAudioData.occlusionType));
			xmlNodeAudio->setAttr("SegmentToAttachTo", m_ropeAudioData.segementToAttachTo);
			xmlNodeAudio->setAttr("Offset", m_ropeAudioData.offset);
		}
	}

	return objNode;
}

//////////////////////////////////////////////////////////////////////////
IRopeRenderNode* CRopeObject::GetRenderNode() const
{
	if (m_pEntity != nullptr)
	{
		IEntityRopeComponent* const pRopeProxy = crycomponent_cast<IEntityRopeComponent*>(m_pEntity->CreateProxy(ENTITY_PROXY_ROPE));

		if (pRopeProxy != nullptr)
		{
			return pRopeProxy->GetRopeRenderNode();
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::CalcBBox()
{
	if (m_points.size() < 2)
	{
		m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
		return;
	}

	// When UnSelecting.
	// Reposition rope, so that rope object position is in the middle of the rope.

	const Matrix34& wtm = GetWorldTM();

	Vec3 wp0 = wtm.TransformPoint(m_points[0]);
	Vec3 wp1 = wtm.TransformPoint(m_points[1]);

	Vec3 center = (wp0 + wp1) * 0.5f;
	if (!center.IsEquivalent(wtm.GetTranslation(), 0.01f))
	{
		// Center should move.
		Vec3 offset = wtm.GetInverted().TransformVector(center - wtm.GetTranslation());
		for (int i = 0; i < m_points.size(); i++)
		{
			m_points[i] -= offset;
		}
		Matrix34 ltm = GetLocalTM();
		CScopedSuspendUndo undoSuspend;
		SetPos(GetPos() + wtm.TransformVector(offset));
	}

	// First point must always be 0,0,0.
	m_bbox.Reset();

	for (auto const& point : m_points)
	{
		m_bbox.Add(point);
	}

	if (m_bbox.min.x > m_bbox.max.x)
	{
		m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
	}

	AABB box;
	box.SetTransformedAABB(GetWorldTM(), m_bbox);
	m_lowestHeight = box.min.z;
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::SetSelected(bool bSelect)
{
	if (IsSelected() && !bSelect && GetPointCount() > 1)
	{
		CalcBBox();
	}
	__super::SetSelected(bSelect);
}

//////////////////////////////////////////////////////////////////////////
void CRopeObject::UpdateAudioData()
{
	IRopeRenderNode* const pIRopeRenderNode = GetRenderNode();

	if (pIRopeRenderNode != nullptr)
	{
		IRopeRenderNode::SRopeAudioParams audioParams;
		audioParams.startTrigger = CryAudio::StringToId(m_ropeAudioData.startTriggerName.c_str());
		audioParams.stopTrigger = CryAudio::StringToId(m_ropeAudioData.stopTriggerName.c_str());
		audioParams.angleParameter = CryAudio::StringToId(m_ropeAudioData.angleParameterName.c_str());

		audioParams.occlusionType = m_ropeAudioData.occlusionType;
		audioParams.segementToAttachTo = m_ropeAudioData.segementToAttachTo;
		audioParams.offset = m_ropeAudioData.offset;

		pIRopeRenderNode->SetAudioParams(audioParams);
	}
}

