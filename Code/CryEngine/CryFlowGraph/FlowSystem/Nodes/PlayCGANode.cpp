// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include <../CryAction/ICooperativeAnimationManager.h>
#include <CryFlowGraph/IFlowBaseNode.h>

class CPlayCGA_Node : public CFlowBaseNode<eNCT_Instanced>
{
	IEntity* m_pEntity;
public:
	CPlayCGA_Node(SActivationInfo* pActInfo) : m_pEntity(0)
	{
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CPlayCGA_Node(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		// TODO: what to do on load?
		// sequence might have played, or should we replay from start or trigger done or ???
		// we set the entity to our best knowledge
		if (ser.IsReading())
			m_pEntity = pActInfo->pEntity;
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("CGA_File",           _HELP("CGA Filename")),
			InputPortConfig<string>("anim_CGA_Animation", _HELP("CGA Animation name")),
			InputPortConfig<bool>("Trigger",              _HELP("Starts the animation")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("Done", _HELP("Set to TRUE when animation is finished")),
			{ 0 }
		};
		config.sDescription = _HELP("Plays a CGA Animation");
		config.nFlags |= EFLN_TARGET_ENTITY;
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

				m_pEntity = pActInfo->pEntity;
				if (m_pEntity != NULL)
				{
					if (IsPortActive(pActInfo, 0))
					{
						m_pEntity->LoadCharacter(0, GetPortString(pActInfo, 0));
					}
					if (!IsPortActive(pActInfo, 2)) break;
					ICharacterInstance* pCharacter = m_pEntity->GetCharacter(0);
					if (pCharacter != NULL)
					{
						CryCharAnimationParams params;
						pCharacter->GetISkeletonAnim()->StartAnimation(GetPortString(pActInfo, 1), params);
						//pCharacter->SetFlags(pCharacter->GetFlags() | CS_FLAG_UPDATE_ALWAYS); doesn't seem to matter
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
						ActivateOutput(pActInfo, 0, false);
					}
					else CryLogAlways("PlayCGA - Get/LoadCharacter failed");
				}
				else CryLogAlways("PlayCGA - Invalid entity pointer");
				break;
			}

		case eFE_Initialize:
			m_pEntity = pActInfo->pEntity;
			if (m_pEntity)
			{
				m_pEntity->LoadCharacter(0, GetPortString(pActInfo, 0));
			}
			break;

		case eFE_Update:
			{
				if (m_pEntity != NULL)
				{
					ICharacterInstance* pCharacter = m_pEntity->GetCharacter(0);
					//CryLogAlways("Using deprecated AnimAnimation node in animation graph (entity %s)", data.pEntity->GetName());
					/*if(pCharacter->GetCurrentAnimation(0) == -1)
					   {
					   ActivateOutput(pActInfo, 0, true);
					   pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					   }*/

					QuatT offset(m_pEntity->GetSlotLocalTM(0, false));
					QuatT renderLocation(m_pEntity->GetSlotWorldTM(0));
					QuatTS animLocation = renderLocation * offset; // TODO: This might be wrong.
					float fDistance = (GetISystem()->GetViewCamera().GetPosition() - animLocation.t).GetLength();

					SAnimationProcessParams params;
					params.locationAnimation = renderLocation;
					params.bOnRender = 0;
					params.zoomAdjustedDistanceFromCamera = fDistance;
					pCharacter->StartAnimationProcessing(params);
				}
				break;
			}
		}
		;
	};
};

class CAnimationBoneInfo_Node
	: public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		eInputPort_BoneName = 0,
		eInputPort_BoneId,
		eInputPort_Enabled,
		eInputPort_Get
	};

	enum EOutputPorts
	{
		eOutputPort_LocalPosition = 0,
		eOutputPort_LocalRotation,
		eOutputPort_WorldPosition,
		eOutputPort_WorldRotation
	};

public:
	CAnimationBoneInfo_Node(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
			OnChange(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>("bone_BoneName", _HELP("Name of Bone to get info for")),
			InputPortConfig<int>("BoneId", INVALID_JOINT_ID, _HELP("Id of the bone to get info for (used only if BoneName is left empty)")),
			InputPortConfig<bool>("Enabled", true, _HELP("Enable / Disable automatic per frame updates")),
			InputPortConfig_Void("Get", _HELP("Retrieve the bone info just once")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("LocalPos", _HELP("Position of bone in Local Space")),
			OutputPortConfig<Vec3>("LocalRot", _HELP("Rotation of bone in Local Space")),
			OutputPortConfig<Vec3>("WorldPos", _HELP("Position of bone in World Space")),
			OutputPortConfig<Vec3>("WorldRot", _HELP("Rotation of bone in World Space")),
			{ 0 }
		};
		config.sDescription = _HELP("Outputs information of the bone [BoneName] of the character of the attached entity");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	static int32 GetDesiredJointId(SActivationInfo* pActInfo, ICharacterInstance* pCharacter)
	{
		int32 result = INVALID_JOINT_ID;

		const IDefaultSkeleton& defaultSkeleton = pCharacter->GetIDefaultSkeleton();
		const string& boneName = GetPortString(pActInfo, eInputPort_BoneName);

		if (!boneName.empty())
		{
			result = defaultSkeleton.GetJointIDByName(boneName.c_str());
			if (result == INVALID_JOINT_ID)
			{
				CryLogAlways("[flow] Animations:BoneInfo: Cannot find bone '%s' in character 0 of entity '%s'", boneName.c_str(), pActInfo->pEntity->GetName());
			}
		}
		else
		{
			result = GetPortInt(pActInfo, eInputPort_BoneId);
			if (result >= static_cast<int32>(defaultSkeleton.GetJointCount()))
			{
				result = INVALID_JOINT_ID;
			}
		}

		return result;
	}

	void OnChange(SActivationInfo* pActInfo)
	{
		struct UpdateChanger
		{
			UpdateChanger(SActivationInfo* const pActivationInfo, const bool bUpdate) : pActivationInfo(pActivationInfo), bUpdate(bUpdate) {}
			~UpdateChanger() { pActivationInfo->pGraph->SetRegularlyUpdated(pActivationInfo->myID, bUpdate); }
			SActivationInfo* pActivationInfo;
			bool             bUpdate;
		};

		UpdateChanger updateChanger(pActInfo, false);

		if (IEntity* pEntity = pActInfo->pEntity)
		{
			if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
			{
				int32 jointId = GetDesiredJointId(pActInfo, pCharacter);
				const bool bEnabled = GetPortBool(pActInfo, eInputPort_Enabled);
				updateChanger.bUpdate = (jointId != INVALID_JOINT_ID) && bEnabled;
			}
		}
	}

	void GetJointInfo(SActivationInfo* pActInfo)
	{
		if (IEntity* pEntity = pActInfo->pEntity)
		{
			if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
			{
				const IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
				const int32 jointID = rIDefaultSkeleton.GetJointIDByName(GetPortString(pActInfo, 0).c_str());
				if (jointID < 0)
				{
					CryLogAlways("[flow] Animations:BoneInfo Cannot find bone '%s' in character 0 of entity '%s'", GetPortString(pActInfo, 0).c_str(), pEntity->GetName());
					return;
				}
				const ISkeletonPose* const pISkeletonPose = pCharacter->GetISkeletonPose();
				Matrix34 mat = Matrix34(pISkeletonPose->GetAbsJointByID(jointID));
				ActivateOutput(pActInfo, eOutputPort_LocalPosition, mat.GetTranslation());

				mat.OrthonormalizeFast();
				const Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(Quat(mat)));
				ActivateOutput(pActInfo, eOutputPort_LocalRotation, Vec3(RAD2DEG(angles)));

				Matrix34 matWorld = pEntity->GetSlotWorldTM(0) * mat;
				ActivateOutput(pActInfo, eOutputPort_WorldPosition, matWorld.GetTranslation());

				matWorld.OrthonormalizeFast();
				const Ang3 anglesWorld = Ang3::GetAnglesXYZ(Matrix33(Quat(matWorld)));
				ActivateOutput(pActInfo, eOutputPort_WorldRotation, Vec3(RAD2DEG(anglesWorld)));
			}
		}
	}

	void OnUpdate(SActivationInfo* pActInfo)
	{
		GetJointInfo(pActInfo);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				OnChange(pActInfo);
				const bool enabled = GetPortBool(pActInfo, eInputPort_Enabled);
				if (!enabled && IsPortActive(pActInfo, eInputPort_Get))
				{
					GetJointInfo(pActInfo);
				}
			}
			break;

		case eFE_Initialize:
			OnChange(pActInfo);
			break;

		case eFE_Update:
			OnUpdate(pActInfo);
			break;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class CPlayAnimation_Node : public CFlowBaseNode<eNCT_Instanced>
{
	uint32 m_token;
	uint32 m_layer;

	bool   m_firedAlmostDone;
	bool   m_bForcedUpdateActivate;
	bool   m_manualAnimationControlledMovement;
	bool   m_bLooping;

	float  m_almostDonePercentage;
	float  m_playbackSpeedMultiplier;

public:

	enum EInputs
	{
		IN_START,
		IN_STOP,
		IN_ANIMATION,
		IN_BLEND_TIME,
		IN_LAYER,
		IN_LOOP,
		IN_REPEAT_LAST_FRAME,
		IN_FORCE_UPDATE,
		IN_PAUSE_ANIM_GRAPH,
		IN_CONTROL_MOVEMENT,
		IN_ALMOST_DONE_PERCENTAGE,
		IN_PLAYBACK_SPEED_MULTIPLIER,
	};

	enum EOutputs
	{
		OUT_DONE,
		OUT_ALMOST_DONE,
	};

	CPlayAnimation_Node(SActivationInfo* pActInfo)
	{
		m_firedAlmostDone = false;
		m_bForcedUpdateActivate = false;
		m_bLooping = false;
		m_manualAnimationControlledMovement = false;
		m_token = 0;
		m_layer = 0;
		m_almostDonePercentage = 0.85f;
		m_playbackSpeedMultiplier = 1.0f;
	};

	~CPlayAnimation_Node()
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CPlayAnimation_Node(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_token", m_token);
		ser.Value("m_firedAlmostDone", m_firedAlmostDone);
		ser.Value("m_bLooping", m_bLooping);
		ser.Value("m_bForcedUpdateActivate", m_bForcedUpdateActivate);
		ser.Value("m_manualAnimationControlledMovement", m_manualAnimationControlledMovement);
		ser.Value("m_layer", m_layer);
		ser.Value("m_almostDonePercentage", m_almostDonePercentage);
		ser.Value("m_playbackSpeedMultiplier", m_playbackSpeedMultiplier);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Start", _HELP("Starts the animation")),
			InputPortConfig_Void("Stop", _HELP("Stops the animation")),
			InputPortConfig<string>("anim_Animation", _HELP("Animation name"), 0, _UICONFIG("ref_entity=entityId")),
			InputPortConfig<float>("BlendInTime", 0.2f, _HELP("Blend in time")),
			InputPortConfig<int>("Layer", _HELP("Layer in which to play the animation (0-15).\nFullbody Animations should be played in layer 0.")),
			InputPortConfig<bool>("Loop", _HELP("When True animation will loop and will never stop")),
			InputPortConfig<bool>("StayOnLastFrame", _HELP("When True animation will not reset to the first frame after it finished. Ignored when 'Loop' is true.")),
			InputPortConfig<bool>("ForceUpdate", false, _HELP("When True animation will play even if not visible")),
			InputPortConfig<bool>("PauseAnimGraph", false, _HELP("Deprecated, this input has no effect")),
			InputPortConfig<bool>("ControlMovement", false, _HELP("When True this animation will control the entities movement")),
			InputPortConfig<float>("AlmostDonePercentage", m_almostDonePercentage, _HELP("Normalised percentage of animation progress at which Almost Done output will trigger, values between 0.05 & 0.95")),
			InputPortConfig<float>("PlaybackSpeedMultiplier", m_playbackSpeedMultiplier, _HELP("Speed multiplier at which to play the animation")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Done",       _HELP("Send an event when animation is finished")),
			OutputPortConfig_Void("AlmostDone", _HELP("Send an event when animation is almost finished - can be used to enhance blending to a different PlayAnimation node.")),
			{ 0 }
		};

		config.sDescription = _HELP("Plays an Animation on this character's skeleton.");
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
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
				if (IsPortActive(pActInfo, IN_START))
				{
					StartAnimation(pActInfo);
				}
				else if (IsPortActive(pActInfo, IN_STOP))
				{
					StopAnimation(pActInfo);
				}
			}
			break;

		case eFE_Update:
			{
				Update(pActInfo);
			}
			break;
		}
		;
	};

private:

	uint32 ChooseToken(ICharacterInstance* pCharacter) const
	{
		ISkeletonAnim* pSkel = pCharacter->GetISkeletonAnim();
		uint32 maxToken = 0;
		for (int i = 0; i < pSkel->GetNumAnimsInFIFO(m_layer); i++)
			maxToken = max(pSkel->GetAnimFromFIFO(m_layer, i).GetUserToken(), maxToken);
		if (!maxToken) // choose a (hopefully safe default)
			maxToken = 0xffff0000;
		else
			maxToken += 10000;
		return maxToken;
	}

	void StartAnimation(SActivationInfo* pActInfo)
	{
		ICharacterInstance* pCharacterInstance = (pActInfo->pEntity != NULL) ? pActInfo->pEntity->GetCharacter(0) : NULL;
		if (pCharacterInstance)
		{
			CryCharAnimationParams aparams;
			aparams.m_fTransTime = GetPortFloat(pActInfo, IN_BLEND_TIME);

			m_almostDonePercentage = clamp_tpl(GetPortFloat(pActInfo, IN_ALMOST_DONE_PERCENTAGE), 0.05f, 0.95f);

			m_bLooping = GetPortBool(pActInfo, IN_LOOP);
			if (m_bLooping)
				aparams.m_nFlags |= CA_LOOP_ANIMATION;

			m_playbackSpeedMultiplier = GetPortFloat(pActInfo, IN_PLAYBACK_SPEED_MULTIPLIER);
			aparams.m_fPlaybackSpeed = m_playbackSpeedMultiplier;

			m_playbackSpeedMultiplier = GetPortFloat(pActInfo, IN_PLAYBACK_SPEED_MULTIPLIER);
			aparams.m_fPlaybackSpeed = m_playbackSpeedMultiplier;

			m_bForcedUpdateActivate = false;
			if (GetPortBool(pActInfo, IN_FORCE_UPDATE))
			{
				aparams.m_nFlags |= CA_FORCE_SKELETON_UPDATE;

				m_bForcedUpdateActivate = true;
				SetForceUpdate(pActInfo->pEntity->GetId(), true);
			}

			const bool animationMovementControl = GetPortBool(pActInfo, IN_CONTROL_MOVEMENT);
			const string& animation = GetPortString(pActInfo, IN_ANIMATION);

			m_layer = GetPortInt(pActInfo, IN_LAYER);
			m_layer = CLAMP(m_layer, 0, 15);          // safety checking on the layer number

			aparams.m_nLayerID = m_layer;
			aparams.m_nUserToken = ChooseToken(pCharacterInstance);
			aparams.m_nFlags |= (animationMovementControl && (m_layer == 0)) ? CA_FULL_ROOT_PRIORITY : 0;

			ISkeletonAnim* pISkeletonAnim = pCharacterInstance->GetISkeletonAnim();
			const bool bStarted = pISkeletonAnim->StartAnimation(animation, aparams);

			IScriptTable* pEntityScript = pActInfo->pEntity->GetScriptTable();
			const char* pFuncName = "OnFlowGraphAnimationStart";
			if (pEntityScript && pEntityScript->HaveValue(pFuncName))
				Script::CallMethod(pEntityScript, pFuncName, animation.c_str(), m_layer, GetPortBool(pActInfo, IN_LOOP));

			if (bStarted)
			{
				m_token = aparams.m_nUserToken;
			}
			else
			{
				//make sure game logic continues nevertheless
				ActivateOutput(pActInfo, OUT_ALMOST_DONE, SFlowSystemVoid());
				ActivateOutput(pActInfo, OUT_DONE, SFlowSystemVoid());
			}

			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			m_firedAlmostDone = false;

			if (animationMovementControl)
			{
				bool needsManualUpdate = (m_layer == 0);
				if (IGameObject* pGameObject = gEnv->pGameFramework->GetGameObject(pActInfo->pEntity->GetId()))
				{
					if (IAnimatedCharacter* pAnimatedCharacter = static_cast<IAnimatedCharacter*>(pGameObject->QueryExtension("AnimatedCharacter")))
					{
						needsManualUpdate = false;
					}
				}

				if (needsManualUpdate)
				{
					pISkeletonAnim->SetAnimationDrivenMotion(1);
					m_manualAnimationControlledMovement = true;
				}
			}
		}
	}

	void StopAnimation(SActivationInfo* pActInfo)
	{
		if (pActInfo->pEntity)
		{
			if (m_bForcedUpdateActivate)
			{
				m_bForcedUpdateActivate = false;
				SetForceUpdate(pActInfo->pEntity->GetId(), false);
			}

			ICharacterInstance* pCharacterInstance = pActInfo->pEntity->GetCharacter(0);
			if (pCharacterInstance != NULL)
			{
				ISkeletonAnim* pSkeletonAnimation = pCharacterInstance->GetISkeletonAnim();
				for (int i = 0; i < pSkeletonAnimation->GetNumAnimsInFIFO(m_layer); i++)
				{
					CAnimation& anim = pSkeletonAnimation->GetAnimFromFIFO(m_layer, i);
					if (anim.HasUserToken(m_token))
					{
						anim.ClearActivated(); // so removal will always work, maybe add a new method to IS
						pSkeletonAnimation->RemoveAnimFromFIFO(m_layer, i);
						break;
					}
				}
			}

			IScriptTable* pEntityScript = pActInfo->pEntity->GetScriptTable();
			const char* pFuncName = "OnFlowGraphAnimationStop";
			if (pEntityScript && pEntityScript->HaveValue(pFuncName))
				Script::CallMethod(pEntityScript, pFuncName);
		}
	}

	void Update(SActivationInfo* pActInfo)
	{
		bool tokenFound = false;
		bool almostDone = false;
		bool isAlmostDoneConnected = false;

		QuatT relativeAnimationMovement(IDENTITY, ZERO);
		ICharacterInstance* pCharacterInstance = m_token && pActInfo->pEntity ? pActInfo->pEntity->GetCharacter(0) : NULL;
		ISkeletonAnim* pSkeletonAnimation = NULL;
		if (pCharacterInstance)
		{
			pSkeletonAnimation = pCharacterInstance->GetISkeletonAnim();
			for (int i = 0; i < pSkeletonAnimation->GetNumAnimsInFIFO(m_layer); i++)
			{
				const CAnimation& animation = pSkeletonAnimation->GetAnimFromFIFO(m_layer, i);
				if (animation.HasUserToken(m_token))
				{
					tokenFound = true;
					const float animationTime = pSkeletonAnimation->GetAnimationNormalizedTime(&animation);
					almostDone = animationTime > m_almostDonePercentage;
					if (m_bLooping && !almostDone)
					{
						m_firedAlmostDone = false;
					}
				}
			}
			relativeAnimationMovement = pSkeletonAnimation->GetRelMovement();
		}

		if (almostDone && !m_firedAlmostDone)
		{
			ActivateOutput(pActInfo, OUT_ALMOST_DONE, SFlowSystemVoid());
			isAlmostDoneConnected = IsOutputConnected(pActInfo, OUT_ALMOST_DONE);
			m_firedAlmostDone = true;
		}
		else if (!tokenFound)
		{
			ActivateOutput(pActInfo, OUT_DONE, SFlowSystemVoid());

			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

			m_firedAlmostDone = false;

			if (pActInfo->pEntity != NULL)
			{
				if (m_bForcedUpdateActivate)
				{
					m_bForcedUpdateActivate = false;
					SetForceUpdate(pActInfo->pEntity->GetId(), false);
				}

				if (m_manualAnimationControlledMovement)
				{
					if (pSkeletonAnimation != NULL)
					{
						pSkeletonAnimation->SetAnimationDrivenMotion(0);
					}
					m_manualAnimationControlledMovement = false;
				}

				IScriptTable* pEntityScript = pActInfo->pEntity->GetScriptTable();
				const char* pFuncName = "OnFlowGraphAnimationEnd";
				if (pEntityScript && pEntityScript->HaveValue(pFuncName))
					Script::CallMethod(pEntityScript, pFuncName);
			}
		}

		// Update entity position if animation is running and controlled movement active.
		// If AlmostDone was triggered and AlmostDone output is connected, we assume that
		// a new animation was triggered.
		// There are situations when this assumption might fail and movement is not updated
		// properly, to get this fixed the controlled movement needs to happen outside of this
		// FG Node
		if (tokenFound && !isAlmostDoneConnected && m_manualAnimationControlledMovement && pActInfo->pEntity)
		{
			QuatT newWorldLocation = QuatT(pActInfo->pEntity->GetWorldTM()) * relativeAnimationMovement;
			newWorldLocation.q.Normalize();
			pActInfo->pEntity->SetWorldTM(Matrix34(newWorldLocation));
		}
	}

	void SetForceUpdate(EntityId entityId, bool bEnable)
	{
		IGameFramework* const pGameFramework = gEnv->pGameFramework;
		IGameObject* const pGameObject = pGameFramework ? pGameFramework->GetGameObject(entityId) : nullptr;
		if (pGameObject)
		{
			pGameObject->ForceUpdate(bEnable);
		}
	}
};


//////////////////////////////////////////////////////////////////////////
class CIsAnimPlaying_Node : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_CHECK,
		IN_CHECK_EACH_FRAME,
		IN_ANIMATION,
		IN_LAYER,
	};

	enum EOutputs
	{
		OUT_IS_PLAYING,
		OUT_IS_NOT_PLAYING,
		OUT_IS_TOP_OF_STACK,
	};

	CIsAnimPlaying_Node(SActivationInfo* pActInfo)   {}

	~CIsAnimPlaying_Node() {}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Check",             _HELP("Test now once whether this animation is playing")),
			InputPortConfig<bool>("CheckAlways",      _HELP("If true, will check each frame whether the anim is playing (costs processing time)")),
			InputPortConfig<string>("anim_Animation", _HELP("Animation name"),                                                                     0,   _UICONFIG("ref_entity=entityId")),
			InputPortConfig<int>("Layer",             _HELP("Which layer should this anim be playing on"),                                         0),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Playing",    _HELP("Triggers if animation was found on the layer")),
			OutputPortConfig_Void("NotPlaying", _HELP("Triggers if animation was not found on the layer")),
			OutputPortConfig_Void("TopOfStack", _HELP("Triggers when animation was found and it is top of the stack (meaning, not currently blending out)")),
			{ 0 }
		};
		config.sDescription = _HELP("Checks whether a specific Animation is playing");
		config.nFlags |= EFLN_TARGET_ENTITY;
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
				if (!pActInfo->pEntity)
					return;

				if (IsPortActive(pActInfo, IN_CHECK))
				{
					PerformAnimCheck(pActInfo);
				}

				bool checkEachFrame = GetPortBool(pActInfo, IN_CHECK_EACH_FRAME);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, checkEachFrame);

				break;
			}

		case eFE_Update:
			{
				if (!pActInfo->pEntity)
					return;

				PerformAnimCheck(pActInfo);

				bool checkEachFrame = GetPortBool(pActInfo, IN_CHECK_EACH_FRAME);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, checkEachFrame);
			}
			break;

		case eFE_Initialize:
			{
				bool checkEachFrame = GetPortBool(pActInfo, IN_CHECK_EACH_FRAME);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, checkEachFrame);
			}
			break;

		}
		;
	};

private:

	void PerformAnimCheck(SActivationInfo* pActInfo)
	{
		ICharacterInstance* pCharacter = pActInfo->pEntity->GetCharacter(0);
		const string& animName = GetPortString(pActInfo, IN_ANIMATION);

		if (pCharacter && !animName.empty())
		{
			int animLayer = GetPortInt(pActInfo, IN_LAYER);

			ISkeletonAnim* pISkeletonAnim = pCharacter->GetISkeletonAnim();

			int animID = pCharacter->GetIAnimationSet()->GetAnimIDByName(animName.c_str());

			int numAnims = pISkeletonAnim->GetNumAnimsInFIFO(animLayer);
			for (int i = 0; i < numAnims; ++i)
			{
				const CAnimation& anim = pISkeletonAnim->GetAnimFromFIFO(animLayer, i);
				int32 animInLayerID = anim.GetAnimationId();
				if (animInLayerID == animID)
				{
					ActivateOutput(pActInfo, OUT_IS_PLAYING, SFlowSystemVoid());

					// animation is playing, but is it also the top of the stack? (FIFO - last one in)
					// this is done this way because in theory the animation could be in the FIFO several times
					// so DO NOT optimize this to just check whether i == numAnims - 1 please
					const CAnimation& topAnim = pISkeletonAnim->GetAnimFromFIFO(animLayer, numAnims - 1);
					int32 topAnimId = topAnim.GetAnimationId();
					if (topAnimId == animID)
					{
						ActivateOutput(pActInfo, OUT_IS_TOP_OF_STACK, SFlowSystemVoid());
					}

					return;
				}
			}

		}

		// else it's apparently all false
		ActivateOutput(pActInfo, OUT_IS_NOT_PLAYING, SFlowSystemVoid());
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_LookAt : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_START,
		IN_STOP,
		IN_FOV,
		IN_BLEND,
		IN_TARGET,
		IN_TARGET_POS,
		IN_PLAYER,
	};
	CFlowNode_LookAt(SActivationInfo* pActInfo) {}
	~CFlowNode_LookAt() {};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("Start",      _HELP("Starts Look at ")),
			InputPortConfig_AnyType("Stop",       _HELP("Stops Look at")),
			InputPortConfig<float>("FieldOfView", 90,                       _HELP("LookAt Field of view (Degrees)")),
			InputPortConfig<float>("Blending",    1,                        _HELP("Blending with animation value")),
			InputPortConfig<EntityId>("Target",   (EntityId)0,              _HELP("Look at target entity")),
			InputPortConfig<Vec3>("TargetPos",    Vec3(0,                   0,                                       0),_HELP("Look at target position")),
			InputPortConfig<bool>("LookAtPlayer", false,                    _HELP("When true looks at player")),
			{ 0 }
		};
		config.sDescription = _HELP("Activates character Look At IK");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_ADVANCED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		Vec3 vTarget(0, 0, 0);
		float fFov = 0;
		ICharacterInstance* pCharacter = 0;
		if (pActInfo->pEntity && pActInfo->pInputPorts)
		{
			pCharacter = pActInfo->pEntity->GetCharacter(0);
			if (pCharacter)
			{
				// Update look IK on the character.
				if (!GetPortBool(pActInfo, IN_PLAYER))
				{
					vTarget = GetPortVec3(pActInfo, IN_TARGET_POS);
					IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)GetPortEntityId(pActInfo, IN_TARGET));
					if (pEntity)
						vTarget = pEntity->GetWorldPos();
				}
				else if (IActor* pActor = gEnv->pGameFramework->GetClientActor())
				{
					if (IMovementController* pMoveController = pActor->GetMovementController())
					{
						SMovementState movementState;
						pMoveController->GetMovementState(movementState);
						vTarget = movementState.eyePosition + GetPortVec3(pActInfo, IN_TARGET_POS);
					}
				}
				fFov = GetPortFloat(pActInfo, IN_FOV);
				GetPortFloat(pActInfo, IN_BLEND);
			}
		}
		/*
		    float lookIKBlends[5];
		    lookIKBlends[0] = 0.00f * fBlend;
		    lookIKBlends[1] = 0.00f * fBlend;
		    lookIKBlends[2] = 0.00f * fBlend;
		    lookIKBlends[3] = 0.10f * fBlend;
		    lookIKBlends[4] = 0.60f * fBlend;
		 */
		switch (event)
		{
		case eFE_Update:
			if (pActInfo->pEntity && pCharacter)
			{
				IGameObject* pGameObject = (IGameObject*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_USER);
				IMovementController* pMC = pGameObject ? pGameObject->GetMovementController() : NULL;
				if (pMC)
				{
					CMovementRequest mc;
					mc.SetLookStyle(LOOKSTYLE_DEFAULT);
					mc.SetLookTarget(vTarget);
					pMC->RequestMovement(mc);
				}
				else
				{
					IAnimationPoseBlenderDir* pIPoseBlenderLook = pCharacter->GetISkeletonPose()->GetIPoseBlenderLook();
					if (pIPoseBlenderLook)
					{
						pIPoseBlenderLook->SetState(1);
						pIPoseBlenderLook->SetFadeoutAngle(DEG2RAD(fFov));
						pIPoseBlenderLook->SetTarget(vTarget);
					}
				}
			}
			break;

		case eFE_Activate:
			if (pActInfo->pEntity)
			{
				if (IsPortActive(pActInfo, IN_START))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
				if (IsPortActive(pActInfo, IN_STOP))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					if (pCharacter)
					{
						// Turns off look ik.
						IGameObject* pGameObject = (IGameObject*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_USER);
						IMovementController* pMC = pGameObject ? pGameObject->GetMovementController() : NULL;
						if (pMC)
						{
							CMovementRequest mc;
							mc.ClearLookTarget();
							pMC->RequestMovement(mc);
						}
						else
						{
							IAnimationPoseBlenderDir* poseBlenderLook = pCharacter->GetISkeletonPose()->GetIPoseBlenderLook();
							if (poseBlenderLook)
							{
								poseBlenderLook->SetState(0);
								poseBlenderLook->SetTarget(vTarget);
								poseBlenderLook->SetFadeoutAngle(DEG2RAD(fFov));
							}
						}
					}
				}
			}
			break;

		case eFE_Initialize:
			break;
		}
		;
	};
};

class CFlowNode_StopAnimation : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_StopAnimation(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<SFlowSystemVoid>("Stop", _HELP("Stops animation playing"), _HELP("Stop!")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done", _HELP("Send an event when animation is finished")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (pActInfo->pEntity)
				if (ICharacterInstance* pCharacter = pActInfo->pEntity->GetCharacter(0))
					pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();
			ActivateOutput(pActInfo, 0, SFlowSystemVoid());
			break;
		}
	}
};

class CFlowNode_NoAiming : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_NoAiming(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<SFlowSystemVoid>("Enable", _HELP("Stops animation playing"), _HELP("Dont Aim!")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Done", _HELP("Send an event when animation is finished")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Controls the AnimationGraph of the attached entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (pActInfo->pEntity)
				if (IGameObject* pGameObject = (IGameObject*)pActInfo->pEntity->GetProxy(ENTITY_PROXY_USER))
					if (IMovementController* pMC = pGameObject->GetMovementController())
					{
						CMovementRequest mc;
						mc.SetNoAiming();
						pMC->RequestMovement(mc);
					}
			ActivateOutput(pActInfo, 0, SFlowSystemVoid());
			break;
		}
	}
};

// TODO: post VS2 we need to make a general system for this
class CFlowNode_SynchronizeTwoAnimations : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_SynchronizeTwoAnimations(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<EntityId>("Entity1",            _HELP("Entity1")),
			InputPortConfig<EntityId>("Entity2",            _HELP("Entity2")),
			InputPortConfig<string>("anim_Animation1",      _HELP("Animation1"),0,                                _UICONFIG("ref_entity=Entity1")),
			InputPortConfig<string>("anim_Animation2",      _HELP("Animation2"),0,                                _UICONFIG("ref_entity=Entity2")),
			InputPortConfig<float>("ResyncTime",            0.2f,               _HELP("ResyncTime")),
			InputPortConfig<float>("MaxPercentSpeedChange", 10,                 _HELP("MaxPercentSpeedChange")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.sDescription = _HELP("Synchronize two animations on two entities");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			break;
		case eFE_Update:
			Update(pActInfo);
			break;
		}
	}

private:
	void Update(SActivationInfo* pActInfo)
	{
		IEntity* pEnt1 = gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, 0));
		IEntity* pEnt2 = gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, 1));
		if (!pEnt1 || !pEnt2)
			return;
		ICharacterInstance* pChar1 = pEnt1->GetCharacter(0);
		ICharacterInstance* pChar2 = pEnt2->GetCharacter(0);
		if (!pChar1 || !pChar2)
			return;
		ISkeletonAnim* pSkel1 = pChar1->GetISkeletonAnim();
		ISkeletonAnim* pSkel2 = pChar2->GetISkeletonAnim();
		if (pSkel1->GetNumAnimsInFIFO(0) == 0 || pSkel2->GetNumAnimsInFIFO(0) == 0)
			return;
		CAnimation* pAnim1 = &pSkel1->GetAnimFromFIFO(0, 0);
		CAnimation* pAnim2 = &pSkel2->GetAnimFromFIFO(0, 0);
		if (pAnim1->GetAnimationId() != pChar1->GetIAnimationSet()->GetAnimIDByName(GetPortString(pActInfo, 2)))
			return;
		if (pAnim2->GetAnimationId() != pChar2->GetIAnimationSet()->GetAnimIDByName(GetPortString(pActInfo, 3)))
			return;

		float tm1 = pSkel1->GetAnimationNormalizedTime(pAnim1);
		float tm2 = pSkel2->GetAnimationNormalizedTime(pAnim2);
		if (tm2 < tm1)
		{
			std::swap(pAnim1, pAnim2);
			std::swap(tm1, tm2);
		}

		if (tm2 - 0.5f > tm1)
		{
			tm1 += 1.0f;
			std::swap(pAnim1, pAnim2);
			std::swap(tm1, tm2);
		}
		float catchupTime = GetPortFloat(pActInfo, 4);
		float gamma = (tm2 - tm1) / catchupTime;
		float pb2 = (-gamma + sqrtf(gamma * gamma + 4.0f)) / 2.0f;
		float pb1 = 1.0f / pb2;
		float maxPB = GetPortFloat(pActInfo, 5) / 100.0f + 1.0f;
		float minPB = 1.0f / maxPB;
		pb2 = clamp_tpl(pb2, minPB, maxPB);
		pb1 = clamp_tpl(pb1, minPB, maxPB);
		pAnim1->SetPlaybackScale(pb1);
		pAnim2->SetPlaybackScale(pb2);
	}
};

// TODO: post VS2 we use events for this
class CFlowNode_TriggerOnKeyTime : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_TriggerOnKeyTime(SActivationInfo* pActInfo, bool state = false) : m_state(state)
	{
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<string>("anim_Animation", _HELP("Animation1")),
			InputPortConfig<float>("TriggerTime",     0.2f,                _HELP("TriggerTime")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Trigger", _HELP("Send an event when animation is finished")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Synchronize two animations on two entities");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			break;
		case eFE_Update:
			Update(pActInfo);
			break;
		}
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pInfo)
	{
		return new CFlowNode_TriggerOnKeyTime(pInfo, m_state);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("state", m_state);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	void Update(SActivationInfo* pActInfo)
	{
		if (!pActInfo->pEntity)
		{
			m_state = false;
			return;
		}
		ICharacterInstance* pChar = pActInfo->pEntity->GetCharacter(0);
		if (!pChar)
		{
			m_state = false;
			return;
		}
		ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
		if (pSkel->GetNumAnimsInFIFO(0) < 1)
		{
			m_state = false;
			return;
		}
		const CAnimation& anim = pSkel->GetAnimFromFIFO(0, 0);
		if (anim.GetAnimationId() != pChar->GetIAnimationSet()->GetAnimIDByName(GetPortString(pActInfo, 0)))
		{
			m_state = false;
			return;
		}
		float triggerTime = GetPortFloat(pActInfo, 1);
		const float animTime = pSkel->GetAnimationNormalizedTime(&anim);
		if (animTime < triggerTime)
		{
			m_state = false;
		}
		else if (animTime > triggerTime + 0.5f)
		{
			m_state = false;
		}
		else if (!m_state)
		{
			ActivateOutput(pActInfo, 0, SFlowSystemVoid());
			m_state = true;
		}
	}

	bool m_state;
};

class CFlowNode_AttachmentControl : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_AttachmentControl(SActivationInfo* pActInfo)
		: m_bHide(false)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AttachmentControl(pActInfo); }

	enum INPUTS
	{
		EIP_Attachment = 0,
		EIP_Show,
		EIP_Hide
	};

	enum OUTPUTS
	{
		EOP_Shown = 0,
		EOP_Hidden
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<string>("Attachment", _HELP("Attachment"),           0, _UICONFIG("dt=attachment,ref_entity=entityId")),
			InputPortConfig_Void("Show",          _HELP("Show the attachment")),
			InputPortConfig_Void("Hide",          _HELP("Hide the attachment")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Shown",  _HELP("Triggered when Shown")),
			OutputPortConfig_Void("Hidden", _HELP("Triggered when Hidden")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("[CUTSCENE ONLY] Show/Hide Character Attachments.");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Initialize)
		{
			IAttachment* pAttachment = GetAttachment(pActInfo);
			if (!pAttachment)
				return;
			m_bHide = pAttachment->IsAttachmentHidden() != 0;
		}
		else if (event == eFE_Activate)
		{
			IAttachment* pAttachment = GetAttachment(pActInfo);
			if (!pAttachment)
				return;
			const bool bHide = IsPortActive(pActInfo, EIP_Hide);
			const bool bShow = IsPortActive(pActInfo, EIP_Show);
			if (bHide || bShow)
			{
				pAttachment->HideAttachment(bHide);
				ActivateOutput(pActInfo, bHide ? EOP_Hidden : EOP_Shown, true);
				m_bHide = bHide;
			}
		}
	}

	IAttachment* GetAttachment(SActivationInfo* pActInfo)
	{
		if (pActInfo->pEntity == 0)
			return NULL;

		ICharacterInstance* pChar = pActInfo->pEntity->GetCharacter(0);
		if (pChar == 0)
			return NULL;

		IAttachmentManager* pAttMgr = pChar->GetIAttachmentManager();
		if (pAttMgr == 0)
			return NULL;

		const string& attachment = GetPortString(pActInfo, EIP_Attachment);
		IAttachment* pAttachment = pAttMgr->GetInterfaceByName(attachment.c_str());
		return pAttachment;
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		IAttachment* pAttachment = GetAttachment(pActInfo);
		if (!pAttachment)
			return;

		ser.BeginGroup("FG_AttachmentControl");
		if (ser.IsWriting())
			m_bHide = pAttachment->IsAttachmentHidden() != 0;

		ser.Value("bHide", m_bHide);

		ser.EndGroup();

		if (ser.IsReading())
			pAttachment->HideAttachment(m_bHide);
	}

private:
	bool m_bHide;
};

//////////////////////////////////////////////////////////////////////////
// Start a cooperative animation
class CFlowNode_CooperativeAnimation : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		eIN_START,
		eIN_STOP,
		eIN_LOOP,
		eIN_FORCESTART,
		eIN_ADJUST_TO_TERRAIN,
		eIN_IGNORE_CHARACTERS_DEATH,
		//eIN_VERTICALCORRECTION,
		eIN_FIRST_ACTOR_DOESNT_COLLIDE,
		eIN_LOCATION,
		eIN_ROTATION,
		eIN_ALIGNMENTTYPE,
		eIN_TARGET_ENTITY01,
		eIN_SIGNALNAME01,
		eIN_SLIDETIME01,
		eIN_ALLOWHPHYSICS01,
		eIN_TARGET_ENTITY02,
		eIN_SIGNALNAME02,
		eIN_SLIDETIME02,
		eIN_ALLOWHPHYSICS02,
		eIN_TARGET_ENTITY03,
		eIN_SIGNALNAME03,
		eIN_SLIDETIME03,
		eIN_ALLOWHPHYSICS03,
		eIN_TARGET_ENTITY04,
		eIN_SIGNALNAME04,
		eIN_SLIDETIME04,
		eIN_ALLOWHPHYSICS04,
	};

	enum EOutputs
	{
		eOUT_FINISHED01,
		eOUT_FINISHED02,
		eOUT_FINISHED03,
		eOUT_FINISHED04,
		eOUT_FINISHEDALL,
	};

	CFlowNode_CooperativeAnimation(SActivationInfo* pActInfo)
	{
		m_bRunning = false;
		Reset();
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_CooperativeAnimation(pActInfo); }

	void                 GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports //_UICONFIG("enum_int:NoChange=0,Hide=-1,Show=1")
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Start",                  _HELP("Starts the Cooperative Animation")),
			InputPortConfig_Void("Stop",                   _HELP("Interrupts and Stops the animations (only needed in looping animations)")),
			InputPortConfig<bool>("Loop",                  false,                                                                            _HELP("Loop Animation (runs until stopped) (0=no loop, 1=loop)"),                                                                                                                                                                                                                                                                                                                                "Looping",   0),
			InputPortConfig<bool>("ForceStart",            true,                                                                             _HELP("Force Start Animation (safer)")),
			InputPortConfig<bool>("AdjustToTerrain",       true,                                                                             _HELP("Make sure the characters are on terrain level (usually safer, but could cause problems on animations played in underground passages within the terrain)")),
			InputPortConfig<bool>("IgnoreCharactersDeath", true,                                                                             _HELP("If false and any of the characters dies, it will stop the animation on all the characters involved")),
			//InputPortConfig<bool>( "VerticalCorrection", 1, _HELP("Tilts the characters on uneven ground to match slope" )),
			InputPortConfig<bool>("NoCollisionBetween",    true,                                                                             _HELP("If true, the first actor won't collide with the other actors involved on this action")),
			InputPortConfig<Vec3>("Location",              _HELP("Starts the animation at a specific position (needs alignment type)")),
			InputPortConfig<Vec3>("Rotation",              _HELP("Starts the animation at a specific rotation (needs alignment type)")),
			InputPortConfig<int>("Alignment",              0,                                                                                _HELP("Alignment Type\nWildMatch: moves both characters the least amount\nFirstActor: first actor can be rotated but not moved\nFirstActorNoRot: first actor can neither be moved nor rotated\nFirstActorPosition: Slides the characters so the first one is at the in Location specified position\nLocation: moves both character until the reference point of the animation is at Location"),  0,           _UICONFIG("enum_int:WildMatch=0,FirstActor=1,FirstActorNoRot=2,FirstActorPosition=3,Location=4")),
			InputPortConfig<EntityId>("Entity_01",         _HELP("First Actor"),                                                             "Entity_01",                                                                                                                                                                                                                                                                                                                                                                                     0),
			InputPortConfig<string>("AnimationName_01",    _HELP("Animation Name")),
			InputPortConfig<float>("SlideDuration_01",     0.2f,                                                                             _HELP("Time in seconds to slide this entity into position")),
			InputPortConfig<bool>("HorizPhysics1",         false,                                                                            _HELP("Prohibits this character from being pushed through walls etc (safer)"),                                                                                                                                                                                                                                                                                                                   "HPhysics1", 0),
			InputPortConfig<EntityId>("Entity_02",         _HELP("Second Actor"),                                                            "Entity_02",                                                                                                                                                                                                                                                                                                                                                                                     0),
			InputPortConfig<string>("AnimationName_02",    _HELP("Animation Name")),
			InputPortConfig<float>("SlideDuration_02",     0.2f,                                                                             _HELP("Time in seconds to slide this entity into position")),
			InputPortConfig<bool>("HorizPhysics2",         false,                                                                            _HELP("Prohibits this character from being pushed through walls etc (safer)"),                                                                                                                                                                                                                                                                                                                   "HPhysics2", 0),
			InputPortConfig<EntityId>("Entity_03",         _HELP("Third Actor"),                                                             "Entity_03",                                                                                                                                                                                                                                                                                                                                                                                     0),
			InputPortConfig<string>("AnimationName_03",    _HELP("Animation Name")),
			InputPortConfig<float>("SlideDuration_03",     0.2f,                                                                             _HELP("Time in seconds to slide this entity into position")),
			InputPortConfig<bool>("HorizPhysics3",         false,                                                                            _HELP("Prohibits this character from being pushed through walls etc (safer)"),                                                                                                                                                                                                                                                                                                                   "HPhysics3", 0),
			InputPortConfig<EntityId>("Entity_04",         _HELP("Fourth Actor"),                                                            "Entity_04",                                                                                                                                                                                                                                                                                                                                                                                     0),
			InputPortConfig<string>("AnimationName_04",    _HELP("Animation Name")),
			InputPortConfig<float>("SlideDuration_04",     0.2f,                                                                             _HELP("Time in seconds to slide this entity into position")),
			InputPortConfig<bool>("HorizPhysics4",         false,                                                                            _HELP("Prohibits this character from being pushed through walls etc (safer)"),                                                                                                                                                                                                                                                                                                                   "HPhysics4", 0),

			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Finished_01", _HELP("Activates when the first actor is done.")),
			OutputPortConfig_Void("Finished_02", _HELP("Activates when the second actor is done.")),
			OutputPortConfig_Void("Finished_03", _HELP("Activates when the third actor is done.")),
			OutputPortConfig_Void("Finished_04", _HELP("Activates when the fourth actor is done.")),
			OutputPortConfig_Void("Done",        _HELP("Activates when all actors are done.")),
			{ 0 }
		};

		//config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Starts a cooperative animation for two characters (including alignment)");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				// Get input port values
				EntityId iEntityID_01 = GetPortEntityId(pActInfo, eIN_TARGET_ENTITY01);
				EntityId iEntityID_02 = GetPortEntityId(pActInfo, eIN_TARGET_ENTITY02);
				EntityId iEntityID_03 = GetPortEntityId(pActInfo, eIN_TARGET_ENTITY03);
				EntityId iEntityID_04 = GetPortEntityId(pActInfo, eIN_TARGET_ENTITY04);

				if (IsPortActive(pActInfo, eIN_START))
				{
					if (m_bRunning)
					{
						// if the coop animation is already running, do nothing
						UpdateOutputs(pActInfo);
					}

					if (!m_bRunning)
					{
						// start coop animation

						// get the actors for both entities
						IActor* pActor01 = reinterpret_cast<IActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(iEntityID_01));
						IActor* pActor02 = reinterpret_cast<IActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(iEntityID_02));
						IActor* pActor03 = reinterpret_cast<IActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(iEntityID_03));
						IActor* pActor04 = reinterpret_cast<IActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(iEntityID_04));
						CRY_ASSERT(pActor01);
						if (!pActor01)
							return;
						//CRY_ASSERT(pActor02); // only one actor is really mandatory

						// get the animation signal name
						const string& sAnimName1 = GetPortString(pActInfo, eIN_SIGNALNAME01);
						const string& sAnimName2 = GetPortString(pActInfo, eIN_SIGNALNAME02);
						const string& sAnimName3 = GetPortString(pActInfo, eIN_SIGNALNAME03);
						const string& sAnimName4 = GetPortString(pActInfo, eIN_SIGNALNAME04);

						// get the rest of the parameters
						Vec3 vLocation = GetPortVec3(pActInfo, eIN_LOCATION);
						Vec3 vTemp = GetPortVec3(pActInfo, eIN_ROTATION);
						Ang3 qRotation(vTemp.x, vTemp.y, vTemp.z);
						bool bForceStart = GetPortBool(pActInfo, eIN_FORCESTART);
						bool bLooping = GetPortBool(pActInfo, eIN_LOOP);
						EAlignmentRef eAlignment = static_cast<EAlignmentRef>(GetPortInt(pActInfo, eIN_ALIGNMENTTYPE));
						static_assert(eAF_Max == 5, "Array size changed, code might need to be updated!");

						bool onlyOneChar = false;
						if (!pActor02 && !pActor03 && !pActor04)
						{
							if (eAlignment == eAF_FirstActorPosition)
							{
								// this is an exact positioning animation for only one Character
								onlyOneChar = true;
							}
							else
								return;
						}

						QuatT qtLocation;
						qtLocation.t = vLocation;
						qtLocation.q.SetIdentity();
						//qtLocation.q.NormalizeSafe();
						if (!vTemp.IsZero())
						{
							qtLocation.q.SetRotationXYZ(qRotation);
							qtLocation.q.NormalizeSafe();
						}

						// Get sliding times
						float slideTime01 = GetPortFloat(pActInfo, eIN_SLIDETIME01);
						float slideTime02 = GetPortFloat(pActInfo, eIN_SLIDETIME02);
						float slideTime03 = GetPortFloat(pActInfo, eIN_SLIDETIME03);
						float slideTime04 = GetPortFloat(pActInfo, eIN_SLIDETIME04);

						// Get physics settings
						bool allowHPhysics01 = GetPortBool(pActInfo, eIN_ALLOWHPHYSICS01);
						bool allowHPhysics02 = GetPortBool(pActInfo, eIN_ALLOWHPHYSICS02);
						bool allowHPhysics03 = GetPortBool(pActInfo, eIN_ALLOWHPHYSICS03);
						bool allowHPhysics04 = GetPortBool(pActInfo, eIN_ALLOWHPHYSICS04);

						SCooperativeAnimParams generalParams(bForceStart, bLooping, eAlignment, qtLocation);
						generalParams.bPreventFallingThroughTerrain = GetPortBool(pActInfo, eIN_ADJUST_TO_TERRAIN);
						generalParams.bIgnoreCharacterDeath = GetPortBool(pActInfo, eIN_IGNORE_CHARACTERS_DEATH);
						generalParams.bNoCollisionsBetweenFirstActorAndRest = GetPortBool(pActInfo, eIN_FIRST_ACTOR_DOESNT_COLLIDE);

						SCharacterParams charParams1(pActor01->GetAnimatedCharacter(), sAnimName1.c_str(), allowHPhysics01, slideTime01);
						charParams1.SetAllowHorizontalPhysics(false);
						if (!onlyOneChar)
						{
							TCharacterParams characterParams;
							characterParams.push_back(charParams1);

							if (pActor02)
							{
								SCharacterParams charParams2(pActor02->GetAnimatedCharacter(), sAnimName2.c_str(), allowHPhysics02, slideTime02);
								charParams2.SetAllowHorizontalPhysics(false);
								characterParams.push_back(charParams2);
							}
							if (pActor03)
							{
								SCharacterParams charParams3(pActor03->GetAnimatedCharacter(), sAnimName3.c_str(), allowHPhysics03, slideTime03);
								charParams3.SetAllowHorizontalPhysics(false);
								characterParams.push_back(charParams3);
							}
							if (pActor04)
							{
								SCharacterParams charParams4(pActor04->GetAnimatedCharacter(), sAnimName4.c_str(), allowHPhysics04, slideTime04);
								charParams4.SetAllowHorizontalPhysics(false);
								characterParams.push_back(charParams4);
							}

							m_bRunning = gEnv->pGameFramework->GetICooperativeAnimationManager()->StartNewCooperativeAnimation(characterParams, generalParams);
						}
						else
						{
							m_bRunning = gEnv->pGameFramework->GetICooperativeAnimationManager()->StartExactPositioningAnimation(charParams1, generalParams);
						}

						m_bStarted = true;

						// make sure I get updated regulary
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);

						if (m_bRunning)
						{
							m_iEntityID_01 = iEntityID_01;
							m_iEntityID_02 = iEntityID_02;
							m_iEntityID_03 = iEntityID_03;
							m_iEntityID_04 = iEntityID_04;
						}
						else
						{
							// starting failed
						}
					}
				}
				else if (IsPortActive(pActInfo, eIN_STOP))
				{
					if (m_bRunning)
					{
						// Stop running cooperative animation on these two characters
						StopAnimations(pActInfo);
					}

					m_bRunning = false;
					Reset();
				}
			}
			break;
		case eFE_Update:
			{
				UpdateOutputs(pActInfo);
			}
			break;
		}
	}

	void UpdateOutputs(SActivationInfo* pActInfo)
	{
		// Update the Outputs of whether this action has finished or not
		bool bActor1Busy = gEnv->pGameFramework->GetICooperativeAnimationManager()->IsActorBusy(m_iEntityID_01);
		bool bActor2Busy = gEnv->pGameFramework->GetICooperativeAnimationManager()->IsActorBusy(m_iEntityID_02);
		bool bActor3Busy = gEnv->pGameFramework->GetICooperativeAnimationManager()->IsActorBusy(m_iEntityID_03);
		bool bActor4Busy = gEnv->pGameFramework->GetICooperativeAnimationManager()->IsActorBusy(m_iEntityID_04);

		if (m_bStarted)
		{
			bool bFinishedAll = false;

			if (!bActor1Busy)
			{
				bFinishedAll = true;

				ActivateOutput(pActInfo, eOUT_FINISHED01, !bActor1Busy && m_bRunning);
			}

			if (!bActor2Busy && m_iEntityID_02)
			{
				ActivateOutput(pActInfo, eOUT_FINISHED02, !bActor2Busy && m_bRunning);
			}
			else
				bFinishedAll = bFinishedAll && (m_iEntityID_02 == 0);

			if (!bActor3Busy && m_iEntityID_03)
			{
				ActivateOutput(pActInfo, eOUT_FINISHED03, !bActor3Busy && m_bRunning);
			}
			else
				bFinishedAll = bFinishedAll && (m_iEntityID_03 == 0);

			if (!bActor4Busy && m_iEntityID_04)
			{
				ActivateOutput(pActInfo, eOUT_FINISHED04, !bActor4Busy && m_bRunning);
			}
			else
				bFinishedAll = bFinishedAll && (m_iEntityID_04 == 0);

			if (bFinishedAll)
			{
				ActivateOutput(pActInfo, eOUT_FINISHEDALL, bFinishedAll && m_bRunning);
			}
		}

		m_bRunning = bActor1Busy || bActor2Busy;

		if (!m_bRunning)
			Reset();
	}

	void StopAnimations(SActivationInfo* pActInfo)
	{
		// stop coop animation for all actors
		IActor* pActor01 = /*static_cast<CActor*>*/ (gEnv->pGameFramework->GetIActorSystem()->GetActor(m_iEntityID_01));
		CRY_ASSERT(pActor01);

		gEnv->pGameFramework->GetICooperativeAnimationManager()->StopCooperativeAnimationOnActor(pActor01->GetAnimatedCharacter());

		ActivateOutput(pActInfo, eOUT_FINISHED01, true);
		ActivateOutput(pActInfo, eOUT_FINISHED02, true);
		ActivateOutput(pActInfo, eOUT_FINISHED03, true);
		ActivateOutput(pActInfo, eOUT_FINISHED04, true);
		ActivateOutput(pActInfo, eOUT_FINISHEDALL, true);

		Reset();
	}

	void Reset()
	{
		m_bRunning = false;
		m_bStarted = false;
	}

	virtual void GetMemoryUsage(ICrySizer* s)  const
	{
		s->Add(*this);
	}

private:

	bool     m_bRunning;
	bool     m_bStarted;
	EntityId m_iEntityID_01;
	EntityId m_iEntityID_02;
	EntityId m_iEntityID_03;
	EntityId m_iEntityID_04;
};

REGISTER_FLOW_NODE("Animations:PlayCGA", CPlayCGA_Node);
REGISTER_FLOW_NODE("Animations:BoneInfo", CAnimationBoneInfo_Node);
REGISTER_FLOW_NODE("Animations:PlayAnimation", CPlayAnimation_Node);
REGISTER_FLOW_NODE("Animations:CheckAnimPlaying", CIsAnimPlaying_Node);
REGISTER_FLOW_NODE("Animations:CooperativeAnimation", CFlowNode_CooperativeAnimation);
REGISTER_FLOW_NODE("Animations:LookAt", CFlowNode_LookAt);

REGISTER_FLOW_NODE("Animations:StopAnimation", CFlowNode_StopAnimation);
REGISTER_FLOW_NODE("Animations:NoAiming", CFlowNode_NoAiming);
REGISTER_FLOW_NODE("Animations:SynchronizeTwoAnimations", CFlowNode_SynchronizeTwoAnimations);
REGISTER_FLOW_NODE("Animations:TriggerOnKeyTime", CFlowNode_TriggerOnKeyTime);
REGISTER_FLOW_NODE("Animations:AttachmentControl", CFlowNode_AttachmentControl);
