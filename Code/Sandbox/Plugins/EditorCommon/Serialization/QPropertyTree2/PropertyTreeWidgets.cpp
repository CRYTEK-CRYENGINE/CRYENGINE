// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTreeWidgets.h"

#include "PropertyTreeModel.h"
#include "Serialization/PropertyTree/Color.h"

#include <CrySerialization/Decorators/ActionButton.h>
#include <CrySerialization/yasli/decorators/Button.h>
#include <CrySerialization/yasli/decorators/Range.h>
#include <CrySerialization/ColorImpl.h>
#include <CrySerialization/Color.h>

namespace PropertyTree2
{
	CTextWidget::CTextWidget()
		: QLineEdit()
	{
		connect(this, &CTextWidget::editingFinished, [this]() 
		{ 
			if(m_previousValue != text())
			{
				OnChanged();

				if (hasFocus())
				{
					selectAll();
				}
			}
		});
	}

	void CTextWidget::focusInEvent(QFocusEvent* e)
	{
		QLineEdit::focusInEvent(e);
		selectAll();
	}

	void CTextWidget::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
	{
		if (type == yasli::TypeID::get<yasli::StringInterface>())
		{
			yasli::StringInterface* si = (yasli::StringInterface*)valuePtr;
			setText(si->get());
		}
		else if (type == yasli::TypeID::get<yasli::WStringInterface>())
		{
			yasli::WStringInterface* si = (yasli::WStringInterface*)valuePtr;
			setText(QString((const QChar*)si->get()));
		}
		else
		{
			return;
		}

		m_previousValue = text();
	}

	void CTextWidget::GetValue(void* valuePtr, const yasli::TypeID& type) const
	{
		if (type == yasli::TypeID::get<yasli::StringInterface>())
		{
			yasli::StringInterface* si = (yasli::StringInterface*)valuePtr;
			si->set(text().toStdString().c_str());
		}
		else if (type == yasli::TypeID::get<yasli::WStringInterface>())
		{
			yasli::WStringInterface* si = (yasli::WStringInterface*)valuePtr;
			si->set((wchar_t*)text().data());
		}
	}

	void CTextWidget::Serialize(Serialization::IArchive& ar)
	{
		string str = text().toStdString().c_str();
		ar(str, "value", "Value");
		if (str != m_previousValue && ar.isInput())
		{
			setText(QString(str));
			m_previousValue = text();
			OnChanged();
		}
	}

	void CTextWidget::SetMultiEditValue()
	{
		setText("");
		setPlaceholderText("Multiple Values");
	}

	//////////////////////////////////////////////////////////////////////////

	CBoolWidget::CBoolWidget()
	{
		connect(this, &CBoolWidget::clicked, [this]() 
		{ 
			setTristate(false);
			OnChanged(); 
		});
	}

	void CBoolWidget::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
	{
		if (type == yasli::TypeID::get<bool>())
		{
			setChecked(*(bool*)valuePtr ? Qt::Checked : Qt::Unchecked);
		}
	}

	void CBoolWidget::GetValue(void* valuePtr, const yasli::TypeID& type) const
	{
		if (type == yasli::TypeID::get<bool>())
		{
			*(bool*)valuePtr = checkState() == Qt::Checked;
		}
	}

	void CBoolWidget::Serialize(Serialization::IArchive& ar)
	{
		bool checked = checkState() == Qt::Checked;
		const bool oldChecked = checked;
		ar(checked, "checked", "Checked");
		if (oldChecked != checked && ar.isInput())
		{
			setChecked(checked ? Qt::Checked : Qt::Unchecked);
			OnChanged();
		}
	}

	void CBoolWidget::SetMultiEditValue()
	{
		setTristate(true);
		setCheckState(Qt::PartiallyChecked);
	}
	
	REGISTER_PROPERTY_WIDGET(bool, CBoolWidget);

	//////////////////////////////////////////////////////////////////////////

	CButtonWidget::CButtonWidget()
		: QPushButton()
	{
		//This widget does not trigger serialization
		connect(this, &CButtonWidget::clicked, this, &CButtonWidget::Call);
	}

	void CButtonWidget::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
	{
		//Setting the label here as it may change every time this is called, while the row is only set once
		setText(m_rowModel->GetLabel());
	}

	void CButtonWidget::GetValue(void* valuePtr, const yasli::TypeID& type) const
	{
		//Do nothing
	}

	class CActionButtonWidget : public CButtonWidget
	{
	public:
		using CButtonWidget::CButtonWidget;

		void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final
		{
			CButtonWidget::SetValue(valuePtr, type, ar);
			if (type == yasli::TypeID::get<Serialization::IActionButton>())
			{
				Serialization::IActionButton* actionButton = (Serialization::IActionButton*)valuePtr;
				//TODO: optimize it so it doesn't have to do allocate and clone every time!
				m_actionButton = actionButton->Clone();
			}
		}

		void Call() final
		{
			m_actionButton->Callback();
		}

	private:
		Serialization::IActionButtonPtr m_actionButton;
	};

	class CYasliButtonWidget : public CButtonWidget
	{
	public:
		CYasliButtonWidget()
			: m_pressed(false)
		{}

		void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) final
		{
			if (type == yasli::TypeID::get<yasli::Button>())
			{
				yasli::Button* button = (yasli::Button*)valuePtr;
				setText(button->text);
				m_pressed = button->pressed;
			}
		}

		void GetValue(void* valuePtr, const yasli::TypeID& type) const final
		{
			if (type == yasli::TypeID::get<yasli::Button>())
			{
				yasli::Button* button = (yasli::Button*)valuePtr;
				button->pressed = m_pressed;
			}
		}

		void Call() final
		{
			m_pressed = true;
			OnChanged();
		}

	private:
		bool m_pressed;
	};

	//////////////////////////////////////////////////////////////////////////


	CPtrTypeSelectWidget::CPtrTypeSelectWidget()
		: m_factory(nullptr)
	{
		//TODO: Disallow mouse wheel movement here as it can be very dangerous
		signalCurrentIndexChanged.Connect(this, &CPtrTypeSelectWidget::OnChanged);
	}

	void CPtrTypeSelectWidget::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
	{
		if (type == yasli::TypeID::get<yasli::PointerInterface>())
		{
			yasli::PointerInterface* ptr = (yasli::PointerInterface*)valuePtr;

			//Rebuild the combo if Multiple Value is detected
			bool ok = false;
			const int data = GetData(0).toInt(&ok);
			const bool bMultipleValues = ok && data == -2;

			if (bMultipleValues || ptr->baseType() != m_baseTypeId)
			{
				//Setup the combo box if not set
				m_baseTypeId = ptr->baseType();
				m_factory = ptr->factory();
				Clear();
				SetupCombo();
			}

			//Select the currently valid entry
			const auto desc = m_factory->descriptionByRegisteredName(ptr->registeredTypeName());
			if (desc)
				SetText(desc->label());
			else //null pointer
				SetCheckedByData(-1);
		}
	}

	void CPtrTypeSelectWidget::GetValue(void* valuePtr, const yasli::TypeID& type) const
	{
		if (type == yasli::TypeID::get<yasli::PointerInterface>())
		{
			yasli::PointerInterface* ptr = (yasli::PointerInterface*)valuePtr;

			bool ok = false;
			int selectedFactoryIndex = GetCurrentData().toInt(&ok);
			CRY_ASSERT(ok);

			if (selectedFactoryIndex == -2) //Multiple values, invalid
				return;

			const auto desc = selectedFactoryIndex >= 0 ? m_factory->descriptionByIndex(selectedFactoryIndex) : nullptr;
			if (desc != m_factory->descriptionByRegisteredName(ptr->registeredTypeName()))
			{
				//Create object of new type
				if (desc)
					ptr->create(desc->name()); //TODO: ensure that row is expanded next ?! perhaps any row that didn't have children and how has should be expanded ?
				else
					ptr->create(nullptr);
			}
		}
	}

	void CPtrTypeSelectWidget::Serialize(Serialization::IArchive& ar)
	{
		string text = GetText().toStdString().c_str();
		ar(text, "type", "Pointer Type");
		string typeName = m_baseTypeId.name();
		ar(typeName, "basetype", "Base Type");

		if(ar.isInput() && typeName == m_baseTypeId.name())
			SetText(QString(text));//This calls OnChanged() if necessary
	}

	void CPtrTypeSelectWidget::SetMultiEditValue()
	{
		Clear();
		AddItem("Multiple Values", -2);
		SetupCombo();
		SetChecked(0, true);
	}

	void CPtrTypeSelectWidget::SetupCombo()
	{
		const size_t count = m_factory->size();

		const char* nullLabel = m_factory->nullLabel();

		if (nullLabel && *nullLabel)
			AddItem(nullLabel, -1);
		else
			AddItem("[ null ]", -1);

		for (int i = 0; i < count; ++i)
		{
			const yasli::TypeDescription* desc = m_factory->descriptionByIndex(i);
			AddItem(desc->label(), i);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	CArrayWidget::CArrayWidget()
	{
		m_addElementButton = new QToolButton();
		m_addElementButton->setIcon(CryIcon("icons:General/Element_Add.ico"));
		m_addElementButton->setToolTip(tr("Add Item"));
		connect(m_addElementButton, &QToolButton::clicked, this, &CArrayWidget::OnAddItem);

		QHBoxLayout* layout = new QHBoxLayout();
		layout->addWidget(m_addElementButton, 0, Qt::AlignRight | Qt::AlignVCenter);
		layout->setMargin(0);
		setLayout(layout);

		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setCursor(Qt::ArrowCursor);
	}


	void CArrayWidget::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
	{
		CRY_ASSERT(type == yasli::TypeID::get<yasli::ContainerInterface>());
		yasli::ContainerInterface* pContainer = (yasli::ContainerInterface*)valuePtr;

		m_variableSize = !pContainer->isFixedSize();
		m_ordered = pContainer->isOrdered();
		m_addElementButton->setVisible(m_variableSize);
		m_action = Action();
	}

	void CArrayWidget::GetValue(void* valuePtr, const yasli::TypeID& type) const
	{
		CRY_ASSERT(type == yasli::TypeID::get<yasli::ContainerInterface>());
		yasli::ContainerInterface* pContainer = (yasli::ContainerInterface*)valuePtr;

		ExecuteAction(pContainer);
	}

	bool CArrayWidget::CanMoveChildren() const
	{
		return m_ordered;
	}

	void CArrayWidget::MoveChild(int oldIndex, int newIndex)
	{
		m_action = { ActionType::MoveItem, oldIndex, newIndex };
		OnChanged();
	}

	void CArrayWidget::ExecuteAction(yasli::ContainerInterface* ci) const
	{
		switch (m_action.type)
		{
		case InsertItem:
			if(m_variableSize)
			{
				ci->begin();
				for (int i = 0; i < m_action.arg1; i++)
					ci->next();

				ci->insert();
			}
			break;
		case MoveItem:
			ci->begin();
			for (int i = 0; i < m_action.arg1; i++)
				ci->next();

			ci->move(m_action.arg2);
			break;
		case DeleteItem:
			if (m_variableSize)
			{
				ci->begin();
				for (int i = 0; i < m_action.arg1; i++)
					ci->next();

				ci->remove();
			}
			break;
		case Resize:
			if(m_variableSize)
				ci->resize(m_action.arg1);
			break;
		default:
			break;
		}
	}

	bool CArrayWidget::IsArrayMutable() const
	{
		return m_variableSize;
	}

	void CArrayWidget::PopulateContextMenu(QMenu* menu, const PropertyTree2::CRowModel* row)
	{
		if (m_variableSize)
		{
			auto action = menu->addAction(tr("Add Item"));
			connect(action, &QAction::triggered, this, &CArrayWidget::OnAddItem);

			action = menu->addAction(tr("Clear All"));
			connect(action, &QAction::triggered, this, &CArrayWidget::OnClearAll);
		}
	}

	void CArrayWidget::PopulateChildContextMenu(QMenu* menu, const PropertyTree2::CRowModel* row)
	{
		CRY_ASSERT(row->GetParent() == m_rowModel);

		//We know that arrays don't have hidden rows so index is usable directly
		const auto index = row->GetIndex();

		if (m_variableSize)
		{
			auto action = menu->addAction(tr("Delete"));
			connect(action, &QAction::triggered, [this, index]()
			{
				m_action = { ActionType::DeleteItem, index, -1 };
				OnChanged();
			});
		}

		if(index > 0)
		{
			auto action = menu->addAction(tr("Move Up"));
			connect(action, &QAction::triggered, [this, row, index]()
			{
				m_action = { ActionType::MoveItem, index, index -1 };
				OnChanged();
			});
		}

		if(index < row->GetParent()->GetChildren().size() - 1)
		{
			auto action = menu->addAction(tr("Move Down"));
			connect(action, &QAction::triggered, [this, row, index]()
			{
				m_action = { ActionType::MoveItem, index, index + 1 };
				OnChanged();
			});
		}

		if(m_variableSize)
		{
			auto action = menu->addAction(tr("Add Above"));
			connect(action, &QAction::triggered, [this, row, index]()
			{
				m_action = { ActionType::InsertItem, index, -1 };
				OnChanged();
			});

			action = menu->addAction(tr("Add Below"));
			connect(action, &QAction::triggered, [this, row, index]()
			{
				m_action = { ActionType::InsertItem, index+1, -1 };
				OnChanged();
			});

			menu->addSeparator();

			action = menu->addAction(tr("Clear All"));
			connect(action, &QAction::triggered, this, &CArrayWidget::OnClearAll);
		}
	}

	void CArrayWidget::SetMultiEditValue()
	{
		QString label = m_rowModel->GetLabel();
		const auto index = label.lastIndexOf('(');
		label = label.left(index);
		label.append("(Multiple Sizes)");
		m_rowModel->SetLabel(label);
	}

	void CArrayWidget::OnAddItem()
	{
		m_action = { ActionType::InsertItem, (int)m_rowModel->GetChildren().size(), -1 };
		OnChanged();
	}

	void CArrayWidget::OnClearAll()
	{
		m_action = { ActionType::Resize, 0, -1 };
		OnChanged();
	}

	void CArrayWidget::Action::Serialize(Serialization::IArchive& ar)
	{
		ar((std::underlying_type<ActionType>::type&)type, "type", "type");
		ar(arg1, "arg1", "arg1");
		ar(arg2, "arg2", "arg2");
	}

	void CArrayWidget::Serialize(Serialization::IArchive& ar)
	{
		//If we have an action in progress then serialization should transfer the action (used in multi-edit),
		//otherwise we transfer the size as it is the only thing really managed by this widget (for copy/paste situations)

		if (ar.isInput())
		{
			const bool actionFound = ar(m_action, "action", "Action");
			
			//Only take into account size if action was not set
			if (!actionFound && m_action.type == ActionType::None)
			{
				int size = 0;
				bool sizeFound = ar(size, "size", "Size");
				if (sizeFound && m_variableSize && size != m_rowModel->GetChildren().size())
				{
					m_action = { ActionType::Resize, size, -1 };
					OnChanged();
				}
			}
		}
		else
		{
			if (m_action.type != ActionType::None)
			{
				ar(m_action, "action", "Action");
			}
			else
			{
				int size = m_rowModel->GetChildren().size();
				ar(size, "size", "Size");
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////

	CStringListWidget::CStringListWidget()
	{
		signalCurrentIndexChanged.Connect(this, &CStringListWidget::OnChanged);
	}

	void CStringListWidget::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
	{
		if (type == yasli::TypeID::get<yasli::StringListValue>())
		{
			const yasli::StringListValue& value = *(yasli::StringListValue*)valuePtr;
			SetupCombo(value);
		}
		else if (type == yasli::TypeID::get<yasli::StringListStaticValue>())
		{
			const yasli::StringListStaticValue& value = *(yasli::StringListStaticValue*)valuePtr;
			SetupCombo(value);
		}
	}

	void CStringListWidget::GetValue(void* valuePtr, const yasli::TypeID& type) const
	{
		if (type == yasli::TypeID::get<yasli::StringListValue>())
		{
			yasli::StringListValue& value = *(yasli::StringListValue*)valuePtr;
			value = GetCheckedItem();
		}
		else if (type == yasli::TypeID::get<yasli::StringListStaticValue>())
		{
			yasli::StringListStaticValue& value = *(yasli::StringListStaticValue*)valuePtr;
			value = GetCheckedItem();
		}
	}

	void CStringListWidget::Serialize(Serialization::IArchive& ar)
	{
		string str = GetText().toStdString().c_str();
		ar(str, "value", "Value");
		if (ar.isInput())
			SetText(QString(str));
	}

	void CStringListWidget::SetMultiEditValue()
	{
		AddItem("Multiple Values");
		SetText("Multiple Values");
	}

	template<typename List>
	void PropertyTree2::CStringListWidget::SetupCombo(const List& list)
	{
		bool shouldInit = false;

		auto stringList = list.stringList();
		if (stringList.size() != GetItemCount())
			shouldInit = true;
		else
		{
			for (int i = 0; i < stringList.size(); i++)
			{
				if (GetItem(i) != stringList[i])
				{
					shouldInit = true;
					break;
				}
			}
		}

		if (shouldInit)
		{
			Clear();
			for (int i = 0; i < stringList.size(); i++)
			{
				AddItem(QString(stringList[i]));
			}
		}

		SetChecked(list.index());
	}

	CColorwidget::CColorwidget()
		: m_multiEdit(false)
	{
		signalColorChanged.Connect(this, &CColorwidget::OnChanged);
		signalColorContinuousChanged.Connect(this, &CColorwidget::OnContinuousChanged);
	}

	void CColorwidget::SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar)
	{
		m_multiEdit = false;
		if (type == yasli::TypeID::get<SerializableColor_tpl<unsigned char>>())
		{
			const SerializableColor_tpl<unsigned char>& value = *(SerializableColor_tpl<unsigned char>*)valuePtr;
			SetColor(value);
			SetHasAlpha(true);
		}
		else if (type == yasli::TypeID::get<SerializableColor_tpl<float>>())
		{
			const SerializableColor_tpl<float>& value = *(SerializableColor_tpl<float>*)valuePtr;
			SetColor(value);
			SetHasAlpha(true);
		}
		else if (type == yasli::TypeID::get<Serialization::Vec3AsColor>())
		{
			const Serialization::Vec3AsColor& value = *(Serialization::Vec3AsColor*)valuePtr;
			SetColor(QColor(value.v.x, value.v.y, value.v.z));
			SetHasAlpha(false);
		}
		else if (type == yasli::TypeID::get<SerializableColor_tpl<property_tree::Color>>())
		{
			const property_tree::Color& value = *(property_tree::Color*)valuePtr;
			SetColor(QColor(value.r, value.g, value.b, value.a));
		}
	}

	void CColorwidget::GetValue(void* valuePtr, const yasli::TypeID& type) const
	{
		if (type == yasli::TypeID::get<SerializableColor_tpl<unsigned char>>())
		{
			ColorB& value = *(SerializableColor_tpl<unsigned char>*)valuePtr;
			value = GetColorB();
		}
		else if (type == yasli::TypeID::get<SerializableColor_tpl<float>>())
		{
			ColorF& value = *(SerializableColor_tpl<float>*)valuePtr;
			value = GetColorF();
		}
		else if (type == yasli::TypeID::get<Serialization::Vec3AsColor>())
		{
			Serialization::Vec3AsColor& value = *(Serialization::Vec3AsColor*)valuePtr;
			const auto color = GetQColor();
			value.v.x = color.red();
			value.v.y = color.green();
			value.v.z = color.blue();
		}
		else if (type == yasli::TypeID::get<SerializableColor_tpl<property_tree::Color>>())
		{
			property_tree::Color& value = *(property_tree::Color*)valuePtr;
			const auto color = GetQColor();
			value.r = color.red();
			value.g = color.green();
			value.b = color.blue();
			value.a = color.alpha();
		}
	}

	void CColorwidget::Serialize(Serialization::IArchive& ar)
	{
		ColorB color = GetColorB();
		ar(color, "color", "Color");
		SetColor(color);
	}

	void CColorwidget::SetMultiEditValue()
	{
		m_multiEdit = true;
		setText("Multiple Values");
	}

	void CColorwidget::paintEvent(QPaintEvent* paintEvent)
	{
		if (!m_multiEdit)
		{
			CColorButton::paintEvent(paintEvent);
		}
		else
		{
			QPushButton::paintEvent(paintEvent);
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//Registration of all the widgets
//////////////////////////////////////////////////////////////////////////

namespace yasli
{
	REGISTER_PROPERTY_WIDGET(StringInterface,		PropertyTree2::CTextWidget);
	REGISTER_PROPERTY_WIDGET(WStringInterface,		PropertyTree2::CTextWidget);
	REGISTER_PROPERTY_WIDGET(PointerInterface,		PropertyTree2::CPtrTypeSelectWidget);
	REGISTER_PROPERTY_WIDGET(Button,				PropertyTree2::CYasliButtonWidget);
	REGISTER_PROPERTY_WIDGET(ContainerInterface,	PropertyTree2::CArrayWidget);
	REGISTER_PROPERTY_WIDGET(StringListStaticValue, PropertyTree2::CStringListWidget)
	REGISTER_PROPERTY_WIDGET(StringListValue,		PropertyTree2::CStringListWidget)

	REGISTER_PROPERTY_WIDGET(float,		PropertyTree2::CNumberWidget<float>);
	REGISTER_PROPERTY_WIDGET(double,	PropertyTree2::CNumberWidget<double>);
	REGISTER_PROPERTY_WIDGET(char,		PropertyTree2::CNumberWidget<char>);

	REGISTER_PROPERTY_WIDGET(i8,		PropertyTree2::CNumberWidget<i8>);
	REGISTER_PROPERTY_WIDGET(i16,		PropertyTree2::CNumberWidget<i16>);
	REGISTER_PROPERTY_WIDGET(i32,		PropertyTree2::CNumberWidget<i32>);
	REGISTER_PROPERTY_WIDGET(i64,		PropertyTree2::CNumberWidget<i64>);

	REGISTER_PROPERTY_WIDGET(u8,		PropertyTree2::CNumberWidget<u8>);
	REGISTER_PROPERTY_WIDGET(u16,		PropertyTree2::CNumberWidget<u16>);
	REGISTER_PROPERTY_WIDGET(u32,		PropertyTree2::CNumberWidget<u32>);
	REGISTER_PROPERTY_WIDGET(u64,		PropertyTree2::CNumberWidget<u64>);

	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<float>,	_float,		PropertyTree2::CNumberWidget<float>);
	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<double>,	_double,	PropertyTree2::CNumberWidget<double>);
	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<char>,		_char,		PropertyTree2::CNumberWidget<char>);

	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<i8>,		_i8,		PropertyTree2::CNumberWidget<i8>);
	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<i16>,		_i16,		PropertyTree2::CNumberWidget<i16>);
	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<i32>,		_i32,		PropertyTree2::CNumberWidget<i32>);
	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<i64>,		_i64,		PropertyTree2::CNumberWidget<i64>);

	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<u8>,		_u8,		PropertyTree2::CNumberWidget<u8>);
	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<u16>,		_u16,		PropertyTree2::CNumberWidget<u16>);
	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<u32>,		_u32,		PropertyTree2::CNumberWidget<u32>);
	REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<u64>,		_u64,		PropertyTree2::CNumberWidget<u64>);
}

namespace Serialization
{
	REGISTER_PROPERTY_WIDGET(IActionButton, PropertyTree2::CActionButtonWidget);
	REGISTER_PROPERTY_WIDGET(Vec3AsColor,	PropertyTree2::CColorwidget);
}

namespace property_tree
{
	REGISTER_PROPERTY_WIDGET(Color, PropertyTree2::CColorwidget);
}

REGISTER_PROPERTY_WIDGET_TEMPLATE(SerializableColor_tpl<float>,			ColorF, PropertyTree2::CColorwidget);
REGISTER_PROPERTY_WIDGET_TEMPLATE(SerializableColor_tpl<unsigned char>, ColorB, PropertyTree2::CColorwidget);

