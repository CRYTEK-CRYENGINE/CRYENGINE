// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QtCore/QObject>
#include <vector>
#include "AnimationReference.h"
#include "PlaybackLayers.h"
#include <CryString/CryFixedString.h>

struct IBackgroundTask;
struct ICharacterInstance;
struct IAnimationSet;
struct SAnimSettings;

namespace CharacterTool
{
using std::vector;

struct AnimationCompressionInfo
{
	uint64 uncompressedSize;
	uint64 compressedPreviewSize;
	uint64 compressedCafSize;

	AnimationCompressionInfo()
		: uncompressedSize(0)
		, compressedPreviewSize(0)
		, compressedCafSize(0)
	{
	}
};

class InterlockedReferenceTarget
{
public:
	InterlockedReferenceTarget() : m_cnt(0) {}
	virtual ~InterlockedReferenceTarget() {}
	int AddRef() { return CryInterlockedIncrement(&m_cnt); }
	int Release()
	{
		const int nCount = CryInterlockedDecrement(&m_cnt);
		if (nCount == 0)
			delete this;
		else if (nCount < 0)
			assert(0);
		return nCount;
	}

	int GetRefCount() const { return m_cnt; }
private:
	volatile int m_cnt;
};

struct StateText
{
	enum EType
	{
		FAIL,
		WARNING,
		PROGRESS,
		INFO
	};
	EType                      type;
	CryStackStringT<char, 128> animation;
	CryStackStringT<char, 128> text;
};

class CompressionMachine : public QObject, public InterlockedReferenceTarget
{
	Q_OBJECT
public:
	CompressionMachine();

	void        SetCharacters(ICharacterInstance* uncompressedCharacter, ICharacterInstance* compressedCharacter);
	void        SetLoop(bool loop);
	void        SetPlaybackSpeed(float speed);
	void        PreviewAnimation(const PlaybackLayers& layers, const vector<bool>& isModified, bool showOriginalAnimation, const vector<SAnimSettings>& animSettings, float normalizedTime, bool forceRecompile, bool expectToReloadChrparams);
	void        Play(float normalizedTime);
	const char* AnimationPathConsideringPreview(const char* inputCaf) const;

	void        Reset();

	bool        IsPlaying() const;

	void        GetAnimationStateText(vector<StateText>* lines, bool compressedCharacter);

	bool        ShowOriginalAnimation() const { return m_showOriginalAnimation; }

signals:
	void SignalAnimationStarted();

private:
	enum State
	{
		eState_Idle,
		eState_Waiting,
		eState_PreviewAnimation,
		eState_PreviewCompressedOnly,
		eState_Failed
	};

	enum AnimationState
	{
		eAnimationState_NotSet,
		eAnimationState_Canceled,
		eAnimationState_Compression,
		eAnimationState_Ready,
		eAnimationState_Failed,
		eAnimationState_WaitingToReloadChrparams,
	};

	struct Animation
	{
		enum Type
		{
			UNKNOWN,
			CAF,
			AIMPOSE,
			BLEND_SPACE,
			ANM
		};

		bool                     enabled;
		AnimationState           state;
		Type                     type;
		string                   path;
		string                   name;
		string                   compressedReferencePath;
		bool                     hasSourceFile;
		bool                     hasReferenceCompressed;
		bool                     hasPreviewCompressed;
		IBackgroundTask*         previewTask;
		IBackgroundTask*         referenceTask;
		int                      compressionSessionIndex;
		string                   failMessage;
		AnimationCompressionInfo compressionInfo;
		SAnimationReference      uncompressedCaf;
		SAnimationReference      compressedCaf;

		string                   compressedAnimationName;
		string                   compressedAnimationPath;
		string                   uncompressedAnimationName;
		string                   uncompressedAnimationPath;

		Animation()
			: state(eAnimationState_NotSet)
			, enabled(true)
			, hasSourceFile(true)
			, hasReferenceCompressed(false)
			, hasPreviewCompressed(false)
			, type(UNKNOWN)
		{}

	};

	class BackgroundTaskCompressPreview;
	class BackgroundTaskCompressReference;
	struct AnimationSetExtender;

	void StartPreview(bool forceRecompile, bool expectToReloadChrParams);
	void OnCompressed(BackgroundTaskCompressPreview* pTask);
	void AnimationStateChanged();
	void SetState(State state);

	vector<Animation>                     m_animations;
	PlaybackLayers                        m_playbackLayers;
	vector<bool>                          m_layerAnimationsModified;
	vector<SAnimSettings>                 m_animSettings;

	float                                 m_normalizedStartTime;
	bool                                  m_showOriginalAnimation;
	bool                                  m_loop;
	float                                 m_playbackSpeed;

	ICharacterInstance*                   m_uncompressedCharacter;
	ICharacterInstance*                   m_compressedCharacter;
	std::unique_ptr<AnimationSetExtender> m_previewReloadListener;
	std::unique_ptr<AnimationSetExtender> m_referenceReloadListener;

	State                                 m_state;

	int m_compressionSessionIndex;
};

}

