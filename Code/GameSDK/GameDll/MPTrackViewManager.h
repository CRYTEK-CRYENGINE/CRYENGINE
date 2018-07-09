// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef MP_TRACK_VIEW_MANAGER
#define MP_TRACK_VIEW_MANAGER

#include "GameRules.h"
#include <CryMovie/IMovieSystem.h>

class CMPTrackViewManager : public IMovieListener
{
public:
	CMPTrackViewManager();
	~CMPTrackViewManager();

	void Init();
	void Update();

	void Server_SynchAnimationTimes(CGameRules::STrackViewParameters& params); 
	void Client_SynchAnimationTimes(const CGameRules::STrackViewParameters& params);
	void AnimationRequested(const CGameRules::STrackViewRequestParameters& params);
	bool HasTrackviewFinished(const CryHashStringId& id) const;

	IAnimSequence* FindTrackviewSequence(int trackviewId);

private:

	// IMovieListener
	virtual void OnMovieEvent(IMovieListener::EMovieEvent movieEvent, IAnimSequence* pAnimSequence);
	// ~IMovieListener

	int m_FinishedTrackViews[CGameRules::STrackViewParameters::sMaxTrackViews];
	float m_FinishedTrackViewTimes[CGameRules::STrackViewParameters::sMaxTrackViews];
	int m_FinishedTrackViewCount;
	bool m_movieListener;

};

#endif