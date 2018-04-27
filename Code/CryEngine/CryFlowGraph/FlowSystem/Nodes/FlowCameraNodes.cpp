// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowFrameworkBaseNode.h"

//////////////////////////////////////////////////////////////////////////
static const int FLOWGRAPH_SHAKE_ID = 24;

//////////////////////////////////////////////////////////////////////////
class CFlowNode_CameraViewShakeEx : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_CameraViewShakeEx(SActivationInfo* pActInfo)
		: m_lastShakeFrameID(0)
	{
	};

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_CameraViewShakeEx(pActInfo); }

	enum eRestriction
	{
		ER_None = 0,
		ER_NoVehicle,
		ER_InVehicle,
	};

	enum eInputs
	{
		EIP_Trigger = 0,
		EIP_Restriction,
		EIP_ViewType,
		EIP_GroundOnly,
		EIP_Smooth,
		EIP_Angle,
		EIP_Shift,
		EIP_Frequency,
		EIP_Randomness,
		EIP_Distance,
		EIP_RangeMin,
		EIP_RangeMax,
		EIP_SustainDuration,
		EIP_FadeInDuration,
		EIP_FadeOutDuration,
		EIP_Stop,
		EIP_Preset,
		// EIP_Flip,
	};

	enum eViewType
	{
		VT_FirstPerson = 0,
		VT_Current     = 1
	};

	enum { NUM_PRESETS = 4 };

	struct SInputParams
	{
		const char*  pName;
		eRestriction restriction;
		eViewType    view;
		bool         groundOnly;
		bool         isSmooth;
		Ang3         angle;
		Vec3         shift;
		float        frequency;
		float        randomness;
		float        distance;
		float        rangeMin;
		float        rangeMax;
		float        sustainDuration;
		float        fadeInDuration;
		float        fadeOutDuration;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
#ifndef _RELEASE
		static char pPresetsHelp[1024] = "";

		if (pPresetsHelp[0] == 0)
		{
			cry_strcpy(pPresetsHelp, "Preset input values. When this is used, all parameter inputs are ignored.\n");
			for (int i = 0; i < NUM_PRESETS; i++)
			{
				char buf[300];
				cry_sprintf(buf, "-- %s : Angle: (%4.3f,%4.3f,%4.3f)  Shift: (%4.3f,%4.3f,%4.3f )  Freq: %3.1f  Random: %3.1f  Distance: %3.1f  RangeMin: %3.1f RangeMax: %3.1f sustainDuration: %3.1f FadeInDur: %3.1f  FadeOutDur: %3.1f \n",
				            m_Presets[i].pName, m_Presets[i].angle.x, m_Presets[i].angle.y, m_Presets[i].angle.z, m_Presets[i].shift.x, m_Presets[i].shift.y, m_Presets[i].shift.z, m_Presets[i].frequency,
				            m_Presets[i].randomness, m_Presets[i].distance, m_Presets[i].rangeMin, m_Presets[i].rangeMax, m_Presets[i].sustainDuration, m_Presets[i].fadeInDuration, m_Presets[i].fadeOutDuration);

				cry_strcat(pPresetsHelp, buf);
			}
		}

		static char pPresetsEnumDef[100] = "";
		if (pPresetsEnumDef[0] == 0)
		{
			cry_strcpy(pPresetsEnumDef, "enum_int:NoPreset=0,");
			for (int i = 0; i < NUM_PRESETS; i++)
			{
				char buf[100];
				cry_sprintf(buf, "%s=%1d,", m_Presets[i].pName, i + 1);
				cry_strcat(pPresetsEnumDef, buf);
			}
		}
#endif

		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",           _HELP("Trigger to start shaking")),
			InputPortConfig<int>("Restrict",          ER_None,                                   _HELP("Restriction"),                                                                                                                              0,      _UICONFIG("enum_int:None=0,NoVehicle=1,InVehicle=2")),
			InputPortConfig<int>("View",              VT_FirstPerson,                            _HELP("Which view to use. FirstPerson or Current (might be Trackview)."),                                                                          0,      _UICONFIG("enum_int:FirstPerson=0,Current=1")),
			InputPortConfig<bool>("GroundOnly",       false,                                     _HELP("Apply shake only when the player is standing on the ground")),
			InputPortConfig<bool>("Smooth",           false,                                     _HELP("Smooth shakes avoid sudden direction changes.")),
			InputPortConfig<Vec3>("Angle",            Vec3(0.7f,                                 0.7f,                                                                                                                                              0.7f),  _HELP("Shake Angles")),
			InputPortConfig<Vec3>("Shift",            Vec3(0.01f,                                0.01f,                                                                                                                                             0.01f), _HELP("Shake shifting")),
			InputPortConfig<float>("Frequency",       12.0f,                                     _HELP("Frequency. Can be changed dynamically."),                                                                                                   0,      _UICONFIG("v_min=0,v_max=100")),
			InputPortConfig<float>("Randomness",      1.f,                                       _HELP("Randomness")),
			InputPortConfig<float>("Distance",        0.0f,                                      _HELP("Distance to effect source. If an entity is asociated to the node, distance from that entity to the current camera will be used instead")),
			InputPortConfig<float>("RangeMin",        0.0f,                                      _HELP("Maximum strength effect range")),
			InputPortConfig<float>("RangeMax",        30.0f,                                     _HELP("Effect range")),
			InputPortConfig<float>("SustainDuration", 0.f,                                       _HELP("duration of the non fading part of the shake. (total duration is fadein + this + fadeout ). -1 = permanent")),
			InputPortConfig<float>("FadeInDuration",  0.f,                                       _HELP("Fade in time (seconds)")),
			InputPortConfig<float>("FadeOutDuration", 3.f,                                       _HELP("Fade out time (seconds)")),
			InputPortConfig_Void("Stop",              _HELP("Stop the shaking (will fade out)")),
#ifdef _RELEASE
			InputPortConfig<int>("Preset",            0,                                         _HELP("Preset input values. When this is used, all parameter inputs are ignored.")),
#else
			InputPortConfig<int>("Preset",            0,                                         pPresetsHelp,                                                                                                                                      0,      _UICONFIG(pPresetsEnumDef)),
#endif
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Camera View Shake node. If an entity is provided, its position will be used for the distance calculations instead of the raw 'distance' input.");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			StopShake(pActInfo);
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Stop))
			{
				StopShake(pActInfo);
			}
			else
			{
				const bool bTriggered = IsPortActive(pActInfo, EIP_Trigger);
				const bool bFreqTriggered = IsPortActive(pActInfo, EIP_Frequency);
				if (bTriggered || bFreqTriggered)
				{
					Shake(pActInfo, !bTriggered && bFreqTriggered);
				}
			}
			break;
		}
	}

	//----------------------------------------------------------------------
	void StopShake(SActivationInfo* pActInfo)
	{
		IView* pView = GetView(pActInfo);
		if (pView)
			pView->StopShake(FLOWGRAPH_SHAKE_ID);
	}

	//----------------------------------------------------------------------
	void Shake(SActivationInfo* pActInfo, bool bUpdateOnly)
	{
		IView* pView = GetView(pActInfo);
		if (!pView)
			return;

		int currentFrameId = gEnv->pRenderer->GetFrameID();

		if (!bUpdateOnly && (m_lastShakeFrameID == currentFrameId))
			return;

		SInputParams inputParams;
		ReadCurrentInputParams(pActInfo, inputParams);

		IView::SShakeParams params;

		params.bGroundOnly = inputParams.groundOnly;
		params.isSmooth = inputParams.isSmooth;
		params.shakeAngle = inputParams.angle;
		params.shakeShift = inputParams.shift;
		params.sustainDuration = inputParams.sustainDuration;
		params.bPermanent = (params.sustainDuration == -1);
		float freq = inputParams.frequency;
		if (iszero(freq) == false)
			freq = 1.0f / freq;
		params.frequency = freq;
		params.randomness = inputParams.randomness;
		params.bFlipVec = true;           // GetPortBool(pActInfo, EIP_Flip);
		params.bUpdateOnly = bUpdateOnly; // it's an update if and only if Frequency has been changed
		params.fadeInDuration = inputParams.fadeInDuration;
		params.fadeOutDuration = inputParams.fadeOutDuration;
		params.shakeID = FLOWGRAPH_SHAKE_ID;

		float distance = inputParams.distance;
		IEntity* pEntityNode = pActInfo->pEntity;
		if (pEntityNode)
		{
			distance = pView->GetCurrentParams()->position.GetDistance(pEntityNode->GetWorldPos());
		}

		const float rangeMin = inputParams.rangeMin;
		const float rangeMax = inputParams.rangeMax;
		float amount = min(1.f, max(0.f, (rangeMax - distance) / (rangeMax - rangeMin)));

		params.shakeAngle *= amount;
		params.shakeShift *= amount;

		pView->SetViewShakeEx(params);

		m_lastShakeFrameID = currentFrameId;
	}

	//----------------------------------------------------------------------
	IView* GetView(SActivationInfo* pActInfo)
	{
		SInputParams inputParams;
		ReadCurrentInputParams(pActInfo, inputParams);
		IGameFramework* pGF = gEnv->pGameFramework;
		IView* pView = 0;
		IView* pActiveView = pGF->GetIViewSystem()->GetActiveView();
		eViewType viewType = inputParams.view;
		if (viewType == VT_FirstPerson) // use player's view
		{
			IActor* pActor = pGF->GetClientActor();
			if (pActor == 0)
				return NULL;

			eRestriction restriction = inputParams.restriction;
			if (restriction != ER_None)
			{
				IVehicle* pVehicle = pActor->GetLinkedVehicle();
				if (restriction == ER_InVehicle && pVehicle == 0)
					return NULL;
				if (restriction == ER_NoVehicle && pVehicle != 0 /* && pVehicle->GetSeatForPassenger(entityId) != 0 */)
					return NULL;
			}

			EntityId entityId = pActor->GetEntityId();
			pView = pGF->GetIViewSystem()->GetViewByEntityId(entityId);
		}
		else // active view
		{
			pView = pActiveView;
		}
		if (pView != pActiveView)
			return NULL;

		return pView;
	}

	//----------------------------------------------------------------------
	void ReadCurrentInputParams(SActivationInfo* pActInfo, SInputParams& inputParams)
	{
		int presetIndex = GetPortInt(pActInfo, EIP_Preset);
		if (presetIndex > 0 && presetIndex <= NUM_PRESETS)
		{
			inputParams = m_Presets[presetIndex - 1];
			inputParams.angle = Ang3(DEG2RAD(m_Presets[presetIndex - 1].angle));
		}
		else
		{
			inputParams.groundOnly = GetPortBool(pActInfo, EIP_GroundOnly);
			inputParams.isSmooth = GetPortBool(pActInfo, EIP_Smooth);
			inputParams.restriction = eRestriction(GetPortInt(pActInfo, EIP_Restriction));
			inputParams.view = eViewType(GetPortInt(pActInfo, EIP_ViewType));
			inputParams.angle = Ang3(DEG2RAD(GetPortVec3(pActInfo, EIP_Angle)));
			inputParams.shift = GetPortVec3(pActInfo, EIP_Shift);
			inputParams.frequency = GetPortFloat(pActInfo, EIP_Frequency);
			inputParams.randomness = GetPortFloat(pActInfo, EIP_Randomness);
			inputParams.distance = GetPortFloat(pActInfo, EIP_Distance);
			inputParams.rangeMin = GetPortFloat(pActInfo, EIP_RangeMin);
			inputParams.rangeMax = GetPortFloat(pActInfo, EIP_RangeMax);
			inputParams.sustainDuration = GetPortFloat(pActInfo, EIP_SustainDuration);
			inputParams.fadeInDuration = GetPortFloat(pActInfo, EIP_FadeInDuration);
			inputParams.fadeOutDuration = GetPortFloat(pActInfo, EIP_FadeOutDuration);
		}
	}

	static SInputParams m_Presets[NUM_PRESETS];
	int                 m_lastShakeFrameID;
};

CFlowNode_CameraViewShakeEx::SInputParams CFlowNode_CameraViewShakeEx::m_Presets[] =
{
	//                                             Ground Smooth      angle                      shift													fr  rnd			d   rm  rm   sd   fi     fo
	{ "DistantExplosion", ER_None, VT_FirstPerson, false, false, Ang3(0.6f,  0.6f,  0.6f),  Vec3(0.001f,   0.001f,   0.001f),   10, 2,    0, 0, 30, 0, 0.2f, 1.2f },
	{ "CloseExplosion",   ER_None, VT_FirstPerson, false, false, Ang3(0.5f,  0.5f,  0.5f),  Vec3(0.003f,   0.003f,   0.003f),   30, 5,    0, 0, 30, 0, 0,    1.1f },
	{ "SmallTremor",      ER_None, VT_FirstPerson, false, false, Ang3(0.2f,  0.2f,  0.2f),  Vec3(0.0001f,  0.0001f,  0.0001f),  18, 1.3f, 0, 0, 30, 2, 1.2f, 3.f  },
	{ "SmallTremor2",     ER_None, VT_FirstPerson, false, false, Ang3(0.25f, 0.25f, 0.25f), Vec3(0.00045f, 0.00045f, 0.00045f), 13, 2,    0, 0, 30, 2, 1.2f, 3.f  },
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_CameraView : public CFlowBaseNode<eNCT_Instanced>
{
private:
	IView*       m_pView;
	IView*       m_pLocalPlayerView;
	IViewSystem* m_pViewSystem;

public:
	CFlowNode_CameraView(SActivationInfo* pActInfo) :
		m_pView(NULL),
		m_pLocalPlayerView(NULL),
		m_pViewSystem(NULL)
	{
	}

	~CFlowNode_CameraView()
	{
		if (m_pViewSystem && m_pView)
		{
			m_pViewSystem->RemoveView(m_pView);
		}
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_CameraView(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_FOV,
		EIP_Blend,
		EIP_BlendFOVSpeed,
		EIP_BlendFOVOffset,
		EIP_BlendPosSpeed,
		EIP_BlendPosOffset,
		EIP_BlendRotSpeed,
		EIP_BlendRotOffset
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Enable",          "Enable custom view"),
			InputPortConfig<bool>("Disable",         "Disable custom view"),
			InputPortConfig<float>("FOV",            60.0f,                 "Field of View used by the custom view"),
			InputPortConfig<bool>("Blend",           false,                 "Enables FOV, Position and Rotation blending"),
			InputPortConfig<float>("BlendFOVSpeed",  5.0f,                  "How fast to blend in the FOV offset"),
			InputPortConfig<float>("BlendFOVOffset", 0.0f,                  "FOV offset"),
			InputPortConfig<float>("BlendPosSpeed",  5.0f,                  "How fast to blend in the position offset"),
			InputPortConfig<Vec3>("BlendPosOffset",  Vec3(ZERO),            "Position offset"),
			InputPortConfig<float>("BlendRotSpeed",  10.0f,                 "How fast to blend in the rotation offset"),
			InputPortConfig<Vec3>("BlendRotOffset",  Vec3(ZERO),            "Rotation offset"),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.sDescription = _HELP("Creates a custom view linked to an entity");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				CreateView();
				return;
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enable))
				{
					if (m_pView == NULL)
						CreateView();

					if (m_pView == NULL)
						return;

					if (pActInfo->pEntity == NULL)
						return;

					SViewParams params;

					params.justActivated = true;
					params.fov = DEG2RAD(GetPortFloat(pActInfo, EIP_FOV));
					params.blend = GetPortBool(pActInfo, EIP_Blend);
					params.blendFOVOffset = DEG2RAD(GetPortFloat(pActInfo, EIP_BlendFOVOffset));
					params.blendFOVSpeed = GetPortFloat(pActInfo, EIP_BlendFOVSpeed);
					params.blendPosOffset = GetPortVec3(pActInfo, EIP_BlendPosOffset);
					params.blendPosSpeed = GetPortFloat(pActInfo, EIP_BlendPosSpeed);
					params.blendRotSpeed = GetPortFloat(pActInfo, EIP_BlendRotSpeed);
					params.blendRotOffset = Quat::CreateRotationXYZ(static_cast<Ang3>(GetPortVec3(pActInfo, EIP_BlendRotOffset)));

					m_pView->SetCurrentParams(params);
					m_pView->LinkTo(pActInfo->pEntity);

					m_pViewSystem->SetActiveView(m_pView);
				}
				else if (IsPortActive(pActInfo, EIP_Disable))
				{
					if (m_pLocalPlayerView)
						m_pViewSystem->SetActiveView(m_pLocalPlayerView);
				}
				else if (IsPortActive(pActInfo, EIP_FOV))
				{
					SViewParams params;
					params.fov = DEG2RAD(GetPortFloat(pActInfo, EIP_FOV));
					m_pView->SetCurrentParams(params);
				}
			}
			break;
		}
	}

	void CreateView()
	{
		IGameFramework* pGF = gEnv->pGameFramework;
		if (pGF == NULL)
			return;

		m_pViewSystem = pGF->GetIViewSystem();

		if (m_pViewSystem == NULL)
			return;
		//if the game has a clientactor, cache the view
		if (pGF->GetClientActor())
		{
			m_pLocalPlayerView = m_pViewSystem->GetViewByEntityId(pGF->GetClientActor()->GetEntityId());
		}

		if (m_pLocalPlayerView && (m_pViewSystem->GetActiveView() != m_pLocalPlayerView))
			m_pViewSystem->SetActiveView(m_pLocalPlayerView);

		if (m_pView == NULL)
			m_pView = m_pViewSystem->CreateView();
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_CameraTransform : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_CameraTransform(SActivationInfo* pActInfo){}

	~CFlowNode_CameraTransform() {}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputPorts
	{
		EIP_Get = 0,
	};

	enum EOutputPorts
	{
		EOP_Pos = 0,
		EOP_Dir,
		EOP_Rot,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get", "Returns the Pos/Dir of the camera"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Pos", "Position of the camera"),
			OutputPortConfig<Vec3>("Dir", "ViewDir of the camera"),
			OutputPortConfig<Vec3>("Rot", "Rotation of the camera (in degrees)"),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Returns the transform for the playercamera");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Get))
				{
					const CCamera& cam = GetISystem()->GetViewCamera();
					const Ang3 angles = RAD2DEG(cam.GetAngles());
					const Vec3 anglesAsVec3(angles);
					ActivateOutput(pActInfo, EOP_Pos, cam.GetPosition());
					ActivateOutput(pActInfo, EOP_Dir, cam.GetViewdir());
					ActivateOutput(pActInfo, EOP_Rot, anglesAsVec3);
				}

			}
			break;
		}
	}
};

REGISTER_FLOW_NODE("Camera:ViewShakeEx", CFlowNode_CameraViewShakeEx);
REGISTER_FLOW_NODE("Camera:View", CFlowNode_CameraView);
REGISTER_FLOW_NODE("Camera:GetTransform", CFlowNode_CameraTransform);
