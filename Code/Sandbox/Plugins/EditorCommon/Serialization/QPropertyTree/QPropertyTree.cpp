/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include <CrySerialization/yasli/Pointers.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/BinArchive.h>
#include "QPropertyTree.h"
#include "QUIFacade.h"
#include "Serialization/PropertyTree/IDrawContext.h"
#include "Serialization/PropertyTree/Serialization.h"
#include "Serialization/PropertyTree/PropertyTreeModel.h"
#include "Serialization/PropertyTree/PropertyOArchive.h"
#include "Serialization/PropertyTree/PropertyIArchive.h"
#include "Serialization/PropertyTree/PropertyTreeStyle.h"
#include "Serialization/PropertyTree/Unicode.h"
#include "Serialization/PropertyTree/PropertyTreeMenuHandler.h"
#include "Serialization/PropertyTree/MathUtils.h"

#include <CrySerialization/yasli/ClassFactory.h>


#include <QRect>
#include <QTimer>
#include <QMimeData>
#include <QMenu>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QLineEdit>
#include <QPainter>
#include <QElapsedTimer>
#include <QStyle>
#include <QToolTip>

// only for clipboard:
#include <QClipboard>
#include <QApplication>
#include "Serialization/PropertyTree/PropertyRowPointer.h"
#include "Serialization/PropertyTree/PropertyRowContainer.h"
// ^^^
#include "Serialization/PropertyTree/PropertyRowObject.h"
#include "QDrawContext.h"
using property_tree::toQRect;
using property_tree::toQPoint;

static int translateKey(int qtKey)
{
	switch (qtKey) {
	case Qt::Key_Backspace: return KEY_BACKSPACE;
	case Qt::Key_Delete: return KEY_DELETE;
	case Qt::Key_Down: return KEY_DOWN;
	case Qt::Key_End: return KEY_END;
	case Qt::Key_Escape: return KEY_ESCAPE;
	case Qt::Key_F2: return KEY_F2;
	case Qt::Key_Home: return KEY_HOME;
	case Qt::Key_Insert: return KEY_INSERT;
	case Qt::Key_Left: return KEY_LEFT;
	case Qt::Key_Menu: return KEY_MENU;
	case Qt::Key_Return: return KEY_RETURN;
	case Qt::Key_Enter: return KEY_ENTER;
	case Qt::Key_Right: return KEY_RIGHT;
	case Qt::Key_Space: return KEY_SPACE;
	case Qt::Key_Up: return KEY_UP;
	case Qt::Key_C: return KEY_C;
	case Qt::Key_V: return KEY_V;
	case Qt::Key_Z: return KEY_Z;
	}

	return KEY_UNKNOWN;
}

static int translateModifiers(int qtModifiers)
{
	int result = 0;
	if (qtModifiers & Qt::ControlModifier)
		result |= MODIFIER_CONTROL;
	else if (qtModifiers & Qt::ShiftModifier)
		result |= MODIFIER_SHIFT;
	else if (qtModifiers & Qt::AltModifier)
		result |= MODIFIER_ALT;
	return result;
}

static KeyEvent translateKeyEvent(const QKeyEvent& ev)
{
	KeyEvent result;
	result.key_ = translateKey(ev.key());
	result.modifiers_ = translateModifiers(ev.modifiers());
	return result;
}

QCursor translateCursor(property_tree::Cursor cursor)
{
	switch (cursor)
	{
	case property_tree::CURSOR_ARROW:
		return QCursor(Qt::ArrowCursor);
	case property_tree::CURSOR_BLANK:
		return QCursor(Qt::BlankCursor);
	case property_tree::CURSOR_SLIDE:
		return QCursor(Qt::ArrowCursor); // TODO
	case property_tree::CURSOR_HAND:
		return QCursor(Qt::PointingHandCursor);
	default:
		return QCursor();
	}
}

using yasli::Serializers;

static QMimeData* propertyRowToMimeData(PropertyRow* row, ConstStringList* constStrings)
{
	PropertyRow::setConstStrings(constStrings);
	SharedPtr<PropertyRow> clonedRow(row->clone(constStrings));
	yasli::BinOArchive oa;
	PropertyRow::setConstStrings(constStrings);
	if (!oa(clonedRow, "row", "Row")) {
		PropertyRow::setConstStrings(0);
        return 0;
	}
	PropertyRow::setConstStrings(0);

	QByteArray byteArray(oa.buffer(), oa.length());
	QMimeData* mime = new QMimeData;
	mime->setText(QString(row->valueAsString()));
	mime->setData("binary/qpropertytree", byteArray);
	return mime;
}

static bool smartPaste(PropertyRow* dest, SharedPtr<PropertyRow>& source, PropertyTreeModel* model, bool onlyCheck)
{
	bool result = false;
	// content of the pulled container has a priority over the node itself
	PropertyRowContainer* destPulledContainer = static_cast<PropertyRowContainer*>(dest->pulledContainer());
	if((destPulledContainer && strcmp(destPulledContainer->elementTypeName(), source->typeName()) == 0)) {
		PropertyRow* elementRow = model->defaultType(destPulledContainer->elementTypeName());
		YASLI_ESCAPE(elementRow, return false);
		if(strcmp(elementRow->typeName(), source->typeName()) == 0){
			result = true;
			if(!onlyCheck){
				PropertyRow* dest = elementRow;
				if(dest->isPointer() && !source->isPointer()){
					PropertyRowPointer* d = static_cast<PropertyRowPointer*>(dest);
					SharedPtr<PropertyRowPointer> newSourceRoot = static_cast<PropertyRowPointer*>(d->clone(model->constStrings()).get());
					source->swapChildren(newSourceRoot, model);
					source = newSourceRoot;
				}
				destPulledContainer->add(source.get());
			}
		}
	}
	else if((source->isContainer() && dest->isContainer() &&
			 strcmp(static_cast<PropertyRowContainer*>(source.get())->elementTypeName(),
					static_cast<PropertyRowContainer*>(dest)->elementTypeName()) == 0) ||
			(!source->isContainer() && !dest->isContainer() && strcmp(source->typeName(), dest->typeName()) == 0)){
		result = true;
		if(!onlyCheck){
			if(dest->isPointer() && !source->isPointer()){
				PropertyRowPointer* d = static_cast<PropertyRowPointer*>(dest);
				SharedPtr<PropertyRowPointer> newSourceRoot = static_cast<PropertyRowPointer*>(d->clone(model->constStrings()).get());
				source->swapChildren(newSourceRoot, model);
				source = newSourceRoot;
			}
			const char* name = dest->name();
			const char* nameAlt = dest->label();
			source->setName(name);
			source->setLabel(nameAlt);
			if(dest->parent())
				dest->parent()->replaceAndPreserveState(dest, source, model);
			else{
				dest->clear();
				dest->swapChildren(source, model);
			}
			source->setLabelChanged();
		}
	}
	else if(dest->isContainer()){
		if(model){
			PropertyRowContainer* container = static_cast<PropertyRowContainer*>(dest);
			PropertyRow* elementRow = model->defaultType(container->elementTypeName());
			YASLI_ESCAPE(elementRow, return false);
			if(strcmp(elementRow->typeName(), source->typeName()) == 0){
				result = true;
				if(!onlyCheck){
					PropertyRow* dest = elementRow;
					if(dest->isPointer() && !source->isPointer()){
						PropertyRowPointer* d = static_cast<PropertyRowPointer*>(dest);
						SharedPtr<PropertyRowPointer> newSourceRoot = static_cast<PropertyRowPointer*>(d->clone(model->constStrings()).get());
						source->swapChildren(newSourceRoot, model);
						source = newSourceRoot;
					}

					container->add(source.get());
				}
			}
			container->setLabelChanged();
		}
	}
	
	return result;
}

static bool propertyRowFromMimeData(SharedPtr<PropertyRow>& row, const QMimeData* mimeData, ConstStringList* constStrings)
{
	PropertyRow::setConstStrings(constStrings);
	QStringList formats = mimeData->formats();
	QByteArray array = mimeData->data("binary/qpropertytree");
	if (array.isEmpty())
		return 0;
	yasli::BinIArchive ia;
	if (!ia.open(array.data(), array.size()))
		return 0;

	if (!ia(row, "row", "Row"))
		return false;

	PropertyRow::setConstStrings(0);
	return true;

}

bool propertyRowFromClipboard(SharedPtr<PropertyRow>& row, ConstStringList* constStrings)
{
	const QMimeData* mime = QApplication::clipboard()->mimeData();
	if (!mime)
		return false;
	return propertyRowFromMimeData(row, mime, constStrings);
}



class FilterEntry : public QLineEdit
{
public:
	FilterEntry(QPropertyTree* tree)
    : QLineEdit(tree)
    , tree_(tree)
	{
	}
protected:

    void keyPressEvent(QKeyEvent * ev)
    {
        if (ev->key() == Qt::Key_Escape || ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter)
        {
            ev->accept();
            tree_->setFocus();
            tree_->keyPressEvent(ev);
        }

        if (ev->key() == Qt::Key_Backspace && text()[0] == '\0')
        {
            tree_->setFilterMode(false);
        }
        QLineEdit::keyPressEvent(ev);
    }
private:
	QPropertyTree* tree_;
};


// ---------------------------------------------------------------------------

DragWindow::DragWindow(QPropertyTree* tree)
: tree_(tree)
, offset_(0, 0)
{
	QWidget::setWindowFlags(Qt::ToolTip);
	QWidget::setWindowOpacity(192.0f/ 256.0f);
}

void DragWindow::set(QPropertyTree* tree, PropertyRow* row, const QRect& rowRect)
{
	QRect rect = tree->rect();
	rect.setTopLeft(tree->mapToGlobal(rect.topLeft()));

	offset_ = rect.topLeft();

	row_ = row;
	rect_ = rowRect;
}

void DragWindow::setWindowPos(bool visible)
{
	QWidget::move(rect_.left() + offset_.x() - 3,  rect_.top() + offset_.y() - 3 + tree_->area_.top());
	QWidget::resize(rect_.width() + 5, rect_.height() + 5);
}

void DragWindow::show()
{
	setWindowPos(true);
	QWidget::show();
}

void DragWindow::move(int deltaX, int deltaY)
{
	offset_ += QPoint(deltaX, deltaY);
	setWindowPos(isVisible());
}

void DragWindow::hide()
{
	setWindowPos(false);
	QWidget::hide();
}

struct DrawVisitor
{
	DrawVisitor(QPainter& painter, const QRect& area, int scrollOffset, bool selectionPass)
		: area_(area)
		, painter_(painter)
		, offset_(0)
		, scrollOffset_(scrollOffset)
		, lastParent_(0)
		, selectionPass_(selectionPass)
	{}

	ScanResult operator()(PropertyRow* row, PropertyTree* tree, int index)
	{
		if(row->visible(tree) && ( row->isRoot() || ((row->parent()->expanded() && !lastParent_) || row->pulledUp()))){
			if(row->rect().top() > scrollOffset_ + area_.height())
				lastParent_ = row->parent();

			QDrawContext context((QPropertyTree*)tree, &painter_);
			if((tree->treeStyle().groupRectangle ? row->rect().top() + row->heightIncludingChildren() : row->rect().bottom()) > scrollOffset_ && row->rect().width() > 0)
				row->drawRow(context, tree, index, selectionPass_);

			return SCAN_CHILDREN_SIBLINGS;
		}
		else
			return SCAN_SIBLINGS;
	}

protected:
	QPainter& painter_;
	QRect area_;
	int offset_;
	int scrollOffset_;
	PropertyRow* lastParent_;
	bool selectionPass_;
};

struct DrawRowVisitor
{
	DrawRowVisitor(QPainter& painter) : painter_(painter) {}

	ScanResult operator()(PropertyRow* row, PropertyTree* tree, int index)
	{
		if(row->pulledUp() && row->visible(tree)) {
			QDrawContext context((QPropertyTree*)tree, &painter_);
			row->drawRow(context, tree, index, true);
			row->drawRow(context, tree, index, false);
		}

		return SCAN_CHILDREN_SIBLINGS;
	}

protected:
	QPainter& painter_;
};


void DragWindow::drawRow(QPainter& p)
{
	QRect entireRowRect(0, 0, rect_.width() + 4, rect_.height() + 4);

	p.setBrush(tree_->palette().button());
	p.setPen(QPen(tree_->palette().color(QPalette::WindowText)));
	p.drawRect(entireRowRect);

	QPoint leftTop = toQRect(row_->rect()).topLeft();
	int offsetX = -leftTop.x() - tree_->config().tabSize + 3;
	int offsetY = -leftTop.y() + 3;
	p.translate(offsetX, offsetY);
	int rowIndex = 0;
	if (row_->parent())
		rowIndex = row_->parent()->childIndex(row_);
	QDrawContext context(tree_, &p);
	row_->drawRow(context, tree_, 0, true);
	row_->drawRow(context, tree_, 0, false);
	DrawRowVisitor visitor(p);
	row_->scanChildren(visitor, tree_);
	p.translate(-offsetX, -offsetY);
}

void DragWindow::paintEvent(QPaintEvent* ev)
{
	QPainter p(this);

	drawRow(p);
}

// ---------------------------------------------------------------------------

class QPropertyTree::DragController
{
public:
	DragController(QPropertyTree* tree)
	: tree_(tree)
	, captured_(false)
	, dragging_(false)
	, before_(false)
	, row_(0)
	, clickedRow_(0)
	, window_(tree)
	, hoveredRow_(0)
	, destinationRow_(0)
	{
	}

	void beginDrag(PropertyRow* clickedRow, PropertyRow* draggedRow, QPoint pt)
	{
		row_ = draggedRow;
		clickedRow_ = clickedRow;
		startPoint_ = pt;
		lastPoint_ = pt;
		captured_ = true;
		dragging_ = false;
	}

	bool dragOn(QPoint screenPoint)
	{
		if (dragging_)
			window_.move(screenPoint.x() - lastPoint_.x(), screenPoint.y() - lastPoint_.y());

		bool needCapture = false;
		if(!dragging_ && (startPoint_ - screenPoint).manhattanLength() >= 5)
			if(row_->canBeDragged()){
				needCapture = true;
				QRect rect = toQRect(row_->rect());
				rect = QRect(rect.topLeft() - toQPoint(tree_->offset_) + QPoint(tree_->config().tabSize, 0), 
							 rect.bottomRight() - toQPoint(tree_->offset_));

				window_.set(tree_, row_, rect);
				window_.move(screenPoint.x() - startPoint_.x(), screenPoint.y() - startPoint_.y());
				window_.show();
				dragging_ = true;
			}

		if(dragging_){
			QPoint point = tree_->mapFromGlobal(screenPoint);
			trackRow(point);
		}
		lastPoint_ = screenPoint;
		return needCapture;
	}

	void interrupt()
	{
		captured_ = false;
		dragging_ = false;
		row_ = 0;
		window_.hide();
	}

	void trackRow(QPoint pt)
	{
		hoveredRow_ = 0;
		destinationRow_ = 0;

		QPoint point = pt;
		PropertyRow* row = tree_->rowByPoint(fromQPoint(point));
		if(!row || !row_)
			return;

		row = row->nonPulledParent();
		if(!row->parent() || row->isChildOf(row_) || row == row_)
			return;

		float pos = (point.y() - row->rect().top()) / float(row->rect().height());
		if(row_->canBeDroppedOn(row->parent(), row, tree_)){
			if(pos < 0.25f){
				destinationRow_ = row->parent();
				hoveredRow_ = row;
				before_ = true;
				return;
			}
			if(pos > 0.75f){
				destinationRow_ = row->parent();
				hoveredRow_ = row;
				before_ = false;
				return;
			}
		}
		if(row_->canBeDroppedOn(row, 0, tree_))
			hoveredRow_ = destinationRow_ = row;
	}

	void drawUnder(QPainter& painter)
	{
		if(dragging_ && destinationRow_ == hoveredRow_ && hoveredRow_){
			QRect rowRect = toQRect(hoveredRow_->rect());
			rowRect.setLeft(rowRect.left() + tree_->config().tabSize);
			QBrush brush(tree_->palette().highlight());
			QColor brushColor = brush.color();
			QColor borderColor(brushColor.alpha() / 4, brushColor.red(), brushColor.green(), brushColor.blue());
			fillRoundRectangle(painter, brush, rowRect, borderColor, 6);
		}
	}

	void drawOver(QPainter& painter)
	{
		if(!dragging_)
			return;

		QRect rowRect = toQRect(row_->rect());

		if(destinationRow_ != hoveredRow_ && hoveredRow_){
			const int tickSize = 4;
			QRect hoveredRect = toQRect(hoveredRow_->rect());
			hoveredRect.setLeft(hoveredRect.left() + tree_->config().tabSize);

			if(!before_){ // previous
				QRect rect(hoveredRect.left() - 1 , hoveredRect.bottom() - 1, hoveredRect.width(), 2);
				QRect rectLeft(hoveredRect.left() - 1 , hoveredRect.bottom() - tickSize, 2, tickSize * 2);
				QRect rectRight(hoveredRect.right() - 1 , hoveredRect.bottom() - tickSize, 2, tickSize * 2);
				painter.fillRect(rect, tree_->palette().highlight());
				painter.fillRect(rectLeft, tree_->palette().highlight());
				painter.fillRect(rectRight, tree_->palette().highlight());
			}
			else{ // next
				QRect rect(hoveredRect.left() - 1 , hoveredRect.top() - 1, hoveredRect.width(), 2);
				QRect rectLeft(hoveredRect.left() - 1 , hoveredRect.top() - tickSize, 2, tickSize * 2);
				QRect rectRight(hoveredRect.right() - 1 , hoveredRect.top() - tickSize, 2, tickSize * 2);
				painter.fillRect(rect, tree_->palette().highlight());
				painter.fillRect(rectLeft, tree_->palette().highlight());
				painter.fillRect(rectRight, tree_->palette().highlight());
			}
		}
	}

	bool drop(QPoint screenPoint)
	{
		bool rowLayoutChanged = false;
		PropertyTreeModel* model = tree_->model();
		if(row_ && hoveredRow_){
			YASLI_ASSERT(destinationRow_);
			clickedRow_->setSelected(false);
			row_->dropInto(destinationRow_, destinationRow_ == hoveredRow_ ? 0 : hoveredRow_, tree_, before_);
			rowLayoutChanged = true;
		}

		captured_ = false;
		dragging_ = false;
		row_ = 0;
		window_.hide();
		hoveredRow_ = 0;
		destinationRow_ = 0;
		return rowLayoutChanged;
	}

	bool captured() const{ return captured_; }
	bool dragging() const{ return dragging_; }
	PropertyRow* draggedRow() { return row_; }
protected:
	DragWindow window_;
	QPropertyTree* tree_;
	PropertyRow* row_;
	PropertyRow* clickedRow_;
	PropertyRow* hoveredRow_;
	PropertyRow* destinationRow_;
	QPoint startPoint_;
	QPoint lastPoint_;
	bool captured_;
	bool dragging_;
	bool before_;
};

// ---------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable: 4355) //  'this' : used in base member initializer list
QPropertyTree::QPropertyTree(QWidget* parent)
: QWidget(parent)
, PropertyTree(new QUIFacade(this))
, sizeHint_(40, 40)
, dragController_(new DragController(this))
, sizeToContent_(false)
, editsScale_(1.6667f)
, updateHeightsTime_(0)
, paintTime_(0)
, aggregatedMouseEventCount_(0)
, aggregateMouseEvents_(false)
, dragDropActive_(false)
, dragDropMimeData_(0)
{
	setFocusPolicy(Qt::WheelFocus);
	setMouseTracking(true); // need to receive mouseMoveEvent to update mouse cursor and tooltip

	scrollBar_ = new QScrollBar(Qt::Vertical, this);
	connect(scrollBar_, SIGNAL(valueChanged(int)), this, SLOT(onScroll(int)));

    filterEntry_.reset(new FilterEntry(this));
    QObject::connect(filterEntry_.data(), SIGNAL(textChanged(const QString&)), this, SLOT(onFilterChanged(const QString&)));
    filterEntry_->hide();

	mouseStillTimer_ = new QTimer(this);
	mouseStillTimer_->setSingleShot(true);
	connect(mouseStillTimer_, SIGNAL(timeout()), this, SLOT(onMouseStillTimer()));

	boldFont_ = font();
	boldFont_.setBold(true);

	setAcceptDrops(true);
}
#pragma warning(pop)

QPropertyTree::~QPropertyTree()
{
	clearMenuHandlers();
}

void QPropertyTree::interruptDrag()
{
	dragController_->interrupt();
}

void QPropertyTree::updateHeights(bool recalculateTextSize)
{
	QFontMetrics fm(font());
	defaultRowHeight_ = max(16, int(fm.lineSpacing() * editsScale_)); // to fit at least 16x16 icons

	QElapsedTimer timer;
	timer.start();

	QRect widgetRect = rect();
	bool scrollbarVisible = updateScrollBar();
	int scrollBarW = scrollbarVisible ? scrollBar_->width() : 0;
	area_ = fromQRect(widgetRect.adjusted(2, 2, -2 - scrollBarW, -2));

	int filterAreaHeight = 0;
	if (filterMode_)
	{
		filterAreaHeight = filterEntry_ ? filterEntry_->height() : 0;
		area_.setTop(area_.top() + filterAreaHeight + 2 + 2);
	}

	model()->root()->updateLabel(this, 0, false);
	int lb = style_->compact ? 0 : 4;
	int rb = widgetRect.right() - lb - scrollBarW - 2;
	int availableWidth = widgetRect.width() - 4 - scrollBarW;
	bool force = lb != leftBorder_ || rb != rightBorder_ || recalculateTextSize;
	leftBorder_ = lb;
	rightBorder_ = rb;
	model()->root()->calculateMinimalSize(this, leftBorder_, availableWidth, force, 0, 0, 0);

	size_.setX(area_.width());

	int totalHeight = area_.top();
	model()->root()->adjustVerticalPosition(this, totalHeight);
	size_.setY(totalHeight - area_.top());
	
	updateScrollBar();

	updateHeightsTime_ = timer.elapsed();

	int	contentHeight = totalHeight + filterAreaHeight + 4;
	if (sizeToContent_) {
		setMaximumHeight(contentHeight);
		setMinimumHeight(contentHeight);
	}
	else {
		setMaximumHeight(QWIDGETSIZE_MAX);
		setMinimumHeight(0);
	}

	QSize contentSize = QSize(area_.width(), contentHeight);
	if (contentSize_.height() != contentHeight) {
		contentSize_ = contentSize;
		signalSizeChanged();
	}
	else {
		contentSize_ = contentSize;
	}

	_arrangeChildren();

	if (updateScrollBar() != scrollbarVisible)
	{
		updateHeights(recalculateTextSize);
	}
	else
	{
		update();
	}

	update();
}

bool QPropertyTree::updateScrollBar()
{
	int pageSize = rect().height() - (filterMode_ ? filterEntry_->height() + 2 : 0) - 4;
	offset_.setX(clamp(offset_.x(), 0, max(0, size_.x() - area_.right() - 1)));
	offset_.setY(clamp(offset_.y(), 0, max(0, size_.y() - pageSize)));

	if (pageSize < size_.y())
	{
		scrollBar_->setRange(0, size_.y() - pageSize);
		scrollBar_->setSliderPosition(offset_.y());
		scrollBar_->setPageStep(pageSize);
		scrollBar_->show();
		scrollBar_->move(rect().right() - scrollBar_->width(), 0);
		scrollBar_->resize(scrollBar_->width(), height());
		return true;
	}
	else
	{
		scrollBar_->hide();
		return false;
	}
}

const QFont& QPropertyTree::boldFont() const
{
	return boldFont_;
}

void QPropertyTree::onScroll(int pos)
{
	offset_.setY(scrollBar_->sliderPosition());
	_arrangeChildren();
	repaint();
}

void QPropertyTree::copyRow(PropertyRow* row)
{
	QMimeData* mime = propertyRowToMimeData(row, model()->constStrings());
	if (mime)
		QApplication::clipboard()->setMimeData(mime);
}

void QPropertyTree::pasteRow(PropertyRow* row)
{
	if(!canBePasted(row))
		return;
	PropertyRow* parent = row->parent();

	model()->rowAboutToBeChanged(row);

	SharedPtr<PropertyRow> source;
	if (!propertyRowFromClipboard(source, model()->constStrings()))
		return;

	if (!smartPaste(row, source, model(), false))
		return;
	
	model()->rowChanged(parent ? parent : model()->root());
}

bool QPropertyTree::canBePasted(PropertyRow* destination)
{
	SharedPtr<PropertyRow> source;
	if (!propertyRowFromClipboard(source, model_->constStrings()))
		return false;

	if (!smartPaste(destination, source, model(), true))
		return false;
	return true;
}

bool QPropertyTree::canBePasted(const char* destinationType)
{
	SharedPtr<PropertyRow> source;
	if (!propertyRowFromClipboard(source, model()->constStrings()))
		return false;

	bool result = strcmp(source->typeName(), destinationType) == 0;
	return result;
}

bool QPropertyTree::hasFocusOrInplaceHasFocus() const
{
	if (hasFocus())
		return true;

	QWidget* inplaceWidget = 0;
	if (widget_.get() && widget_->actualWidget())
		inplaceWidget = (QWidget*)widget_->actualWidget();

	if (inplaceWidget) {
		if (inplaceWidget->hasFocus())
			return true;
		if (inplaceWidget->isAncestorOf(QWidget::focusWidget()))
			return true;

	}

	return false;
}

void QPropertyTree::setFilterMode(bool inFilterMode)
{
    bool changed = filterMode_ != inFilterMode;
    filterMode_ = inFilterMode;
    
	if (filterMode_)
	{
        filterEntry_->show();
		filterEntry_->setFocus();
        filterEntry_->selectAll();
	}
    else
        filterEntry_->hide();

    if (changed)
    {
        onFilterChanged(QString());
    }
}

void QPropertyTree::startFilter(const char* filter)
{
	setFilterMode(true);
	filterEntry_->setText(filter);
    onFilterChanged(filter);
}

void QPropertyTree::_arrangeChildren()
{
	if(widget_.get()){
		PropertyRow* row = widgetRow_;
		if(row->visible(this)){
			QWidget* w = (QWidget*)widget_->actualWidget();
			YASLI_ASSERT(w);
			if(w){
				QRect rect = toQRect(row->widgetRect(this));
				rect = QRect(rect.topLeft() - toQPoint(offset_), 
							 rect.bottomRight() - toQPoint(offset_));
				w->move(rect.topLeft());
				w->resize(rect.size());
				if(!w->isVisible()){
					w->show();
					if (widget_->autoFocus())
						w->setFocus();
				}
			}
			else{
				//YASLI_ASSERT(w);
			}
		}
		else{
			widget_.reset();
		}
	}

	if (filterEntry_) {
		QSize size = rect().size();
		const int padding = 2;
		QRect pos(padding, padding, size.width() - padding * 2, filterEntry_->height());
		filterEntry_->move(pos.topLeft());
		filterEntry_->resize(pos.size() - QSize(scrollBar_ ? scrollBar_->width() : 0, 0));
	}
}

QPoint QPropertyTree::_toScreen(Point point) const
{
	QPoint pt ( point.x() - offset_.x() + area_.left(), 
				point.y() - offset_.y() + area_.top() );

	return mapToGlobal(pt);
}

void QPropertyTree::attachPropertyTree(PropertyTree* propertyTree) 
{ 
	if(attachedPropertyTree_)
		disconnect((QPropertyTree*)attachedPropertyTree_, SIGNAL(signalChanged()), this, SLOT(onAttachedTreeChanged()));
	PropertyTree::attachPropertyTree(propertyTree);
	connect((QPropertyTree*)attachedPropertyTree_, SIGNAL(signalChanged()), this, SLOT(onAttachedTreeChanged()));
}

const QMimeData* QPropertyTree::getMimeData()
{
	assert(dragDropActive_);
	return dragDropMimeData_;
}

struct FilterVisitor
{
	const QPropertyTree::RowFilter& filter_;

	FilterVisitor(const QPropertyTree::RowFilter& filter) 
    : filter_(filter)
    {
    }

	static void markChildrenAsBelonging(PropertyRow* row, bool belongs)
	{
		int count = int(row->count());
		for (int i = 0; i < count; ++i)
		{
			PropertyRow* child = row->childByIndex(i);
			child->setBelongsToFilteredRow(belongs);

			markChildrenAsBelonging(child, belongs);
		}
	}
	
	static bool hasMatchingChildren(PropertyRow* row)
	{
		int numChildren = (int)row->count();
		for (int i = 0; i < numChildren; ++i)
		{
			PropertyRow* child = row->childByIndex(i);
			if (!child)
				continue;
			if (child->matchFilter())
				return true;
			if (hasMatchingChildren(child))
				return true;
		}
		return false;
	}

	ScanResult operator()(PropertyRow* row, PropertyTree* _tree)
	{
		QPropertyTree* tree = (QPropertyTree*)_tree;
		const char* label = row->labelUndecorated();
		yasli::string value = row->valueAsString();

		bool matchFilter = filter_.match(label, filter_.NAME_VALUE, 0, 0) || filter_.match(value.c_str(), filter_.NAME_VALUE, 0, 0);
		if (matchFilter && filter_.typeRelevant(filter_.NAME))
			filter_.match(label, filter_.NAME, 0, 0);
		if (matchFilter && filter_.typeRelevant(filter_.VALUE))
			matchFilter = filter_.match(value.c_str(), filter_.VALUE, 0, 0);
		if (matchFilter && filter_.typeRelevant(filter_.TYPE))
			matchFilter = filter_.match(row->typeNameForFilter(tree), filter_.TYPE, 0, 0);						   
		
		int numChildren = int(row->count());
		if (matchFilter) {
			if (row->pulledBefore() || row->pulledUp()) {
				// treat pulled rows as part of parent
				PropertyRow* parent = row->parent();
				parent->setMatchFilter(true);
				markChildrenAsBelonging(parent, true);
				parent->setBelongsToFilteredRow(false);
			}
			else {
				markChildrenAsBelonging(row, true);
				row->setBelongsToFilteredRow(false);
				row->setLayoutChanged();
				row->setLabelChanged();
			}
		}
		else {
			bool belongs = hasMatchingChildren(row);
			row->setBelongsToFilteredRow(belongs);
			if (belongs) {
				tree->expandRow(row, true, false);
				for (int i = 0; i < numChildren; ++i) {
					PropertyRow* child = row->childByIndex(i);
					if (child->pulledUp())
						child->setBelongsToFilteredRow(true);
				}
			}
			else {
				row->_setExpanded(false);
				row->setLayoutChanged();
			}
		}

		row->setMatchFilter(matchFilter);
		return SCAN_CHILDREN_SIBLINGS;
	}

protected:
	yasli::string labelStart_;
};


void QPropertyTree::onFilterChanged(const QString& text)
{
	QByteArray arr = filterEntry_->text().toLocal8Bit();
	const char* filterStr = filterMode_ ? arr.data() : "";
	rowFilter_.parse(filterStr);
	FilterVisitor visitor(rowFilter_);
	model()->root()->scanChildrenBottomUp(visitor, this);
	updateHeights(false);
}

void QPropertyTree::drawFilteredString(QPainter& p, const char* text, RowFilter::Type type, const QFont* font, const QRect& rect, const QColor& textColor, bool pathEllipsis, bool center) const
{
	int textLen = (int)strlen(text);

	if (textLen == 0)
		return;

	QFontMetrics fm(*font);
	QString str(text);
	QRect textRect = rect;
	int alignment;
	if (center)
		alignment = Qt::AlignHCenter | Qt::AlignVCenter;
	else {
		if (pathEllipsis && textRect.width() < fm.width(str))
			alignment = Qt::AlignRight | Qt::AlignVCenter;
		else
			alignment = Qt::AlignLeft | Qt::AlignVCenter;
	}

	if (filterMode_) {
		size_t hiStart = 0;
		size_t hiEnd = 0;
		bool matched = rowFilter_.match(text, type, &hiStart, &hiEnd) && hiStart != hiEnd;
		if (!matched && (type == RowFilter::NAME || type == RowFilter::VALUE))
			matched = rowFilter_.match(text, RowFilter::NAME_VALUE, &hiStart, &hiEnd);
		if (matched && hiStart != hiEnd) {
			QRectF boxFull;
			QRectF boxStart;
			QRectF boxEnd;

			boxFull = fm.boundingRect(textRect, alignment, str);

			if (hiStart > 0)
				boxStart = fm.boundingRect(textRect, alignment, str.left(hiStart));
			else {
				boxStart = fm.boundingRect(textRect, alignment, str);
				boxStart.setWidth(0.0f);
			}
			boxEnd = fm.boundingRect(textRect, alignment, str.left(hiEnd));

			QColor highlightColor, highlightBorderColor;
			{
				highlightColor = palette().color(QPalette::Highlight);
				int h, s, v;
				highlightColor.getHsv(&h, &s, &v);
				h -= 175;
				if (h < 0)
					h += 360;
				highlightColor.setHsv(h, min(255, int(s * 1.33f)), v, 255);
				highlightBorderColor.setHsv(h, s * 0.5f, v, 255);
			}

			int left = int(boxFull.left() + boxStart.width()) - 1;
			int top = int(boxFull.top());
			int right = int(boxFull.left() + boxEnd.width());
			int bottom = int(boxFull.top() + boxEnd.height());
			QRect highlightRect(left, top, right - left, bottom - top);
			QBrush br(highlightColor);
			p.setBrush(br);
			p.setPen(highlightBorderColor);
			bool oldAntialiasing = p.renderHints().testFlag(QPainter::Antialiasing);
			p.setRenderHint(QPainter::Antialiasing, true);

			QRect intersectedHighlightRect = rect.intersected(highlightRect);
			p.drawRoundedRect(intersectedHighlightRect, 2.0, 2.0);
			p.setRenderHint(QPainter::Antialiasing, oldAntialiasing);
		}
	}

	QBrush textBrush(textColor);
	p.setBrush(textBrush);
	p.setPen(textColor);
	QFont previousFont = p.font();
	p.setFont(*font);
	p.drawText(textRect, alignment, str, 0);
	p.setFont(previousFont);
}

void QPropertyTree::_drawRowValue(QPainter& p, const char* text, const QFont* font, const QRect& rect, const QColor& textColor, bool pathEllipsis, bool center) const
{
	drawFilteredString(p, text, RowFilter::VALUE, font, rect, textColor, pathEllipsis, center);
}

QSize QPropertyTree::sizeHint() const
{
	if (sizeToContent_)
		return minimumSize();
	else
		return sizeHint_;
}

bool QPropertyTree::event(QEvent* ev)
{
	if (ev->type() == QEvent::ToolTip)
	{
		Point point = fromQPoint(mapFromGlobal(QCursor::pos()));
		PropertyRow* row = rowByPoint(point);

		if (row && row->onShowToolTip())
			return true;
	}

	return QWidget::event(ev);
}

void QPropertyTree::paintEvent(QPaintEvent* ev)
{
	QElapsedTimer timer;
	timer.start();
	QPainter painter(this);
	QRect clientRect = this->rect();

	int clientWidth = clientRect.width();
	int clientHeight = clientRect.height();
	painter.fillRect(clientRect, palette().window());

	painter.translate(-offset_.x(), -offset_.y());

	if(dragController_->captured())
	 	dragController_->drawUnder(painter);

    if (model()->root()) {
        DrawVisitor selectionOp(painter, toQRect(area_), offset_.y(), true);
		selectionOp(model()->root(), this, 0);
        model()->root()->scanChildren(selectionOp, this);

        DrawVisitor op(painter, toQRect(area_), offset_.y(), false);
		op(model()->root(), this, 0);
        model()->root()->scanChildren(op, this);
    }

	painter.translate(offset_.x(), offset_.y());

	//painter.setClipRect(rect());

	if (size_.y() > clientHeight)
	{
 	  const int shadowHeight = 10;
		QColor color1(0, 0, 0, 0);
		QColor color2(0, 0, 0, 96);

		QRect upperRect(rect().left(), rect().top(), area_.width(), shadowHeight);
		QLinearGradient upperGradient(upperRect.left(), upperRect.top(), upperRect.left(), upperRect.bottom());
		upperGradient.setColorAt(0.0f, color2);
		upperGradient.setColorAt(1.0f, color1);
		QBrush upperBrush(upperGradient);
		painter.fillRect(upperRect, upperBrush);

		QRect lowerRect(rect().left(), rect().bottom() - shadowHeight / 2, area_.width(), shadowHeight / 2 + 1);
		QLinearGradient lowerGradient(lowerRect.left(), lowerRect.top(), lowerRect.left(), lowerRect.bottom());		
		lowerGradient.setColorAt(0.0f, color1);
		lowerGradient.setColorAt(1.0f, color2);
		QBrush lowerBrush(lowerGradient);
		painter.fillRect(lowerRect, lowerGradient);
	}
	
	if (dragController_->captured()) {
	 	painter.translate(-toQPoint(offset_));
	 	dragController_->drawOver(painter);
	 	painter.translate(toQPoint(offset_));
	}
	else{
	// 	if(model()->focusedRow() != 0 && model()->focusedRow()->isRoot() && tree_->hasFocus()){
	// 		clientRect.left += 2; clientRect.top += 2;
	// 		clientRect.right -= 2; clientRect.bottom -= 2;
	// 		DrawFocusRect(dc, &clientRect);
	// 	}
	}
	paintTime_ = timer.elapsed();
}

QPoint QPropertyTree::pointToRootSpace(const QPoint& point) const
{
	return toQPoint(PropertyTree::pointToRootSpace(fromQPoint(point)));
}

QPoint QPropertyTree::pointFromRootSpace(const QPoint& point) const
{
	return toQPoint(PropertyTree::pointFromRootSpace(fromQPoint(point)));
}

void QPropertyTree::moveEvent(QMoveEvent* ev)
{
	QWidget::moveEvent(ev);
}

void QPropertyTree::resizeEvent(QResizeEvent* ev)
{
	QWidget::resizeEvent(ev);

	if (treeStyle().propertySplitter) {
		const int splitterBorder = 2 * defaultRowHeight_;
		int propertySplitterPos = propertySplitterPos_;

		if (ev->oldSize().width() < 0 && ev->size().width() > 0) {
			propertySplitterPos = ev->size().width() / 2;
		}
		else if (ev->size().width() > 50) {
			float scale = float(ev->size().width()) / float(ev->oldSize().width());
			propertySplitterPos = int(float(propertySplitterPos) * scale);
		}

		propertySplitterPos_ = min(ev->size().width() - splitterBorder, max(splitterBorder, propertySplitterPos));
	}
	
	updateHeights();
}

void QPropertyTree::mousePressEvent(QMouseEvent* ev)
{
	setFocus(Qt::MouseFocusReason);

	if (ev->button() == Qt::LeftButton)
	{
		int x = ev->x();
		int y = ev->y();

		Point point = fromQPoint(ev->pos());
		PropertyRow* row = rowByPoint(point);
		if (row && !row->isRoot() && treeStyle().propertySplitter && (!treeStyle().groupRectangle || !row->isGroupParent(this))) {
			Rect rect = getPropertySplitterRect();
			if (rect.contains(point)) {
				splitterDragging_ = true;
				return;
			}
		}

		if(row && !row->isSelectable())
			row = row->parent();
		if(row){
			if(onRowLMBDown(row, row->rect(), fromQPoint(pointToRootSpace(ev->pos())), ev->modifiers().testFlag(Qt::ControlModifier), ev->modifiers().testFlag(Qt::ShiftModifier))) {
				capturedRow_ = row;
				lastStillPosition_ = fromQPoint(pointToRootSpace(ev->pos()));
			}
			else if (!dragCheckMode_){
				row = rowByPoint(fromQPoint(ev->pos()));
				PropertyRow* draggedRow = row;
				while (draggedRow && (!draggedRow->isSelectable() || draggedRow->pulledUp() || draggedRow->pulledBefore()))
					draggedRow = draggedRow->parent();
				if(draggedRow && !draggedRow->userReadOnly() && !widget_.get()){
					dragController_->beginDrag(row, draggedRow, ev->globalPos());
				}
			}
		}
		update();
	}
	else if (ev->button() == Qt::RightButton)
	{
		Point point = fromQPoint(ev->pos());
		PropertyRow* row = rowByPoint(point);
		if(row){
			model()->setFocusedRow(row);
			update();

			onRowRMBDown(row, row->rect(), fromQPoint(pointToRootSpace(ev->pos())));
		}
		else{
			Rect rect = fromQRect(this->rect());
			onRowRMBDown(model()->root(), rect, fromQPoint(pointToRootSpace(ev->pos())));
		}
	}
}

void QPropertyTree::mouseReleaseEvent(QMouseEvent* ev)
{
	QWidget::mouseReleaseEvent(ev);
	
	if (splitterDragging_)
	{
		splitterDragging_ = false;
		return;
	}

	if (ev->button() == Qt::LeftButton)
	{
		 if(dragController_->captured()){
		 	if (dragController_->drop(QCursor::pos()))
				updateHeights();
			else
				update();
		}
		 if (dragCheckMode_) {
			 dragCheckMode_ = false;
		 }
		 else {
			 Point point = fromQPoint(ev->pos());
			 PropertyRow* row = rowByPoint(point);
			 if(capturedRow_){
				 Rect rowRect = capturedRow_->rect();
				 onRowLMBUp(capturedRow_, rowRect, pointToRootSpace(point));
				 mouseStillTimer_->stop();
				 capturedRow_ = 0;
				 update();
			 }
		 }
	}
	else if (ev->button() == Qt::RightButton)
	{

	}

	unsetCursor();
}

void QPropertyTree::focusInEvent(QFocusEvent* ev)
{
	QWidget::focusInEvent(ev);
	_cancelWidget();
}

void QPropertyTree::defocusInplaceEditor()
{
	if (hasFocusOrInplaceHasFocus())
		setFocus();
}

void QPropertyTree::keyPressEvent(QKeyEvent* ev)
{
	if (ev->key() == Qt::Key_F && ev->modifiers() == Qt::CTRL) {
		setFilterMode(true);
	}

	if (ev->key() == Qt::Key_Up){
		int y = model()->root()->verticalIndex(this, model()->focusedRow());
		if (filterMode_ && y == 0) {
			setFilterMode(true);
			update();
			return;
		}
	}
	else if (ev->key() == Qt::Key_Down) {
		if (filterMode_ && filterEntry_->hasFocus()) {
			setFocus();
			update();
			return;
		}
	}
	else if ((ev->key() == Qt::Key_Tab || ev->key() == Qt::Key_Backtab) && !ev->modifiers().testFlag(Qt::ControlModifier))
	{
		ev->accept();
		return;
	}

	if (filterMode_) {
		if (ev->key() == Qt::Key_Escape && ev->modifiers() == Qt::NoModifier) {
			setFilterMode(false);
		}
	}

	bool result = false;
	if (!widget_.get()) {
		PropertyRow* row = model()->focusedRow();
		if (row) {
			KeyEvent keyEvent = translateKeyEvent(*ev);
			onRowKeyDown(row, &keyEvent);
		}
	}
	update();
	if(!result)
		QWidget::keyPressEvent(ev);
}


void QPropertyTree::mouseDoubleClickEvent(QMouseEvent* ev)
{
	QWidget::mouseDoubleClickEvent(ev);
	
	Point point = fromQPoint(ev->pos());
	PropertyRow* row = rowByPoint(point);
	if (row && !row->isRoot() && treeStyle().propertySplitter && (!treeStyle().groupRectangle || !row->isGroupParent(this)))
	{
		Rect rect = getPropertySplitterRect();
		if (rect.contains(point))
			return;
	}

	if(row){
		PropertyActivationEvent e;
		e.tree = this;
		e.force = true;
		e.reason = e.REASON_DOUBLECLICK;
		PropertyRow* nonPulledParent = row;
		while (nonPulledParent && nonPulledParent->pulledUp())
			nonPulledParent = nonPulledParent->parent();

		PropertyRow* expanding_row = row;
		if(row->widgetRect(this).contains(pointToRootSpace(point))){
			if (!row->onActivate(e))
				expanding_row = nonPulledParent;
		}

		if (expanding_row->expanded())
			collapseChildren(expanding_row);
		else
			expandChildren(expanding_row);
	}
}

void QPropertyTree::onMouseStillTimer()
{
	onMouseStill();
}

void QPropertyTree::flushAggregatedMouseEvents()
{
	if (aggregatedMouseEventCount_ > 0) {
		bool gotPendingEvent = aggregatedMouseEventCount_ > 1;
		aggregatedMouseEventCount_ = 0;
		if (gotPendingEvent && lastMouseMoveEvent_.data())
			mouseMoveEvent(lastMouseMoveEvent_.data());
	}
}

void QPropertyTree::mouseMoveEvent(QMouseEvent* ev)
{
	if (ev->type() == QEvent::MouseMove && aggregateMouseEvents_) {
		lastMouseMoveEvent_.reset(new QMouseEvent(QEvent::MouseMove, ev->localPos(), ev->windowPos(), ev->screenPos(), ev->button(), ev->buttons(), ev->modifiers()));
		ev = lastMouseMoveEvent_.data();
		++aggregatedMouseEventCount_;
		if (aggregatedMouseEventCount_ > 1)
			return;
	}

	if (splitterDragging_)
	{
		if (ev->buttons().testFlag(Qt::LeftButton))
		{
			setPropertySplitterPos(ev->x());
			return;
		}
		else
		{
			splitterDragging_ = false;
		}
	}
	
	QCursor newCursor = QCursor(Qt::ArrowCursor);
	QString newToolTip;
	if(dragController_->captured() && !ev->buttons().testFlag(Qt::LeftButton))
		dragController_->interrupt();
	if(dragController_->captured()){
		QPoint pos = QCursor::pos();
		if (dragController_->dragOn(pos)) {
			// SetCapture
		}
		update();
	}
	else{
		Point point = fromQPoint(ev->pos());
		PropertyRow* row = rowByPoint(point);

		if (row && !row->isRoot()) {
			if (treeStyle().propertySplitter && (!treeStyle().groupRectangle || !row->isGroupParent(this)))
			{
				Rect rect = getPropertySplitterRect();
				if (rect.contains(point) && !ev->buttons().testFlag(Qt::LeftButton))
				{
					setCursor(QCursor(Qt::SplitHCursor));
					return;
				}
			}
		}

		if (row && dragCheckMode_ && row->widgetRect(this).contains(pointToRootSpace(point))) {
			if (row->onMouseDragCheck(this, dragCheckValue_))
			{
				// HACK: There are some instances of onMouseDragCheck (ex: PropertyRowBool) where it recreates or 
				// reverts the current PropertyTree. In this case we need to get the new row at the current mouse 
				// cursor position since the row we were pointing to has been invalidated
				row = rowByPoint(point);
			}
		}
		else if(capturedRow_){
			Modifier modifiers = (Modifier)translateModifiers(ev->modifiers());
			onRowMouseMove(capturedRow_, Rect(), point, modifiers);
			if (config_.sliderUpdateDelay >= 0)
				mouseStillTimer_->start(config_.sliderUpdateDelay);

			/*
			if (cursor().shape() == Qt::BlankCursor)
			{
				pressDelta_ += pointToRootSpace(fromQPoint(ev->pos())) - pressPoint_;
				pointerMovedSincePress_ = true;
				QCursor::setPos(mapToGlobal(toQPoint(PropertyTree::pointFromRootSpace(pressPoint_))));
			}
			else
			*/
			{
				pressDelta_ = pointToRootSpace(fromQPoint(ev->pos())) - pressPoint_;
			}
		}

		PropertyRow* hoverRow = row;
		if (mouseOverRow_ && row != mouseOverRow_)
			mouseOverRow_->onHideToolTip();
		mouseOverRow_ = row;
		if (capturedRow_)
			hoverRow = capturedRow_;
		PropertyHoverInfo hover;
		if (hoverRow) {
			Point pointInRootSpace = pointToRootSpace(point);
			if (hoverRow->getHoverInfo(&hover, pointInRootSpace, this)) {
				newCursor = translateCursor(hover.cursor);
				newToolTip = QString::fromUtf8(hover.toolTip.c_str());

				PropertyRow* tooltipRow = hoverRow;
				while (newToolTip.isEmpty() && tooltipRow->parent() && (tooltipRow->pulledUp() || tooltipRow->pulledBefore())) {
					// check if parent of inlined property has a tooltip instead
					tooltipRow = tooltipRow->parent();
					if (tooltipRow->getHoverInfo(&hover, pointInRootSpace, this))
						newToolTip = QString::fromUtf8(hover.toolTip.c_str());
				}
			}
			
			if (hoverRow->validatorWarningIconRect(this).contains(pointToRootSpace(point))) {
				newCursor = QCursor(Qt::PointingHandCursor);
				newToolTip = "Jump to next warning";
			}
			if (hoverRow->validatorErrorIconRect(this).contains(pointToRootSpace(point))) {
				newCursor = QCursor(Qt::PointingHandCursor);
				newToolTip = "Jump to next error";
			}
		}
	}
	setCursor(newCursor);
	if (toolTip() != newToolTip)
		setToolTip(newToolTip);
		if (newToolTip.isEmpty())
			QToolTip::hideText();
}

void QPropertyTree::wheelEvent(QWheelEvent* ev) 
{
	QWidget::wheelEvent(ev);
	
	float delta = ev->angleDelta().ry() / 360.0f;
	if (ev->modifiers() & Qt::CTRL)	{
		if (delta > 0)
			zoomLevel_ += 1;
		else
			zoomLevel_ -= 1;
		if (zoomLevel_ < 8)
			zoomLevel_ = 8;
		if (zoomLevel_ > 30)
			zoomLevel_ = 30;
		float scale = zoomLevel_ * 0.1f;
		QFont font;
		font.setPointSizeF(font.pointSizeF() * scale);		
		setFont(font);
		font.setBold(true);
		boldFont_ = font;

		updateHeights(true);
	}
	else {
		if (scrollBar_->isVisible() && scrollBar_->isEnabled())
			scrollBar_->setValue(scrollBar_->value() + -ev->delta());
	}
}

bool QPropertyTree::_isDragged(const PropertyRow* row) const
{
	if (!dragController_->dragging())
		return false;
	if (dragController_->draggedRow() == row)
		return true;
	return false;
}

void QPropertyTree::onAttachedTreeChanged()
{
	revert();
}

void QPropertyTree::setSizeToContent(bool sizeToContent)
{
	if (sizeToContent != sizeToContent_)
	{
		sizeToContent_ = sizeToContent;
		updateHeights(false);
	}
}

void QPropertyTree::_cancelWidget()
{
	QWidget* inplaceWidget = 0;
	if (widget_.get() && widget_->actualWidget())
		inplaceWidget = (QWidget*)widget_->actualWidget();

	bool inplaceWidgetHasFocus = false;

	if (inplaceWidget) {
		if (inplaceWidget->hasFocus())
			inplaceWidgetHasFocus = true;
		else if (inplaceWidget->isAncestorOf(QWidget::focusWidget()))
			inplaceWidgetHasFocus = true;
	}

	if (inplaceWidgetHasFocus)
		setFocus();

	PropertyTree::_cancelWidget();
}

bool QPropertyTree::focusNextPrevChild(bool next)
{
	PropertyRow* row = model()->focusedRow();
	if (!row)
		row = model()->getFirstFocusableChild(nullptr, true);

	if (!row)
		row = model()->getFirstFocusableChild(nullptr, false);

	if (row)
	{
		if (!spawnWidget(row, false))
		{
			row = next ? model()->getNextFocusableRow() : model()->getPrevFocusableRow();
			if (row)
			{
				setSelectedRow(row);
				model()->setFocusedRow(row);
				spawnWidget(row, false);
				return false;
			}
		}
	}

	return true;
}


void QPropertyTree::HandleDragDrop(QDropEvent* ev, PropertyRowDragDropHandler handler)
{
	const Point point = fromQPoint(ev->pos());
	PropertyRow* row = rowByPoint(point);

	dragDropMimeData_ = ev->mimeData();
	dragDropActive_ = true;

	if(row && (row->*handler)(this))
	{
		ev->acceptProposedAction();
	}
	else
	{
		ev->setDropAction(Qt::IgnoreAction);
		ev->accept();
	}

	dragDropActive_ = false;
	dragDropMimeData_ = 0;
}

void QPropertyTree::dragEnterEvent(QDragEnterEvent* ev)
{
	HandleDragDrop(ev, &PropertyRow::onDataDragMove);
}

void QPropertyTree::dragMoveEvent(QDragMoveEvent* ev)
{
	HandleDragDrop(ev, &PropertyRow::onDataDragMove);
}

void QPropertyTree::dropEvent(QDropEvent* ev)
{
	HandleDragDrop(ev, &PropertyRow::onDataDrop);
}

FORCE_SEGMENT(PropertyRowIconXPM)
FORCE_SEGMENT(PropertyRowFileSave)

// vim:ts=4 sw=4:

