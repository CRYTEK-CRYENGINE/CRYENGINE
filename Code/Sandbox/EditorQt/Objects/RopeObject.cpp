// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RopeObject.h"
#include "IEditorImpl.h"
#include "SelectionGroup.h"

#include "Material/Material.h"
#include "Material/MaterialManager.h"

#include <Objects/DisplayContext.h>
#include <Objects/InspectorWidgetCreator.h>
#include <Objects/ObjectLoader.h>

#include <Cry3DEngine/I3DEngine.h>
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ResourcesAudio.h>

REGISTER_CLASS_DESC(CRopeObjectClassDesc);

IMPLEMENT_DYNCREATE(CRopeObject, CShapeObject)

const ColorB g_lineConnectedColor(0, 255, 0);
const ColorB g_lineDisconnectedColor(255, 255, 0);

#define ROPE_PHYS_SEGMENTS_MAX 100

SERIALIZATION_ENUM_BEGIN_NESTED(CryAudio, EOcclusionType, "OcclusionType")
SERIALIZATION_ENUM(CryAudio::EOcclusionType::Ignore, "ignore_state_name", "Ignore");
SERIALIZATION_ENUM(CryAudio::EOcclusionType::Adaptive, "adaptive_state_name", "Adaptive");
SERIALIZATION_ENUM(CryAudio::EOcclusionType::Low, "low_state_name", "Low");
SERIALIZATION_ENUM(CryAudio::EOcclusionType::Medium, "medium_state_name", "Medium");
SERIALIZATION_ENUM(CryAudio::EOcclusionType::High, "high_state_name", "High");
SERIALIZATION_ENUM_END();

SERIALIZATION_ENUM_BEGIN_NESTED(IRopeRenderNode, ERopeSegAxis, "Axis")
SERIALIZATION_ENUM(IRopeRenderNode::ERopeSegAxis::eRopeSeg_Auto, "auto", "Auto (longest)");
SERIALIZATION_ENUM(IRopeRenderNode::ERopeSegAxis::eRopeSeg_X, "x", "X");
SERIALIZATION_ENUM(IRopeRenderNode::ERopeSegAxis::eRopeSeg_Y, "y", "Y");
SERIALIZATION_ENUM(IRopeRenderNode::ERopeSegAxis::eRopeSeg_Z, "z", "Z");
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
		node->getAttr("sizeChange", rp.sizeChange);
		node->getAttr("smoothIters", rp.boneSmoothIters);
		node->getAttr("segObjLen", rp.segObjLen);
		node->getAttr("segObjRot", rp.segObjRot);
		node->getAttr("segObjRot0", rp.segObjRot0);
		node->getAttr("segObjAxis", (int&)rp.segObjAxis);
		rp.segmentObj = node->getAttr("segmentObj");
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
		node->setAttr("sizeChange", rp.sizeChange);
		node->setAttr("smoothIters", rp.boneSmoothIters);
		node->setAttr("segObjLen", rp.segObjLen);
		node->setAttr("segObjRot", rp.segObjRot);
		node->setAttr("segObjRot0", rp.segObjRot0);
		node->setAttr("segObjAxis", rp.segObjAxis);
		node->setAttr("segmentObj", rp.segmentObj);
	}
}

CRopeObject::CRopeObject()
{
	mv_closed = false;
	//mv_castShadow = true;
	//mv_recvShadow = true;

	UseMaterialLayersMask(true);

	m_bPerVertexHeight = false;
	SetColor(g_lineDisconnectedColor);

	m_entityClass = "RopeEntity";

	m_ropeParams.nFlags |= IRopeRenderNode::eRope_BindEndPoints;

	memset(&m_linkedObjects, 0, sizeof(m_linkedObjects));

	m_endLinksDisplayUpdateCounter = 0;
	m_bskipInversionTools = true;
}

void CRopeObject::InitVariables()
{
	//CEntity::InitVariables();
}

bool CRopeObject::Init(CBaseObject* prev, const string& file)
{
	bool bResult = __super::Init(prev, file);
	if (bResult && prev)
	{
		m_ropeParams = ((CRopeObject*)prev)->m_ropeParams;
	}
	return bResult;
}

void CRopeObject::Done()
{
	__super::Done();
}

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

void CRopeObject::Display(CObjectRenderHelper& objRenderHelper)
{
	SDisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	bool bPrevShowIcons = dc.showIcons;
	const Matrix34& wtm = GetWorldTM();

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
			dc.showIcons = false; // Selected Ropes hide icons.

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
			SetColor(g_lineConnectedColor);
		else
			SetColor(g_lineDisconnectedColor);
	}

	__super::Display(objRenderHelper);
	dc.showIcons = bPrevShowIcons;
}

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
			points.resize(GetPointCount());
			for (int i = 0; i < GetPointCount(); i++)
			{
				points[i] = GetPoint(i);
			}
			if (!m_points[0].IsEquivalent(m_points[1]))
			{
				pRopeNode->SetMatrix(GetWorldTM());

				pRopeNode->SetName(GetName());

				IMaterial* pCurMtl = pRopeNode->GetMaterial();
				if (!m_ropeParams.segmentObj.empty() && (!pCurMtl || pCurMtl == gEnv->p3DEngine->GetMaterialManager()->GetDefaultMaterial()))
				{
					IStatObj* pSegObj = gEnv->p3DEngine->LoadStatObj(m_ropeParams.segmentObj, nullptr, nullptr, false);
					if (pSegObj && pSegObj->GetMaterial() != pCurMtl)
						SetMaterial(GetIEditorImpl()->GetMaterialManager()->FromIMaterial(pSegObj->GetMaterial()));
				}

				CMaterial* mtl = (CMaterial*)GetMaterial();
				if (mtl)
					mtl->AssignToEntity(pRopeNode);

				uint8 nMaterialLayersFlags = GetMaterialLayersMask();
				pRopeNode->SetMaterialLayers(nMaterialLayersFlags);

				pRopeNode->SetParams(m_ropeParams);

				pRopeNode->SetPoints(&points[0], points.size());

				UpdateRopeLinks();
			}
			if (m_pEntity && m_physicsState)
				m_pEntity->SetPhysicsState(m_physicsState);
		}

		UpdateAudioData();

		m_bAreaModified = false;
	}
}

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

void CRopeObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CShapeObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CRopeObject>("Rope", &CRopeObject::SerializeProperties);
}

void CRopeObject::SerializeProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	if (ar.openBlock("rope_properties", "Rope Properties"))
	{
		ar(Serialization::Range(m_ropeParams.fThickness, 0.0f, 10000.0f, 0.0f, 0.5f, 0.01f), "radius", "Radius");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_Smooth, "smooth", "Smooth");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_UseBones, "boned", "Use Bones");
		ar(Serialization::Range(m_ropeParams.nNumSegments, 1, 1000, 1, 100, 1), "num_segments", "Num Segments");
		ar(Serialization::Range(m_ropeParams.nNumSides, 2, 100, 2, 32, 1), "num_sides", "Num Sides");
		ar(Serialization::Range(m_ropeParams.sizeChange, -1.0f, 100.0f, -1.0f, 1.0f, 0.1f), "sizeChange", "Radius Change");
		ar(Serialization::Range(m_ropeParams.boneSmoothIters, 0, 10), "smooth_iters", "Bone Smooth Iters");

		ar(m_ropeParams.fTextureTileU, "tex_u_tile", "Texture U Tiling");
		ar(m_ropeParams.fTextureTileV, "tex_v_tile", "Texture V Tiling");

		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_CastShadows, "cast_shadows", "Cast Shadows");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_BindEndPoints, "bind_end_radius", "Bind End Radius");

		ar(m_ropeParams.fAnchorRadius, "bind_radius", "Bind Radius");

		if (ar.openBlock("seg_properties", "Custom Segment Mesh"))
		{
			ar(Serialization::ModelFilename(m_ropeParams.segmentObj), "segObj", "Mesh");
			ar(m_ropeParams.segObjAxis, "segObjAxis", "Mesh Axis");
			ar(Serialization::RadiansWithRangeAsDeg(m_ropeParams.segObjRot0, -180.0f, 180.0f), "segObjRot0", "Initial Rotation");
			ar(m_ropeParams.segObjLen, "segObjLen", "Repeat Length");
			ar(Serialization::RadiansWithRangeAsDeg(m_ropeParams.segObjRot, -180.0f, 180.0f), "segObjRot", "Repeat Rotation");
			SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_SegObjBends, "bends", "Bendable");
		}

		ar.closeBlock();
	}

	if (ar.openBlock("physics", "Physics Properties"))
	{
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_Subdivide, "subdivide", "Subdivide");
		ar(Serialization::Range(m_ropeParams.nMaxSubVtx, 1, 100, 1, 8, 1), "max_subd_verts", "Max Subdiv Verts");
		ar(Serialization::Range(m_ropeParams.nPhysSegments, 1, ROPE_PHYS_SEGMENTS_MAX), "phys_segments", "Physical Segments");
		ar(Serialization::Range(m_ropeParams.tension, -100.0f, 100.0f, -0.1f, 1.0f, 0.01f), "tension", "Tension");
		ar(Serialization::Range(m_ropeParams.friction, 0.0f, 1000.0f, 0.0f, 2.0f, 0.1f), "friction", "Friction");
		ar(m_ropeParams.wind, "wind", "Wind");
		ar(Serialization::Range(m_ropeParams.windVariance, 0.0f, 100.0f, 0.0f, 1.0f, 0.1f), "wind_var", "Wind Variation");
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
		ar(Serialization::Range(m_ropeParams.mass, 0.0f, 10000.0f, 0.0f, 100.0f, 0.1f), "mass", "Mass");
		ar(Serialization::Range(m_ropeParams.frictionPull, 0.0f, 1000.0f, 0.0f, 2.0f, 0.1f), "friction_pull", "Friction Pull");
		ar(m_ropeParams.maxForce, "max_force", "Max Force");
		SerializeBitflag(ar, m_ropeParams.nFlags, IRopeRenderNode::eRope_Awake, "awake", "Awake");
		ar(Serialization::Range(m_ropeParams.nMaxIters, 0, 100000, 1, 1000, 100), "solver_iter", "Solver Iterations");
		ar(Serialization::Range(m_ropeParams.maxTimeStep, 0.001f, 1.0f, 0.001f, 0.1f, 0.01f), "max_timestep", "Max Timestep");
		ar(Serialization::Range(m_ropeParams.stiffness, 0.0f, 1000.0f, 0.0f, 100.0f, 1.0f), "stiffness", "Stiffness");
		ar(Serialization::Range(m_ropeParams.hardness, 0.0f, 1000.0f, 0.0f, 100.0f, 1.0f), "hardness", "Contact Hardness");
		ar(Serialization::Range(m_ropeParams.damping, 0.0f, 100.0f, 0.0f, 10.0f, 0.1f), "damping", "Damping");
		ar(Serialization::Range(m_ropeParams.sleepSpeed, 0.0f, 100.0f, 0.0f, 0.5f, 0.01f), "sleep_speed", "Sleep Speed");

		ar.closeBlock();
	}

	if (ar.openBlock("audio_prop", "Audio Properties"))
	{
		ar(Serialization::AudioTrigger(m_ropeAudioData.startTriggerName), "start_trigger_name", "Start Trigger");
		ar(Serialization::AudioTrigger(m_ropeAudioData.stopTriggerName), "stop_trigger_name", "Stop Trigger");
		ar(Serialization::AudioParameter(m_ropeAudioData.angleParameterName), "angle_parameter_name", "Angle Parameter");
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
	case EVENT_PHYSICS_GETSTATE: case EVENT_PHYSICS_RESETSTATE: case EVENT_PHYSICS_APPLYSTATE:
		CEntityObject::OnEvent(event);
		break;
	}
}

void CRopeObject::Reload(bool bReloadScript)
{
	__super::Reload(bReloadScript);
	m_bAreaModified = true;
	UpdateGameArea();
}

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
			xmlNodeAudio->setAttr("OcclusionType", static_cast<std::underlying_type<CryAudio::EOcclusionType>::type>(m_ropeAudioData.occlusionType));
			xmlNodeAudio->setAttr("SegmentToAttachTo", m_ropeAudioData.segementToAttachTo);
			xmlNodeAudio->setAttr("Offset", m_ropeAudioData.offset);
		}
	}

	__super::Serialize(ar);
}

void CRopeObject::PostLoad(CObjectArchive& ar)
{
	__super::PostLoad(ar);
}

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
			xmlNodeAudio->setAttr("OcclusionType", static_cast<std::underlying_type<CryAudio::EOcclusionType>::type>(m_ropeAudioData.occlusionType));
			xmlNodeAudio->setAttr("SegmentToAttachTo", m_ropeAudioData.segementToAttachTo);
			xmlNodeAudio->setAttr("Offset", m_ropeAudioData.offset);
		}
	}

	return objNode;
}

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

void CRopeObject::SetSelected(bool bSelect)
{
	if (IsSelected() && !bSelect && GetPointCount() > 1)
	{
		CalcBBox();
	}
	__super::SetSelected(bSelect);
}

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
