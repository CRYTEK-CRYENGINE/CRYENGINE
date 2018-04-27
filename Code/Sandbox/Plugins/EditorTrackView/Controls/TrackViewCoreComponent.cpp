// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "TrackViewPlugin.h"
#include "TrackViewCoreComponent.h"
#include <EditorFramework/Preferences.h>
#include <CrySerialization/yasli/decorators/Range.h>

namespace Private_TrackViewCoreComponent
{

struct STrackViewPreferences : public SPreferencePage
{
	STrackViewPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	int              userInterfaceRefreshRate;
	static const int oneSec = 1000; // millisesonds
};

STrackViewPreferences gTrackViewPreferences;
REGISTER_PREFERENCES_PAGE_PTR(STrackViewPreferences, &gTrackViewPreferences)

STrackViewPreferences::STrackViewPreferences()
	: SPreferencePage("General", "Track View/General")
	, userInterfaceRefreshRate(oneSec / 15)
{
}

bool STrackViewPreferences::Serialize(yasli::Archive& ar)
{
	const int userInterfaceMinFps = 10;
	const int userInterfaceMaxFps = 30;

	ar.openBlock("general", "General");
	int fps = oneSec / userInterfaceRefreshRate;
	ar(yasli::Range(fps, userInterfaceMinFps, userInterfaceMaxFps), "uiFps", "UI refresh rate (fps)");
	userInterfaceRefreshRate = oneSec / fps;

	ar.closeBlock();

	return true;
}

}


CTrackViewCoreComponent::CTrackViewCoreComponent(CTrackViewCore* pTrackViewCore, bool bUseEngineListeners)
	: m_pTrackViewCore(pTrackViewCore)
	, m_bUseEngineListeners(bUseEngineListeners)
	, m_bIsListening(false)
{
	Initialize();
}

CTrackViewCoreComponent::~CTrackViewCoreComponent()
{
	SetUseEngineListeners(false);
}

void CTrackViewCoreComponent::Initialize()
{
	SetUseEngineListeners(m_bUseEngineListeners);
}

int CTrackViewCoreComponent::GetUiRefreshRateMilliseconds()
{
	using namespace Private_TrackViewCoreComponent;
	return gTrackViewPreferences.userInterfaceRefreshRate;
}

void CTrackViewCoreComponent::SetUseEngineListeners(bool bEnable)
{
	if (bEnable && !m_bIsListening)
	{
		CTrackViewPlugin::GetAnimationContext()->AddListener(this);
		CTrackViewPlugin::GetSequenceManager()->AddListener(this);

		CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
		const uint numSequences = pSequenceManager->GetCount();
		for (uint i = 0; i < numSequences; ++i)
		{
			CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(i);
			pSequence->AddListener(this);
		}
	}
	else if (!bEnable && m_bIsListening)
	{
		CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
		const uint numSequences = pSequenceManager->GetCount();
		for (uint i = 0; i < numSequences; ++i)
		{
			CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(i);
			pSequence->RemoveListener(this);
		}
		CTrackViewPlugin::GetSequenceManager()->RemoveListener(this);
		CTrackViewPlugin::GetAnimationContext()->RemoveListener(this);
	}
	m_bIsListening = bEnable;
}

