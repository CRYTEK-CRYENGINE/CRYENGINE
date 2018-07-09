// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <QPoint>
#include <QString>
#include <QLabel>
#include "EditorCommonAPI.h"

//! Can be used as a base class for custom tooltips,
//! as it implements tooltip showing, placing and moving logic
class EDITOR_COMMON_API QTrackingTooltip : public QFrame
{
	Q_OBJECT;
public:
	QTrackingTooltip(QWidget* parent = nullptr);

	virtual ~QTrackingTooltip(void);
	void Show();
	bool SetText(const QString& str);
	bool SetPixmap(const QPixmap& pixmap);
	void SetPos(const QPoint& p);
	void SetPosCursorOffset(const QPoint& p = QPoint());

	QPoint GetCursorOffset() const { return m_cursorOffset; }

	void SetAutoHide(bool autoHide) { m_autoHide = autoHide; }

	//! Shows a tooltip at the given position.
	//! \param str Text to display in the tooltip. If empty, the tooltip will be hidden.
	//! \param p Screen coordinate for the tooltip.
	static void ShowTextTooltip(const QString& str, const QPoint& p);

	static void ShowTrackingTooltip(QSharedPointer<QTrackingTooltip>& tooltip, const QPoint& cursorOffset = QPoint());

	//! Hide tooltip only if tooltip is being displayed
	static void HideTooltip(QTrackingTooltip* tooltip);

	//! Hide tooltip regardless of what is displayed
	static void HideTooltip();

protected:
	QPoint AdjustPosition(QPoint p) const;
	int    GetToolTipScreen() const;
	bool   eventFilter(QObject* object, QEvent* event) override;
	void   resizeEvent(QResizeEvent* resizeEvent) override;

	bool   m_isTracking;
	bool   m_autoHide;
	QPoint m_cursorOffset;
	static QSharedPointer<QTrackingTooltip> m_instance;
};

