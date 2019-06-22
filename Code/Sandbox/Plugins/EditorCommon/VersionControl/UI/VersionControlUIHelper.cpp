// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "VersionControlUIHelper.h"
#include "VersionControl/VersionControlFileStatus.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "Controls/QMenuComboBox.h"

namespace Private_VersionControlUIHelper
{

class CVCSStatusOperator : public Attributes::IAttributeFilterOperator
{
public:
	virtual QString GetName() override { return QWidget::tr(""); }

	virtual bool    Match(const QVariant& value, const QVariant& filterValue) override
	{
		if (!value.isValid()) return false;
		auto filterVal = filterValue.toInt();
		return filterVal == 0 || value.toInt() & filterVal || (value.toInt() == 0 && filterVal & UP_TO_DATE_VAL);
	}

	virtual QWidget* CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues) override
	{
		QMenuComboBox* pComboBox = new QMenuComboBox();
		pComboBox->SetMultiSelect(true);
		pComboBox->SetCanHaveEmptySelection(true);

		pComboBox->AddItem(QObject::tr("Added"), CVersionControlFileStatus::eState_AddedLocally);
		pComboBox->AddItem(QObject::tr("Checked out"), CVersionControlFileStatus::eState_CheckedOutLocally);
		pComboBox->AddItem(QObject::tr("Checked out & Modified"), CVersionControlFileStatus::eState_ModifiedLocally);
		pComboBox->AddItem(QObject::tr("Checked out remotely"), CVersionControlFileStatus::eState_CheckedOutRemotely);
		pComboBox->AddItem(QObject::tr("Updated remotely"), CVersionControlFileStatus::eState_UpdatedRemotely);
		pComboBox->AddItem(QObject::tr("Deleted remotely"), CVersionControlFileStatus::eState_DeletedRemotely);
		pComboBox->AddItem(QObject::tr("Up-to-Date"), UP_TO_DATE_VAL);

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

		pComboBox->signalItemChecked.Connect([this, pComboBox, pFilter]()
		{
			const auto& indices = pComboBox->GetCheckedIndices();
			int sum = 0;
			for (int index : indices)
			{
				sum += pComboBox->GetData(index).toInt();
			}
			pFilter->SetFilterValue(sum);
		});

		return pComboBox;
	}

	virtual void UpdateWidget(QWidget* widget, const QVariant& value) override
	{
		QMenuComboBox* combo = qobject_cast<QMenuComboBox*>(widget);
		if (combo)
		{
			combo->SetChecked(value.toStringList());
		}
	}

private:
	// This is sort of a hack because up-to-date state is represented by 0 bitmask.
	static constexpr int UP_TO_DATE_VAL = 1 << 31;
};

static CAttributeType<int> g_vcsStatusAttributeType({ new CVCSStatusOperator() });
static CItemModelAttribute s_vcsStatusAttribute("Version Control Status", &g_vcsStatusAttributeType, CItemModelAttribute::Visible, true, QVariant(), VersionControlUIHelper::GetVCSStatusRole());

static CAttributeType<QIcon> g_iconAttributeType(nullptr);
static CItemModelAttribute g_vcsIconAttribute("VC status", &g_iconAttributeType, CItemModelAttribute::Visible, false);

}

namespace VersionControlUIHelper
{

QIcon GetIconFromStatus(int status)
{
	using FS = CVersionControlFileStatus;

	static const int notLatestStatus = FS::eState_UpdatedRemotely | FS::eState_DeletedRemotely;
	static const int notTrackedStatus = FS::eState_NotTracked | FS::eState_DeletedLocally;

	if (status & notLatestStatus)
	{
		return QIcon("icons:VersionControl/not_latest.ico");
	}
	else if (status & FS::eState_CheckedOutRemotely)
	{
		return QIcon("icons:VersionControl/locked.ico");
	}
	else if (status & FS::eState_CheckedOutLocally)
	{
		return QIcon(status & FS::eState_ModifiedLocally ? "icons:VersionControl/checked_out_modified.ico"
			: "icons:VersionControl/checked_out.ico");
	}
	else if (status & FS::eState_AddedLocally)
	{
		return QIcon("icons:VersionControl/new.ico");
	}
	else if (status & FS::eState_DeletedLocally)
	{
		return QIcon("icons:VersionControl/deleted.ico");
	}
	else if (status & notTrackedStatus)
	{
		return QIcon("icons:VersionControl/not_tracked.ico");
	}
	return QIcon("icons:VersionControl/latest.ico");
}

int GetVCSStatusRole()
{
	return Qt::UserRole + 20452;
}

CItemModelAttribute* GetVCSStatusAttribute()
{
	return &Private_VersionControlUIHelper::s_vcsStatusAttribute;
}

CItemModelAttribute* GetVCSIconAttribute()
{
	return &Private_VersionControlUIHelper::g_vcsIconAttribute;
}

}