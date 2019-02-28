// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequenceToolbar.h"

#include "CryIcon.h"

#include "TrackViewComponentsManager.h"
#include "Controls/SequenceTabWidget.h"

CTrackViewSequenceToolbar::CTrackViewSequenceToolbar(CTrackViewCore* pTrackViewCore)
	: CTrackViewCoreComponent(pTrackViewCore, false)
{
	setMovable(true);
	setWindowTitle(QString("Sequence Controls"));

	addAction(CryIcon("icons:Trackview/Add_Selected_Entities_To_Sequence.ico"), "Add Selected Entities", this, SLOT(OnAddSelectedEntities()));
	addAction(CryIcon("icons:General/Options.ico"), "Sequence Properties", this, SLOT(OnShowProperties()));
	addSeparator();
	addAction(GetIEditor()->GetICommandManager()->GetAction("general.zoom_in"));
	addAction(GetIEditor()->GetICommandManager()->GetAction("general.zoom_out"));
}

void CTrackViewSequenceToolbar::OnAddSelectedEntities()
{
	GetTrackViewCore()->OnAddSelectedEntities();
}

void CTrackViewSequenceToolbar::OnShowProperties()
{
	if (CTrackViewComponentsManager* pManager = GetTrackViewCore()->GetComponentsManager())
	{
		if (CTrackViewSequenceTabWidget* pTabWidget = pManager->GetTrackViewSequenceTabWidget())
		{
			if (CTrackViewSequence* pSequence = pTabWidget->GetActiveSequence())
			{
				pTabWidget->ShowSequenceProperties(pSequence);
			}
		}
	}
}

