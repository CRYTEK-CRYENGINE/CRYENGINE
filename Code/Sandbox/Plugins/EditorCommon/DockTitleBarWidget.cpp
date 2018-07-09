// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DockTitleBarWidget.h"

#include <QStyle>
#include <QStyleOptionToolButton>

#include "CryIcon.h"
#include "EditorStyleHelper.h"
#include "Expected.h"
#include "QtUtil.h"

class CDockWidgetTitleButton : public QAbstractButton
{
public:
	CDockWidgetTitleButton(QWidget* parent);

	QSize sizeHint() const;
	QSize minimumSizeHint() const { return sizeHint(); }

protected:
	void enterEvent(QEvent* ev);
	void leaveEvent(QEvent* ev);
	void paintEvent(QPaintEvent* ev);
};

class CTitleBarText : public QWidget
{
public:

	CTitleBarText(QWidget* parent, QDockWidget* dockWidget)
		: QWidget(parent)
		, m_dockWidget(dockWidget)
	{
		QFont font;
		font.setBold(true);
		setFont(font);
	}

	void paintEvent(QPaintEvent* ev) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		QRect r = rect().adjusted(2, 2, -3, -3);
		p.translate(0.5f, 0.5f);
		p.setPen(Qt::NoPen);
		p.drawRoundedRect(r, 4, 4, Qt::AbsoluteSize);
		p.setPen(QPen(GetStyleHelper()->windowTextColor()));
		QTextOption textOption(Qt::AlignLeft | Qt::AlignVCenter);
		textOption.setWrapMode(QTextOption::NoWrap);
		p.drawText(r.adjusted(4, 0, 0, 0), m_dockWidget->windowTitle(), textOption);
	}
private:
	QDockWidget* m_dockWidget;
};

// ---------------------------------------------------------------------------

CDockWidgetTitleButton::CDockWidgetTitleButton(QWidget* parent)
	: QAbstractButton(parent)
{
	setFocusPolicy(Qt::NoFocus);
}

QSize CDockWidgetTitleButton::sizeHint() const
{
	ensurePolished();

	int size = 2 * style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin, 0, this);
	if (!icon().isNull())
	{
		int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);
		QSize sz = icon().actualSize(QSize(iconSize, iconSize));
		size += qMax(sz.width(), sz.height());
	}

	return QSize(size, size);
}

void CDockWidgetTitleButton::enterEvent(QEvent* ev)
{
	if (isEnabled())
		update();
	QAbstractButton::enterEvent(ev);
}

void CDockWidgetTitleButton::leaveEvent(QEvent* ev)
{
	if (isEnabled())
		update();
	QAbstractButton::leaveEvent(ev);
}

void CDockWidgetTitleButton::paintEvent(QPaintEvent* ev)
{
	QPainter painter(this);

	QStyleOptionToolButton opt;
	opt.state = QStyle::State_AutoRaise;
	opt.init(this);
	opt.state |= QStyle::State_AutoRaise;

	if (isEnabled() && underMouse() && !isChecked() && !isDown())
		opt.state |= QStyle::State_Raised;
	if (isChecked())
		opt.state |= QStyle::State_On;
	if (isDown())
		opt.state |= QStyle::State_Sunken;
	if (opt.state & (QStyle::State_Raised | QStyle::State_Sunken))
		style()->drawPrimitive(QStyle::PE_PanelButtonTool, &opt, &painter, this);

	opt.icon = icon();
	opt.subControls = 0;
	opt.activeSubControls = 0;
	opt.features = QStyleOptionToolButton::None;
	opt.arrowType = Qt::NoArrow;
	int size = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);
	opt.iconSize = QSize(size, size);
	style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &painter, this);
}

// ---------------------------------------------------------------------------

CDockTitleBarWidget::CDockTitleBarWidget(QDockWidget* dockWidget)
	: m_dockWidget(dockWidget)
{
	CTitleBarText* textWidget = new CTitleBarText(this, dockWidget);

	m_layout = new QBoxLayout(QBoxLayout::LeftToRight);
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_layout->setSpacing(0);
	m_layout->addWidget(textWidget, 1);

	m_buttonLayout = new QBoxLayout(QBoxLayout::RightToLeft);
	m_buttonLayout->setContentsMargins(0, 0, 0, 0);
	m_buttonLayout->setSpacing(0);
	m_layout->addLayout(m_buttonLayout, 0);

	setLayout(m_layout);
}

QAbstractButton* CDockTitleBarWidget::AddButton(const QIcon& icon, const char* tooltip)
{
	auto button = new CDockWidgetTitleButton(m_dockWidget);
	button->setIcon(icon);
	button->setToolTip(tooltip);
	m_buttonLayout->addWidget(button);
	return button;
}

CCustomDockWidget::CCustomDockWidget(const char* title, QWidget* parent, Qt::WindowFlags flags)
	: QDockWidget(title, parent, flags)
{
	const auto titleBar = new CDockTitleBarWidget(this);
	setTitleBarWidget(titleBar);

	{
		const auto closeButton = titleBar->AddButton(CryIcon("icons:Window/Window_Close.ico"), "Close");
		closeButton->setVisible(features() & QDockWidget::DockWidgetClosable);
		EXPECTED(connect(closeButton, &QAbstractButton::clicked, [this]() { close(); }));
	}

	{
		const auto floatButton = titleBar->AddButton(CryIcon("icons:Window/Window_Maximize.ico"), "Toggle Floating");
		floatButton->setVisible(features() & QDockWidget::DockWidgetFloatable);
		EXPECTED(connect(floatButton, &QAbstractButton::clicked, [this]() { setFloating(!isFloating()); }));
	}
}

