// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QToolWindowManager.h"
#include "QToolWindowArea.h"
#include "QToolWindowRollupBarArea.h"
#include "QToolWindowWrapper.h"

#include <QApplication>
#include <QBoxLayout>
#include <QTabBar>
#include <QTimer>
#include <QSet>
#include <QWindowStateChangeEvent>
#include <QWindow>

#include "QToolWindowDragHandlerDropTargets.h"
#include "IToolWindowDragHandler.h"
#include "QCustomWindowFrame.h"

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif

QToolWindowManager::QToolWindowManager(QWidget *parent /*= 0*/, QVariant config, QToolWindowManagerClassFactory* factory)
	: QWidget(parent),
	m_factory(factory ? factory : new QToolWindowManagerClassFactory()),
	m_dragHandler(nullptr),
	m_config(config.toMap()),
	m_layoutChangeNotifyLocks(0),
	m_closingWindow(0)
{
	if (!m_factory->parent())
	{
		m_factory->setParent(this);
	}
	m_mainWrapper = new QToolWindowWrapper(this);
	m_mainWrapper->getWidget()->setObjectName("mainWrapper");
	setLayout(new QVBoxLayout(this));
	layout()->setMargin(0);
	layout()->addWidget(m_mainWrapper->getWidget());
	m_lastArea = createArea();
	m_draggedWrapper = nullptr;
	m_resizedWrapper = nullptr;
	m_mainWrapper->setContents(m_lastArea->getWidget());

	m_dragHandler = createDragHandler();

	m_preview = new QRubberBand(QRubberBand::Rectangle);
	m_preview->hide();

	m_raiseTimer = new QTimer(this);
	connect(m_raiseTimer, SIGNAL(timeout()), this, SLOT(raiseCurrentArea()));

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	if (parent && parent->styleSheet() != styleSheet())
	{
		setStyleSheet(parent->styleSheet());
	}

	qApp->installEventFilter(this);
}

QToolWindowManager::~QToolWindowManager()
{
	suspendLayoutNotifications();
	while(!m_areas.isEmpty())
	{
		auto a = m_areas.first();
		a->setParent(nullptr);
		delete a;
	}
	while(!m_wrappers.isEmpty())
	{
		auto w = m_wrappers.first();
		w->setParent(nullptr);
		delete w;
	}
	delete m_dragHandler;
}


QSizePreservingSplitter::QSizePreservingSplitter(QWidget * parent) 
	: QSplitter(parent) 
{
}
QSizePreservingSplitter::QSizePreservingSplitter(Qt::Orientation orientation, QWidget * parent) 
	: QSplitter(orientation, parent) 
{
}

void QSizePreservingSplitter::childEvent(QChildEvent *c)
{
	if (c->type() == QEvent::ChildRemoved)
	{
		QList<int> l = sizes();
		int i = indexOf(static_cast<QWidget*>(c->child()));
		if (i != -1 && l.size() > 1)
		{
			int s = l[i] + handleWidth();
			if (i == 0)
			{
				l[1] = l[1]+s;
			}
			else
			{
				l[i-1] = l[i-1]+s;
			}
			l.removeAt(i);
			QSplitter::childEvent(c);
			setSizes(l);
		}
		else
		{
			QSplitter::childEvent(c);
		}
	}
	else
	{
		QSplitter::childEvent(c);
	}
}


QSplitter *QToolWindowManager::createSplitter()
{
	return m_factory->createSplitter(this);
}

IToolWindowArea* QToolWindowManager::createArea(QTWMWrapperAreaType areaType)
{
	IToolWindowArea* a = m_factory->createArea(this, nullptr, areaType);
	m_lastArea = a;
	m_areas << a;
	return a;
}

IToolWindowWrapper* QToolWindowManager::createWrapper()
{
	IToolWindowWrapper* w = m_factory->createWrapper(this);
	QString name;
	while (true)
	{
		int i = qrand();
		name = QString::asprintf("wrapper#%d", i);
		foreach(IToolWindowWrapper* w2, m_wrappers)
		{
			if (name.compare(w2->getWidget()->objectName()))
			{
				continue;
			}
		}
		break;
	}
	w->getWidget()->setObjectName(name);
	m_wrappers << w;
	return w;
}

void QToolWindowManager::removeArea(IToolWindowArea* area)
{
	m_areas.removeOne(area);
	if (m_lastArea == area)
	{
		m_lastArea = nullptr;
	}
}

void QToolWindowManager::removeWrapper(IToolWindowWrapper* wrapper)
{
	m_wrappers.removeOne(wrapper);
}

void QToolWindowManager::startDrag(const QList<QWidget*> &toolWindows, IToolWindowArea* area)
{
	if (toolWindows.isEmpty())
	{
		return;
	}
	m_dragHandler->startDrag();
	m_draggedToolWindows = toolWindows;

	QRect floatingGeometry = QRect(QCursor::pos(), area->size());
	moveToolWindows(toolWindows, nullptr, QToolWindowAreaReference::Drag, -1, floatingGeometry);
	m_lastArea = nullptr;
	updateDragPosition();
}

void QToolWindowManager::startDrag(IToolWindowWrapper* wrapper)
{
	m_dragHandler->startDrag();
	m_draggedWrapper = wrapper;
	m_lastArea = nullptr;
	updateDragPosition();
}

void QToolWindowManager::startResize(IToolWindowWrapper* wrapper)
{
	m_resizedWrapper = wrapper;
}

void QToolWindowManager::addToolWindow(QWidget* toolWindow, IToolWindowArea* area, QToolWindowAreaReference::eType reference /*= Combine*/, int index /*= -1*/, QRect geometry /*= QRect()*/)
{
	addToolWindows(QList<QWidget*>() << toolWindow, area, reference, index, geometry);
}

void QToolWindowManager::addToolWindow(QWidget* toolWindow, const QToolWindowAreaTarget& target, const QTWMToolType toolType /*= ttStandard*/)
{
	insertToToolTypes(toolWindow, toolType);
	addToolWindow(toolWindow, target.area, target.reference, target.index, target.geometry);
}

void QToolWindowManager::addToolWindows(const QList<QWidget*>& toolWindows, IToolWindowArea* area, QToolWindowAreaReference::eType reference /*= Combine*/, int index /*= -1*/, QRect geometry /*= QRect()*/)
{
	foreach(QWidget* toolWindow, toolWindows)
	{
		if(!ownsToolWindow(toolWindow))
		{
			toolWindow->hide();
			toolWindow->setParent(0);
			toolWindow->installEventFilter(this);
			insertToToolTypes(toolWindow, ttStandard);
			m_toolWindows << toolWindow;
		}
	}
	moveToolWindows(toolWindows, area, reference, index, geometry);
}

void QToolWindowManager::addToolWindows(const QList<QWidget*>& toolWindows, const QToolWindowAreaTarget& target, const QTWMToolType toolType /*= ttStandard*/)
{
	foreach(QWidget* toolWindow, toolWindows)
	{
		insertToToolTypes(toolWindow, toolType);
	}
	addToolWindows(toolWindows, target.area, target.reference, target.index, target.geometry);
}

void QToolWindowManager::moveToolWindow(QWidget* toolWindow, IToolWindowArea* area, QToolWindowAreaReference::eType reference /*= Combine*/, int index /*= -1*/, QRect geometry /*= QRect()*/)
{
	moveToolWindows(QList<QWidget*>() << toolWindow, area, reference, index, geometry);
}

void QToolWindowManager::moveToolWindow(QWidget* toolWindow, const QToolWindowAreaTarget& target, const QTWMToolType toolType /*= ttStandard*/)
{
	insertToToolTypes(toolWindow, toolType);
	addToolWindow(toolWindow, target.area, target.reference, target.index, target.geometry);
}

void QToolWindowManager::moveToolWindows(const QList<QWidget*>& toolWindows, const QToolWindowAreaTarget& target, const QTWMToolType toolType /*= ttStandard*/)
{
	foreach(QWidget* toolWindow, toolWindows)
	{
		insertToToolTypes(toolWindow, toolType);
	}
	addToolWindows(toolWindows, target.area, target.reference, target.index, target.geometry);
}

void QToolWindowManager::moveToolWindows(const QList<QWidget*>& toolWindows, IToolWindowArea* area, QToolWindowAreaReference::eType reference /*= Combine*/, int index /*= -1*/, QRect geometry /*= QRect()*/)
{
	IToolWindowWrapper* wrapper = nullptr;
	bool currentAreaIsSimple = true;
	foreach(QWidget* toolWindow, toolWindows)
	{
		// when iterating  over the tool windows, we will figure out if the current are is actually roll-ups and not tabs
		IToolWindowArea* currentArea = findClosestParent<IToolWindowArea*>(toolWindow);
		if (currentAreaIsSimple && currentArea && currentArea->areaType() == watTabs)
			currentAreaIsSimple = false;
		releaseToolWindow(toolWindow, false);
	}

	if (!area)
	{
		if (m_lastArea)
		{
			area = m_lastArea;
		}
		else if (!m_areas.isEmpty())
		{
			area = m_areas.first();
		}
		else
		{
			area = m_lastArea = createArea();
			m_mainWrapper->setContents(m_lastArea->getWidget());
		}
	}
	switch (reference)
	{
	case QToolWindowAreaReference::Top: // top of furthest parent
	case QToolWindowAreaReference::Bottom: // bottom of furthest parent
	case QToolWindowAreaReference::Left: // left of furthest parent
	case QToolWindowAreaReference::Right: // right of furthest parent
		area = qobject_cast<IToolWindowArea*>(splitArea(getFurthestParentArea(area->getWidget())->getWidget(), reference));
		break;
	case QToolWindowAreaReference::HSplitTop: // above closest parent
	case QToolWindowAreaReference::HSplitBottom: // below closest parent
	case QToolWindowAreaReference::VSplitLeft: // left of closest parent
	case QToolWindowAreaReference::VSplitRight: // right of closest parent
		area = qobject_cast<IToolWindowArea*>(splitArea(area->getWidget(), reference));
		break;
	case QToolWindowAreaReference::Floating:
	case QToolWindowAreaReference::Drag:
	{
		// when dragging we will try to determine target are type, from window types.
		// when we drag more tool windows, we preserve area type geofre drag started
		QTWMWrapperAreaType areaType = watTabs;
		if (toolWindows.count() > 1)
		{
			if (currentAreaIsSimple)
				areaType = watRollups;
		}
		else if (m_toolWindowsTypes[toolWindows[0]] == ttSimple)
		{
			areaType = watRollups;
		}

		// create new window
		area = createArea(areaType);
		wrapper = createWrapper();
		wrapper->setContents(area->getWidget());
		wrapper->getWidget()->show();

		if (geometry != QRect())
		{
			wrapper->getWidget()->setGeometry(geometry);
		}
		else
		{
			wrapper->getWidget()->setGeometry(QRect(QPoint(0, 0), toolWindows[0]->sizeHint()));
			wrapper->getWidget()->move(QCursor::pos());
		}		
	}
	break;
	default: //combine + hidden
		// no special handling needed
		break;
	}
	if (reference != QToolWindowAreaReference::Hidden)
	{
		area->addToolWindows(toolWindows, index);
		m_lastArea = area;
	}
	simplifyLayout();
	foreach(QWidget* toolWindow, toolWindows)
	{
		toolWindowVisibilityChanged(toolWindow, toolWindow->parent() != 0);
	}
	notifyLayoutChange();

	if (reference == QToolWindowAreaReference::Drag && wrapper)
	{
		m_draggedWrapper = wrapper;
		// Move wrapper to mouse position
		wrapper->getWidget()->move(QCursor::pos());
		wrapper->startDrag();
	}
}

bool QToolWindowManager::eventFilter(QObject* o, QEvent* e)
{
	if (o == qApp)
	{
		if (e->type() == QEvent::ApplicationActivate && !config().value(QTWM_WRAPPERS_ARE_CHILDREN, false).toBool() && config().value(QTWM_BRING_ALL_TO_FRONT, true).toBool())
		{
			// Restore all the Z orders
			bringAllToFront();
		}
		return false;
	}

	QWidget* w = static_cast<QWidget*>(o);

	switch (e->type())
	{
	case QEvent::Destroy:
		if (!m_closingWindow && ownsToolWindow(w) && w->isVisible())
		{
			releaseToolWindow(w, true);
			return false;
		}
		break;
	default:
		break;
	}

	return QWidget::eventFilter(o, e);
}

bool QToolWindowManager::event(QEvent* e)
{
	if (e->type() == QEvent::StyleChange)
	{
		if (parentWidget() && parentWidget()->styleSheet() != styleSheet())
		{
			setStyleSheet(parentWidget()->styleSheet());
		}
	}
#if defined(WIN32) || defined(WIN64)
	if (e->type() == QEvent::ParentChange && m_config.value(QTWM_WRAPPERS_ARE_CHILDREN, false).toBool())
	{
		foreach(IToolWindowWrapper* wrapper, m_wrappers)
		{
			if (wrapper->getWidget()->isWindow() && wrapper->getContents())
			{
				SetWindowLongPtr((HWND)wrapper->getWidget()->winId(), GWLP_HWNDPARENT, (LONG_PTR)(this->winId()));
			}
		}
	}
#endif
	return QWidget::event(e);
}

QWidget* QToolWindowManager::splitArea(QWidget* area, QToolWindowAreaReference reference, QWidget* insertWidget)
{
	QWidget* residingWidget = insertWidget;
	
	if (!residingWidget)
	{
		residingWidget = createArea()->getWidget();
	}

	if (!reference.requiresSplit()) //combine + floating
	{
		qWarning("Invalid reference for area split");
		return nullptr;
	}
	bool forceOuter = reference.isOuter();
	reference = static_cast<QToolWindowAreaReference>(reference & 0x3); //Only preserve direction
	QSplitter* parentSplitter = nullptr;
	IToolWindowWrapper* targetWrapper = findClosestParent<IToolWindowWrapper*>(area);
	if (!forceOuter)
	{
		parentSplitter = qobject_cast<QSplitter*>(area->parentWidget());
	}
	if (!parentSplitter && !targetWrapper)
	{
		qWarning("Could not determine area parent");
		return nullptr;
	}
	bool useParentSplitter = false;
	int targetIndex = 0;
	QList<int> parentSizes; 
	if (parentSplitter)
	{
		parentSizes = parentSplitter->sizes();
		targetIndex += parentSplitter->indexOf(area);
		useParentSplitter = parentSplitter->orientation() == reference.splitOrientation();
	}
	if (useParentSplitter)
	{
		int origIndex = targetIndex;
		targetIndex += reference & 0x1;
		IToolWindowDragHandler::SplitSizes newSizes = m_dragHandler->getSplitSizes(parentSizes[origIndex]);
		parentSizes[origIndex] = newSizes.oldSize;
		parentSizes.insert(targetIndex, newSizes.newSize);
		parentSplitter->insertWidget(targetIndex, residingWidget); // left/top = +0, right/bottom = +1
		parentSplitter->setSizes(parentSizes);
		return residingWidget;
	}
	QSplitter* splitter = createSplitter();
	splitter->setOrientation(reference.splitOrientation());

	if (forceOuter || area == targetWrapper->getContents())
	{
		QWidget* firstChild = targetWrapper->getContents();
		targetWrapper->setContents(nullptr);
		splitter->addWidget(firstChild);
	}
	else
	{
		area->hide();
		area->setParent(nullptr);
		splitter->addWidget(area);
		area->show();
	}
	splitter->insertWidget(reference & 0x1, residingWidget);
	if (parentSplitter)
	{
		parentSplitter->insertWidget(targetIndex, splitter);
		parentSplitter->setSizes(parentSizes);
	}
	else
	{
		targetWrapper->setContents(splitter);
	}

	QList<int> sizes;
	int baseSize = (splitter->orientation() == Qt::Vertical ? splitter->height() : splitter->width());
	IToolWindowDragHandler::SplitSizes newSizes = m_dragHandler->getSplitSizes(baseSize);
	sizes.append(newSizes.oldSize);
	sizes.insert(reference & 0x1, newSizes.newSize);
	splitter->setSizes(sizes);

	// HACK: For some reason, contents aren't always properly notified when parent changes.
	// This can issues, as contents can still be referencing deleted wrappers.
	// Manually send all contents a parent changed event.
	QList<QWidget*> contentsWidgets = residingWidget->findChildren<QWidget*>();
	foreach(QWidget* w, contentsWidgets)
		qApp->sendEvent(w, new QEvent(QEvent::ParentChange));

	return residingWidget;
}

IToolWindowArea* QToolWindowManager::getClosestParentArea(QWidget* widget)
{
	IToolWindowArea* area = findClosestParent<IToolWindowArea*>(widget);
	while (area && !ownsArea(area))
	{
		area = findClosestParent<IToolWindowArea*>(area->getWidget()->parentWidget());
	}

	return area;
}

IToolWindowArea* QToolWindowManager::getFurthestParentArea(QWidget* widget)
{
	IToolWindowArea* area = findClosestParent<IToolWindowArea*>(widget);
	IToolWindowArea* previousArea = area;
	while (area && !ownsArea(area))
	{
		area = findClosestParent<IToolWindowArea*>(area->getWidget()->parentWidget());

		if (!area)
		{
			return previousArea;
		}
		else
		{
			previousArea = area;
		}
	}

	return area;
}

bool QToolWindowManager::releaseToolWindows(QList<QWidget*> toolWindows, bool allowClose /* = false */)
{
	bool result = true;
	while (!toolWindows.empty())
	{
		result &= releaseToolWindow(toolWindows.takeFirst(), allowClose);
	}
	return result;
}


bool QToolWindowManager::tryCloseToolWindow(QWidget* toolWindow)
{
	m_closingWindow++;
	bool result = true;

	if (!toolWindow->close())
	{
		qWarning("Widget could not be closed");
		result =  false;
	}

	m_closingWindow--;
	return result;
}

bool QToolWindowManager::releaseToolWindow(QWidget* toolWindow, bool allowClose /* = false */)
{
	// No parent, so can't possibly be inside an IToolWindowArea
	if (!toolWindow->parentWidget())
	{
		return false;
	}

	if (!ownsToolWindow(toolWindow))
	{
		qWarning("Unknown tool window");
		return false;
	}

	IToolWindowArea* previousTabWidget = findClosestParent<IToolWindowArea*>(toolWindow);
	if (!previousTabWidget)
	{
		qWarning("cannot find tab widget for tool window");
		return false;
	}
	if (allowClose)
	{
		QTWMReleaseCachingPolicy releasePolicy = (QTWMReleaseCachingPolicy)config().value(QTWM_RELEASE_POLICY, rcpWidget).toInt();
		switch (releasePolicy)
		{
		case rcpKeep:
			moveToolWindow(toolWindow, nullptr, QToolWindowAreaReference::Hidden);
			if (config().value(QTWM_ALWAYS_CLOSE_WIDGETS, true).toBool() && !tryCloseToolWindow(toolWindow))
			{
				return false;
			}
			break;
		case rcpWidget:
			if (!toolWindow->testAttribute(Qt::WA_DeleteOnClose))
			{
				if (config().value(QTWM_ALWAYS_CLOSE_WIDGETS, true).toBool() && !tryCloseToolWindow(toolWindow))
				{
					return false;
				}
				moveToolWindow(toolWindow, nullptr, QToolWindowAreaReference::Hidden);				
				break;
			}
			//intentional fallthrough
		case rcpForget:
		case rcpDelete:
			if (!tryCloseToolWindow(toolWindow))
			{
				return false;
			}
			moveToolWindow(toolWindow, nullptr, QToolWindowAreaReference::Hidden);
			m_toolWindows.removeOne(toolWindow);
			
			if (releasePolicy == rcpDelete)
				toolWindow->deleteLater();
			return true;
		}
	}
	previousTabWidget->removeToolWindow(toolWindow);
	if (allowClose)
	{
		simplifyLayout();
	}
	else
	{
		previousTabWidget->adjustDragVisuals();
	}
	toolWindow->hide();
	toolWindow->setParent(0);	
	return true;
}

void QToolWindowManager::updateDragPosition()
{
	if (!m_draggedWrapper)
	{
		return;
	}

	m_draggedWrapper->getWidget()->raise();

	QWidget* hoveredWindow = windowBelow(m_draggedWrapper->getWidget());
	if (hoveredWindow)
	{
		QWidget* handlerWidget = hoveredWindow->childAt(hoveredWindow->mapFromGlobal(QCursor::pos()));
		if (handlerWidget)
		{
			if (!m_dragHandler->isHandlerWidget(handlerWidget))
			{
				IToolWindowArea* area = getClosestParentArea(handlerWidget);

				if (area && m_lastArea != area)
				{
					m_dragHandler->switchedArea(m_lastArea, area);
					m_lastArea = area;
					int delayTime = m_config.value(QTWM_RAISE_DELAY, 500).toInt();
					m_raiseTimer->stop();
					if (delayTime > 0)
					{
						m_raiseTimer->start(delayTime);
					}
				}
			}
		}
	}

	QToolWindowAreaTarget target = m_dragHandler->getTargetFromPosition(m_lastArea);
	if (m_lastArea && target.reference != QToolWindowAreaReference::Floating)
	{
		if (target.reference.isOuter())
		{
			IToolWindowWrapper* previewArea = findClosestParent<IToolWindowWrapper*>(m_lastArea->getWidget());
			QWidget* previewAreaContents = previewArea->getContents();
			m_preview->setParent(previewArea->getWidget());
			m_preview->setGeometry(m_dragHandler->getRectFromCursorPos(previewAreaContents, m_lastArea).translated(previewAreaContents->pos()));
		}
		else
		{
			QWidget* previewArea = m_lastArea->getWidget();
			m_preview->setParent(previewArea);
			m_preview->setGeometry(m_dragHandler->getRectFromCursorPos(previewArea, m_lastArea));
		}
		
		m_preview->show();
	}
	else
	{
		m_preview->hide();
	}

	//Request tooltip update
	updateTrackingTooltip(textForPosition(target.reference), QCursor::pos() + config().value(QTWM_TOOLTIP_OFFSET, QPoint(1, 20)).toPoint());
}

void QToolWindowManager::finishWrapperDrag()
{
	QToolWindowAreaTarget target = m_dragHandler->finishDrag(m_draggedToolWindows, nullptr, m_lastArea);
	m_lastArea = nullptr;

	IToolWindowWrapper* wrapper = m_draggedWrapper;
	m_draggedWrapper = nullptr;
	m_raiseTimer->stop();
	m_preview->setParent(nullptr);
	m_preview->hide();

	// keep a list of all contained tools for later notifications
	QWidget* const contents = wrapper->getContents();
	QList<QWidget*> toolWindows;
	QList<QWidget*> contentsWidgets = contents->findChildren<QWidget*>();
	contentsWidgets << contents;

	foreach(QWidget* w, contentsWidgets)
	{
		IToolWindowArea* area = qobject_cast<IToolWindowArea*>(w);
		if (area && ownsArea(area))
		{
			toolWindows << area->toolWindows();
		}
	}

	if (target.reference != QToolWindowAreaReference::Floating)
	{
		QWidget* contents = wrapper->getContents();
		wrapper->setContents(nullptr);

		switch (target.reference)
		{
		case QToolWindowAreaReference::Top: // top of furthest parent
		case QToolWindowAreaReference::Bottom: // bottom of furthest parent
		case QToolWindowAreaReference::Left: // left of furthest parent
		case QToolWindowAreaReference::Right: // right of furthest parent
			splitArea(getFurthestParentArea(target.area->getWidget())->getWidget(), target.reference, contents);
			break;
		case QToolWindowAreaReference::HSplitTop: // above closest parent
		case QToolWindowAreaReference::HSplitBottom: // below closest parent
		case QToolWindowAreaReference::VSplitLeft: // left of closest parent
		case QToolWindowAreaReference::VSplitRight: // right of closest parent
			splitArea(target.area->getWidget(), target.reference, contents);
			break;
		case QToolWindowAreaReference::Combine:
			// take all widgets anywhere in the wrapper and add them to the selected area		
			moveToolWindows(toolWindows, target);
			break;
		}
		wrapper->getWidget()->close();
		simplifyLayout();
	}
	if (target.reference != QToolWindowAreaReference::Combine)
	{
		// Inform about the move of all tools in the dragged wrapper and make sure the layout is saved
		foreach(QWidget* w, toolWindows)
		{
			toolWindowVisibilityChanged(w, true);
		}
	}
	if (target.reference != QToolWindowAreaReference::Floating)
	{
		notifyLayoutChange();
	}
	updateTrackingTooltip("", QPoint());
}

void QToolWindowManager::finishWrapperResize()
{
	QList<QWidget*> toolWindows;
	{
		QWidget* const contents = m_resizedWrapper->getContents();
		QList<QWidget*> contentsWidgets = contents->findChildren<QWidget*>();
		contentsWidgets << contents;

		foreach(QWidget* w, contentsWidgets)
		{
			IToolWindowArea* area = qobject_cast<IToolWindowArea*>(w);
			if (area && ownsArea(area))
			{
				toolWindows << area->toolWindows();
			}
		}
	}

	// Inform about the resize of all tools in the dragged wrapper and make sure the layout is saved
	foreach(QWidget* w, toolWindows)
	{
		toolWindowVisibilityChanged(w, true);
	}

	m_resizedWrapper = nullptr;
}

void QToolWindowManager::simplifyLayout(bool clearMain /* = false */)
{
	suspendLayoutNotifications();
	bool madeChanges; // Some layout changes may require multiple iterations to fully simplify.
	do 
	{
		madeChanges = false;
		QList<IToolWindowArea*> areasToRemove;
		foreach(IToolWindowArea* area, m_areas)
		{
			//remove empty areas (if this is only area in main wrapper, only remove it when we explicitly ask for that)
			if (area->count() == 0 && (clearMain || qobject_cast<IToolWindowWrapper*>(area->parent()) != m_mainWrapper))
			{
				areasToRemove.append(area);
				madeChanges = true;
			}

			QSplitter* s = qobject_cast<QSplitter*>(area->parentWidget());
			while (s && s->parentWidget())
			{
				QSplitter* sp_s = qobject_cast<QSplitter*>(s->parentWidget());
				IToolWindowWrapper* sp_w = qobject_cast<IToolWindowWrapper*>(s->parentWidget());
				IToolWindowWrapper* sw = findClosestParent<IToolWindowWrapper*>(s);

				//If splitter only contains one object, replace the splitter with the contained object
				if (s->count() == 1) 
				{					
					if (sp_s)
					{
						int index = sp_s->indexOf(s);
						QList<int> sizes = sp_s->sizes();
						sp_s->insertWidget(index, s->widget(0));
						s->hide();
						s->setParent(nullptr);
						s->deleteLater();
						sp_s->setSizes(sizes);
						madeChanges = true;
					}
					else if (sp_w)
					{
						sp_w->setContents(s->widget(0));
						s->setParent(nullptr);
						s->deleteLater();
						madeChanges = true;
					}
					else
					{
						qWarning("Unexpected splitter parent");
					}
				}
				//If splitter's parent is also a splitter, and both have same orientation, replace splitter with contents
				else if (sp_s && s->orientation() == sp_s->orientation())
				{
					int index = sp_s->indexOf(s);
					QList<int> newSizes = sp_s->sizes();
					QList<int> oldSizes = s->sizes();
					int newSum = newSizes[index];
					int oldSum = 0;
					foreach(int i, oldSizes)
					{
						oldSum += i;
					}
					for(int i = oldSizes.count() - 1; i >= 0; i--)
					{
						sp_s->insertWidget(index, s->widget(i));
						newSizes.insert(index, (float)oldSizes[i] / oldSum * newSum);
					}
					s->hide();
					s->setParent(nullptr);
					s->deleteLater();
					sp_s->setSizes(newSizes);
					madeChanges = true;
				}
				s = sp_s;
			}
		}

		foreach(IToolWindowArea* area, areasToRemove)
		{
			area->hide();
			IToolWindowWrapper* aw = qobject_cast<IToolWindowWrapper*>(area->parentWidget());
			if (aw)
			{
				aw->setContents(nullptr);
			}
			area->setParent(nullptr);
			m_areas.removeOne(area);
			area->deleteLater();
		}
	} while (madeChanges);

	QList<IToolWindowWrapper*> wrappersToRemove;
	//Remove empty wrappers
	foreach(IToolWindowWrapper* wrapper, m_wrappers)
	{
		if (wrapper->getWidget()->isWindow() && !wrapper->getContents())
		{
			wrappersToRemove.append(wrapper);
		}
	}

	foreach(IToolWindowWrapper* wrapper, wrappersToRemove)
	{
		m_wrappers.removeOne(wrapper);
		wrapper->hide();
		wrapper->deferDeletion();
	}

	//Update area visuals
	foreach(IToolWindowArea* area, m_areas)
	{
		area->adjustDragVisuals();
	}
	
	resumeLayoutNotifications();
	notifyLayoutChange();
}

QVariant QToolWindowManager::saveState()
{
	QVariantMap result;
	result["toolWindowManagerStateFormat"] = 2;
	if (m_mainWrapper->getContents() && m_mainWrapper->getContents()->metaObject())
	{
		result["mainWrapper"] = saveWrapperState(m_mainWrapper);
	}
	QVariantList floatingWindowsData;
	foreach(IToolWindowWrapper* wrapper, m_wrappers)
	{
		if (wrapper->getWidget()->isWindow() && wrapper->getContents() && wrapper->getContents()->metaObject())
		{
			QVariantMap m = saveWrapperState(wrapper);
			if (!m.isEmpty())
			{
				floatingWindowsData << m;
			}
		}
	}
	result["floatingWindows"] = floatingWindowsData;
	return result;
}

void QToolWindowManager::restoreState(const QVariant &data)
{
	if (!data.isValid()) { return; }
	QVariantMap dataMap = data.toMap();
	int stateFormat = dataMap["toolWindowManagerStateFormat"].toInt();
	if (stateFormat != 1 && stateFormat != 2)
	{
		qWarning("state format is not recognized");
		return;
	}
	suspendLayoutNotifications();
	m_mainWrapper->getWidget()->hide();
	foreach(IToolWindowWrapper* wrapper, m_wrappers)
	{
		wrapper->getWidget()->hide();
	}
	moveToolWindows(m_toolWindows, 0, QToolWindowAreaReference::Hidden);
	simplifyLayout(true);
	m_mainWrapper->setContents(nullptr);
	restoreWrapperState(dataMap["mainWrapper"].toMap(), stateFormat, m_mainWrapper);
	foreach(QVariant windowData, dataMap["floatingWindows"].toList())
	{
		restoreWrapperState(windowData.toMap(), stateFormat);
	}
	simplifyLayout();
	foreach(QWidget* toolWindow, m_toolWindows)
	{
		toolWindowVisibilityChanged(toolWindow, toolWindow->parentWidget() != 0);
	}
	resumeLayoutNotifications();
	notifyLayoutChange();	
}

bool QToolWindowManager::isAnyWindowActive()
{
	foreach(QWidget* tool, m_toolWindows)
	{
		if (tool->isActiveWindow())
		{
			return true;
		}
	}
	return false;
}

QVariantMap QToolWindowManager::saveWrapperState(IToolWindowWrapper* wrapper)
{
	if (!wrapper->getContents())
	{
		qWarning("Empty top level wrapper");
		return QVariantMap();
	}

	QVariantMap result;
	result["geometry"] = wrapper->getWidget()->saveGeometry().toBase64();
	result["name"] = wrapper->getWidget()->objectName();

	QSplitter* splitter = qobject_cast<QSplitter*>(wrapper->getContents());
	IToolWindowArea* area = qobject_cast<IToolWindowArea*>(wrapper->getContents());
	if (splitter)
	{
		result["splitter"] = saveSplitterState(splitter);
		return result;
	}
	else if (area)
	{
		result["area"] = area->saveState();
		return result;
	}
	else
	{
		qWarning("Couldn't find valid child widget");
		return QVariantMap();
	}
}

IToolWindowWrapper* QToolWindowManager::restoreWrapperState(const QVariantMap &data, int stateFormat, IToolWindowWrapper* wrapper)
{
	QWidget* newContents = nullptr;
	
	if (wrapper && data.contains("name"))
	{
		wrapper->getWidget()->setObjectName(data["name"].toString());
	}

	if (data.contains("splitter"))
	{
		newContents = restoreSplitterState(data["splitter"].toMap(), stateFormat);
	}
	else if (data.contains("area"))
	{
		QTWMWrapperAreaType areaType = watTabs;
		if (data["area"].toMap().contains("type") && data["area"].toMap()["type"].toString() == "rollup")
		{
			areaType = watRollups;
		}
		IToolWindowArea* area = createArea(areaType);
		area->restoreState(data["area"].toMap(), stateFormat);
		if (area->count() > 0)
		{
			newContents = area->getWidget();
		}
		else
		{
			area->deleteLater();
		}
	}

	if (!wrapper)
	{
		if (newContents)
		{
			wrapper = createWrapper();
			if (data.contains("name"))
			{
				wrapper->getWidget()->setObjectName(data["name"].toString());
			}
		}
		else
		{
			// No contents, so don't bother making a new wrapper, just return.
			return nullptr;
		}
	}

	wrapper->setContents(newContents);

	switch (stateFormat)
	{
	case 1:
		if (data.contains("geometry"))
		{
			if (!wrapper->getWidget()->restoreGeometry(data["geometry"].toByteArray()))
			{
				qWarning("Failed to restore wrapper geometry");
			}
		}
		break;
	case 2:
		if (data.contains("geometry"))
		{
			if (!wrapper->getWidget()->restoreGeometry(QByteArray::fromBase64(data["geometry"].toByteArray())))
			{
				qWarning("Failed to restore wrapper geometry");
			}
		}
		break;
	default:
		qWarning("Unknown state format");
		break;
	}

	if (data.contains("geometry"))
	{
		// Adjust position of frameless non-maximized windows since Qt will otherwise adjust for a non-existant frame (as of Qt 5.6)
		if (wrapper->getWidget()->windowFlags().testFlag(Qt::FramelessWindowHint) && !wrapper->getWidget()->windowState().testFlag(Qt::WindowMaximized))
		{
			QWidget* w = wrapper->getWidget();
			QRect r = w->geometry();
			r.translate(QPoint(w->x() - r.x(), w->y() - r.y()));
			w->setGeometry(r);
		}
		// If geometry was not saved, do not show.
		wrapper->getWidget()->show();
	}
	return wrapper;
}

QVariantMap QToolWindowManager::saveSplitterState(QSplitter *splitter) 
{
	QVariantMap result;
	result["state"] = splitter->saveState().toBase64();
	result["type"] = "splitter";
	QVariantList items;
	for(int i = 0; i < splitter->count(); i++)
	{
		QWidget* item = splitter->widget(i);
		QVariantMap itemValue;
		IToolWindowArea* area = qobject_cast<IToolWindowArea*>(item);
		if (area)
		{
			itemValue = area->saveState();
		}
		else
		{
			QSplitter* childSplitter = qobject_cast<QSplitter*>(item);
			if (childSplitter)
			{
				itemValue = saveSplitterState(childSplitter);
			}
			else
			{
				qWarning("Unknown splitter item");
			}
		}
		items << itemValue;
	}
	result["items"] = items;
	return result;
}

QSplitter *QToolWindowManager::restoreSplitterState(const QVariantMap &data, int stateFormat)
{
	if (data["items"].toList().count() < 2) {
		qWarning("Invalid splitter encountered");
		if (data["items"].toList().count() == 0)
		{
			return nullptr;
		}
	}
	QSplitter* splitter = createSplitter();

	foreach(QVariant itemData, data["items"].toList())
	{
		QVariantMap itemValue = itemData.toMap();
		QString itemType = itemValue["type"].toString();
		if (itemType == "splitter")
		{
			QWidget* w = restoreSplitterState(itemValue, stateFormat);
			if (w)
			{
				splitter->addWidget(w);
			}
		}
		else if (itemType == "area")
		{
			IToolWindowArea* area = createArea();
			area->restoreState(itemValue, stateFormat);
			splitter->addWidget(area->getWidget());
		}
		else if (itemType == "rollup")
		{
			IToolWindowArea* area = createArea(watRollups);
			area->restoreState(itemValue, stateFormat);
			splitter->addWidget(area->getWidget());
		}
		else
		{
			qWarning("Unknown item type");
		}
	}
	switch (stateFormat)
	{
	case 1:
		if (data.contains("state"))
		{
			if (!splitter->restoreState(data["state"].toByteArray()))
			{
				qWarning("Failed to restore splitter state");
			}
		}
		break;
	case 2:
		if (data.contains("state"))
		{
			if (!splitter->restoreState(QByteArray::fromBase64(data["state"].toByteArray())))
			{
				QByteArray b = data["state"].toByteArray();
				qWarning("Failed to restore splitter state");
			}
		}
		break;
	default:
		qWarning("Unknown state format");
		break;
	}
	return splitter;
}

void QToolWindowManager::resizeSplitter(QWidget* widget, QList<int> sizes)
{
	QSplitter* s = qobject_cast<QSplitter*>(widget);
	if (!s)
	{
		s = findClosestParent<QSplitter*>(widget);
	}
	if (!s)
	{
		qWarning("Could not find a matching splitter!");
		return;
	}

	int scaleFactor = s->orientation() == Qt::Horizontal ? s->width() : s->height();
	for (auto it = sizes.begin(); it != sizes.end(); it++)
	{
		*it *= scaleFactor;
	}
	s->setSizes(sizes);
}

QString QToolWindowManager::textForPosition(QToolWindowAreaReference reference)
{
	static const char* texts[10] = {
		"Place at top of window",
		"Place at bottom of window",
		"Place on left side of window",
		"Place on right side of window",
		"Split horizontally, place above",
		"Split horizontally, place below",
		"Split vertically, place left",
		"Split vertically, place right",
		"Add to tab list",
		""
	};
	QString s;
	return texts[reference];
}

IToolWindowDragHandler* QToolWindowManager::createDragHandler()
{
	if (m_dragHandler)
		delete m_dragHandler;
	return m_factory->createDragHandler(this);
}

void QToolWindowManager::insertToToolTypes(QWidget* tool, QTWMToolType type)
{
	if (!m_toolWindowsTypes.contains(tool))
		m_toolWindowsTypes.insert(tool, type);
}

void QToolWindowManager::raiseCurrentArea()
{
	if (m_lastArea)
	{
		QWidget* w = m_lastArea->getWidget();
		while (w->parentWidget() && ! w->isWindow())
		{
			w = w->parentWidget();
		}
		w->raise();
		updateDragPosition();
	}
	m_raiseTimer->stop();
}

void QToolWindowManager::clear()
{
	releaseToolWindows(toolWindows(), true);
	// make sure pending deletions are processed, so two unique tools don't co-exist
	qApp->processEvents();
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	if (!m_areas.isEmpty())
	{
		m_lastArea = m_areas.first();
	}
	else
	{
		m_lastArea = nullptr;
	}
}

void QToolWindowManager::bringAllToFront()
{
#if defined(WIN32) || defined(WIN64)
	QWidget* w;
	QSet<HWND> handles;
	HWND h;
	QList<QWidget*> list = QApplication::topLevelWidgets();
	foreach (w, list)
	{
		if (w && !w->windowState().testFlag(Qt::WindowMinimized) && (qobject_cast<IToolWindowWrapper*>(w) || qobject_cast<QCustomWindowFrame*>(w)))
		{
			h = (HWND)w->winId();
			handles.insert(h);
		}
	}
	h = GetTopWindow(NULL);
	QList<HWND> orderedHandles;
	while (h)
	{
		if (handles.contains(h))
		{
			orderedHandles.push_front(h);
		}
		h = GetNextWindow(h, GW_HWNDNEXT);
	}
	foreach(h, orderedHandles)
	{
		BringWindowToTop(h);
	}
#endif
}

void QToolWindowManager::bringToFront(QWidget* toolWindow)
{
	IToolWindowArea* area = areaOf(toolWindow);
	if (!area)
		return;
	while (area->indexOf(toolWindow) == -1)
	{
		toolWindow = toolWindow->parentWidget();		
	}
	area->setCurrentWidget(toolWindow);
	area->getWidget()->window()->raise();
	toolWindow->setFocus();
}

QString QToolWindowManager::getToolPath(QWidget* toolWindow) const
{
	QWidget* w = toolWindow;
	QString result;
	while (QWidget* pw = w->parentWidget())
	{
		if (QSplitter* s = qobject_cast<QSplitter*>(pw))
		{
			result = result.prepend(QString::asprintf("/%c/%d", (s->orientation() == Qt::Horizontal ? 'h' : 'v'), s->indexOf(w)));
		}
		else if (qobject_cast<IToolWindowWrapper*>(pw))
		{
			result = result.prepend(toolWindow->window()->objectName());
			break;
		}
		w = pw;
	}
	return result;
}

QToolWindowAreaTarget QToolWindowManager::targetFromPath(const QString& toolPath) const
{
	foreach(IToolWindowArea* w, m_areas)
	{
		if (w->count() > 0 && getToolPath(w->widget(0)) == toolPath)
		{
			return QToolWindowAreaTarget(w, QToolWindowAreaReference::Combine);
		}
	}
	return QToolWindowAreaTarget(QToolWindowAreaReference::Floating);
}

IToolWindowArea *QToolWindowManager::areaOf(QWidget *toolWindow)
{
	return findClosestParent<IToolWindowArea*>(toolWindow);
}

void QToolWindowManager::SwapAreaType(IToolWindowArea* oldArea, QTWMWrapperAreaType areaType)
{
	QSplitter* parentSplitter = nullptr;
	IToolWindowWrapper* targetWrapper = findClosestParent<IToolWindowWrapper*>(oldArea->getWidget());
	parentSplitter = qobject_cast<QSplitter*>(oldArea->parentWidget());
	IToolWindowArea* newArea = createArea(areaType);

	if (!parentSplitter && !targetWrapper)
	{
		qWarning("Could not determine area parent");
		return ;
	}
	
	newArea->addToolWindows(oldArea->toolWindows());
	if (parentSplitter)
	{
		int targetIndex = parentSplitter->indexOf(oldArea->getWidget());
		parentSplitter->insertWidget(targetIndex, newArea->getWidget());		
	}
	else
	{
		targetWrapper->setContents(newArea->getWidget());
	}
	if (m_lastArea == oldArea)
		m_lastArea = newArea;
	int oldAreaIndex = m_areas.indexOf(oldArea);
	m_areas.removeAt(oldAreaIndex);
	m_areas.insert(oldAreaIndex, newArea);
	oldArea->getWidget()->setParent(nullptr);
	newArea->adjustDragVisuals();
	
}

IToolWindowArea* QToolWindowManagerClassFactory::createArea(QToolWindowManager* manager, QWidget *parent /*= 0*/, QTWMWrapperAreaType areaType)
{
	if (manager->config().value(QTWM_SUPPORT_SIMPLE_TOOLS, false).toBool() && areaType == watRollups)
		return new QToolWindowRollupBarArea(manager, parent);
	else
		return new QToolWindowArea(manager, parent);
}

IToolWindowWrapper* QToolWindowManagerClassFactory::createWrapper(QToolWindowManager* manager)
{
	return new QToolWindowWrapper(manager, Qt::Tool);
}

IToolWindowDragHandler* QToolWindowManagerClassFactory::createDragHandler(QToolWindowManager* manager)
{
	return new QToolWindowDragHandlerDropTargets(manager);
}

QSplitter* QToolWindowManagerClassFactory::createSplitter(QToolWindowManager* manager)
{
	QSplitter* splitter;

	if (manager->config().value(QTWM_PRESERVE_SPLITTER_SIZES, true).toBool())
	{
		splitter = new QSizePreservingSplitter();
	}
	else
	{
		splitter = new QSplitter();
	}
	splitter->setChildrenCollapsible(false);
	return splitter;
}

QToolWindowManager::QTWMNotifyLock::QTWMNotifyLock(QToolWindowManager* parent, bool allowNotify) : m_parent(parent), m_notify(allowNotify)
{
	m_parent->suspendLayoutNotifications();
}

QToolWindowManager::QTWMNotifyLock::QTWMNotifyLock(const QTWMNotifyLock& other) : m_parent(other.m_parent), m_notify(other.m_notify)
{
	m_parent->suspendLayoutNotifications();
}

QToolWindowManager::QTWMNotifyLock::QTWMNotifyLock(QTWMNotifyLock&& other) : m_parent(other.m_parent), m_notify(other.m_notify)
{
}

QToolWindowManager::QTWMNotifyLock::~QTWMNotifyLock()
{
	m_parent->resumeLayoutNotifications();
	if (m_notify)
	{
		m_parent->notifyLayoutChange();
	}
}

