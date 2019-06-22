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
	: CItemModelAttribute(szName, &Attributes::s_enumAttributeType, visibility, filterable, defaultFilterValue)
	, m_enumEntries(enumEntries)
{}

CItemModelAttributeEnum::CItemModelAttributeEnum(const char* szName, char** enumEntries, int enumEntryCount, Visibility visibility, bool filterable, QVariant defaultFilterValue)
	: CItemModelAttribute(szName, &Attributes::s_enumAttributeType, visibility, filterable, defaultFilterValue)
{
	for (int i = 0; i < enumEntryCount; i++)
	{
		m_enumEntries += enumEntries[i];
	}
}

CItemModelAttributeEnum::CItemModelAttributeEnum(const char* szName, char* commaSeparatedString, Visibility visibility, bool filterable, QVariant defaultFilterValue)
	: CItemModelAttribute(szName, &Attributes::s_enumAttributeType, visibility, filterable, defaultFilterValue)
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

	QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues) override
	{
		auto widget = new QLineEdit();
		auto currentValue = pFilter->GetFilterValue();

		if (currentValue.type() == QVariant::String)
		{
			widget->setText(currentValue.toString());
		}

		QWidget::connect(widget, &QLineEdit::editingFinished, [pFilter, widget]()
			{
				QRegExp regexp;
				regexp.setPatternSyntax(QRegExp::FixedString);
				regexp.setCaseSensitivity(Qt::CaseInsensitive);
				regexp.setPattern(widget->text());
				pFilter->SetFilterValue(regexp);
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
	QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues) override
	{
		auto pWidget = new QNumericBox();
		pWidget->setRestrictToInt();

		auto currentValue = pFilter->GetFilterValue();

		if (currentValue.isValid())
			pWidget->setValue(currentValue.toInt());
		else
			pWidget->setValue(0);

		QWidget::connect(pWidget, &QNumericBox::valueSubmitted, [=]()
		{
			pFilter->SetFilterValue((int)pWidget->value());
		});

		return pWidget;
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
	QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues) override
	{
		auto pWidget = new QNumericBox();
		auto currentValue = pFilter->GetFilterValue();

		if (currentValue.isValid())
			pWidget->setValue(currentValue.toDouble());
		else
			pWidget->setValue(0);

		QWidget::connect(pWidget, &QNumericBox::valueSubmitted, [=]()
		{
			pFilter->SetFilterValue(pWidget->value());
		});

		return pWidget;
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

	virtual QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues) override
	{
		auto pWidget = new QCheckBox();
		auto currentValue = pFilter->GetFilterValue();
		pWidget->setChecked(currentValue.toBool());
		pWidget->setText(currentValue.toBool() ? "True" : "False");

		QWidget::connect(pWidget, &QCheckBox::stateChanged, [=](int newState)
			{
				switch (newState)
				{
				case Qt::Unchecked:
					pFilter->SetFilterValue(false);
					pWidget->setText("False");
					break;
				case Qt::Checked:
					pFilter->SetFilterValue(true);
					pWidget->setText("True");
					break;
				default:
					assert(0); //Not handled
					break;
				}
		  });

		return pWidget;
	}

	void UpdateWidget(QWidget* widget, const QVariant& value) override
	{
		QCheckBox* pCheckBox = qobject_cast<QCheckBox*>(widget);
		if (pCheckBox && value.type() == QVariant::Bool)
		{
			pCheckBox->setChecked(value.toBool());
			pCheckBox->setText(value.toBool() ? "True" : "False");
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

	virtual QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues) override
	{
		QMenuComboBox* pComboBox = new QMenuComboBox();
		pComboBox->SetMultiSelect(true);
		pComboBox->SetCanHaveEmptySelection(true);

		if (pAttributeValues)
		{
			pComboBox->AddItems(*pAttributeValues);
		}
		else
		{
			CItemModelAttributeEnum* attr = static_cast<CItemModelAttributeEnum*>(pFilter->GetAttribute());
			pComboBox->AddItems(attr->GetEnumEntries());
		}

		QVariant& val = pFilter->GetFilterValue();
		if (val.isValid())
		{
			bool ok = false;
			int valToI = val.toInt(&ok);
			if (ok)
				pComboBox->SetChecked(valToI);
			else
			{
				QStringList items = val.toString().split(", ");
				pComboBox->SetChecked(items);
			}
		}

		pComboBox->signalItemChecked.Connect([pComboBox, pFilter]()
			{
				pFilter->SetFilterValue(pComboBox->GetCheckedItems());
		  });

		return pComboBox;
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

CAttributeType<QString> s_stringAttributeType(new CContainsOperator());
CAttributeType<bool> s_booleanAttributeType(new CBoolTestOperator());
CAttributeType<int> s_enumAttributeType(new CEnumTestOperator());
CAttributeType<int> s_intAttributeType(
{
	new CEqualOperator<CBaseIntOperator>(),
	new CGreaterThanOperator<CBaseIntOperator>(),
	new CLessThanOperator<CBaseIntOperator>()
});
CAttributeType<float> s_floatAttributeType(
{
	new CEqualOperator<CBaseFloatOperator>(),
	new CGreaterThanOperator<CBaseFloatOperator>(),
	new CLessThanOperator<CBaseFloatOperator>()
});

CItemModelAttribute s_nameAttribute("Name", &s_stringAttributeType);
CItemModelAttribute s_visibleAttribute("Visible", &s_booleanAttributeType, CItemModelAttribute::Visible, true, Qt::Unchecked, Qt::CheckStateRole);
CItemModelAttribute s_favoriteAttribute("Favorite", &s_booleanAttributeType, CItemModelAttribute::Visible, false, Qt::Unchecked, Qt::CheckStateRole);
CItemModelAttribute s_thumbnailAttribute("_thumb_", &s_stringAttributeType, CItemModelAttribute::AlwaysHidden, false);

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
		m_pOperator = m_pAttribute->GetType()->GetDefaultOperator();
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
