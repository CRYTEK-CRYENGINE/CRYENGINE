// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>
#include <CryRenderer/IStereoRenderer.h>
#include <CryFlowGraph/IFlowBaseNode.h>

class CVRTools : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_RecenterPose = 0
	};

	enum OUTPUTS
	{
		EOP_Done = 0,
		EOP_Triggered,
		EOP_Failed,
	};

public:
	CVRTools(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("RecentrePose", _HELP("Resets the tracking origin to the headset's current location, and sets the yaw origin to the current headset yaw value")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done",      _HELP("The selected operation has been acknoledge. This output will always get triggered.")),
			OutputPortConfig_AnyType("Triggered", _HELP("The selected operation has been triggered.")),
			OutputPortConfig_AnyType("Failed",    _HELP("The selected operation did not work as expected (e.g. the VR operation was not supported).")),
			{ 0 }
		};
		config.sDescription = _HELP("Various VR-specific utils");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_RecenterPose))
				{
					bool triggered = false;
					const IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
					IHmdDevice* pDev = pHmdManager ? pHmdManager->GetHmdDevice() : nullptr;
					if (pDev && pHmdManager->IsStereoSetupOk())
					{
						const HmdTrackingState& sensorState = pDev->GetLocalTrackingState();
						if (sensorState.CheckStatusFlags(eHmdStatus_IsUsable))
						{
							pDev->RecenterPose();
							triggered = true;
						}
					}

					ActivateOutput(pActInfo, triggered ? EOP_Triggered : EOP_Failed, true);
					ActivateOutput(pActInfo, EOP_Done, true);
				}
			}
			break;
		}
	}
};

class CVRTransformInfo : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Enabled = 0,
	};

	enum OUTPUTS
	{
		EOP_CamPos,
		EOP_CamRot,
		EOP_CamDataValid,
		EOP_HmdPos,
		EOP_HmdRot,
		EOP_HmdDataValid,
		EOP_HmdControllerPos_L,
		EOP_HmdControllerRot_L,
		EOP_HmdControllerValidData_L,
		EOP_HmdControllerPos_R,
		EOP_HmdControllerRot_R,
		EOP_HmdControllerValidData_R,
		EOP_PlayerPos,
		EOP_PlayerViewRot,
		EOP_PlayerDataValid
	};

public:
	CVRTransformInfo(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Enabled", true, _HELP("Enable / disable the node")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Camera pos",                     _HELP("The position of the current camera in world coordinates")),
			OutputPortConfig<Vec3>("Camera rot (PRY)",               _HELP("The orientation of the current camera in world coordinates (Pitch,Roll,Yaw) in Degrees")),
			OutputPortConfig<bool>("Camera valid",                   _HELP("The camera data shown is valid")),
			OutputPortConfig<Vec3>("HMD pos",                        _HELP("The position of the HMD with respect to the recentred pose of the tracker")),
			OutputPortConfig<Vec3>("HMD rot (PRY)",                  _HELP("The orientation of the HMD in world coordinates (Pitch,Roll,Yaw) in Degrees")),
			OutputPortConfig<bool>("HMD valid",                      _HELP("The HMD data shown is valid")),
			OutputPortConfig<Vec3>("HMD left controller pos",        _HELP("The position of the HMD left controller with respect to the recentred pose of the tracker")),
			OutputPortConfig<Vec3>("HMD left controller rot (PRY)",  _HELP("The orientation of the HMD left controller in world coordinates (Pitch,Roll,Yaw) in Degrees")),
			OutputPortConfig<bool>("HMD left controller ok",         _HELP("The left HMD controller is connected and the data is valid")),
			OutputPortConfig<Vec3>("HMD right controller pos",       _HELP("The position of the HMD right controller with respect to the recentred pose of the tracker")),
			OutputPortConfig<Vec3>("HMD right controller rot (PRY)", _HELP("The orientation of the HMD right controller in world coordinates (Pitch,Roll,Yaw) in Degrees")),
			OutputPortConfig<bool>("HMD right controller ok",        _HELP("The right HMD controller is connected and the data is valid")),
			OutputPortConfig<Vec3>("Player pos",                     _HELP("The position of the local player in world coordinates")),
			OutputPortConfig<Vec3>("Player view rot (PRY)",          _HELP("The orientation of the player (Pitch,Roll,Yaw) in Degrees")),
			OutputPortConfig<bool>("Player valid",                   _HELP("The HMD data shown is valid")),
			{ 0 }
		};

		config.sDescription = _HELP("This node provides information about the orientation and position of the camera, player and HMD");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				if (pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, EIP_Enabled);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
				}
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enabled) && pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, EIP_Enabled);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
				}
			}
			break;

		case eFE_Update:
			{
				// Camera info
				IRenderer* pRenderer = gEnv->pRenderer;
				if (pRenderer)
				{
					const CCamera& rCam = GetISystem()->GetViewCamera();
					const Ang3 angles = RAD2DEG(rCam.GetAngles());

					ActivateOutput(pActInfo, EOP_CamPos, rCam.GetPosition());
					ActivateOutput(pActInfo, EOP_CamRot, Vec3(angles.y, angles.z, angles.x)); // camera angles are in YPR and we need PRY
				}

				// HMD info
				bool bHmdOk = false, bHmdLeftControllerOk = false, bHmdRightControllerOk = false;
				const IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
				const IHmdDevice* pDev = pHmdManager ? pHmdManager->GetHmdDevice() : nullptr;
				if (pDev)
				{
					const HmdTrackingState& sensorState = pDev->GetLocalTrackingState();
					if (sensorState.CheckStatusFlags(eHmdStatus_IsUsable))
					{
						bHmdOk = true;
						const Ang3 hmdAngles(sensorState.pose.orientation);
						ActivateOutput(pActInfo, EOP_HmdRot, Vec3(RAD2DEG(hmdAngles)));
						ActivateOutput(pActInfo, EOP_HmdPos, sensorState.pose.position);

						if (const IHmdController* pController = pDev->GetController())
						{
							bHmdLeftControllerOk = pController->IsConnected(eHmdController_OculusLeftHand);
							bHmdRightControllerOk = pController->IsConnected(eHmdController_OculusRightHand);

							const HmdTrackingState& leftCtrlState = pController->GetLocalTrackingState(eHmdController_OculusLeftHand);
							const Ang3 hmdLeftCtrlAngles(leftCtrlState.pose.orientation);
							ActivateOutput(pActInfo, EOP_HmdControllerRot_L, Vec3(RAD2DEG(hmdLeftCtrlAngles)));
							ActivateOutput(pActInfo, EOP_HmdControllerPos_L, leftCtrlState.pose.position);

							const HmdTrackingState& rightCtrlState = pController->GetLocalTrackingState(eHmdController_OculusRightHand);
							const Ang3 hmdRightCtrlAngles(rightCtrlState.pose.orientation);
							ActivateOutput(pActInfo, EOP_HmdControllerRot_R, Vec3(RAD2DEG(hmdRightCtrlAngles)));
							ActivateOutput(pActInfo, EOP_HmdControllerPos_R, rightCtrlState.pose.position);
						}
						else
						{
							ActivateOutput(pActInfo, EOP_HmdControllerRot_L, Vec3(ZERO));
							ActivateOutput(pActInfo, EOP_HmdControllerPos_L, Vec3(ZERO));
							ActivateOutput(pActInfo, EOP_HmdControllerRot_R, Vec3(ZERO));
							ActivateOutput(pActInfo, EOP_HmdControllerPos_R, Vec3(ZERO));
						}
					}
				}

				ActivateOutput(pActInfo, EOP_CamDataValid, pRenderer != nullptr);
				ActivateOutput(pActInfo, EOP_HmdDataValid, bHmdOk);
				ActivateOutput(pActInfo, EOP_HmdControllerValidData_L, bHmdLeftControllerOk);
				ActivateOutput(pActInfo, EOP_HmdControllerValidData_R, bHmdRightControllerOk);

				// Player Info
				const IActor* pActor = gEnv->pGameFramework->GetClientActor();
				if (pActor)
				{
					Vec3 entityRotationInDegrees(RAD2DEG(Ang3(pActor->GetViewRotation())));
					ActivateOutput(pActInfo, EOP_PlayerViewRot, entityRotationInDegrees);
					ActivateOutput(pActInfo, EOP_PlayerPos, pActor->GetEntity()->GetWorldPos());
				}

				ActivateOutput(pActInfo, EOP_PlayerDataValid, pActor != nullptr);
			}
			break;
		}
	}
};

// ------------------------------------------------------------------------------------------------------------
class CVROculusController : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Enabled = 0,
	};

	enum OUTPUTS
	{
		EOP_ConnectedL,
		EOP_ConnectedR,

		EOP_ButtonA,
		EOP_ButtonB,
		EOP_ButtonX,
		EOP_ButtonY,
		EOP_ButtonLThumb,
		EOP_ButtonRThumb,

		EOP_TriggerL,
		EOP_TriggerR,
		EOP_TriggerL2,
		EOP_TriggerR2,

		EOP_ThumbStickL,
		EOP_ThumbStickR,

		EFirst_bool  = EOP_ConnectedL,
		ELast_bool   = EOP_ButtonRThumb,
		EFirst_float = EOP_TriggerL,
		ELast_float  = EOP_TriggerR2,
	};

public:
	CVROculusController(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Enabled", true, _HELP("Enable / disable the node")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("Connected_L",   _HELP("Is left hand controller connected?")),
			OutputPortConfig<bool>("Connected_R",   _HELP("Is right hand controller connected?")),

			OutputPortConfig<bool>("Button_A",      _HELP("Is A button pressed?")),
			OutputPortConfig<bool>("Button_B",      _HELP("Is B button pressed?")),
			OutputPortConfig<bool>("Button_X",      _HELP("Is X button pressed?")),
			OutputPortConfig<bool>("Button_Y",      _HELP("Is Y button pressed?")),
			OutputPortConfig<bool>("Button_LThumb", _HELP("Is left thumb stick pressed?")),
			OutputPortConfig<bool>("Button_RThumb", _HELP("Is right thunk stick pressed?")),

			OutputPortConfig<float>("Trigger_L",    _HELP("Left trigger analog value")),
			OutputPortConfig<float>("Trigger_R",    _HELP("Right trigger analog value")),
			OutputPortConfig<float>("Trigger_L2",   _HELP("Second left trigger analog value")),
			OutputPortConfig<float>("Trigger_R2",   _HELP("Second right trigger analog value")),

			OutputPortConfig<Vec3>("ThumbStick_L",  _HELP("Left thumb stick analog value")),
			OutputPortConfig<Vec3>("ThumbStick_R",  _HELP("Right thumb stick analog value")),

			{ 0 }
		};

		config.sDescription = _HELP("This node provides information about the connected Oculus Touch VR controllers");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ClearAllOutputs(SActivationInfo* pActInfo)
	{
		for (size_t i = EFirst_bool; i < ELast_bool; ++i)
			ActivateOutput(pActInfo, i, false);

		for (size_t i = EFirst_float; i < ELast_float; ++i)
			ActivateOutput(pActInfo, i, 0.f);

		ActivateOutput(pActInfo, EOP_ThumbStickL, Vec3(ZERO));
		ActivateOutput(pActInfo, EOP_ThumbStickR, Vec3(ZERO));
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				if (pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, EIP_Enabled);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
				}
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enabled) && pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, EIP_Enabled);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
				}
			}
			break;

		case eFE_Update:
			{
				const IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
				const IHmdDevice* pDev = pHmdManager ? pHmdManager->GetHmdDevice() : nullptr;
				if (pDev != nullptr)
				{
					if (const IHmdController* pController = pDev->GetController())
					{
						ActivateOutput(pActInfo, EOP_ConnectedL, pController->IsConnected(eHmdController_OculusLeftHand));
						ActivateOutput(pActInfo, EOP_ConnectedR, pController->IsConnected(eHmdController_OculusRightHand));

						// Note: for Oculus we do not use the controller id to disambiguate which button we refer to.
						ActivateOutput(pActInfo, EOP_ButtonA, pController->IsButtonPressed(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_A));
						ActivateOutput(pActInfo, EOP_ButtonB, pController->IsButtonPressed(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_B));
						ActivateOutput(pActInfo, EOP_ButtonX, pController->IsButtonPressed(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_X));
						ActivateOutput(pActInfo, EOP_ButtonY, pController->IsButtonPressed(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_Y));
						ActivateOutput(pActInfo, EOP_ButtonLThumb, pController->IsButtonPressed(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_L3));
						ActivateOutput(pActInfo, EOP_ButtonRThumb, pController->IsButtonPressed(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_R3));

						ActivateOutput(pActInfo, EOP_TriggerL, pController->GetTriggerValue(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_L1));
						ActivateOutput(pActInfo, EOP_TriggerR, pController->GetTriggerValue(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_R1));
						ActivateOutput(pActInfo, EOP_TriggerL2, pController->GetTriggerValue(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_L2));
						ActivateOutput(pActInfo, EOP_TriggerR2, pController->GetTriggerValue(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_R2));

						const Vec2 tsL = pController->GetThumbStickValue(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_StickL_Y);
						const Vec2 tsR = pController->GetThumbStickValue(eHmdController_OculusLeftHand, eKI_Motion_OculusTouch_StickR_Y);
						ActivateOutput(pActInfo, EOP_ThumbStickL, Vec3(tsL.x, tsL.y, 0.f));
						ActivateOutput(pActInfo, EOP_ThumbStickR, Vec3(tsR.x, tsR.y, 0.f));
						break;
					}
				}
				ClearAllOutputs(pActInfo);
			}
			break;
		}
	}
};

// ------------------------------------------------------------------------------------------------------------
class COpenVRController : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Enabled = 0,
		EIP_Index,
	};

	enum OUTPUTS
	{
		EOP_Connected,

		EOP_ButtonSystem,
		EOP_ButtonAppMenu,
		EOP_ButtonGrip,
		EOP_ButtonTrigger,
		EOP_ButtonPad,

		EOP_Trigger,

		EOP_Pad,

		EFirst_bool  = EOP_Connected,
		ELast_bool   = EOP_ButtonPad,
		EFirst_float = EOP_Trigger,
		ELast_float  = EOP_Pad,
	};

public:
	COpenVRController(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Enabled", true, _HELP("Enable / disable the node")),
			InputPortConfig<int>("Index",    true, _HELP("Controller Index")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("Connected",       _HELP("Is Controller connected?")),

			OutputPortConfig<bool>("Button_System",   _HELP("Is system button pressed?")),
			OutputPortConfig<bool>("Button_AppMenu",  _HELP("Is application menu button pressed?")),
			OutputPortConfig<bool>("Button_Grip",     _HELP("Is grip button pressed?")),
			OutputPortConfig<bool>("Button_Trigger",  _HELP("Is trigger pressed?")),
			OutputPortConfig<bool>("Button_TouchPad", _HELP("Is touch pad pressed?")),

			OutputPortConfig<float>("Trigger",        _HELP("Trigger analog value")),

			OutputPortConfig<Vec3>("TouchPad",        _HELP("Touch pad analog value")),

			{ 0 }
		};

		config.sDescription = _HELP("This node provides information about the connected OpenVR controllers");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ClearAllOutputs(SActivationInfo* pActInfo)
	{
		for (size_t i = EFirst_bool; i < ELast_bool; ++i)
			ActivateOutput(pActInfo, i, false);

		for (size_t i = EFirst_float; i < ELast_float; ++i)
			ActivateOutput(pActInfo, i, 0.f);

		ActivateOutput(pActInfo, EOP_Pad, Vec3(ZERO));
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				if (pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, EIP_Enabled);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
				}
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enabled) && pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, EIP_Enabled);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
				}
			}
			break;

		case eFE_Update:
			{
				const IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
				const IHmdDevice* pDev = pHmdManager ? pHmdManager->GetHmdDevice() : nullptr;
				int index = GetPortInt(pActInfo, EIP_Index);
				if (index >= 0 && index < eHmdController_OpenVR_MaxNumOpenVRControllers)
				{
					EHmdController controller = (EHmdController)index;
					if (pDev != nullptr)
					{
						if (const IHmdController* pController = pDev->GetController())
						{
							ActivateOutput(pActInfo, EOP_Connected, pController->IsConnected(controller));

							ActivateOutput(pActInfo, EOP_ButtonSystem, pController->IsButtonPressed(controller, eKI_Motion_OpenVR_System));
							ActivateOutput(pActInfo, EOP_ButtonAppMenu, pController->IsButtonPressed(controller, eKI_Motion_OpenVR_ApplicationMenu));
							ActivateOutput(pActInfo, EOP_ButtonGrip, pController->IsButtonPressed(controller, eKI_Motion_OpenVR_Grip));
							ActivateOutput(pActInfo, EOP_ButtonTrigger, pController->IsButtonPressed(controller, eKI_Motion_OpenVR_TriggerBtn));
							ActivateOutput(pActInfo, EOP_ButtonPad, pController->IsButtonPressed(controller, eKI_Motion_OpenVR_TouchPad_X));

							ActivateOutput(pActInfo, EOP_Trigger, pController->GetTriggerValue(controller, eKI_Motion_OpenVR_Trigger));

							const Vec2 tp = pController->GetThumbStickValue(controller, eKI_Motion_OpenVR_TouchPad_X);
							ActivateOutput(pActInfo, EOP_Pad, Vec3(tp.x, tp.y, 0.f));
							break;
						}
					}
				}
				ClearAllOutputs(pActInfo);
			}
			break;
		}
	}
};

class CVRControllerTracking : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Enabled = 0,
		EIP_OffsetLs,
		EIP_Scale
	};

	enum OUTPUTS
	{
		EOP_PosWS_L,
		EOP_RotWS_L,
		EOP_ValidData_L,
		EOP_PosWS_R,
		EOP_RotWS_R,
		EOP_ValidData_R,
	};

public:
	CVRControllerTracking(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Enabled", true,     _HELP("Enable / disable the node")),
			InputPortConfig<Vec3>("OffseLS", Vec3(0.f, 0.f,                                        0.f),_HELP("Offset in the selected entity's Local Space")),
			InputPortConfig<float>("Scale",  1.f,      _HELP("Scales the controllers' movements")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Left Pos",        _HELP("The position of the HMD left controller in world space")),
			OutputPortConfig<Vec3>("Left Rot (PRY)",  _HELP("The orientation of the HMD left controller in world space (Pitch,Roll,Yaw) in Degrees")),
			OutputPortConfig<bool>("Left data ok",    _HELP("The left HMD controller is connected and the data is valid")),
			OutputPortConfig<Vec3>("Right Pos",       _HELP("The position of the HMD right controller in world space")),
			OutputPortConfig<Vec3>("Right Rot (PRY)", _HELP("The orientation of the HMD right controller in world space (Pitch,Roll,Yaw) in Degrees")),
			OutputPortConfig<bool>("Right data ok",   _HELP("The right HMD controller is connected and the data is valid")),
			{ 0 }
		};

		config.sDescription = _HELP("This node provides information about the orientation and position of the VR controller in world space based on the world transform of the selected entity");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				if (pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, EIP_Enabled);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
				}
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enabled) && pActInfo->pGraph != nullptr)
				{
					const bool enabled = GetPortBool(pActInfo, EIP_Enabled);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, enabled);
				}
			}
			break;

		case eFE_Update:
			{
				bool bHmdLeftControllerOk = false, bHmdRightControllerOk = false;

				const IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
				const IHmdDevice* pDev = pHmdManager ? pHmdManager->GetHmdDevice() : nullptr;
				if (pDev)
				{
					// by default use the selected entity as reference
					const IEntity* pEntity = pActInfo->pEntity;
					if (pEntity == nullptr)
					{
						// if no entity is passed to the node use the local player
						if (const IActor* pActor = gEnv->pGameFramework->GetClientActor())
						{
							pEntity = pActor->GetEntity();
						}
					}
					if (pEntity != nullptr)
					{
						const HmdTrackingState& sensorState = pDev->GetLocalTrackingState();
						if (sensorState.CheckStatusFlags(eHmdStatus_IsUsable))
						{
							if (const IHmdController* pController = pDev->GetController())
							{
								bHmdLeftControllerOk = pController->IsConnected(eHmdController_OculusLeftHand);
								bHmdRightControllerOk = pController->IsConnected(eHmdController_OculusRightHand);

								const Matrix34& entityTM = pEntity->GetWorldTM();
								Matrix33 m;
								entityTM.GetRotation33(m);
								const Quat entityRotWS(m);

								const HmdTrackingState& leftCtrlState = pController->GetLocalTrackingState(eHmdController_OculusLeftHand);
								const Quat hmdLeftQ = entityRotWS * leftCtrlState.pose.orientation;
								const Ang3 hmdLeftCtrlAngles(hmdLeftQ);
								ActivateOutput(pActInfo, EOP_RotWS_L, Vec3(RAD2DEG(hmdLeftCtrlAngles)));

								const Vec3 offsetLS = GetPortVec3(pActInfo, EIP_OffsetLs);
								const float scale = GetPortFloat(pActInfo, EIP_Scale);

								const Vec3 hmdPosL = entityTM.GetTranslation() + entityRotWS * (leftCtrlState.pose.position * scale + offsetLS);
								ActivateOutput(pActInfo, EOP_PosWS_L, hmdPosL);

								const HmdTrackingState& rightCtrlState = pController->GetLocalTrackingState(eHmdController_OculusRightHand);
								const Quat hmdRightQ = entityRotWS * rightCtrlState.pose.orientation;
								const Ang3 hmdRightCtrlAngles(hmdRightQ);
								ActivateOutput(pActInfo, EOP_RotWS_R, Vec3(RAD2DEG(hmdRightCtrlAngles)));

								const Vec3 hmdPosR = entityTM.GetTranslation() + entityRotWS * (rightCtrlState.pose.position * scale + offsetLS);
								ActivateOutput(pActInfo, EOP_PosWS_R, hmdPosR);
							}
						}
					}
				}

				ActivateOutput(pActInfo, EOP_ValidData_L, bHmdLeftControllerOk);
				ActivateOutput(pActInfo, EOP_ValidData_R, bHmdRightControllerOk);
			}
			break;
		}
	}
};

class CVQuadRenderLayer : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Layer = 0,
		EIP_Enable,
		EIP_Type,
		EIP_ApplyType,
		EIP_Position,
		EIP_ApplyPos,
		EIP_Orientation,
		EIP_ApplyOrientation,
		EIP_Scale,
		EIP_ApplyScale,
	};

	enum OUTPUTS
	{
		EOP_Done,
		EOP_Error,
	};

public:
	CVQuadRenderLayer(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("Layer",               0,                          _HELP("VR Quad render layer"),                         _HELP("Layer")),
			InputPortConfig<bool>("Enable",             false,                      _HELP("Enable/Disable sending this layer to the Hmd"), _HELP("Enable")),
			InputPortConfig<int>("Type",                0,                          _HELP("Free Quad or Locked Quad"),                     _HELP("Type"),   _UICONFIG(RenderLayer::GetQuadLayerType_UICONFIG())),
			InputPortConfig_AnyType("ApplyType",        _HELP("Apply Type"),        _HELP("ApplyType")),
			InputPortConfig<Vec3>("Position",           Vec3(0.f,                   0.f,                                                   -1.f),           _HELP("Layer's position (in meters) in camera space (e.g. X=1 [1m. to the right] Y=1 [1m. up] Z=1 [1m. in front of the player)")),
			InputPortConfig_AnyType("ApplyPos",         _HELP("Apply Postition"),   _HELP("ApplyPos")),
			InputPortConfig<Vec3>("Orientation",        Vec3(0.f,                   0.f,                                                   -1.f),           _HELP("Layer's orientation in degrees (x,y,z axis)")),
			InputPortConfig_AnyType("ApplyOrientation", _HELP("Apply orientation"), _HELP("ApplyOrientation")),
			InputPortConfig<float>("Scale",             1.f,                        _HELP("Scales the Layer")),
			InputPortConfig_AnyType("ApplyScale",       _HELP("Apply Scale"),       _HELP("ApplyScale")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done",  _HELP("Done")),
			OutputPortConfig_AnyType("Error", _HELP("Error")),
			{ 0 }
		};

		config.sDescription = _HELP("Allows to modify parameters in a VR Quad layer");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				// Only process certain events
				const bool bApplyScale = IsPortActive(pActInfo, EIP_ApplyScale);
				const bool bApplyPos = IsPortActive(pActInfo, EIP_ApplyPos);
				const bool bApplyOri = IsPortActive(pActInfo, EIP_ApplyOrientation);
				const bool bEnableChange = IsPortActive(pActInfo, EIP_Enable);
				const bool bApplyType = IsPortActive(pActInfo, EIP_ApplyType);

				if (!bApplyScale && !bApplyPos && !bApplyOri && !bEnableChange && !bApplyType)
				{
					return;
				}

				// Check layer id validity (should never be wrong but just in case)
				const int id = GetPortInt(pActInfo, EIP_Layer);
				if (!RenderLayer::IsQuadLayer(id))
				{
					ActivateOutput(pActInfo, EOP_Error, true);
					return;
				}

				const RenderLayer::EQuadLayers layerId = RenderLayer::SafeCastToQuadLayer(id);

				// Get layer properties
				RenderLayer::CProperties* pLayerProps = nullptr;
				if (IStereoRenderer* pStereoRenderer = gEnv->pRenderer->GetIStereoRenderer())
				{
					if (IHmdRenderer* pHmdRender = pStereoRenderer->GetIHmdRenderer())
					{
						pLayerProps = pHmdRender->GetQuadLayerProperties(layerId);
					}
				}

				if (pLayerProps == nullptr)
				{
					ActivateOutput(pActInfo, EOP_Error, true);
					return;
				}

				// Enable/Disable
				if (bEnableChange && pActInfo->pGraph != nullptr)
				{
					const bool bActive = GetPortBool(pActInfo, EIP_Enable);
					pLayerProps->SetActive(bActive);
					ActivateOutput(pActInfo, EOP_Done, true);
					return;
				}

				// Quad Type
				if (bApplyType && pActInfo->pGraph != nullptr)
				{
					const int type = GetPortInt(pActInfo, EIP_Type);
					if (type == RenderLayer::eLayer_Quad || type == RenderLayer::eLayer_Quad_HeadLocked)
					{
						pLayerProps->SetType(static_cast<RenderLayer::ELayerType>(type));
						ActivateOutput(pActInfo, EOP_Done, true);
					}
					else
					{
						ActivateOutput(pActInfo, EOP_Error, true);
					}
					return;
				}

				// Apply Pose changes
				QuatTS pose = pLayerProps->GetPose();
				if (bApplyScale && pActInfo->pGraph != nullptr)
				{
					const float scale = GetPortFloat(pActInfo, EIP_Scale);
					pose.s = scale;
				}
				if (bApplyPos && pActInfo->pGraph != nullptr)
				{
					const Vec3 position = GetPortVec3(pActInfo, EIP_Position);
					pose.t = position;
				}
				if (bApplyOri && pActInfo->pGraph != nullptr)
				{
					const Vec3 orientation = GetPortVec3(pActInfo, EIP_Orientation);
					pose.q = Quat(Ang3(DEG2RAD(orientation)));
				}
				pLayerProps->SetPose(pose);
				ActivateOutput(pActInfo, EOP_Done, true);
			}
			break;
		}
	}
};

class CVRRenderLayerTexture : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_LayerType = 0,
		EIP_LayerId,
		EIP_Texture,
		EIP_Apply,
		EIP_Disable,
	};

	enum OUTPUTS
	{
		EOP_Done,
		EOP_Error,
	};

public:
	CVRRenderLayerTexture(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("LayerType",          0,                                                                  _HELP("VR render layer type"), _HELP("LayerType"),   _UICONFIG(RenderLayer::GetLayerType_UICONFIG())),
			InputPortConfig<int>("QuadLayerId",        0,                                                                  _HELP("VR Quad layer id"),     _HELP("QuadLayerId"), _UICONFIG(RenderLayer::GetQuadLayer_UICONFIG())),
			InputPortConfig<string>("tex_TextureName", _HELP("Texture Name")),
			InputPortConfig_AnyType("ApplyScale",      _HELP("Apply"),                                                     _HELP("Apply")),
			InputPortConfig_AnyType("Disable",         _HELP("Do not render the selected texture into the layer anymore"), _HELP("Disable")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done",  _HELP("Done")),
			OutputPortConfig_AnyType("Error", _HELP("Error")),
			{ 0 }
		};

		config.sDescription = _HELP("Forces a texture to be rendered on a particular layer");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				// Get Layer info
				RenderLayer::CProperties* pLayerProps = nullptr;
				const int layerType = GetPortInt(pActInfo, EIP_LayerType);
				const int layerId = GetPortInt(pActInfo, EIP_LayerId);
				if (layerType == RenderLayer::eLayer_Scene3D)
				{
					// TODO: allow more than one SCENE3D Layer
					if (IStereoRenderer* pStereoRenderer = gEnv->pRenderer->GetIStereoRenderer())
					{
						if (IHmdRenderer* pHmdRender = pStereoRenderer->GetIHmdRenderer())
						{
							pLayerProps = pHmdRender->GetSceneLayerProperties(RenderLayer::eSceneLayers_0);
						}
					}
				}
				else if (layerType == RenderLayer::eLayer_Quad || layerType == RenderLayer::eLayer_Quad_HeadLocked)
				{
					if (!RenderLayer::IsQuadLayer(layerId))
					{
						ActivateOutput(pActInfo, EOP_Error, true);
						return;
					}

					if (IStereoRenderer* pStereoRenderer = gEnv->pRenderer->GetIStereoRenderer())
					{
						if (IHmdRenderer* pHmdRender = pStereoRenderer->GetIHmdRenderer())
						{
							pLayerProps = pHmdRender->GetQuadLayerProperties(RenderLayer::SafeCastToQuadLayer(layerId));
						}
					}
				}

				if (pLayerProps == nullptr)
				{
					ActivateOutput(pActInfo, EOP_Error, true);
					return;
				}

				// Apply
				if (IsPortActive(pActInfo, EIP_Apply) && pActInfo->pGraph != nullptr)
				{
					const string& textureName = GetPortString(pActInfo, EIP_Texture);
					ITexture* pITexture = nullptr;
					if (!textureName.empty() && gEnv->pRenderer)
					{
						pITexture = gEnv->pRenderer->EF_LoadTexture(textureName, FT_DONT_STREAM);
					}
					pLayerProps->SetTexture(pITexture);

					ActivateOutput(pActInfo, EOP_Done, true);
				}
				// Disable
				else if (IsPortActive(pActInfo, EIP_Disable) && pActInfo->pGraph != nullptr)
				{
					pLayerProps->SetTexture(nullptr);
				}
			}
			break;
		}
	}
};

REGISTER_FLOW_NODE("VR:Tools", CVRTools);
REGISTER_FLOW_NODE("VR:TransformInfo", CVRTransformInfo);
REGISTER_FLOW_NODE("VR:OculusController", CVROculusController);
REGISTER_FLOW_NODE("VR:ControllerTracking", CVRControllerTracking);
REGISTER_FLOW_NODE("VR:QuadRenderLayer", CVQuadRenderLayer);
REGISTER_FLOW_NODE("VR:RenderLayerTexture", CVRRenderLayerTexture);
REGISTER_FLOW_NODE("VR:OpenVRController", COpenVRController);
