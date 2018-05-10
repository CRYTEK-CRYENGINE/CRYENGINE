// Copyright 2001-2015 Crytek GmbH. All rights reserved.

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

