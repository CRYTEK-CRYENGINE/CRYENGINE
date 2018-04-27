// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QCollapsibleFrame.h"

#include <qtabbar.h>

// removes dark tinting
const CryIconColorMap CCollapsibleFrameHeader::s_colorMap = []
{
	CryIconColorMap colorMap;
	colorMap[QIcon::Mode::Normal] = QColor(255, 255, 255);
	return colorMap;
} ();

QCollapsibleFrame::QCollapsibleFrame(QWidget* pParent)
	: QWidget(pParent)
	, m_pWidget(nullptr)
	, m_pHeaderWidget(nullptr)
	, m_pContentsFrame(nullptr)
{
	auto pMainLayout = new QVBoxLayout;
	pMainLayout->setSpacing(0);
	pMainLayout->setContentsMargins(2, 2, 2, 2);
	setLayout(pMainLayout);
}

QCollapsibleFrame::QCollapsibleFrame(const QString& title, QWidget* pParent)
	: QCollapsibleFrame(pParent)
{
	SetHeaderWidget(new CCollapsibleFrameHeader(title, this));
}

void QCollapsibleFrame::SetWidget(QWidget* pWidget)
{
	if (m_pContentsFrame == nullptr)
	{
		m_pContentsFrame = new QFrame(this);
		auto frameLayout = new QVBoxLayout();
		frameLayout->setSpacing(0);
		frameLayout->setContentsMargins(2, 2, 2, 2);
		m_pContentsFrame->setLayout(frameLayout);
		layout()->addWidget(m_pContentsFrame);
	}

	auto pMainLayout = m_pContentsFrame->layout();

	if (m_pWidget)
	{
		pMainLayout->removeWidget(m_pWidget);
		m_pWidget->deleteLater();
	}

	m_pWidget = pWidget;
	if (m_pWidget)
	{
		pMainLayout->addWidget(m_pWidget);
		m_pContentsFrame->setHidden(m_pHeaderWidget->m_bCollapsed);
	}
}

void QCollapsibleFrame::SetTitle(const QString& title)
{
	m_pHeaderWidget->SetTitle(title);
}

bool QCollapsibleFrame::Closable() const
{
	return m_pHeaderWidget->Closable();
}

void QCollapsibleFrame::SetClosable(bool closable)
{
	m_pHeaderWidget->SetClosable(closable);
}

QCollapsibleFrame::~QCollapsibleFrame()
{}

QWidget* QCollapsibleFrame::GetWidget() const
{
	return m_pWidget;
}

QWidget * QCollapsibleFrame::GetDragHandler() const
{
	return m_pHeaderWidget->m_pCollapseButton;
}

bool QCollapsibleFrame::Collapsed() const
{
	return m_pHeaderWidget->m_bCollapsed;
}

void QCollapsibleFrame::SetCollapsed(bool bCollapsed)
{
	m_pHeaderWidget->SetCollapsed(bCollapsed);
}

void QCollapsibleFrame::SetCollapsedStateChangeCallback(std::function<void(bool bCollapsed)> callback)
{
	m_pHeaderWidget->m_onCollapsedStateChanged = callback; 
}

void QCollapsibleFrame::paintEvent(QPaintEvent*)
{
	QStyleOption opt;
	opt.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

void QCollapsibleFrame::SetHeaderWidget(CCollapsibleFrameHeader* pHeader)
{
	m_pHeaderWidget = pHeader;

	layout()->addWidget(m_pHeaderWidget);

	connect(m_pHeaderWidget->m_pCloseButton, SIGNAL(clicked()), this, SLOT(OnCloseRequested()));
}

void QCollapsibleFrame::OnCloseRequested()
{
	CloseRequested(this);
}

CCollapsibleFrameHeader::CCollapsibleFrameHeader(const QString& title, QCollapsibleFrame* pParentCollapsible, const QString& icon, bool bCollapsed)
	: m_iconSize(16, 16)
	, m_pParentCollapsible(pParentCollapsible)
	, m_pTitleLabel(new QLabel(title))
	, m_bCollapsed(false)
{
	if (icon.size() > 0)
	{
		m_pIconLabel = new QLabel();
		m_pIconLabel->setPixmap(CryIcon(icon).pixmap(16, 16, QIcon::Normal, QIcon::On));
	}
	else
	{
		m_pIconLabel = nullptr;
	}

	QPixmap collapsedPixmap("icons:Window/Collapse_Arrow_Right_Tinted.ico");
	if (collapsedPixmap.isNull())
	{
		m_collapsedIcon = QApplication::style()->standardIcon(QStyle::SP_TitleBarShadeButton);
	}
	else
	{
		m_collapsedIcon = CryIcon(collapsedPixmap, s_colorMap);
	}
	QPixmap expandedPixmap("icons:Window/Collapse_Arrow_Down_Tinted.ico");
	if (expandedPixmap.isNull())
	{
		m_expandedIcon = QApplication::style()->standardIcon(QStyle::SP_TitleBarUnshadeButton);
	}
	else
	{
		m_expandedIcon = CryIcon(expandedPixmap, s_colorMap);
	}
	SetupCollapseButton();
	SetupMainLayout();

	// do not install in SImplementation constructor because events may
	// be fired and eventFilter needs m_pImpl to be fully constructed
	m_pCollapseButton->installEventFilter(this);
	connect(m_pCollapseButton, SIGNAL(clicked()), this, SLOT(OnCollapseButtonClick()));
}

QSize CCollapsibleFrameHeader::GetIconSize() const
{
	return m_iconSize;
}

void CCollapsibleFrameHeader::SetupCollapseButton()
{
	m_pCollapseButton = new QPushButton(m_pParentCollapsible);
	m_pCollapseButton->setObjectName("CollapseButton");
	
	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(6, 2, 6, 2);
	m_pCollapseButton->setLayout(layout);
	
	m_pCollapseIconLabel = new QLabel(m_pCollapseButton);
	m_pCollapseIconLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	
	m_pTitleLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	
	m_pCloseButton = new QToolButton(m_pCollapseButton);
	m_pCloseButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DockWidgetCloseButton));
	m_pCloseButton->setFixedHeight(16);
	m_pCloseButton->setFixedWidth(16);

	m_pCloseButton->setVisible(false);
	m_pCollapseIconLabel->setPixmap(m_expandedIcon.pixmap(m_expandedIcon.actualSize(m_iconSize)));
	
	layout->addWidget(m_pCollapseIconLabel);

	if (m_pIconLabel != nullptr)
	{
		layout->addWidget(m_pIconLabel);
	}

	layout->addWidget(m_pTitleLabel);
	layout->addStretch();
	layout->addWidget(m_pCloseButton);
}

void CCollapsibleFrameHeader::SetTitle(const QString& title)
{
	m_pTitleLabel->setText(title);
}

void CCollapsibleFrameHeader::SetIconSize(const QSize& iconSize)
{
	m_iconSize = iconSize;
}

void CCollapsibleFrameHeader::SetupMainLayout()
{
	auto pTitleLayout = new QHBoxLayout;
	pTitleLayout->setContentsMargins(0, 0, 0, 0);
	pTitleLayout->setSpacing(0);
	pTitleLayout->addWidget(m_pCollapseButton);
	setLayout(pTitleLayout);
}

void CCollapsibleFrameHeader::SetCollapsed(bool collapsed)
{
	if (collapsed != m_bCollapsed)
	{
		m_bCollapsed = collapsed;

		if (m_bCollapsed)
		{
			m_pCollapseIconLabel->setPixmap(m_collapsedIcon.pixmap(m_collapsedIcon.actualSize(m_iconSize)));
		}
		else
		{
			m_pCollapseIconLabel->setPixmap(m_expandedIcon.pixmap(m_expandedIcon.actualSize(m_iconSize)));
		}

		if (m_pParentCollapsible->m_pContentsFrame)
		{
			m_pParentCollapsible->m_pContentsFrame->setHidden(m_bCollapsed);
		}
	}
}

void CCollapsibleFrameHeader::OnCollapseButtonClick()
{
	SetCollapsed(!m_bCollapsed);

	if (m_onCollapsedStateChanged)
	{
		m_onCollapsedStateChanged(m_bCollapsed);
	}
}

void CCollapsibleFrameHeader::SetClosable(bool closable)
{
	m_pCloseButton->setVisible(closable);
}

bool CCollapsibleFrameHeader::Closable() const
{
	return m_pCloseButton->isVisible();
}

void CCollapsibleFrameHeader::paintEvent(QPaintEvent*)
{
	QStyleOption opt;
	opt.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

bool CCollapsibleFrameHeader::eventFilter(QObject* pWatched, QEvent* pEvent)
{
	auto pCollapseButton = m_pCollapseButton;
	// can only be disabled when parent is disabled. cannot be explicitly disabled from outside this
	// class due to no public access.
	if ((pEvent->type() != QEvent::MouseButtonRelease) || (pWatched != pCollapseButton) || isEnabled())
	{
		return QWidget::eventFilter(pWatched, pEvent);
	}
	OnCollapseButtonClick();
	return true;
}
