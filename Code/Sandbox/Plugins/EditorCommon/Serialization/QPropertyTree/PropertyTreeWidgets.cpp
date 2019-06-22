// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTreeWidgets.h"

#include "PropertyTreeModel.h"
#include "Serialization/PropertyTreeLegacy/Color.h"

#include <CryIcon.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/ColorImpl.h>
#include <CrySerialization/Decorators/ActionButton.h>
#include <CrySerialization/yasli/decorators/Button.h>
#include <CrySerialization/yasli/decorators/Range.h>
#include <CrySerialization/yasli/StringList.h>
#include <QAction>
#include <QBoxLayout>
#include <QMenu>
#include <QToolButton>

namespace PropertyTree2
{
CTextWidget::CTextWidget()
	: QLineEdit()
{
	connect(this, &CTextWidget::editingFinished, [this]()
		{
			if (m_previousValue != text())
			{
			  OnChanged();

			  if (hasFocus())
			  {
			    selectAll();
				}
			}
		});
}

void CTextWidget::focusInEvent(QFocusEvent* pEvent)
{
	QLineEdit::focusInEvent(pEvent);
	selectAll();
}

void CTextWidget::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	if (type == yasli::TypeID::get<yasli::StringInterface>())
	{
		yasli::StringInterface* pStringInterface = (yasli::StringInterface*)pValue;
		setText(pStringInterface->get());
	}
	else if (type == yasli::TypeID::get<yasli::WStringInterface>())
	{
		yasli::WStringInterface* pWStringInterface = (yasli::WStringInterface*)pValue;
		setText(QString((const QChar*)pWStringInterface->get()));
	}
	else
	{
		return;
	}

	m_previousValue = text();
}

void CTextWidget::GetValue(void* pValue, const yasli::TypeID& type) const
{
	if (type == yasli::TypeID::get<yasli::StringInterface>())
	{
		yasli::StringInterface* pStringInterface = (yasli::StringInterface*)pValue;
		pStringInterface->set(text().toStdString().c_str());
	}
	else if (type == yasli::TypeID::get<yasli::WStringInterface>())
	{
		yasli::WStringInterface* pWStringInterface = (yasli::WStringInterface*)pValue;
		pWStringInterface->set((wchar_t*)text().data());
	}
}

void CTextWidget::Serialize(Serialization::IArchive& ar)
{
	string widgetText = text().toStdString().c_str();
	ar(widgetText, "value", "Value");
	if (widgetText != m_previousValue && ar.isInput())
	{
		setText(QString(widgetText));
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

void CBoolWidget::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	if (type == yasli::TypeID::get<bool>())
	{
		setChecked(*(bool*)pValue ? Qt::Checked : Qt::Unchecked);
	}
}

void CBoolWidget::GetValue(void* pValue, const yasli::TypeID& type) const
{
	if (type == yasli::TypeID::get<bool>())
	{
		*(bool*)pValue = checkState() == Qt::Checked;
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

void CButtonWidget::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	//Setting the label here as it may change every time this is called, while the row is only set once
	setText(m_rowModel->GetLabel());
}

void CButtonWidget::GetValue(void* pValue, const yasli::TypeID& type) const
{
	//Do nothing
}

class CActionButtonWidget : public CButtonWidget
{
public:
	using CButtonWidget::CButtonWidget;

	void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final
	{
		CButtonWidget::SetValue(pValue, type, ar);
		if (type == yasli::TypeID::get<Serialization::IActionButton>())
		{
			Serialization::IActionButton* pActionButton = (Serialization::IActionButton*)pValue;
			//TODO: optimize it so it doesn't have to do allocate and clone every time!
			m_pActionButton = pActionButton->Clone();
		}
	}

	void Call() final
	{
		m_pActionButton->Callback();
	}

private:
	Serialization::IActionButtonPtr m_pActionButton;
};

class CYasliButtonWidget : public CButtonWidget
{
public:
	CYasliButtonWidget()
		: m_pressed(false)
	{}

	void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final
	{
		if (type == yasli::TypeID::get<yasli::Button>())
		{
			yasli::Button* pYasliButton = (yasli::Button*)pValue;
			setText(pYasliButton->text);
			m_pressed = pYasliButton->pressed;
		}
	}

	void GetValue(void* pValue, const yasli::TypeID& type) const final
	{
		if (type == yasli::TypeID::get<yasli::Button>())
		{
			yasli::Button* pYasliButton = (yasli::Button*)pValue;
			pYasliButton->pressed = m_pressed;
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
	: m_pFactory(nullptr)
{
	//TODO: Disallow mouse wheel movement here as it can be very dangerous
	signalCurrentIndexChanged.Connect(this, &CPtrTypeSelectWidget::OnChanged);
}

void CPtrTypeSelectWidget::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	if (type == yasli::TypeID::get<yasli::PointerInterface>())
	{
		yasli::PointerInterface* pPointerInterface = (yasli::PointerInterface*)pValue;

		//Rebuild the combo if Multiple Value is detected
		bool ok = false;
		const int data = GetData(0).toInt(&ok);
		const bool multipleValues = ok && data == -2;

		if (multipleValues || pPointerInterface->baseType() != m_baseTypeId)
		{
			//Setup the combo box if not set
			m_baseTypeId = pPointerInterface->baseType();
			m_pFactory = pPointerInterface->factory();
			Clear();
			SetupCombo();
		}

		//Select the currently valid entry
		const yasli::TypeDescription* pTypeDescription = m_pFactory->descriptionByRegisteredName(pPointerInterface->registeredTypeName());
		if (pTypeDescription)
		{
			SetText(pTypeDescription->label());
		}
		else   //null pointer
		{
			SetCheckedByData(-1);
		}
	}
}

void CPtrTypeSelectWidget::GetValue(void* pValue, const yasli::TypeID& type) const
{
	if (type == yasli::TypeID::get<yasli::PointerInterface>())
	{
		yasli::PointerInterface* pPointerInterface = (yasli::PointerInterface*)pValue;

		bool ok = false;
		int selectedFactoryIndex = GetCurrentData().toInt(&ok);
		CRY_ASSERT(ok);

		if (selectedFactoryIndex == -2)   //Multiple values, invalid
			return;

		const yasli::TypeDescription* pTypeDescription = selectedFactoryIndex >= 0 ? m_pFactory->descriptionByIndex(selectedFactoryIndex) : nullptr;
		if (pTypeDescription != m_pFactory->descriptionByRegisteredName(pPointerInterface->registeredTypeName()))
		{
			//Create object of new type
			if (pTypeDescription)
			{
				pPointerInterface->create(pTypeDescription->name());   //TODO: ensure that row is expanded next ?! perhaps any row that didn't have children and how has should be expanded ?
			}
			else
			{
				pPointerInterface->create(nullptr);
			}
		}
	}
}

void CPtrTypeSelectWidget::Serialize(Serialization::IArchive& ar)
{
	string text = GetText().toStdString().c_str();
	ar(text, "type", "Pointer Type");
	string typeName = m_baseTypeId.name();
	ar(typeName, "basetype", "Base Type");

	if (ar.isInput() && typeName == m_baseTypeId.name())
	{
		SetText(QString(text));  //This calls OnChanged() if necessary
	}
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
	const size_t count = m_pFactory->size();

	const char* nullLabel = m_pFactory->nullLabel();

	if (nullLabel && *nullLabel)
	{
		AddItem(nullLabel, -1);
	}
	else
	{
		AddItem("[ null ]", -1);
	}

	for (int i = 0; i < count; ++i)
	{
		const yasli::TypeDescription* pTypeDescription = m_pFactory->descriptionByIndex(i);
		AddItem(pTypeDescription->label(), i);
	}
}

//////////////////////////////////////////////////////////////////////////

CArrayWidget::CArrayWidget()
{
	m_pAddElementButton = new QToolButton();
	m_pAddElementButton->setIcon(CryIcon("icons:General/Element_Add.ico"));
	m_pAddElementButton->setToolTip(tr("Add Item"));
	connect(m_pAddElementButton, &QToolButton::clicked, this, &CArrayWidget::OnAddItem);

	QHBoxLayout* pHBoxLayout = new QHBoxLayout();
	pHBoxLayout->addWidget(m_pAddElementButton, 0, Qt::AlignRight | Qt::AlignVCenter);
	pHBoxLayout->setMargin(0);
	setLayout(pHBoxLayout);

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setCursor(Qt::ArrowCursor);
}

void CArrayWidget::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	CRY_ASSERT(type == yasli::TypeID::get<yasli::ContainerInterface>());
	yasli::ContainerInterface* pContainer = (yasli::ContainerInterface*)pValue;

	m_isFixedSize = pContainer->isFixedSize();
	m_ordered = pContainer->isOrdered();
	m_pAddElementButton->setVisible(!m_isFixedSize);
	m_action = Action();
}

void CArrayWidget::GetValue(void* pValue, const yasli::TypeID& type) const
{
	CRY_ASSERT(type == yasli::TypeID::get<yasli::ContainerInterface>());
	yasli::ContainerInterface* pContainer = (yasli::ContainerInterface*)pValue;

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

void CArrayWidget::ExecuteAction(yasli::ContainerInterface* pContainerInterface) const
{
	switch (m_action.type)
	{
	case InsertItem:
		if (!m_isFixedSize)
		{
			pContainerInterface->begin();
			for (int i = 0; i < m_action.arg1; i++)
			{
				pContainerInterface->next();
			}

			pContainerInterface->insert();
		}
		break;
	case MoveItem:
		pContainerInterface->begin();
		for (int i = 0; i < m_action.arg1; i++)
		{
			pContainerInterface->next();
		}

		pContainerInterface->move(m_action.arg2);
		break;
	case DeleteItem:
		if (!m_isFixedSize)
		{
			pContainerInterface->begin();
			for (int i = 0; i < m_action.arg1; i++)
			{
				pContainerInterface->next();
			}

			pContainerInterface->remove();
		}
		break;
	case Resize:
		if (!m_isFixedSize)
		{
			pContainerInterface->resize(m_action.arg1);
		}
		break;
	default:
		break;
	}
}

bool CArrayWidget::IsArrayMutable() const
{
	return !m_isFixedSize;
}

void CArrayWidget::PopulateContextMenu(QMenu* pMenu, const PropertyTree2::CRowModel* pRow)
{
	if (!m_isFixedSize)
	{
		QAction* pAction = pMenu->addAction(tr("Add Item"));
		connect(pAction, &QAction::triggered, this, &CArrayWidget::OnAddItem);

		pAction = pMenu->addAction(tr("Clear All"));
		connect(pAction, &QAction::triggered, this, &CArrayWidget::OnClearAll);
	}
}

void CArrayWidget::PopulateChildContextMenu(QMenu* pMenu, const PropertyTree2::CRowModel* pRow)
{
	CRY_ASSERT(pRow->GetParent() == m_rowModel);

	//We know that arrays don't have hidden rows so index is usable directly
	const int index = pRow->GetIndex();

	if (!m_isFixedSize)
	{
		QAction* pAction = pMenu->addAction(tr("Delete"));
		connect(pAction, &QAction::triggered, [this, index]()
			{
				m_action = { ActionType::DeleteItem, index, -1 };
				OnChanged();
			});
	}

	if (index > 0)
	{
		QAction* pAction = pMenu->addAction(tr("Move Up"));
		connect(pAction, &QAction::triggered, [this, pRow, index]()
			{
				m_action = { ActionType::MoveItem, index, index - 1 };
				OnChanged();
			});
	}

	if (index < pRow->GetParent()->GetChildren().size() - 1)
	{
		QAction* pAction = pMenu->addAction(tr("Move Down"));
		connect(pAction, &QAction::triggered, [this, pRow, index]()
			{
				m_action = { ActionType::MoveItem, index, index + 1 };
				OnChanged();
			});
	}

	if (!m_isFixedSize)
	{
		QAction* pAction = pMenu->addAction(tr("Add Above"));
		connect(pAction, &QAction::triggered, [this, pRow, index]()
			{
				m_action = { ActionType::InsertItem, index, -1 };
				OnChanged();
			});

		pAction = pMenu->addAction(tr("Add Below"));
		connect(pAction, &QAction::triggered, [this, pRow, index]()
			{
				m_action = { ActionType::InsertItem, index + 1, -1 };
				OnChanged();
			});

		pMenu->addSeparator();

		pAction = pMenu->addAction(tr("Clear All"));
		connect(pAction, &QAction::triggered, this, &CArrayWidget::OnClearAll);
	}
}

void CArrayWidget::SetMultiEditValue()
{
	QString label = m_rowModel->GetLabel();
	const int index = label.lastIndexOf('(');
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
			if (sizeFound && !m_isFixedSize && size != m_rowModel->GetChildren().size())
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
}

void CStringListWidget::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	if (type == yasli::TypeID::get<yasli::StringListValue>())
	{
		const yasli::StringListValue& stringListValue = *(yasli::StringListValue*)pValue;
		SetupCombo(stringListValue);
	}
	else if (type == yasli::TypeID::get<yasli::StringListStaticValue>())
	{
		const yasli::StringListStaticValue& stringListStaticValue = *(yasli::StringListStaticValue*)pValue;
		SetupCombo(stringListStaticValue);
	}
}

void CStringListWidget::GetValue(void* pValue, const yasli::TypeID& type) const
{
	//This will be called on serialization of data into archive which happens just after item selection
	//this serialization is called on EVERY item in the property tree, so we must make sure to load the proper index even if we are lazy loading
	//aka. the widget popup menu is not shown yet
	if (type == yasli::TypeID::get<yasli::StringListValue>())
	{
		yasli::StringListValue& stringListValue = *(yasli::StringListValue*)pValue;
		//If lazy loading is complete (popup has been show at least once) return the checked item, if not return m_selected
		//this is because the checked item will be zero as we load only one entry of the menu (m_selected has the correct index)
		if (m_loadComplete)
		{
			stringListValue = GetCheckedItem();
		}
		else
		{
			stringListValue = m_selected;
		}
	}
	else if (type == yasli::TypeID::get<yasli::StringListStaticValue>())
	{
		//If lazy loading is complete (popup has been show at least once) return the checked item, if not return m_selected
		//this is because the checked item will be zero as we load only one entry of the menu (m_selected has the correct index)
		yasli::StringListStaticValue& staticValue = *(yasli::StringListStaticValue*)pValue;
		if (m_loadComplete)
		{
			staticValue = GetCheckedItem();
		}
		else
		{
			staticValue = m_selected;
		}
	}
}

void CStringListWidget::Serialize(Serialization::IArchive& ar)
{
	string listValue = GetText().toStdString().c_str();
	ar(listValue, "value", "Value");

	//Called when we are receiving data from a serializable wrapper (aka copy paste data)
	if (ar.isInput())
	{
		//If all the items have been loaded in the widget we can just SetText as the value will be available
		if (m_loadComplete)
		{
			SetText(QString(listValue));
		}
		else  //let's see if a valid value is being set, clear the previously set one, replace it and finally serialize to out archive
		{
			for (int i = 0; i < m_values.size(); i++)
			{
				if (listValue == m_values[i])
				{
					Clear();
					m_selected = i;
					//this will automatically call SetChecked and consequently pt2 OnChange code that will cause serialization to archive
					//we need to have all info that will be serialized into archive ready by this point, aka m_selected needs to be set to correct index
					AddItem(QString(listValue));
					break;
				}
			}
		}
	}
}

void CStringListWidget::SetMultiEditValue()
{
	AddItem("Multiple Values");
	SetText("Multiple Values");
}

void CStringListWidget::ShowPopup()
{
	signalCurrentIndexChanged.DisconnectObject(this);

	bool shouldInit = false;

	//is it ready or do we have to reload it
	if (m_values.size() != GetItemCount())
	{
		shouldInit = true;
	}
	else
	{
		for (int i = 0; i < m_values.size(); i++)
		{
			if (m_values[i] != GetItem(i))
			{
				shouldInit = true;
				break;
			}
		}
	}

	if (shouldInit)
	{
		Clear();
		//Actually add the items to the popup
		AddItems(m_values);
		m_loadComplete = true;
	}

	SetChecked(m_selected);
	signalCurrentIndexChanged.Connect(this, &CStringListWidget::OnChanged);

	QMenuComboBox::ShowPopup();
}

template<typename List>
void PropertyTree2::CStringListWidget::SetupCombo(const List& list)
{
	//NOTE: serialization to archive is disabled from now on
	//This means that all the operations happening into this scope will not generate property tree events and consequently will not trigger serialization into/from archives
	signalCurrentIndexChanged.DisconnectObject(this);

	bool shouldInit = false;

	auto stringList = list.stringList();
	if (stringList.size() != m_values.size())
	{
		shouldInit = true;
	}
	else
	{
		for (int i = 0; i < m_values.size(); i++)
		{
			if (m_values[i] != stringList[i])
			{
				shouldInit = true;
				break;
			}
		}
	}

	//Cache the list of strings in m_values but don't register them with the menu combo box (AddItem)
	//this is done first time ShowPopup is called and it's skipped here because it's a very expensive operation
	if (shouldInit)
	{
		Clear();
		m_values.clear();
		for (int i = 0; i < stringList.size(); i++)
		{
			m_values.push_back(QString(stringList[i]));
		}
		//only add and select the value we are actually going to see in the preview
		AddItem(m_values[list.index()]);
		m_loadComplete = false;
	}
	else if (m_selected != list.index()) //The list is the same size with the same items but the selected value has changed (for example this happens if we are undoing)
	{
		//values are already loaded into combo box, we just need to check them
		if (m_loadComplete)
		{
			SetChecked(list.index());
		}
		else //we need to reset the value shown in the combo box, this requires reload
		{
			Clear();
			AddItem(m_values[list.index()]);
		}
	}

	m_selected = list.index();

	//NOTE: serialization to archive is reenabled here
	signalCurrentIndexChanged.Connect(this, &CStringListWidget::OnChanged);
}

CColorwidget::CColorwidget()
	: m_multiEdit(false)
{
	signalColorChanged.Connect(this, &CColorwidget::OnChanged);
	signalColorContinuousChanged.Connect(this, &CColorwidget::OnContinuousChanged);
}

void CColorwidget::SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar)
{
	m_multiEdit = false;
	if (type == yasli::TypeID::get<SerializableColor_tpl<unsigned char>>())
	{
		const SerializableColor_tpl<unsigned char>& value = *(SerializableColor_tpl<unsigned char>*)pValue;
		SetColor(value);
		SetHasAlpha(true);
	}
	else if (type == yasli::TypeID::get<SerializableColor_tpl<float>>())
	{
		const SerializableColor_tpl<float>& value = *(SerializableColor_tpl<float>*)pValue;
		SetColor(value);
		SetHasAlpha(true);
	}
	else if (type == yasli::TypeID::get<Serialization::Vec3AsColor>())
	{
		const Serialization::Vec3AsColor& value = *(Serialization::Vec3AsColor*)pValue;
		SetColor(QColor(value.v.x, value.v.y, value.v.z));
		SetHasAlpha(false);
	}
	else if (type == yasli::TypeID::get<SerializableColor_tpl<property_tree::Color>>())
	{
		const property_tree::Color& value = *(property_tree::Color*)pValue;
		SetColor(QColor(value.r, value.g, value.b, value.a));
	}
}

void CColorwidget::GetValue(void* pValue, const yasli::TypeID& type) const
{
	if (type == yasli::TypeID::get<SerializableColor_tpl<unsigned char>>())
	{
		ColorB& value = *(SerializableColor_tpl<unsigned char>*)pValue;
		value = GetColorB();
	}
	else if (type == yasli::TypeID::get<SerializableColor_tpl<float>>())
	{
		ColorF& value = *(SerializableColor_tpl<float>*)pValue;
		value = GetColorF();
	}
	else if (type == yasli::TypeID::get<Serialization::Vec3AsColor>())
	{
		Serialization::Vec3AsColor& value = *(Serialization::Vec3AsColor*)pValue;
		const QColor color = GetQColor();
		value.v.x = color.red();
		value.v.y = color.green();
		value.v.z = color.blue();
	}
	else if (type == yasli::TypeID::get<SerializableColor_tpl<property_tree::Color>>())
	{
		property_tree::Color& value = *(property_tree::Color*)pValue;
		const QColor color = GetQColor();
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
REGISTER_PROPERTY_WIDGET(StringInterface, PropertyTree2::CTextWidget);
REGISTER_PROPERTY_WIDGET(WStringInterface, PropertyTree2::CTextWidget);
REGISTER_PROPERTY_WIDGET(PointerInterface, PropertyTree2::CPtrTypeSelectWidget);
REGISTER_PROPERTY_WIDGET(Button, PropertyTree2::CYasliButtonWidget);
REGISTER_PROPERTY_WIDGET(ContainerInterface, PropertyTree2::CArrayWidget);
REGISTER_PROPERTY_WIDGET(StringListStaticValue, PropertyTree2::CStringListWidget)
REGISTER_PROPERTY_WIDGET(StringListValue, PropertyTree2::CStringListWidget)

REGISTER_PROPERTY_WIDGET(float, PropertyTree2::CNumberWidget<float> );
REGISTER_PROPERTY_WIDGET(double, PropertyTree2::CNumberWidget<double> );
REGISTER_PROPERTY_WIDGET(char, PropertyTree2::CNumberWidget<char> );

REGISTER_PROPERTY_WIDGET(i8, PropertyTree2::CNumberWidget<i8> );
REGISTER_PROPERTY_WIDGET(i16, PropertyTree2::CNumberWidget<i16> );
REGISTER_PROPERTY_WIDGET(i32, PropertyTree2::CNumberWidget<i32> );
REGISTER_PROPERTY_WIDGET(i64, PropertyTree2::CNumberWidget<i64> );

REGISTER_PROPERTY_WIDGET(u8, PropertyTree2::CNumberWidget<u8> );
REGISTER_PROPERTY_WIDGET(u16, PropertyTree2::CNumberWidget<u16> );
REGISTER_PROPERTY_WIDGET(u32, PropertyTree2::CNumberWidget<u32> );
REGISTER_PROPERTY_WIDGET(u64, PropertyTree2::CNumberWidget<u64> );

REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<float>, _float, PropertyTree2::CNumberWidget<float> );
REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<double>, _double, PropertyTree2::CNumberWidget<double> );
REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<char>, _char, PropertyTree2::CNumberWidget<char> );

REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<i8>, _i8, PropertyTree2::CNumberWidget<i8> );
REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<i16>, _i16, PropertyTree2::CNumberWidget<i16> );
REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<i32>, _i32, PropertyTree2::CNumberWidget<i32> );
REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<i64>, _i64, PropertyTree2::CNumberWidget<i64> );

REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<u8>, _u8, PropertyTree2::CNumberWidget<u8> );
REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<u16>, _u16, PropertyTree2::CNumberWidget<u16> );
REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<u32>, _u32, PropertyTree2::CNumberWidget<u32> );
REGISTER_PROPERTY_WIDGET_TEMPLATE(RangeDecorator<u64>, _u64, PropertyTree2::CNumberWidget<u64> );
}

namespace Serialization
{
REGISTER_PROPERTY_WIDGET(IActionButton, PropertyTree2::CActionButtonWidget);
REGISTER_PROPERTY_WIDGET(Vec3AsColor, PropertyTree2::CColorwidget);
}

namespace property_tree
{
REGISTER_PROPERTY_WIDGET(Color, PropertyTree2::CColorwidget);
}

REGISTER_PROPERTY_WIDGET_TEMPLATE(SerializableColor_tpl<float>, ColorF, PropertyTree2::CColorwidget);
REGISTER_PROPERTY_WIDGET_TEMPLATE(SerializableColor_tpl<unsigned char>, ColorB, PropertyTree2::CColorwidget);
