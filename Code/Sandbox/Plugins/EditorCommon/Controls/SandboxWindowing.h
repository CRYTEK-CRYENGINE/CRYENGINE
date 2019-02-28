// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "QToolWindowManager/QToolWindowManager.h"
#include "QToolWindowManager/QCustomWindowFrame.h"
#include "QToolWindowManager/QToolWindowCustomWrapper.h"

//Extra icon keys for the TWM.
#define SANDBOX_WRAPPER_MINIMIZE_ICON "sandboxMinimizeIcon"
#define SANDBOX_WRAPPER_MAXIMIZE_ICON "sandboxMaximizeIcon"
#define SANDBOX_WRAPPER_RESTORE_ICON "sandboxRestoreIcon"
#define SANDBOX_WRAPPER_CLOSE_ICON "sandboxWindowCloseIcon"

// QSplitterHandle class that also lets us know when we have started resizing the layout
class EDITOR_COMMON_API QNotifierSplitterHandle : public QSplitterHandle
{
	Q_OBJECT;
public:
	QNotifierSplitterHandle(Qt::Orientation orientation, QSplitter* parent);
	void mousePressEvent(QMouseEvent* e);
	void mouseReleaseEvent(QMouseEvent* e);
	void mouseMoveEvent(QMouseEvent *e);
};

// QSplitter class that creates QNotifierSplitterHandle
class EDITOR_COMMON_API QNotifierSplitter : public QSizePreservingSplitter
{
	Q_OBJECT;
public:
	QNotifierSplitter(QWidget* parent = 0);

	QSplitterHandle* createHandle();
};


class EDITOR_COMMON_API QSandboxWindow : public QCustomWindowFrame
{
public:
	QSandboxWindow(QToolWindowManager* manager) : QCustomWindowFrame(), m_manager(manager), m_config() {};
	QSandboxWindow(QVariantMap config) : QCustomWindowFrame(), m_manager(nullptr), m_config(config) {};
	virtual void ensureTitleBar();
	virtual void keyPressEvent(QKeyEvent* keyEvent) override;
	virtual bool eventFilter(QObject *, QEvent *) override;
	static QSandboxWindow* QSandboxWindow::wrapWidget(QWidget* w, QToolWindowManager* manager);
private:

	QToolWindowManager* m_manager;
	QVariantMap m_config;
};

class EDITOR_COMMON_API QSandboxWrapper : public QToolWindowCustomWrapper
{
public:
	QSandboxWrapper(QToolWindowManager* manager) : QToolWindowCustomWrapper(manager) {};
	virtual void ensureTitleBar();
	virtual void keyPressEvent(QKeyEvent* keyEvent) override;
	virtual bool eventFilter(QObject *, QEvent *) override;
};

class EDITOR_COMMON_API QSandboxTitleBar : public QCustomTitleBar
{
	Q_OBJECT;
public:
	QSandboxTitleBar(QWidget* parent, const QVariantMap& config);

public slots:
	void updateWindowStateButtons() override;

private:
	QVariantMap m_config;
};

