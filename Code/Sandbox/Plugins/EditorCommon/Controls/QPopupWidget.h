// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QFrame>

#include "EditorCommonAPI.h"

class QVBoxLayout;
class QEventLoop;

class EDITOR_COMMON_API QPopupWidget : public QFrame
{
	Q_OBJECT

public:

	enum Origin
	{
		Unknown     = -1,
		Fixed       = 0x0,
		Top         = 0x1,
		Left        = 0x2,
		Right       = 0x4,
		Bottom      = 0x8,

		TopLeft     = Top | Left,
		TopRight    = Top | Right,
		BottomLeft  = Bottom | Left,
		BottomRight = Bottom | Right
	};

	QPopupWidget(const QString& name, QWidget* pContent, QSize sizeHint = QSize(400, 300), bool bFixedSize = false, QWidget* pParent = nullptr);

	virtual QSize sizeHint() const override             { return m_sizeHint; }
	void          SetFocusShareWidget(QWidget* pWidget) { m_pFocusShareWidget = pWidget; }

	// If origin is left as unknown then the widget will be in charge of what the origin should be
	// based off where on the screen the widget is about to spawn and the content size
	void   ShowAt(const QPoint& globalPos, Origin origin = Unknown);
	void   SetContent(QWidget* pContent);
	void   SetFixedSize(bool bFixedSize) { bFixedSize ? m_resizeFlags = Fixed : m_resizeFlags = ~m_origin & (Origin::TopLeft | Origin::BottomRight); }
	bool   IsFixedSize()                 { return m_resizeFlags == 0; }
	Origin GetOrigin() const             { return static_cast<Origin>(m_origin); }

Q_SIGNALS:
	void SignalHide();

protected:
	void         OnFocusChanged(QWidget* pOld, QWidget* pNew);
	void         RecursiveEnableMouseTracking(QWidget* pWidget);
	QPoint       GetSpawnPosition(const QPoint& globalPos);

	virtual void mouseMoveEvent(QMouseEvent* pEvent) override;
	virtual void mousePressEvent(QMouseEvent* pEvent) override;
	virtual void mouseReleaseEvent(QMouseEvent* pEvent) override;
	virtual void showEvent(QShowEvent* pEvent) override;
	virtual bool eventFilter(QObject* pWatched, QEvent* pEvent) override;
	virtual void hideEvent(QHideEvent* pEvent) override;

protected:
	QWidget*     m_pContent;
	QWidget*     m_pFocusShareWidget; // Our popup won't be hidden if this widget gains focus
	QVBoxLayout* m_pInsetLayout;
	QPoint       m_mousePressPos;
	QPoint       m_resizeConstraint;
	QRect        m_initialGeometry;
	QSize        m_sizeHint;

	// This should ideally be based off the content's rect rather than a predefined offset
	// but there were issues with content widgets not following the stylesheet's rules
	int m_borderOffset;
	int m_resizeFlags;
	int m_origin;
};

