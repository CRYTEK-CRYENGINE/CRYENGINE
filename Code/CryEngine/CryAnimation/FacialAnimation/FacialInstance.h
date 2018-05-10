// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FaceEffector.h"
#include "FaceState.h"
#include "FaceAnimation.h"
#include "EyeMovementFaceAnim.h"
#include "FacialModel.h"

class CCharInstance;

#if USE_FACIAL_ANIMATION_FRAMERATE_LIMITING

// Helper class to maintain a constant target frame rate
class CConstantFrameRateSkipper
{
	enum { SECOND_SLICES = 5 };
public:
	CConstantFrameRateSkipper() :
		m_currFrame(0),
		m_frameSkip(1),
		m_targetFrameRate(0),
		m_lastSecondSliceUpdates(0),
		m_enabled(true)
	{
		m_lastTime = gEnv->pTimer->GetAsyncCurTime();
	}
	void SetTargetFramerate(uint32 frameRate)
	{
		m_targetFrameRate = frameRate;
	}
	bool UpdateDue()
	{
		CTimeValue currTime = gEnv->pTimer->GetAsyncCurTime();
		f32 timeDelta = (currTime - m_lastTime).GetSeconds();

		if (timeDelta > 1.0f / SECOND_SLICES)
		{
			uint32 fps = m_lastSecondSliceUpdates * SECOND_SLICES;

			if (m_targetFrameRate < fps)
				m_frameSkip++;
			if (m_targetFrameRate > fps)
				m_frameSkip--;

			if (m_frameSkip <= 1)
				m_frameSkip = 1;

			m_lastSecondSliceUpdates = 0;
			m_lastTime = currTime;
		}

		m_currFrame++;
		if ((m_currFrame % m_frameSkip) == 0)
		{
			m_lastSecondSliceUpdates++;
			return true;
		}
		return !m_enabled;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	void SetEnabled(bool enabled)
	{
		m_enabled = enabled;
	}

private:
	uint32     m_targetFrameRate;
	uint32     m_currFrame;
	uint32     m_lastSecondSliceUpdates;
	uint32     m_frameSkip;
	CTimeValue m_lastTime;
	bool       m_enabled;
};

#endif

//////////////////////////////////////////////////////////////////////////
// Instance of facial animation model.
//////////////////////////////////////////////////////////////////////////
class CFacialInstance : public IFacialInstance, public _i_reference_target_t
{
public:
	CFacialInstance(CFacialModel* pDefaultSkeleton, CCharInstance* pChar);

	//////////////////////////////////////////////////////////////////////////
	// IFacialInstance interface
	//////////////////////////////////////////////////////////////////////////
	virtual IFacialModel*        GetFacialModel();
	virtual IFaceState*          GetFaceState();

	virtual uint32               StartEffectorChannel(IFacialEffector* pEffector, float fWeight, float fFadeTime, float fLifeTime = 0, int nRepeatCount = 0);
	virtual void                 StopEffectorChannel(uint32 nChannelID, float fFadeOutTime);
	virtual void                 PreviewEffector(IFacialEffector* pEffector, float fWeight, float fBalance = 0.0f);
	virtual void                 PreviewEffectors(IFacialEffector** pEffectors, float* fWeights, float* fBalances, int nEffectorsCount);

	virtual IFacialAnimSequence* LoadSequence(const char* sSequenceName, bool addToCache = true);
	virtual void                 PrecacheFacialExpression(const char* sSequenceName);
	virtual void                 PlaySequence(IFacialAnimSequence* pSequence, EFacialSequenceLayer layer, bool bExclusive = false, bool bLooping = false);
	virtual void                 StopSequence(EFacialSequenceLayer layer);
	virtual bool                 IsPlaySequence(IFacialAnimSequence* pSequence, EFacialSequenceLayer layer);
	virtual void                 PauseSequence(EFacialSequenceLayer layer, bool bPaused);
	virtual void                 SeekSequence(EFacialSequenceLayer layer, float fTime);
	virtual void                 LipSyncWithSound(uint32 nSoundId, bool bStop = false);
	virtual void                 OnExpressionLibraryLoad();

	virtual void                 EnableProceduralFacialAnimation(bool bEnable);
	virtual bool                 IsProceduralFacialAnimationEnabled() const;

	virtual void                 SetForcedRotations(int numForcedRotations, CFacialAnimForcedRotationEntry* forcedRotations);

	virtual void                 SetMasterCharacter(ICharacterInstance* pMasterInstance);
	virtual void                 TemporarilyEnableBoneRotationSmoothing();

	virtual void                 StopAllSequencesAndChannels();

	virtual void                 SetUseFrameRateLimiting(bool useFrameRateLimiting);
	//////////////////////////////////////////////////////////////////////////

	CFacialModel*            GetModel() const           { return m_pDefaultSkeleton; }
	CFaceState*              GetState()                 { return m_pFaceState; }
	CCharInstance*           GetMasterCharacter() const { return m_pMasterInstance; }

	CFacialAnimationContext* GetAnimContext()           { return m_pAnimContext; }
	IFacialEffector*         FindEffector(CFaceIdentifierHandle ident);
	IFacialEffector*         FindEffector(const char* ident);

	// Called every time facial animation needs to be recalculated.
	void UpdatePlayingSequences(float fDeltaTimeSec, const QuatTS& rAnimLocationNext);
	void ProcessWaitingLipSync();
	void Update(CFacialDisplaceInfo& pInfo, float fDeltaTimeSec, const QuatTS& rAnimLocationNext);

#if USE_FACIAL_ANIMATION_FRAMERATE_LIMITING
	void SetTargetFramerate(uint32 fps) { m_frameSkipper.SetTargetFramerate(fps); }
#endif

	void   ApplyProceduralFaceBehaviour(Vec3& vCamPos);
	void   UpdateProceduralFaceBehaviour();

	uint32 SizeOfThis()
	{
		uint32 size = 0;
		size += sizeofVector(m_forcedRotations);
		if (m_pFaceState)
			size += m_pFaceState->SizeOfThis();

		return size;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const;
private:
	void Reset();
	void UpdateCurrentSequence(IFacialAnimSequence* pPreviousSequence);

	struct LayerInfo
	{
		LayerInfo() : sequence(0), bExclusive(false), bLooping(false) {}
		LayerInfo(IFacialAnimSequence* sequence, bool bExclusive, bool bLooping)
			: sequence(sequence), bExclusive(bExclusive), bLooping(bLooping) {}

		_smart_ptr<IFacialAnimSequence> sequence;
		bool                            bExclusive;
		bool                            bLooping;
		void                            GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(this, sizeof(*this));
		};
	};

	LayerInfo m_layers[eFacialSequenceLayer_COUNT];
	int       m_currentLayer;

#if USE_FACIAL_ANIMATION_FRAMERATE_LIMITING
	CConstantFrameRateSkipper m_frameSkipper;
#endif

	CCharInstance*                      m_pMasterInstance; // == m_pCharacter, unless char is attachment, then master attachment.
	_smart_ptr<CFacialModel>            m_pDefaultSkeleton;

	_smart_ptr<CFaceState>              m_pFaceState;
	_smart_ptr<CFacialAnimationContext> m_pAnimContext;
	_smart_ptr<CEyeMovementFaceAnim>    m_pEyeMovement;

	class CLipSync
	{
	public:
		CLipSync() : m_soundID(-1) {}
		void StartWaiting(IFacialAnimSequence* pSeq, int32 soundID) { m_pSequence = pSeq; m_soundID = soundID; }
		bool IsWaiting() const                                      { return (m_soundID >= 0); };
		void Done()                                                 { m_soundID = -1; m_pSequence = NULL; };
		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(m_pSequence);
			pSizer->AddObject(m_soundID);
		};
		int32                GetSoundID() const      { return m_soundID; }
		IFacialAnimSequence* GetAnimSequence() const { return m_pSequence; }
	private:
		_smart_ptr<IFacialAnimSequence> m_pSequence;
		int32                           m_soundID;
	};

	CLipSync m_waitingLipSync;

	//////////////////////////////////////////////////////////////////////////
	struct tProceduralFace
	{
		Vec3                        m_JitterEyesPositions[4];
		int                         m_nCurrEyesJitter;
		float                       m_fEyesJitterScale;
		float                       m_fLastJitterTime;
		float                       m_fJitterTimeChange;
		float                       m_fBlinkingTime;
		_smart_ptr<CFacialEffector> m_pBlink;
		bool                        m_bEyesBlinking;
		bool                        m_bEnabled;
		void                        GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(this, sizeof(*this));
		};
	};
	tProceduralFace                          m_tProcFace;

	DynArray<CFacialAnimForcedRotationEntry> m_forcedRotations;
};
