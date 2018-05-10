// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <QAbstractButton>
#include <QWidget>
#include <QPainter>
#include <QDockWidget>
#include <QBoxLayout>

class EDITOR_COMMON_API CDockTitleBarWidget : public QWidget
{
	Q_OBJECT
public:

	CDockTitleBarWidget(QDockWidget* dockWidget);

	QSize sizeHint() const override
	{
		QFontMetrics fm(font());
		return QSize(40, fm.height() + 8);
	}

	//! Creates a new button within this title bar widget.
	//! \param icon An icon to be used by the button.
	//! \param tooltip A tooltip for the button.
	//! \return A pointer to the newly created button instance. No ownership is transferred. Pointer remains valid until the end of this widget's lifetime.
	QAbstractButton* AddButton(const QIcon& icon, const char* tooltip);

private:

	QDockWidget* m_dockWidget;
	QBoxLayout*  m_layout;
	QBoxLayout*  m_buttonLayout;
};

class EDITOR_COMMON_API CCustomDockWidget : public QDockWidget
{
	Q_OBJECT
public:
	CCustomDockWidget(const char* title, QWidget* parent = nullptr, Qt::WindowFlags flags = 0);
};

