// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTreeFormWidget.h"

#include <QEvent.h>
#include <QStyle>
#include <QToolTip>
#include <QClipboard>
#include <QPainter>
#include <QStyleOption>
#include <QApplication>
#include <QMenu>

#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

#include "QtUtil.h"
#include "DragDrop.h"
#include <EditorFramework/Events.h>

namespace PropertyTree2
{

CFormWidget::SFormRow::SFormRow(const CRowModel* pRowModel)
	: m_pModel(pRowModel)
	, m_pChildContainer(nullptr)
	, m_pWidget(nullptr)
{
}

CFormWidget::SFormRow::~SFormRow()
{
	//Deletes the hosted CFormWidget and removes it from the current tree hierarchy
	if (m_pChildContainer)
	{
		m_pChildContainer->setParent(nullptr);
		m_pChildContainer->deleteLater();
	}
}

bool CFormWidget::SFormRow::IsExpanded() const
{
	//We rely on "local" visible flag for expansion status. isVisible() is not enough as it needs all parents to also be visible
	return m_pChildContainer && m_pChildContainer->isVisibleTo(qobject_cast<QWidget*>(m_pChildContainer->parent()));
}

QWidget* CFormWidget::SFormRow::GetQWidget()
{
	return m_pWidget;
}

IPropertyTreeWidget* CFormWidget::SFormRow::GetPropertyTreeWidget()
{
	return qobject_cast<IPropertyTreeWidget*>(m_pWidget);
}

bool CFormWidget::SFormRow::HasChildren() const
{
	//If all the children are hidden then the form row doesn't have children
	return m_pModel->HasVisibleChildren();
}

CFormWidget::CFormWidget(QPropertyTree* pParentTree, const CRowModel* pRow, int nesting)
	: m_pRowModel(pRow)
	, m_pParentTree(pParentTree)
	, m_nesting(nesting)
	, m_pActiveRow(nullptr)
	, m_dragInsertIndex(-2)
	, m_spacing(4)
	, m_minRowHeight(20)
	, m_iconSize(16)
	, m_nestingIndent(10)
	, m_groupBorderWidth(0)
	, m_groupBorderColor(Qt::transparent)
{
	setContentsMargins(0, 0, 0, 0);

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setMinimumSize(QSize(40, 500));
	setMouseTracking(true);

	SetupWidgets();
	UpdateMinimumSize();

	const IPropertyTreeWidget* pParentPtWidget = m_pRowModel ? m_pRowModel->GetPropertyTreeWidget() : nullptr;
	m_allowReorderChildren = pParentPtWidget ? pParentPtWidget->CanMoveChildren() : false;

	setAcceptDrops(m_allowReorderChildren);
	setFocusPolicy(Qt::WheelFocus);
}

CFormWidget::~CFormWidget()
{
	//detach widgets from parent relationship here because they are owned by CRowDescriptor
	ReleaseWidgets();
}

void CFormWidget::OnSplitterPosChanged()
{
	//Note: this here could be a trimmed down version of DoLayout(), which only relayouts the widgets (not row rects or child widgets)
	DoLayout(size());

	//Recurse to all children
	for (std::unique_ptr<SFormRow>& formRow : m_rows)
	{
		if (formRow->IsExpanded())
		{
			formRow->m_pChildContainer->OnSplitterPosChanged();
		}
	}

	update();
}

void CFormWidget::SetupWidgets()
{
	if (!m_pRowModel)
	{
		m_rows.clear();
		return;
	}

	if (m_pActiveRow)
	{
		//Clear active row so it can be recomputed once the widgets have changed
		SetActiveRow(nullptr);
		m_lastCursorPos = QPoint(INT_MAX, INT_MAX);
	}

	//Get all the children of the current model
	const std::vector<_smart_ptr<CRowModel>>& childrenModels = m_pRowModel->GetChildren();

	//Cleanup rows without a matching model in the new models tree
	auto rowsIterator = std::remove_if(m_rows.begin(), m_rows.end(), [&childrenModels, this](const std::unique_ptr<SFormRow>& row)
		{
			//Look for a model in use matching this row model
			bool foundModel = std::find(childrenModels.begin(), childrenModels.end(), row->m_pModel) != childrenModels.end();
			return row->m_pModel->IsRoot() || row->m_pModel->IsHidden() || !foundModel;
		});
	m_rows.erase(rowsIterator, m_rows.end());

	const int count = m_pRowModel->GetChildren().size();
	m_rows.reserve(count);

	//Sets up the widgets, parenting relationship, signal/slot mechanism, etc...
	for (int i = 0; i < count; i++)
	{
		const _smart_ptr<CRowModel>& pChild = m_pRowModel->GetChildren()[i];

		//Skip hidden rows
		if (pChild->IsHidden())
		{
			continue;
		}

		SFormRow* pFormRow = nullptr;
		//If we have a row with this model use it. If not create a new one
		auto foundRow = std::find_if(m_rows.begin(), m_rows.end(), [pChild](std::unique_ptr<SFormRow>& pFormRow)
			{
				return pFormRow->m_pModel.get() == pChild.get();
			});

		if (foundRow == m_rows.end())
		{
			m_rows.push_back(std::make_unique<SFormRow>(pChild));
			pFormRow = m_rows.back().get();
		}
		else
		{
			pFormRow = (*foundRow).get();
		}

		if (pChild->IsWidgetSet())
		{
			QWidget* pChildWidget = pChild->GetWidget();
			RecursiveInstallEventFilter(pChildWidget);   // Install event filter to catch all mouse wheel events and forward to scroll area
			pFormRow->m_pWidget = pChildWidget;

			if (pChildWidget->parent() != this)
			{
				pChildWidget->setParent(this);
				pChildWidget->setVisible(true);

				IPropertyTreeWidget* pPropertyTreeWidget = pChild->GetPropertyTreeWidget();
				if (pPropertyTreeWidget)   //The property tree supports non property tree widgets however they will not be interacting with serialization
				{
					pPropertyTreeWidget->signalDiscarded.Connect(m_pParentTree, &QPropertyTree::OnRowDiscarded);
					pPropertyTreeWidget->signalChanged.Connect(m_pParentTree, &QPropertyTree::OnRowChanged);
					pPropertyTreeWidget->signalContinuousChanged.Connect(m_pParentTree, &QPropertyTree::OnRowContinuousChanged);
					pPropertyTreeWidget->signalPreChanged.Connect(m_pParentTree, &QPropertyTree::OnRowPreChanged);
				}
			}
		}
		else if (pChild->HasChildren())
		{
			//No widget and children, let's check if there is a need for inlining child widgets
			CRowModel* pInlinedChild = pChild->GetChildren()[0];
			if (pInlinedChild->GetFlags() & CRowModel::Inline)
			{
				CInlineWidgetBox* pInlineWidgetBox = new CInlineWidgetBox(this);
				pFormRow->m_pWidget = pInlineWidgetBox;
				//in some instances widgets are hidden after spawning, this makes sure they are visible
				pInlineWidgetBox->setVisible(true);

				int index = 0;
				const int count = pChild->GetChildren().size();

				while (index < count)
				{
					pInlinedChild = pChild->GetChildren()[index];
					if (pInlinedChild->GetWidget() && pInlinedChild->GetFlags() & CRowModel::Inline)
					{
						RecursiveInstallEventFilter(pInlinedChild->GetWidget());   // Install event filter to catch all mouse wheel events and forward to scroll area
						pInlineWidgetBox->AddWidget(pInlinedChild->GetWidget(), m_pParentTree);
						index++;
					}
					else
					{
						break;
					}
				}
			}
		}

		//Auto expand only the first time (child container has not yet been created)
		if (pChild->HasVisibleChildren() && pFormRow->m_pChildContainer == nullptr)
		{
			if (pChild->GetFlags() & CRowModel::ExpandByDefault || (m_nesting < m_pParentTree->GetAutoExpandDepth() && !(pChild->GetFlags() & CRowModel::CollapseByDefault)))
			{
				//This actually creates the CFormWidget corresponding to the new SFormRow
				ToggleExpand(*pFormRow, true);
			}
		}

		pChild->MarkClean();
	}

}

void CFormWidget::RecursiveInstallEventFilter(QWidget* pWidget)
{
	pWidget->installEventFilter(this);

	QList<QWidget*> childWidgets = pWidget->findChildren<QWidget*>();
	for (QWidget* pChild : childWidgets)
	{
		RecursiveInstallEventFilter(pChild);
	}
}

void CFormWidget::ReleaseWidgets()
{
	//The widgets in the property tree do not follow by the regular Qt model of hierarchy.
	//As they are created by the row model we only transfer temporary ownership to the formWidget or parents for display, but return them afterwards.
	//CRowModel is responsible for their lifetime
	for (std::unique_ptr<SFormRow>& pRow : m_rows)
	{
		QWidget* pWidget = pRow->m_pWidget;

		if (pWidget)
		{
			pWidget->setParent(nullptr);

			IPropertyTreeWidget* pPropertyTreeWidget = pRow->GetPropertyTreeWidget();
			if (pPropertyTreeWidget)
			{
				pPropertyTreeWidget->signalDiscarded.DisconnectObject(m_pParentTree);
				pPropertyTreeWidget->signalChanged.DisconnectObject(m_pParentTree);
				pPropertyTreeWidget->signalContinuousChanged.DisconnectObject(m_pParentTree);
				pPropertyTreeWidget->signalPreChanged.DisconnectObject(m_pParentTree);
			}

			CInlineWidgetBox* pInlineWidgetBox = qobject_cast<CInlineWidgetBox*>(pWidget);
			if (pInlineWidgetBox)
			{
				pInlineWidgetBox->ReleaseWidgets(m_pParentTree);
				pInlineWidgetBox->deleteLater();
			}
		}

		pRow->m_pWidget = nullptr;
	}
}

void CFormWidget::SetSpacing(int value)
{
	if (value != m_spacing)
	{
		m_spacing = value;
		DoFullLayoutUpdate();
	}
}

void CFormWidget::SetMinRowHeight(int value)
{
	if (value != m_minRowHeight)
	{
		m_minRowHeight = value;
		DoFullLayoutUpdate();
	}
}

void CFormWidget::SetIconSize(int value)
{
	if (value != m_iconSize)
	{
		m_iconSize = value;
		update();   //only requires repaint, not full layout
	}
}

void CFormWidget::SetGroupBorderWidth(int value)
{
	if (value != m_groupBorderWidth)
	{
		m_groupBorderWidth = value;
		DoFullLayoutUpdate();
	}
}

void CFormWidget::SetGroupBorderColor(const QColor& color)
{
	if (m_groupBorderColor != color)
	{
		m_groupBorderColor = color;
		update();
	}
}

void CFormWidget::SetMargins(const QRect& margins)
{
	if (margins != GetMargins())
	{
		setContentsMargins(QMargins(margins.left(), margins.top(), margins.width(), margins.height()));
		DoFullLayoutUpdate();
	}
}

QRect CFormWidget::GetMargins() const
{
	const QMargins margins = contentsMargins();
	return QRect(margins.left(), margins.top(), margins.right(), margins.bottom());
}

CFormWidget* CFormWidget::ExpandRow(const CRowModel* pRow, bool expand)
{
	CRY_ASSERT(pRow && pRow->GetParent() == m_pRowModel && pRow->HasVisibleChildren());

	SFormRow* pFormRow = ModelToFormRow(pRow);
	CRY_ASSERT(pFormRow);

	if (pFormRow->IsExpanded() != expand)
	{
		ToggleExpand(*pFormRow);
	}

	return pFormRow->m_pChildContainer;
}

CFormWidget* CFormWidget::ExpandToRow(const CRowModel* pRow)
{
	CRY_ASSERT(pRow && !pRow->IsRoot());

	if (pRow->GetParent() == m_pRowModel)
	{
		return this;
	}
	else
	{
		//Expand parent row (recursive)
		CFormWidget* pFormWidget = ExpandToRow(pRow->GetParent());
		CFormWidget* pChildFormWidget = pFormWidget->ExpandRow(pRow->GetParent(), true);
		return pChildFormWidget;
	}
}

void CFormWidget::ScrollToRow(const CRowModel* pRow)
{
	const SFormRow* pFormRow = ModelToFormRow(pRow);
	if (!pFormRow)
	{
		return;
	}

	QScrollArea* pScrollArea = m_pParentTree->GetScrollArea();

	QPoint formBottomRightToScrollArea = mapTo(pScrollArea->widget(), pFormRow->rowRect.bottomRight());
	pScrollArea->ensureVisible(formBottomRightToScrollArea.x(), formBottomRightToScrollArea.y(), 0, 0);

	QPoint formTopLeftToScrollArea = mapTo(pScrollArea->widget(), pFormRow->rowRect.topLeft());
	pScrollArea->ensureVisible(formTopLeftToScrollArea.x(), formTopLeftToScrollArea.y(), 0, 0);

}

void CFormWidget::UpdateTree()
{
	if (m_pRowModel->IsClean())
	{
		return;
	}

	bool doSetup = false;

	if (m_pRowModel->HasDirtyChildren())
	{
		for (std::unique_ptr<SFormRow>& pRow : m_rows)
		{
			if (pRow->m_pChildContainer)
			{
				if (pRow->m_pModel->IsClean())
				{
					continue;
				}

				pRow->m_pChildContainer->UpdateTree();

				//Clean up rows that no longer have children
				if (!pRow->m_pModel->HasChildren())
				{
					pRow->m_pChildContainer->deleteLater();
					pRow->m_pChildContainer = nullptr;

					doSetup = true;
				}
			}
			else if (pRow->m_pModel->HasFirstTimeChildren())   //Expand rows that have children for the first time
			{
				ToggleExpand(*pRow);
			}
		}
	}

	if (doSetup || m_pRowModel->IsDirty())
	{
		SetupWidgets();
		UpdateMinimumSize();
	}

	m_pRowModel->MarkClean();
	update();
}

void CFormWidget::SetNestingIndent(int value)
{
	m_nestingIndent = value;
	update();   //only requires repaint, not full layout
}

bool CFormWidget::eventFilter(QObject* pWatched, QEvent* pEvent)
{
	// The property tree takes ownership of wheel events. No widget should handle wheel events
	// since scrolling the property tree would be unreliable when hovering over widgets
	if (pEvent->type() == QEvent::Wheel)
	{
		m_pParentTree->HandleScroll(static_cast<QWheelEvent*>(pEvent));
		return true;
	}

	return false;
}

void CFormWidget::paintEvent(QPaintEvent* pEvent)
{
	QStyle* pStyle = QWidget::style();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QStyleOption styleOption;
	styleOption.initFrom(this);

	const QPoint cursorPosition = mapFromGlobal(QCursor::pos());

	//Update active row for cases that wouldn't trigger mouse events, such as moving between widgets on the right side.
	if (!m_pParentTree->GetActiveRow() && rect().contains(cursorPosition))
	{
		QObject* pWidget = QApplication::widgetAt(QCursor::pos());
		CFormWidget* pForm = nullptr;
		while (pWidget && !pForm)
		{
			pWidget = pWidget->parent();
			pForm = qobject_cast<CFormWidget*>(pWidget);
		}
		if (pForm == this)
		{
			UpdateActiveRow(cursorPosition);
		}
	}

	//Paint background due to group widgets
	painter.fillRect(rect(), palette().background());

	//Paint the drag highlight
	//TODO: problem drawing before the first row or after the last row as it is not in this widget's rect
	if (m_dragInsertIndex >= 0 && m_dragInsertIndex < m_rows.size())
	{
		const int index = m_dragInsertIndex;
		QRect highlightRect = m_rows[index]->rowRect;
		highlightRect.setTop(highlightRect.top() - m_spacing);
		highlightRect.setBottom(highlightRect.top() + m_spacing - 1);

		//In QSS this color is set by the selection-background-color property.
		painter.fillRect(highlightRect, palette().highlight());
	}

	const bool isDragging = m_dragInsertIndex != -2 || m_pParentTree->IsDraggingSplitter();
	const int flags = Qt::AlignVCenter | Qt::TextForceLeftToRight | Qt::TextSingleLine;
	const int leftMargin = contentsMargins().left();
	const int textIndent = leftMargin + m_groupBorderWidth + m_nesting * m_nestingIndent + m_iconSize + m_spacing;

	const int count = m_rows.size();
	for (int i = 0; i < count; i++)
	{
		const std::unique_ptr<SFormRow>& pRow = m_rows[i];
		const _smart_ptr<const CRowModel>& rowModel = pRow->m_pModel;

		//Draw group frame
		if (pRow->HasChildren() && m_groupBorderWidth > 0)
		{
			QRect rowFrame = pRow->rowRect;
			QPainterPath path;
			path.addRoundedRect(rowFrame, 3, 3);

			painter.fillPath(path, m_pActiveRow != pRow.get() ? m_groupBorderColor : palette().alternateBase());
		}
		else if (m_pActiveRow == pRow.get() && !isDragging)   //draw highlighted row if not in any drag mode
		{
			//In QSS this color is set by the alternate-background-color property.
			painter.fillRect(pRow->rowRect, palette().alternateBase());
		}

		//Draw labels

		QRect textRect = pRow->rowRect;
		textRect.setLeft(textRect.left() + textIndent);
		textRect.setRight(GetSplitterPosition());

		pStyle->drawItemText(&painter, textRect, flags, styleOption.palette, isEnabled(), rowModel->GetLabel(), foregroundRole());

		//Draw expand icon
		if (rowModel->HasVisibleChildren())
		{
			const QRect iconRect = GetExpandIconRect(*pRow);
			const bool bActive = iconRect.contains(cursorPosition);

			if (pRow->IsExpanded())
			{
				m_pParentTree->GetExpandedIcon().paint(&painter, iconRect, Qt::AlignCenter, bActive ? QIcon::Active : QIcon::Normal);
			}
			else
			{
				m_pParentTree->GetCollapsedIcon().paint(&painter, iconRect, Qt::AlignCenter, bActive ? QIcon::Active : QIcon::Normal);
			}
		}

		//Draw drag icon
		if (m_allowReorderChildren)
		{
			const QRect iconRect = GetDragHandleIconRect(*pRow);
			m_pParentTree->GetDragHandleIcon().paint(&painter, iconRect, Qt::AlignCenter, QIcon::Normal);
		}
	}

	QWidget::paintEvent(pEvent);
}

QRect CFormWidget::GetExpandIconRect(const SFormRow& row) const
{
	QRect iconRect = row.rowRect;
	if (m_groupBorderWidth > 0 && row.HasChildren())
	{
		iconRect.setLeft(iconRect.left() + m_groupBorderWidth);
	}

	iconRect.setLeft(iconRect.left() + contentsMargins().left() + m_nesting * m_nestingIndent);
	iconRect.setWidth(m_iconSize);
	return iconRect;
}

QRect CFormWidget::GetDragHandleIconRect(const SFormRow& row) const
{
	QRect iconRect = row.rowRect;
	if (m_groupBorderWidth > 0 && row.HasChildren())
	{
		iconRect.setRight(iconRect.right() - m_groupBorderWidth);
	}

	iconRect.setLeft(iconRect.right() - contentsMargins().right() - m_iconSize);
	iconRect.setWidth(m_iconSize);
	return iconRect;
}

void CFormWidget::ToggleExpand(SFormRow& row, bool skipLayout /* = false*/)
{
	if (row.IsExpanded())
	{
		row.m_pChildContainer->setVisible(false);
	}
	else
	{
		if (row.m_pChildContainer)
		{
			row.m_pChildContainer->setVisible(true);
		}
		else
		{
			//Create child container
			row.m_pChildContainer = new CFormWidget(m_pParentTree, row.m_pModel, m_nesting + 1);
			row.m_pChildContainer->setParent(this);
			row.m_pChildContainer->setVisible(true);
		}
	}

	if (!skipLayout)
	{
		DoFullLayoutUpdate();
	}
}

void CFormWidget::resizeEvent(QResizeEvent* pEvent)
{
	QWidget::resizeEvent(pEvent);
	DoLayout(pEvent->size());
}

void CFormWidget::enterEvent(QEvent* pEvent)
{
	const QPoint cursorPos = mapFromGlobal(QCursor::pos());
	UpdateActiveRow(cursorPos);
}

void CFormWidget::leaveEvent(QEvent* pEvent)
{
	if (m_pActiveRow)
	{
		const QPoint cursorPos = mapFromGlobal(QCursor::pos());
		UpdateActiveRow(cursorPos);
	}
}

void CFormWidget::contextMenuEvent(QContextMenuEvent* pEvent)
{
	if (!m_pActiveRow)
	{
		return;
	}

	QMenu* pMenu = new QMenu();

	//Active row
	if (m_pActiveRow->m_pModel->GetPropertyTreeWidget())
	{
		m_pActiveRow->m_pModel->GetPropertyTreeWidget()->PopulateContextMenu(pMenu, m_pActiveRow->m_pModel);
	}

	pMenu->addSeparator();

	//Parent row may add some entries
	if (m_pRowModel->GetPropertyTreeWidget())
	{
		m_pRowModel->GetPropertyTreeWidget()->PopulateChildContextMenu(pMenu, m_pActiveRow->m_pModel);
	}

	pMenu->addSeparator();

	//Generic entries
	QAction* pAction = pMenu->addAction(tr("Copy"));
	connect(pAction, &QAction::triggered, this, &CFormWidget::OnCopy);

	pAction = pMenu->addAction(tr("Paste"));
	connect(pAction, &QAction::triggered, this, &CFormWidget::OnPaste);

	if (pEvent->reason() == QContextMenuEvent::Reason::Mouse)
	{
		//Spawned by the mouse, place at mouse position
		pMenu->popup(pEvent->globalPos());
	}
	else
	{
		//If this is not spawned by the mouse we move the menu to a position from top left that corresponds to half the height on both x and y
		int offset = static_cast<int>(m_pActiveRow->rowRect.height() / 2);
		pMenu->popup(mapToGlobal(QPoint(m_pActiveRow->rowRect.topLeft().x() + offset, m_pActiveRow->rowRect.topLeft().y() + offset)));
	}
}

void CFormWidget::mousePressEvent(QMouseEvent* pEvent)
{
	UpdateActiveRow(pEvent->pos());

	if (pEvent->buttons() & Qt::LeftButton)
	{
		m_mouseDownPos = m_lastCursorPos;
	}

	if (m_pActiveRow)
	{
		if (m_pActiveRow->m_pModel->HasVisibleChildren())
		{
			//Use the whole row rect as a target for Expand/Collapse
			if (m_pActiveRow->rowRect.marginsAdded(QMargins(2, 0, 2, 0)).contains(pEvent->pos()))
			{
				ToggleExpand(*m_pActiveRow);
			}
		}
	}

	const int splitterPos = GetSplitterPosition();
	if (abs(pEvent->x() - splitterPos) <= 5)
	{
		m_pParentTree->SetDraggingSplitter(true);
		grabMouse();
		update();
	}
}

void CFormWidget::mouseMoveEvent(QMouseEvent* pEvent)
{
	UpdateActiveRow(pEvent->pos());

	const bool isInsideWidget = rect().contains(pEvent->pos());

	if (isInsideWidget && m_pActiveRow && m_pActiveRow->m_pModel->HasVisibleChildren())
	{
		//Repaint when nearing the icon to ensure the active state is updated correctly
		QRect iconRect = GetExpandIconRect(*m_pActiveRow);
		if (iconRect.marginsAdded(QMargins(2, 0, 2, 0)).contains(pEvent->pos()))
		{
			update();   //Trigger repaint
		}
	}

	if (m_pParentTree->IsDraggingSplitter())
	{
		m_pParentTree->SetSplitterPosition(mapTo(m_pParentTree, pEvent->pos()).x());
	}
	else
	{
		const int splitterPos = GetSplitterPosition();
		const int sliderHalfAreaWidth = 5;
		if (abs(pEvent->x() - splitterPos) <= sliderHalfAreaWidth)
		{
			setCursor(QCursor(Qt::SplitHCursor));
		}
		else if (m_pActiveRow && m_allowReorderChildren && GetDragHandleIconRect(*m_pActiveRow).contains(pEvent->pos())) //we need to check if children can be dragged before enabling dragging
		{
			setCursor(QCursor(Qt::OpenHandCursor));
		}
		else
		{
			setCursor(QCursor(Qt::ArrowCursor));
		}

		if (pEvent->buttons() & Qt::LeftButton && isEnabled())
		{
			if (m_allowReorderChildren && (m_mouseDownPos - pEvent->pos()).manhattanLength() > QApplication::startDragDistance())
			{
				int rowIndex = RowIndexAtPosition(m_mouseDownPos);
				if (rowIndex != -1)
				{
					PropertyTree2::CFormWidget::SFormRow& pDraggedRow = *m_rows[rowIndex];

					CDragDropData* pDragData = new CDragDropData();
					pDragData->SetCustomData("PropertyRow", pDraggedRow.m_pModel);

					QPixmap* pPixmap = new QPixmap(pDraggedRow.rowRect.size());
					render(pPixmap, QPoint(), QRegion(pDraggedRow.rowRect));

					CDragDropData::StartDrag(this, Qt::MoveAction, pDragData, pPixmap, pDraggedRow.rowRect.topLeft() - pEvent->pos());
				}
			}
		}
	}
}

void CFormWidget::mouseReleaseEvent(QMouseEvent* pEvent)
{
	if (m_pParentTree->IsDraggingSplitter())
	{
		releaseMouse();
		m_pParentTree->SetDraggingSplitter(false);
		setCursor(QCursor(Qt::ArrowCursor));
		update();
	}
}

void CFormWidget::dragEnterEvent(QDragEnterEvent* pEvent)
{
	const CDragDropData* pDragData = CDragDropData::FromDragDropEvent(pEvent);
	if (pDragData->HasCustomData("PropertyRow"))
	{
		const CRowModel* pDraggedRowModel = pDragData->GetCustomData<CRowModel*>("PropertyRow");
		if (pDraggedRowModel->GetParent() == m_pRowModel && m_allowReorderChildren)
		{
			pEvent->acceptProposedAction();
		}
	}

	m_dragInsertIndex = -1;
	SetActiveRow(nullptr);
}

void CFormWidget::dragMoveEvent(QDragMoveEvent* pEvent)
{
	UpdateActiveRow(pEvent->pos());

	QWidget::dragMoveEvent(pEvent);

	const int oldDragIndex = m_dragInsertIndex;
	m_dragInsertIndex = InsertIndexAtPosition(pEvent->pos());

	if (m_dragInsertIndex != oldDragIndex)
	{
		update();
	}

	if (m_dragInsertIndex >= 0)
	{
		const CDragDropData* pDragData = CDragDropData::FromDragDropEvent(pEvent);
		if (pDragData->HasCustomData("PropertyRow"))
		{
			const CRowModel* pDraggedRowModel = pDragData->GetCustomData<CRowModel*>("PropertyRow");

			//Note: Because the only case for reordering is arrays which should not have hidden rows, we take the model index at face value
			const int index = pDraggedRowModel->GetIndex();

			//A row cannot be dropped on itself or below itself
			//TODO: Currently it is difficult to handle drop are above the first element and below the last element. This is due to the widget hierarchy, not being able to paint above or below.
			//A solution could be to add the spacing inside the child form widget so it can handle it, or otherwise have a global overlay for highlighting. For now these cases are not handled and guarded against here.
			if (index == m_dragInsertIndex || index == m_dragInsertIndex - 1 || m_dragInsertIndex == 0 || m_dragInsertIndex == m_rows.size())
			{
				m_dragInsertIndex = -1;
			}
			else
			{
				pEvent->acceptProposedAction();
				return;
			}
		}
	}

	pEvent->ignore();
}

void CFormWidget::dragLeaveEvent(QDragLeaveEvent* pEvent)
{
	m_dragInsertIndex = -2;
	if (m_pActiveRow)
	{
		const QPoint cursorPos = mapFromGlobal(QCursor::pos());
		UpdateActiveRow(cursorPos);
	}
}

void CFormWidget::dropEvent(QDropEvent* pEvent)
{
	m_dragInsertIndex = -2;
	const QPoint cursorPos = mapFromGlobal(QCursor::pos());
	UpdateActiveRow(cursorPos);

	const CDragDropData* pDragData = CDragDropData::FromDragDropEvent(pEvent);
	if (pDragData->HasCustomData("PropertyRow"))
	{
		const CRowModel* pDraggedRowModel = pDragData->GetCustomData<CRowModel*>("PropertyRow");
		if (pDraggedRowModel->GetParent() == m_pRowModel && m_allowReorderChildren)
		{
			const int insertIndex = InsertIndexAtPosition(pEvent->pos());

			IPropertyTreeWidget* pPropertyTreeWidget = m_pRowModel->GetPropertyTreeWidget();
			CRY_ASSERT(pPropertyTreeWidget && pPropertyTreeWidget->CanMoveChildren());

			//Note: Because the only case for reordering is arrays which should not have hidden rows, we take the model index at face value
			const int index = pDraggedRowModel->GetIndex();
			pPropertyTreeWidget->MoveChild(index, index < insertIndex ? insertIndex - 1 : insertIndex);
			pEvent->acceptProposedAction();
		}
	}
}

void CFormWidget::keyPressEvent(QKeyEvent* pEvent)
{
	if (!m_pActiveRow)
	{
		//If the form widget with active row is not in focus, the root form will dispatch the message properly
		if (m_pRowModel->IsRoot())
		{
			const CRowModel* pRow = m_pParentTree->GetActiveRow();
			if (pRow && pRow->GetParent() != m_pRowModel)
			{
				CFormWidget* pForm = ExpandToRow(pRow);
				CRY_ASSERT(pForm != this);

				//Don't use qapp::sendEvent because it will lead back here and recurse until stack overflow
				pForm->event(pEvent);
			}
		}

		pEvent->ignore();
		return;
	}

	switch (pEvent->key())
	{
	case Qt::Key_Up:
		{
			const int index = RowIndex(m_pActiveRow);

			if (index > 0)
			{
				const SFormRow* pRow = m_rows[index - 1].get();
				CFormWidget* pForm = this;
				while (pRow->IsExpanded())
				{
					pForm = pRow->m_pChildContainer;
					pRow = pRow->m_pChildContainer->m_rows.back().get();
				}

				m_pParentTree->EnsureRowVisible(pRow->m_pModel);
				m_pParentTree->SetActiveRow(pRow->m_pModel);
				pForm->setFocus();
			}
			else if (!m_pRowModel->IsRoot())
			{
				m_pParentTree->EnsureRowVisible(m_pRowModel);
				m_pParentTree->SetActiveRow(m_pRowModel);
				CFormWidget* pParentForm = qobject_cast<CFormWidget*>(parent());
				if (pParentForm)
				{
					pParentForm->setFocus();
				}
			}

			break;
		}
	case Qt::Key_Down:
		{
			//Get first child
			if (m_pActiveRow->m_pModel->HasVisibleChildren() && m_pActiveRow->IsExpanded())
			{
				CFormWidget* pForm = m_pActiveRow->m_pChildContainer;
				CRY_ASSERT(pForm->m_rows.size() > 0);
				pForm->setFocus();
				m_pParentTree->EnsureRowVisible(pForm->m_rows[0]->m_pModel);
				m_pParentTree->SetActiveRow(pForm->m_rows[0]->m_pModel);
			}
			else
			{
				SFormRow* pFormRow = m_pActiveRow;
				CFormWidget* pFormWidget = this;

				while (pFormWidget && pFormRow)
				{
					//Get next sibling
					const int formRowIndex = pFormWidget->RowIndex(pFormRow);
					const size_t formRowsCount = pFormWidget->m_rows.size();
					if (formRowsCount - 1 > formRowIndex)
					{
						pFormRow = pFormWidget->m_rows[formRowIndex + 1].get();
						break;
					}

					//Or go to parent for next loop

					CFormWidget* pParentForm = qobject_cast<CFormWidget*>(pFormWidget->parent());
					if (pParentForm)
					{
						const int index = pParentForm->RowIndex(pParentForm->ModelToFormRow(pFormWidget->m_pRowModel));
						pFormRow = pParentForm->m_rows[index].get();
					}
					pFormWidget = pParentForm;

				}

				if (pFormWidget && pFormRow)
				{
					m_pParentTree->EnsureRowVisible(pFormRow->m_pModel);
					m_pParentTree->SetActiveRow(pFormRow->m_pModel);
					pFormWidget->setFocus();
				}
			}
			break;
		}
	case Qt::Key_Left:
		{
			if (m_pActiveRow->IsExpanded())
			{
				ToggleExpand(*m_pActiveRow);
			}
			else if (!m_pRowModel->IsRoot()) //select parent row
			{
				CFormWidget* form = qobject_cast<CFormWidget*>(parent());
				if (form)
				{
					m_pParentTree->EnsureRowVisible(m_pRowModel);
					m_pParentTree->SetActiveRow(m_pRowModel);
					form->setFocus();
				}
			}
			break;
		}
	case Qt::Key_Right:
		{
			if (m_pActiveRow->m_pModel->HasVisibleChildren() && !m_pActiveRow->IsExpanded())
				ToggleExpand(*m_pActiveRow);
			break;
		}
	default:
		QWidget::keyPressEvent(pEvent);
	}
}

bool CFormWidget::event(QEvent* pEvent)
{
	switch (pEvent->type())
	{
	/* All the following events are reimplemented from QWidget::event to allow usage when the widget is disabled
	   Note that keyPressEvent cannot be handled as disabled widgets cannot get focus.
	   Then again, disabled property tree also disables the scroll area so it is very impractical ...*/
	case QEvent::MouseMove:
		mouseMoveEvent((QMouseEvent*)pEvent);
		break;
	case QEvent::MouseButtonPress:
		mousePressEvent((QMouseEvent*)pEvent);
		break;
	case QEvent::MouseButtonRelease:
		mouseReleaseEvent((QMouseEvent*)pEvent);
		break;
	/* End event overrides for disabled state */
	case QEvent::LayoutRequest:
		{
			DoFullLayoutUpdate();
			break;
		}
	case QEvent::ToolTip:
		{
			QHelpEvent* pHelpEvent = static_cast<QHelpEvent*>(pEvent);
			if (m_pActiveRow)
			{
				QString tooltip = m_pActiveRow->m_pModel->GetTooltip();
				if (tooltip.isEmpty())
				{
					QToolTip::hideText();
					pEvent->ignore();
				}
				else
				{
					QToolTip::hideText();
					QToolTip::showText(pHelpEvent->globalPos(), tooltip);
				}
			}

			break;
		}
	case SandboxEvent::Command:
		{
			CommandEvent* pCommandEvent = static_cast<CommandEvent*>(pEvent);

			const string& command = pCommandEvent->GetCommand();

			if (command == "general.copy" || command == "general.cut")
			{
				OnCopy();
				pEvent->accept();
			}
			else if (command == "general.paste")
			{
				OnPaste();
				pEvent->accept();
			}
			else
			{
				pEvent->ignore();
				return false;
			}
			break;
		}
	default:
		return QWidget::event(pEvent);
	}

	return true;
}

void CFormWidget::DoFullLayoutUpdate()
{
	//TODO: layout is called too often.
	//Update min size already triggers a resize event/ relayout so doLayout should not be called again
	//Also doLayout could have the wrong size!

	UpdateMinimumSize();
	DoLayout(size());
	update();
}

QSize CFormWidget::sizeHint() const
{
	return minimumSize();
}

QSize CFormWidget::minimumSizeHint() const
{
	return minimumSize();
}

bool CFormWidget::focusNextPrevChild(bool next)
{
	//Ensure that tab will target the current active row's widget if possible
	QWidget* pCandidate = focusWidget();
	if (pCandidate->hasFocus() && !qobject_cast<CFormWidget*>(pCandidate))
	{
		return QWidget::focusNextPrevChild(next);
	}
	else if (m_pParentTree->GetActiveRow() != nullptr)
	{
		//TODO: with inlining this may not be the right widget, tab navigation needs to be adressed
		if (m_pParentTree->GetActiveRow()->IsWidgetSet())
		{
			m_pParentTree->GetActiveRow()->GetWidget()->setFocus();
			return true;
		}
		else
		{
			return false;
		}
	}

	return QWidget::focusNextPrevChild(next);
}

void CFormWidget::UpdateMinimumSize()
{
	//Note: this mimics the structure and behavior of UpdateMinimumSize, keep in sync!
	int height = contentsMargins().top() + contentsMargins().bottom();

	int widgetsMinWidth = 0;
	int nestedFormsMinWidth = 0;

	const int childCount = m_rows.size();
	if (childCount > 0)
	{
		height += (childCount - 1) * m_spacing;
	}

	for (int i = 0; i < childCount; i++)
	{
		const SFormRow& row = *m_rows[i];
		const int groupPadding = row.HasChildren() ? m_groupBorderWidth * 2 : 0;

		height += groupPadding;

		QWidget* pWidget = row.m_pWidget;
		if (pWidget)
		{
			const QSize sizeHint = pWidget->minimumSizeHint();
			height += max(sizeHint.height(), m_minRowHeight);
			widgetsMinWidth = max(widgetsMinWidth, sizeHint.width() + groupPadding);
		}
		else
		{
			height += m_minRowHeight;
		}

		if (row.m_pChildContainer && row.IsExpanded())
		{
			const QSize minSize = row.m_pChildContainer->minimumSizeHint();
			height += minSize.height() + (m_groupBorderWidth == 0 ? m_spacing : m_groupBorderWidth);
			nestedFormsMinWidth = max(nestedFormsMinWidth, minSize.width() + groupPadding);
		}
	}

	//reserve some space for the labels //TODO: this must be enforced when sizing down so widgets are not actually laid out past their min width
	const int labelsMinWidth = 20;
	int width = contentsMargins().left() + contentsMargins().right() + labelsMinWidth + widgetsMinWidth;

	if (m_allowReorderChildren)
	{
		width += m_iconSize + m_spacing;
	}

	width = max(width, nestedFormsMinWidth);

	setMinimumSize(width, height);
}

void CFormWidget::DoLayout(const QSize& size)
{
	if (!isVisible())
	{
		return;
	}

	//Actually sets the geometry on all the children
	//Note: this mimics the structure and behavior of UpdateMinimumSize, keep in sync!

	int height = contentsMargins().top();

	//Left and right limit for content
	const int left = contentsMargins().left();
	const int right = size.width() - contentsMargins().right();

	//Left and right limit for widgets (right side of the tree)
	const int wLeft = GetSplitterPosition();
	const int wRight = right - (m_allowReorderChildren ? m_iconSize + m_spacing : 0);

	const int childCount = m_rows.size();
	for (int i = 0; i < childCount; i++)
	{
		SFormRow& row = *m_rows[i];
		const bool isGroup = row.HasChildren();

		int groupPadding = 0;
		if (isGroup)
		{
			groupPadding = m_groupBorderWidth;
		}

		QWidget* pWidget = row.m_pWidget;
		if (pWidget != nullptr)
		{
			//TODO: HeightForWidth widgets are not handled
			const QSize sizeHint = pWidget->minimumSizeHint();
			const bool expandsHorizontal = pWidget->sizePolicy().expandingDirections() & Qt::Horizontal;
			const int widgetWidth = expandsHorizontal ? wRight - wLeft - groupPadding : min(sizeHint.width(), wRight - wLeft - groupPadding);
			const int rowHeight = max(m_minRowHeight, sizeHint.height()) + 2 * groupPadding;

			row.rowRect = QRect(left, height, right - left, rowHeight);
			pWidget->setGeometry(wLeft, height + (rowHeight - sizeHint.height()) / 2, widgetWidth, sizeHint.height());
		}
		else
		{
			const int rowHeight = m_minRowHeight + 2 * groupPadding;
			row.rowRect = QRect(left, height, right - left, rowHeight);
		}

		height += row.rowRect.height();

		if (row.m_pChildContainer && row.IsExpanded())
		{
			height += m_groupBorderWidth == 0 ? m_spacing : 0;

			const int rowHeight = row.m_pChildContainer->minimumSizeHint().height();
			row.m_pChildContainer->setGeometry(left + groupPadding, height, right - groupPadding * 2 - left, rowHeight);

			height += rowHeight + groupPadding;
		}

		height += m_spacing;
	}

	//Check that the size fits expected minimum size
	//Equality cannot be tested for because if internal controls are affected by the style, minimum size is not necessarily recomputed
	//This is most certainly a bug in Qt as a style change should trigger LayoutRequest
	//Note that too many spacings was added during the loop so we remove it before testing
	if (height - m_spacing > minimumSize().height())
	{
		UpdateMinimumSize();
		CRY_ASSERT(height - m_spacing <= minimumSize().height());
	}
}

//Fakes a serializable around the tree and its widgets, for copy paste
struct SerializableWrapper
{
	SerializableWrapper(const CRowModel* pModel) : m_pModel(pModel) {}

	void Serialize(yasli::Archive& ar)
	{
		if (m_pModel->GetPropertyTreeWidget())
		{
			m_pModel->GetPropertyTreeWidget()->Serialize(ar);
		}

		for (const _smart_ptr<CRowModel>& child : m_pModel->GetChildren())
		{
			ar(SerializableWrapper(child), child->GetName(), "");
		}
	}

	const CRowModel* m_pModel;
};

void CFormWidget::OnCopy()
{
	if (!m_pActiveRow)
	{
		return;
	}

	yasli::JSONOArchive ar;
	SerializableWrapper wrapper(m_pActiveRow->m_pModel);
	ar(wrapper);

	QClipboard* pClipboard = QApplication::clipboard();
	CRY_ASSERT(pClipboard);

	QMimeData* pMimeData = new QMimeData();
	pMimeData->setText(ar.c_str());
	pClipboard->setMimeData(pMimeData);
}

void CFormWidget::OnPaste()
{
	if (!m_pActiveRow)
	{
		return;
	}

	QClipboard* pClipboard = QApplication::clipboard();
	CRY_ASSERT(pClipboard);

	if (pClipboard->mimeData()->hasText())
	{
		string text = pClipboard->mimeData()->text().toStdString().c_str();

		yasli::JSONIArchive ar;
		ar.open(text.data(), text.length());

		m_pParentTree->SetAccumulateChanges(true);

		SerializableWrapper wrapper(m_pActiveRow->m_pModel);
		ar(wrapper);

		//Force only one undo notification even though this may trigger many serialization passes
		m_pParentTree->SetAccumulateChanges(false);
	}
}

int CFormWidget::GetSplitterPosition() const
{
	const int splitterPos = m_pParentTree->GetSplitterPosition();
	return mapFrom(m_pParentTree, QPoint(splitterPos, 0)).x();
}

PropertyTree2::CFormWidget::SFormRow* CFormWidget::ModelToFormRow(const CRowModel* pRowModel)
{
	CRY_ASSERT(pRowModel && pRowModel->GetParent() == m_pRowModel && !pRowModel->IsHidden());

	const int index = pRowModel->GetIndex();

	//Because of hidden rows, the index of the form row is either at the index or before. Start at index and work our way back to 0.
	auto beginIt = index >= m_rows.size() ? m_rows.rbegin() : m_rows.rbegin() + (m_rows.size() - index - 1);
	auto rowsIterator = std::find_if(beginIt, m_rows.rend(), [&](const std::unique_ptr<SFormRow>& pFormRow) { return pFormRow->m_pModel.get() == pRowModel; });
	if (rowsIterator != m_rows.rend())
	{
		return (*rowsIterator).get();
	}
	else
	{
		return nullptr;
	}
}

PropertyTree2::CFormWidget::SFormRow* CFormWidget::RowAtPosition(const QPoint& position)
{
	int index = RowIndexAtPosition(position);
	if (index != -1)
	{
		return m_rows[index].get();
	}
	return nullptr;
}

size_t CFormWidget::RowIndexAtPosition(const QPoint& position)
{
	//Binary search through the rows, using only vertical position
	int low = 0;
	int high = m_rows.size() - 1;

	while (low <= high)
	{
		int mid = (low + high) / 2;
		SFormRow& row = *m_rows[mid];

		if (position.y() < row.rowRect.top())
		{
			high = mid - 1;
		}
		else if (position.y() > row.rowRect.bottom())
		{
			low = mid + 1;
		}
		else if (row.rowRect.contains(position))
		{
			return mid;
		}
		else
		{
			return -1;
		}
	}

	return -1;
}

int CFormWidget::InsertIndexAtPosition(const QPoint& position, int tolerancePx /*= 5*/)
{
	//Computes the insertion index when dragging something and dropping it between two rows
	//This assumes spacing + tolerance < average row size

	//TODO: if above row has a child widget then this algorithm makes it possible to drop between the top row and its children, this is strange, we really should only test the row below!

	int index = -1;

	int currentRowIndex = RowIndexAtPosition(position);
	if (currentRowIndex != -1)
	{
		SFormRow& pRow = *m_rows[currentRowIndex];

		if (!pRow.rowRect.contains(QPoint(position.x(), position.y() - tolerancePx)))
		{
			//pos is closer to the top of the row
			index = currentRowIndex;
		}
		else if (!pRow.rowRect.contains(QPoint(position.x(), position.y() + tolerancePx)))
		{
			//pos is closer to the bottom of the row
			index = currentRowIndex + 1;
		}
	}
	else   //Current position is in between rows
	{
		int rowAboveIndex = RowIndexAtPosition(QPoint(position.x(), position.y() - m_spacing));
		if (rowAboveIndex != -1)
		{
			index = rowAboveIndex + 1;
		}
		else
		{
			int rowBelowIndex = RowIndexAtPosition(QPoint(position.x(), position.y() + m_spacing));
			if (rowBelowIndex != -1)
			{
				index = rowBelowIndex;
			}
			else
			{
				rowAboveIndex = RowIndexAtPosition(QPoint(position.x(), position.y() - m_spacing - tolerancePx));
				if (rowAboveIndex != -1)
				{
					index = rowAboveIndex + 1;
				}
				else
				{
					int rowBelowIndex = RowIndexAtPosition(QPoint(position.x(), position.y() + m_spacing + tolerancePx));
					if (rowBelowIndex != -1)
					{
						index = rowBelowIndex;
					}
				}
			}
		}
	}

	//Cannot insert after the last row
	if (index >= m_rows.size())
	{
		index = -1;
	}

	return index;
}

int CFormWidget::RowIndex(const SFormRow* pRow)
{
	//find the location of the row in the m_rows array
	auto rowLocation = std::find_if(m_rows.begin(), m_rows.end(), [pRow](std::unique_ptr<SFormRow>& pFormRow)
		{
			return pFormRow.get() == pRow;
		});

	//get the distance from the beginning of the array
	return int(rowLocation - m_rows.begin());
}

void CFormWidget::OnActiveRowChanged(const CRowModel* pNewActiveRow)
{
	CRY_ASSERT(!pNewActiveRow || !pNewActiveRow->IsRoot());

	if (m_pActiveRow && m_pActiveRow->m_pModel == pNewActiveRow)
	{
		return;
	}

	if (pNewActiveRow && pNewActiveRow->GetParent() == m_pRowModel)
	{
		SetActiveRow(ModelToFormRow(pNewActiveRow));
	}
	else
	{
		SetActiveRow(nullptr);
	}

	//Recurse to all children
	for (std::unique_ptr<SFormRow>& pFormRow : m_rows)
	{
		if (pFormRow->IsExpanded())
		{
			pFormRow->m_pChildContainer->OnActiveRowChanged(pNewActiveRow);
		}
	}
}

void CFormWidget::UpdateActiveRow(const QPoint& cursorPos)
{
	if (cursorPos == m_lastCursorPos)
	{
		return;
	}

	m_lastCursorPos = cursorPos;

	if (m_pActiveRow && !rect().contains(cursorPos))
	{
		SetActiveRow(nullptr);
		m_pParentTree->SetActiveRow(nullptr);
		return;
	}

	//But first test the existing row and discard early
	if (m_pActiveRow && m_pActiveRow->rowRect.contains(cursorPos))
	{
		return;
	}

	SFormRow* pFormRow = RowAtPosition(cursorPos);
	SetActiveRow(pFormRow);
	if (pFormRow)
	{
		m_pParentTree->SetActiveRow(pFormRow->m_pModel);
	}
	else
	{
		m_pParentTree->SetActiveRow(nullptr);
	}
}

void CFormWidget::SetActiveRow(SFormRow* pRow)
{
	if (pRow != m_pActiveRow)
	{
		m_pActiveRow = pRow;
		update();
	}
}

//////////////////////////////////////////////////////////////////////////

CInlineWidgetBox::CInlineWidgetBox(CFormWidget* pParent)
	: QWidget(pParent)
{
	m_pHBoxLayout = new QHBoxLayout();
	m_pHBoxLayout->setMargin(0);
	m_pHBoxLayout->setSpacing(pParent->GetSpacing());
	setLayout(m_pHBoxLayout);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void CInlineWidgetBox::AddWidget(QWidget* pWidget, QPropertyTree* pPropertyTree)
{
	pWidget->setParent(this);
	pWidget->setVisible(true);
	m_pHBoxLayout->addWidget(pWidget);

	IPropertyTreeWidget* pPropertyTreeWidget = qobject_cast<IPropertyTreeWidget*>(pWidget);
	if (pPropertyTreeWidget)
	{
		pPropertyTreeWidget->signalDiscarded.Connect(pPropertyTree, &QPropertyTree::OnRowDiscarded);
		pPropertyTreeWidget->signalPreChanged.Connect(pPropertyTree, &QPropertyTree::OnRowPreChanged);
		pPropertyTreeWidget->signalChanged.Connect(pPropertyTree, &QPropertyTree::OnRowChanged);
		pPropertyTreeWidget->signalContinuousChanged.Connect(pPropertyTree, &QPropertyTree::OnRowContinuousChanged);
	}
}

void CInlineWidgetBox::ReleaseWidgets(QPropertyTree* pPropertyTree)
{
	while (m_pHBoxLayout->count() > 0)
	{
		QLayoutItem* pItem = m_pHBoxLayout->takeAt(layout()->count() - 1);
		QWidget* pItemWidget = pItem->widget();
		if (pItemWidget)
		{
			pItemWidget->setParent(nullptr);

			IPropertyTreeWidget* pPropertyTreeWidget = qobject_cast<IPropertyTreeWidget*>(pItemWidget);
			if (pPropertyTreeWidget)
			{
				pPropertyTreeWidget->signalDiscarded.DisconnectObject(pPropertyTree);
				pPropertyTreeWidget->signalChanged.DisconnectObject(pPropertyTree);
				pPropertyTreeWidget->signalContinuousChanged.DisconnectObject(pPropertyTree);
				pPropertyTreeWidget->signalPreChanged.DisconnectObject(pPropertyTree);
			}
		}
	}
}

}
