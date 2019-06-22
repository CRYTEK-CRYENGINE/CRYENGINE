// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "QtUtil.h"

#include <CrySandbox/CrySignal.h>

#include <QVariant>
#include <QWidget>

#include <qnamespace.h>

namespace Attributes
{
struct IAttributeFilterOperator;
}

struct EDITOR_COMMON_API IAttributeType
{
	virtual std::vector<Attributes::IAttributeFilterOperator*> GetOperators() const = 0;
	virtual Attributes::IAttributeFilterOperator*              GetDefaultOperator() const = 0;
	virtual QVariant                                           ToQVariant(const char* szString) const = 0;

	virtual ~IAttributeType() {}
};

template<typename T>
class EDITOR_COMMON_API CAttributeType : public IAttributeType
{
public:
	virtual std::vector<Attributes::IAttributeFilterOperator*> GetOperators() const override
	{
		return m_operators;
	}

	virtual Attributes::IAttributeFilterOperator* GetDefaultOperator() const override
	{
		return !m_operators.empty() ? m_operators.front() : nullptr;
	}

	virtual QVariant ToQVariant(const char* szValue) const override
	{
		const QVariant v(QtUtil::ToQString(szValue));
		return v.value<T>();
	}

	CAttributeType(const std::vector<Attributes::IAttributeFilterOperator*>& operators)
		: m_operators(operators)
	{
	}

	CAttributeType(Attributes::IAttributeFilterOperator* pOperator)
	{
		m_operators.push_back(pOperator);
	}

	CAttributeType() = delete;
private:
	std::vector<Attributes::IAttributeFilterOperator*> m_operators;
};

class EDITOR_COMMON_API CItemModelAttribute
{
public:

	enum Visibility
	{
		Visible,
		StartHidden,
		AlwaysHidden
	};

	CItemModelAttribute(const char* szName, const IAttributeType* pType, Visibility visibility = Visibility::Visible, bool filterable = true, QVariant defaultFilterValue = QVariant(), int filterRole = Qt::DisplayRole)
		: m_name(szName)
		, m_pType(pType)
		, m_visibility(visibility)
		, m_filterable(filterable)
		, m_defaultVal(defaultFilterValue)
		, m_filterRole(filterRole)
	{}

	virtual ~CItemModelAttribute() {}

	const QString&        GetName() const               { return m_name; }
	const IAttributeType* GetType() const               { return m_pType; }
	Visibility            GetVisibility() const         { return (Visibility)m_visibility; }
	bool                  IsFilterable() const          { return m_filterable; }
	const QVariant&       GetDefaultFilterValue() const { return m_defaultVal; }
	int                   GetFilterRole() const         { return m_filterRole; }

private:
	//TODO : This could have a menu path if we ever have a complex set of attributes to organize in the tree view's menu
	const QString         m_name;
	const IAttributeType* m_pType;
	const uint8           m_visibility;
	const bool            m_filterable;
	const QVariant        m_defaultVal;
	const int             m_filterRole;
};

class EDITOR_COMMON_API CItemModelAttributeEnum : public CItemModelAttribute
{
public:
	CItemModelAttributeEnum(const char* szName, QStringList enumEntries, Visibility visibility = CItemModelAttribute::Visible, bool filterable = true, QVariant defaultFilterValue = QVariant());
	CItemModelAttributeEnum(const char* szName, char** enumEntries, int enumEntryCount, Visibility visibility = CItemModelAttribute::Visible, bool filterable = true, QVariant defaultFilterValue = QVariant());

	//! commaSeparatedString needs to be in the form "value1,value2,value3"
	CItemModelAttributeEnum(const char* szName, char* commaSeparatedString, Visibility visibility = CItemModelAttribute::Visible, bool filterable = true, QVariant defaultFilterValue = QVariant());

	virtual QStringList GetEnumEntries() const { return m_enumEntries; }

protected:
	QStringList m_enumEntries;
};

class EDITOR_COMMON_API CItemModelAttributeEnumFunc : public CItemModelAttributeEnum
{
public:
	CItemModelAttributeEnumFunc(const char* szName, std::function<QStringList()> populateFunc, Visibility visibility = CItemModelAttribute::Visible, bool filterable = true, QVariant defaultFilterValue = QVariant());

	virtual QStringList GetEnumEntries() const override;

private:
	void Populate();
	std::function<QStringList()> m_populateFunc;
};

//! Use this for serializable enum types
template<typename EnumType>
class CItemModelAttributeEnumT : public CItemModelAttributeEnumFunc
{
public:
	CItemModelAttributeEnumT(const char* szName, Visibility visibility = CItemModelAttribute::Visible, bool filterable = true, QVariant defaultFilterValue = QVariant())
		: CItemModelAttributeEnumFunc(szName, WrapMemberFunction(this, &CItemModelAttributeEnumT::PopulateFunc), visibility, filterable, defaultFilterValue)
	{}

	QStringList PopulateFunc()
	{
		QStringList enumEntries;
		auto enumDesc = Serialization::getEnumDescription<EnumType>();
		auto stringList = enumDesc.labels();

		for (auto& str : stringList)
			enumEntries.push_back(str);

		return enumEntries;
	}
};

Q_DECLARE_METATYPE(CItemModelAttribute*)

namespace Attributes
{
//! Set of reusable and common attribute types for many models.
EDITOR_COMMON_API extern CAttributeType<QString> s_stringAttributeType;
EDITOR_COMMON_API extern CAttributeType<int> s_intAttributeType;
EDITOR_COMMON_API extern CAttributeType<float> s_floatAttributeType;
EDITOR_COMMON_API extern CAttributeType<bool> s_booleanAttributeType;
EDITOR_COMMON_API extern CAttributeType<int> s_enumAttributeType;

//! Set of reusable and common attributes for many models.
EDITOR_COMMON_API extern CItemModelAttribute s_nameAttribute;
EDITOR_COMMON_API extern CItemModelAttribute s_visibleAttribute;
EDITOR_COMMON_API extern CItemModelAttribute s_favoriteAttribute;
EDITOR_COMMON_API extern CItemModelAttribute s_thumbnailAttribute;

//! Used as the default role to get attributes. Will be used with headerData(), expects CItemModelAttribute*
constexpr int s_getAttributeRole = Qt::UserRole + 1984;

//! Used to generate a hierarchical view of attributes (ex: the context menu on headers in QAdvancedTreeView). Used with headerData(), expects QString()
constexpr int s_attributeMenuPathRole = Qt::UserRole + 1985;
}

//////////////////////////////////////////////////////////////////////////
// Attribute Filtering
//////////////////////////////////////////////////////////////////////////

class CAttributeFilter;

namespace Attributes
{
struct IAttributeFilterOperator
{
	virtual ~IAttributeFilterOperator() {}
	virtual QString  GetName() = 0;
	virtual bool     Match(const QVariant& value, const QVariant& filterValue) = 0;
	virtual QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues) = 0;
	//TODO : make this method be called on signal filter changed instead of explicitly right now
	virtual void     UpdateWidget(QWidget* widget, const QVariant& value) = 0;
	virtual void     InitFilterValue(QVariant& filterValueStorage) const            {}
	//! This takes the current stored filter value and translates it to be persisted in a state
	virtual QVariant TranslateFilterValue(const QVariant& filterValueStorage) const { return filterValueStorage; }
};

}

class EDITOR_COMMON_API CAttributeFilter
{
public:
	CAttributeFilter(CItemModelAttribute* pAttribute);

	CItemModelAttribute* GetAttribute() const { return m_pAttribute; }
	void                 SetAttribute(CItemModelAttribute* pAttribute);

	void                 SetFilterValue(QVariant value);

	//Use this one to get the semantical value, to exchange between operators
	QVariant                              GetFilterValue() const;

	void                                  SetEnabled(bool bEnabled);
	bool                                  IsEnabled() const;

	void                                  SetInverted(bool bInverted);
	bool                                  IsInverted() const { return m_inverted; }

	bool                                  Match(QVariant value) const;

	void                                  SetOperator(Attributes::IAttributeFilterOperator* pOperator);
	Attributes::IAttributeFilterOperator* GetOperator() const { return m_pOperator; }

	CCrySignal<void()> signalChanged;

private:

	QVariant                              m_FilterValue;
	CItemModelAttribute*                  m_pAttribute;
	Attributes::IAttributeFilterOperator* m_pOperator;
	bool m_bEnabled;
	bool m_inverted;
};

typedef std::shared_ptr<CAttributeFilter> AttributeFilterSharedPtr;
