// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IANIMATIONGRAPH_H__
#define __IANIMATIONGRAPH_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAnimation/ICryAnimation.h>
#include <CrySystem/TimeValue.h>

class CCryName;
struct IGameObject;
struct IAnimatedCharacter;

const uint32 INVALID_ANIMATION_TOKEN = 0xffffffff;

enum EAnimationGraphPauser
{
	eAGP_FlowGraph,
	eAGP_TrackView,
	eAGP_StartGame,
	eAGP_Freezing,
	eAGP_PlayAnimationNode,
	eAGP_HitDeathReactions,
	eAGP_ActionController,
};

enum EAnimationGraphTriggerUser
{
	eAGTU_AI = 0,
	eAGTU_VehicleSystem
};

enum EMovementControlMethod
{
	// !!! WARNING: Update g_szMCMString in AnimationGraph.cpp !!!

	eMCM_Undefined           = 0,
	eMCM_Entity              = 1,
	eMCM_Animation           = 2,
	eMCM_DecoupledCatchUp    = 3,
	eMCM_ClampedEntity       = 4,
	eMCM_SmoothedEntity      = 5,
	eMCM_AnimationHCollision = 6,

	// !!! WARNING: Update g_szMCMString in AnimationGraph.cpp !!!

	eMCM_COUNT,
	eMCM_FF = 0xFF
};
AUTO_TYPE_INFO(EMovementControlMethod);

extern char* g_szMCMString[eMCM_COUNT];

enum EColliderMode
{
	// !!! WARNING: Update g_szColliderModeString in AnimationGraph.cpp !!!

	eColliderMode_Undefined = 0,
	eColliderMode_Disabled,
	eColliderMode_GroundedOnly,
	eColliderMode_Pushable,
	eColliderMode_NonPushable,
	eColliderMode_PushesPlayersOnly,
	eColliderMode_Spectator,

	// !!! WARNING: Update g_szColliderModeString in AnimationGraph.cpp !!!

	eColliderMode_COUNT,
	eColliderMode_FF = 0xFF
};
extern char* g_szColliderModeString[eColliderMode_COUNT];

AUTO_TYPE_INFO(EColliderMode);

enum EColliderModeLayer
{
	// !!! WARNING: Update g_szColliderModeLayerString in AnimationGraph.cpp !!!

	eColliderModeLayer_AnimGraph = 0,
	eColliderModeLayer_Game,
	eColliderModeLayer_Script,
	eColliderModeLayer_FlowGraph,
	eColliderModeLayer_Animation,
	eColliderModeLayer_ForceSleep,

	eColliderModeLayer_Debug,

	// !!! WARNING: Update g_szColliderModeLayerString in AnimationGraph.cpp !!!

	eColliderModeLayer_COUNT,
	eColliderModeLayer_FF = 0xFF
};

extern char* g_szColliderModeLayerString[eColliderModeLayer_COUNT];

enum EAnimationGraphUserData
{
	eAGUD_MovementControlMethodH = 0,
	eAGUD_MovementControlMethodV,
	eAGUD_ColliderMode,
	eAGUD_AnimationControlledView,
	eAGUD_AdditionalTurnMultiplier,
	eAGUD_NUM_BUILTINS
};

struct IAnimationGraphState;

struct IAnimationGraphStateListener;

typedef uint8 AnimationGraphInputID;

class CAnimationPlayerProxy
{
public:
	virtual ~CAnimationPlayerProxy(){}

	virtual bool StartAnimation(IEntity* entity, const char* szAnimName0, const CryCharAnimationParams& Params, float speedMultiplier = 1.0f)
	{
		ICharacterInstance* pICharacter = entity->GetCharacter(0);
		if (pICharacter)
		{
			ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();

			if (pISkeletonAnim->StartAnimation(szAnimName0, Params))
			{
				pISkeletonAnim->SetLayerBlendWeight(Params.m_nLayerID, speedMultiplier);
				return true;
			}
		}
		return false;
	}

	virtual bool StartAnimationById(IEntity* entity, int animId, const CryCharAnimationParams& Params, float speedMultiplier = 1.0f)
	{
		ICharacterInstance* pICharacter = entity->GetCharacter(0);
		if (pICharacter)
		{
			ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();

			if (pISkeletonAnim->StartAnimationById(animId, Params))
			{
				pISkeletonAnim->SetLayerBlendWeight(Params.m_nLayerID, speedMultiplier);
				return true;
			}
		}
		return false;
	}

	virtual bool StopAnimationInLayer(IEntity* entity, int32 nLayer, f32 BlendOutTime)
	{
		ICharacterInstance* pICharacter = entity->GetCharacter(0);
		ISkeletonAnim* pISkeletonAnim = pICharacter ? pICharacter->GetISkeletonAnim() : NULL;

		return pISkeletonAnim ? pISkeletonAnim->StopAnimationInLayer(nLayer, BlendOutTime) : false;
	}

	virtual bool RemoveAnimationInLayer(IEntity* entity, int32 nLayer, uint32 token)
	{
		ICharacterInstance* pICharacter = entity->GetCharacter(0);
		if (pICharacter)
		{
			ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();

			int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO(nLayer);
			for (int i = 0; i < nAnimsInFIFO; ++i)
			{
				const CAnimation& anim = pISkeletonAnim->GetAnimFromFIFO(nLayer, i);
				if (anim.HasUserToken(token))
				{
					return pISkeletonAnim->RemoveAnimFromFIFO(nLayer, i);
				}
			}
		}

		return false;
	}

	virtual float GetAnimationNormalizedTime(IEntity* pEntity, const CAnimation* pAnimation)
	{
		if (!pEntity)
			return -1.f;

		if (!pAnimation)
			return -1.f;

		ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0);
		if (!pCharacterInstance)
			return -1.f;

		ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim();
		if (!pSkeletonAnim)
			return -1.f;

		return pSkeletonAnim->GetAnimationNormalizedTime(pAnimation);
	}

	virtual bool SetAnimationNormalizedTime(IEntity* pEntity, CAnimation* pAnimation, float normalizedTime)
	{
		if (!pEntity)
			return false;

		if (!pAnimation)
			return false;

		ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0);
		if (!pCharacterInstance)
			return false;

		ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim();
		if (!pSkeletonAnim)
			return false;

		pSkeletonAnim->SetAnimationNormalizedTime(pAnimation, normalizedTime);
		return true;
	}

	virtual float GetTopAnimExpectedSecondsLeft(IEntity* entity, int32 layer)
	{
		ICharacterInstance* pICharacter = entity->GetCharacter(0);
		if (!pICharacter)
			return -1.f;

		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
		if (!pISkeletonAnim)
			return -1.f;

		const CAnimation* pAnim = GetTopAnimation(entity, layer);
		if (!pAnim)
			return -1.f;

		return (1.f - pISkeletonAnim->GetAnimationNormalizedTime(pAnim)) * pAnim->GetExpectedTotalDurationSeconds();
	}

	virtual float GetTopAnimNormalizedTime(IEntity* entity, int32 layer)
	{
		ICharacterInstance* pICharacter = entity->GetCharacter(0);
		if (!pICharacter)
			return -1.f;

		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
		if (!pISkeletonAnim)
			return -1.f;

		const CAnimation* pAnim = GetTopAnimation(entity, layer);
		if (!pAnim)
			return -1.f;

		return pISkeletonAnim->GetAnimationNormalizedTime(pAnim);
	}

	virtual const CAnimation* GetTopAnimation(IEntity* entity, int32 layer)
	{
		ICharacterInstance* pICharacter = entity->GetCharacter(0);
		if (pICharacter)
		{
			ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();

			int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO(layer);
			if (nAnimsInFIFO > 0)
			{
				return &pISkeletonAnim->GetAnimFromFIFO(layer, nAnimsInFIFO - 1);
			}
		}

		return NULL;
	}

	virtual CAnimation* GetAnimation(IEntity* entity, int32 layer, uint32 token)
	{
		ICharacterInstance* pICharacter = entity->GetCharacter(0);
		if (!pICharacter)
			return NULL;

		ISkeletonAnim* pISkeletonAnim = pICharacter->GetISkeletonAnim();
		if (!pISkeletonAnim)
			return NULL;

		if (token == INVALID_ANIMATION_TOKEN)
		{
			int nAnimsInFIFO = pISkeletonAnim->GetNumAnimsInFIFO(layer);
			if (nAnimsInFIFO == 0)
			{
				return NULL;
			}
			else
			{
				return &pISkeletonAnim->GetAnimFromFIFO(layer, 0);
			}
		}
		else
		{
			return pISkeletonAnim->FindAnimInFIFO(token, layer);
		}
	}

protected:
};

struct SAnimationTargetRequest
{
	SAnimationTargetRequest()
		: position(ZERO)
		, direction(FORWARD_DIRECTION)
		, startArcAngle(0.0f)
		, directionTolerance(gf_PI)
		, prepareRadius(3.0f)
		, startWidth(0.0f)
		, projectEnd(false)
		, navSO(false)
	{}
	Vec3  position;
	Vec3  direction;
	float startArcAngle;
	float directionTolerance;
	float prepareRadius;
	float startWidth;
	bool  projectEnd;

	// we allow bigger errors in start position while passing through
	// a smart object to avoid too much slowing down in front of it
	bool navSO;

	bool operator==(const SAnimationTargetRequest& rhs) const
	{
		static const float eps = 1e-3f;
		return position.GetSquaredDistance(rhs.position) < eps
		       && direction.Dot(rhs.direction) > (1.0f - eps)
		       && fabsf(startArcAngle - rhs.startArcAngle) < eps
		       && fabsf(directionTolerance - rhs.directionTolerance) < eps
		       && fabsf(prepareRadius - rhs.prepareRadius) < eps
		       && fabsf(startWidth - rhs.startWidth) < eps
		       && projectEnd == rhs.projectEnd
		       && navSO == rhs.navSO;
	}
	bool operator!=(const SAnimationTargetRequest& rhs) const
	{
		return !this->operator==(rhs);
	}
};

struct SAnimationTarget
{
	SAnimationTarget()
		: preparing(false)
		, activated(false)
		, doingSomething(false)
		, allowActivation(false)
		, notAiControlledAnymore(false)
		, isNavigationalSO(false)
		, maxRadius(0)
		, position(ZERO)
		, startWidth(0.0f)
		, positionWidth(0.0f)
		, positionDepth(0.0f)
		, orientationTolerance(0.0f)
		, orientation(IDENTITY)
		, activationTimeRemaining(0)
		, errorVelocity(ZERO)
		, errorRotationalVelocity(IDENTITY)
	{}
	uint32         preparing              : 1;
	uint32         activated              : 1;
	uint32         doingSomething         : 1;
	mutable uint32 allowActivation        : 1;
	mutable uint32 notAiControlledAnymore : 1;
	uint32         isNavigationalSO       : 1;
	float          maxRadius;
	Vec3           position;
	float          startWidth;
	float          positionWidth;
	float          positionDepth;
	float          orientationTolerance;
	Quat           orientation;
	float          activationTimeRemaining;
	Vec3           errorVelocity;
	Quat           errorRotationalVelocity;
};

//TODO: find a better place for this!!!

#define MAX_PREDICTION_PARAMS 6

// TODO: Rename to SMotionParameters
struct SPredictedCharacterStates
{
public:
	SPredictedCharacterStates()
	{
		m_numParams = 0;
		memset(m_motionParameter, 0, MAX_PREDICTION_PARAMS * sizeof(m_motionParameter[0]));
		memset(m_motionParameterID, 0, MAX_PREDICTION_PARAMS * sizeof(m_motionParameterID[0]));
	}

	bool        IsSet() const { return m_numParams > 0; }

	inline void Reset()
	{
		m_numParams = 0;
	}

	bool SetParam(EMotionParamID motionParameterID, float value)
	{
		int index = 0;
		while ((index < m_numParams) && (m_motionParameterID[index] != motionParameterID))
			++index;

		if (index == m_numParams)
		{
			if (index == MAX_PREDICTION_PARAMS)
				return false;

			++m_numParams;
		}

		m_motionParameter[index] = value;
		m_motionParameterID[index] = motionParameterID;

		return true;
	}

	bool GetParam(EMotionParamID motionParameterID, float& value) const
	{
		for (int index = 0; index < m_numParams; ++index)
		{
			if (m_motionParameterID[index] == motionParameterID)
			{
				value = m_motionParameter[index];
				return true;
			}
		}

		return false;
	}

	bool IsValid() const
	{
		bool isValid = true;
		for (int index = 0; index < m_numParams; ++index)
		{
			isValid = isValid && NumberValid(m_motionParameter[index]);
		}

		return isValid;
	}

private:
	f32   m_motionParameter[MAX_PREDICTION_PARAMS];
	uint8 m_motionParameterID[MAX_PREDICTION_PARAMS];
	uint8 m_numParams;
};

typedef uint32 TAnimationGraphQueryID;

struct IAnimationGraphAuxillaryInputs
{
	typedef AnimationGraphInputID InputID;

	virtual ~IAnimationGraphAuxillaryInputs(){}
	virtual IAnimationGraphState* GetState() = 0;
	virtual void                  SetInput(InputID, float) = 0;
	virtual void                  SetInput(InputID, int) = 0;
	virtual void                  SetInput(InputID, const char*) = 0;

	template<class T>
	inline void SetInput(const char* name, T value);
};

typedef IAnimationGraphAuxillaryInputs IAnimationSpacialTrigger;

struct IAnimationGraphExistanceQuery : public IAnimationGraphAuxillaryInputs
{
	/// Execute the query.
	virtual bool       Complete() = 0;
	/// Returns animation length after query is Complete() and successful. Otherwise returns CTimeValue(0).
	virtual CTimeValue GetAnimationLength() const = 0;
	virtual void       Reset() = 0;
	virtual void       Release() = 0;
};

struct IAnimationGraphState
{
public:
	typedef AnimationGraphInputID InputID;

	virtual ~IAnimationGraphState() {}

	// (see lower level versions below)
	template<class T>
	inline bool SetInput(const char* name, T value, TAnimationGraphQueryID* pQueryID = 0)
	{
		return SetInput(GetInputId(name), value, pQueryID);
	}
	// SetInputOptional is same as SetInput except that it will not set the default input value in case a non-existing value is passed
	inline bool SetInputOptional(const char* name, const char* value, TAnimationGraphQueryID* pQueryID = 0)
	{
		return SetInputOptional(GetInputId(name), value, pQueryID);
	}
	inline void QueryChangeInput(const char* name, TAnimationGraphQueryID* pQueryID)
	{
		QueryChangeInput(GetInputId(name), pQueryID);
	}
	inline void LockInput(const char* name, bool locked)
	{
		LockInput(GetInputId(name), locked);
	}

	// recurse setting. query mechanism needs to be wrapped by wrapper.
	// Associated QueryID will be given to QueryComplete when ALL layers supporting the input have reached their matching states.
	// wrapper generates it's own query IDs which are associated to a bunch of sub IDs with rules for how to handle the sub IDs into wrapped IDs.
	virtual bool        SetInput(InputID, float, TAnimationGraphQueryID * pQueryID = 0) = 0;
	virtual bool        SetInput(InputID, int, TAnimationGraphQueryID * pQueryID = 0) = 0;
	virtual bool        SetInput(InputID, const char*, TAnimationGraphQueryID * pQueryID = 0) = 0;
	// SetInputOptional is same as SetInput except that it will not set the default input value in case a non-existing value is passed
	virtual bool        SetInputOptional(InputID, const char*, TAnimationGraphQueryID * pQueryID = 0) = 0;
	virtual void        ClearInput(InputID) = 0;
	virtual void        LockInput(InputID, bool locked) = 0;

	virtual bool        SetVariationInput(const char* name, const char* value) = 0;
	virtual bool        SetVariationInput(InputID inputID, const char* value) = 0;
	virtual const char* GetVariationInput(InputID inputID) const = 0;

	// assert all equal, use any (except if signalled, then return the one not equal to default, or return default of all default)
	virtual void GetInput(InputID, char*) const = 0;

	// get input from specific layer if layer index is valid for this state
	virtual void GetInput(InputID, char*, int layerIndex) const = 0;

	// AND all layers
	virtual bool IsDefaultInputValue(InputID) const = 0;

	// returns NULL if InputID is out of range
	virtual const char* GetInputName(InputID) const = 0;
	virtual const char* GetVariationInputName(InputID) const = 0;

	// When QueryID of SetInput (reached queried state) is emitted this function is called by the outside, by convention(verify!).
	// Remember which layers supported the SetInput query and emit QueryLeaveState QueryComplete when all those layers have left those states.
	virtual void QueryLeaveState(TAnimationGraphQueryID* pQueryID) = 0;

	// assert all equal, forward to all layers, complete when all have changed once (trivial, since all change at once via SetInput).
	// (except for signalled, forward only to layers which currently are not default, complete when all those have changed).
	virtual void QueryChangeInput(InputID, TAnimationGraphQueryID*) = 0;

	// Just register and non-selectivly call QueryComplete on all listeners (regardless of what ID's they are actually interested in).
	virtual void AddListener(const char* name, IAnimationGraphStateListener* pListener) = 0;
	virtual void RemoveListener(IAnimationGraphStateListener* pListener) = 0;

	// Not used
	virtual bool DoesInputMatchState(InputID) const = 0;

	// TODO: This should be turned into registered callbacks or something instead (look at AnimationGraphStateListener).
	// Use to access the SelectLocomotionState() callback in CAnimatedCharacter.
	// Only set for fullbody, null for upperbody.
	virtual void SetAnimatedCharacter(class CAnimatedCharacter* animatedCharacter, int layerIndex, IAnimationGraphState* parentLayerState) = 0;

	// simply recurse
	virtual bool Update() = 0;
	virtual void Release() = 0;
	virtual void ForceTeleportToQueriedState() = 0;

	// simply recurse (will be ignored by each layer individually if state not found)
	virtual void PushForcedState(const char* state, TAnimationGraphQueryID* pQueryID = 0) = 0;

	// simply recurse
	virtual void ClearForcedStates() = 0;

	// same as GetInput above
	virtual float GetInputAsFloat(InputID inputId) = 0;

	// wrapper generates it's own input IDs for the union of all inputs in all layers, and for each input it maps to the layer specific IDs.
	virtual InputID GetInputId(const char* input) = 0;
	virtual InputID GetVariationInputId(const char* variationInputName) const = 0;

	// simply recurse (preserve order), and don't forget to serialize the wrapper stuff, ID's or whatever.
	virtual void Serialize(TSerialize ser) = 0;

	// simply recurse
	virtual void SetAnimationActivation(bool activated) = 0;
	virtual bool GetAnimationActivation() = 0;

	// Concatenate all layers state names with '+'. Use only fullbody layer state name if upperbody layer is not allowed/mixed.
	virtual const char* GetCurrentStateName() = 0;

	// don't expose (should only be called on specific layer state directly, by AGAnimation)
	//virtual void ForceLeaveCurrentState() = 0;
	//virtual void InvalidateQueriedState() = 0;

	// simply recurse
	virtual void Pause(bool pause, EAnimationGraphPauser pauser, float fOverrideTransTime = -1.0f) = 0;

	// is the same for all layers (equal assertion should not even be needed)
	virtual bool IsInDebugBreak() = 0;

	// don't expose (not used) (if used outside AGAnimation, specify layer)
	//virtual CTimeValue GetAnimationLength() = 0;

	// don't expose (only used by AGOutput)
	//virtual void SetOutput( int id ) = 0;
	//virtual void ClearOutput( int id ) = 0;

	// find highest layer that has output id, or null (this allows upperbody to override fullbody).
	// Use this logic when calling SetOutput on listeners.
	virtual const char* QueryOutput(const char* name) = 0;

	// Don't expose (only used on specific layer in AGParams)
	//virtual void SetCurrentStructure( const CCryName& ) = 0;
	//virtual const CCryName& GetCurrentStructure() = 0;

	// don't expose (exact positioning uses fullbody layer only, and AGAnimation operates on specific layer already)
	//virtual uint32 GetCurrentToken() = 0;

	// Exact positioning: Forward to fullbody layer only (hardcoded)
	virtual IAnimationSpacialTrigger* SetTrigger(const SAnimationTargetRequest& req, EAnimationGraphTriggerUser user, TAnimationGraphQueryID* pQueryStart, TAnimationGraphQueryID* pQueryEnd) = 0;
	virtual void                      ClearTrigger(EAnimationGraphTriggerUser user) = 0;
	virtual const SAnimationTarget*   GetAnimationTarget() = 0;
	virtual bool                      HasAnimationTarget() const = 0;
	virtual bool                      IsUpdateReallyNecessary() = 0;

	// Creates an object you can use to test whether a specific combination of inputs will select a state
	// (and to get a bit more information about that state)
	virtual IAnimationGraphExistanceQuery* CreateExistanceQuery() = 0;
	virtual IAnimationGraphExistanceQuery* CreateExistanceQuery(int layer) = 0;

	// simply recurse
	virtual void Reset() = 0;

	// simply recurse (hurry all layers, let them hurry independently where they can)
	virtual void Hurry() = 0;

	// simply recurse (first person skippable states are skipped independently by each layer)
	virtual void SetFirstPersonMode(bool on) = 0;

	// Removed (remove this from original interface as well., including SetIWeights
	//virtual uint16 GetBlendSpaceWeightFlags() = 0;
	//virtual void  SetBlendSpaceWeightFlags(uint16 flags) = 0;

	// simply recurse (will add all layer's containers to the sizer)
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	// the wrapper simply returns false
	virtual bool IsMixingAllowedForCurrentState() const = 0;

	// used by CAnimationGraphStates
	virtual bool IsSignalledInput(InputID intputId) const = 0;
};

template<class T>
inline void IAnimationGraphAuxillaryInputs::SetInput(const char* name, T value)
{
	SetInput(GetState()->GetInputId(name), value);
}

struct IAnimationGraphStateListener
{
	virtual ~IAnimationGraphStateListener(){}
	virtual void SetOutput(const char* output, const char* value) = 0;
	virtual void QueryComplete(TAnimationGraphQueryID queryID, bool succeeded) = 0;
	virtual void DestroyedState(IAnimationGraphState*) = 0;
};
#endif
