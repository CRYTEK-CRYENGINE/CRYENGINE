// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <QValidator>
#include <QLineEdit>

class CTimeValidator : public QValidator
{
public:
	explicit CTimeValidator(QWidget* parent = 0);

	virtual State validate(QString& input, int& position) const;

	virtual void  fixup(QString& input) const;
};

class EDITOR_COMMON_API CTimeEditControl : public QLineEdit
{
	Q_OBJECT
public:
	explicit CTimeEditControl(QWidget* parent = 0);

	QTime time() const;
	void  setTime(const QTime& time);

signals:
	void         timeChanged(const QTime& time);
protected:
	static QTime TimeFromString(const QString& string);

	virtual void keyPressEvent(QKeyEvent* event);
};

