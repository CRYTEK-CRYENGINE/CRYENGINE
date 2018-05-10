// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "NotificationWidget.h"
#include "NotificationCenterImpl.h"

// EditorCommon
#include "QControls.h"
#include "CryIcon.h"
#include "EditorStyleHelper.h"

// Editor
#include "IEditor.h"

// Qt
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QPainter>
#include <QStyleOption>
#include <QTimer>
#include <QProgressBar>
#include <QResizeEvent>
#include <QTextDocument>

// Switched window flag Qt::SubWindow to Qt::Tool to prevent it from stealing focus / closing menus
// Qt::Tool is commonly used for pop ups

CNotificationWidget::CNotificationWidget(int notificationId, QWidget* pParent /* = nullptr*/)
	: QWidget(pParent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
	, m_id(notificationId)
	, m_pHideTimer(nullptr)
	, m_count(1)
	, notificationDuration(2250)
	, m_pIconLabel(nullptr)
	, m_pProgressBar(nullptr)
	, m_bIsAccepted(false)
	, m_bDisableTimerHide(false)
{
	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	Internal::CNotification* pNotificationDesc = pNotificationCenter->GetNotification(m_id);
	connect(pNotificationDesc, &Internal::CNotification::Hidden, this, &CNotificationWidget::OnAccepted);
	m_title = pNotificationDesc->GetTitle();
	m_bIsLogMessage = pNotificationDesc->IsLogMessage();

	setFocusPolicy(Qt::FocusPolicy::NoFocus);
	setAttribute(Qt::WA_ShowWithoutActivating);
	// Create inset widget and layout so we can fake a border around the widget
	QVBoxLayout* pInsetLayout = new QVBoxLayout();
	QWidget* pInsetContainer = new QWidget();
	pInsetLayout->setMargin(0);
	pInsetLayout->setSpacing(0);
	pInsetLayout->addWidget(pInsetContainer);

	QVBoxLayout* pMainLayout = new QVBoxLayout();
	QHBoxLayout* pInfoLayout = new QHBoxLayout();
	pInfoLayout->setSpacing(0);
	pInfoLayout->setMargin(0);

	m_pTitle = new QLabel(m_title);
	QFontMetrics metrics(m_pTitle->font());
	QString elidedText = metrics.elidedText(m_title, Qt::ElideRight, sizeHint().width() - 50);
	m_pTitle->setText(elidedText);
	m_pTitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_pTitle->setObjectName("title"); // name accessible through qss
	m_pTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
	pMainLayout->addWidget(m_pTitle, Qt::AlignTop | Qt::AlignLeft);

	QString messageText(pNotificationDesc->GetMessage());
	QString linkMessage("Go to error");
	Internal::CNotification::Type notificationType = pNotificationDesc->GetType();
	if (notificationType != Internal::CNotification::Warning && notificationType != Internal::CNotification::Critical)
		linkMessage = "Go to..";

	if (pNotificationDesc->GetCommandCount())
		messageText += "<a style=\"color:" + GetStyleHelper()->selectedIconTint().name() + ";\" href=\"#execute_commands\">" + linkMessage + "</a>";

	m_pMessage = new QLabel(messageText); // should maybe do something for long messages
	m_pMessage->setObjectName("message");
	m_pMessage->setWordWrap(true);
	m_pMessage->setOpenExternalLinks(false);
	pInfoLayout->addWidget(m_pMessage, Qt::AlignTop | Qt::AlignLeft);
	pMainLayout->addLayout(pInfoLayout);

	connect(m_pMessage, &QLabel::linkActivated , [pNotificationDesc](const QString& link)
	{
		pNotificationDesc->Execute();
	});

	const int iconSize = 48;
	if (notificationType == Internal::CNotification::Progress)
	{
		Internal::CProgressNotification* pProgress = static_cast<Internal::CProgressNotification*>(pNotificationDesc);
		connect(pProgress, &Internal::CProgressNotification::ProgressChanged, this, &CNotificationWidget::ProgressChanged);
		connect(pProgress, &Internal::CProgressNotification::MessageChanged, this, &CNotificationWidget::MessageChanged);
		connect(pProgress, &Internal::CProgressNotification::TaskComplete, this, &CNotificationWidget::TaskComplete);

		if (pProgress->ShowProgress())
		{
			m_pProgressBar = new QProgressBar(this);
			pMainLayout->addWidget(m_pProgressBar);
		}
		
		m_pIconLabel = new QLoading(this);
		pInfoLayout->addWidget(m_pIconLabel);
		
		if (pProgress->IsTaskComplete())
			TaskComplete();
	}
	else
	{
		m_pIconLabel = new QLabel();
		switch (notificationType)
		{
		case Internal::CNotification::Info:
			m_pIconLabel->setPixmap(CryIcon("icons:Dialogs/dialog-question.ico").pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On));
			break;
		case Internal::CNotification::Warning:
			m_pIconLabel->setPixmap(CryIcon("icons:Dialogs/dialog-warning.ico").pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On));
			break;
		case Internal::CNotification::Critical:
			m_pIconLabel->setPixmap(CryIcon("icons:Dialogs/dialog-error.ico").pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On));
			break;
		}
		pInfoLayout->addWidget(m_pIconLabel);
	}

	if (m_pIconLabel)
	{
		m_pIconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
		m_pIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	}

	pInsetContainer->setLayout(pMainLayout);
	setLayout(pInsetLayout);
}

CNotificationWidget::~CNotificationWidget()
{
	signalNotificationHide.DisconnectAll();
}

QString CNotificationWidget::GetMessage() const
{
	// Return non-html version of the message
	QTextDocument textDoc(m_pMessage->text());
	return textDoc.toPlainText();
}

void CNotificationWidget::CombineNotifications(const QString& newTitle)
{
	if (!m_title.contains(newTitle))
	{
		m_title += " & " + newTitle;
	}
	QFontMetrics metrics(m_pTitle->font());
	QString elidedText = metrics.elidedText(m_title, Qt::ElideRight, sizeHint().width() - 20);
	m_pTitle->setText(elidedText);
	m_pMessage->setText("There are currently <b><font color=" + GetStyleHelper()->selectedIconTint().name() + ">" + QString::number(++m_count) + " " + m_title + "s</b>");
	// Force resize of notification
	layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);
	adjustSize();
}

void CNotificationWidget::ShowAt(const QPoint& globalPos)
{
	show();
	// Move the Widget after calling show(). Size is otherwise incorrect
	move(globalPos.x() - width(), globalPos.y());

	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	Internal::CNotification* pNotification = pNotificationCenter->GetNotification(m_id);
	if (pNotification && pNotification->GetType() == Internal::CNotification::Progress)
	{
		Internal::CProgressNotification* pProgress = static_cast<Internal::CProgressNotification*>(pNotification);
		if (!pProgress->IsTaskComplete())
			return;
	}

	StartHideTimer();
}

bool CNotificationWidget::CanHide() const
{ 
	return m_pProgressBar == nullptr || m_pProgressBar->value() == m_pProgressBar->maximum(); 
}

void CNotificationWidget::Hide()
{
	if (m_pHideTimer)
		m_pHideTimer->stop();

	hide();
}

void CNotificationWidget::OnAccepted()
{
	if (m_bIsAccepted)
		return;
	
	m_bIsAccepted = true;
	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	Internal::CNotification* pNotification = pNotificationCenter->GetNotification(m_id);
	pNotification->Execute();
	pNotification->Hide();
	signalNotificationHide();
}

void CNotificationWidget::OnHideTimerElapsed()
{
	if (m_bDisableTimerHide)
		return;

	signalNotificationHide();
}

void CNotificationWidget::StartHideTimer()
{
	if (!m_pHideTimer)
	{
		m_pHideTimer = new QTimer();
		connect(m_pHideTimer, &QTimer::timeout, this, &CNotificationWidget::OnHideTimerElapsed);
	}

	m_pHideTimer->setSingleShot(true);
	m_pHideTimer->start(notificationDuration);
}

void CNotificationWidget::TaskComplete()
{
	if (m_pIconLabel)
		static_cast<QLoading*>(m_pIconLabel)->SetLoading(false);

	if (m_pProgressBar)
		m_pProgressBar->setValue(m_pProgressBar->maximum());

	StartHideTimer();
}

void CNotificationWidget::ProgressChanged(float value)
{
	if (m_pProgressBar)
		m_pProgressBar->setValue(static_cast<int>(value * 100.0f));
}

void CNotificationWidget::MessageChanged(const QString& message)
{
	m_pMessage->setText(message);
}

void CNotificationWidget::mousePressEvent(QMouseEvent* pEvent)
{
	if (pEvent->button() != Qt::LeftButton)
	{
		pEvent->setAccepted(false);
		return;
	}

	else if (pEvent->modifiers() & Qt::KeyboardModifier::ShiftModifier)
		signalNotificationHideAll();
	else if (parent()) // Check if it's a pop up notification
	{
		CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
		Internal::CNotification* pNotification = pNotificationCenter->GetNotification(m_id);

		// If it's not a pop-up progress notification and the task has not been completed, then don't allow hiding by clicking
		if (pNotification && pNotification->GetType() == Internal::CNotification::Progress)
		{
			Internal::CProgressNotification* pProgress = static_cast<Internal::CProgressNotification*>(pNotification);
			if (!pProgress->IsTaskComplete())
				return;
		}
	}

	signalNotificationHide();

}

void CNotificationWidget::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void CNotificationWidget::resizeEvent(QResizeEvent* pEvent)
{
	QFontMetrics metrics(m_pTitle->font());
	QString elidedText = metrics.elidedText(m_title, Qt::ElideRight, pEvent->size().width() - 20);
	m_pTitle->setText(elidedText);
}

void CNotificationWidget::enterEvent(QEvent* pEvent)
{
	m_bDisableTimerHide = true;
}

void CNotificationWidget::leaveEvent(QEvent* pEvent)
{
	m_bDisableTimerHide = false;
	if (m_pHideTimer && !m_pHideTimer->isActive())
		signalNotificationHide();
}

