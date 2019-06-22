// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetBrowserDialog.h"

#include "AssetBrowser.h"
#include "AssetModel.h"
#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetManagerHelpers.h"
#include "AssetSystem/Loader/AssetLoaderHelpers.h"

#include "Controls/QuestionDialog.h"
#include "EditorFramework/PersonalizationManager.h"
#include "FileDialogs/FileNameLineEdit.h"
#include "PathUtils.h"
#include "ProxyModels/AttributeFilterProxyModel.h"
#include "QAdvancedTreeView.h"
#include "QThumbnailView.h"
#include "QtUtil.h"

#include <QAbstractButton>
#include <QDirIterator>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QVariant>

#include <numeric>

namespace Private_AssetBrowserDialog
{

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

} // namespace Private_AssetBrowserDialog

class CAssetBrowserDialog::CBrowser : public CAssetBrowser
{
public:
	CBrowser(const std::vector<CAssetType*>& assetTypes, CAssetBrowserDialog* pOwner, bool bAllowMultipleAssets, QWidget* pParent)
		: CAssetBrowser(assetTypes, !pOwner->IsReadOnlyMode(), pParent)
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

	virtual void OnActivated(CAsset* pAsset) override
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

	virtual void UpdatePreview(const QModelIndex& currentIndex) override
	{
		// The dialog does not do quick edit/preview. 
	}
};

CAssetBrowserDialog::CAssetBrowserDialog(const std::vector<string>& assetTypeNames, Mode mode, QWidget* pParent)
	: CEditorDialog(QStringLiteral("CAssetPicker"), pParent)
	, m_mode(mode)
	, m_overwriteMode(OverwriteMode::AllowOverwrite)
	, m_pBrowser(new CBrowser(AssetManagerHelpers::GetAssetTypesFromTypeNames(assetTypeNames), this, mode == Mode::OpenMultipleAssets, nullptr))
	, m_pAssetType(nullptr)
{
	using namespace Private_AssetBrowserDialog;

	m_pBrowser->Initialize();
	const QString propertyName = QtUtil::ToQString(std::accumulate(assetTypeNames.begin(), assetTypeNames.end(), "Layout"));

	// Load generic asset browser layout valid for all asset types. Some things might be later overwritten by asset type specific personalization
	const QVariantMap& personalization = GetIEditor()->GetPersonalizationManager()->GetState(GetDialogName());
	QVariant layout = personalization.value("layout");
	if (layout.isValid())
	{
		m_pBrowser->SetLayout(layout.toMap());
	}

	AddPersonalizedProjectProperty(propertyName, [this]()
	{
		return m_pBrowser->GetLayout();
	}, [this](const QVariant& variant)
	{
		if (variant.isValid() && variant.canConvert<QVariantMap>())
		{
			QVariantMap map = variant.value<QVariantMap>();
			map.remove("filters");
			// No need to have editor content differing between asset types
			map.remove("editorContent");
			m_pBrowser->SetLayout(map);
		}
	});

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
				assetNames << pAsset->GetName().c_str();
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

	const QString assetTypename(assetTypeNames.size() == 1 ? QtUtil::ToQString(assetTypeNames[0]) : QStringLiteral("asset"));

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

	if (!IsReadOnlyMode())
	{
		CRY_ASSERT(assetTypeNames.size() == 1);
		m_pAssetType = CAssetManager::GetInstance()->FindAssetType(assetTypeNames[0]);
	}

	pPathLayout->addWidget(m_pPathEdit);

	auto* const pMainLayout = new QVBoxLayout();
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(m_pBrowser);
	pMainLayout->addLayout(pPathLayout);
	pMainLayout->addWidget(pButtons, Qt::AlignRight);
	setLayout(pMainLayout);
}
	
void CAssetBrowserDialog::showEvent(QShowEvent* pEvent)
{
	CEditorDialog::showEvent(pEvent);

	m_pPathEdit->setFocus();
}

CAsset* CAssetBrowserDialog::GetSelectedAsset()
{
	return m_pBrowser->GetLastSelectedAsset();
}

std::vector<CAsset*> CAssetBrowserDialog::GetSelectedAssets()
{
	return m_pBrowser->GetSelectedAssets();
}

void CAssetBrowserDialog::SelectAsset(const CAsset& asset)
{
	m_pBrowser->SelectAsset(asset);
}

void CAssetBrowserDialog::SelectAsset(const string& path)
{
	m_pBrowser->SelectAsset(path.c_str());

	if (m_mode != Mode::Create)
	{
		return;
	}

	const QStringList filenames = m_pPathEdit->GetFileNames();
	if (!filenames.isEmpty())
	{
		return;
	}

	// We did not find any existing asset by the specified name, 
	// set the name as the default asset name for the new asset.
	const string assetName(AssetLoader::GetAssetName(path));
	m_pPathEdit->setText(QtUtil::ToQString(assetName));
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

void CAssetBrowserDialog::OnAcceptSave()
{
	using namespace Private_AssetBrowserDialog;

	CRY_ASSERT(m_pAssetType);

	const std::vector<string>& selectedFolders = m_pBrowser->GetSelectedFolders();
	const string folder = selectedFolders.empty() ? string() : selectedFolders.back();

	const QStringList filenames = m_pPathEdit->GetFileNames();
	if (filenames.isEmpty() || filenames.back().isEmpty())
	{
		return;
	}

	const string assetName = QtUtil::ToString(filenames.back());
	const string assetPath = PathUtil::Make(folder, assetName);

	const string cryassetPath = m_pAssetType->MakeMetadataFilename(assetPath.c_str());

	string reasonToReject;
	if (!CAssetType::IsValidAssetPath(cryassetPath.c_str(), reasonToReject))
	{
		CQuestionDialog::SCritical(tr("Asset path is invalid."), QtUtil::ToQString(reasonToReject));
		return;
	}

	const string absFolderPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), folder);
	const std::vector<string> otherAssets = FindAssetMetaDataPaths(absFolderPath);
	const string filename = m_pAssetType->MakeMetadataFilename(assetName.c_str());
	bool bAlreadyExists = false;
	for (const string& otherFilename : otherAssets)
	{
		if (!stricmp(otherFilename.c_str(), filename.c_str()))
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

	m_assetPath = assetPath;

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
