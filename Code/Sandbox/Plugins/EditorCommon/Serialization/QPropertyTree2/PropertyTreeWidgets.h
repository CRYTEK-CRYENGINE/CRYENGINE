// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "IPropertyTreeWidget.h"

#include "Controls/QNumericBox.h"
#include "Controls/ColorButton.h"
#include <QCheckBox>

namespace PropertyTree2
{
	class CTextWidget : public QLineEdit, public IPropertyTreeWidget
	{
		Q_OBJECT
		Q_INTERFACES(IPropertyTreeWidget)
	public:
		CTextWidget();

		void focusInEvent(QFocusEvent* e) final;
		virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final;
		virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const final;
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

		virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final;
		virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const final;
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

		virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) override;
		virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const override;
		virtual void Serialize(Serialization::IArchive& ar) final {}
		virtual bool SupportsMultiEdit() const final { return false; }

	protected:
		virtual void Call() = 0;
	};

	class CPtrTypeSelectWidget : public QMenuComboBox, public IPropertyTreeWidget
	{
		Q_OBJECT
		Q_INTERFACES(IPropertyTreeWidget)
	public:
		CPtrTypeSelectWidget();

		virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final;
		virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const final;
		virtual void Serialize(Serialization::IArchive& ar) final;
		virtual bool SupportsMultiEdit() const final { return true; }
		virtual void SetMultiEditValue() final;

	private:
		void SetupCombo();

		const yasli::ClassFactoryBase* m_factory;
		yasli::TypeID m_baseTypeId;
	};

	class CArrayWidget : public QWidget, public IPropertyTreeWidget
	{
		Q_OBJECT
		Q_INTERFACES(IPropertyTreeWidget)
	public:
		CArrayWidget();

		virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final;
		virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const final;

		//TODO: Nothing to serialize for this row, but perhaps we want to serialize the entire subobject when copy/pasting this row ?
		virtual void Serialize(Serialization::IArchive& ar) final;

		virtual bool CanMoveChildren() const final;
		virtual void MoveChild(int oldIndex, int newIndex) final;

		virtual void PopulateContextMenu(QMenu* menu, const PropertyTree2::CRowModel* row) final;
		virtual void PopulateChildContextMenu(QMenu* menu, const PropertyTree2::CRowModel* row) final;
		virtual bool SupportsMultiEdit() const final { return true; }
		virtual void SetMultiEditValue() final;

	private:
		void ExecuteAction(yasli::ContainerInterface* ci) const;
		bool IsArrayMutable() const;

		void OnAddItem();
		void OnClearAll();

		QToolButton* m_addElementButton;
		bool m_variableSize;
		bool m_ordered;

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
			int arg1;
			int arg2; //some actions use two indices, such as move(from,to)

			void Serialize(Serialization::IArchive& ar);
		};

		Action m_action;
	};

	class CStringListWidget : public QMenuComboBox, public IPropertyTreeWidget
	{
		Q_OBJECT
		Q_INTERFACES(IPropertyTreeWidget)
	public:
		CStringListWidget();

		virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final;
		virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const final;
		virtual void Serialize(Serialization::IArchive& ar) final;
		virtual bool SupportsMultiEdit() const final { return true; }
		virtual void SetMultiEditValue() final;

	private:
		template<typename List>
		void SetupCombo(const List& list);
	};

	class CColorwidget : public CColorButton, public IPropertyTreeWidget
	{
		Q_OBJECT
		Q_INTERFACES(IPropertyTreeWidget)
	public:
		CColorwidget();

		virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final;
		virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const final;
		virtual void Serialize(Serialization::IArchive& ar) final;
		virtual bool SupportsMultiEdit() const final { return true; }
		virtual void SetMultiEditValue() final;

		void paintEvent(QPaintEvent* paintEvent) override;
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

		virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final;
		virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const final;
		virtual void Serialize(Serialization::IArchive& ar) final;
		virtual bool SupportsMultiEdit() const final { return true; }
		virtual void SetMultiEditValue() final;

	private:
		double m_previousValue;
	};

	template<typename T>
	CNumberWidget<T>::CNumberWidget()
	{
		connect(this, &CNumberWidget::valueChanged, [this](double value)
		{
			if (m_previousValue != value)
				OnContinuousChanged();
		});
		connect(this, &CNumberWidget::valueSubmitted, [this](double value)
		{
			if (m_previousValue != value)
				OnChanged();
		});

		if (std::is_integral<T>::value)
			setRestrictToInt();
		else
			setPrecision(3);
	}

	template<typename T>
	void CNumberWidget<T>::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
	{
		if (type == yasli::TypeID::get<T>())
		{
			const T val = *(T*)valuePtr;
			m_previousValue = val;
			setValue(val);
		}
		else if (type == yasli::TypeID::get<yasli::RangeDecorator<T>>())
		{
			const yasli::RangeDecorator<T>& decorator = *(yasli::RangeDecorator<T>*)valuePtr;
			m_previousValue = *decorator.value;
			setMaximum(decorator.hardMax);
			setMinimum(decorator.hardMin);
			setSingleStep(decorator.singleStep);
			setValue(m_previousValue);
		}
	}


	template<typename T>
	void CNumberWidget<T>::GetValue(void* valuePtr, const yasli::TypeID& type) const
	{
		//Do nothing if we are in the "multiple values" state
		if (text() == QString())
			return;

		if (type == yasli::TypeID::get<T>())
		{
			*(T*)valuePtr = value();
		}
		else if (type == yasli::TypeID::get<yasli::RangeDecorator<T>>())
		{
			const yasli::RangeDecorator<T>& decorator = *(yasli::RangeDecorator<T>*)valuePtr;
			*decorator.value = value();
		}
	}

	template<typename T>
	void CNumberWidget<T>::SetMultiEditValue()
	{
		setText("");
		setPlaceholderText("Multiple Values");
	}

	template<typename T>
	void CNumberWidget<T>::Serialize(Serialization::IArchive& ar)
	{
		//value() cannot be called with empty text, so it must be avoided
		if (ar.isOutput())
		{
			if (text() != QString())
			{
				double val = value();
				const double oldVal = val;
				ar(val, "value", "Value");
			}
		}
		else if (ar.isInput())
		{
			double val = 0.f;
			const bool found = ar(val, "value", "Value");

			if (found && (text() == QString() || val != value()))
			{
				setValue(val);
				OnChanged();
			}
		}
	}
}


