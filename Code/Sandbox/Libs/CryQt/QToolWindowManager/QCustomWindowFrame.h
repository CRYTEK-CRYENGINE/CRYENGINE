// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "QToolWindowManagerCommon.h"
#include <QFrame>

class QPushButton;
class QToolButton;
class QGridLayout;
class QLabel;

class QCustomWindowFrame;

class QTOOLWINDOWMANAGER_EXPORT QCustomTitleBar : public QFrame
{
	Q_OBJECT;
public:
	QCustomTitleBar(QWidget* parent);
	virtual ~QCustomTitleBar();

public:
	virtual void updateWindowStateButtons();
	void setActive(bool active);
	
protected slots:
	virtual void toggleMaximizedParent();
	void showSystemMenu(QPoint p);
	void onIconChange();
	void onFrameContentsChanged(QWidget* newContents);
public slots:
	void onBeginDrag();

protected:
	virtual void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
	virtual void mouseReleaseEvent(QMouseEvent*) Q_DECL_OVERRIDE;
	virtual bool eventFilter(QObject *, QEvent *) Q_DECL_OVERRIDE;

#if QT_VERSION >= 0x050000
	virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;
#endif
#if (defined(_WIN32) || defined(_WIN64))
	bool winEvent(MSG *msg, long *result);
#endif

protected:
	bool m_dragging;

	QLabel* m_caption;
	QToolButton* m_sysMenuButton;
	QToolButton* m_minimizeButton;
	QToolButton* m_maximizeButton;
	QToolButton* m_closeButton;

	friend class QCustomWindowFrame;
};

class QTOOLWINDOWMANAGER_EXPORT QCustomWindowFrame : public QFrame
{
public:
	static QCustomWindowFrame* wrapWidget(QWidget* w);

	Q_OBJECT;
public:
	QCustomWindowFrame();
	virtual ~QCustomWindowFrame();

	virtual void ensureTitleBar();

protected:
	virtual void internalSetContents(QWidget* widget, bool useContentsGeometry = true);

#if QT_VERSION >= 0x050000
	virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;
#endif
#if (defined(_WIN32) || defined(_WIN64))
	bool winEvent(MSG *msg, long *result);
#endif
	virtual bool event(QEvent*) Q_DECL_OVERRIDE;
	virtual void closeEvent(QCloseEvent *) Q_DECL_OVERRIDE;
	virtual void changeEvent(QEvent *) Q_DECL_OVERRIDE;
	virtual bool eventFilter(QObject *, QEvent *) Q_DECL_OVERRIDE;
	virtual void mouseReleaseEvent(QMouseEvent*) Q_DECL_OVERRIDE;

signals:
	void contentsChanged(QWidget* newContents);

protected slots:
	void nudgeWindow();
	virtual Qt::WindowFlags calcFrameWindowFlags();
	void updateWindowFlags();
	void onIconChange();

protected:

	QWidget* m_contents;
	QCustomTitleBar* m_titleBar;
	QGridLayout* m_grid;

#if (defined(_WIN32) || defined(_WIN64))
	// DWM library
	/*HMODULE*/ void* m_dwm;
	// Pointers to DWM functions
	void* dwmExtendFrameIntoClientArea;
#endif
};
