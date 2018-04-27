// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include <CrySchematyc2/GUID.h>

#include "GenericWidget.h"
#include "GenericWidgetModelImpl.h"

#include <QCheckBox>
#include <QGridLayout>

//////////////////////////////////////////////////////////////////////////
namespace Cry {
namespace SchematycEd {

template <class T>
struct CGenericWidget::Unit : public virtual CGenericWidget
{
	Unit() : m_pCheckBox(new QCheckBox(T::Name(), this))
	{
		AddCategory(m_category);
		AddTolayout();		

		QObject::connect(m_pCheckBox, &QCheckBox::stateChanged, [this](int state)
		{
			ReloadCategory(m_category, state == Qt::Checked);
		});
	}

	virtual ~Unit()
	{
		m_pCheckBox->deleteLater();
	}

	template <typename V> inline void Visit(const V& v)
	{
		ReloadCategory(m_category, m_pCheckBox->isChecked());
	}

private:
	inline void AddTolayout()
	{
		m_pGridLayout->addWidget(m_pCheckBox, T::GRID_X, T::GRID_Y);
	}

private:
	QCheckBox*           m_pCheckBox;
	typename T::Category m_category;
};

//////////////////////////////////////////////////////////////////////////
template <class T>
struct CGenericWidget::Impl : public CHierarchy<T, CGenericWidget::Unit>
{
	virtual void LoadClassInternal() override
	{
		CHierarchy<T, CGenericWidget::Unit>::Visit(std::make_tuple("LoadClassInternal"));
	}
};

////////////////////////////////////////////////////////////////////////////
using CGenericWidgetImpl = CGenericWidget::Impl
<
	TYPELIST_5(
		CTypesCategoryTraits,
		CGraphsCategoryTraits,
		CSignalsCategoryTraits,
		CVariablesCategoryTraits,
		CComponentsCategoryTraits
	)
>;

} //namespace SchematycEd
} //namespace Cry
