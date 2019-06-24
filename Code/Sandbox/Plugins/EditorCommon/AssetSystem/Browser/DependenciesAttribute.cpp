// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DependenciesAttribute.h"

#include "AssetModel.h"
#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetResourceSelector.h"

#include "CryIcon.h"

#include <QLineEdit>
#include <QToolButton>
#include <QHBoxLayout>

namespace Attributes
{

class ResourceSelectionCallback : public IResourceSelectionCallback
{
public:
	ResourceSelectionCallback(CAttributeFilter* pFilter, QLineEdit* pLineEdit)
		: m_pFilter(pFilter)
		, m_pLineEdit(pLineEdit)
	{
	}
	virtual void SetValue(const char* szNewValue) override
	{
		const QString path(QtUtil::ToQString(szNewValue));
		m_pLineEdit->setText(path);
		m_pFilter->SetFilterValue(path);
	}
private:
	CAttributeFilter* const m_pFilter;
	QLineEdit* const        m_pLineEdit;
};

QWidget* CDependenciesOperatorBase::CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues)
{
	auto pWidget = new QWidget();

	QLineEdit* const pLineEdit = new QLineEdit();
	auto currentValue = pFilter->GetFilterValue();

	if (currentValue.type() == QVariant::String)
	{
		pLineEdit->setText(currentValue.toString());
	}

	QWidget::connect(pLineEdit, &QLineEdit::editingFinished, [pLineEdit, pFilter]()
		{
			pFilter->SetFilterValue(pLineEdit->text());
		});

	QToolButton* pButton = new QToolButton();
	pButton->setToolTip(QObject::tr("Open"));
	pButton->setIcon(CryIcon("icons:General/Folder.ico"));
	QWidget::connect(pButton, &QToolButton::clicked, [pLineEdit, pFilter]()
		{
			ResourceSelectionCallback callback(pFilter.get(), pLineEdit);
			SResourceSelectorContext context;
			context.callback = &callback;

			const string value(QtUtil::ToString(pLineEdit->text()));

			SResourceSelectionResult result = SStaticAssetSelectorEntry::SelectFromAsset(context, {}, value.c_str());

			if (result.selectionAccepted)
			{
			  callback.SetValue(result.selectedResource.c_str());
			}
			else // restore the previous value
			{
			  callback.SetValue(value.c_str());
			}
		});

	QHBoxLayout* pLayout = new QHBoxLayout();
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(pLineEdit);
	pLayout->addWidget(pButton);
	pWidget->setLayout(pLayout);
	pWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	return pWidget;
}

void CDependenciesOperatorBase::UpdateWidget(QWidget* pWidget, const QVariant& value)
{
	QLineEdit* const pLineEdit = static_cast<QLineEdit*>(pWidget->layout()->itemAt(0)->widget());
	if (pLineEdit)
	{
		pLineEdit->setText(value.toString());
	}
}

bool CUsedBy::Match(const QVariant& value, const QVariant& filterValue)
{
	if (!filterValue.isValid())
	{
		return true;
	}

	const CAsset* const pAsset = value.isValid() ? reinterpret_cast<CAsset*>(value.value<intptr_t>()) : nullptr;
	if (!pAsset)
	{
		return false;
	}

	const string path(QtUtil::ToString(filterValue.toString()));
	return pAsset->IsAssetUsedBy(path).first;
}

std::pair<bool, int> CUsedBy::GetUsageInfo(const CAsset& asset, const string& pathToTest) const
{
	return asset.IsAssetUsedBy(pathToTest.c_str());
}

bool CUse::Match(const QVariant& value, const QVariant& filterValue)
{
	if (!filterValue.isValid())
	{
		return true;
	}

	const CAsset* const pAsset = value.isValid() ? reinterpret_cast<CAsset*>(value.value<intptr_t>()) : nullptr;
	if (!pAsset)
	{
		return false;
	}

	const string path(QtUtil::ToString(filterValue.toString()));
	return pAsset->DoesAssetUse(path).first;
}

std::pair<bool, int> CUse::GetUsageInfo(const CAsset& asset, const string& pathToTest) const
{
	return asset.DoesAssetUse(pathToTest.c_str());
}

static CAttributeType<QString> s_dependenciesAttributeType({ new CUse(), new CUsedBy() });

CDependenciesAttribute::CDependenciesAttribute() : CItemModelAttribute("Dependencies", &s_dependenciesAttributeType, CItemModelAttribute::AlwaysHidden, true, QVariant(), (int)CAssetModel::Roles::InternalPointerRole)
{
	static CAssetModel::CAutoRegisterColumn column(this, [](const CAsset* pAsset, const CItemModelAttribute* /*pAttribute*/, int role)
	{
		return QVariant();
	});
}

CDependenciesAttribute s_dependenciesAttribute;
}