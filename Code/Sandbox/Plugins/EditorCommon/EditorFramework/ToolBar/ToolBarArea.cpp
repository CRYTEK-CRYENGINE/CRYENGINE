// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ToolBarArea.h"

//EditorCommon
#include "Commands/QCommandAction.h"
#include "DragDrop.h"
#include "EditorFramework/Editor.h"
#include "EditorFramework/Events.h"
#include "QtUtil.h"
#include "ToolBarCustomizeDialog.h"

// CryQt
#include <CryIcon.h>

// Qt
#include <QBoxLayout>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPainter>
#include <QStyleOption>
#include <QToolBar>

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(toolbar, customize,
                                                  CCommandDescription("Displays toolbar customization dialog"))
REGISTER_EDITOR_UI_COMMAND_DESC(toolbar, customize, "Customize...", "", "", false)

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(toolbar, toggle_lock,
                                                  CCommandDescription("Toggles toolbar locking for editing or setting as fixed"))
REGISTER_EDITOR_UI_COMMAND_DESC(toolbar, toggle_lock, "Lock Toolbars", "", "", true)

namespace Private_ToolBarArea
{
QList<CToolBarAreaItem*> GetAllItems(const CToolBarArea* pArea)
{
	return pArea->findChildren<CToolBarAreaItem*>("", Qt::FindDirectChildrenOnly);
}
}

CToolBarArea::CToolBarArea(CEditor* pParent, Qt::Orientation orientation)
	: QWidget(pParent)
	, m_pEditor(pParent)
	, m_pDropTargetSpacer(nullptr)
	, m_orientation(orientation)
	, m_isLocked(false)
{
	if (m_orientation == Qt::Horizontal)
	{
		m_pLayout = new QBoxLayout(QBoxLayout::LeftToRight);
	}
	else
	{
		m_pLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	}

	UpdateLayoutAlignment();

	setSizePolicy(QSizePolicy::Minimum , QSizePolicy::Minimum);
	m_pLayout->setSpacing(0);
	m_pLayout->setMargin(0);

	setLayout(m_pLayout);

	setAcceptDrops(true);
	setContextMenuPolicy(Qt::CustomContextMenu);
}

std::vector<CToolBarItem*> CToolBarArea::GetToolBars() const
{
	const QList<CToolBarAreaItem*> items = Private_ToolBarArea::GetAllItems(this);
	int size = items.size();
	size = 0;
	std::vector<CToolBarItem*> toolBarItems;
	toolBarItems.reserve(items.size());

	for (CToolBarAreaItem* pItem : items)
	{
		if (pItem->GetType() != CToolBarAreaItem::Type::ToolBar)
			continue;

		toolBarItems.push_back(qobject_cast<CToolBarItem*>(pItem));
	}

	return toolBarItems;
}

QSize CToolBarArea::GetLargestItemMinimumSize() const
{
	QSize minSize = minimumSize();
	const QList<CToolBarAreaItem*> items = Private_ToolBarArea::GetAllItems(this);

	for (CToolBarAreaItem* pItem : items)
	{
		minSize = minSize.expandedTo(pItem->GetMinimumSize());
	}
	
	return minSize;
}

void CToolBarArea::SetOrientation(Qt::Orientation orientation)
{
	m_orientation = orientation;
	m_pLayout->setDirection(m_orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
	UpdateLayoutAlignment();

	const QList<CToolBarAreaItem*> items = Private_ToolBarArea::GetAllItems(this);
	for (CToolBarAreaItem* pItem : items)
	{
		pItem->SetOrientation(m_orientation);
	}
}

void CToolBarArea::SetLocked(bool isLocked)
{
	m_isLocked = isLocked;
	const QList<CToolBarAreaItem*> items = Private_ToolBarArea::GetAllItems(this);

	for (CToolBarAreaItem* pItem : items)
	{
		pItem->SetLocked(isLocked);
	}
}

void CToolBarArea::OnDragStart(CToolBarAreaItem* pItem)
{
	UpdateLayoutAlignment(pItem);
	pItem->setVisible(false);
	signalDragStart();
}

void CToolBarArea::OnDragEnd(CToolBarAreaItem* pItem)
{
	pItem->setVisible(true);
	signalDragEnd();
}

void CToolBarArea::RemoveDropTargetSpacer()
{
	if (m_pDropTargetSpacer)
	{
		m_pLayout->removeItem(m_pDropTargetSpacer);
		// Explicitly delete since this is not a QObject and has no deleteLater
		delete m_pDropTargetSpacer;
		m_pDropTargetSpacer = nullptr;
	}
}

void CToolBarArea::AddItem(CToolBarAreaItem* pItem, int targetIndex)
{
	pItem->signalDragStart.Connect(this, &CToolBarArea::OnDragStart);
	pItem->signalDragEnd.Connect(this, &CToolBarArea::OnDragEnd);
	pItem->SetOrientation(m_orientation);
	pItem->SetLocked(m_isLocked);
	m_pLayout->insertWidget(targetIndex, pItem);
	pItem->SetArea(this);
}

void CToolBarArea::AddToolBar(QToolBar* pToolBar, int targetIndex)
{
	AddItem(new CToolBarItem(this, pToolBar, m_orientation), targetIndex);
}

void CToolBarArea::AddSpacer(CSpacerItem::SpacerType spacerType, int targetIndex)
{
	AddItem(new CSpacerItem(this, spacerType, m_orientation), targetIndex);

	// As soon as an expanding spacer is inserted, we must set alignment back to default,
	// otherwise the layout alignment will have preference over the spacer's size policy
	if (spacerType == CSpacerItem::SpacerType::Expanding)
		m_pLayout->setAlignment(0);
}

void CToolBarArea::RemoveItem(CToolBarAreaItem* pItem)
{
	m_pLayout->removeWidget(pItem);
	pItem->signalDragStart.DisconnectObject(this);
	pItem->signalDragEnd.DisconnectObject(this);

	UpdateLayoutAlignment(pItem);
}

void CToolBarArea::DeleteToolBar(CToolBarAreaItem* pToolBarItem)
{
	CRY_ASSERT_MESSAGE(isAncestorOf(pToolBarItem), "Trying to remove non-owned toolbar from area");
	pToolBarItem->deleteLater();
}

void CToolBarArea::HideAll()
{
	QList<CToolBarAreaItem*> items = Private_ToolBarArea::GetAllItems(this);

	for (CToolBarAreaItem* pItem : items)
	{
		pItem->setVisible(false);
	}
}

CToolBarItem* CToolBarArea::FindToolBarByName(const char* szToolBarName) const
{
	const QString name(szToolBarName);
	const QList<CToolBarItem*> toolBarItems = findChildren<CToolBarItem*>("", Qt::FindDirectChildrenOnly);
	for (CToolBarItem* pToolBarItem : toolBarItems)
	{
		if (pToolBarItem->GetName() == name)
			return pToolBarItem;
	}

	CRY_ASSERT_MESSAGE("Toolbar not found: %s", szToolBarName);

	return nullptr;
}

void CToolBarArea::FillContextMenu(CAbstractMenu* pMenu)
{
	int sec = pMenu->FindSectionByName("Spacers");
	QCommandAction* pToggleSpacerTypeAction = pMenu->CreateCommandAction("toolbar.toggle_spacer_type", sec);
	QCommandAction* pRemoveSpacerAction = pMenu->CreateCommandAction("toolbar.remove_spacer", sec);

	CToolBarAreaItem* pItem = GetItemAtPosition(m_actionContextPosition);

	if (m_isLocked || pItem && pItem->GetType() != CToolBarAreaItem::Type::Spacer)
	{
		pRemoveSpacerAction->setEnabled(false);
		pToggleSpacerTypeAction->setEnabled(false);
	}
	else if (pItem && pItem->GetType() == CToolBarAreaItem::Type::Spacer)
	{
		CSpacerItem* pSpacer = qobject_cast<CSpacerItem*>(pItem);
		if (pSpacer->GetSpacerType() == CSpacerItem::SpacerType::Expanding)
		{
			pToggleSpacerTypeAction->setText("Convert to Fixed Spacer");
		}
		else
		{
			pToggleSpacerTypeAction->setText("Convert to Expanding Spacer");
		}
	}
}

void CToolBarArea::UpdateLayoutAlignment(CToolBarAreaItem* pItemToBeRemoved)
{
	// If there are no expanding spacers left or the only spacer left is the one we removed (qt will remove next frame)
	// then make sure to reset the layout alignment or all items in the layout will be center aligned
	QList<CSpacerItem*> spacers = findChildren<CSpacerItem*>("", Qt::FindDirectChildrenOnly);
	std::vector<CSpacerItem*> expandingSpacers;
	for (CSpacerItem* pSpacer : spacers)
	{
		if (pSpacer->GetSpacerType() == CSpacerItem::SpacerType::Expanding)
		{
			expandingSpacers.push_back(pSpacer);
		}
	}

	if (expandingSpacers.empty() || !expandingSpacers.size() || (expandingSpacers.size() == 1 && expandingSpacers[0] == pItemToBeRemoved))
	{
		if (m_orientation == Qt::Horizontal)
		{
			m_pLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		}
		else
		{
			m_pLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
		}
	}
}

int CToolBarArea::IndexForItem(CToolBarAreaItem* pItem) const
{
	return m_pLayout->indexOf(pItem);
}

void CToolBarArea::MoveItem(CToolBarAreaItem* pItem, int destinationIndex)
{
	int sourceIndex = IndexForItem(pItem);

	if (sourceIndex == destinationIndex)
		return;

	m_pLayout->insertWidget(destinationIndex, pItem);
}

QVariantMap CToolBarArea::GetState() const
{
	QVariantMap state;
	QVariantList itemsVariant;

	QList<CToolBarAreaItem*> items = Private_ToolBarArea::GetAllItems(this);
	// Sort items by index
	std::sort(items.begin(), items.end(), [this, items](CToolBarAreaItem* pItemA, CToolBarAreaItem* pItemB)
	{
		return m_pLayout->indexOf(pItemA) < m_pLayout->indexOf(pItemB);
	});

	for (CToolBarAreaItem* pItem : items)
	{
		QVariantMap itemVariant;
		if (pItem->GetType() == CToolBarAreaItem::Type::ToolBar)
		{
			CToolBarItem* pToolBarItem = qobject_cast<CToolBarItem*>(pItem);
			itemVariant["name"] = pToolBarItem->GetName();
			itemVariant["visible"] = pToolBarItem->isVisible();
		}
		else if (pItem->GetType() == CToolBarAreaItem::Type::Spacer)
		{
			CSpacerItem* pSpacerItem = qobject_cast<CSpacerItem*>(pItem);
			itemVariant["spacerType"] = static_cast<int>(pSpacerItem->GetSpacerType());
		}

		itemVariant["index"] = m_pLayout->indexOf(pItem);
		itemVariant["type"] = static_cast<int>(pItem->GetType());
		itemsVariant.push_back(itemVariant);
	}

	state["items"] = itemsVariant;

	return state;
}

void CToolBarArea::SetState(const QVariantMap& state)
{
	QVariantList itemsVariant = state["items"].toList();

	for (unsigned i = 0; i < itemsVariant.size(); ++i)
	{
		const QVariantMap& item = itemsVariant[i].toMap();
		CToolBarAreaItem::Type type = static_cast<CToolBarAreaItem::Type>(item["type"].toInt());
		switch (type)
		{
		case CToolBarAreaItem::Type::Spacer:
			{
				CSpacerItem::SpacerType spacerType = static_cast<CSpacerItem::SpacerType>(item["spacerType"].toInt());
				AddSpacer(spacerType, item["index"].toInt());
			}
			break;
		case CToolBarAreaItem::Type::ToolBar:
			{
				string toolBarName = QtUtil::ToString(item["name"].toString());
				CToolBarItem* pToolBarItem = FindToolBarByName(toolBarName.c_str());

				if (!pToolBarItem)
					continue;

				pToolBarItem->setVisible(item["visible"].toBool());

				MoveItem(pToolBarItem, item["index"].toInt());
			}
			break;
		default:
			CRY_ASSERT_MESSAGE(0, "Area item Type not implemented: %d", type);
		}
	}
}

CToolBarAreaItem* CToolBarArea::GetItemAtPosition(const QPoint& globalPos)
{
	QObject* pObject = QApplication::widgetAt(globalPos);

	// If the target widget is this toolbar area, then just return
	if (!pObject || pObject == this)
	{
		return nullptr;
	}

	CToolBarAreaItem* pItem = qobject_cast<CToolBarAreaItem*>(pObject);
	while (!pItem && pObject->parent())
	{
		pObject = pObject->parent();
		pItem = qobject_cast<CToolBarAreaItem*>(pObject);
	}

	return pItem;
}

int CToolBarArea::GetPlacementIndexFromPosition(const QPoint& globalPos)
{
	// Figure out where the tool bar must be placed within the toolbar area
	int targetIndex = -1;
	CToolBarAreaItem* pItem = GetItemAtPosition(globalPos);

	// Make sure we're dropping on top of another toolbar, if not we'll just append
	if (!pItem)
	{
		QPoint localPos = mapFromGlobal(globalPos);
		// If there's a drop target, make sure we're not dropping on top of it
		if (m_pDropTargetSpacer)
		{
			QRect geom = m_pDropTargetSpacer->geometry();
			if (geom.contains(localPos))
				targetIndex = m_pLayout->indexOf(m_pDropTargetSpacer);
		}

		return targetIndex;
	}

	targetIndex = IndexForItem(pItem);

	// If the toolbar is being dropped past the half width of the toolbar,
	// then it must be placed after the toolbar under the cursor
	if (m_orientation == Qt::Horizontal && pItem->mapFromGlobal(globalPos).x() > pItem->width() / 2)
	{
		++targetIndex;
	}
	else if (m_orientation == Qt::Vertical && pItem->mapFromGlobal(globalPos).y() > pItem->height() / 2)
	{
		++targetIndex;
	}

	// If we're inserting to the end, then targetIndex must be -1
	if (targetIndex >= m_pLayout->count())
	{
		targetIndex = -1;
	}

	return targetIndex;
}

void CToolBarArea::dragEnterEvent(QDragEnterEvent* pEvent)
{
	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData(CToolBarAreaItem::GetMimeType()))
	{
		QByteArray byteArray = pDragDropData->GetCustomData(CToolBarAreaItem::GetMimeType());
		QDataStream stream(byteArray);
		intptr_t value;
		stream >> value;
		CToolBarAreaItem* pDraggedItem = reinterpret_cast<CToolBarAreaItem*>(value);

		if (!parentWidget()->isAncestorOf(pDraggedItem))
			return;

		pEvent->acceptProposedAction();

		// Set styling property for when a toolbar area is hovered
		setProperty("dragHover", true);
		style()->unpolish(this);
		style()->polish(this);
	}
}

void CToolBarArea::dragMoveEvent(QDragMoveEvent* pEvent)
{
	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData(CToolBarAreaItem::GetMimeType()))
	{
		int targetIndex = GetPlacementIndexFromPosition(mapToGlobal(pEvent->pos()));
		pEvent->acceptProposedAction();

		// If we're dragging on top of the current drop target, just return
		if (m_pDropTargetSpacer && targetIndex == m_pLayout->indexOf(m_pDropTargetSpacer))
			return;

		QByteArray byteArray = pDragDropData->GetCustomData(CToolBarAreaItem::GetMimeType());
		QDataStream stream(byteArray);
		intptr_t value;
		stream >> value;
		CToolBarAreaItem* pDraggedItem = reinterpret_cast<CToolBarAreaItem*>(value);

		if (!parentWidget()->isAncestorOf(pDraggedItem))
			return;

		int spacerWidth = pDraggedItem->width();
		int spacerHeight = pDraggedItem->height();

		// If orientation of dragged toolbar doesn't match our orientation, then flip width/height
		if (pDraggedItem->GetOrientation() != m_orientation)
		{
			const int tempWidth = spacerWidth;
			spacerWidth = spacerHeight;
			spacerHeight = tempWidth;
		}

		if (!m_pDropTargetSpacer)
		{
			m_pDropTargetSpacer = new QSpacerItem(spacerWidth, spacerHeight, QSizePolicy::Fixed, QSizePolicy::Fixed);
		}
		else
		{
			int spacerIndex = m_pLayout->indexOf(m_pDropTargetSpacer);
			if (spacerIndex == targetIndex - 1 || (targetIndex == -1 && spacerIndex == m_pLayout->count() - 1))
				return;

			RemoveDropTargetSpacer();
			m_pDropTargetSpacer = new QSpacerItem(spacerWidth, spacerHeight, QSizePolicy::Fixed, QSizePolicy::Fixed);
		}

		m_pLayout->insertSpacerItem(targetIndex, m_pDropTargetSpacer);
	}
}

void CToolBarArea::dragLeaveEvent(QDragLeaveEvent* pEvent)
{
	RemoveDropTargetSpacer();

	// Set styling property for when a toolbar area is no longer hovered
	setProperty("dragHover", false);
	style()->unpolish(this);
	style()->polish(this);

	pEvent->accept();
}

void CToolBarArea::dropEvent(QDropEvent* pEvent)
{
	const CDragDropData* pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData(CToolBarAreaItem::GetMimeType()))
	{
		pEvent->acceptProposedAction();
		QByteArray byteArray = pDragDropData->GetCustomData(CToolBarAreaItem::GetMimeType());
		QDataStream stream(byteArray);
		intptr_t value;
		stream >> value;
		CToolBarAreaItem* pItem = reinterpret_cast<CToolBarAreaItem*>(value);

		int targetIndex = -1;

		if (m_pDropTargetSpacer)
		{
			targetIndex = m_pLayout->indexOf(m_pDropTargetSpacer);

			// If the dragged tool bar is contained in this area already,
			// then make sure to compensate for it being removed from the layout
			int containerIndex = m_pLayout->indexOf(pItem);
			if (containerIndex >= 0 && containerIndex < targetIndex)
			{
				--targetIndex;
			}

			RemoveDropTargetSpacer();
		}

		if (targetIndex >= m_pLayout->count())
		{
			targetIndex = -1;
		}

		// Signal the manager to move the toolbar from it's current area to this area
		signalItemDropped(pItem, this, targetIndex);
	}
}

void CToolBarArea::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(pEvent);

		const string& command = commandEvent->GetCommand();

		if (command == "toolbar.insert_expanding_spacer")
		{
			AddSpacer(CSpacerItem::SpacerType::Expanding, GetPlacementIndexFromPosition(m_actionContextPosition));
			pEvent->setAccepted(true);
		}
		else if (command == "toolbar.insert_fixed_spacer")
		{
			AddSpacer(CSpacerItem::SpacerType::Fixed, GetPlacementIndexFromPosition(m_actionContextPosition));
			pEvent->setAccepted(true);
		}
		else if (command == "toolbar.toggle_spacer_type")
		{
			CToolBarAreaItem* pItem = GetItemAtPosition(m_actionContextPosition);
			if (!pItem)
				return;

			CSpacerItem* pSpacerItem = qobject_cast<CSpacerItem*>(pItem);

			if (!pSpacerItem)
				return;

			if (pSpacerItem->GetSpacerType() == CSpacerItem::SpacerType::Expanding)
			{
				pSpacerItem->SetSpacerType(CSpacerItem::SpacerType::Fixed);
				UpdateLayoutAlignment();
			}
			else
			{
				pSpacerItem->SetSpacerType(CSpacerItem::SpacerType::Expanding);
				m_pLayout->setAlignment(0);
			}
			pEvent->setAccepted(true);
		}
		else if (command == "toolbar.remove_spacer")
		{
			CToolBarAreaItem* pItem = GetItemAtPosition(m_actionContextPosition);
			if (!pItem || !qobject_cast<CSpacerItem*>(pItem))
				return;

			RemoveItem(pItem);
			pItem->deleteLater();
			pEvent->setAccepted(true);
		}
	}
}

void CToolBarArea::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption styleOption;
	styleOption.init(this);
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);
}
