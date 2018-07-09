// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ViewportPane_h__
#define __ViewportPane_h__
#pragma once

#include "EditorCommonAPI.h"
#include "Viewport.h"

#include <QWidget>
#include <QEvent>
#include <QLayout>

class CLevelEditorViewport;
class QViewportHeader;

//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API QViewportWidget : public QWidget
{
	Q_OBJECT

public:
	QViewportWidget(CViewport* viewport);

	//////////////////////////////////////////////////////////////////////////
	virtual void keyPressEvent(QKeyEvent*) override;
	virtual void keyReleaseEvent(QKeyEvent*) override;
	virtual void mousePressEvent(QMouseEvent*) override;
	virtual void mouseReleaseEvent(QMouseEvent*) override;
	virtual void mouseDoubleClickEvent(QMouseEvent*) override;
	virtual void mouseMoveEvent(QMouseEvent*) override;
	virtual void wheelEvent(QWheelEvent*) override;

	virtual void enterEvent(QEvent*) override;
	virtual void leaveEvent(QEvent*) override;
	//////////////////////////////////////////////////////////////////////////
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void paintEvent(QPaintEvent*) override;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Focus related
	virtual void focusInEvent(QFocusEvent*) override;
	virtual void focusOutEvent(QFocusEvent*) override;
	bool focusNextPrevChild(bool next) override;
	//////////////////////////////////////////////////////////////////////////
	//Drag & drop
	virtual void          dragEnterEvent(QDragEnterEvent*) override;
	virtual void          dragMoveEvent(QDragMoveEvent*) override;
	virtual void          dragLeaveEvent(QDragLeaveEvent*) override;
	virtual void          dropEvent(QDropEvent*) override;
	virtual QPaintEngine* paintEngine() const override { return nullptr; }
	virtual void          customEvent(QEvent *) override;

	CViewport*            GetViewport()                { return m_viewport.get(); }

Q_SIGNALS:
	void resolutionChanged();

private:
	_smart_ptr<CViewport> m_viewport;
};

// Custom layout that handles layout in a perspective correct manner
class QAspectLayout : public QLayout
{
public:
	QAspectLayout();
	~QAspectLayout();

	void         addItem(QLayoutItem* item);
	QSize        sizeHint() const;
	QSize        minimumSize() const;
	int          count() const;
	QLayoutItem* itemAt(int i) const;
	QLayoutItem* takeAt(int i);

	void         setGeometry(const QRect& rect);

private:
	QLayoutItem* m_child;
};

class QAspectRatioWidget : public QWidget
{
	Q_OBJECT
public:
	QAspectRatioWidget(QViewportWidget* pViewport);
	void paintEvent(QPaintEvent* pe);

public Q_SLOTS:
	void updateAspect();

private:
	QAspectLayout* m_layout;
};

//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API QViewportPane : public QWidget
{
public:
	QViewportPane(CViewport* viewport, QWidget* headerWidget);

private:
	friend class QViewportWidget;
	QWidget* m_headerWidget;
	QViewportWidget* m_viewWidget;

private:
	// This widget owns viewport
	_smart_ptr<CViewport> m_viewport;
};

#endif //__ViewportPane_h__

