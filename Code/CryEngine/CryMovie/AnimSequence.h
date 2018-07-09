// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __animsequence_h__
#define __animsequence_h__

#pragma once

#include <CryMovie/IMovieSystem.h>

class CAnimSequence : public IAnimSequence
{
public:
	CAnimSequence(IMovieSystem* pMovieSystem, uint32 id);

	// Movie system.
	IMovieSystem*                GetMovieSystem() const { return m_pMovieSystem; };

	virtual void                 SetName(const char* name) override;
	virtual const char*          GetName() const override;
	virtual uint32               GetId() const override                        { return m_id; }

	SAnimTime                    GetTime() const                               { return m_time; }

	virtual float                GetFixedTimeStep() const override             { return m_fixedTimeStep; }
	virtual void                 SetFixedTimeStep(float dt) override           { m_fixedTimeStep = dt; }

	virtual void                 SetOwner(IAnimSequenceOwner* pOwner) override { m_pOwner = pOwner; }
	virtual IAnimSequenceOwner*  GetOwner() const override                     { return m_pOwner; }

	virtual void                 SetActiveDirector(IAnimNode* pDirectorNode) override;
	virtual IAnimNode*           GetActiveDirector() const override;

	virtual void                 SetFlags(int flags) override;
	virtual int                  GetFlags() const override;
	virtual int                  GetCutSceneFlags(const bool localFlags = false) const override;

	virtual void                 SetParentSequence(IAnimSequence* pParentSequence) override;
	virtual const IAnimSequence* GetParentSequence() const override;
	virtual bool                 IsAncestorOf(const IAnimSequence* pSequence) const override;

	virtual void                 SetTimeRange(TRange<SAnimTime> timeRange) override;
	virtual TRange<SAnimTime>    GetTimeRange() override { return m_timeRange; };

	virtual int                  GetNodeCount() const override;
	virtual IAnimNode*           GetNode(int index) const override;

	virtual IAnimNode*           FindNodeByName(const char* sNodeName, const IAnimNode* pParentDirector) override;
	virtual IAnimNode*           FindNodeById(int nNodeId);

	virtual void                 Reset(bool bSeekToStart) override;
	virtual void                 Pause() override;
	virtual void                 Resume() override;
	virtual bool                 IsPaused() const override;

	virtual void                 OnStart();
	virtual void                 OnStop();

	//! Add animation node to sequence.
	virtual bool       AddNode(IAnimNode* node) override;
	virtual IAnimNode* CreateNode(EAnimNodeType nodeType) override;
	virtual IAnimNode* CreateNode(XmlNodeRef node) override;
	virtual void       RemoveNode(IAnimNode* node) override;
	virtual void       RemoveAll() override;

	virtual void       Activate() override;
	virtual bool       IsActivated() const override { return m_bActive; }
	virtual void       Deactivate() override;

	virtual void       PrecacheData(SAnimTime startTime) override;
	virtual void       PrecacheStatic(const SAnimTime startTime);
	virtual void       PrecacheDynamic(SAnimTime time);
	virtual void       PrecacheEntity(IEntity* pEntity);

	virtual void       StillUpdate() override;
	virtual void       Animate(const SAnimContext& animContext) override;
	virtual void       Render() override;

	virtual void       Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks = true, uint32 overrideId = 0, bool bResetLightAnimSet = false) override;

	//! Add/remove track events in sequence
	virtual bool AddTrackEvent(const char* szEvent) override;
	virtual bool RemoveTrackEvent(const char* szEvent) override;
	virtual bool RenameTrackEvent(const char* szEvent, const char* szNewEvent) override;
	virtual bool MoveUpTrackEvent(const char* szEvent) override;
	virtual bool MoveDownTrackEvent(const char* szEvent) override;
	virtual void ClearTrackEvents() override;

	//! Get the track events in the sequence
	virtual int         GetTrackEventsCount() const override;
	virtual char const* GetTrackEvent(int iIndex) const override;

	//! Call to trigger a track event
	virtual void TriggerTrackEvent(const char* event, const char* param = NULL) override;

	//! Track event listener
	virtual void    AddTrackEventListener(ITrackEventListener* pListener) override;
	virtual void    RemoveTrackEventListener(ITrackEventListener* pListener) override;

	virtual CryGUID GetGUID() const override { return m_guid; }

	//! Sequence audio trigger
	virtual void                         SetAudioTrigger(const SSequenceAudioTrigger& audioTrigger) override { m_audioTrigger = audioTrigger; }
	virtual const SSequenceAudioTrigger& GetAudioTrigger() const override                                    { return m_audioTrigger; }

private:
	void ComputeTimeRange();
	void NotifyTrackEvent(ITrackEventListener::ETrackEventReason reason,
	                      const char* event, const char* param = NULL);

	void ExecuteAudioTrigger(const CryAudio::ControlId audioTriggerId);

	// Create a new animation node.
	IAnimNode* CreateNodeInternal(EAnimNodeType nodeType, uint32 nNodeId = -1);

	bool       AddNodeNeedToRender(IAnimNode* pNode);
	void       RemoveNodeNeedToRender(IAnimNode* pNode);

	typedef std::vector<_smart_ptr<IAnimNode>> AnimNodes;
	AnimNodes         m_nodes;
	AnimNodes         m_nodesNeedToRender;

	CryGUID           m_guid;
	uint32            m_id;
	string            m_name;
	mutable string    m_fullNameHolder;
	TRange<SAnimTime> m_timeRange;
	TrackEvents       m_events;

	// Listeners
	typedef std::list<ITrackEventListener*> TTrackEventListeners;
	TTrackEventListeners  m_listeners;

	int                   m_flags;

	bool                  m_precached;
	bool                  m_bResetting;

	IAnimSequence*        m_pParentSequence;

	IMovieSystem*         m_pMovieSystem;
	bool                  m_bPaused;
	bool                  m_bActive;

	uint32                m_lastGenId;

	IAnimSequenceOwner*   m_pOwner;

	IAnimNode*            m_pActiveDirector;

	SAnimTime             m_time;
	float                 m_fixedTimeStep;

	SSequenceAudioTrigger m_audioTrigger;

	VectorSet<IEntity*>   m_precachedEntitiesSet;
};

#endif // __animsequence_h__
