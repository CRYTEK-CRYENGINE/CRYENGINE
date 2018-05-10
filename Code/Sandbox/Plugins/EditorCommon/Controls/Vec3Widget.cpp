// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Vec3Widget.h"

// CryCommon
#include <CrySerialization/Math.h>

// EditorCommon
#include "CryIcon.h"
#include "Controls/QNumericBox.h"

//Qt
#include <QHBoxLayout>
#include <QMouseEvent>

CVec3Widget::CVec3Widget()
	: m_ignoreSignals(false)
{
	QHBoxLayout* pMainLayout = new QHBoxLayout();

	pMainLayout->setSpacing(4);
	pMainLayout->setMargin(0);

	m_pXField = new QNumericBox();
	m_pYField = new QNumericBox();
	m_pZField = new QNumericBox();

	connect(m_pXField, &QNumericBox::valueSubmitted, [this]() { OnValueChanged(m_pXField, false); });
	connect(m_pYField, &QNumericBox::valueSubmitted, [this]() { OnValueChanged(m_pYField, false); });
	connect(m_pZField, &QNumericBox::valueSubmitted, [this]() { OnValueChanged(m_pZField, false); });

	connect(m_pXField, &QNumericBox::valueChanged, [this]() { OnValueChanged(m_pXField, true); });
	connect(m_pYField, &QNumericBox::valueChanged, [this]() { OnValueChanged(m_pYField, true); });
	connect(m_pZField, &QNumericBox::valueChanged, [this]() { OnValueChanged(m_pZField, true); });

	pMainLayout->addWidget(m_pXField);
	pMainLayout->addWidget(m_pYField);
	pMainLayout->addWidget(m_pZField);

	if (GetIEditor() != nullptr)
	{
		GetIEditor()->GetPersonalizationManager()->GetProperty("BaseObject", "UniformScale", m_uniformScale);
	}

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setLayout(pMainLayout);
}

void CVec3Widget::OnValueChanged(QWidget* pField /* = nullptr*/, bool isContinuous /* = false*/)
{
	if (m_ignoreSignals)
		return;

	Vec3 newValue(m_pXField->value(), m_pYField->value(), m_pZField->value());
	if (m_previousValue == newValue)
		return;

	// If uniform scale is enabled then we must figure out which value changed and apply
	// the same scale factor to the rest of the vector
	if (m_type == yasli::TypeID::get<Serialization::SScale>() && m_uniformScale)
	{
		if (pField == m_pXField)
		{
			float multiplier = m_previousValue.x != 0 ? newValue.x / m_previousValue.x : 1.0f;
			newValue.y *= multiplier;
			newValue.z *= multiplier;
		}
		else if (pField == m_pYField)
		{
			float multiplier = m_previousValue.y != 0 ? newValue.y / m_previousValue.y : 1.0f;
			newValue.x *= multiplier;
			newValue.z *= multiplier;
		}
		else if (pField == m_pZField)
		{
			float multiplier = m_previousValue.z != 0 ? newValue.z / m_previousValue.z : 1.0f;
			newValue.x *= multiplier;
			newValue.y *= multiplier;
		}

		SetValue(newValue);
	}

	m_previousValue = newValue;

	if (isContinuous)
	{
		OnContinuousChanged();
	}
	else
	{
		OnChanged();
	}
}

void CVec3Widget::UpdateRow(yasli::TypeID type)
{
	// This widget needs to update it's layout in case of handling scale instead of any other type of Vec3
	// uniform scaling needs to be accounted for
	if (type == m_type)
		return;

	if (layout()->count() > 3)
	{
		QLayoutItem* pItem = layout()->itemAt(3);
		layout()->removeItem(pItem);
		delete pItem;
	}

	yasli::TypeID scaleType = yasli::TypeID::get<Serialization::SScale>();

	if (m_type != scaleType)
	{
		if (type == scaleType)
		{
			QToolButton* pUniformScaleButton = new QToolButton();
			pUniformScaleButton->setToolTip("Uniform Scale");
			pUniformScaleButton->setIcon(m_uniformScale ? CryIcon("icons:General/Uniform.ico") : CryIcon("icons:General/NonUniform.ico"));
			pUniformScaleButton->setCheckable(true);
			pUniformScaleButton->setChecked(m_uniformScale);
			connect(pUniformScaleButton, &QToolButton::clicked, [this, pUniformScaleButton]()
			{
				m_uniformScale = pUniformScaleButton->isChecked();
				pUniformScaleButton->setIcon(m_uniformScale ? CryIcon("icons:General/Uniform.ico") : CryIcon("icons:General/NonUniform.ico"));
				GetIEditor()->GetPersonalizationManager()->SetProperty("BaseObject", "UniformScale", m_uniformScale);
			});
			layout()->addWidget(pUniformScaleButton);
		}
		else
		{
			layout()->addItem(new QSpacerItem(31, 0, QSizePolicy::Fixed, QSizePolicy::Fixed)); // TODO: Fix hardcoded value
		}
	}

	m_type = type;
}

void CVec3Widget::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
{
	if (m_type != type)
	{
		UpdateRow(type);
	}

	if (type == yasli::TypeID::get<Serialization::SPosition>())
	{
		const Serialization::SPosition& value = *(Serialization::SPosition*)valuePtr;
		SetValue(value.vec);

		m_previousValue = value.vec;
	}
	else if (type == yasli::TypeID::get<Serialization::SScale>())
	{
		const Serialization::SScale& value = *(Serialization::SScale*)valuePtr;
		SetValue(value.vec);

		m_previousValue = value.vec;
	}
	else if (type == yasli::TypeID::get<Serialization::SRotation>())
	{
		const Serialization::SRotation& value = *(Serialization::SRotation*)valuePtr;
		Vec3 vec = Vec3(RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(value.quat))));
		SetValue(vec);

		m_previousValue = vec;
	}
}

void CVec3Widget::SetValue(const Vec3& value)
{
	m_ignoreSignals = true;
	m_pXField->setValue(value.x);
	m_pYField->setValue(value.y);
	m_pZField->setValue(value.z);
	m_ignoreSignals = false;
}

void CVec3Widget::GetValue(void* valuePtr, const yasli::TypeID& type) const
{
	if (type == yasli::TypeID::get<Serialization::SPosition>())
	{
		Serialization::SPosition& value = *(Serialization::SPosition*)valuePtr;
		value.vec.x = m_pXField->value();
		value.vec.y = m_pYField->value();
		value.vec.z = m_pZField->value();
	}
	else if (type == yasli::TypeID::get<Serialization::SScale>())
	{
		Serialization::SScale& value = *(Serialization::SScale*)valuePtr;
		value.vec.x = m_pXField->value();
		value.vec.y = m_pYField->value();
		value.vec.z = m_pZField->value();
	}
	else if (type == yasli::TypeID::get<Serialization::SRotation>())
	{
		Serialization::SRotation& value = *(Serialization::SRotation*)valuePtr;

		Vec3 vec;
		vec.x = m_pXField->value();
		vec.y = m_pYField->value();
		vec.z = m_pZField->value();

		value.quat = Quat(Ang3(DEG2RAD(vec)));
	}
}

void CVec3Widget::Serialize(Serialization::IArchive& ar)
{
	Vec3 value;
	if (ar.isInput())
	{
		ar(value, "value", "Value");
		SetValue(value);
		OnValueChanged();
	}
	else
	{
		value.x = m_pXField->value();
		value.y = m_pYField->value();
		value.z = m_pZField->value();
		ar(value, "value", "Value");
	}
}

void CVec3Widget::SetMultiEditValue()
{

}

namespace Serialization
{
REGISTER_PROPERTY_WIDGET(SPosition, CVec3Widget);
REGISTER_PROPERTY_WIDGET(SRotation, CVec3Widget);
REGISTER_PROPERTY_WIDGET(SScale, CVec3Widget);
}

