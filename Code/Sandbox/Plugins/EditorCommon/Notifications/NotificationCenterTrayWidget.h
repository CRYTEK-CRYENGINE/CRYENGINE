// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QToolButton>
#include "NotificationCenter.h"

#include "EditorCommonAPI.h"
#include "QtViewPane.h"

class QVBoxLayout;
class CNotificationWidget;
class QPopupWidget;

//! Dockable for whole notification center to be used as a tool window

class EDITOR_COMMON_API CNotificationCenterDockable : public CDockableWidget
{
	Q_OBJECT
public:
	CNotificationCenterDockable(QWidget* pParent = nullptr);

	//////////////////////////////////////////////////////////
	// CDockableWidget implementation
	virtual const char* GetPaneTitle() const override { return "Notification Center"; }
	virtual QRect       GetPaneRect() override { return QRect(0, 0, 640, 400); }
	//////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////

//! Notification Center widget that is registered in the tray area.

class EDITOR_COMMON_API CNotificationCenterTrayWidget : public QToolButton
{
	Q_PROPERTY(int notificationDistance READ GetNotificationDistance WRITE SetNotificationDistance DESIGNABLE true)
	Q_PROPERTY(int animationDuration READ GetAnimationDuration WRITE SetAnimationDuration DESIGNABLE true)
	Q_PROPERTY(int animationDistance READ GetAnimationDistance WRITE SetAnimationDistance DESIGNABLE true)
	Q_PROPERTY(int notificationMaxCount READ GetNotificationMaxCount WRITE SetNotificationMaxCount DESIGNABLE true)

	Q_OBJECT
public:

	CNotificationCenterTrayWidget(QWidget* pParent = nullptr);
	~CNotificationCenterTrayWidget();

protected:
	void OnNotificationAdded(int id);
	void OnClicked(bool bChecked);

	int  GetNotificationDistance() const;
	int  GetNotificationMaxCount() const;
	int  GetAnimationDuration() const;
	int  GetAnimationDistance() const;

	void SetNotificationDistance(int distance);
	void SetNotificationMaxCount(int maxCount);
	void SetAnimationDuration(int duration);
	void SetAnimationDistance(int distance);

	// QWidget
	virtual bool eventFilter(QObject* pWatched, QEvent* pEvent) override;

protected:
	QPopupWidget*                  m_pPopUpMenu;
	class CNotificationPopupManager* m_pPopUpManager;
	// Set to the maximum warning type emitted. Resets when acknowledged by user.
	int                              m_notificationType;
};

