// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

#include <QMenu>
#include <QSlider>
#include <QFrame>
#include <QWidgetAction>
#include <QColor>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>

//////////////////////////////////////////////////////////////////////////
// This file is intended to contain reusable Qt Widgets that are
// of low complexity and do not warrant being in a separate file.
//////////////////////////////////////////////////////////////////////////

class EDITOR_COMMON_API QDynamicPopupMenu : public QMenu
{
	Q_OBJECT

public:
	QDynamicPopupMenu(QWidget* parent = nullptr)
		: QMenu(parent)
	{
		connect(this, &QDynamicPopupMenu::aboutToShow, [this]() { this->clear(); this->OnPopulateMenu(); });
		connect(this, &QDynamicPopupMenu::triggered, this, &QDynamicPopupMenu::OnTrigger);
	}

	virtual ~QDynamicPopupMenu()
	{
		clear();
	}

protected:
	virtual void OnTrigger(QAction* action) {};
	virtual void OnPopulateMenu()           {};
};

//////////////////////////////////////////////////////////////////////////
//! This extends slider behavior
//! - Adds the possibility to press shift for more precision
//! - Adds click to set value instead of drag
class EDITOR_COMMON_API QPrecisionSlider : public QSlider
{
	Q_OBJECT

public:
	QPrecisionSlider(Qt::Orientation orientation, QWidget* parent);

	virtual void mouseMoveEvent(QMouseEvent* evt) override;
	virtual void mousePressEvent(QMouseEvent* evt) override;

Q_SIGNALS:
	// called on slider release
	void OnSliderRelease(int value);
	// called on slider press
	void OnSliderPress(int value);
	void OnSliderChanged(int value);

private:
	void SliderInit();
	void SliderExit();
	void SliderChanged();

	// Keep track of initial position to modify the mouse coordinates from
	int  m_initialMouseValue;
	bool m_sliding;
	bool m_activateSlow;
};

//////////////////////////////////////////////////////////////////////////

//! Useful to contain a single child widget, can be used as a placeholder for widget that are dynamically created but overall layout is preserved
class EDITOR_COMMON_API QContainer : public QFrame
{
public:
	QContainer(QWidget* parent = 0);

	void SetChild(QWidget* child);
	QWidget* GetChild() { return m_child; }
	void Clear() { SetChild(nullptr); }

private:
	QWidget* m_child;
};

//////////////////////////////////////////////////////////////////////////

class QLabelSeparatorWidget : public QWidget
{
	//Necessary for styling
	Q_OBJECT
public:
	QLabelSeparatorWidget(const QString& text, QWidget* parent);

private:
	void   paintEvent(QPaintEvent* event) override;

	QColor lineColor() { return m_color; }
	void   setLineColor(QColor& c);

	QLabel* m_label;
	QColor  m_color;

	Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor);
};

//! Used as a menu separator to describe its section better
class EDITOR_COMMON_API QMenuLabelSeparator : public QWidgetAction
{
public:
	QMenuLabelSeparator(const char* text = "");

protected:
	virtual QWidget* createWidget(QWidget* parent) override;

	QString m_text;
};

//! Provides a rotating "loading" image, or a "tick" if not loading. By default the QLoading is loading.
class EDITOR_COMMON_API QLoading : public QLabel
{
public:
	QLoading(QWidget* parent = nullptr);
	void SetLoading(bool loading);

protected:
	virtual void paintEvent(QPaintEvent* pEvent) override;

	QPixmap       m_loadingImage;
	QPixmap       m_doneImage;
	QTimer        m_timer;
	QElapsedTimer m_elapsedTimer;
	const int     m_delta;       // constant update time
	int           m_elapsedTime; // In ms. Needed because QTimer doesn't track elapsed time
	bool          m_bLoading;
};

