// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EnvironmentAssetType.h"

#include <AssetSystem/AssetEditor.h>
#include <CryEditDoc.h>
#include <FileUtils.h>
#include <IEditor.h>
#include <Notifications/NotificationCenter.h>
#include <PathUtils.h>
#include <ThreadingUtils.h>

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/ISystem.h>

namespace Private_EnvironmentAssetType
{

class CXmlPresetsImporter : public IAutoEditorNotifyListener
{
public:
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override
	{
		if (event != eNotify_OnMainFrameInitialized)
		{
			return;
		}

		GetIEditor()->UnregisterNotifyListener(this);

		ThreadingUtils::AsyncQueue([]()
			{
				const std::vector<string> presets = CollectFilesToConvert();
				if (presets.empty())
				{
				  return;
				}

				CProgressNotification notif(QObject::tr("Importing environment presets"), "", true);
				for (size_t i = 0, n = presets.size(); i < n; ++i)
				{
				  const string dataFilename = PathUtil::ReplaceExtension(presets[i], "env");
				  FileUtils::Pak::CopyFileAllowOverwrite(presets[i], dataFilename);
				  notif.SetProgress(float(i + 1) / n);
				}
			});
	}

private:
	static std::vector<string> CollectFilesToConvert()
	{
		std::vector<string> presets;

		SDirectoryEnumeratorHelper enumerator;
		enumerator.ScanDirectoryRecursive("", "libs/environmentpresets", "*.xml", presets);

		// It seems ScanDirectoryRecursive can return *.xml* files. Remove them from the list, if any.
		presets.erase(std::remove_if(presets.begin(), presets.end(), [](const string& x)
			{
				if (stricmp(PathUtil::GetExt(x.c_str()), "xml") != 0)
				{
				  return true;
				}

				// Do not convert already converted presets.
				if (GetISystem()->GetIPak()->IsFileExist(PathUtil::ReplaceExtension(x, "env")))
				{
				  return true;
				}

				// Check if the xml file is an environment preset.
				XmlNodeRef container = GetISystem()->LoadXmlFromFile(x);
				if (strcmp(container->getTag(), "EnvironmentPreset") != 0)
				{
				  return true;
				}

				return false;
			}), presets.end());

		return presets;
	}
};

}

REGISTER_ASSET_TYPE(CEnvironmentAssetType)

void CEnvironmentAssetType::AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* pMenu) const
{
	if (assets.size() != 1)
	{
		return;
	}

	CAsset* const pAsset = assets.front();

	QAction* pAction = pMenu->CreateAction(QObject::tr("Preview Environment Asset"));
	pAction->setDisabled(!GetIEditor()->GetDocument()->IsDocumentReady());
	QObject::connect(pAction, &QAction::triggered, [pAsset]()
	{
		GetISystem()->GetI3DEngine()->GetTimeOfDay()->PreviewPreset(pAsset->GetFile(0));
	});

	pAction = pMenu->CreateAction(QObject::tr("Set as Default for this Level"));
	pAction->setDisabled(!GetIEditor()->GetDocument()->IsDocumentReady());
	QObject::connect(pAction, &QAction::triggered, [pAsset]()
	{
		ITimeOfDay* const pTimeOfDay = GetISystem()->GetI3DEngine()->GetTimeOfDay();
		if (!pTimeOfDay->LoadPreset(pAsset->GetFile(0)))
		{
		    CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Can not load asset file: %s. Please check if the file exists and valid.", pAsset->GetFile(0));
		    return;
		}
		pTimeOfDay->SetDefaultPreset(pAsset->GetFile(0));
	});
}

void CEnvironmentAssetType::OnInit()
{
	using namespace Private_EnvironmentAssetType;
	static CXmlPresetsImporter importer;

	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.Connect([this](const std::vector<CAsset*>& assets)
	{
		for (const CAsset* pAsset : assets)
		{
			if (pAsset->GetType() != this)
			{
				continue;
			}
			ITimeOfDay* const pTimeOfDay = GetISystem()->GetI3DEngine()->GetTimeOfDay();
			pTimeOfDay->RemovePreset(pAsset->GetFile(0));
		}
	});
}

bool CEnvironmentAssetType::OnCreate(INewAsset& asset, const SCreateParams* pCreateParams) const
{
	const string dataFilename = PathUtil::RemoveExtension(asset.GetMetadataFile());
	asset.SetFiles({ dataFilename });
	ITimeOfDay* const pTimeOfDay = GetISystem()->GetI3DEngine()->GetTimeOfDay();
	if (!pTimeOfDay->ResetPreset(dataFilename) && !pTimeOfDay->AddNewPreset(dataFilename))
	{
		return false;
	}

	// Try to apply the default time of day settings.
	pTimeOfDay->ImportPreset(dataFilename, ITimeOfDay::GetDefaultPresetFilepath());

	return pTimeOfDay->SavePreset(dataFilename);
}

CAssetEditor* CEnvironmentAssetType::Edit(CAsset* pAsset) const
{
	return CAssetEditor::OpenAssetForEdit("Environment Editor", pAsset);
}

CryIcon CEnvironmentAssetType::GetIconInternal() const
{
	return CryIcon("icons:common/assets_environment_settings.ico");
}
