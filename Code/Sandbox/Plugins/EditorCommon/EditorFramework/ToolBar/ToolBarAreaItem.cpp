// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ToolBarAreaItem.h"

//EditorCommon
#include "DragDrop.h"
#include "EditorFramework/Editor.h"
#include "EditorFramework/Events.h"

// CryQt
#include <CryIcon.h>

// Qt
#include <QBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QToolBar>

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(toolbar, insert_expanding_spacer,
                                                  CCommandDescription("Inserts an expanding spacer at the current position. Expanding spacers expand to get as much space as possible"))
REGISTER_EDITOR_UI_COMMAND_DESC(toolbar, insert_expanding_spacer, "Insert Expanding Spacer", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(toolbar, insert_fixed_spacer,
                                                  CCommandDescription("Inserts an fixed spacer at the current position. Fixed spacers take only the amount of space a toolbutton would"))
REGISTER_EDITOR_UI_COMMAND_DESC(toolbar, insert_fixed_spacer, "Insert Fixed Spacer", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(toolbar, remove_spacer,
                                                  CCommandDescription("Removes spacer at the current position"))
REGISTER_EDITOR_UI_COMMAND_DESC(toolbar, remove_spacer, "Remove Spacer", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(toolbar, toggle_spacer_type,
                                                  CCommandDescription("Toggles the selected spacer between fixed and expanding"))
REGISTER_EDITOR_UI_COMMAND_DESC(toolbar, toggle_spacer_type, "Toggle Spacer Type", "", "", false)

class CDragHandle : public QLabel
{
public:
	CDragHandle(CToolBarAreaItem* pContent, Qt::Orientation orientation)
		: m_pContent(pContent)
		, m_hasIconOverride(false)
	{
		if (orientation == Qt::Horizontal)
		{
			setPixmap(CryIcon("icons:General/Drag_Handle_Horizontal.ico").pixmap(16, 16));
		}
		else
		{
			setPixmap(CryIcon("icons:General/Drag_Handle_Vertical.ico").pixmap(16, 16));
		}

		setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		setCursor(Qt::SizeAllCursor);
	}

	void SetOrientation(Qt::Orientation orientation)
	{
		if (m_hasIconOverride)
			return;

		if (orientation == Qt::Horizontal)
		{
			setPixmap(CryIcon("icons:General/Drag_Handle_Horizontal.ico").pixmap(16, 16));
		}
		else
		{
			setPixmap(CryIcon("icons:General/Drag_Handle_Vertical.ico").pixmap(16, 16));
		}
	}

	void SetIconOverride(const QPixmap& pixmap)
	{
		m_hasIconOverride = true;
		setPixmap(pixmap);
	}

protected:
	void mouseMoveEvent(QMouseEvent* pEvent)
	{
		if (pEvent->buttons() & Qt::LeftButton && isEnabled())
		{
			CDragDropData* pDragData = new CDragDropData();
			QByteArray byteArray;
			QDataStream stream(&byteArray, QIODevice::ReadWrite);
			stream << reinterpret_cast<intptr_t>(m_pContent);
			pDragData->SetCustomData(CToolBarAreaItem::GetMimeType(), byteArray);

			QPixmap* pPixmap = new QPixmap(m_pContent->size());
			m_pContent->render(pPixmap);

			signalDragStart();
			CDragDropData::StartDrag(this, Qt::MoveAction, pDragData, pPixmap, -pEvent->pos());
			signalDragEnd();
		}
	}

public:
	CCrySignal<void()> signalDragStart;
	CCrySignal<void()> signalDragEnd;

private:
	CToolBarAreaItem* m_pContent;
	bool              m_hasIconOverride;
};

//////////////////////////////////////////////////////////////////////////

CToolBarAreaItem::CToolBarAreaItem(CToolBarArea* pArea, Qt::Orientation orientation)
	: m_orientation(orientation)
	, m_pArea(pArea)
	, m_pContent(nullptr)
{
	m_pLayout = new QBoxLayout(m_orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
	m_pLayout->setMargin(0);
	m_pLayout->setSpacing(0);

	m_pDragHandle = new CDragHandle(this, m_orientation);
	m_pDragHandle->signalDragStart.Connect(this, &CToolBarAreaItem::OnDragStart);
	m_pDragHandle->signalDragEnd.Connect(this, &CToolBarAreaItem::OnDragEnd);
	m_pLayout->addWidget(m_pDragHandle);

	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	setLayout(m_pLayout);
}

void CToolBarAreaItem::SetContent(QWidget* pNewContent)
{
	if (!m_pContent)
	{
		m_pContent = pNewContent;
		m_pLayout->addWidget(m_pContent);
		return;
	}

	m_pLayout->replaceWidget(m_pContent, pNewContent);
	m_pContent->setVisible(false);
	pNewContent->setVisible(true);

	m_pContent->deleteLater();
	m_pContent = pNewContent;
}

void CToolBarAreaItem::SetLocked(bool isLocked)
{
	m_pDragHandle->setVisible(!isLocked);
}

void CToolBarAreaItem::SetOrientation(Qt::Orientation orientation)
{
	m_orientation = orientation;
	m_pLayout->setDirection(m_orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
	m_pDragHandle->SetOrientation(orientation);
}

void CToolBarAreaItem::OnDragStart()
{
	signalDragStart(this);
}

void CToolBarAreaItem::OnDragEnd()
{
	signalDragEnd(this);
}

void CToolBarAreaItem::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}

//////////////////////////////////////////////////////////////////////////

CToolBarItem::CToolBarItem(CToolBarArea* pArea, QToolBar* pToolBar, Qt::Orientation orientation)
	: CToolBarAreaItem(pArea, orientation)
	, m_pToolBar(pToolBar)
{
	m_pToolBar->setOrientation(m_orientation);
	SetContent(m_pToolBar);
}

void CToolBarItem::ReplaceToolBar(QToolBar* pNewToolBar)
{
	pNewToolBar->setOrientation(m_orientation);
	SetContent(pNewToolBar);
	m_pToolBar = pNewToolBar;
}

void CToolBarItem::SetOrientation(Qt::Orientation orientation)
{
	CToolBarAreaItem::SetOrientation(orientation);
	m_pLayout->setAlignment(orientation == Qt::Horizontal? Qt::AlignVCenter : Qt::AlignHCenter);
	m_pToolBar->setOrientation(m_orientation);
}

QSize CToolBarItem::GetMinimumSize() const
{
	// Get min size set on the toolbar area item
	QSize minSize = CToolBarAreaItem::GetMinimumSize();
	// Expand to toolbars min size if it exceeds the widget's min size
	minSize = minSize.expandedTo(m_pToolBar->minimumSize());
	// Expand to the toolbar layouts minimum size, this accounts for internal widget min sizes
	minSize = minSize.expandedTo(m_pToolBar->layout()->minimumSize());

	return minSize;
}

QString CToolBarItem::GetTitle() const
{
	return m_pToolBar->windowTitle();
}

QString CToolBarItem::GetName() const
{
	return m_pToolBar->objectName();
}

QToolBar* CToolBarItem::GetToolBar() const
{
	return m_pToolBar;
}

void CToolBarItem::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}

//////////////////////////////////////////////////////////////////////////

CSpacerItem::CSpacerItem(CToolBarArea* pArea, SpacerType spacerType, Qt::Orientation orientation)
	: CToolBarAreaItem(pArea, orientation)
{
	setProperty("locked", false);
	SetSpacerType(spacerType);

	m_pDragHandle->SetIconOverride(CryIcon("icons:Spacer/Drag_Handle.ico").pixmap(24, 24, QIcon::Disabled, QIcon::Off));
}

void CSpacerItem::SetLocked(bool isLocked)
{
	CToolBarAreaItem::SetLocked(isLocked);

	setProperty("locked", isLocked);
	style()->unpolish(this);
	style()->polish(this);
}

void CSpacerItem::SetOrientation(Qt::Orientation orientation)
{
	CToolBarAreaItem::SetOrientation(orientation);

	if (m_spacerType == SpacerType::Fixed)
		return;

	if (m_orientation == Qt::Horizontal)
	{
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	}
	else
	{
		setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	}
}

void CSpacerItem::SetSpacerType(SpacerType spacerType)
{
	m_spacerType = spacerType;

	if (m_spacerType == SpacerType::Expanding)
	{
		if (m_orientation == Qt::Horizontal)
		{
			setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
		}
		else
		{
			setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
		}
	}
	else
	{
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	}
}

void CSpacerItem::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}
