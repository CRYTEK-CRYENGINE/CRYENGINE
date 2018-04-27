// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemModelAttribute.h"

#include "Controls/QMenuComboBox.h"
#include "Controls/QNumericBox.h"

#include <QLineEdit>
#include <QCheckBox>

//////////////////////////////////////////////////////////////////////////
// Attribute types
//////////////////////////////////////////////////////////////////////////

CItemModelAttributeEnum::CItemModelAttributeEnum(const char* szName, QStringList enumEntries, Visibility visibility, bool filterable, QVariant defaultFilterValue)
	: CItemModelAttribute(szName, eAttributeType_Enum, visibility, filterable, defaultFilterValue)
	, m_enumEntries(enumEntries)
{}

CItemModelAttributeEnum::CItemModelAttributeEnum(const char* szName, char** enumEntries, int enumEntryCount, Visibility visibility, bool filterable, QVariant defaultFilterValue)
	: CItemModelAttribute(szName, eAttributeType_Enum, visibility, filterable, defaultFilterValue)
{
	for (int i = 0; i < enumEntryCount; i++)
	{
		m_enumEntries += enumEntries[i];
	}
}

CItemModelAttributeEnum::CItemModelAttributeEnum(const char* szName, char* commaSeparatedString, Visibility visibility, bool filterable, QVariant defaultFilterValue)
	: CItemModelAttribute(szName, eAttributeType_Enum, visibility, filterable, defaultFilterValue)
{
	QString qstring(commaSeparatedString);
	m_enumEntries = qstring.split(',', QString::SkipEmptyParts);
}

//////////////////////////////////////////////////////////////////////////

CItemModelAttributeEnumFunc::CItemModelAttributeEnumFunc(const char* szName, std::function<QStringList()> populateFunc, Visibility visibility /*= Visibility::Visible*/, bool filterable /*= true*/, QVariant defaultFilterValue /*= QVariant()*/)
	: CItemModelAttributeEnum(szName, QStringList(), visibility, filterable, defaultFilterValue)
	, m_populateFunc(populateFunc)
{}

QStringList CItemModelAttributeEnumFunc::GetEnumEntries() const
{
	if (m_enumEntries.length() == 0)
	{
		const_cast<CItemModelAttributeEnumFunc*>(this)->Populate();
	}

	return m_enumEntries;
}

void CItemModelAttributeEnumFunc::Populate()
{
	m_enumEntries = m_populateFunc();
}

//////////////////////////////////////////////////////////////////////////

namespace Attributes
{
CItemModelAttribute s_nameAttribute("Name", eAttributeType_String);
CItemModelAttribute s_visibleAttribute("Visible", eAttributeType_Boolean);
CItemModelAttribute s_favoriteAttribute("Favorite", eAttributeType_Boolean, CItemModelAttribute::Visible, false);
CItemModelAttribute s_thumbnailAttribute("_thumb_", eAttributeType_String, CItemModelAttribute::AlwaysHidden, false);

//////////////////////////////////////////////////////////////////////////
// Operators
//////////////////////////////////////////////////////////////////////////

class CContainsOperator : public IAttributeFilterOperator
{
public:
	virtual QString GetName() override { return QWidget::tr("Contains"); }

	virtual bool    Match(const QVariant& value, const QVariant& filterValue) override
	{
		QRegExp regexp = filterValue.toRegExp();
		return value.toString().contains(regexp);
	}

	QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> filter) override
	{
		auto widget = new QLineEdit();
		auto currentValue = filter->GetFilterValue();

		if (currentValue.type() == QVariant::String)
		{
			widget->setText(currentValue.toString());
		}

		QWidget::connect(widget, &QLineEdit::editingFinished, [filter, widget]()
			{
				QRegExp regexp;
				regexp.setPatternSyntax(QRegExp::FixedString);
				regexp.setCaseSensitivity(Qt::CaseInsensitive);
				regexp.setPattern(widget->text());
				filter->SetFilterValue(regexp);
		  });

		return widget;
	}

	void UpdateWidget(QWidget* widget, const QVariant& value) override
	{
		QLineEdit* edit = qobject_cast<QLineEdit*>(widget);
		if (edit)
			edit->setText(value.toString());
	}

	void InitFilterValue(QVariant& filterValueStorage) const override
	{
		// Check if the value is already in the proper format.
		if (filterValueStorage.canConvert<QRegExp>())
		{
			// Most likely this should never happen, but for safety.
			return;
		}

		QRegExp regexp;
		regexp.setPatternSyntax(QRegExp::FixedString);
		regexp.setCaseSensitivity(Qt::CaseInsensitive);
		regexp.setPattern(filterValueStorage.toString());
		filterValueStorage = regexp;
	}

	QVariant TranslateFilterValue(const QVariant& filterValueStorage) const override
	{
		QRegExp regexp = filterValueStorage.toRegExp();
		return regexp.pattern();
	}
};

class CBaseIntOperator : public IAttributeFilterOperator
{
public:
	QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> filter) override
	{
		auto widget = new QNumericBox();
		widget->setRestrictToInt();

		auto currentValue = filter->GetFilterValue();

		if (currentValue.isValid())
			widget->setValue(currentValue.toInt());
		else
			widget->setValue(0);

		QWidget::connect(widget, &QNumericBox::valueSubmitted, [=]()
		{
			filter->SetFilterValue((int)widget->value());
		});

		return widget;
	}

	void UpdateWidget(QWidget* widget, const QVariant& value) override
	{
		QNumericBox* editWidget = qobject_cast<QNumericBox*>(widget);
		if (editWidget)
			editWidget->setValue(value.toInt());
	}
};

class CBaseFloatOperator : public IAttributeFilterOperator
{
public:
	QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> filter) override
	{
		auto widget = new QNumericBox();
		auto currentValue = filter->GetFilterValue();

		if (currentValue.isValid())
			widget->setValue(currentValue.toDouble());
		else
			widget->setValue(0);

		QWidget::connect(widget, &QNumericBox::valueSubmitted, [=]()
		{
			filter->SetFilterValue(widget->value());
		});

		return widget;
	}

	void UpdateWidget(QWidget* widget, const QVariant& value) override
	{
		QNumericBox* editWidget = qobject_cast<QNumericBox*>(widget);
		if (editWidget)
			editWidget->setValue(value.toDouble());
	}
};

template<typename Base>
class CEqualOperator : public Base
{
public:
	virtual QString GetName() override { return QWidget::tr("="); }
	virtual bool    Match(const QVariant& value, const QVariant& filterValue) override
	{
		return value == filterValue;
	}
};

template<typename Base>
class CGreaterThanOperator : public Base
{
public:
	virtual QString GetName() override { return QWidget::tr(">"); }
	virtual bool    Match(const QVariant& value, const QVariant& filterValue) override
	{
		return value > filterValue;
	}

	virtual QVariant GetFilterValue() const { return m_filterValue; }

	QVariant m_filterValue;
};

template<typename Base>
class CLessThanOperator : public Base
{
public:
	virtual QString GetName() override { return QWidget::tr("<"); }
	virtual bool    Match(const QVariant& value, const QVariant& filterValue) override
	{
		return value < filterValue;
	}

	virtual QVariant GetFilterValue() const { return m_filterValue; }

	QVariant m_filterValue;
};

class CBoolTestOperator : public IAttributeFilterOperator
{
public:
	virtual QString GetName() override { return QWidget::tr("="); }
	virtual bool    Match(const QVariant& value, const QVariant& filterValue) override
	{
		if (!value.isValid() || !filterValue.isValid())
			return false;

		Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
		switch (state)
		{
		case Qt::Unchecked:
			return filterValue.toBool() == false;
		case Qt::Checked:
		case Qt::PartiallyChecked:
			return filterValue.toBool() == true;
		default:
			assert(0);   //Not handled
			break;
		}

		return false;
	}

	virtual QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> filter) override
	{
		auto widget = new QCheckBox();
		auto currentValue = filter->GetFilterValue();
		widget->setChecked(currentValue.toBool());
		widget->setText(currentValue.toBool() ? "True" : "False");

		QWidget::connect(widget, &QCheckBox::stateChanged, [=](int newState)
			{
				switch (newState)
				{
				case Qt::Unchecked:
					filter->SetFilterValue(false);
					widget->setText("False");
					break;
				case Qt::Checked:
					filter->SetFilterValue(true);
					widget->setText("True");
					break;
				default:
					assert(0); //Not handled
					break;
				}
		  });

		return widget;
	}

	void UpdateWidget(QWidget* widget, const QVariant& value) override
	{
		QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget);
		if (checkBox && value.type() == QVariant::Bool)
		{
			checkBox->setChecked(value.toBool());
			checkBox->setText(value.toBool() ? "True" : "False");
		}
	}
};

class CEnumTestOperator : public IAttributeFilterOperator
{
public:
	virtual QString GetName() override { return QWidget::tr("="); }
	virtual bool    Match(const QVariant& value, const QVariant& filterValue) override
	{
		//TODO : handle do match by integer value if value is a number : implement attribute convert int to string and match string
		QStringList list = filterValue.toStringList();
		if (list.empty())
			return true;
		else
		{
			QString valueStr = value.toString();
			if (valueStr.indexOf(", ") == -1)
				return list.contains(valueStr);
			else
			{
				QStringList valueItems = valueStr.split(", ");
				for (auto& item : valueItems)
				{
					if (list.contains(item))
						return true;
				}
				return false;
			}
		}
	}

	virtual QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> filter) override
	{
		QMenuComboBox* cb = new QMenuComboBox();
		cb->SetMultiSelect(true);
		cb->SetCanHaveEmptySelection(true);

		CItemModelAttributeEnum* attr = static_cast<CItemModelAttributeEnum*>(filter->GetAttribute());
		cb->AddItems(attr->GetEnumEntries());

		QVariant& val = filter->GetFilterValue();
		if (val.isValid())
		{
			bool ok = false;
			int valToI = val.toInt(&ok);
			if (ok)
				cb->SetChecked(valToI);
			else
			{
				QStringList items = val.toString().split(", ");
				cb->SetChecked(items);
			}
		}

		cb->signalItemChecked.Connect([cb, filter]()
			{
				filter->SetFilterValue(cb->GetCheckedItems());
		  });

		return cb;
	}

	void UpdateWidget(QWidget* widget, const QVariant& value) override
	{
		QMenuComboBox* combo = qobject_cast<QMenuComboBox*>(widget);
		if (combo)
		{
			combo->SetChecked(value.toStringList());
		}
	}
};

//////////////////////////////////////////////////////////////////////////

class AttributeFilterOperatorRegistry
{
public:
	AttributeFilterOperatorRegistry()
	{
		//Register all operators

		{
			std::vector<IAttributeFilterOperator*> operators;
			operators.push_back(new CContainsOperator());
			m_registry[eAttributeType_String] = operators;
		}

		{
			std::vector<IAttributeFilterOperator*> operators;
			operators.push_back(new CEqualOperator<CBaseIntOperator>());
			operators.push_back(new CGreaterThanOperator<CBaseIntOperator>());
			operators.push_back(new CLessThanOperator<CBaseIntOperator>());
			m_registry[eAttributeType_Int] = operators;
		}

		{
			std::vector<IAttributeFilterOperator*> operators;
			operators.push_back(new CEqualOperator<CBaseFloatOperator>());
			operators.push_back(new CGreaterThanOperator<CBaseFloatOperator>());
			operators.push_back(new CLessThanOperator<CBaseFloatOperator>());
			m_registry[eAttributeType_Float] = operators;
		}

		{
			std::vector<IAttributeFilterOperator*> operators;
			operators.push_back(new CBoolTestOperator());
			m_registry[eAttributeType_Boolean] = operators;
		}

		{
			std::vector<IAttributeFilterOperator*> operators;
			operators.push_back(new CEnumTestOperator());
			m_registry[eAttributeType_Enum] = operators;
		}
	}

	std::map<EAttributeType, std::vector<IAttributeFilterOperator*>> m_registry;
};

static AttributeFilterOperatorRegistry g_registry;

void GetOperatorsForType(EAttributeType type, std::vector<IAttributeFilterOperator*>& operatorsOut)
{
	operatorsOut = g_registry.m_registry[type];
}

IAttributeFilterOperator* GetDefaultOperatorForType(EAttributeType type)
{
	return g_registry.m_registry[type][0];
}

}

//////////////////////////////////////////////////////////////////////////

CAttributeFilter::CAttributeFilter(CItemModelAttribute* pAttribute)
	: m_pAttribute(pAttribute)
	, m_bEnabled(true)
	, m_pOperator(nullptr)
	, m_inverted(false)
{
	//set operator by default
	if (m_pAttribute)
	{
		m_pOperator = Attributes::GetDefaultOperatorForType(pAttribute->GetType());
	}
}

void CAttributeFilter::SetAttribute(CItemModelAttribute* pAttribute)
{
	if (m_pAttribute != pAttribute)
	{
		m_pAttribute = pAttribute;
		m_FilterValue = pAttribute->GetDefaultFilterValue();
		if (!pAttribute || IsEnabled())
		{
			signalChanged();
		}
	}
}

void CAttributeFilter::SetFilterValue(QVariant value)
{
	if (!m_pOperator)
	{
		return;
	}

	m_pOperator->InitFilterValue(value);

	if (value != m_FilterValue)
	{
		m_FilterValue = value;
		signalChanged();
	}
}

QVariant CAttributeFilter::GetFilterValue() const
{
	return m_pOperator ? m_pOperator->TranslateFilterValue(m_FilterValue) : QVariant();
}

void CAttributeFilter::SetEnabled(bool bChecked)
{
	if (m_bEnabled != bChecked)
	{
		m_bEnabled = bChecked;
		if (!bChecked || IsEnabled())
		{
			signalChanged();
		}
	}
}

bool CAttributeFilter::IsEnabled() const
{
	return m_pAttribute && m_pOperator && m_bEnabled;
}

void CAttributeFilter::SetInverted(bool bInverted)
{
	if (m_inverted != bInverted)
	{
		m_inverted = bInverted;
		signalChanged();
	}
}

bool CAttributeFilter::Match(QVariant value) const
{
	assert(IsEnabled());
	return m_pOperator->Match(value, m_FilterValue) ^ m_inverted;
}

void CAttributeFilter::SetOperator(Attributes::IAttributeFilterOperator* pOperator)
{
	if (m_pOperator != pOperator)
	{
		if (m_pOperator)
		{
			m_FilterValue = m_pOperator->TranslateFilterValue(m_FilterValue);
		}

		m_pOperator = pOperator;

		if (m_pOperator)
		{
			m_pOperator->InitFilterValue(m_FilterValue);
		}

		if (!pOperator || IsEnabled())
		{
			signalChanged();
		}
	}
}

