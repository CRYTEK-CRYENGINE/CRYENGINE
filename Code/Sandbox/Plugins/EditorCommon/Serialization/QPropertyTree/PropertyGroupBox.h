// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QPropertyGroupBox : public QWidget
{
	Q_OBJECT
public:
	explicit QPropertyGroupBox(QWidget* parent = 0)
		: QWidget(parent)
	{
	}

	virtual ~QPropertyGroupBox() override {}
};

class QPropertyGroupHeader : public QWidget
{
	Q_OBJECT
public:
	explicit QPropertyGroupHeader(QWidget* parent = 0)
		: QWidget(parent)
	{
	}

	virtual ~QPropertyGroupHeader() override {}
};


