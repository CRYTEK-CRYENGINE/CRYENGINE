// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "QToolWindowManager.h"
#include "QToolWindowRollupBarArea.h"
#include <QWidget>
#include <QVariantMap>
#include <QMouseEvent>
#include <QCollapsibleFrame.h>
#include <QBoxLayout>
#include <QStyleOptionToolBar>
#include <QMenu>
#include <QAction>

class QDragger : public QLabel
{
public:
	QDragger(QWidget* parent)
		: QLabel(parent)
	{
		this->setMinimumSize(1, 1);
		setScaledContents(false);
	}

	void resizeEvent(QResizeEvent * e)
	{
		
		QPixmap corner_img(this->size());
		corner_img.fill(Qt::transparent);
		QStyleOptionToolBar option;
		option.initFrom(parentWidget());
		option.features = QStyleOptionToolBar::Movable;
		option.toolBarArea = Qt::NoToolBarArea;
		option.direction = Qt::RightToLeft;
		option.rect = corner_img.rect();
		option.rect.setWidth(option.rect.width() - 10);
		option.rect.moveTo(5, 3);
		QPainter painter(&corner_img);
		painter.setOpacity(0.5);
		parentWidget()->style()->drawPrimitive(QStyle::PE_IndicatorToolBarHandle, &option, &painter, parentWidget());
		setPixmap(corner_img);
	

		QLabel::resizeEvent(e);
	}
	QSize sizeHint() const
	{
		int w = this->width();
		return QSize(w, 8);
	}

};


QToolWindowRollupBarArea::QToolWindowRollupBarArea(QToolWindowManager* manager, QWidget *parent)
	: QRollupBar(parent),
	m_manager(manager),
	m_tabDragCanStart(false),
	m_areaDragCanStart(false),
	m_areaDragStart(),
	m_areaDraggable(true)
{
	SetRollupsClosable(true);
	connect(this, SIGNAL(RollupCloseRequested(int)), this, SLOT(closeRollup(int)));
	QBoxLayout* currentLayout = qobject_cast<QBoxLayout*>(layout());
	if (currentLayout)
	{
		if (m_manager->config().value(QTWM_AREA_IMAGE_HANDLE, false).toBool())
		{
			QHBoxLayout* horizontalLayout = new QHBoxLayout();
			horizontalLayout->setSpacing(0);
			horizontalLayout->addStretch();
			m_pTopWidget = new QLabel(this);
			m_pTopWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
			m_pTopWidget->setFixedHeight(16);
			horizontalLayout->addWidget(m_pTopWidget);
			currentLayout->insertLayout(0, horizontalLayout);
			QPixmap corner_img;
			corner_img.load(manager->config().value(QTWM_DROPTARGET_COMBINE, ":/QtDockLibrary/gfx/drag_handle.png").toString());
			m_pTopWidget->setPixmap(corner_img);
		}
		else {
			m_pTopWidget = new QDragger(this);
			m_pTopWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
			m_pTopWidget->setFixedHeight(8);
			currentLayout->insertWidget(0, m_pTopWidget);
		}

		m_pTopWidget->setAttribute(Qt::WA_TranslucentBackground);
		m_pTopWidget->setCursor(Qt::CursorShape::OpenHandCursor);
		m_pTopWidget->installEventFilter(this);
	}
	SetRollupsReorderable(true);
}

QToolWindowRollupBarArea::~QToolWindowRollupBarArea()
{
	if (m_manager)
	{
		m_manager->removeArea(this);
		m_manager = nullptr;
	}
}

void QToolWindowRollupBarArea::addToolWindow(QWidget* toolWindow, int index /*= -1*/)
{
	addToolWindows(QList<QWidget*>() << toolWindow, index);
}

void QToolWindowRollupBarArea::addToolWindows(const QList<QWidget*>& toolWindows, int index /*= -1*/)
{
	foreach(QWidget* toolWindow, toolWindows)
	{
		if (index < 0)
		{
			addWidget(toolWindow);
		}
		else
		{
			insertWidget(toolWindow, index);
		}
	}
}

void QToolWindowRollupBarArea::removeToolWindow(QWidget* toolWindow) 
{
	removeWidget(toolWindow);
}

QList<QWidget*> QToolWindowRollupBarArea::toolWindows()
{
	QList<QWidget*> result;
	for (int i = 0; i < count(); i++)
	{
		result.append(widget(i));
	}
	return result;
}

QVariantMap QToolWindowRollupBarArea::saveState()
{
	QVariantMap result;
	result["type"] = "rollup";
	QStringList objectNames;
	QStringList collapsedObjects;
	for (int i = 0; i < count(); i++)
	{
		QString name = widget(i)->objectName();
		if (name.isEmpty())
		{
			qWarning("cannot save state of tool window without object name");
		}
		else
		{
			QCollapsibleFrame* frame = rollupAt(i);
			if (frame->Collapsed())
				collapsedObjects << name;
			objectNames << name;
		}
	}
	result["objectNames"] = objectNames;
	result["collapsedObjects"] = collapsedObjects;
	return result;
}

void QToolWindowRollupBarArea::restoreState(const QVariantMap &data, int stateFormat)
{
	QStringList collapsed = data["collapsedObjects"].toStringList();
	foreach(QVariant objectNameValue, data["objectNames"].toList())
	{
		QString objectName = objectNameValue.toString();
		if (objectName.isEmpty()) { continue; }
		bool found = false;
		foreach(QWidget* toolWindow, m_manager->toolWindows())
		{
			if (toolWindow->objectName() == objectName)
			{
				addToolWindow(toolWindow);
				if (collapsed.contains(objectName))
				{
					QCollapsibleFrame* frame = rollupAt(count() - 1);
					frame->SetCollapsed(true);
				}
				
				found = true;
				break;
			}
		}
		if (!found)
		{
			qWarning("tool window with name '%s' not found", objectName.toLocal8Bit().constData());
		}
	}
}

void QToolWindowRollupBarArea::adjustDragVisuals()
{
	if (count() == 1 && m_manager->isFloatingWrapper(parentWidget()))
	{
		rollupAt(0)->GetDragHandler()->setHidden(true);
	} else if (count() >= 1)
	{
		rollupAt(0)->GetDragHandler()->setHidden(false);
	}

	if ( count() == 1)
	{
		m_pTopWidget->setHidden(true);
	}
	else 
	{
		m_pTopWidget->setVisible(true);
	}

	if (m_manager->config().value(QTWM_RETITLE_WRAPPER, true).toBool() && m_manager->isFloatingWrapper(parentWidget()) && count() == 1)
	{
		IToolWindowWrapper* w = wrapper();
		if (w)
		{
			w->getWidget()->setWindowTitle(widget(0)->windowTitle());
		}
	}
	else
	{
		IToolWindowWrapper* w = wrapper();
		if (w)
		{
			w->getWidget()->setWindowTitle(QCoreApplication::applicationName());
		}
	}
}

bool QToolWindowRollupBarArea::switchAutoHide(bool newValue)
{
	return true;
}

int QToolWindowRollupBarArea::indexOf(QWidget* w) const
{
	return QRollupBar::indexOf(w);
}

void QToolWindowRollupBarArea::setCurrentWidget(QWidget* w) 
{

}

QPoint QToolWindowRollupBarArea::mapCombineDropAreaFromGlobal(const QPoint & pos) const
{
	return getDropTarget()->mapFromGlobal(pos);
}

QRect QToolWindowRollupBarArea::combineAreaRect() const
{
	return getDropTarget()->rect();
}

QRect QToolWindowRollupBarArea::combineSubWidgetRect(int index) const 
{
	QCollapsibleFrame* rollup = rollupAt(index);
	if (rollup)
	{
		QRect newPos = rollup->geometry();
		newPos.moveTop(newPos.y() + getDropTarget()->pos().y());
		return newPos;
	}

	return QRect();
}

int QToolWindowRollupBarArea::subWidgetAt(const QPoint& pos) const 
{	
	return rollupIndexAt(pos);
}

bool QToolWindowRollupBarArea::eventFilter(QObject *o, QEvent *ev)
{
	if (isDragHandle(static_cast<QWidget*>(o)) || o == m_pTopWidget)
	{
		if (ev->type() == QEvent::MouseButtonPress)
		{
			if (o == m_pTopWidget)
			{
				if (m_areaDraggable)
				{ 
					m_areaDragCanStart = true;
					QMouseEvent* me = static_cast<QMouseEvent*>(ev);
					m_areaDragStart = me->pos();
				}

			}
			else
			{
				m_tabDragCanStart = true;
			}

			if (m_manager->isMainWrapper(parentWidget()))
			{
				m_areaDragCanStart = false;
				if (count() == 1)
				{
					m_tabDragCanStart = false;
				}
			}
		}
		else if (ev->type() == QEvent::MouseButtonRelease)
		{ 
			m_areaDragCanStart = false;
			m_tabDragCanStart = false;
		}
		else if (ev->type() == QEvent::MouseMove)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(ev);
			if (m_tabDragCanStart)
			{
				if (rect().contains(mapFromGlobal(me->globalPos())))
				{
					return QRollupBar::eventFilter(o, ev);
				}
				if (qApp->mouseButtons() != Qt::LeftButton)
				{
					return QRollupBar::eventFilter(o, ev);
				}
				QWidget* toolWindow = nullptr;
				for (int i = 0; i < count(); i++)
				{
					if (getDragHandleAt(i) == o)
					{
						toolWindow = widget(i);
					}
				}
				m_tabDragCanStart = false;
				QMouseEvent* releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, me->pos(), Qt::LeftButton, Qt::LeftButton, 0);
				qApp->sendEvent(o, releaseEvent);
				m_manager->startDrag(QList<QWidget*>() << toolWindow, this);
				releaseMouse();
			}
			else if (m_areaDragCanStart)
			{
				if (qApp->mouseButtons() == Qt::LeftButton && !m_areaDragStart.isNull() && (me->pos() - m_areaDragStart).manhattanLength() > QApplication::startDragDistance())
				{
					QList<QWidget*> toolWindows;
					for (int i = 0; i < count(); i++)
					{
						QWidget* toolWindow = widget(i);
						toolWindows << toolWindow;
					}
					QMouseEvent* releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, me->pos(), Qt::LeftButton, Qt::LeftButton, 0);
					qApp->sendEvent(m_pTopWidget, releaseEvent);
					m_manager->startDrag(toolWindows, this);
					releaseMouse();
				}
			}
		}
	}
	return QRollupBar::eventFilter(o, ev);
}

void QToolWindowRollupBarArea::closeRollup(int index)
{
	m_manager->releaseToolWindow(widget(index), true);
}

void QToolWindowRollupBarArea::mouseReleaseEvent(QMouseEvent * e)
{
	if (e->button() == Qt::RightButton)
	{
		if (m_pTopWidget->rect().contains(e->pos()))
		{
			QAction swap("Swap to Tabs", this);
			e->accept();
			connect(&swap, SIGNAL(triggered()), this, SLOT(swapToRollup()));
			QMenu menu(this);
			menu.addAction(&swap);
			menu.exec(mapToGlobal(QPoint(e->pos().x(), e->pos().y() + 10)));
		}

	}

	QRollupBar::mouseReleaseEvent(e);
}

void QToolWindowRollupBarArea::swapToRollup()
{
	m_manager->SwapAreaType(this, watTabs);
}

void QToolWindowRollupBarArea::setDraggable(bool draggable)
{
	m_areaDraggable = draggable;
	if (draggable)
	{
		m_pTopWidget->setCursor(Qt::CursorShape::OpenHandCursor);
	}	
	else
	{
		m_pTopWidget->setCursor(Qt::CursorShape::ArrowCursor);
	}
}

