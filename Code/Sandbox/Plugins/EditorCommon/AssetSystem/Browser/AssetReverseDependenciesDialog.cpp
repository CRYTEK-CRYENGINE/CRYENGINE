// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetReverseDependenciesDialog.h"
#include "AssetModel.h"
#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"

#include "QtUtil.h"
#include "QAdvancedTreeView.h"
#include <QDialogButtonBox>
#include <QBoxLayout>
#include <QGroupBox>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QSplitter>
#include <QLabel>

namespace Private_AssetReverseDependenciesDialog
{

class CFilteredAssetsModel : public QSortFilterProxyModel
{
public:
	CFilteredAssetsModel(std::vector<CAsset*>&& assets)
		: m_assets(std::move(assets))
	{
		std::sort(m_assets.begin(), m_assets.end());
		setSourceModel(CAssetModel::GetInstance());
	}

	std::vector<CAsset*> GetDependentAssets() const
	{
		std::unordered_set<CAsset*> set;
		for (CAsset* pAsset : m_assets)
		{
			std::vector<std::pair<CAsset*,int32>> tmp = CAssetManager::GetInstance()->GetReverseDependencies(*pAsset);
			std::transform(tmp.cbegin(), tmp.cend(), std::inserter(set, set.begin()), [](auto& x) 
			{
				return x.first;
			});
		}

		std::vector<CAsset*> v;
		v.reserve(set.size());
		std::copy_if(set.cbegin(), set.cend(), std::back_inserter(v), [this](const CAsset* x)
		{
			// The assets to be deleted are not considered as dependent assets.
			return !Contains(x);
		});
		return v;
	}

	std::vector<CAsset*> GetDependentAssets(QModelIndexList& selectedIndexes) const
	{
		std::unordered_set<CAsset*> set;
		for (const QModelIndex& index : selectedIndexes)
		{
			CAsset* pAsset = ToAsset(index);
			if (!pAsset)
			{
				continue;
			}
			std::vector<std::pair<CAsset*, int32>> tmp = CAssetManager::GetInstance()->GetReverseDependencies(*pAsset);
			std::transform(tmp.cbegin(), tmp.cend(), std::inserter(set, set.begin()), [](auto& x)
			{
				return x.first;
			});
		}
		std::vector<CAsset*> v;
		v.reserve(set.size());
		std::copy_if(set.cbegin(), set.cend(), std::back_inserter(v), [this](const CAsset* x)
		{
			return !Contains(x);
		});
		return v;
	}

	void SetAssets(std::vector<CAsset*>&& assets)
	{
		beginResetModel();
		m_assets = std::move(assets);
		std::sort(m_assets.begin(), m_assets.end());
		endResetModel();
	}

protected:
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
	{
		MAKE_SURE(sourceModel(), return false);

		auto index = sourceModel()->index(sourceRow, EAssetColumns::eAssetColumns_Uid, sourceParent);
		return std::binary_search(m_assets.cbegin(), m_assets.cend(), ToAsset(index));
	}

	CAsset* ToAsset(const QModelIndex& index) const
	{
		QVariant variant = index.data((int)CAssetModel::Roles::InternalPointerRole);
		if (!variant.isValid())
		{
			return nullptr;
		}
		return reinterpret_cast<CAsset*>(variant.value<intptr_t>());
	}

	bool Contains(const CAsset* pAsset) const
	{
		return std::binary_search(m_assets.cbegin(), m_assets.cend(), pAsset);
	}
private:
	std::vector<CAsset*> m_assets;
};

static QAdvancedTreeView* CreateAssetsView(QAbstractItemModel* pModel, QAbstractItemView::SelectionMode selectionMode)
{
	QAdvancedTreeView* pView = new QAdvancedTreeView(QAdvancedTreeView::UseItemModelAttribute);
	pView->setModel(pModel);
	pView->setSelectionMode(selectionMode);
	pView->setSelectionBehavior(QAbstractItemView::SelectRows);
	pView->setUniformRowHeights(true);
	pView->sortByColumn((int)eAssetColumns_Name, Qt::AscendingOrder);
	pView->header()->setStretchLastSection(false);
	pView->header()->resizeSection((int)eAssetColumns_Name, pView->fontMetrics().width(QStringLiteral("wwwwwwwwwwwwwwwwwwwwwwwwww")));
	pView->header()->resizeSection((int)eAssetColumns_Type, pView->fontMetrics().width(QStringLiteral("wwwwwwwwwwwwwww")));
	pView->setTreePosition((int)eAssetColumns_Name);
	pView->setItemsExpandable(false);
	pView->setRootIsDecorated(false);
	return pView;
}

static QWidget* CreateGroup(const QString& title, std::initializer_list<QWidget*> widgets)
{
	QVBoxLayout* const pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);
	for (const auto widget : widgets)
	{
		pLayout->addWidget(widget);
	}

	QGroupBox* const pGroup = new QGroupBox(title);
	pGroup->setFlat(true);
	pGroup->setContentsMargins(0, 0, 0, 0);
	pGroup->setLayout(pLayout);
	return pGroup;
}

static QWidget* CreateInfoBox(const QString& infoText)
{
	QWidget* pInfoBox = new QWidget();

	QLabel* pIconLabel = new QLabel();
	pIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	const int iconSize = 48;
	pIconLabel->setPixmap(CryIcon("icons:Dialogs/dialog-warning.ico").pixmap(iconSize, iconSize, QIcon::Normal, QIcon::On));

	QLabel* pInfoLabel = new QLabel(infoText);
	pInfoLabel->setWordWrap(true);

	QLayout* const pLayout = new QHBoxLayout();
	pLayout->addWidget(pIconLabel);
	pLayout->addWidget(pInfoLabel);

	pInfoBox->setLayout(pLayout);
	return pInfoBox;
}

}

CAssetReverseDependenciesDialog::CAssetReverseDependenciesDialog(
	const QVector<CAsset*>& assets,
	const QString& assetsGroupTitle,
	const QString& dependentAssetsGroupTitle,
	const QString& dependentAssetsInfoText,
	const QString& question,
	QWidget* pParent)
	: CEditorDialog("CAssetReverseDependenciesDialog", pParent)
{
	using namespace Private_AssetReverseDependenciesDialog;

	auto pAssetModel = new CFilteredAssetsModel(std::move(assets.toStdVector()));
	auto pDependentAssetModel = new CFilteredAssetsModel(std::move(pAssetModel->GetDependentAssets()));
	m_models.emplace_back(pAssetModel);
	m_models.emplace_back(pDependentAssetModel);

	QAdvancedTreeView* const pAssetsView = CreateAssetsView(pAssetModel, QAbstractItemView::ExtendedSelection);
	pAssetsView->SetColumnVisible(EAssetColumns::eAssetColumns_Favorite, false);

	QAdvancedTreeView* const pDependentAssetsView = CreateAssetsView(pDependentAssetModel, QAbstractItemView::NoSelection);
	pDependentAssetsView->SetColumnVisible(EAssetColumns::eAssetColumns_Favorite, false);
	pDependentAssetsView->SetColumnVisible(EAssetColumns::eAssetColumns_Status, false);
	pDependentAssetsView->SetColumnVisible(EAssetColumns::eAssetColumns_Folder, true);

	auto updateDependentAssets = [this, pDependentAssetModel, pAssetModel, pAssetsView]()
	{
		if (pAssetsView->selectionModel()->hasSelection())
		{
			auto selection = pAssetsView->selectionModel()->selectedRows(eAssetColumns_Name);
			pDependentAssetModel->SetAssets(pAssetModel->GetDependentAssets(selection));
		}
		else
		{
			pDependentAssetModel->SetAssets(pAssetModel->GetDependentAssets());
		}
	};
	connect(pAssetsView->selectionModel(), &QItemSelectionModel::selectionChanged, [updateDependentAssets](auto)
	{
		updateDependentAssets();
	});

	QWidget* const pAssetsGroup = CreateGroup(assetsGroupTitle, { pAssetsView });

	QWidget* pInfoBox = CreateInfoBox(dependentAssetsInfoText);
	QWidget* const pDependentAssetsGroup = CreateGroup(dependentAssetsGroupTitle, { pInfoBox, pDependentAssetsView });

	QSplitter* const pSplitter = new QSplitter();
	pSplitter->setOrientation(Qt::Vertical);
	pSplitter->addWidget(pAssetsGroup);
	pSplitter->addWidget(pDependentAssetsGroup);

	QVBoxLayout* const pMainLayout = new QVBoxLayout();
	pMainLayout->setObjectName("mainLayout");
	pMainLayout->setContentsMargins(3, 3, 3, 3);
	pMainLayout->addWidget(pSplitter);

	if (!question.isEmpty())
	{
		QLabel* const pQuestion = new QLabel(question);
		pQuestion->setObjectName("questionLabel");
		pQuestion->setContentsMargins(10, 0, 10, 0);
		QDialogButtonBox* const pButtons = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No);
		connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

		pMainLayout->addWidget(pQuestion);
		pMainLayout->addWidget(pButtons);
	}

	setLayout(pMainLayout);
}

