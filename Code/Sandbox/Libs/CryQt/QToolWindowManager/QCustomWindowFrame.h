// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "QToolWindowManagerCommon.h"
#include <QFrame>

class QBoxLayout;
class QCustomWindowFrame;
class QLabel;
class QPushButton;
class QToolButton;

class QTOOLWINDOWMANAGER_EXPORT QCustomTitleBar : public QFrame
{
	Q_OBJECT;
public:
	QCustomTitleBar(QWidget* parent);

public:
	virtual void updateWindowStateButtons();
	void         setActive(bool active);

protected slots:
	virtual void toggleMaximizedParent();
	void         showSystemMenu(QPoint p);
	void         onIconChange();
	void         onFrameContentsChanged(QWidget* newContents);
public slots:
	void         onBeginDrag();

protected:
	virtual void mousePressEvent(QMouseEvent*) override;
	virtual void mouseReleaseEvent(QMouseEvent*) override;
	virtual bool eventFilter(QObject*, QEvent*) override;
	virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

#if (defined(_WIN32) || defined(_WIN64))
	bool winEvent(MSG* msg, long* result);
#endif

protected:
	bool         m_dragging;

	QLabel*      m_caption;
	QToolButton* m_sysMenuButton;
	QToolButton* m_minimizeButton;
	QToolButton* m_maximizeButton;
	QToolButton* m_closeButton;

	friend class QCustomWindowFrame;
};

class QTOOLWINDOWMANAGER_EXPORT QCustomWindowFrame : public QFrame
{
	Q_OBJECT
	Q_PROPERTY(int resizeMargin READ GetResizeMargin WRITE SetResizeMargin DESIGNABLE true)

public:
	static QCustomWindowFrame* wrapWidget(QWidget* w);

public:
	QCustomWindowFrame();
	virtual ~QCustomWindowFrame();

	virtual void ensureTitleBar();

protected:
	virtual void internalSetContents(QWidget* widget, bool useContentsGeometry = true);
	virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

#if (defined(_WIN32) || defined(_WIN64))
	bool         winEvent(MSG* msg, long* result);
#endif
	virtual bool event(QEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual void changeEvent(QEvent*) override;
	virtual bool eventFilter(QObject*, QEvent*) override;
	virtual void mouseReleaseEvent(QMouseEvent*) override;

signals:
	void contentsChanged(QWidget* newContents);

protected slots:
	void                    nudgeWindow();
	virtual Qt::WindowFlags calcFrameWindowFlags();
	void                    updateWindowFlags();
	void                    onIconChange();

private:
	int GetResizeMargin() const { return m_resizeMargin; }
	void SetResizeMargin(int resizeMargin) { m_resizeMargin = resizeMargin; }

protected:

	QWidget*         m_contents;
	QCustomTitleBar* m_titleBar;
	QBoxLayout*      m_layout;
	int              m_resizeMargin;

#if (defined(_WIN32) || defined(_WIN64))
	// DWM library
	/*HMODULE*/ void* m_dwm;
	// Pointers to DWM functions
	void*             dwmExtendFrameIntoClientArea;
#endif
};
