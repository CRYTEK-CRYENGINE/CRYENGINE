// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QControls.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QMouseEvent>
#include <QTimer>

#include "CryIcon.h"

QPrecisionSlider::QPrecisionSlider(Qt::Orientation orientation, QWidget* parent)
	: QSlider(orientation, parent)
	, m_sliding(false)
	, m_activateSlow(false)
{
	connect(this, &QPrecisionSlider::sliderReleased, this, &QPrecisionSlider::SliderExit);
	connect(this, &QPrecisionSlider::sliderPressed, this, &QPrecisionSlider::SliderInit);
	connect(this, &QPrecisionSlider::valueChanged, this, &QPrecisionSlider::SliderChanged);
}

void QPrecisionSlider::mouseMoveEvent(QMouseEvent* evt)
{
	// all magic happens here, we intercept the mouse event and tweak the relevant variable
	if (evt->modifiers() & Qt::ShiftModifier)
	{
		if (!m_activateSlow)
		{
			m_activateSlow = true;

			if (m_sliding)
			{
				QPoint initpos = mapFromGlobal(QCursor::pos());
				m_initialMouseValue = (orientation() == Qt::Horizontal) ? initpos.x() : initpos.y();
			}
		}

		if (m_sliding)
		{
			QPoint posnew(evt->localPos().x(), evt->localPos().y());
			if (orientation() == Qt::Horizontal)
			{
				posnew.setX(m_initialMouseValue + 0.1 * (posnew.x() - m_initialMouseValue));
			}
			else
			{
				posnew.setY(m_initialMouseValue + 0.1 * (posnew.y() - m_initialMouseValue));
			}
			QMouseEvent newevent(evt->type(), posnew, evt->button(), evt->buttons(), evt->modifiers());
			QSlider::mouseMoveEvent(&newevent);
			return;
		}
	}
	else
	{
		m_activateSlow = false;
	}

	QSlider::mouseMoveEvent(evt);
}

void QPrecisionSlider::mousePressEvent(QMouseEvent* evt)
{
	if (evt->button() == Qt::LeftButton)
	{
		setValue(minimum() + maximum() * evt->x() / width());
		evt->accept();
	}

	QSlider::mousePressEvent(evt);
}

void QPrecisionSlider::SliderInit()
{
	if (m_activateSlow)
	{
		QPoint initpos = mapFromGlobal(QCursor::pos());
		m_initialMouseValue = (orientation() == Qt::Horizontal) ? initpos.x() : initpos.y();
	}

	m_sliding = true;

	OnSliderPress(value());
}

void QPrecisionSlider::SliderExit()
{
	m_sliding = false;

	OnSliderRelease(value());
	OnSliderChanged(value());
}

void QPrecisionSlider::SliderChanged()
{
	if (!m_sliding)
	{
		OnSliderChanged(value());
	}
}

//////////////////////////////////////////////////////////////////////////

QContainer::QContainer(QWidget* parent /*= 0*/)
	: QFrame(parent)
	, m_child(nullptr)
{
	auto layout = new QVBoxLayout();
	layout->setSpacing(0);
	layout->setMargin(0);
	setLayout(layout);
}

void QContainer::SetChild(QWidget* child)
{
	if (child != m_child)
	{
		if (m_child && child)
		{
			QLayoutItem* pItem = layout()->replaceWidget(m_child, child);
			delete pItem;
			m_child->deleteLater();
		}
		else if (m_child && !child)
		{
			layout()->removeWidget(m_child);
		}
		else if (!m_child && child)
		{
			layout()->addWidget(child);
		}

		m_child = child;
	}
}

//////////////////////////////////////////////////////////////////////////

QMenuLabelSeparator::QMenuLabelSeparator(const char* text /*= ""*/)
	: QWidgetAction(nullptr)
	, m_text(text)
{}

QWidget* QMenuLabelSeparator::createWidget(QWidget* parent)
{
	return new QLabelSeparatorWidget(m_text, parent);
}

//////////////////////////////////////////////////////////////////////////

QLabelSeparatorWidget::QLabelSeparatorWidget(const QString& text, QWidget* parent)
	: QWidget(parent)
	, m_color(66, 66, 66)
{
	m_label = new QLabel();
	m_label->setText(text);
	m_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	auto hbox = new QHBoxLayout();
	hbox->setContentsMargins(0, 0, 0, 0);
	hbox->addWidget(m_label, 0, Qt::AlignLeft);

	setLayout(hbox);
}

void QLabelSeparatorWidget::paintEvent(QPaintEvent* event)
{
	QWidget::paintEvent(event);

	auto rectx = rect();

	QPainter p(this);
	p.setPen(QPen(m_color));
	p.drawLine(QPoint(m_label->size().width(), rectx.height() / 2), QPoint(rectx.right(), rectx.height() / 2));
}

void QLabelSeparatorWidget::setLineColor(QColor& c)
{
	m_color = c;
}

//////////////////////////////////////////////////////////////////////////

QLoading::QLoading(QWidget* parent /*= nullptr*/)
	: QLabel(parent)
	, m_bLoading(true)
	, m_elapsedTime(0)
	, m_delta(16)
{
	m_loadingImage = CryIcon("icons:Dialogs/dialog_loading.ico").pixmap(size() /*, QIcon::Normal, QIcon::On*/);
	m_doneImage = CryIcon("icons:General/Tick.ico").pixmap(size() /*, QIcon::Normal, QIcon::On*/);
	setPixmap(m_loadingImage);

	m_timer.setInterval(m_delta);
	m_timer.setSingleShot(false);
	m_timer.start();
	m_elapsedTimer.start();

	connect(&m_timer, &QTimer::timeout, [this]() { repaint(); });
}

void QLoading::SetLoading(bool loading)
{
	if (loading != m_bLoading)
	{
		m_bLoading = loading;
	}
}

void QLoading::paintEvent(QPaintEvent* pEvent)
{
	// We do this to keep the pixmap square. Get the shortest side
	int pixMapSize = width() > height() ? height() : width();

	QPainter p(this);
	if (m_bLoading)
	{
		QTransform rotation;
		rotation.translate(pixMapSize / 2, pixMapSize / 2);
		rotation.rotate(0.360 * m_elapsedTimer.elapsed());
		rotation.translate(-pixMapSize / 2, -pixMapSize / 2);
		p.setRenderHint(QPainter::SmoothPixmapTransform);
		p.setTransform(rotation);
		p.drawPixmap(QRect(0, 0, pixMapSize, pixMapSize), m_loadingImage);
	}
	else
	{
		p.drawPixmap(QRect(0, 0, pixMapSize, pixMapSize), m_doneImage);
	}
}

