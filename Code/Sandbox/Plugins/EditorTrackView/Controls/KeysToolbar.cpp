// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "KeysToolbar.h"
#include "CryIcon.h"

#include <QActionGroup>

CTrackViewKeysToolbar::CTrackViewKeysToolbar(CTrackViewCore* pTrackViewCore)
	: CTrackViewCoreComponent(pTrackViewCore, false)
{
	setMovable(true);
	setWindowTitle(QString("Keys Controls"));

	m_pSnappingGroup = new QActionGroup(this);
	m_pSnappingGroup->setExclusive(true);

	QAction* pNoSnapping = new QAction(m_pSnappingGroup);
	pNoSnapping->setIcon(CryIcon("icons:Trackview/No_Snapping.ico"));
	pNoSnapping->setText("No Snapping");
	pNoSnapping->setData(QVariant(eSnapMode_NoSnapping));
	pNoSnapping->connect(pNoSnapping, SIGNAL(triggered()), this, SLOT(OnSnapModeChanged()));
	pNoSnapping->setCheckable(true);
	m_pSnappingGroup->addAction(pNoSnapping);

	QAction* pKeySnapping = new QAction(m_pSnappingGroup);
	pKeySnapping->setIcon(CryIcon("icons:Trackview/Key_Snapping.ico"));
	pKeySnapping->setText("Key Snapping");
	pKeySnapping->setData(QVariant(eSnapMode_KeySnapping));
	pKeySnapping->connect(pKeySnapping, SIGNAL(triggered()), this, SLOT(OnSnapModeChanged()));
	pKeySnapping->setCheckable(true);
	m_pSnappingGroup->addAction(pKeySnapping);

	QAction* pTimeSnapping = new QAction(m_pSnappingGroup);
	pTimeSnapping->setIcon(CryIcon("icons:Trackview/Time_Snapping.ico"));
	pTimeSnapping->setText("Frame Snapping");
	pTimeSnapping->setData(QVariant(eSnapMode_TimeSnapping));
	pTimeSnapping->connect(pTimeSnapping, SIGNAL(triggered()), this, SLOT(OnSnapModeChanged()));
	pTimeSnapping->setCheckable(true);
	m_pSnappingGroup->addAction(pTimeSnapping);

	addAction(CryIcon("icons:Trackview/Go_To_Prev_Key.ico"), "Go to previous Key", this, SLOT(OnGoToPreviousKey()));
	addAction(CryIcon("icons:Trackview/Go_To_Next_Key.ico"), "Go to next Key", this, SLOT(OnGoToNextKey()));

	m_pMoveModeGroup = new QActionGroup(this);
	m_pMoveModeGroup->setExclusive(true);

	QAction* pSlideAction = new QAction(m_pMoveModeGroup);
	pSlideAction->setIcon(CryIcon("icons:Trackview/trackview_keys-slide.ico"));
	pSlideAction->setText("Slide Keys");
	pSlideAction->setData(QVariant(eKeyMoveMode_Slide));
	pSlideAction->connect(pSlideAction, SIGNAL(triggered()), this, SLOT(OnSlideKeys()));
	pSlideAction->setCheckable(true);
	m_pMoveModeGroup->addAction(pSlideAction);

	QAction* pScaleAction = new QAction(m_pMoveModeGroup);
	pScaleAction->setIcon(CryIcon("icons:Trackview/trackview_keys-scale.ico"));
	pScaleAction->setText("Scale Keys");
	pScaleAction->setData(QVariant(eKeyMoveMode_Scale));
	pScaleAction->connect(pScaleAction, SIGNAL(triggered()), this, SLOT(OnScaleKeys()));
	pScaleAction->setCheckable(true);
	m_pMoveModeGroup->addAction(pScaleAction);

	addSeparator();
	addActions(m_pSnappingGroup->actions());
	addActions(m_pMoveModeGroup->actions());
	addSeparator();
	addAction(CryIcon("icons:Trackview/Sync_Selected_Tracks_To_Base_Pos.ico"), "Sync selected Tracks to Base Position", this, SLOT(OnSyncSelectedTracksToBase()));
	addAction(CryIcon("icons:Trackview/Sync_Selected_Tracks_From_Base_Pos.ico"), "Sync selected Tracks from Base Position", this, SLOT(OnSyncSelectedTracksFromBase()));
}

void CTrackViewKeysToolbar::OnGoToPreviousKey()
{
	GetTrackViewCore()->OnGoToPreviousKey();
}

void CTrackViewKeysToolbar::OnGoToNextKey()
{
	GetTrackViewCore()->OnGoToNextKey();
}

void CTrackViewKeysToolbar::OnSlideKeys()
{
	GetTrackViewCore()->OnSlideKeys();
}

void CTrackViewKeysToolbar::OnScaleKeys()
{
	GetTrackViewCore()->OnScaleKeys();
}

void CTrackViewKeysToolbar::OnTrackViewEditorEvent(ETrackViewEditorEvent event)
{
	switch (event)
	{
	case eTrackViewEditorEvent_OnSnapModeChanged:
		{
			ETrackViewSnapMode newSnapMode = GetTrackViewCore()->GetCurrentSnapMode();
			for (auto pAction : m_pSnappingGroup->actions())
			{
				if (pAction->data().toUInt() == newSnapMode)
				{
					pAction->setChecked(true);
					break;
				}
			}
			break;
		}

	case eTrackViewEditorEvent_OnKeyMoveModeChanged:
		{
			ETrackViewKeyMoveMode moveMode = GetTrackViewCore()->GetCurrentKeyMoveMode();
			for (auto pAction : m_pMoveModeGroup->actions())
			{
				if (pAction->data().toUInt() == moveMode)
				{
					pAction->setChecked(true);
					break;
				}
			}
			break;
		}
	}
}

void CTrackViewKeysToolbar::OnSnapModeChanged()
{
	QAction* pSenderAction = static_cast<QAction*>(sender());
	if (pSenderAction == nullptr)
		return;

	GetTrackViewCore()->SetSnapMode((ETrackViewSnapMode)pSenderAction->data().toUInt());
}

void CTrackViewKeysToolbar::OnSyncSelectedTracksToBase()
{
	GetTrackViewCore()->OnSyncSelectedTracksToBase();
}

void CTrackViewKeysToolbar::OnSyncSelectedTracksFromBase()
{
	GetTrackViewCore()->OnSyncSelectedTracksFromBase();
}

