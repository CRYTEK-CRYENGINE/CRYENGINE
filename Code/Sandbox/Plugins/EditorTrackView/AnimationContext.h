// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include "IEditor.h"
#include "TrackViewSequenceManager.h"
#include "GameEngine.h"

#include <CryMath/Range.h>
#include <CryMovie/IMovieSystem.h>

class CTrackViewSequence;

/** CAnimationContext listener interface
 */
struct IAnimationContextListener
{
	virtual void OnSequenceChanged(CTrackViewSequence* pNewSequence) {}
	virtual void OnTimeChanged(SAnimTime newTime)                    {}
	virtual void OnPlaybackStateChanged(bool bPlaying, bool bPaused) {}
	virtual void OnRecordingStateChanged(bool bRecording)            {}
	virtual void OnLoopingStateChanged(bool bRecording)              {}
};

/**	CAnimationContext stores information about current editable animation sequence.
    Stores information about whenever animation is being recorded know,
    current sequence, current time in sequence etc.
 */
class CAnimationContext : public IEditorNotifyListener,
	                        public IUndoManagerListener, public ITrackViewSequenceManagerListener,
	                        public IGameEngineListener, public IMovieListener
{
public:
	CAnimationContext();
	~CAnimationContext();

	// Listeners
	void AddListener(IAnimationContextListener* pListener);
	void RemoveListener(IAnimationContextListener* pListener);

	/** Return current animation time in active sequence.
	   @return Current time.
	 */
	SAnimTime GetTime() const            { return m_currTime; };

	float     GetTimeScale() const       { return m_fTimeScale; }
	void      SetTimeScale(float fScale) { m_fTimeScale = fScale; }

	/** Set active editing sequence.
	   @param seq New active sequence.
	 */
	void SetSequence(CTrackViewSequence* pSequence, bool bForce, bool bNoNotify);

	/** Get currently edited sequence.
	 */
	CTrackViewSequence* GetSequence() const { return m_pSequence; };

	/** Set time markers to play within.
	 */
	void SetPlaybackRange(TRange<SAnimTime> Marker) { m_playbackRange = Marker; }

	/** Get time markers to play within.
	 */
	TRange<SAnimTime> GetPlaybackRange() { return m_playbackRange; }

	/** Get time range of active animation sequence.
	 */
	TRange<SAnimTime> GetTimeRange() const { return m_timeRange; }

	/** Returns true if editor is recording animations now.
	 */
	bool IsRecording() const { return m_bRecording && !m_bPlaying; };

	/** Returns true if editor is playing animation now.
	 */
	bool IsPlaying() const { return m_bPlaying; };

	/** Returns true if editor is paused
	 */
	bool IsPaused() const { return m_bPaused; };

	/** Return if animation context is now in recording mode.
	    In difference from IsRecording function this function not affected by pause state.
	 */
	bool IsRecordMode() const { return m_bRecording; };

	/** Returns true if currently looping as activated.
	 */
	bool IsLoopMode() const { return m_bLooping; }

	/** Enable/Disable looping.
	 */
	void SetLoopMode(bool bLooping);

	//////////////////////////////////////////////////////////////////////////
	// Operators
	//////////////////////////////////////////////////////////////////////////

	/** Set current animation time in active sequence.
	   @param seq New active time.
	 */
	void SetTime(SAnimTime t);

	/** Start animation recorduing.
	   Automatically stop playing.
	   @param recording True to start recording, false to stop.
	 */
	void SetRecording(bool playing);

	/** Enables/Disables automatic recording, sets the time step for each recorded frame.
	 */
	void SetAutoRecording(bool bEnable, SAnimTime fTimeStep);

	//! Check if auto recording enabled.
	bool IsAutoRecording() const { return m_bAutoRecording; };

	/** Start/Pause animation playing.
	   Automatically stop recording.
	   @param playing True to play, false to pause.
	 */
	void PlayPause(bool playOrPause);

	void Stop();

	/** Force animation for current sequence.
	 */
	void ForceAnimation();

	//////////////////////////////////////////////////////////////////////////
	// Enables playback of sequence with encoding to the AVI.
	// Arguments:
	//		bEnable - Enables/Disables writing to the AVI when playing sequence.
	//		aviFilename - Filename of avi file, must be with .avi extension.
	void PlayToAVI(bool bEnable, const char* aviFilename = NULL);
	bool IsPlayingToAVI() const;

	void OnPostRender();
	void UpdateTimeRange();

private:
	/** Called every frame to update all animations if animation should be playing.
	 */
	void         Update();

	// Updates the animation time of lights animated by the light animation set.
	void UpdateAnimatedLights();

	static void  GoToFrameCmd(IConsoleCmdArgs* pArgs);

	virtual void BeginUndoTransaction() override;
	virtual void EndUndoTransaction() override;

	virtual void OnSequenceRemoved(CTrackViewSequence* pSequence) override;

	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

	virtual void OnPreEditorUpdate() override  { Update(); }
	virtual void OnPostEditorUpdate() override {}

	// IMovieListener
	virtual void OnMovieEvent(EMovieEvent movieEvent, IAnimSequence* pAnimSequence) override;
	// ~IMovieListener

	//! Check if a camera delegate might be active in editor
	EntityId GetActiveCameraEntityId() const;

	//! Current time within active animation sequence.
	SAnimTime m_currTime;

	//! Force update in next frame
	bool m_bForceUpdateInNextFrame;

	//! Time within active animation sequence while reset animation.
	SAnimTime m_resetTime;

	float     m_fTimeScale;

	// Recording time step.
	SAnimTime m_recordingTimeStep;
	SAnimTime m_recordingCurrTime;

	bool      m_bAutoRecording;

	//! Time range of active animation sequence.
	TRange<SAnimTime> m_timeRange;

	TRange<SAnimTime> m_playbackRange;

	//! Currently active animation sequence.
	CTrackViewSequence* m_pSequence;

	//! Name of active sequence (for switching back from game mode and saving)
	string m_sequenceName;

	//! Time of active sequence (for switching back from game mode and saving)
	SAnimTime m_sequenceTime;

	bool      m_bLooping;

	//! True if editor is recording animations now.
	bool m_bRecording;
	bool m_bWasRecording;
	bool m_bSavedRecordingState;

	//! True if editor is playing animation now.
	bool m_bPlaying;

	bool m_bPaused;

	bool m_bSingleFrame;

	bool m_bPostRenderRegistered;

	//TODO : Improve observer system, right now every function has to loop and every event is a new virtual function...
	std::vector<IAnimationContextListener*> m_contextListeners;
};

