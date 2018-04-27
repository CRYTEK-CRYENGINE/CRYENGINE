// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ColorButton.h"

#include <QColorDialog>
#include <QDesktopWidget>
#include "QtUtil.h"

CColorButton::CColorButton()
	: m_color(Qt::white)
	, m_hasAlpha(true)
{
	connect(this, &CColorButton::clicked, this, &CColorButton::OnClick);
}

void CColorButton::SetColor(const ColorB& color)
{
	SetColor(QtUtil::ToQColor(color));
}

void CColorButton::SetColor(const ColorF& color)
{
	SetColor(QtUtil::ToQColor(color));
}

void CColorButton::SetColor(const QColor& color)
{
	if (color != m_color)
	{
		m_color = color;
		update();
		signalColorChanged(m_color);
	}
}

ColorB CColorButton::GetColorB() const
{
	return QtUtil::ToColorB(m_color);
}

ColorF CColorButton::GetColorF() const
{
	return QtUtil::ToColorF(m_color);
}

void CColorButton::paintEvent(QPaintEvent* paintEvent)
{
	static QImage checkboardPattern;
	if (checkboardPattern.isNull())
	{
		const int size = 14;
		static int pixels[size * size];
		for (int i = 0; i < size * size; ++i)
			pixels[i] = ((i / size) / (size / 2) + (i % size) / (size / 2)) % 2 ? 0xffffffff : 0x000000ff;
		checkboardPattern = QImage((unsigned char*)pixels, size, size, size * 4, QImage::Format_RGBA8888);
	}

	QStyle* style = QWidget::style();
	QPainter painter(this);

	QStyleOption opt;
	opt.initFrom(this);

	painter.setRenderHint(QPainter::Antialiasing);
	QPainterPath path;
	path.addRoundedRect(rect(), 3, 3);
	QPen pen(Qt::black, 2);
	painter.setPen(pen);
	painter.drawPath(path);

	const bool opaque = m_color.alpha() == 255;

	if (!opaque)
	{
		//Paint checkerboard
		painter.fillPath(path, QBrush(checkboardPattern));
	}

	painter.fillPath(path, m_color);
}

void CColorButton::OnClick()
{
	//Show the color picker as a drop-down

	const QColor initialColor = m_color;

	QColorDialog* picker = new QColorDialog(initialColor, this);
	picker->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);

	QColorDialog::ColorDialogOptions options = QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog;
	if (m_hasAlpha)
		options |= QColorDialog::ShowAlphaChannel;
	picker->setOptions(options);

	picker->setProperty("colorChanged", false);
	picker->setProperty("finished", false); //workaround for 'pick screen color' in color dialog

	QObject::connect(picker, &QColorDialog::currentColorChanged, [picker, this](const QColor& clr)
	{
		QVariant finished = picker->property("finished");
		if (finished.toBool())
			return;

		QVariant colorchanged = picker->property("colorChanged");

		if (!colorchanged.toBool())
			picker->setProperty("colorChanged", true);

		m_color = clr;
		signalColorContinuousChanged(m_color);
		update();
	});

	QObject::connect(picker, &QColorDialog::finished, [initialColor, picker, this](int result)
	{
		QVariant colorchanged = picker->property("colorChanged");
		if (CryGetAsyncKeyState(VK_ESCAPE)) //TODO: avoid using this, an event filter could be good for instance
		{
			m_color = initialColor;
			signalColorContinuousChanged(m_color);
		}
		else if (colorchanged.toBool())
			signalColorChanged(m_color);

		picker->setProperty("finished", true);
	});

	{
		//adjust pos
		const QDesktopWidget* desktop = QApplication::desktop();
		const int screen = desktop->screenNumber(this);
		const QRect screenRect = desktop->screenGeometry(screen);
		const QSize pickerSizeHint = picker->sizeHint();

		const QPoint popupCoords = mapToGlobal(rect().bottomLeft());

		QPoint pickerPos(popupCoords.x(), popupCoords.y());

		if (pickerPos.y() + pickerSizeHint.height() > screenRect.bottom())
		{
			pickerPos.setY(popupCoords.y() - rect().height() - pickerSizeHint.height() - 1);
		}

		if (pickerPos.x() < screenRect.left())
		{
			pickerPos.setX(screenRect.left() + 1);
		}

		if (pickerPos.x() + pickerSizeHint.width() > screenRect.right())
		{
			pickerPos.setX(screenRect.right() - pickerSizeHint.width() - 1);
		}

		picker->move(pickerPos);
	}

	picker->show();
}
