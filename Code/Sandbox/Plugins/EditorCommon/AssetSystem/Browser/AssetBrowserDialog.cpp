// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetBrowserDialog.h"
#include "AssetBrowser.h"
#include "AssetModel.h"
#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"

#include "QtUtil.h"
#include "QAdvancedTreeView.h"
#include "QThumbnailView.h"
#include "ProxyModels/AttributeFilterProxyModel.h"
#include "FileDialogs/FileNameLineEdit.h"
#include "FilePathUtil.h"

#include <QDirIterator>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Private_AssetBrowserDialog
{

QStringList GetUiNamesFromAssetTypeNames(const std::vector<string>& typeNames)
{
	CAssetManager* const pManager = CAssetManager::GetInstance();
	QStringList uiNames;
	uiNames.reserve(typeNames.size());
	for (const string& typeName : typeNames)
	{
		CAssetType* const pType = pManager->FindAssetType(typeName.c_str());
		if (pType)
		{
			uiNames.push_back(pType->GetUiTypeName());
		}
	}
	return uiNames;
}

void SetButtonsEnabled(QDialogButtonBox* pButtons, QDialogButtonBox::ButtonRole role, bool bValue)
{
	for (auto b : pButtons->buttons())
	{
		if (pButtons->buttonRole(b) == role)
		{
			b->setEnabled(bValue);
		}
	}
}

} // namespace Private_AssetBrowserDialog

class CAssetBrowserDialog::CBrowser : public CAssetBrowser
{
public:
	CBrowser(CAssetBrowserDialog* pOwner, bool bAllowMultipleAssets, QWidget* pParent)
		: CAssetBrowser(!pOwner->IsReadOnlyMode(), pParent)
		, m_pOwner(pOwner)
	{
		if (!bAllowMultipleAssets)
		{
			GetDetailsView()->setSelectionMode(QAbstractItemView::SingleSelection);
			GetThumbnailsView()->GetInternalView()->setSelectionMode(QAbstractItemView::SingleSelection);
		}

		GetDetailsView()->setDragEnabled(false);
		GetThumbnailsView()->GetInternalView()->setDragEnabled(false);
	}

	virtual void OnDoubleClick(CAsset* pAsset) override
	{
		m_pOwner->OnAccept();
	}

	void AddFilter(AttributeFilterSharedPtr pFilter)
	{
		GetAttributeFilterProxyModel()->AddFilter(pFilter);
	}

	CAssetBrowserDialog* m_pOwner;

protected:
	void showEvent(QShowEvent* pEvent) override
	{
		// Defer ScrollToSelected until the ThumbnailView calculates icons layout.
		QTimer::singleShot(0, [this]()
		{
			ScrollToSelected();
		});
	}
};

CAssetBrowserDialog::CAssetBrowserDialog(const std::vector<string>& assetTypeNames, Mode mode, QWidget* pParent)
	: CEditorDialog(QStringLiteral("CAssetPicker"), pParent)
	, m_mode(mode)
	, m_overwriteMode(OverwriteMode::AllowOverwrite)
	, m_pBrowser(new CBrowser(this, mode == Mode::OpenMultipleAssets, nullptr))
{
	using namespace Private_AssetBrowserDialog;

	AddPersonalizedProjectProperty("Layout", [this]()
	{
		return m_pBrowser->GetLayout();
	}, [this](const QVariant& variant)
	{
		if (variant.isValid() && variant.canConvert<QVariantMap>())
		{
			QVariantMap map = variant.value<QVariantMap>();
			map.remove("filters");

			m_pBrowser->SetLayout(map);
		}
	});

	m_pAssetTypeFilter = std::make_shared<CAttributeFilter>(&AssetModelAttributes::s_AssetTypeAttribute);
	m_pAssetTypeFilter->SetFilterValue(GetUiNamesFromAssetTypeNames(assetTypeNames));
	m_pBrowser->AddFilter(m_pAssetTypeFilter);

	QDialogButtonBox* pButtons = new QDialogButtonBox();
	pButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	connect(pButtons, &QDialogButtonBox::accepted, this, &CAssetBrowserDialog::OnAccept);
	connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_pBrowser, &CAssetBrowser::SelectionChanged, [pButtons, this]()
	{
		auto assets = m_pBrowser->GetSelectedAssets();
		QStringList assetNames;
		for (CAsset* pAsset : assets)
		{
			if (pAsset)
			{
				assetNames << pAsset->GetName();
			}
		}
		if (!assetNames.empty())
		{
			m_pPathEdit->SetFileNames(assetNames);
		}

		SetButtonsEnabled(pButtons, QDialogButtonBox::ButtonRole::AcceptRole, !assets.empty() || !IsReadOnlyMode());

		SelectionChanged(assets);
	});
	SetButtonsEnabled(pButtons, QDialogButtonBox::ButtonRole::AcceptRole, (m_pBrowser->GetLastSelectedAsset() != nullptr) || !IsReadOnlyMode());

	m_pPathEdit = new CFileNameLineEdit();
	m_pPathEdit->setReadOnly(IsReadOnlyMode()); // TODO :! Allow user-editable paths in the future.

	QHBoxLayout* const pPathLayout = new QHBoxLayout();
	pPathLayout->setObjectName(QStringLiteral("pathLayout"));
	pPathLayout->setContentsMargins(3, 0, 3, 0);

	QString assetTypename(assetTypeNames.size() == 1 ? QtUtil::ToQString(assetTypeNames[0]) : QStringLiteral("asset"));

	switch (m_mode)
	{
	case Mode::OpenSingleAsset:
		setWindowTitle(tr("Select %1").arg(assetTypename));
		pPathLayout->addWidget(new QLabel(tr("Asset path: ")));
		break;
	case Mode::OpenMultipleAssets:
		setWindowTitle(tr("Select asset"));
		pPathLayout->addWidget(new QLabel(tr("Asset path: ")));
		break;
	case Mode::Save:
		setWindowTitle(tr("Save %1").arg(assetTypename));
		pPathLayout->addWidget(new QLabel(tr("Asset name: ")));
		break;
	case Mode::Create:
		setWindowTitle(tr("Create %1").arg(assetTypename));
		pPathLayout->addWidget(new QLabel(tr("New asset name: ")));
		break;
	default:
		CRY_ASSERT_MESSAGE(false, "Unknown dialog mode");
	}

	pPathLayout->addWidget(m_pPathEdit);

	auto* const pMainLayout = new QVBoxLayout();
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(m_pBrowser);
	pMainLayout->addLayout(pPathLayout);
	pMainLayout->addWidget(pButtons, Qt::AlignRight);
	setLayout(pMainLayout);
}

CAsset* CAssetBrowserDialog::GetSelectedAsset()
{
	return m_pBrowser->GetLastSelectedAsset();
}

std::vector<CAsset*> CAssetBrowserDialog::GetSelectedAssets()
{
	return m_pBrowser->GetSelectedAssets().toStdVector();
}

void CAssetBrowserDialog::SelectAsset(const CAsset& asset)
{
	m_pBrowser->SelectAsset(asset);
}

void CAssetBrowserDialog::OnAccept()
{
	if (!IsReadOnlyMode())
	{
		OnAcceptSave();
	}
	else
	{
		accept();
	}
}

static std::vector<string> FindAssetMetaDataPaths(const string& dir)
{
	const size_t dirLen = PathUtil::AddSlash(dir).size();
	std::vector<string> assetPaths;
	QDirIterator iterator(QtUtil::ToQString(dir), QStringList() << "*.cryasset", QDir::Files, QDirIterator::Subdirectories);
	while (iterator.hasNext())
	{
		const string filePath = QtUtil::ToString(iterator.next()).substr(dirLen); // Remove leading path to search directory.
		assetPaths.push_back(filePath);
	}
	return assetPaths;
}

static bool ShowConfirmOverwriteDialog()
{
	CQuestionDialog dialog;
	dialog.SetupQuestion(
	  QObject::tr("Asset already exists."),
	  QObject::tr("Overwrite existing asset?"),
	  QDialogButtonBox::Yes | QDialogButtonBox::No);

	return dialog.Execute() == QDialogButtonBox::Yes;
}

void CAssetBrowserDialog::OnAcceptSave()
{
	const QStringList selectedFolders = m_pBrowser->GetSelectedFolders();
	const QString folder = selectedFolders.isEmpty() ? QString() : selectedFolders.back();

	const QStringList filenames = m_pPathEdit->GetFileNames();
	if (filenames.isEmpty() || filenames.back().isEmpty())
	{
		return;
	}
	const string filename = QtUtil::ToString(filenames.back());

	const string absFolderPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), QtUtil::ToString(folder));
	std::vector<string> otherAssets = FindAssetMetaDataPaths(absFolderPath);
	bool bAlreadyExists = false;
	for (const string& otherAsset : otherAssets)
	{
		// Here we remove two extensions in total. Since each asset meta-data file has a name in the
		// format <name>.<ext>.cryasset, this will give us <name>.
		const string otherAssetName = PathUtil::GetFileName(PathUtil::RemoveExtension(otherAsset));

		if (!stricmp(otherAssetName.c_str(), filename.c_str()))
		{
			bAlreadyExists = true;
			break;
		}
	}

	if (bAlreadyExists)
	{
		if (m_overwriteMode == OverwriteMode::AllowOverwrite)
		{
			if (!ShowConfirmOverwriteDialog())
			{
				return; // Cancelled by user.
			}
		}
		else if (m_overwriteMode == OverwriteMode::NoOverwrite)
		{
			const char* const szWhat =
				"Cannot create new file, because file name already exists."
				"Choose another file name.";
			CQuestionDialog::SWarning(tr("File already exists"), tr(szWhat));
			return;
		}
		else
		{
			CRY_ASSERT(0 && "Unknown overwrite mode");
			return;
		}
	}

	m_assetPath = PathUtil::Make(QtUtil::ToString(folder), filename);

	accept();
}

void CAssetBrowserDialog::SetOverwriteMode(OverwriteMode mode)
{
	m_overwriteMode = mode;
}

bool CAssetBrowserDialog::IsReadOnlyMode() const
{
	return (m_mode == Mode::OpenSingleAsset) || (m_mode == Mode::OpenMultipleAssets);
}

CAsset* CAssetBrowserDialog::OpenSingleAssetForTypes(const std::vector<string>& assetTypeNames)
{
	CAssetBrowserDialog picker(assetTypeNames, Mode::OpenSingleAsset);
	return picker.Execute() ? picker.GetSelectedAsset() : nullptr;
}

std::vector<CAsset*> CAssetBrowserDialog::OpenMultipleAssetsForTypes(const std::vector<string>& assetTypeNames)
{
	CAssetBrowserDialog picker(assetTypeNames, Mode::OpenMultipleAssets);
	return picker.Execute() ? picker.GetSelectedAssets() : std::vector<CAsset*>();
}

string CAssetBrowserDialog::SaveSingleAssetForType(const string& assetTypeName, OverwriteMode overwriteMode)
{
	CAssetBrowserDialog dialog({ assetTypeName }, Mode::Save);
	dialog.SetOverwriteMode(overwriteMode);
	return dialog.Execute() ? dialog.m_assetPath : string();
}

string CAssetBrowserDialog::CreateSingleAssetForType(const string& assetTypeName, OverwriteMode overwriteMode)
{
	CAssetBrowserDialog dialog({ assetTypeName }, Mode::Create);
	dialog.SetOverwriteMode(overwriteMode);
	return dialog.Execute() ? dialog.m_assetPath : string();
}
