// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action to deploy a rope

-------------------------------------------------------------------------
History:
- 30:11:2006: Created by Mathieu Pinard
- 22:04:2010: Re-factored and re-worked by Paul Slinger

*************************************************************************/

#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleActionDeployRope.h"
#include "Game.h"

#include <IActorSystem.h>
//#include <IAnimatedCharacter.h>

//------------------------------------------------------------------------
typedef std::vector <Vec3> TVec3Vector;

//------------------------------------------------------------------------
DEFINE_VEHICLEOBJECT(CVehicleActionDeployRope);

//------------------------------------------------------------------------
const float	CVehicleActionDeployRope::ms_extraRopeLength = 10.0f;

//------------------------------------------------------------------------
CVehicleActionDeployRope::CVehicleActionDeployRope() : m_pVehicle(NULL),
																											 m_pSeat(NULL),
																											 m_pRopeHelper(NULL),
																											 m_pDeployAnim(NULL),
																											 m_deployAnimOpenedId(InvalidVehicleAnimStateId), m_deployAnimClosedId(InvalidVehicleAnimStateId),
																											 m_actorId(0), m_upperRopeId(0), m_lowerRopeId(0),
																											 m_altitude(0.0f)
{
}

//------------------------------------------------------------------------
CVehicleActionDeployRope::~CVehicleActionDeployRope()
{
	Reset();
	m_pVehicle->UnregisterVehicleEventListener(this);
}

//------------------------------------------------------------------------
bool CVehicleActionDeployRope::Init(IVehicle *pVehicle, IVehicleSeat* pSeat, const CVehicleParams &table)
{
	if(!pVehicle)
	{
		return false;
	}

	m_pVehicle	= pVehicle;
	m_pSeat			= pSeat;

	m_pVehicle->RegisterVehicleEventListener(this, "SeatActionDeployRope");

	CVehicleParams	deployRopeTable = table.findChild("DeployRope");
	
	if(!deployRopeTable)
	{
		return false;
	}

	if(deployRopeTable.haveAttr("helper"))
	{
		m_pRopeHelper = m_pVehicle->GetHelper(deployRopeTable.getAttr("helper"));
	}

	if(deployRopeTable.haveAttr("animation"))
	{
		if(m_pDeployAnim = m_pVehicle->GetAnimation(deployRopeTable.getAttr("animation")))
		{
			m_deployAnimOpenedId	= m_pDeployAnim->GetStateId("opened");
			m_deployAnimClosedId	= m_pDeployAnim->GetStateId("closed");

			if(m_deployAnimOpenedId == InvalidVehicleAnimStateId || m_deployAnimClosedId == InvalidVehicleAnimStateId)
			{
				m_pDeployAnim = NULL;
			}
		}
	}

	return m_pRopeHelper != NULL;
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::Reset()
{
	if(m_upperRopeId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_upperRopeId);

		m_upperRopeId = 0;
	}

	if(m_lowerRopeId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_lowerRopeId);

		m_lowerRopeId = 0;
	}

	m_actorId		= 0;
	m_altitude	= 0.0f;
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::StartUsing(EntityId passengerId)
{
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::StopUsing()
{
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
 	if(actionId == eVAI_Attack1 && activationMode == eAAM_OnPress)
	{
		if(m_pDeployAnim)
		{
			m_pDeployAnim->ChangeState(m_deployAnimOpenedId);
		}
		
		DeployRope();
	}
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams &params)
{
	if(event == eVE_PassengerExit && params.iParam == m_pSeat->GetSeatId())
	{
    IActor *actor = gEnv->pGameFramework->GetIActorSystem()->GetActor(params.entityId);
    
    if(actor && !actor->IsDead() && !m_pVehicle->IsDestroyed() && !gEnv->pSystem->IsSerializingFile())
		{
			m_actorId = params.entityId;

			DeployRope();

			//AttachActorToRope(m_actorId, m_upperRopeId);
		}
	} 
	else if(event == eVE_Destroyed)
	{
		Reset();
	}
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::Serialize(TSerialize serialize, EEntityAspects aspects)
{
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::PostSerialize()
{
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::Update(const float deltaTime)
{
	CRY_ASSERT(gEnv->pGameFramework->GetIActorSystem());

	IActor					*pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_actorId);

	IRopeRenderNode	*pUpperRope = GetRopeRenderNode(m_upperRopeId);

	IRopeRenderNode *pLowerRope = GetRopeRenderNode(m_lowerRopeId);

	if(!m_pRopeHelper || !pActor || !pUpperRope || !pLowerRope)
	{
		return;
	}

	Vec3	ropePoints[2];
	
	ropePoints[0] = m_pRopeHelper->GetWorldSpaceTranslation();
	ropePoints[1]	= pActor->GetEntity()->GetWorldTM().GetTranslation();

/*
	ICharacterInstance	*pCharacterInstance = pActor->GetEntity()->GetCharacter(0);

	if(pCharacterInstance)
	{
		ISkeletonPose	*pSkeletonPose = pCharacterInstance->GetISkeletonPose();

		if(pSkeletonPose)
		{
			int16	jointId = pSkeletonPose->GetJointIDByName("Bip01 L Hand");

			if(jointId >= 0)
			{
				ropePoints[1] = pSkeletonPose->GetAbsJointByID(jointId).t;
			}
		}
	}
*/

	pUpperRope->SetPoints(ropePoints, 2);

	pLowerRope->SetPoints(ropePoints + 1, 1);
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->Add(*this);
}

//------------------------------------------------------------------------
bool CVehicleActionDeployRope::DeployRope()
{
	CRY_ASSERT(gEnv->pGameFramework->GetIActorSystem());

	IActor	*pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_actorId);

	if(!m_pRopeHelper || !pActor)
	{
		return false;
	}

	m_altitude = m_pVehicle->GetAltitude();

	Vec3	ropePoints[3];

	ropePoints[0] = m_pRopeHelper->GetWorldSpaceTranslation();
	ropePoints[1]	= pActor->GetEntity()->GetWorldTM().GetTranslation();
	ropePoints[2]	= Vec3(ropePoints[1].x, ropePoints[1].y, ropePoints[1].z - (m_altitude + ms_extraRopeLength));

	//m_upperRopeId	= CreateRope(m_pVehicle->GetEntity()->GetPhysics(), ropePoints[0], ropePoints[1]);
	//m_lowerRopeId	= CreateRope(pActor->GetEntity()->GetPhysics(), ropePoints[1], ropePoints[2]);

	m_lowerRopeId = CreateRope(m_pVehicle->GetEntity()->GetPhysics(), ropePoints[0], ropePoints[2]);

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	return true;
}

//------------------------------------------------------------------------
EntityId CVehicleActionDeployRope::CreateRope(IPhysicalEntity *pLinkedEntity, const Vec3 &highPos, const Vec3 &lowPos)
{
	CRY_ASSERT(gEnv->pEntitySystem);

	char	ropeName[256];

	cry_sprintf(ropeName, "%s_rope_%d", m_pVehicle->GetEntity()->GetName(), m_pSeat->GetSeatId());

	SEntitySpawnParams	params;

	params.sName	= ropeName;
	params.nFlags	= ENTITY_FLAG_CLIENT_ONLY;
	params.pClass	= gEnv->pEntitySystem->GetClassRegistry()->FindClass("RopeEntity");

	IEntity	*pRopeEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
	 
	if(!pRopeEntity)
	{
		return 0;
	}

	pRopeEntity->SetFlags(pRopeEntity->GetFlags() | ENTITY_FLAG_CASTSHADOW);

	IEntityRopeComponent* pEntityRopeProxy = pRopeEntity->GetOrCreateComponent<IEntityRopeComponent>();

	if(!pEntityRopeProxy)
	{
		return 0;
	}

	IRopeRenderNode	*pRopeNode = pEntityRopeProxy->GetRopeRenderNode();

	if(!pRopeNode)
	{
		return 0;
	}

	Vec3	ropePoints[2];

	ropePoints[0] = highPos;
	ropePoints[1] = lowPos;

	IRopeRenderNode::SRopeParams m_ropeParams;

	m_ropeParams.nFlags						= IRopeRenderNode::eRope_CheckCollisinos | IRopeRenderNode::eRope_Smooth;
	m_ropeParams.fThickness				= 0.05f;
	m_ropeParams.fAnchorRadius		= 0.1f;
	m_ropeParams.nNumSegments			= 12;
	m_ropeParams.nNumSides				= 4;
	m_ropeParams.nMaxSubVtx				= 3;
	m_ropeParams.nPhysSegments		= 12;
	m_ropeParams.mass							= 1.0f;
	m_ropeParams.friction					= 2;
	m_ropeParams.frictionPull			= 2;
	m_ropeParams.wind							= Vec3(0.0f, 0.0f, 0.0f);
	m_ropeParams.windVariance			= 0;
	m_ropeParams.waterResistance	= 0;
	m_ropeParams.jointLimit				= 0;
	m_ropeParams.maxForce					= 0;
	m_ropeParams.airResistance		= 0;
	m_ropeParams.fTextureTileU		= 1.0f;
	m_ropeParams.fTextureTileV		= 10.0f;
	m_ropeParams.tension					= 0.5f;

	pRopeNode->SetParams(m_ropeParams);

	pRopeNode->SetPoints(ropePoints, 2);

	//pRopeNode->SetEntityOwner(m_pVehicle->GetEntity()->GetId());

	if(pLinkedEntity)
	{
		pRopeNode->LinkEndEntities(pLinkedEntity, NULL);
	}

	return pRopeEntity->GetId();
}

//------------------------------------------------------------------------
IRopeRenderNode *CVehicleActionDeployRope::GetRopeRenderNode(EntityId ropeId)
{
	CRY_ASSERT(gEnv->pEntitySystem);

	IEntity	*pEntity = gEnv->pEntitySystem->GetEntity(ropeId);

	if(!pEntity)
	{
		return NULL;
	}

	IEntityRopeComponent	*pEntityProxy = (IEntityRopeComponent *)pEntity->GetProxy(ENTITY_PROXY_ROPE);

	if(!pEntityProxy)
	{
		return NULL;
	}

	return pEntityProxy->GetRopeRenderNode();
}

//------------------------------------------------------------------------
bool CVehicleActionDeployRope::AttachActorToRope(EntityId actorId, EntityId ropeId)
{
	CRY_ASSERT(gEnv->pGameFramework->GetIActorSystem());

	IActor	*pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(actorId);

	if(!pActor)
	{
		return false;
	}

	IEntity	*pEntity = pActor->GetEntity();

	if(!pEntity)
	{
		return false;
	}

	IRopeRenderNode	*pRope = GetRopeRenderNode(ropeId);
	
	if(!pRope)
	{
		return false;
	}

	IPhysicalEntity	*pRopePhysics = pRope->GetPhysics();

	if(!pRopePhysics)
	{
		return false;
	}

	pe_status_rope	ropeStatus;

	int							pointCount;
	
	if(pRopePhysics->GetStatus(&ropeStatus))
	{
		pointCount = ropeStatus.nSegments + 1;
	}
	else
	{
		pointCount = 0;
	}

	if(pointCount < 2)
	{
		return false;
	}

	TVec3Vector	points;

	points.resize(pointCount);

	ropeStatus.pPoints = &points[0];

	if(pRopePhysics->GetStatus(&ropeStatus))
	{
		Matrix34	worldTM;

		worldTM.SetIdentity();

		worldTM = Matrix33(m_pVehicle->GetEntity()->GetWorldTM());

		worldTM.SetTranslation(ropeStatus.pPoints[1]);

		pEntity->SetWorldTM(worldTM);
	}

	pRope->LinkEndEntities(m_pVehicle->GetEntity()->GetPhysics(), pEntity->GetPhysics());

	return true;
}