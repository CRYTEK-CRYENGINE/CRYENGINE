// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>
#include <QPushButton>

#include "EditorCommonAPI.h"

class QNumericEdit;
class QNumericButton;

class EDITOR_COMMON_API QNumericBox : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QColor sliderColor READ getSliderColor WRITE setSliderColor DESIGNABLE true)

public:
	QNumericBox(QWidget* parent = nullptr);
	virtual ~QNumericBox() override;

	//! makes the numeric box restricted to integer numbers
	void   setRestrictToInt();
	void   setMinimum(double min)       { m_min = min; }
	double minimum()                    { return m_min; }
	void   setMaximum(double max)       { m_max = max; }
	double maximum()                    { return m_max; }
	bool   hasMinMax() const;
	void   setSingleStep(double step)   { m_step = step; }
	double singleStep() const           { return m_step; }
	QColor getSliderColor() const       { return m_sliderColor; }
	void   setSliderColor(QColor color) { m_sliderColor = color; }
	void   setAlignment(Qt::Alignment alignment);
	void   setAccelerated(bool accelerated);
	void   setValue(double value);
	void   setPrecision(int precision); //set to 0 for an integer spin box
	void   step(double multiplier);
	double value() const;
	double getSliderPosition() const;
	void   setEditMode(bool edit);
	void   alignText();
	void   setPlaceholderText(const QString& text);

	//!Grabs the keyboard focus to allow immediate typing into the numeric box
	void   grabFocus();

signals:
	void valueChanged(double value);
	void valueSubmitted(double value);
	void shown();

protected:
	virtual void paintEvent(QPaintEvent* e) override;
	virtual void wheelEvent(QWheelEvent* e) override;
	virtual void showEvent(QShowEvent* event) override;
	virtual void resizeEvent(QResizeEvent* event) override;
	virtual void focusInEvent(QFocusEvent* e) override;

	void setText(const QString& text);
	QString text() const;
private:
	void textEdited();

	QNumericEdit*   m_pEdit;
	QNumericButton* m_pUpButton;
	QNumericButton* m_pDownButton;
	int             m_precision;
	double          m_min;
	double          m_max;
	double          m_step;
	QColor          m_sliderColor;
};

class QNumericButton : public QWidget
{
	Q_OBJECT

public:
	QNumericButton(QWidget* parent = nullptr);

	virtual ~QNumericButton() override {}

	void setIcon(const QIcon& icon)       { m_icon = icon; }
	void setAccelerated(bool accelerated) { m_accelerated = accelerated; }

signals:
	void singleStep();
	void clicked();

protected:
	virtual void paintEvent(QPaintEvent* e) override;
	virtual void mousePressEvent(QMouseEvent* e) override;
	virtual void mouseReleaseEvent(QMouseEvent* e) override;
	virtual void focusOutEvent(QFocusEvent* e) override;
	virtual void timerEvent(QTimerEvent* e) override;
	virtual void changeEvent(QEvent* e) override;

private:
	void reset();

	QIcon m_icon;
	int   m_spinClickTimerId;
	int   m_spinClickTimerInterval;
	int   m_spinClickThresholdTimerId;
	int   m_spinClickThresholdTimerInterval;
	int   m_effectiveSpinRepeatRate;
	int   m_acceleration;
	bool  m_accelerated;
};

