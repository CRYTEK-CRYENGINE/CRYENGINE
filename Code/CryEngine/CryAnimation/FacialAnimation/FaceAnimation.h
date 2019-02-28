// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FaceEffector.h"
#include "FaceState.h"
#include "FaceAnimSequence.h"
#include <CryInput/IJoystick.h>

struct ICharacterInstance;
class CDefaultSkeleton;
class CFacialInstance;
class CFacialModel;
class CPhonemesLibrary;
class CFaceState;

#define FACIAL_SEQUENCE_EXT "fsq"
#define FACIAL_JOYSTICK_EXT "joy"

//////////////////////////////////////////////////////////////////////////
// Currently active animation Channel.
// Channel references an effector, and apply a weight on this effector,
// during facial animation.
//////////////////////////////////////////////////////////////////////////
struct SFacialEffectorChannel
{
	enum EChannelStatus
	{
		STATUS_ONE,       // Channel is at full level.
		STATUS_FADE_IN,   // Channel is fading in.
		STATUS_FADE_OUT   // Channel is fading out.
	};
	_smart_ptr<CFacialEffector> pEffector; // Effector targeted by this channel.
	uint32                      nChannelId;

	EChannelStatus              status;
	// Blend params in/out times in ms.
	float                       fFadeTime;
	// Life time of this channel at one level.
	float                       fLifeTime;
	// the desired weight of the channel at full level.
	float                       fWeight;
	// current weight of channel.
	float                       fCurrWeight;
	// Channel balance.
	float                       fBalance;
	// time since start of the channel fading.
	CTimeValue                  startFadeTime;
	uint32                      bNotRemovable   : 1;
	uint32                      bPreview        : 1; // Preview channel.
	uint32                      bIgnoreLifeTime : 1; // Preview channel.
	uint32                      bTemporary      : 1; // Channel removed after one time.
	uint32                      bLipSync        : 1; // Channel created by lipsyncing.
	uint32                      nRepeatCount    : 8; // Number of times to go between fade in/fade out sequence.

	SFacialEffectorChannel()
	{
		bNotRemovable = false;
		bPreview = false;
		bIgnoreLifeTime = true;
		bTemporary = false;
		bLipSync = false;
		nChannelId = ~0;
		startFadeTime = 0.0f;
		status = STATUS_ONE;
		fFadeTime = 0;
		fCurrWeight = 0;
		fWeight = 0;
		fLifeTime = 0;
		nRepeatCount = 0;
		fBalance = 0.0f;
	}
	void StartFadeOut(CTimeValue time)
	{
		startFadeTime = time;
		status = STATUS_FADE_OUT;
		fWeight = fCurrWeight; // Start fading from the current channel weight.
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};

//////////////////////////////////////////////////////////////////////////
class CFacialAnimationContext : public _i_reference_target_t
{
public:
	CFacialAnimationContext(CFacialInstance* pFace);

	CFacialInstance* GetInstance() { return m_pFace; }

	// Creates a new facial animation channel,
	// Returns Channel id.
	uint32 StartChannel(SFacialEffectorChannel& channel);
	// Stop channel with blending.
	void   StopChannel(uint32 nChannelId, float fFadeTime = 0);
	// Removes channel from the list.
	void   RemoveChannel(uint32 nChannelId);
	void   RemoveAllChannels();

	// Set a new weight for the currently playing channel id.
	void                    SetChannelWeight(uint32 nChannelId, float fWeight);
	SFacialEffectorChannel* GetChannel(uint32 nChannelId);

	// Set the multiplier for a morph target when the target is used in lipsyncing
	// (should only be called by CFacialAnimSequence).
	void SetLipsyncStrength(int index, float lipsyncStrength) { m_lipsyncStrength[index] = lipsyncStrength; }

	void UpdatePlayingSequences(const QuatTS& rAnimLocationNext);
	bool Update(CFaceState& faceState, const QuatTS& rAnimLocationNext);

	void ApplyEffector(CFaceState& faceState, CFacialEffector* pEffector, float fWeight, float fBalance, bool bIsLipSync);

	//////////////////////////////////////////////////////////////////////////
	// Sequence playback.
	//////////////////////////////////////////////////////////////////////////
	void PlaySequence(CFacialAnimSequence* pSequence, bool bExclusive = false, bool bLooping = false);
	void StopSequence(CFacialAnimSequence* pSequence);
	void PauseSequence(CFacialAnimSequence* pSequence, bool bPaused);
	bool IsPlayingSequence(CFacialAnimSequence* pSequence);
	void StopAllSequences();
	void SeekSequence(IFacialAnimSequence* pSequence, float fTime);

	// Remove all channels with preview flag.
	void   RemoveAllPreviewChannels();

	uint32 FadeInChannel(IFacialEffector* pEffector, float fWeight, float fFadeTime, float fLifeTime = 0, int nRepeatCount = 0);

	void   TemporarilyEnableAdditionalBoneRotationSmoothing();
	float  GetBoneRotationSmoothRatio();

	void   GetMemoryUsage(ICrySizer* pSizer) const;

private:
	void ResetFaceState(CFaceState& faceState);
	void AverageFaceState(CFaceState& faceState);
	void AnimatePlayingSequences(const QuatTS& rAnimLocationNext);

private:
	CTimeValue m_time; // Current animation time.

	// Channels that are currently participating in the facial animation.
	std::vector<SFacialEffectorChannel> m_channels;

	int                m_nTotalAppliedEffectors;
	// Array of the same size as face state.
	std::vector<int>   m_effectorsPerIndex;
	std::vector<float> m_lipsyncStrength;

	CFacialInstance*   m_pFace;
	CFacialModel*      m_pFacialModel;

	uint32             m_nLastChannelId;

	// Currently playing sequences.
	struct SPlayingSequence
	{
		_smart_ptr<CFacialAnimSequence>         pSequence;
		_smart_ptr<CFacialAnimSequenceInstance> pSequenceInstance;
		Range      timeRange;
		float      playTime;
		CTimeValue startTime;

		// weight of sequence.
		float fWeight;
		float fCurrWeight;

		bool  bLooping;
		bool  bPaused;

		void  GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(this, sizeof(*this));
			pSizer->AddObject(pSequence);
			pSizer->AddObject(pSequenceInstance);
		};
	};
	std::vector<SPlayingSequence> m_sequences;

	string                        m_debugText;

	bool                          m_bForceLastUpdate;

	float                         m_fBoneRotationSmoothingRemainingTime;
};

//////////////////////////////////////////////////////////////////////////
class CFacialAnimation : public IFacialAnimation, IJoystickContext
{
public:
	CFacialAnimation();
	~CFacialAnimation();

	//////////////////////////////////////////////////////////////////////////
	// IFacialAnimation implementation
	//////////////////////////////////////////////////////////////////////////
	virtual IPhonemeLibrary*         GetPhonemeLibrary();
	virtual void                     ClearAllCaches();
	virtual IFacialEffectorsLibrary* CreateEffectorsLibrary();
	virtual void                     ClearEffectorsLibraryFromCache(const char* filename);
	virtual IFacialEffectorsLibrary* LoadEffectorsLibrary(const char* filename);
	virtual IFacialAnimSequence*     CreateSequence();
	virtual void                     ClearSequenceFromCache(const char* filename);
	virtual IFacialAnimSequence*     LoadSequence(const char* filename, bool bNoWarnings = false, bool addToCache = true);
	virtual IFacialAnimSequence*     StartStreamingSequence(const char* filename);
	virtual IFacialAnimSequence*     FindLoadedSequence(const char* filename) const;

	virtual CFaceIdentifierHandle    CreateIdentifierHandle(const char* identifierString);

	//////////////////////////////////////////////////////////////////////////
	// IJoystickContext implementation
	//////////////////////////////////////////////////////////////////////////
	virtual IJoystickContext* GetJoystickContext();
	virtual IJoystick*        CreateJoystick(uint64 id);
	virtual IJoystickSet*     CreateJoystickSet();
	virtual IJoystickSet*     LoadJoystickSet(const char* filename, bool bNoWarnings);

	virtual IJoystickChannel* CreateJoystickChannel(IFacialAnimChannel* pChannel);
	virtual void              BindJoystickSetToSequence(IJoystickSet* pJoystickSet, IFacialAnimSequence* pSequence);

	//////////////////////////////////////////////////////////////////////////
	void RegisterLib(CFacialEffectorsLibrary* pLib);
	void UnregisterLib(CFacialEffectorsLibrary* pLib);

	void RenameAnimSequence(CFacialAnimSequence* pSeq, const char* sNewName, bool addToCache = true);

	void OnSequenceStreamed(IFacialAnimSequence* pSequence, bool wasSuccess);

	void GetMemoryUsage(ICrySizer* pSizer) const;

private:
	typedef std::map<string, _smart_ptr<IFacialAnimSequence>, stl::less_stricmp<string>> SeqMap;
	SeqMap                                m_loadedSequences;
	std::vector<CFacialEffectorsLibrary*> m_effectorLibs;
	CPhonemesLibrary*                     m_pPhonemeLibrary;
};
