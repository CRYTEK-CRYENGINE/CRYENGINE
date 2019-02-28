// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 26:7:2007   17:01 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptControlledPhysics.h"

namespace SCP
{
	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		const int eventID = eGFE_OnPostStep;
		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		gameObject.RegisterExtForEvents( &goExt, &eventID, 1 );
	}
}


//------------------------------------------------------------------------
CScriptControlledPhysics::CScriptControlledPhysics()
: m_moving(false)
, m_moveTarget(ZERO)
, m_lastVelocity(ZERO)
, m_speed(0.0f)
, m_maxSpeed(0.0f)
, m_acceleration(0.0f)
, m_stopTime(1.0f)
,	m_rotating(false)
, m_rotationTarget(IDENTITY)
, m_rotationSpeed(0.0f)
, m_rotationMaxSpeed(0.0f)
, m_rotationAcceleration(0.0f)
, m_rotationStopTime(0.0f)
{
}

//------------------------------------------------------------------------
CScriptControlledPhysics::~CScriptControlledPhysics()
{
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptControlledPhysics::

	SCRIPT_REG_TEMPLFUNC(Reset, "");
	SCRIPT_REG_TEMPLFUNC(GetSpeed, "");
	SCRIPT_REG_TEMPLFUNC(GetAcceleration, "");
	
	SCRIPT_REG_TEMPLFUNC(GetAngularSpeed, "");
	SCRIPT_REG_TEMPLFUNC(GetAngularAcceleration, "");

	SCRIPT_REG_TEMPLFUNC(MoveTo, "point, initialSpeed, speed, acceleration, stopTime");
	SCRIPT_REG_TEMPLFUNC(RotateTo, "dir, roll, initialSpeed, speed, acceleration, stopTime");
	SCRIPT_REG_TEMPLFUNC(RotateToAngles, "angles, initialSpeed, speed, acceleration, stopTime");

	SCRIPT_REG_TEMPLFUNC(HasArrived, "");
}

//------------------------------------------------------------------------
bool CScriptControlledPhysics::Init(IGameObject * pGameObject )
{
	CScriptableBase::Init(gEnv->pScriptSystem, gEnv->pSystem, 1);

	SetGameObject(pGameObject);
	pGameObject->EnablePhysicsEvent(true, eEPE_OnPostStepImmediate);

	RegisterGlobals();
	RegisterMethods();

	SmartScriptTable thisTable(gEnv->pScriptSystem);

	thisTable->SetValue("__this", ScriptHandle(GetEntityId()));
	thisTable->Delegate(GetMethodsTable());
	GetEntity()->GetScriptTable()->SetValue("scp", thisTable);

	return true;
}


//------------------------------------------------------------------------
void CScriptControlledPhysics::Release()
{
	delete this;
}

void CScriptControlledPhysics::FullSerialize( TSerialize ser )
{
	ser.Value("m_moving", m_moving);
	ser.Value("m_moveTarget", m_moveTarget);
	ser.Value("m_speed", m_speed);
	ser.Value("m_maxSpeed", m_maxSpeed);
	ser.Value("m_acceleration", m_acceleration);
	ser.Value("m_stopTime", m_stopTime);
	ser.Value("m_rotating", m_rotating);
	ser.Value("m_rotationTarget", m_rotationTarget);
	ser.Value("m_rotationSpeed", m_rotationSpeed);
	ser.Value("m_rotationMaxSpeed", m_rotationMaxSpeed);
	ser.Value("m_rotationAcceleration", m_rotationAcceleration);
	ser.Value("m_rotationStopTime", m_rotationStopTime);
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::PostInit( IGameObject * pGameObject )
{
	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
	{
		SCP::RegisterEvents(*this,*pGameObject);
	
		pe_params_flags fp;
		fp.flagsOR = pef_monitor_poststep;

		pPE->SetParams(&fp);
	}
}

//------------------------------------------------------------------------
bool CScriptControlledPhysics::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
	{
		SCP::RegisterEvents(*this,*pGameObject);
	}
	else
	{
		pGameObject->UnRegisterExtForEvents( this, NULL, 0 );
	}

	CRY_ASSERT_MESSAGE(false, "CScriptControlledPhysics::ReloadExtension not implemented");
	
	return false;
}

//------------------------------------------------------------------------
bool CScriptControlledPhysics::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CScriptControlledPhysics::GetEntityPoolSignature not implemented");
	
	return true;
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::HandleEvent(const SGameObjectEvent& event)
{
	switch(event.event)
	{
	case eGFE_OnPostStep:
		OnPostStep((EventPhysPostStep *)event.ptr);
		break;
	}
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::OnPostStep(EventPhysPostStep *pPostStep)
{
	pe_action_set_velocity av;

	const bool moving = m_moving;
	const bool rotating = m_rotating;

	const float deltaTime = pPostStep->dt;

	if (m_moving)
	{
		const Vec3 current = pPostStep->pos;
		const Vec3 target = m_moveTarget;
		const Vec3 delta = target - current;
		const float distanceSq = delta.len2();

		if (distanceSq <= sqr(0.025f) || (delta.dot(m_lastVelocity) < 0.0f))
		{
			m_speed = 0.0f;
			m_moving = false;
			m_lastVelocity.zero();

			pPostStep->pos = target;

			av.v = ZERO;
		}
		else
		{
			float velocity = m_speed;
			float acceleration = m_acceleration;
			Vec3 direction = delta;
			const float distanceToEnd = direction.NormalizeSafe();

			// Accelerate
			velocity = std::min(velocity + acceleration * deltaTime, m_maxSpeed);

			// Calculate acceleration and time needed to stop
			const float accelerationNeededToStop = (-(velocity*velocity) / distanceToEnd) / 2;
			if (fabsf(accelerationNeededToStop) > 0.0f)
			{
				const float timeNeededToStop = sqrtf((distanceToEnd / 0.5f) / fabsf(accelerationNeededToStop));

				if (timeNeededToStop < m_stopTime)
				{
					acceleration = accelerationNeededToStop;
				}
			}

			// Prevent overshooting
			if ((velocity * deltaTime) > distanceToEnd)
			{
				const float multiplier = distanceToEnd / fabsf(velocity * deltaTime);
				velocity *= multiplier;
				acceleration *= multiplier;
			}

			m_acceleration = acceleration;
			m_speed = velocity;

			m_lastVelocity = direction * velocity;
			av.v = direction * velocity;
		}
	}

	if (m_rotating)
	{
		Quat current=pPostStep->q;
		Quat target=m_rotationTarget;

		Quat rotation=target*!current;
		float angle=acos_tpl(CLAMP(rotation.w, -1.0f, 1.0f))*2.0f;
		float original=angle;
		if (angle>gf_PI)
			angle=angle-gf_PI2;
		else if (angle<-gf_PI)
			angle=angle+gf_PI2;

		if (fabs_tpl(angle)<0.01f)
		{
			m_rotationSpeed=0.0f;
			m_rotating=false;
			pPostStep->q=m_rotationTarget;
			av.w=ZERO;
		}
		else
		{
			float a=m_rotationSpeed/m_rotationStopTime;
			float d=m_rotationSpeed*m_stopTime-0.5f*a*m_rotationStopTime*m_rotationStopTime;

			if (fabs_tpl(angle)<d+0.001f)
				m_rotationAcceleration=(angle-m_rotationSpeed*m_rotationStopTime)/(m_rotationStopTime*m_rotationStopTime);

			m_rotationSpeed=m_rotationSpeed+sgn(angle)*m_rotationAcceleration*deltaTime;
			if (fabs_tpl(m_rotationSpeed*deltaTime)>fabs_tpl(angle))
				m_rotationSpeed=angle/deltaTime;
			else if (fabs_tpl(m_rotationSpeed)<0.001f)
				m_rotationSpeed=sgn(m_rotationSpeed)*0.001f;
			else if (fabs_tpl(m_rotationSpeed)>=m_rotationMaxSpeed)
			{
				m_rotationSpeed=sgn(m_rotationSpeed)*m_rotationMaxSpeed;
				m_rotationAcceleration=0.0f;
			}
		}

		if(fabs_tpl(angle)>=0.001f)
			av.w=(rotation.v/sin_tpl(original*0.5f)).normalized();
		av.w*=m_rotationSpeed;
	}

	if (moving || rotating)
	{
		if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
			pPE->Action(&av, 1);
	}
/*
	if ((moving && !m_moving) || (rotating && !m_rotating))
		GetEntity()->SetWorldTM(Matrix34::Create(GetEntity()->GetScale(), pPostStep->q, pPostStep->pos));
*/
}


//------------------------------------------------------------------------
int CScriptControlledPhysics::Reset(IFunctionHandler *pH)
{
	m_moving = false;
	m_moveTarget.zero();
	m_lastVelocity.zero();
	m_speed  = 0.0f;
	m_maxSpeed = 0.0f;
	m_acceleration = 0.0f;
	m_stopTime = 1.0f;
	m_rotating = false;
	m_rotationTarget.SetIdentity();
	m_rotationSpeed = 0.0f;
	m_rotationMaxSpeed = 0.0f;
	m_rotationAcceleration = 0.0f;
	m_rotationStopTime = 0.0f;

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::GetSpeed(IFunctionHandler *pH)
{
	return pH->EndFunction(m_speed);
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::GetAcceleration(IFunctionHandler *pH)
{
	return pH->EndFunction(m_acceleration);
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::GetAngularSpeed(IFunctionHandler *pH)
{
	return pH->EndFunction(m_rotationSpeed);
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::GetAngularAcceleration(IFunctionHandler *pH)
{
	return pH->EndFunction(m_rotationAcceleration);
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::MoveTo(IFunctionHandler *pH, Vec3 point, float initialSpeed, float speed, float acceleration, float stopTime)
{
	m_moveTarget=point;
	m_lastVelocity.zero();
	m_moving=true;
	m_speed=initialSpeed;
	m_maxSpeed=speed;
	m_acceleration=acceleration;
	m_stopTime=max(0.05f, stopTime);

	pe_action_awake aa;
	aa.bAwake=1;

	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
		pPE->Action(&aa);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::RotateTo(IFunctionHandler *pH, Vec3 dir, float roll, float initialSpeed, float speed, float acceleration, float stopTime)
{
	m_rotationTarget=Quat::CreateRotationVDir(dir, roll);
	m_rotating=true;
	m_rotationSpeed=DEG2RAD(initialSpeed);
	m_rotationMaxSpeed=DEG2RAD(speed);
	m_rotationAcceleration=DEG2RAD(acceleration);
	m_rotationStopTime=stopTime;

	pe_action_awake aa;
	aa.bAwake=1;

	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
		pPE->Action(&aa);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::RotateToAngles(IFunctionHandler *pH, Vec3 angles, float initialSpeed, float speed, float acceleration, float stopTime)
{
	m_rotationTarget=Quat::CreateRotationXYZ(Ang3(angles));
	m_rotating=true;
	m_rotationSpeed=DEG2RAD(initialSpeed);
	m_rotationMaxSpeed=DEG2RAD(speed);
	m_rotationAcceleration=DEG2RAD(acceleration);
	m_rotationStopTime=stopTime;

	pe_action_awake aa;
	aa.bAwake=1;

	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
		pPE->Action(&aa);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::HasArrived(IFunctionHandler *pH)
{
	return pH->EndFunction(!m_moving);
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::GetMemoryUsage(ICrySizer *pSizer) const
{	
	pSizer->AddObject(this,sizeof(*this));
}

