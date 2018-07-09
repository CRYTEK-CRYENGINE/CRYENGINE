// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QToolBar>
#include "TrackViewCoreComponent.h"

class CTrackViewSequenceToolbar : public QToolBar, public CTrackViewCoreComponent
{
	Q_OBJECT

public:
	CTrackViewSequenceToolbar(CTrackViewCore* pTrackViewCore);
	~CTrackViewSequenceToolbar() {}

protected:
	virtual void        OnTrackViewEditorEvent(ETrackViewEditorEvent event) override {}
	virtual const char* GetComponentTitle() const override                           { return "Sequence Toolbar"; }

private slots:
	void OnAddSelectedEntities();
	void OnShowProperties();
};

