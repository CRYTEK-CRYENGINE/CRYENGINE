// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "NotificationList.h"
#include "NotificationWidget.h"
#include "NotificationCenterImpl.h"

// Qt
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QToolButton>
#include <QApplication>
#include <QClipboard>
#include <QMenu>

// EditorCommon
#include "CryIcon.h"
#include "QtUtil.h"

// Editor
#include "IEditor.h"

CNotificationList::CNotificationList(QWidget* pParent /*= nullptr*/)
	: QWidget(pParent)
	, m_pMainLayout(new QVBoxLayout())
	, m_combineIdx(-1)
{
	setLayout(m_pMainLayout);
	m_pMainLayout->setMargin(0);
	m_pMainLayout->setSpacing(0);

	QToolButton* pClearAll = new QToolButton();
	connect(pClearAll, &QToolButton::clicked, this, &CNotificationList::ClearAll);
	pClearAll->setIcon(CryIcon("icons:General/Element_Clear.ico"));
	m_pMainLayout->addWidget(pClearAll);

	QWidget* pNotificationsContainer = new QWidget();
	m_pNotificationsLayout = new QVBoxLayout();
	m_pNotificationsLayout->setMargin(0);
	m_pNotificationsLayout->setSpacing(0);

	m_pNotificationsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
	pNotificationsContainer->setLayout(m_pNotificationsLayout);
	pNotificationsContainer->setObjectName("notificationsContainer");
	m_pMainLayout->addWidget(QtUtil::MakeScrollable(pNotificationsContainer));

	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	std::vector<Internal::CNotification*>& notifications = pNotificationCenter->GetNotifications();
	for (Internal::CNotification* pNotification : notifications)
	{
		if (!pNotification->IsHidden())
			AddNotification(pNotification->GetId());
	}

	setContextMenuPolicy(Qt::CustomContextMenu);

	connect(this, &CNotificationList::customContextMenuRequested, this, &CNotificationList::OnContextMenu);
	connect(pNotificationCenter, &CNotificationCenter::NotificationAdded, this, static_cast<void (CNotificationList::*)(int)>(&CNotificationList::AddNotification));
}

CNotificationList::~CNotificationList()
{

}

void CNotificationList::AddNotification(int notificationId)
{
	// Combine notifications if necessary
	if (gNotificationPreferences.combineNotifications() && (m_combineIdx > -1 || (m_notifications.size() + 1) > gNotificationPreferences.maxNotificationCount()))
	{
		if (CombineNotifications(notificationId))
			return;
	}

	CNotificationWidget* pNotification = new CNotificationWidget(notificationId, this);
	pNotification->signalNotificationHide.Connect([this, pNotification]
	{
		pNotification->signalNotificationHide.DisconnectAll();

		QGraphicsOpacityEffect* opacityEffect = new QGraphicsOpacityEffect(pNotification);
		pNotification->setGraphicsEffect(opacityEffect);
		QPropertyAnimation* pAlphaAnim = new QPropertyAnimation(opacityEffect, "opacity");
		pAlphaAnim->setDuration(250);
		pAlphaAnim->setStartValue(1.0f);
		pAlphaAnim->setEndValue(0.0f);

		pAlphaAnim->start(QAbstractAnimation::DeleteWhenStopped);
		connect(pAlphaAnim, &QPropertyAnimation::finished, [this, pNotification]()
		{
			int idx = m_notifications.indexOf(pNotification);

			if (idx >= 0)
			{
				if (idx == m_combineIdx)
					m_combineIdx = -1;
				else if (idx < m_combineIdx)
					m_combineIdx--;

				m_notifications.removeAt(idx);
			}

			m_pNotificationsLayout->removeWidget(pNotification);
			pNotification->deleteLater();
		});
	});

	// We can assume that notifications won't be added out of order
	m_pNotificationsLayout->insertWidget(m_pNotificationsLayout->count() - 1, pNotification, 0, Qt::AlignTop);
	m_notifications.push_back(pNotification);
}

bool CNotificationList::CombineNotifications(int notificationId)
{
	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	Internal::CNotification* pNotificationDesc = pNotificationCenter->GetNotification(notificationId);

	if (!pNotificationDesc->IsLogMessage())
		return false;

	if (m_combineIdx > -1)
	{
		m_notifications[m_combineIdx]->CombineNotifications(pNotificationDesc->GetTitle());
		return true;
	}

	for (auto i = 0; i < m_notifications.size(); ++i)
	{
		if (m_notifications[i] && m_notifications[i]->IsLogMessage())
		{
			m_notifications[i]->CombineNotifications(pNotificationDesc->GetTitle());
			m_combineIdx = i;
			break;
		}
	}

	// Go through and remove all log notification except the combined notifications
	for (int i = m_notifications.size() - 1; i > m_combineIdx; --i)
	{
		if (m_notifications[i]->IsLogMessage())
		{
			m_notifications[m_combineIdx]->CombineNotifications(m_notifications[i]->GetTitle());
			m_notifications[i]->deleteLater();
			m_notifications.removeAt(i);
		}
	}

	return true;
}

void CNotificationList::ClearAll()
{
	for (CNotificationWidget* pNotification : m_notifications)
	{
		pNotification->OnAccepted();
	}

	m_notifications.clear();
	m_combineIdx = -1; // reset combine index
}

void CNotificationList::OnContextMenu(const QPoint& point)
{
	QMenu menu;
	connect(menu.addAction(CryIcon("icons:General/File_Copy.ico"), "Copy"), &QAction::triggered, [this, point]()
	{
		QObject* pChild = childAt(point);
		CNotificationWidget* pNotificationWidget = qobject_cast<CNotificationWidget*>(pChild);
		while (!pNotificationWidget)
		{
			pChild = pChild->parent();

			if (!pChild || pChild == this)
				return;

			pNotificationWidget = qobject_cast<CNotificationWidget*>(pChild);
		}
		if (pNotificationWidget)
			QApplication::clipboard()->setText(pNotificationWidget->GetMessage());
	});
	menu.exec(QCursor::pos());
}

