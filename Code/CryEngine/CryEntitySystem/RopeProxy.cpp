// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "RopeProxy.h"
#include "Entity.h"
#include "EntitySlot.h"
#include "EntitySystem.h"
#include <CryNetwork/ISerialize.h>

#include  <CrySchematyc/Env/IEnvRegistrar.h>
#include  <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>

CRYREGISTER_CLASS(CEntityComponentRope);

//////////////////////////////////////////////////////////////////////////
CEntityComponentRope::CEntityComponentRope()
	: m_pRopeRenderNode(nullptr)
	, m_nSegmentsOrg(0)
	, m_texTileVOrg(0.0f)
{
	m_componentFlags.Add(EEntityComponentFlags::NoSave);
}

//////////////////////////////////////////////////////////////////////////
CEntityComponentRope::~CEntityComponentRope()
{
	// Delete physical entity from physical world.
	m_pRopeRenderNode = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentRope::Initialize()
{
	m_pRopeRenderNode = (IRopeRenderNode*)gEnv->p3DEngine->CreateRenderNode(eERType_Rope);
	int nSlot = GetOrMakeEntitySlotId();
	GetEntity()->SetSlotRenderNode(nSlot, m_pRopeRenderNode);
	m_nSegmentsOrg = -1;
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentRope::Update(SEntityUpdateContext& ctx)
{
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentRope::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_HIDE:
		if (m_pRopeRenderNode)
		{
			if (m_pRopeRenderNode->GetPhysics())
				gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pRopeRenderNode->GetPhysics(), 1);
		}
		break;
	case ENTITY_EVENT_UNHIDE:
		if (m_pRopeRenderNode)
		{
			if (m_pRopeRenderNode->GetPhysics())
				gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pRopeRenderNode->GetPhysics(), 2);
		}
		break;
	case ENTITY_EVENT_LEVEL_LOADED:
		// Relink physics.
		if (m_pRopeRenderNode)
			m_pRopeRenderNode->LinkEndPoints();
		break;
	case ENTITY_EVENT_RESET:
		if (m_pRopeRenderNode && m_nSegmentsOrg >= 0)
		{
			IRopeRenderNode::SRopeParams params = m_pRopeRenderNode->GetParams();
			params.nNumSegments = m_nSegmentsOrg;
			params.fTextureTileV = m_texTileVOrg;
			m_pRopeRenderNode->SetParams(params);
			m_nSegmentsOrg = -1;
		}
		break;
	}
}

uint64 CEntityComponentRope::GetEventMask() const
{
	return
	  ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_UNHIDE) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_VISIBLE) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_INVISIBLE) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_DONE) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_PHYS_BREAK) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentRope::PreserveParams()
{
	if (m_pRopeRenderNode && m_nSegmentsOrg < 0)
	{
		const IRopeRenderNode::SRopeParams& params = m_pRopeRenderNode->GetParams();
		m_nSegmentsOrg = params.nNumSegments;
		m_texTileVOrg = params.fTextureTileV;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentRope::NeedGameSerialize()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentRope::GameSerialize(TSerialize ser)
{
	if (m_pRopeRenderNode && m_pRopeRenderNode->GetPhysics())
		if (ser.IsReading())
		{
			m_pRopeRenderNode->GetPhysics()->SetStateFromSnapshot(ser);
			m_pRopeRenderNode->OnPhysicsPostStep();
		}
		else
			m_pRopeRenderNode->GetPhysics()->GetStateSnapshot(ser);
	/*
	   ser.BeginGroup("PhysicsProxy");
	   if (m_pPhysicalEntity)
	   {
	   if (ser.IsReading())
	   {
	    if (CVar::pAllowInterpolation->GetIVal() && m_pInterpolator && ser.ShouldCommitValues())
	    {
	      m_pInterpolator->PreSynchronize( m_pPhysicalEntity );
	      m_pPhysicalEntity->SetStateFromSnapshot( ser, 0 );
	      m_pInterpolator->PostSynchronize( m_pPhysicalEntity );
	    }
	    else
	    {
	      m_pPhysicalEntity->SetStateFromSnapshot( ser, 0 );
	    }
	   }
	   else
	   {
	    m_pPhysicalEntity->GetStateSnapshot( ser );
	   }
	   }

	   if (ser.GetSerializationTarget()!=eST_Network) // no folieage over network for now.
	   {
	   CEntityObject *pSlot = m_pEntity->GetSlot(0);
	   if (pSlot && pSlot->pStatObj && pSlot->pFoliage)
	    pSlot->pFoliage->Serialize(ser);

	   if (pSlot && pSlot->pCharacter && !pSlot->pCharacter->GetISkeleton()->GetCharacterPhysics())
	    for(int i=0;pSlot->pCharacter->GetISkeleton()->GetCharacterPhysics(i);i++)
	      if (ser.IsReading())
	        pSlot->pCharacter->GetISkeleton()->GetCharacterPhysics(i)->SetStateFromSnapshot(ser);
	      else
	        pSlot->pCharacter->GetISkeleton()->GetCharacterPhysics(i)->GetStateSnapshot(ser);
	   }

	   ser.EndGroup();
	 */
}

//////////////////////////////////////////////////////////////////////////
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
		node->getAttr("tension", rp.tension);
		node->getAttr("friction", rp.friction);
		node->getAttr("friction_pull", rp.frictionPull);
		node->getAttr("wind", rp.wind);
		node->getAttr("wind_var", rp.windVariance);
		node->getAttr("air_resist", rp.airResistance);
		node->getAttr("water_resist", rp.waterResistance);
		node->getAttr("joint_lim", rp.jointLimit);
		node->getAttr("max_force", rp.maxForce);
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
		node->setAttr("tension", rp.tension);
		node->setAttr("friction", rp.friction);
		node->setAttr("friction_pull", rp.frictionPull);
		node->setAttr("wind", rp.wind);
		node->setAttr("wind_var", rp.windVariance);
		node->setAttr("air_resist", rp.airResistance);
		node->setAttr("water_resist", rp.waterResistance);
		node->setAttr("joint_lim", rp.jointLimit);
		node->setAttr("max_force", rp.maxForce);
		node->setAttr("max_iters", rp.nMaxIters);
		node->setAttr("max_time_step", rp.maxTimeStep);
		node->setAttr("stiffness", rp.stiffness);
		node->setAttr("hardness", rp.hardness);
		node->setAttr("damping", rp.damping);
		node->setAttr("sleepSpeed", rp.sleepSpeed);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentRope::LegacySerializeXML(XmlNodeRef& entityNode, XmlNodeRef& componentNode, bool bLoading)
{
	IRopeRenderNode::SRopeParams ropeParams = m_pRopeRenderNode->GetParams();
	if (bLoading)
	{
		uint32 nMaterialLayers = 0;
		if (entityNode->getAttr("MatLayersMask", nMaterialLayers))
		{
			m_pRopeRenderNode->SetMaterialLayers(nMaterialLayers);
		}

		XmlNodeRef ropeNode = entityNode->findChild("Rope");
		if (ropeNode)
		{
			RopeParamsToXml(ropeParams, ropeNode, bLoading);
			m_pRopeRenderNode->SetParams(ropeParams);

			XmlNodeRef pointsNode = ropeNode->findChild("Points");
			if (pointsNode)
			{
				DynArray<Vec3> points;
				points.resize(pointsNode->getChildCount());
				for (int i = 0, num = pointsNode->getChildCount(); i < num; i++)
				{
					XmlNodeRef pnt = pointsNode->getChild(i);
					Vec3 p;
					pnt->getAttr("Pos", p);
					points[i] = p;
				}
				m_pRopeRenderNode->SetMatrix(m_pEntity->GetWorldTM());
				m_pRopeRenderNode->SetPoints(&points[0], points.size());
			}

			XmlNodeRef const xmlNodeAudio = ropeNode->findChild("Audio");

			if (xmlNodeAudio)
			{
				IRopeRenderNode::SRopeAudioParams audioParams;
				char const* szTemp = nullptr;
				xmlNodeAudio->getAttr("StartTrigger", &szTemp);
				audioParams.startTrigger = CryAudio::StringToId(szTemp);

				xmlNodeAudio->getAttr("StopTrigger", &szTemp);
				audioParams.stopTrigger = CryAudio::StringToId(szTemp);

				xmlNodeAudio->getAttr("AngleParameter", &szTemp);
				audioParams.angleParameter = CryAudio::StringToId(szTemp);

				std::underlying_type<CryAudio::EOcclusionType>::type tempOcclusionType;
				xmlNodeAudio->getAttr("OcclusionType", tempOcclusionType);
				audioParams.occlusionType = static_cast<CryAudio::EOcclusionType>(tempOcclusionType);
				xmlNodeAudio->getAttr("SegmentToAttachTo", audioParams.segementToAttachTo);
				xmlNodeAudio->getAttr("Offset", audioParams.offset);
				m_pRopeRenderNode->SetAudioParams(audioParams);
			}
		}
	}
	else
	{
		// No saving.
		//XmlNodeRef ropeNode = entityNode->newChild("Rope");
		//RopeParamsToXml( ropeParams,ropeNode,bLoading );
	}
}
