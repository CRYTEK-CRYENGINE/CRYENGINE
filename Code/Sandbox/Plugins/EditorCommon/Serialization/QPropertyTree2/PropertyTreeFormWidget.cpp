// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTreeFormWidget.h"

#include <QEvent.h>
#include <QStyle>
#include <QToolTip>
#include <QClipboard>

#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

#include "QtUtil.h"
#include "DragDrop.h"

namespace PropertyTree2
{

	CFormWidget::FormRow::FormRow(const CRowModel* rowModel)
		: model(rowModel)
		, childContainer(nullptr)
		, widget(nullptr)
	{
	}

	CFormWidget::FormRow::FormRow(FormRow&& other)
	{
		*this = std::move(other);
	}

	CFormWidget::FormRow& CFormWidget::FormRow::operator=(FormRow&& other)
	{
		model = other.model;
		childContainer = other.childContainer;
		widget = other.widget;
		other.childContainer = nullptr;
		other.widget = nullptr;
		return *this;
	}

	CFormWidget::FormRow::~FormRow()
	{
		if (childContainer)
		{
			childContainer->setParent(nullptr);
			childContainer->deleteLater();
		}
	}

	bool CFormWidget::FormRow::IsExpanded() const
	{
		//We rely on "local" visible flag for expansion status. isVisible() is not enough as it needs all parents to also be visible
		return childContainer && childContainer->isVisibleTo(qobject_cast<QWidget*>(childContainer->parent()));
	}

	QWidget* CFormWidget::FormRow::GetQWidget()
	{
		return widget;
	}

	IPropertyTreeWidget* CFormWidget::FormRow::GetPropertyTreeWidget()
	{
		return qobject_cast<IPropertyTreeWidget*>(widget);
	}

	bool CFormWidget::FormRow::HasChildren() const
	{
		//If all the children are hidden then the form row doesn't have children
		return model->HasVisibleChildren();
	}

	//////////////////////////////////////////////////////////////////////////

	CFormWidget::CFormWidget(QPropertyTree2* parentTree, const CRowModel* row, int nesting)
		: m_rowModel(row)
		, m_parentTree(parentTree)
		, m_nesting(nesting)
		, m_activeRow(nullptr)
		, m_dragInsertIndex(-2)
	{
		//default values for style properties
		m_spacing = 4;
		m_minRowHeight = 20;
		m_iconSize = 16;
		m_nestingIndent = 10;
		m_groupBorderWidth = 0;
		m_groupBorderColor = Qt::transparent;

		setContentsMargins(0, 0, 0, 0);

		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setMinimumSize(QSize(40, 500));
		setMouseTracking(true);
		

		SetupWidgets();
		UpdateMinimumSize();

		const auto parentPtWidget = m_rowModel ? m_rowModel->GetPropertyTreeWidget() : nullptr;
		m_allowReorderChildren = parentPtWidget ? parentPtWidget->CanMoveChildren() : false;

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
		for (auto& formRow : m_rows)
		{
			if (formRow.IsExpanded())
				formRow.childContainer->OnSplitterPosChanged();
		}

		update();
	}

	void CFormWidget::SetupWidgets()
	{
		if (!m_rowModel)
		{
			m_rows.clear();
			return;
		}

		if(m_activeRow)
		{
			//Clear active row so it can be recomputed once the widgets have changed
			SetActiveRow(nullptr);
			m_lastCursorPos = QPoint(INT_MAX, INT_MAX);
		}

		//Cleanup old rows
		auto it = std::remove_if(m_rows.begin(), m_rows.end(), [](const FormRow& row) 
		{ 
			return row.model->IsRoot() || row.model->IsHidden(); 
		});
		m_rows.erase(it, m_rows.end());

		const int count = m_rowModel->GetChildren().size();
		m_rows.reserve(count);

		//Sets up the widgets, parenting relationship, signal/slot mechanism, etc...
		for (int i = 0; i < count; i++)
		{
			const auto& child = m_rowModel->GetChildren()[i];

			//Skip hidden rows
			if (child->IsHidden())
				continue;

			FormRow* pFormRow = nullptr;
			if(m_rows.size() <= i)
			{
				m_rows.emplace_back(child);
				pFormRow = &m_rows.back();
			}
			else if (m_rows[i].model != child)
			{
				m_rows.emplace(m_rows.begin() + i, child);
				pFormRow = &m_rows[i];
			}
			else
			{
				pFormRow = &m_rows[i];
			}

			if (child->IsWidgetSet())
			{
				QWidget* w = child->GetQWidget();
				pFormRow->widget = w;

				if (w->parent() != this)
				{
					w->setParent(this);
					w->setVisible(true);

					IPropertyTreeWidget* ptw = child->GetPropertyTreeWidget();
					if (ptw) //The property tree supports non property tree widgets however they will not be interacting with serialization
					{
						ptw->signalChanged.Connect(m_parentTree, &QPropertyTree2::OnRowChanged);
						ptw->signalContinuousChanged.Connect(m_parentTree, &QPropertyTree2::OnRowContinuousChanged);
					}
				}
			}
			else if(child->HasChildren())
			{
				//No widget and children, let's check if there is a need for inlining child widgets
				CRowModel* inlinedChild = child->GetChildren()[0];
				if (inlinedChild->GetFlags() & CRowModel::Inline)
				{
					CInlineWidgetBox* box = new CInlineWidgetBox(this);
					pFormRow->widget = box;

					int index = 0;
					const int count = child->GetChildren().size();

					while (index < count)
					{
						inlinedChild = child->GetChildren()[index];
						if (inlinedChild->GetQWidget() && inlinedChild->GetFlags() & CRowModel::Inline)
						{
							box->AddWidget(inlinedChild->GetQWidget(), m_parentTree);
							index++;
						}
						else
							break;
					}
				}
			}

			//Auto expand only the first time (child container has not yet been created)
			if (child->HasVisibleChildren() && pFormRow->childContainer == nullptr)
			{
				if (child->GetFlags() & CRowModel::ExpandByDefault || (m_nesting < m_parentTree->GetAutoExpandDepth() && !(child->GetFlags() & CRowModel::CollapseByDefault)))
				{
					ToggleExpand(*pFormRow, true);
				}
			}

			child->MarkClean();
		}
	}

	void CFormWidget::ReleaseWidgets()
	{
		//The widgets in the property tree do not follow by the regular Qt model of hierarchy.
		//As they are created by the row model we only transfer temporary ownership to the formWidget or parents for display, but return them afterwards.
		//CRowModel is responsible for their lifetime
		for (auto& row : m_rows)
		{
			QWidget* widget = row.widget;

			if (widget)
			{
				widget->setParent(nullptr);

				IPropertyTreeWidget* ptw = row.GetPropertyTreeWidget();
				if (ptw)
				{
					ptw->signalChanged.DisconnectObject(m_parentTree);
					ptw->signalContinuousChanged.DisconnectObject(m_parentTree);
				}
				
				CInlineWidgetBox* box = qobject_cast<CInlineWidgetBox*>(widget);
				if (box)
				{
					box->ReleaseWidgets(m_parentTree);
					box->deleteLater();
				}
			}

			row.widget = nullptr;
		}
	}

	void CFormWidget::SetSpacing(int value)
	{
		if(value != m_spacing)
		{
			m_spacing = value;
			DoFullLayoutUpdate();
		}
	}

	void CFormWidget::SetMinRowHeight(int value)
	{
		if(value != m_minRowHeight)
		{
			m_minRowHeight = value;
			DoFullLayoutUpdate();
		}
	}

	void CFormWidget::SetIconSize(int value)
	{
		if(value != m_iconSize)
		{
			m_iconSize = value;
			update(); //only requires repaint, not full layout
		}
	}

	void CFormWidget::SetGroupBorderWidth(int value)
	{
		if(value != m_groupBorderWidth)
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
		const auto  margins = contentsMargins();
		return QRect(margins.left(), margins.top(), margins.right(), margins.bottom());
	}

	CFormWidget* CFormWidget::ExpandRow(const CRowModel* row, bool expand)
	{
		CRY_ASSERT(row && row->GetParent() == m_rowModel && row->HasVisibleChildren());

		FormRow* formRow = ModelToFormRow(row);
		CRY_ASSERT(formRow);

		if (formRow->IsExpanded() != expand)
			ToggleExpand(*formRow);

		return formRow->childContainer;
	}

	CFormWidget* CFormWidget::ExpandToRow(const CRowModel* row)
	{
		CRY_ASSERT(row && !row->IsRoot());

		if (row->GetParent() == m_rowModel)
		{
			return this;
		}
		else
		{
			//Expand parent row (recursive)
			auto formWidget = ExpandToRow(row->GetParent());
			auto childFormWidget = formWidget->ExpandRow(row->GetParent(), true);
			return childFormWidget;
		}
	}

	void CFormWidget::ScrollToRow(const CRowModel* row)
	{
		const FormRow* formRow = ModelToFormRow(row);
		if (!formRow)
			return;

		QScrollArea* scrollArea = m_parentTree->GetScrollArea();

		{
			auto point = mapTo(scrollArea->widget(), formRow->rowRect.bottomRight());
			scrollArea->ensureVisible(point.x(), point.y(), 0, 0);
		}
		{
			auto point = mapTo(scrollArea->widget(), formRow->rowRect.topLeft());
			scrollArea->ensureVisible(point.x(), point.y(), 0, 0);
		}
	}

	void CFormWidget::UpdateTree()
	{
		if (m_rowModel->IsClean())
			return;

		bool doSetup = false;

		if (m_rowModel->HasDirtyChildren())
		{
			for (auto& row : m_rows)
			{
				if (row.childContainer)
				{
					if (row.model->IsClean())
						continue;

					row.childContainer->UpdateTree();

					//Clean up rows that no longer have children
					if (!row.model->HasChildren())
					{
						row.childContainer->deleteLater();
						row.childContainer = nullptr;

						doSetup = true;
					}
				}
				else if (row.model->HasFirstTimeChildren()) //Expand rows that have children for the first time 
				{
					ToggleExpand(row);
				}
			}
		}

		if (doSetup || m_rowModel->IsDirty())
		{
			SetupWidgets();
			UpdateMinimumSize();
		}

		m_rowModel->MarkClean();
		update();
	}

	void CFormWidget::SetNestingIndent(int value)
	{
		m_nestingIndent = value;
		update(); //only requires repaint, not full layout
	}

	void CFormWidget::paintEvent(QPaintEvent* event)
	{
		QStyle* style = QWidget::style();
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);

		QStyleOption opt;
		opt.initFrom(this);

		const QPoint cursorPos = mapFromGlobal(QCursor::pos());

		//Update active row for cases that wouldn't trigger mouse events, such as moving between widgets on the right side.
		if (!m_parentTree->GetActiveRow() && rect().contains(cursorPos))
		{
			QObject* w = QApplication::widgetAt(QCursor::pos());
			CFormWidget* form = nullptr;
			while (w && !form)
			{
				w = w->parent();
				form = qobject_cast<CFormWidget*>(w);
			}
			if(form == this)
				UpdateActiveRow(cursorPos);
		}

		//Paint background due to group widgets
		painter.fillRect(rect(), palette().background());

		//Paint the drag highlight
		if (m_dragInsertIndex >= 0)
		{
			//TODO: problem drawing before the first row or after the last row as it is not in this widget's rect
			const auto index = m_dragInsertIndex;
			CRY_ASSERT(index >= 0 && index < m_rows.size());
			QRect highightRect = m_rows[index].rowRect;
			highightRect.setTop(highightRect.top() - m_spacing);
			highightRect.setBottom(highightRect.top() + m_spacing - 1);

			//In QSS this color is set by the selection-background-color property.
			painter.fillRect(highightRect, palette().highlight());
		}

		const bool isDragging = m_dragInsertIndex != -2 || m_parentTree->IsDraggingSplitter();
		const int flags = Qt::AlignVCenter | Qt::TextForceLeftToRight | Qt::TextSingleLine;
		const int leftMargin = contentsMargins().left();
		const int textIndent = leftMargin + m_groupBorderWidth + m_nesting * m_nestingIndent + m_iconSize + m_spacing;

		const int count = m_rows.size();
		for (int i = 0; i < count; i++)
		{
			const auto& row = m_rows[i];
			const auto& rowModel = row.model;

			//Draw group frame
			if (row.HasChildren() && m_groupBorderWidth > 0)
			{
				QRect frame = row.rowRect;
				if (row.IsExpanded())
					frame.setBottom(frame.bottom() + m_groupBorderWidth + row.childContainer->rect().height());

				QPainterPath path;
				path.addRoundedRect(frame, 3, 3);

				painter.fillPath(path, m_activeRow != &row ? m_groupBorderColor : palette().alternateBase());
			}
			else if (m_activeRow == &row && !isDragging) //draw highlighted row if not in any drag mode
			{
				//In QSS this color is set by the alternate-background-color property.
				painter.fillRect(row.rowRect, palette().alternateBase());
			}

			//Draw labels
			{
				QRect textRect = row.rowRect;
				textRect.setLeft(textRect.left() + textIndent);
				textRect.setRight(GetSplitterPosition());

				style->drawItemText(&painter, textRect, flags, opt.palette, isEnabled(), rowModel->GetLabel(), foregroundRole());
			}

			//Draw expand icon
			if (rowModel->HasVisibleChildren())
			{
				const QRect iconRect = GetExpandIconRect(row);
				const bool bActive = iconRect.contains(cursorPos);
				
				if (row.IsExpanded())
					m_parentTree->GetExpandedIcon().paint(&painter, iconRect, Qt::AlignCenter, bActive ? QIcon::Active : QIcon::Normal);
				else
					m_parentTree->GetCollapsedIcon().paint(&painter, iconRect, Qt::AlignCenter, bActive ? QIcon::Active : QIcon::Normal);
			}

			//Draw drag icon
			if (m_allowReorderChildren)
			{
				const QRect iconRect = GetDragHandleIconRect(row);
				m_parentTree->GetDragHandleIcon().paint(&painter, iconRect, Qt::AlignCenter, QIcon::Normal);
			}
		}

		QWidget::paintEvent(event);
	}

	QRect CFormWidget::GetExpandIconRect(const FormRow& row) const
	{
		QRect iconRect = row.rowRect;
		if (m_groupBorderWidth > 0 && row.HasChildren())
			iconRect.setLeft(iconRect.left() + m_groupBorderWidth);
		iconRect.setLeft(iconRect.left() + contentsMargins().left() + m_nesting * m_nestingIndent);
		iconRect.setWidth(m_iconSize);
		return iconRect;
	}

	QRect CFormWidget::GetDragHandleIconRect(const FormRow& row) const
	{
		QRect iconRect = row.rowRect;
		if (m_groupBorderWidth > 0 && row.HasChildren())
			iconRect.setRight(iconRect.right() - m_groupBorderWidth);
		iconRect.setLeft(iconRect.right() - contentsMargins().right() - m_iconSize);
		iconRect.setWidth(m_iconSize);
		return iconRect;
	}

	void CFormWidget::ToggleExpand(FormRow& row, bool skipLayout /* = false*/)
	{
		if (row.IsExpanded())
		{
			row.childContainer->setVisible(false);
		}
		else
		{
			if (row.childContainer)
			{
				row.childContainer->setVisible(true);
			}
			else
			{
				//Create child container
				row.childContainer = new CFormWidget(m_parentTree, row.model, m_nesting + 1);
				row.childContainer->setParent(this);
				row.childContainer->setVisible(true);
			}
		}

		if(!skipLayout)
			DoFullLayoutUpdate();
	}

	void CFormWidget::resizeEvent(QResizeEvent* event)
	{
		QWidget::resizeEvent(event);
		DoLayout(event->size());
	}

	void CFormWidget::enterEvent(QEvent* ev)
	{
		const QPoint cursorPos = mapFromGlobal(QCursor::pos());
		UpdateActiveRow(cursorPos);
	}

	void CFormWidget::leaveEvent(QEvent* ev)
	{
		if(m_activeRow)
		{
			const QPoint cursorPos = mapFromGlobal(QCursor::pos());
			UpdateActiveRow(cursorPos);
		}
	}

	void CFormWidget::contextMenuEvent(QContextMenuEvent* event)
	{
		if (!m_activeRow)
			return;

		QMenu* menu = new QMenu();

		//Active row
		if (m_activeRow->model->GetPropertyTreeWidget())
			m_activeRow->model->GetPropertyTreeWidget()->PopulateContextMenu(menu, m_activeRow->model);

		menu->addSeparator();
		
	    //Parent row may add some entries
		if (m_rowModel->GetPropertyTreeWidget())
			m_rowModel->GetPropertyTreeWidget()->PopulateChildContextMenu(menu, m_activeRow->model);

		menu->addSeparator();

		//Generic entries
		auto action = menu->addAction(tr("Copy"));
		connect(action, &QAction::triggered, this, &CFormWidget::OnCopy);

		action = menu->addAction(tr("Paste"));
		connect(action, &QAction::triggered, this, &CFormWidget::OnPaste);

		menu->popup(event->globalPos());
	}

	void CFormWidget::mousePressEvent(QMouseEvent* ev)
	{
		UpdateActiveRow(ev->pos());

		if(ev->buttons() & Qt::LeftButton)
			m_mouseDownPos = m_lastCursorPos;

		if (m_activeRow)
		{
			if(m_activeRow->model->HasVisibleChildren())
			{
				QRect iconRect = GetExpandIconRect(*m_activeRow);
				if (iconRect.marginsAdded(QMargins(2, 0, 2, 0)).contains(ev->pos()))
				{
					ToggleExpand(*m_activeRow);
				}
			}
		}

		const int splitterPos = GetSplitterPosition();
		if (abs(ev->x() - splitterPos) <= 5)
		{
			m_parentTree->SetDraggingSplitter(true);
			grabMouse();
			update();
		}
	}


	void CFormWidget::mouseMoveEvent(QMouseEvent* ev)
	{
		UpdateActiveRow(ev->pos());

		const bool isInsideWidget = rect().contains(ev->pos());

		if (isInsideWidget && m_activeRow && m_activeRow->model->HasVisibleChildren())
		{
			//Repaint when nearing the icon to ensure the active state is updated correctly
			QRect iconRect = GetExpandIconRect(*m_activeRow);
			if (iconRect.marginsAdded(QMargins(2, 0, 2, 0)).contains(ev->pos()))
				update(); //Trigger repaint
		}

		if (m_parentTree->IsDraggingSplitter())
		{
			m_parentTree->SetSplitterPosition(mapTo(m_parentTree, ev->pos()).x());
		}
		else
		{
			const int splitterPos = GetSplitterPosition();
			const int sliderHalfAreaWidth = 5;
			if (abs(ev->x() - splitterPos) <= sliderHalfAreaWidth)
			{
				setCursor(QCursor(Qt::SplitHCursor));
			}
			else if (m_activeRow && GetDragHandleIconRect(*m_activeRow).contains(ev->pos()))
			{
				setCursor(QCursor(Qt::OpenHandCursor));
			}
			else
			{
				setCursor(QCursor(Qt::ArrowCursor));
			}

			if (ev->buttons() & Qt::LeftButton && isEnabled())
			{
				if (m_allowReorderChildren && (m_mouseDownPos - ev->pos()).manhattanLength() > QApplication::startDragDistance())
				{
					auto draggedRow = RowAtPos(m_mouseDownPos);
					if (draggedRow)
					{
						auto dragData = new CDragDropData();
						dragData->SetCustomData("PropertyRow", draggedRow->model);

						QPixmap* pixmap = new QPixmap(draggedRow->rowRect.size());
						render(pixmap, QPoint(), QRegion(draggedRow->rowRect));

						CDragDropData::StartDrag(this, Qt::MoveAction, dragData, pixmap, draggedRow->rowRect.topLeft() - ev->pos());
					}
				}
			}
		}
	}
	
	void CFormWidget::mouseReleaseEvent(QMouseEvent* ev)
	{
		if (m_parentTree->IsDraggingSplitter())
		{
			releaseMouse();
			m_parentTree->SetDraggingSplitter(false);
			setCursor(QCursor(Qt::ArrowCursor));
			update();
		}
	}

	void CFormWidget::dragEnterEvent(QDragEnterEvent* ev)
	{
		const CDragDropData* data = CDragDropData::FromDragDropEvent(ev);
		if (data->HasCustomData("PropertyRow"))
		{
			const CRowModel* draggedRow = data->GetCustomData<CRowModel*>("PropertyRow");
			if (draggedRow->GetParent() == m_rowModel && m_allowReorderChildren)
			{
				ev->acceptProposedAction();
			}
		}

		m_dragInsertIndex = -1;
		SetActiveRow(nullptr);
	}

	void CFormWidget::dragMoveEvent(QDragMoveEvent* ev)
	{
		UpdateActiveRow(ev->pos());

		QWidget::dragMoveEvent(ev);

		const auto oldDragIndex = m_dragInsertIndex;
		m_dragInsertIndex = InsertionIndexAtPos(ev->pos());

		if(m_dragInsertIndex != oldDragIndex)
			update();
		
		if (m_dragInsertIndex >= 0)
		{
			const CDragDropData* data = CDragDropData::FromDragDropEvent(ev);
			if (data->HasCustomData("PropertyRow"))
			{
				const CRowModel* draggedRow = data->GetCustomData<CRowModel*>("PropertyRow");
				
				//Note: Because the only case for reordering is arrays which should not have hidden rows, we take the model index at face value
				const auto index = draggedRow->GetIndex();

				//A row cannot be dropped on itself or below itself
				//TODO: Currently it is difficult to handle drop are above the first element and below the last element. This is due to the widget hierarchy, not being able to paint above or below.
				//A solution could be to add the spacing inside the child form widget so it can handle it, or otherwise have a global overlay for highlighting. For now these cases are not handled and guarded against here.
				if (index == m_dragInsertIndex || index == m_dragInsertIndex - 1 || m_dragInsertIndex == 0 || m_dragInsertIndex == m_rows.size())
				{
					m_dragInsertIndex = -1;
				}
				else
				{
					ev->acceptProposedAction();
					return;
				}
			}
		}

		ev->ignore();
	}

	void CFormWidget::dragLeaveEvent(QDragLeaveEvent* ev)
	{
		m_dragInsertIndex = -2;
		if (m_activeRow)
		{
			const QPoint cursorPos = mapFromGlobal(QCursor::pos());
			UpdateActiveRow(cursorPos);
		}
	}

	void CFormWidget::dropEvent(QDropEvent* ev)
	{
		m_dragInsertIndex = -2;
		const QPoint cursorPos = mapFromGlobal(QCursor::pos());
		UpdateActiveRow(cursorPos);

		const CDragDropData* data = CDragDropData::FromDragDropEvent(ev);
		if (data->HasCustomData("PropertyRow"))
		{
			const CRowModel* draggedRow = data->GetCustomData<CRowModel*>("PropertyRow");
			if (draggedRow->GetParent() == m_rowModel && m_allowReorderChildren)
			{
				const int insertIndex = InsertionIndexAtPos(ev->pos());

				const auto w = m_rowModel->GetPropertyTreeWidget();
				CRY_ASSERT(w && w->CanMoveChildren());

				//Note: Because the only case for reordering is arrays which should not have hidden rows, we take the model index at face value
				const auto index = draggedRow->GetIndex();
				w->MoveChild(index, index < insertIndex ? insertIndex - 1 : insertIndex);
				ev->acceptProposedAction();
			}
		}
	}

	void CFormWidget::keyPressEvent(QKeyEvent* ev)
	{
		if (!m_activeRow)
		{
			//If the form widget with active row is not in focus, the root form will dispatch the message properly
			if (m_rowModel->IsRoot())
			{
				auto row = m_parentTree->GetActiveRow();
				if(row && row->GetParent() != m_rowModel)
				{
					auto form = ExpandToRow(row);
					CRY_ASSERT(form != this);
					form->setFocus();

					//Don't use qapp::sendEvent because it will lead back here and recurse until stack overflow
					form->event(ev); 
				}
			}

			ev->ignore();
			return;
		}

		switch (ev->key())
		{
		case Qt::Key_Up:
		{
			const auto index = RowIndex(m_activeRow);
			
			if (index > 0)
			{
				FormRow* row = &m_rows[index - 1];
				CFormWidget* form = this;
				while (row->IsExpanded())
				{
					form = row->childContainer;
					row = &row->childContainer->m_rows.back();
				}

				m_parentTree->EnsureRowVisible(row->model);
				m_parentTree->SetActiveRow(row->model);
				form->setFocus();
			}
			else if(!m_rowModel->IsRoot())
			{
				m_parentTree->EnsureRowVisible(m_rowModel);
				m_parentTree->SetActiveRow(m_rowModel);
				auto parentForm = qobject_cast<CFormWidget*>(parent());
				if(parentForm)
					parentForm->setFocus();
			}
			
			break;
		}
		case Qt::Key_Down:
		{
			//Get first child
			if (m_activeRow->model->HasVisibleChildren() && m_activeRow->IsExpanded())
			{
				CFormWidget* form = m_activeRow->childContainer;
				CRY_ASSERT(form->m_rows.size() > 0);
				form->setFocus();
				m_parentTree->EnsureRowVisible(form->m_rows[0].model);
				m_parentTree->SetActiveRow(form->m_rows[0].model);
			}
			else
			{
				FormRow* row = m_activeRow;
				CFormWidget* form = this;

				while (form && row)
				{
					//Get next sibling
					{
						const auto index = form->RowIndex(row);
						const auto count = form->m_rows.size();
						if (count - 1 > index)
						{
							row = &form->m_rows[index + 1];
							break;
						}
					}

					//Or go to parent for next loop
					{
						CFormWidget* parentForm = qobject_cast<CFormWidget*>(form->parent());
						if (parentForm)
						{
							const auto index = parentForm->RowIndex(parentForm->ModelToFormRow(form->m_rowModel));
							row = &parentForm->m_rows[index];
						}
						form = parentForm;
					}
				}

				if (form && row)
				{
					m_parentTree->EnsureRowVisible(row->model);
					m_parentTree->SetActiveRow(row->model);
					form->setFocus();
				}
			}
			break;
		}
		case Qt::Key_Left:
		{
			if (m_activeRow->IsExpanded())
				ToggleExpand(*m_activeRow);
			else if(!m_rowModel->IsRoot()) //select parent row
			{
				CFormWidget* form = qobject_cast<CFormWidget*>(parent());
				if (form)
				{
					m_parentTree->EnsureRowVisible(m_rowModel);
					m_parentTree->SetActiveRow(m_rowModel);
					form->setFocus();
				}
			}
			break;
		}
		case Qt::Key_Right:
		{
			if (m_activeRow->model->HasVisibleChildren() && !m_activeRow->IsExpanded())
				ToggleExpand(*m_activeRow);
			break;
		}
		default:
			QWidget::keyPressEvent(ev);
		}
	}

	bool CFormWidget::event(QEvent* ev)
	{
		switch (ev->type())
		{
		/* All the following events are reimplemented from QWidget::event to allow usage when the widget is disabled 
		   Note that keyPressEvent cannot be handled as disabled widgets cannot get focus. 
		   Then again, disabled property tree also disables the scroll area so it is very impractical ...*/
		case QEvent::MouseMove:
			mouseMoveEvent((QMouseEvent*)ev);
			break;
		case QEvent::MouseButtonPress:
			mousePressEvent((QMouseEvent*)ev);
			break;
		case QEvent::MouseButtonRelease:
			mouseReleaseEvent((QMouseEvent*)ev);
			break;
		/* End event overrides for disabled state */
		case QEvent::LayoutRequest:
		{
			DoFullLayoutUpdate();
			break;
		}
		case QEvent::ToolTip:
		{
			QHelpEvent *helpEvent = static_cast<QHelpEvent *>(ev);
			if (m_activeRow)
			{
				QString tooltip = m_activeRow->model->GetTooltip();
				if(tooltip.isEmpty())
				{
					QToolTip::hideText();
					ev->ignore();
				}
				else
				{
					QToolTip::hideText();
					QToolTip::showText(helpEvent->globalPos(), tooltip);
				}
			}

			break;
		}
		case SandboxEvent::Command:
		{
			CommandEvent* commandEvent = static_cast<CommandEvent*>(ev);

			const string& command = commandEvent->GetCommand();

			if (command == "general.copy" || command == "general.cut")
			{
				OnCopy();
				ev->accept();
			}
			else if (command == "general.paste")
			{
				OnPaste();
				ev->accept();
			}
			else
			{
				ev->ignore();
				return false;
			}
			break;
		}
		default:
			return QWidget::event(ev);
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
		QWidget* candidate = focusWidget();
		if (candidate->hasFocus() && !qobject_cast<CFormWidget*>(candidate))
		{
			return QWidget::focusNextPrevChild(next);
		}
		else if (m_parentTree->GetActiveRow() != nullptr)
		{
			//TODO: with inlining this may not be the right widget, tab navigation needs to be adressed
			if (m_parentTree->GetActiveRow()->IsWidgetSet())
			{
				m_parentTree->GetActiveRow()->GetQWidget()->setFocus();
				return true;
			}
			else
				return false;
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
			height += (childCount - 1) * m_spacing;

		for (int i = 0; i < childCount; i++)
		{
			const auto& row = m_rows[i];
			const int groupPadding = row.HasChildren() ? m_groupBorderWidth * 2 : 0;

			height += groupPadding;

			QWidget* widget = row.widget;
			if (widget)
			{
				const QSize sizeHint = widget->minimumSizeHint();
				height += max(sizeHint.height(), m_minRowHeight);
				widgetsMinWidth = max(widgetsMinWidth, sizeHint.width() + groupPadding);
			}
			else
			{
				height += m_minRowHeight;
			}

			if (row.childContainer && row.IsExpanded())
			{
				const QSize minSize = row.childContainer->minimumSizeHint();
				height += minSize.height() + (m_groupBorderWidth == 0 ? m_spacing : m_groupBorderWidth);
				nestedFormsMinWidth = max(nestedFormsMinWidth, minSize.width() + groupPadding);
			}
		}

		//reserve some space for the labels //TODO: this must be enforced when sizing down so widgets are not actually laid out past their min width
		const int labelsMinWidth = 20;
		int width = contentsMargins().left() + contentsMargins().right() + labelsMinWidth + widgetsMinWidth;

		if(m_allowReorderChildren)
			width += m_iconSize + m_spacing;

		width = max(width, nestedFormsMinWidth);

		setMinimumSize(width, height);
	}

	void CFormWidget::DoLayout(const QSize& size)
	{
		if (!isVisible())
			return;

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
			auto& row = m_rows[i];
			const bool isGroup = row.HasChildren();

			int groupPadding = 0;
			if (isGroup)
				groupPadding = m_groupBorderWidth;

			QWidget* widget = row.widget;
			if (widget != nullptr)
			{
				//TODO: HeightForWidth widgets are not handled
				const QSize sizeHint = widget->minimumSizeHint();
				const bool expandsHorizontal = widget->sizePolicy().expandingDirections() & Qt::Horizontal;
				const int widgetWidth = expandsHorizontal ? wRight - wLeft - groupPadding : min(sizeHint.width(), wRight - wLeft - groupPadding);
				const int rowHeight = max(m_minRowHeight, sizeHint.height()) + 2*groupPadding;

				row.rowRect = QRect(left, height, right - left, rowHeight);
				widget->setGeometry(wLeft, height + (rowHeight - sizeHint.height()) / 2, widgetWidth, sizeHint.height());
			}
			else
			{
				const int rowHeight = m_minRowHeight + 2*groupPadding;
				row.rowRect = QRect(left, height, right - left, rowHeight);
			}

			height += row.rowRect.height();

			if (row.childContainer && row.IsExpanded())
			{
				height += m_groupBorderWidth == 0 ? m_spacing : 0;

				const int rowHeight = row.childContainer->minimumSizeHint().height();
				row.childContainer->setGeometry(left + groupPadding, height, right - groupPadding * 2 - left, rowHeight);

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
		SerializableWrapper(const CRowModel* model) : m_model(model) {}

		void Serialize(yasli::Archive& ar)
		{
			if (m_model->GetPropertyTreeWidget())
			{
				m_model->GetPropertyTreeWidget()->Serialize(ar);
			}

			for (auto& child : m_model->GetChildren())
			{
				ar(SerializableWrapper(child), child->GetName(), "");
			}
		}

		const CRowModel* m_model;
	};

	void CFormWidget::OnCopy()
	{
		if (!m_activeRow)
			return;

		yasli::JSONOArchive ar;
		SerializableWrapper wrapper(m_activeRow->model);
		ar(wrapper);

		QClipboard* pClipboard = QApplication::clipboard();
		CRY_ASSERT(pClipboard);

		QMimeData* mimeData = new QMimeData();
		mimeData->setText(ar.c_str());
		pClipboard->setMimeData(mimeData);
	}

	void CFormWidget::OnPaste()
	{
		if (!m_activeRow)
			return;

		QClipboard* pClipboard = QApplication::clipboard();
		CRY_ASSERT(pClipboard);

		if (pClipboard->mimeData()->hasText())
		{
			string text = pClipboard->mimeData()->text().toStdString().c_str();

			yasli::JSONIArchive ar;
			ar.open(text.data(), text.length());

			m_parentTree->SetAccumulateChanges(true);

			SerializableWrapper wrapper(m_activeRow->model);
			ar(wrapper);

			//Force only one undo notification even though this may trigger many serialization passes
			m_parentTree->SetAccumulateChanges(false);
		}
	}

	int CFormWidget::GetSplitterPosition() const
	{
		const auto splitterPos = m_parentTree->GetSplitterPosition();
		return mapFrom(m_parentTree, QPoint(splitterPos, 0)).x();
	}

	PropertyTree2::CFormWidget::FormRow* CFormWidget::ModelToFormRow(const CRowModel* row)
	{
		CRY_ASSERT(row && row->GetParent() == m_rowModel && !row->IsHidden());

		const auto index = row->GetIndex();

		//Because of hidden rows, the index of the form row is either at the index or before. Start at index and work our way back to 0.
		auto beginIt = index > m_rows.size() ? m_rows.rbegin() : m_rows.rbegin() + (m_rows.size() - index - 1);
		auto it = std::find_if(beginIt, m_rows.rend(), [&](const FormRow& formRow) { return formRow.model == row; });
		if (it != m_rows.rend())
			return &(*it);
		else
			return nullptr;
	}

	PropertyTree2::CFormWidget::FormRow* CFormWidget::RowAtPos(const QPoint& position)
	{
		//Binary search through the rows, using only vertical position
		int low = 0;
		int high = m_rows.size() - 1;

		while (low <= high)
		{
			int mid = (low + high) / 2;
			auto& row = m_rows[mid];

			if (position.y() < row.rowRect.top())
				high = mid - 1;
			else if (position.y() > row.rowRect.bottom())
				low = mid + 1;
			else if (row.rowRect.contains(position))
				return &row;
			else
				return nullptr;
		}

		return nullptr;
	}

	int CFormWidget::InsertionIndexAtPos(const QPoint& position, int tolerancePx)
	{
		//Computes the insertion index when dragging something and dropping it between two rows
		//This assumes spacing + tolerance < average row size

		//TODO: if above row has a child widget then this algorithm makes it possible to drop between the top row and its children, this is strange, we really should only test the row below!

		int index = -1;

		auto row = RowAtPos(position);
		if (row)
		{
			if (!row->rowRect.contains(QPoint(position.x(), position.y() - tolerancePx)))
			{
				//pos is closer to the top of the row
				index = RowIndex(row);
			}
			else if (!row->rowRect.contains(QPoint(position.x(), position.y() + tolerancePx)))
			{
				//pos is closer to the bottom of the row
				index = RowIndex(row) + 1;
			}
		}
		else //Current position is in between rows
		{
			auto rowAbove = RowAtPos(QPoint(position.x(), position.y() - m_spacing));
			if (rowAbove)
			{
				index = RowIndex(rowAbove) + 1;
			}
			else
			{
				auto rowBelow = RowAtPos(QPoint(position.x(), position.y() + m_spacing));
				if (rowBelow)
				{
					index = RowIndex(rowBelow);
				}
				else
				{
					rowAbove = RowAtPos(QPoint(position.x(), position.y() - m_spacing - tolerancePx));
					if (rowAbove)
					{
						index = RowIndex(rowAbove) + 1;
					}
					else
					{
						rowBelow = RowAtPos(QPoint(position.x(), position.y() + m_spacing + tolerancePx));
						if (rowBelow)
						{
							index = RowIndex(rowBelow);
						}
					}
				}
			}
		}
		
		//Cannot insert after the last row
		if (index >= m_rows.size())
			index = -1;

		return index;
	}

	int CFormWidget::RowIndex(const FormRow* row)
	{
		return int(row - &(*m_rows.begin()));
	}

	void CFormWidget::OnActiveRowChanged(const CRowModel* newActiveRow)
	{
		CRY_ASSERT(!newActiveRow || !newActiveRow->IsRoot());

		if (m_activeRow && m_activeRow->model == newActiveRow)
			return;

		if (newActiveRow && newActiveRow->GetParent() == m_rowModel)
		{
			SetActiveRow(ModelToFormRow(newActiveRow));
		}
		else
		{
			SetActiveRow(nullptr);
		}

		//Recurse to all children
		for (auto& formRow : m_rows)
		{
			if (formRow.IsExpanded())
				formRow.childContainer->OnActiveRowChanged(newActiveRow);
		}
	}

	void CFormWidget::UpdateActiveRow(const QPoint& cursorPos)
	{
		if (cursorPos == m_lastCursorPos)
			return;

		m_lastCursorPos = cursorPos;

		if (m_activeRow && !rect().contains(cursorPos))
		{
			SetActiveRow(nullptr);
			m_parentTree->SetActiveRow(nullptr);
			return;
		}

		//But first test the existing row and discard early
		if (m_activeRow && m_activeRow->rowRect.contains(cursorPos))
			return;

		FormRow* formRowPtr = RowAtPos(cursorPos);
		SetActiveRow(formRowPtr);
		if(formRowPtr)
			m_parentTree->SetActiveRow(formRowPtr->model);
		else
			m_parentTree->SetActiveRow(nullptr);
	}

	void CFormWidget::SetActiveRow(FormRow* row)
	{
		if (row != m_activeRow)
		{
			m_activeRow = row;
			update();
		}
	}

	//////////////////////////////////////////////////////////////////////////

	CInlineWidgetBox::CInlineWidgetBox(CFormWidget* parent)
		: QWidget(parent)
	{
		QHBoxLayout* layout = new QHBoxLayout();
		layout->setMargin(0);
		layout->setSpacing(parent->GetSpacing());
		setLayout(layout);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	void CInlineWidgetBox::AddWidget(QWidget* widget, QPropertyTree2* pTree)
	{
		widget->setParent(this);
		qobject_cast<QHBoxLayout*>(layout())->addWidget(widget);
		widget->setVisible(true);

		IPropertyTreeWidget* ptw = qobject_cast<IPropertyTreeWidget*>(widget);
		if (ptw)
		{
			ptw->signalChanged.Connect(pTree, &QPropertyTree2::OnRowChanged);
			ptw->signalContinuousChanged.Connect(pTree, &QPropertyTree2::OnRowContinuousChanged);
		}
	}

	void CInlineWidgetBox::ReleaseWidgets(QPropertyTree2* pTree)
	{
		while (layout()->count() > 0)
		{
			auto item = layout()->takeAt(layout()->count() - 1);
			auto w = item->widget();
			if (w)
			{
				w->setParent(nullptr);

				IPropertyTreeWidget* ptw = qobject_cast<IPropertyTreeWidget*>(w);
				if (ptw)
				{
					ptw->signalChanged.DisconnectObject(pTree);
					ptw->signalContinuousChanged.DisconnectObject(pTree);
				}
			}
		}
	}

}


