// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EditorCommonAPI.h"
#include <QTime>
#include <QKeyEvent>
#include "TimeEditControl.h"

CTimeValidator::CTimeValidator(QWidget* parent /*= 0*/) : QValidator(parent)
{

}

QValidator::State CTimeValidator::validate(QString& input, int& position) const
{
	if (input.isEmpty())
		return Acceptable;

	bool res = false;
	if (position == 1)
	{
		QString sChar(input.at(0));
		uint nDigit = sChar.toUInt(&res);
		if (nDigit > 2)
		{
			input[1] = input[0];
			input[0] = '0';
			position = 3;
		}
	}

	if (position == 4)
	{
		QString sChar(input.at(3));
		uint nDigit = sChar.toUInt(&res);
		if (nDigit > 5)
		{
			input[4] = input[3];
			input[3] = '0';
			position = 5;
		}
	}

	QStringList time = input.split(':');
	if (time.size() == 2)
	{
		QString hours = time.at(0);
		QString minutes = time.at(1);

		uint nHours = hours.toUInt(&res);
		if (!res)
			return Invalid;

		if (nHours > 23)
			return Invalid;

		uint nMinutes = minutes.toUInt(&res);
		bool bSaces = (minutes == "  ") || (minutes == " ");
		if (!res && !bSaces)
			return Invalid;

		if (nMinutes > 59)
			return Invalid;

		return Acceptable;
	}

	return Invalid;
}

void CTimeValidator::fixup(QString& input) const
{
	if (input.at(1) == ' ')
	{
		QChar tmp = input[0];
		input[0] = input[1];
		input[1] = tmp;
	}
	if (input.at(4) == ' ')
	{
		QChar tmp = input[3];
		input[3] = input[4];
		input[4] = tmp;
	}
	input.replace(" ", "0");
}

//////////////////////////////////////////////////////////////////////////

CTimeEditControl::CTimeEditControl(QWidget* parent /*= 0*/) : QLineEdit(parent)
{
	setText("00:00");
	setValidator(new CTimeValidator(this));
	setInputMask("99:99");
	setMaxLength(5);
	setFixedWidth(37);

	connect(this, &QLineEdit::textEdited, [this](const QString& text)
	{
		QString fixed_text = text;
		this->validator()->fixup(fixed_text);
		QTime time = TimeFromString(fixed_text);
		this->timeChanged(time);
	});

	//  connect(this, &QLineEdit::editingFinished, [this]()
	//  {
	//    QTime time = this->time();
	//    this->timeChanged(time);
	//  });
}

QTime CTimeEditControl::time() const
{
	const QString text = this->text();
	return TimeFromString(text);
}

void CTimeEditControl::setTime(const QTime& time)
{
	int nHour = time.hour();
	int nMinute = time.minute();

	QString text = QString("%1:%2").arg(nHour, 2, 10, QChar('0')).arg(nMinute, 2, 10, QChar('0'));
	setText(text);
}

QTime CTimeEditControl::TimeFromString(const QString& string)
{
	QTime result;
	QStringList splittedString = string.split(':');
	if (splittedString.size() == 2)
	{
		QString hours = splittedString.at(0);
		QString minutes = splittedString.at(1);

		bool bHRes = false;
		bool bMRes = false;
		uint nHours = hours.toUInt(&bHRes);
		uint nMinutes = minutes.toUInt(&bMRes);

		if (!bMRes)
		{
			nMinutes = 0;
			bMRes = true;
		}

		if (bHRes && bMRes)
		{
			result.setHMS(nHours, nMinutes, 0);
		}
	}
	return result;
}

void CTimeEditControl::keyPressEvent(QKeyEvent* event)
{
	if ((event->key() == Qt::Key_Return) || (event->key() == Qt::Key_Enter))
	{
		setCursorPosition(0);
	}
	QLineEdit::keyPressEvent(event);
}

