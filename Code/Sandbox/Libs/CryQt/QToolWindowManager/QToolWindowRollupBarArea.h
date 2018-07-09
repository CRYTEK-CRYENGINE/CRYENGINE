// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <QPointer>
#include "QRollupBar.h"
#include "IToolWindowArea.h"

class QToolWindowManager;
class QLabel;

class QTOOLWINDOWMANAGER_EXPORT QToolWindowRollupBarArea : public QRollupBar, public IToolWindowArea
{
	Q_OBJECT;
	Q_INTERFACES(IToolWindowArea);

public:
	explicit QToolWindowRollupBarArea(QToolWindowManager* manager, QWidget *parent = 0);
	virtual ~QToolWindowRollupBarArea();

	void addToolWindow(QWidget* toolWindow, int index = -1) Q_DECL_OVERRIDE;
	void addToolWindows(const QList<QWidget*>& toolWindows, int index = -1) Q_DECL_OVERRIDE;

	void removeToolWindow(QWidget* toolWindow) Q_DECL_OVERRIDE;

	QList<QWidget*> toolWindows() Q_DECL_OVERRIDE;

	QVariantMap saveState() Q_DECL_OVERRIDE;
	void restoreState(const QVariantMap &data, int stateFormat) Q_DECL_OVERRIDE;
	void adjustDragVisuals() Q_DECL_OVERRIDE;

	QWidget* getWidget() Q_DECL_OVERRIDE { return this; }

	virtual bool switchAutoHide(bool newValue);

	const QPalette& palette() const Q_DECL_OVERRIDE { return QRollupBar::palette(); };
	void clear() Q_DECL_OVERRIDE { QRollupBar::clear(); };
	QRect rect() const Q_DECL_OVERRIDE { return QRollupBar::rect(); };
	QSize size() const Q_DECL_OVERRIDE { return QRollupBar::size(); };
	int count() const Q_DECL_OVERRIDE { return QRollupBar::count(); };
	QWidget* widget(int index) const Q_DECL_OVERRIDE { return QRollupBar::widget(index); };
	void deleteLater() Q_DECL_OVERRIDE { QRollupBar::deleteLater(); };
	int width() const Q_DECL_OVERRIDE { return QRollupBar::width(); };
	int height() const Q_DECL_OVERRIDE { return QRollupBar::height(); };
	const QRect geometry() const Q_DECL_OVERRIDE { return QRollupBar::geometry(); };
	void hide() Q_DECL_OVERRIDE { QRollupBar::hide(); };
	QObject* parent() const Q_DECL_OVERRIDE { return QRollupBar::parent(); };
	void setParent(QWidget* parent) Q_DECL_OVERRIDE { QRollupBar::setParent(parent); };
	int indexOf(QWidget* w) const Q_DECL_OVERRIDE;
	QWidget* parentWidget() const Q_DECL_OVERRIDE { return QRollupBar::parentWidget(); };
	QPoint mapFromGlobal(const QPoint & pos) const Q_DECL_OVERRIDE { return QRollupBar::mapFromGlobal(pos); };
	QPoint mapToGlobal(const QPoint & pos) const Q_DECL_OVERRIDE { return QRollupBar::mapToGlobal(pos); };
	void setCurrentWidget(QWidget* w) Q_DECL_OVERRIDE;

	QPoint mapCombineDropAreaFromGlobal(const QPoint & pos) const Q_DECL_OVERRIDE;
	QRect combineAreaRect() const Q_DECL_OVERRIDE;
	QRect combineSubWidgetRect(int index) const Q_DECL_OVERRIDE;
	int subWidgetAt(const QPoint& pos) const Q_DECL_OVERRIDE;
	virtual QTWMWrapperAreaType areaType() const { return QTWMWrapperAreaType::watRollups; };
protected:
	virtual bool eventFilter(QObject *o, QEvent *ev) Q_DECL_OVERRIDE;
protected Q_SLOTS:
	void closeRollup(int index);
	void mouseReleaseEvent(QMouseEvent * e);
	void swapToRollup();
private:
	void setDraggable(bool draggable);
	QPointer<QToolWindowManager> m_manager;
	QLabel* m_pTopWidget;
	QPoint m_areaDragStart;
	bool m_areaDraggable;
	bool m_tabDragCanStart;
	bool m_areaDragCanStart;
};

