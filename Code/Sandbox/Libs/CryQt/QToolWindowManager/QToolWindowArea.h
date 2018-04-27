// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QTabWidget>
#include <QTabBar>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QPointer>

#include "QToolWindowManagerCommon.h"
#include "QToolWindowTabBar.h"
#include "IToolWindowArea.h"

class QToolWindowManager;
class QToolWindowTabBar;

class QTOOLWINDOWMANAGER_EXPORT QToolWindowSingleTabAreaFrame: public QFrame
{
	Q_OBJECT;
public:
	QToolWindowSingleTabAreaFrame(QToolWindowManager* manager, QWidget* parent);
	virtual ~QToolWindowSingleTabAreaFrame() {};
	QWidget* contents() { return m_contents; }
	void setContents(QWidget*);

	void setCloseButtonVisible(bool bVisible);

private slots:
	void closeWidget();
private:
	virtual void closeEvent(QCloseEvent* e) Q_DECL_OVERRIDE;
	virtual void changeEvent(QEvent *ev) Q_DECL_OVERRIDE;
	QIcon getCloseButtonIcon() const;
	QGridLayout* m_layout;
	QToolWindowManager* m_manager;
	QPushButton* m_closeButton;
	QLabel* m_caption;
	QWidget* m_contents;
	friend class QToolWindowArea;
};

class QTOOLWINDOWMANAGER_EXPORT QToolWindowArea : public QTabWidget, public IToolWindowArea
{
	Q_OBJECT;
	Q_INTERFACES(IToolWindowArea);
public:
	explicit QToolWindowArea(QToolWindowManager* manager, QWidget *parent = 0);
	virtual ~QToolWindowArea();

	void addToolWindow(QWidget* toolWindow, int index = -1 ) Q_DECL_OVERRIDE;
	void addToolWindows(const QList<QWidget*>& toolWindows, int index = -1 ) Q_DECL_OVERRIDE;

	void removeToolWindow(QWidget* toolWindow) Q_DECL_OVERRIDE;

	QList<QWidget*> toolWindows() Q_DECL_OVERRIDE;

	QVariantMap saveState() Q_DECL_OVERRIDE;
	void restoreState(const QVariantMap &data, int stateFormat) Q_DECL_OVERRIDE;
	void adjustDragVisuals() Q_DECL_OVERRIDE;

	QWidget* getWidget() Q_DECL_OVERRIDE { return this; }

	virtual bool switchAutoHide(bool newValue);

	QToolWindowTabBar* tabBar() const { return (QToolWindowTabBar*)QTabWidget::tabBar(); }

	//QTabWidget members
	const QPalette& palette() const Q_DECL_OVERRIDE {return QTabWidget::palette();};
	void clear() Q_DECL_OVERRIDE {QTabWidget::clear();};
	QRect rect() const Q_DECL_OVERRIDE {return QTabWidget::rect();};
	QSize size() const Q_DECL_OVERRIDE {return QTabWidget::size();};
	int count() const Q_DECL_OVERRIDE {return QTabWidget::count();};
	QWidget* widget(int index) const Q_DECL_OVERRIDE {return QTabWidget::widget(index);};
	void deleteLater() Q_DECL_OVERRIDE {QTabWidget::deleteLater();};
	int width() const Q_DECL_OVERRIDE {return QTabWidget::width();};
	int height() const Q_DECL_OVERRIDE {return QTabWidget::height();};
	const QRect geometry() const Q_DECL_OVERRIDE {return QTabWidget::geometry();};
	void hide() Q_DECL_OVERRIDE {QTabWidget::hide();};
	QObject* parent() const Q_DECL_OVERRIDE {return QTabWidget::parent();};
	void setParent(QWidget* parent) Q_DECL_OVERRIDE {QTabWidget::setParent(parent);};
	int indexOf(QWidget* w) const Q_DECL_OVERRIDE;
	QWidget* parentWidget() const Q_DECL_OVERRIDE {return QTabWidget::parentWidget();};
	QPoint mapFromGlobal(const QPoint & pos) const Q_DECL_OVERRIDE {return QTabWidget::mapFromGlobal(pos);};
	QPoint mapToGlobal(const QPoint & pos) const Q_DECL_OVERRIDE {return QTabWidget::mapToGlobal(pos);};
	void setCurrentWidget(QWidget* w) Q_DECL_OVERRIDE;

	//QTabBar wrappers
	QPoint mapCombineDropAreaFromGlobal(const QPoint & pos) const Q_DECL_OVERRIDE {return tabBar()->mapFromGlobal(pos);}
	QRect combineAreaRect() const Q_DECL_OVERRIDE;
	QRect combineSubWidgetRect(int index) const Q_DECL_OVERRIDE {return tabBar()->tabRect(index);}
	int subWidgetAt(const QPoint& pos) const Q_DECL_OVERRIDE {return tabBar()->tabAt(pos);}
	virtual QTWMWrapperAreaType areaType() const Q_DECL_OVERRIDE { return QTWMWrapperAreaType::watTabs; };
protected:
	virtual bool eventFilter(QObject *o, QEvent *ev) Q_DECL_OVERRIDE;

protected Q_SLOTS:
	void tabCloseButtonClicked();
	void closeTab(int index);
	void showContextMenu(const QPoint &point);
	void swapToRollup();

private:
	QPushButton* createCloseButton();
	bool shouldShowSingleTabFrame();
	virtual void mouseReleaseEvent(QMouseEvent * e);
	QPointer<QToolWindowManager> m_manager;
	bool m_tabDragCanStart;
	bool m_areaDragCanStart;
	QToolWindowSingleTabAreaFrame* m_tabFrame;
	bool m_useCustomTabCloseButton;
	friend class QToolWindowSingleTabAreaFrame;
};
