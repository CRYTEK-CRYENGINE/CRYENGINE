// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include "ManipScene.h"

class QMenuComboBox;

class QLabel;
class QLineEdit;

namespace CharacterTool
{

class TransformPanel : public QWidget
{
	Q_OBJECT
public:
	TransformPanel();

	void                        SetTransform(const QuatT& transform);
	const QuatT&                Transform() const { return m_transform; }

	void                        SetMode(Manip::ETransformationMode mode);
	void                        SetEnabled(bool enabled);

	void                        SetSpace(Manip::ETransformationSpace space);
	Manip::ETransformationSpace Space() const { return m_space; }

signals:
	void SignalSpaceChanged();
	void SignalChanged();
	void SignalChangeFinished();

protected slots:
	void OnSpaceChanged();
	void OnEditXChanged();
	void OnEditXEditingFinished();
	void OnEditYChanged();
	void OnEditYEditingFinished();
	void OnEditZChanged();
	void OnEditZEditingFinished();

private:
	bool ReadEdit(int axis);
	void WriteEdits();

	Manip::ETransformationSpace m_space;
	Manip::ETransformationMode  m_mode;
	QMenuComboBox*              m_comboSpace;
	QLabel*                     m_labelX;
	QLabel*                     m_labelY;
	QLabel*                     m_labelZ;
	QLineEdit*                  m_editX;
	QLineEdit*                  m_editY;
	QLineEdit*                  m_editZ;
	QuatT                       m_transform;
	Ang3                        m_rotationAngles;
};

}

