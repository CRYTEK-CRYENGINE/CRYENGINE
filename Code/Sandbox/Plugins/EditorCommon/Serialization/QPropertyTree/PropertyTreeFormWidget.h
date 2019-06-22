// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QLayout>
#include <QRect>

#include "PropertyTreeModel.h"
#include "PropertyTree.h"

namespace PropertyTree2
{
//This widget container is in charge of laying out the widgets in the tree and drawing the extra elements
//This also handles interaction with the tree's UI elements (expanding/collapsing, slider...)
class CFormWidget : public QWidget
{
	Q_OBJECT

	//Styling properties
	Q_PROPERTY(int nesting READ GetNesting)
	Q_PROPERTY(int spacing READ GetSpacing WRITE SetSpacing DESIGNABLE true)
	Q_PROPERTY(int minRowHeight READ GetMinRowHeight WRITE SetMinRowHeight DESIGNABLE true)
	Q_PROPERTY(int iconSize READ GetIconSize WRITE SetIconSize DESIGNABLE true)
	Q_PROPERTY(int nestingIndent READ GetNestingIndent WRITE SetNestingIndent DESIGNABLE true)
	Q_PROPERTY(int groupBorderWidth READ GetGroupBorderWidth WRITE SetGroupBorderWidth DESIGNABLE true)
	Q_PROPERTY(QColor groupBorderColor READ GetGroupBorderColor WRITE SetGroupBorderColor DESIGNABLE true)
	Q_PROPERTY(QRect margins READ GetMargins WRITE SetMargins DESIGNABLE true)

public:
	CFormWidget(QPropertyTree* pParentTree, const CRowModel* pRow, int nesting);
	~CFormWidget();

	//Styling
	int              GetNesting() const       { return m_nesting; }

	int              GetNestingIndent() const { return m_nestingIndent; }
	void             SetNestingIndent(int value);

	int              GetSpacing() const { return m_spacing; }
	void             SetSpacing(int value);

	int              GetMinRowHeight() const { return m_minRowHeight; }
	void             SetMinRowHeight(int value);

	int              GetIconSize() const { return m_iconSize; }
	void             SetIconSize(int value);

	int              GetGroupBorderWidth() const { return m_groupBorderWidth; }
	void             SetGroupBorderWidth(int value);

	QColor           GetGroupBorderColor() const { return m_groupBorderColor; }
	void             SetGroupBorderColor(const QColor& color);

	const CRowModel* GetRowModel() const { return m_pRowModel; }
	void             OnActiveRowChanged(const CRowModel* pNewActiveRow);

	QRect            GetMargins() const;
	void             SetMargins(const QRect& margins);

	void             OnSplitterPosChanged();

	//! Expands a row in this form's scope. Returns the formWidget child of the row
	CFormWidget* ExpandRow(const CRowModel* pRow, bool expand);

	//! Recursively expands to the specified row. Returns the formWidget that contains the row
	CFormWidget* ExpandToRow(const CRowModel* pRow);

	//! Scrolls the parent scroll area to make this row visible
	void ScrollToRow(const CRowModel* pRow);

	//Updates the tree if the model is marked dirty
	void UpdateTree();

private:
	bool  eventFilter(QObject* pWatched, QEvent* pEvent) override;
	void  paintEvent(QPaintEvent* pEvent) override;
	void  resizeEvent(QResizeEvent* event) override;
	bool  event(QEvent* pEvent) override;
	void  mousePressEvent(QMouseEvent* pEvent) override;
	void  mouseReleaseEvent(QMouseEvent* pEvent) override;
	void  mouseMoveEvent(QMouseEvent* pEvent) override;
	void  enterEvent(QEvent* pEvent) override;
	void  leaveEvent(QEvent* pEvent) override;
	void  contextMenuEvent(QContextMenuEvent* pEvent) override;
	void  dragEnterEvent(QDragEnterEvent* pEvent) override;
	void  dragMoveEvent(QDragMoveEvent* pEvent) override;
	void  dragLeaveEvent(QDragLeaveEvent* pEvent) override;
	void  dropEvent(QDropEvent* pEvent) override;
	void  keyPressEvent(QKeyEvent* pEvent) override;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
	bool  focusNextPrevChild(bool next) override;

	void  SetupWidgets();
	void  ReleaseWidgets();

	void  UpdateMinimumSize();
	void  DoLayout(const QSize& rect);
	void  DoFullLayoutUpdate();

	void  OnCopy();
	void  OnPaste();

	void  RecursiveInstallEventFilter(QWidget* pWidget);

	//!An SFormRow is owned by the CFormWidget in the m_rows vector and represent all the children rows of that widget
	/*!
	For example:
		root CFormWidget:
		 -> SFormRow with model Material Settings
		    -> Material Settings CFormWidget
				  -> SFormRow with model Shader Type
					     -> Shader Type CFormWidget with child widget created (and actually owned by) the Shader Type model
		 -> SFormRow shader parameters
				-> Material Settings CFormWidget
				   -> ...
		Note that IPropertyTree derived widgets are only attached to their CFormWidget for painting, construction and release to factory is handled by the model
	*/
	//!This is used to hold a CFormWidget and its related model
	struct SFormRow
	{
		SFormRow(const CRowModel* pRowModel);
		SFormRow(const SFormRow& other) = delete;
		~SFormRow();

		SFormRow& operator=(const SFormRow& other) = delete;

		_smart_ptr<const CRowModel> m_pModel;
		CFormWidget*                m_pChildContainer;
		QWidget*                    m_pWidget; //Widget that is actually displayed on this row, may differ from model's widget due to inlining behavior

		//Cached rowRect for paint event (paint labels, hover highlight and so on)
		QRect                rowRect;

		bool                 IsExpanded() const;
		QWidget*             GetQWidget();
		IPropertyTreeWidget* GetPropertyTreeWidget();
		bool                 HasChildren() const;
	};

	int       GetSplitterPosition() const;
	SFormRow* ModelToFormRow(const CRowModel* pRow);
	//Finds the row at position (if it exist)
	SFormRow* RowAtPosition(const QPoint& position);
	//Finds the row at position (if it exist) and returns the index into the m_rows array
	size_t    RowIndexAtPosition(const QPoint& position);
	int       InsertIndexAtPosition(const QPoint& position, int tolerancePx = 5);
	int       RowIndex(const SFormRow* pRow);

	void      UpdateActiveRow(const QPoint& cursorPos);
	void      SetActiveRow(SFormRow* pRow);
	QRect     GetExpandIconRect(const SFormRow& row) const;
	QRect     GetDragHandleIconRect(const SFormRow& row) const;

	void      ToggleExpand(SFormRow& row, bool skipLayout = false);

	_smart_ptr<const CRowModel>            m_pRowModel;
	QPropertyTree*                        m_pParentTree;
	const int                              m_nesting;

	std::vector<std::unique_ptr<SFormRow>> m_rows;
	SFormRow*                              m_pActiveRow;
	//Last position tested for hovering. Used to avoid unnecessary computing.
	QPoint m_lastCursorPos;                      
	QPoint m_mouseDownPos;
	bool   m_allowReorderChildren;
	//-2 = not dragging at all, -1 = dragging but invalid index, >0 = valid index
	int    m_dragInsertIndex;                      

	//Style props
	int    m_spacing;
	int    m_minRowHeight;
	int    m_iconSize;
	int    m_nestingIndent;
	int    m_groupBorderWidth;
	QColor m_groupBorderColor;
};

class CInlineWidgetBox : public QWidget
{
	Q_OBJECT
public:
	CInlineWidgetBox(CFormWidget* pParent);

	void AddWidget(QWidget* pWidget, QPropertyTree* pPropertyTree);
	void ReleaseWidgets(QPropertyTree* pPropertyTree);

private:
	QHBoxLayout* m_pHBoxLayout;
};
}
