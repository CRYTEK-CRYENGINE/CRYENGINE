// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/EditorDialog.h"

class QMenuComboBox;

class QSpinBox;
class QPushButton;

class SANDBOX_API QCustomResolutionDialog : public CEditorDialog
{
	Q_OBJECT

public:
	QCustomResolutionDialog(int xresolution, int yresolution, bool use_aspect = true, QWidget* parent = nullptr);

	void GetResolution(int& xresolution, int& yresolution);

public slots:
	void ResolutionChangedX(int value);
	void ResolutionChangedY(int value);
	void AspectOptionChanged(int value);
	void AspectChangedX(int value);
	void AspectChangedY(int value);

private:
	int  ClampValue(int value);
	void AspectChanged();

	enum class eConstraintAspect
	{
		eConstraintNone   = 0,
		eConstraintWidth  = 1,
		eConstraintHeight = 2,
	};
	QSpinBox*         widthField;
	QSpinBox*         heightField;
	QPushButton*      okButton;
	QPushButton*      cancelButton;
	QMenuComboBox*    aspectCombo;
	int               m_xres;
	int               m_yres;
	eConstraintAspect m_aspectConstraint;
	int               m_aspectNumerator;
	int               m_aspectDenominator;
};

