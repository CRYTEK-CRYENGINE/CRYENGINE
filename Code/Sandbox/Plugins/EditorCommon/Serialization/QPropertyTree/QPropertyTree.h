/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <string>
#include <CrySerialization/yasli/Config.h>
#include <CrySerialization/yasli/Serializer.h>
#include <CrySerialization/yasli/Object.h>
#include <CrySerialization/yasli/Pointers.h>
#include "Serialization/PropertyTree/PropertyTree.h"
#include "Serialization/PropertyTree/PropertyTreeStyle.h"
#include "Serialization/PropertyTree/PropertyRow.h"
#include "UIConfigLocal.h"
#include <QWidget>
#include <QPainter>
#include <QIcon>
#include <vector>

class QLineEdit;
class QScrollBar;
class QPropertyTree;
class QMimeData;

namespace property_tree { 
class QDrawContext; 
class QUIFacade;
class IMenu;

#ifndef YASLI_ICON_DEFINED
typedef QIcon PropertyIcon;
#endif
}

class DragWindow : public QWidget
{
	Q_OBJECT
public:
	DragWindow(QPropertyTree* tree);

	void set(QPropertyTree* tree, PropertyRow* row, const QRect& rowRect);
	void setWindowPos(bool visible);
	void show();
	void move(int deltaX, int deltaY);
	void hide();

	void drawRow(QPainter& p);
	void paintEvent(QPaintEvent* ev);

protected:
	bool useLayeredWindows_;
	PropertyRow* row_;
	QRect rect_;
	QPropertyTree* tree_;
	QPoint offset_;
};

class PROPERTY_TREE_API QPropertyTree : public QWidget, public PropertyTree
{
	Q_OBJECT
	Q_PROPERTY(QIcon branchOpened READ getBranchOpened WRITE setBranchOpened DESIGNABLE true)
	Q_PROPERTY(QIcon branchClosed READ getBranchClosed WRITE setBranchClosed DESIGNABLE true)
	Q_PROPERTY(float editsScale READ getEditsScale WRITE setEditsScale DESIGNABLE true)
	Q_PROPERTY(float rowSpacing READ getRowSpacing WRITE setRowSpacing DESIGNABLE true)
private:
	typedef bool (PropertyRow::*PropertyRowDragDropHandler)(PropertyTree* tree);
public:
	explicit QPropertyTree(QWidget* parent = 0);
	~QPropertyTree();
	
	QIcon getBranchOpened() const { return branchOpened_; }
	void setBranchOpened(QIcon icon) { branchOpened_ = icon; }
 
	QIcon getBranchClosed() const { return branchClosed_; }
	void setBranchClosed(QIcon icon) { branchClosed_ = icon; }
 
	float getEditsScale() const { return editsScale_; }
	void setEditsScale(float scale) { editsScale_ = scale; }
	
	float getRowSpacing() const { return treeStyle().rowSpacing; }

	const QFont& boldFont() const;

	// Limit number of mouse-movement updates per-frame. Used to prevent large tree updates
	// from draining all the idle time.
	void setAggregateMouseEvents(bool aggregate) { aggregateMouseEvents_ = aggregate; }
	void flushAggregatedMouseEvents();

	// Default size.
	void setSizeHint(const QSize& size) { sizeHint_ = size; }
	// Sets minimal size of the widget to the size of the visible content of the tree.
	void setSizeToContent(bool sizeToContent);
	bool sizeToContent() const{ return sizeToContent_; }
	// Retrieves size of the content, doesn't require sizeToContent to be set.
	QSize contentSize() const{ return contentSize_; }

	void attachPropertyTree(PropertyTree* propertyTree) override;

	const QMimeData* getMimeData();

public slots:
	void onAttachedTreeChanged();
public:
	// internal methods:
	QPoint _toScreen(Point point) const;
	void _drawRowValue(QPainter& p, const char* text, const QFont* font, const QRect& rect, const QColor& color, bool pathEllipsis, bool center) const;
	int _updateHeightsTime() const{ return updateHeightsTime_; }
	int _paintTime() const{ return paintTime_; }
	bool hasFocusOrInplaceHasFocus() const override;
	bool _isDragged(const PropertyRow* row) const override;
	void _cancelWidget() override;

	QSize sizeHint() const override;

signals:
	// Emitted for every finished changed of the value.  E.g. when you drag a slider,
	// signalChanged will be invoked when you release a mouse button.
	void signalChanged();
	// Used fast-pace changes, like movement of the slider before mouse gets released.
	void signalContinuousChange();
	// Called before and after serialization is invoked for each object. Can be used to update context list
	// in archive.
	void signalAboutToSerialize(yasli::Archive& ar);
	void signalSerialized(yasli::Archive& ar);
	// OBSOLETE: do not use
	void signalObjectChanged(const yasli::Object& obj);
	// Invoked whenever selection changed.
	void signalSelected();
	
	// Invoked before any change is going to occur and can be used to store current version
	// of data for own undo stack.
	void signalPushUndo();
	// Called when visual size of the tree changes, i.e. when things are deserialized and
	// and when rows are expanded/collapsed.
	void signalSizeChanged();
public slots:
    void onFilterChanged(const QString& str);
protected slots:
	void onScroll(int pos);
	void onMouseStillTimer();

protected:
	void onAboutToSerialize(yasli::Archive& ar) override { signalAboutToSerialize(ar); }
	void onSerialized(yasli::Archive& ar) override { signalSerialized(ar); }
	void onChanged() override { signalChanged(); }
	void onContinuousChange() override { signalContinuousChange(); }
	void onSelected() override { signalSelected(); }
	void onPushUndo() override { signalPushUndo(); }

	void copyRow(PropertyRow* row) override;
	void pasteRow(PropertyRow* row) override;
	bool canBePasted(PropertyRow* destination) override;
	bool canBePasted(const char* destinationType) override;

	void interruptDrag() override;
	void defocusInplaceEditor() override;
	class DragController;

	void updateHeights(bool recalculateText = false) override;
	void repaint() override { update(); }
	void resetFilter() override { onFilterChanged(QString()); }

	bool event(QEvent* ev) override;
	void paintEvent(QPaintEvent* ev) override;
	void moveEvent(QMoveEvent* ev) override;
	void resizeEvent(QResizeEvent* ev) override;
	void mousePressEvent(QMouseEvent* ev) override;
	void mouseReleaseEvent(QMouseEvent* ev) override;
	void mouseDoubleClickEvent(QMouseEvent* ev) override;
	void mouseMoveEvent(QMouseEvent* ev) override;
	void wheelEvent(QWheelEvent* ev) override;
	void keyPressEvent(QKeyEvent* ev) override;
	void focusInEvent(QFocusEvent* ev) override;
	bool focusNextPrevChild(bool next) override;

	void dragEnterEvent(QDragEnterEvent* ev) override;
	void dragMoveEvent(QDragMoveEvent* ev) override;
	void dropEvent(QDropEvent* ev) override;

	bool updateScrollBar() override;

	void setFilterMode(bool inFilterMode);
	void startFilter(const char* filter) override;

	using PropertyTree::pointToRootSpace;
	using PropertyTree::pointFromRootSpace;
	QPoint pointToRootSpace(const QPoint& pointInWindowSpace) const;
	QPoint pointFromRootSpace(const QPoint& point) const;

	void _arrangeChildren() override;

	void drawFilteredString(QPainter& p, const char* text, RowFilter::Type type, const QFont* font, const QRect& rect, const QColor& color, bool pathEllipsis, bool center) const;

private:
	void HandleDragDrop(QDropEvent* ev, PropertyRowDragDropHandler handler);

protected:
	QScopedPointer<QLineEdit> filterEntry_; 
	
	QScrollBar* scrollBar_;
	QFont boldFont_;
	QSize sizeHint_;
	DragController* dragController_;
	QTimer* mouseStillTimer_;
	QIcon branchOpened_;
	QIcon branchClosed_;

	float editsScale_;
	bool aggregateMouseEvents_;
	int aggregatedMouseEventCount_;
	QScopedPointer<QMouseEvent> lastMouseMoveEvent_;

	int updateHeightsTime_;
	int paintTime_;
	bool sizeToContent_;
	QSize contentSize_;

private:
	bool dragDropActive_;
	const QMimeData* dragDropMimeData_;  // Contains valid data if dragDropActive_ == true

	friend class property_tree::QDrawContext;
	friend class property_tree::QUIFacade;
	friend struct PropertyTreeMenuHandler;
	friend class FilterEntry;
	friend class DragWindow;
};

