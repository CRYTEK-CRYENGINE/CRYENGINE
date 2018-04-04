// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InputExtension.h"

CInputExtension::CInputExtension()
	: m_vDeltaMovement(ZERO)
	, m_rotation(ZERO)
	, m_mouseSensitivity(0.0f)
	, m_controllerSensitivity(0.0f)
	, m_bUseControllerRotation(false)
	, m_bBoost(false)
{
}

void CInputExtension::OnShutDown()
{
	GetGameObject()->ReleaseActions(this);

	if (gEnv->IsEditor())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

	gEnv->pConsole->UnregisterVariable("gamezero_mouse_sensitivity", true);
	gEnv->pConsole->UnregisterVariable("gamezero_controller_sensitivity", true);
}

void CInputExtension::PostInit(IGameObject* pGameObject)
{
	RegisterInputHandler();

	IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();
	if (pActionMapManager)
	{
		pActionMapManager->InitActionMaps("libs/config/defaultprofile.xml");
		pActionMapManager->SetDefaultActionEntity(GetEntityId());
		pActionMapManager->EnableActionMap("player", true);
		pActionMapManager->Enable(true);
	}

	pGameObject->CaptureActions(this);

	if (gEnv->IsEditor())
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this,"CInputExtension");
	}

	REGISTER_CVAR2("gamezero_mouse_sensitivity", &m_mouseSensitivity, 0.002f, VF_NULL, "Mouse Sensitivity.");
	REGISTER_CVAR2("gamezero_controller_sensitivity", &m_controllerSensitivity, 0.03f, VF_NULL, "Controller Sensitivity.");
}

void CInputExtension::RegisterInputHandler()
{
	m_playerActionHandler.AddHandler(ActionId("controller_movex"), &CInputExtension::OnActionControllerMoveX);
	m_playerActionHandler.AddHandler(ActionId("controller_movey"), &CInputExtension::OnActionControllerMoveY);
	m_playerActionHandler.AddHandler(ActionId("controller_rotateyaw"), &CInputExtension::OnActionControllerRotateYaw);
	m_playerActionHandler.AddHandler(ActionId("controller_rotatepitch"), &CInputExtension::OnActionControllerRotatePitch);

	m_playerActionHandler.AddHandler(ActionId("mouse_rotateyaw"), &CInputExtension::OnActionMouseRotateYaw);
	m_playerActionHandler.AddHandler(ActionId("mouse_rotatepitch"), &CInputExtension::OnActionMouseRotatePitch);

	m_playerActionHandler.AddHandler(ActionId("moveforward"), &CInputExtension::OnActionMoveForward);
	m_playerActionHandler.AddHandler(ActionId("moveback"), &CInputExtension::OnActionMoveBackward);
	m_playerActionHandler.AddHandler(ActionId("moveleft"), &CInputExtension::OnActionMoveLeft);
	m_playerActionHandler.AddHandler(ActionId("moveright"), &CInputExtension::OnActionMoveRight);
	m_playerActionHandler.AddHandler(ActionId("moveup"), &CInputExtension::OnActionMoveUp);
	m_playerActionHandler.AddHandler(ActionId("movedown"), &CInputExtension::OnActionMoveDown);

	m_playerActionHandler.AddHandler(ActionId("boost"), &CInputExtension::OnActionBoost);
}

void CInputExtension::OnAction(const ActionId& action, int activationMode, float value)
{
	m_playerActionHandler.Dispatch(this, GetEntityId(), action, activationMode, value);
}

bool CInputExtension::OnActionControllerMoveX(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_vDeltaMovement.x = value;
	return false;
}

bool CInputExtension::OnActionControllerMoveY(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_vDeltaMovement.y = value;
	return false;
}

bool CInputExtension::OnActionControllerRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_rotation.z = (-value * m_controllerSensitivity);
	m_bUseControllerRotation = true;
	return false;
}

bool CInputExtension::OnActionControllerRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_rotation.x = (value * m_controllerSensitivity);
	m_bUseControllerRotation = true;
	return false;
}

bool CInputExtension::OnActionMoveForward(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
		m_vDeltaMovement.y = value;
	else if (activationMode == eAAM_OnRelease && m_vDeltaMovement.y > 0.0f)
		m_vDeltaMovement.y = 0.0f;

	return false;
}

bool CInputExtension::OnActionMoveBackward(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
		m_vDeltaMovement.y = -value;
	else if (activationMode == eAAM_OnRelease && m_vDeltaMovement.y < 0.0f)
		m_vDeltaMovement.y = 0.0f;

	return false;
}

bool CInputExtension::OnActionMoveLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
		m_vDeltaMovement.x = -value;
	else if (activationMode == eAAM_OnRelease && m_vDeltaMovement.x < 0.0f)
		m_vDeltaMovement.x = 0.0f;

	return false;
}

bool CInputExtension::OnActionMoveRight(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
		m_vDeltaMovement.x = value;
	else if (activationMode == eAAM_OnRelease && m_vDeltaMovement.x > 0.0f)
		m_vDeltaMovement.x = 0.0f;

	return false;
}

bool CInputExtension::OnActionMouseRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (m_bUseControllerRotation)
	{
		m_rotation = GetEntity()->GetWorldAngles();
	}

	m_rotation.z -= (value * m_mouseSensitivity);
	m_bUseControllerRotation = false;
	return false;
}

bool CInputExtension::OnActionMouseRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (m_bUseControllerRotation)
	{
		m_rotation = GetEntity()->GetWorldAngles();
	}

	const float fRotation = m_rotation.x - (value * m_mouseSensitivity);
	m_rotation.x = clamp_tpl(fRotation, -static_cast<float>(g_PI) * 0.5f, static_cast<float>(g_PI) * 0.5f);
	m_bUseControllerRotation = false;
	return false;
}

bool CInputExtension::OnActionMoveUp(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
		m_vDeltaMovement.z = value;
	else if (activationMode == eAAM_OnRelease && m_vDeltaMovement.z > 0.0f)
		m_vDeltaMovement.z = 0.0f;

	return false;
}

bool CInputExtension::OnActionMoveDown(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
		m_vDeltaMovement.z = -value;
	else if (activationMode == eAAM_OnRelease && m_vDeltaMovement.z < 0.0f)
		m_vDeltaMovement.z = 0.0f;

	return false;
}

bool CInputExtension::OnActionBoost(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	m_bBoost = (activationMode == eAAM_OnPress);
	return false;
}

void CInputExtension::FullSerialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() == eST_Script)
	{
		if (m_bUseControllerRotation)
		{
			Ang3 worldAngles = GetEntity()->GetWorldAngles();
			worldAngles += m_rotation;
			worldAngles.y = 0.0f;
			worldAngles.x = clamp_tpl(worldAngles.x, -static_cast<float>(g_PI) * 0.5f, static_cast<float>(g_PI) * 0.5f);
			ser.Value("rotation", worldAngles);
		}
		else
		{
			ser.Value("rotation", m_rotation);
		}

		ser.Value("deltaMovement", m_vDeltaMovement);
		ser.Value("boost", m_bBoost);
	}
}

void CInputExtension::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
		{
			m_rotation = GetEntity()->GetWorldAngles();
			m_bUseControllerRotation = false;
		}
		break;
	}
}
