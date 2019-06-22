// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "IPropertyTreeWidget.h"

#include "Controls/QNumericBox.h"
#include "Controls/ColorButton.h"
#include "Controls/QMenuComboBox.h"
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace PropertyTree2
{
class CTextWidget : public QLineEdit, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
public:
	CTextWidget();

	void         focusInEvent(QFocusEvent* pEvent) final;
	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const final;
	virtual void Serialize(Serialization::IArchive& ar) final;
	virtual bool SupportsMultiEdit() const final { return true; }
	virtual void SetMultiEditValue() final;

private:
	QString m_previousValue;
};

class CBoolWidget : public QCheckBox, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
public:
	CBoolWidget();

	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const final;
	virtual void Serialize(Serialization::IArchive& ar) final;
	virtual bool SupportsMultiEdit() const final { return true; }
	virtual void SetMultiEditValue() final;
};

class CButtonWidget : public QPushButton, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
public:
	CButtonWidget();

	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) override;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const override;
	virtual void Serialize(Serialization::IArchive& ar) final {}
	virtual bool SupportsMultiEdit() const final              { return false; }

protected:
	virtual void Call() = 0;
};

class CPtrTypeSelectWidget : public QMenuComboBox, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
public:
	CPtrTypeSelectWidget();

	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const final;
	virtual void Serialize(Serialization::IArchive& ar) final;
	virtual bool SupportsMultiEdit() const final { return true; }
	virtual void SetMultiEditValue() final;

private:
	void SetupCombo();

	const yasli::ClassFactoryBase* m_pFactory;
	yasli::TypeID                  m_baseTypeId;
};

class CArrayWidget : public QWidget, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
public:
	CArrayWidget();

	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const final;

	//TODO: Nothing to serialize for this row, but perhaps we want to serialize the entire subobject when copy/pasting this row ?
	virtual void Serialize(Serialization::IArchive& ar) final;

	virtual bool CanMoveChildren() const final;
	virtual void MoveChild(int oldIndex, int newIndex) final;

	virtual void PopulateContextMenu(QMenu* pMenu, const PropertyTree2::CRowModel* pRow) final;
	virtual void PopulateChildContextMenu(QMenu* pMenu, const PropertyTree2::CRowModel* pRow) final;
	virtual bool SupportsMultiEdit() const final { return true; }
	virtual void SetMultiEditValue() final;

private:
	void ExecuteAction(yasli::ContainerInterface* pContainerInterface) const;
	bool IsArrayMutable() const;

	void OnAddItem();
	void OnClearAll();

	QToolButton* m_pAddElementButton;
	bool         m_isFixedSize;
	bool         m_ordered;

	enum ActionType
	{
		None,
		InsertItem,
		MoveItem,
		DeleteItem,
		Resize
	};

	struct Action
	{
		Action(ActionType t = ActionType::None, int a1 = -1, int a2 = -1) : type(t), arg1(a1), arg2(a2) {}
		ActionType type;
		int        arg1;
		int        arg2; //some actions use two indices, such as move(from,to)

		void       Serialize(Serialization::IArchive& ar);
	};

	Action m_action;
};

class CStringListWidget : public QMenuComboBox, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
public:
	CStringListWidget();

	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const final;
	virtual void Serialize(Serialization::IArchive& ar) final;
	virtual bool SupportsMultiEdit() const final { return true; }
	virtual void SetMultiEditValue() final;
	//!Show the values in this string list, actually populates the popup with the actions the first time it's called
	void         ShowPopup() override;
private:
	template<typename List>
	//!Setup the widget with a new list of values, this values are cached in m_values but not added to the popup widget until ShowPopup is called
	void SetupCombo(const List& list);
	//!List of cached strings we need to show when the popup is opened
	std::vector<QString> m_values;
	int                  m_selected;
	//!The widget has the values it should display but they have not been added to the popup yet
	bool                 m_loadComplete;
};

class CColorwidget : public CColorButton, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
public:
	CColorwidget();

	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const final;
	virtual void Serialize(Serialization::IArchive& ar) final;
	virtual bool SupportsMultiEdit() const final { return true; }
	virtual void SetMultiEditValue() final;

	void         paintEvent(QPaintEvent* paintEvent) override;
private:
	bool m_multiEdit;
};

//////////////////////////////////////////////////////////////////////////

class CNumberWidgetBase : public QNumericBox, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
};

template<typename T>
class CNumberWidget : public CNumberWidgetBase
{
public:
	CNumberWidget();

	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const final;
	virtual void Serialize(Serialization::IArchive& ar) final;
	virtual bool SupportsMultiEdit() const final { return true; }
	virtual void SetMultiEditValue() final;

private:
	void OnValueChanged(double value);
	void OnValueSubmitted(double value);

	double m_previousValue;
};

template<typename T>
CNumberWidget<T>::CNumberWidget()
{
	//avoid sending change signal when we are setting up this widget, they will register invalid undos
	RECURSION_GUARD(m_ignoreChangeSignals);

	//We register change events handlers here to avoid invalid event being sent on setup
	connect(this, &CNumberWidget::valueChanged, this, &CNumberWidget::OnValueChanged);
	connect(this, &CNumberWidget::valueSubmitted, this, &CNumberWidget::OnValueSubmitted);

	if (std::is_integral<T>::value)
	{
		setRestrictToInt();
	}
	else
	{
		//this will actually call set value and emit an invalid undo if change signals are enabled
		setPrecision(3);
	}
}

template<typename T>
void CNumberWidget<T >::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	if (type == yasli::TypeID::get<T>())
	{
		const T value = *(T*)pValue;
		m_previousValue = value;
		setValue(value);
	}
	else if (type == yasli::TypeID::get<yasli::RangeDecorator<T>>())
	{
		const yasli::RangeDecorator<T>& decorator = *(yasli::RangeDecorator<T>*)pValue;
		m_previousValue = *decorator.value;
		setMaximum(decorator.softMax);
		setMinimum(decorator.softMin);
		setSingleStep(decorator.singleStep);
		setValue(m_previousValue);
	}
}

template<typename T>
void CNumberWidget<T >::GetValue(void* pValue, const yasli::TypeID& type) const
{
	//Do nothing if we are in the "multiple values" state
	if (text() == QString())
	{
		return;
	}

	if (type == yasli::TypeID::get<T>())
	{
		*(T*)pValue = value();
	}
	else if (type == yasli::TypeID::get<yasli::RangeDecorator<T>>())
	{
		const yasli::RangeDecorator<T>& decorator = *(yasli::RangeDecorator<T>*)pValue;
		*decorator.value = value();
	}
}

template<typename T>
void CNumberWidget<T >::SetMultiEditValue()
{
	setText("");
	setPlaceholderText("Multiple Values");
}

template<typename T>
void CNumberWidget<T >::Serialize(Serialization::IArchive& ar)
{
	//value() cannot be called with empty text, so it must be avoided
	if (ar.isOutput())
	{
		if (text() != QString())
		{
			double widgetValue = value();
			const double oldwidgetValue = widgetValue;
			ar(widgetValue, "value", "Value");
		}
	}
	else if (ar.isInput())
	{
		double widgetValue = 0.f;
		const bool found = ar(widgetValue, "value", "Value");

		if (found && (text() == QString() || widgetValue != value()))
		{
			setValue(widgetValue);
			OnChanged();
		}
	}
}

template<typename T>
void PropertyTree2::CNumberWidget<T>::OnValueSubmitted(double value)
{
	if (m_previousValue != value)
	{
		OnChanged();
	}
	else
	{
		//If we are setting back to previous value discard all the changes that might have happened in valueChanged/continuous change
		OnDiscarded();
	}
}

template<typename T>
void PropertyTree2::CNumberWidget<T>::OnValueChanged(double value)
{
	if (m_previousValue != value)
	{
		OnContinuousChanged();
	}
}
}
