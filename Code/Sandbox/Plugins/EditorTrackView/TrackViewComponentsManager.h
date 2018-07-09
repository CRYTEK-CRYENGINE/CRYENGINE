// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2016.

#pragma once

#include "Controls/KeysToolbar.h"
#include "Controls/SequenceToolbar.h"
#include "Controls/SequenceTabWidget.h"
#include "Controls/PropertyTreeDockWidget.h"
#include "Controls/PlaybackControlsToolbar.h"

class CTrackViewComponentsManager
{
public:
	CTrackViewComponentsManager();
	~CTrackViewComponentsManager();

	void                               Init(CTrackViewCore* pTrackViewCore);
	void                               BroadcastTrackViewEditorEvent(ETrackViewEditorEvent event);

	CTrackViewKeysToolbar*             GetTrackViewKeysToolbar()         const { return m_pKeysToolbar; }
	CTrackViewSequenceToolbar*         GetTrackViewSequenceToolbar()     const { return m_pSequenceToolbar; }
	CTrackViewSequenceTabWidget*       GetTrackViewSequenceTabWidget()   const { return m_pSequenceTabWidget; }
	CTrackViewPropertyTreeWidget*      GetTrackViewPropertyTreeWidget()  const { return m_pPropertyTreeWidget; }
	CTrackViewPlaybackControlsToolbar* GetTrackViewPlaybackToolbar()     const { return m_pPlaybackToolbar; }

private:
	CTrackViewKeysToolbar*                m_pKeysToolbar;
	CTrackViewSequenceToolbar*            m_pSequenceToolbar;
	CTrackViewSequenceTabWidget*          m_pSequenceTabWidget;
	CTrackViewPropertyTreeWidget*         m_pPropertyTreeWidget;
	CTrackViewPlaybackControlsToolbar*    m_pPlaybackToolbar;

	CTrackViewCore*                       m_pTrackViewCore;
	std::vector<CTrackViewCoreComponent*> m_pTrackViewComponents;

	template<class T>
	T* CreateComponent(CTrackViewCore* pTrackViewCore)
	{
		T* pComponent = new T(pTrackViewCore);
		pComponent->Initialize();
		stl::push_back_unique(m_pTrackViewComponents, pComponent);
		return pComponent;
	}

	template<class T>
	T RemoveComponent(T component)
	{
		stl::find_and_erase(m_pTrackViewComponents, component);
		return component;
	}
};

