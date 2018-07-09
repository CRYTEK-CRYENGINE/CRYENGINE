// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorSubstanceManager.h"

#include "SandboxPlugin.h"
#include <CryString/CryPath.h>

#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include <CrySystem/File/IFileChangeMonitor.h>
#include <CrySerialization/IArchiveHost.h> 
#include "CrySerialization/Serializer.h"
#include "CrySerialization/STL.h"
#include "IEditor.h"
#include <AssetSystem/AssetEditor.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/AssetManagerHelpers.h>
#include "Notifications/NotificationCenter.h"
#include <FilePathUtil.h>
#include "Util/FileUtil.h"
#include "ThreadingUtils.h"
#include "Util/Image.h"
#include "Util/ImageUtil.h"
#include "QtUtil.h"

#include "SubstanceCommon.h"
#include "SubstancePreset.h"
#include "ISubstanceManager.h"
#include "PresetCreator.h"
#include "AssetTypes/SubstanceArchive.h"
#include "Renderers/Compressed.h"
#include "Renderers/Preview.h"

#include <QMenu>

namespace EditorSubstance 
{

CManager* CManager::s_pInstance = nullptr;

class CManager::CSubstanceFileListener : public AssetManagerHelpers::CAsyncFileListener
{
public:
	virtual bool AcceptFile(const char* szFilename, EChangeType changeType) override
	{
		if (changeType != IFileChangeListener::eChangeType_Modified && changeType != IFileChangeListener::eChangeType_RenamedNewName)
		{
			return false;
		}

		const char* const szExt = PathUtil::GetExt(szFilename);
		if (stricmp(szExt, "crysub"))
		{
			return false;
		}

		// if opened in editor, it will not be reloaded.
		if (CManager::Instance()->IsEditing(szFilename))
		{
			return false;
		}

		return true;
	}

	virtual bool ProcessFile(const char* szFilename, EChangeType changeType) override
	{
		const string assetPath(szFilename);

		ThreadingUtils::AsyncQueue([assetPath]()
		{
			CProgressNotification notification(QObject::tr("Saving substance textures"), QString(QtUtil::ToQString(assetPath)));

			const string gameFolderPath = PathUtil::AddSlash(PathUtil::GetGameProjectAssetsPath());
			string additionalSettings;
			// TODO for now it looks like sandbox isn't reloading the texture when tif is not existent and dd
			// is written directly, so we need to write the tif
			//additionalSettings += " /directdds /sourceroot=\"" + gameFolderPath + "\"";
			additionalSettings += "/sourceroot=\"" + gameFolderPath + "\"";

			AssetManagerHelpers::RCLogger logger;

			CResourceCompilerHelper::CallResourceCompiler(
				assetPath.c_str(),
				additionalSettings,
				&logger,
				false, // may show window?
				CResourceCompilerHelper::eRcExePath_editor,
				true,  // silent?
				true); // no user dialog?
		});

		return true;
	}

};

CManager* CManager::Instance() 
{
	if (!s_pInstance) 
	{
		s_pInstance = new CManager;
	}
	return s_pInstance;
}

CManager::~CManager()
{
	CAssetManager::GetInstance()->signalAssetChanged.DisconnectObject(this);
	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.DisconnectObject(this);
}

void CManager::Init()
{
	string configPath;
	{
		std::vector<char> buffer;
		buffer.resize(MAX_PATH);
		auto getPath = [](std::vector<char>& buffer)
		{
			return CResourceCompilerHelper::GetResourceCompilerConfigPath(&buffer[0], buffer.size(), CResourceCompilerHelper::eRcExePath_editor);
		};
		const int len = getPath(buffer);
		if (len >= buffer.size())
		{
			buffer.resize(len + 1);
			getPath(buffer);
		}
		configPath.assign(&buffer[0]);
	}

	LoadTexturePresets(configPath);

	m_pCompressedRenderer = new EditorSubstance::Renderers::CCompressedRenderer();
	m_pPreviewRenderer = new EditorSubstance::Renderers::CPreviewRenderer();
	ISubstanceManager::Instance()->RegisterInstanceRenderer(m_pCompressedRenderer);
	ISubstanceManager::Instance()->RegisterInstanceRenderer(m_pPreviewRenderer);
	m_renderQueue.resize(2);
	m_renderQueue[0].renderer = static_cast<Renderers::CInstanceRenderer*>(m_pCompressedRenderer);
	m_renderQueue[1].renderer = static_cast<Renderers::CInstanceRenderer*>(m_pPreviewRenderer);

	m_pFileListener = new CSubstanceFileListener();
	GetIEditor()->GetFileMonitor()->RegisterListener(m_pFileListener, "", "crysub");
	GetIEditor()->RegisterNotifyListener(this);
	CAssetManager::GetInstance()->signalAssetChanged.Connect(this, &CManager::OnAssetModified);
	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.Connect(this, &CManager::OnAssetsRemoved);
}

bool CManager::IsEditing(const string& presetName)
{
	CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(presetName);
	if (asset)
	{
		return asset->IsBeingEdited();
	}
	return false;
}

CManager::CManager(QObject* parent)
	: QObject(parent)
	, m_pPresetCreator(nullptr)
	, m_renderUID(0)
	, m_currentRendererID(0)
{

}

#define MAXIMUM_SUBSTANCE_INSTANCES_TO_RENDER 4
void CManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
	{
		if (m_currentRendererID > m_renderQueue.size() - 1)
		{
			m_currentRendererID = 0;
		}

		for (auto& renderJob : m_renderQueue)
		{
			renderJob.renderer->ProcessComputedOutputs();
		}

		if (!m_pushedRenderJob.presets.empty() && m_renderUID != 0 && !ISubstanceManager::Instance()->IsRenderPending(m_renderUID) && !m_pushedRenderJob.renderer->OutputsInQueue())
		{
			m_renderUID = 0;
			m_pushedRenderJob.presets.clear();
		}

		if (!m_presetsToDelete.empty() && m_pushedRenderJob.presets.empty())
		{
			std::unordered_set<ISubstancePreset*> usedPresets;
			for (auto& renderJob : m_renderQueue)
			{
				std::unordered_set<ISubstancePreset*> s(renderJob.presets.begin(), renderJob.presets.end());
				usedPresets.insert(s.begin(), s.end());
			}

			std::vector<ISubstancePreset*>::iterator it = m_presetsToDelete.begin();

			while (it != m_presetsToDelete.end()) {

				if (!usedPresets.count(*it))
				{
					ISubstancePreset* preset = *it;
					for (auto& renderJob : m_renderQueue)
					{
						renderJob.renderer->RemovePresetRenderData(preset);
					}
					// before preset gets deleted, we need to go trough images it potentially loaded.
					for (auto iit = m_loadedImages.begin(); iit != m_loadedImages.end();) {
						SInputImageRefCount* refCount = &iit->second;
						refCount->usedByInstances.erase(preset->GetInstanceID());
						if (refCount->usedByInstances.empty()) {
							iit = m_loadedImages.erase(iit);
						}
						else
							iit++;
					}
					it = m_presetsToDelete.erase(it);
					preset->Destroy();
				}
				else
				{
					++it;
				}
			}
		}

		if (!m_renderQueue[m_currentRendererID].presets.empty() && m_pushedRenderJob.presets.empty())
		{
			while (m_renderQueue[m_currentRendererID].presets.size() != 0 && m_pushedRenderJob.presets.size() <= MAXIMUM_SUBSTANCE_INSTANCES_TO_RENDER)
			{
				m_pushedRenderJob.presets.push_back(*m_renderQueue[m_currentRendererID].presets.begin());
				m_renderQueue[m_currentRendererID].presets.erase(m_renderQueue[m_currentRendererID].presets.begin());
			}
			m_pushedRenderJob.renderer = m_renderQueue[m_currentRendererID].renderer;
			m_renderUID = ISubstanceManager::Instance()->GenerateOutputsAsync(m_pushedRenderJob.presets, m_pushedRenderJob.renderer);
			// when something fails, renderUID can be zero, we just need to forget current queue and pretend nothing happened
			if (m_renderUID == 0)
			{
				m_pushedRenderJob.presets.clear();
			}
		}

		if (m_currentRendererID >= m_renderQueue.size() - 1)
		{
			m_currentRendererID = 0;
		}
		else
		{
			m_currentRendererID++;
		}

	}
	break;
	}
}

STexturePreset CManager::GetConfigurationForPreset(const string& presetName)
{
	if (m_Presets.count(presetName) > 0)
	{
		return m_Presets[presetName];
	}
	else
	{
		return STexturePreset();
	}
}

const SubstanceAir::InputImage::SPtr& CManager::GetInputImage(const ISubstancePreset* preset, const string& path)
{
	string nameLow(path);
	nameLow.MakeLower();
	const uint32 crc = CCrc32::Compute(nameLow);
		
	if (m_loadedImages.count(crc) == 0)
	{
		CImageEx image;
		if (!CImageUtil::LoadImage(path, image))
		{
			CImageUtil::LoadImage("engineassets/texturemsg/replaceme.dds", image);
		}
			
		SubstanceTexture texture = {
			image.GetData(), // No buffer (content will be set later for demo purposes)
			image.GetWidth(),  // Width;
			image.GetHeight(),  // Height
			Substance_PF_RGBA,
			Substance_ChanOrder_RGBA,
			1 }; 

		SInputImageRefCount& data = m_loadedImages[crc];
		// Create InputImage object from texture description
		data.imageData = SubstanceAir::InputImage::create(texture);
	}

	SInputImageRefCount& data = m_loadedImages[crc];
	data.usedByInstances.insert(preset->GetInstanceID());
	return data.imageData;
}

ISubstancePreset* CManager::GetSubstancePreset(CAsset* asset)
{
	return ISubstancePreset::Load(asset->GetFile(0));
}

ISubstancePreset* CManager::GetPreviewPreset(const string& archiveName, const string& graphName)
{
	ISubstancePreset* preset = ISubstancePreset::Instantiate(archiveName, graphName);
	preset->SetGraphResolution(8, 8);
	return preset;
}

ISubstancePreset* CManager::GetPreviewPreset(ISubstancePreset* sourcePreset)
{
	ISubstancePreset* preset = sourcePreset->Clone();
	preset->SetGraphResolution(8, 8);
	return preset;
}

void CManager::PresetEditEnded(ISubstancePreset* preset)
{
	m_presetsToDelete.push_back(preset);
}

struct DefaultPresetsSerializer
{
	std::vector<SSubstanceOutput> presets;

	void Serialize(Serialization::IArchive& ar)
	{
		ar(presets, "presets");
	}
};
#define MAPPING_FILE_NAME "default_substance_output_mapping.json"
std::vector<SSubstanceOutput> CManager::GetProjectDefaultOutputSettings()
{
	DefaultPresetsSerializer serializer;
	if (GetIEditor()->GetSystem()->GetIPak()->IsFileExist(MAPPING_FILE_NAME))
	{
		Serialization::LoadJsonFile(serializer, MAPPING_FILE_NAME);
	}
	// fallback to engine wide mapping
	else if (GetIEditor()->GetSystem()->GetIPak()->IsFileExist("%EDITOR%/DefaultSubstanceOutputMapping.json"))
	{
		Serialization::LoadJsonFile(serializer, "%EDITOR%/DefaultSubstanceOutputMapping.json");
	}
	return serializer.presets;
}

void CManager::SaveProjectDefaultOutputSettings(const std::vector<SSubstanceOutput>& outputs)
{
	DefaultPresetsSerializer serializer;
	serializer.presets = outputs;
	Serialization::SaveJsonFile(MAPPING_FILE_NAME, serializer);
}

void CManager::AddSubstanceArchiveContextMenu(CAsset* asset, CAbstractMenu* menu)
{

	CAbstractMenu* subMenu = menu->CreateMenu("Create Instance");
	string archivePath(asset->GetFile(0));
	for each(string graphName in GetArchiveGraphs(archivePath))
	{
		QAction* newAct = subMenu->CreateAction(graphName.c_str());
		QStringList info;
		info << asset->GetMetadataFile() << graphName.c_str();
		newAct->setData(QVariant(info));
		QObject::connect(newAct, &QAction::triggered, this, &CManager::OnCreateInstance);
	}
	QAction* rebuild = menu->CreateAction("Rebuild All Instances");
	rebuild->setEnabled(!CAssetManager::GetInstance()->GetReverseDependencies(*asset).empty());
	QObject::connect(rebuild, &QAction::triggered, this, [=]() 
	{
		for (auto& item : CAssetManager::GetInstance()->GetReverseDependencies(*asset))
		{
			CAsset* pDepAsset = item.first;
			string dependantAssetTypeName(pDepAsset->GetType()->GetTypeName());
			if (dependantAssetTypeName == "SubstanceInstance")
			{
				ForcePresetRegeneration(pDepAsset);
			}
		}
	});
}

void CManager::AddSubstanceInstanceContextMenu(CAsset* asset, CAbstractMenu* menu)
{
	QAction* rebuild = menu->CreateAction("Rebuild Instance");
	QObject::connect(rebuild, &QAction::triggered, this, [=]() {
		ForcePresetRegeneration(asset);
	});
}
void CManager::OnCreateInstance()
{
	if (m_pPresetCreator)
	{
		m_pPresetCreator->raise();
		m_pPresetCreator->setFocus();
		return;
	}


	QAction* sourceAction = qobject_cast<QAction*>(sender());
	QStringList data = sourceAction->data().toStringList();
	string graphName(data[1].toStdString().c_str());
	CAsset* archiveAsset = CAssetManager::GetInstance()->FindAssetForMetadata(data[0].toStdString().c_str());
	string archiveName(archiveAsset->GetFile(0));

	std::vector<SSubstanceOutput> grapOutputs;
	Vec2i resolution(10, 10);
	AssetTypes::CSubstanceArchiveType::SSubstanceArchiveCache cache = static_cast<const AssetTypes::CSubstanceArchiveType*>(archiveAsset->GetType())->GetArchiveCache(archiveAsset);
	if (cache.HasGraph(graphName))
	{
		const AssetTypes::CSubstanceArchiveType::SSubstanceArchiveCache::Graph& graph = cache.GetGraph(graphName);		
		for (const SSubstanceOutput& output : graph.GetOutputsConfiguration())
		{
			grapOutputs.push_back(output);
			SSubstanceOutput& toWork = grapOutputs.back();
			toWork.UpdateState(graph.GetOutputNames());
		}
		resolution = graph.GetResolution();
	}

	m_pPresetCreator = new CPressetCreator(archiveAsset, graphName, grapOutputs, resolution);
	connect(m_pPresetCreator, &CPressetCreator::accepted, this, &CManager::OnCreatorAccepted);
	connect(m_pPresetCreator, &CPressetCreator::rejected, this, &CManager::OnCreatorRejected);
	m_pPresetCreator->Popup();

}

void CManager::OnCreatorAccepted()
{
	if (!m_pPresetCreator->GetSubstanceArchive().empty())
	{
		CreateInstance(m_pPresetCreator->GetSubstanceArchive(), m_pPresetCreator->GetTargetFileName(), m_pPresetCreator->GetGraphName(), m_pPresetCreator->GetOutputs(), m_pPresetCreator->GetResolution());
	}
	OnCreatorRejected();
}

void CManager::OnCreatorRejected()
{
	m_pPresetCreator->deleteLater();
	m_pPresetCreator = nullptr;
}

void CManager::OnAssetModified(CAsset& asset)
{
	if (!asset.IsBeingEdited())
	{
		string typeName(asset.GetType()->GetTypeName());
		if (typeName == "Texture" /*|| typeName == "SubstanceDefinition"*/)
		{
			for (auto& item : CAssetManager::GetInstance()->GetReverseDependencies(asset))
			{
				CAsset* pDepAsset = item.first;
				string dependantAssetTypeName(pDepAsset->GetType()->GetTypeName());
				if (dependantAssetTypeName == "SubstanceInstance")
				{
					ForcePresetRegeneration(pDepAsset);
				}
			}
			
		}
	}
		
}

void CManager::OnAssetsRemoved(const std::vector<CAsset*>& assets)
{
	for (const CAsset* asset: assets)
	{
		// manualy deleting *.tif files with the same name as dds that has crysub as source.
		string typeName(asset->GetType()->GetTypeName());
		if (typeName != "Texture")
		{
			continue;
		}

		string srcExtension = PathUtil::GetExt(asset->GetSourceFile());
		if (srcExtension.CompareNoCase("crysub") == 0)
		{
			string fileName = asset->GetFile(0);
			fileName = PathUtil::ReplaceExtension(fileName, "tif");
			const string absPath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), fileName);
			if (CFileUtil::FileExists(absPath))
			{
				gEnv->pCryPak->RemoveFile(fileName);
			}
		}

	}

}

void CManager::CreateInstance(const string& archiveName, const string& instanceName, const string& instanceGraph, const std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution) const
{
	const string absToDir = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::GetDirectory(instanceName));
	CFileUtil::CreatePath(absToDir);
	ISubstanceManager::Instance()->CreateInstance(archiveName, instanceName, instanceGraph, outputs, resolution);
	CAssetType* presetType = CAssetManager::GetInstance()->FindAssetType("SubstanceInstance");
	const auto absTargetFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), instanceName);
	CAssetType* at = CAssetManager::GetInstance()->FindAssetType("SubstanceInstance");
	at->Create(instanceName+".cryasset", &archiveName);
}

void CManager::EnquePresetRender(ISubstancePreset* preset, ISubstanceInstanceRenderer* renderer)
{
	std::vector<CRenderJobPair>::iterator it = m_renderQueue.begin();
	for (it; it != m_renderQueue.end(); ++it)
	{
		if (it->renderer == renderer)
		{
			if (std::find(it->presets.begin(), it->presets.end(), preset) == it->presets.end())
				it->presets.push_back(preset);
			return;
		}
	}

}

void CManager::ForcePresetRegeneration(CAsset* asset)
{
	string fileName = asset->GetFile(0);
	m_pFileListener->OnFileChange(fileName, IFileChangeListener::eChangeType_Modified);
}

std::vector<string> CManager::GetGraphOutputs(const string& archiveName, const string& graphName)
{
	CAsset* asset = CAssetManager::GetInstance()->FindAssetForMetadata(archiveName + ".cryasset");
	if (!asset)
	{
		return{};
	}
	return static_cast<const AssetTypes::CSubstanceArchiveType*>(asset->GetType())->GetArchiveCache(asset).GetGraph(graphName).GetOutputNames();
}

std::vector<string> CManager::GetArchiveGraphs(const string& archiveName)
{
	CAsset* asset = CAssetManager::GetInstance()->FindAssetForMetadata(archiveName + ".cryasset");
	if (!asset)
	{
		return{};
	}

	return static_cast<const AssetTypes::CSubstanceArchiveType*>(asset->GetType())->GetArchiveCache(asset).GetGraphNames();

}

std::set<string> CManager::GetTexturePresetsForFile(const string& mask)
{
	if (mask.empty())
		return m_FilteredPresets[""];
	std::set<string> matchingPresets;
	std::set<string>::const_iterator p;
	std::list<string>::const_iterator l;
	string filename("_");
	filename += mask;
	// Collect names of all matching presets
	std::map<string, std::set<string> >::const_iterator s;
	for (s = m_FilteredPresets.begin(); s != m_FilteredPresets.end(); ++s)
	{
		const string& suffix = s->first;
		if (CryStringUtils::MatchWildcard(filename, suffix))
		{
			for (p = s->second.begin(); p != s->second.end(); ++p)
			{
				matchingPresets.insert(*p);
			}
		}
	}
	if (matchingPresets.size() == 0)
	{
		for (p = m_FilteredPresets[""].begin(); p != m_FilteredPresets[""].end(); ++p)
		{
			matchingPresets.insert(*p);
		}

	}
	return matchingPresets;
}

#include "RCConfigParser.inl"

typedef std::map<const string, EditorSubstance::EPixelFormat> PixelFormatsMap;
PixelFormatsMap pixelFormatsMap{
	{ "BC1", EditorSubstance::ePixelFormat_BC1 },
	{ "BC1a", EditorSubstance::ePixelFormat_BC1a },
	{ "BC3", EditorSubstance::ePixelFormat_BC3 },
	{ "BC4", EditorSubstance::ePixelFormat_BC4 },
	{ "BC5", EditorSubstance::ePixelFormat_BC5 },
	{ "BC5s", EditorSubstance::ePixelFormat_BC5s },
	{ "BC7", EditorSubstance::ePixelFormat_BC7 },
	{ "BC7t", EditorSubstance::ePixelFormat_BC7t },
	{ "A8R8G8B8", EditorSubstance::ePixelFormat_A8R8G8B8 },

};

EditorSubstance::EPixelFormat GetPixelFormatFromString(const string& pxFormat)
{
	if (pixelFormatsMap.count(pxFormat))
	{
		return pixelFormatsMap[pxFormat];
	}
	else {
		return EditorSubstance::ePixelFormat_Unsupported;
	}

}

void CManager::LoadTexturePresets(const string& configPath)
{
	RCConfigParser::ParsedConfig config;
	RCConfigParser::LoadConfigFile(configPath.c_str(), config);

	for each (std::pair<const string&, const RCConfigParser::ConfigSection&> sectionInfo in config.GetSections())
	{
		string name = sectionInfo.first;
		if (name.empty() || name.c_str()[0] == '_')
		{
			continue;
		}
		STexturePreset& texPreset = m_Presets[name];
		texPreset.name = name;
		const RCConfigParser::ConfigSection& section = sectionInfo.second;
		RCConfigParser::ConfigEntries::const_iterator iter;
		for (iter = section.GetEntries().begin(); iter != section.GetEntries().end(); ++iter) {
			string entryName = iter->first;
			RCConfigParser::ConfigEntry* entry = iter->second.get();
			if (entryName == "pixelformat")
			{
				texPreset.format = GetPixelFormatFromString(entry->GetAsString());
			}
			else if (entryName == "mipmaps")
			{
				texPreset.mipmaps = entry->GetAsBool();
			}
			else if (entryName == "colorspace")
			{
				string colorSpaceInput = "sRGB";
				string colorSpaceOutput = "auto";

				std::vector<string> values = entry->GetAsStringVector();
				if (values.size() >= 1)
					colorSpaceInput = values[0];
				if (values.size() >= 2)
					colorSpaceOutput = values[1];

				if (colorSpaceOutput == "auto")
					colorSpaceOutput = colorSpaceInput;

				if (colorSpaceOutput == "sRGB")
					texPreset.colorSpace = EditorSubstance::eColorSpaceSRGB;
				else
					texPreset.colorSpace = EditorSubstance::eColorSpaceLinear;

			}
			else if (entryName == "filemasks")
			{
				std::vector<string> values = entry->GetAsStringVector();
				texPreset.fileMasks.insert(texPreset.fileMasks.end(), values.begin(), values.end());

			}
			else if (entryName == "pixelformatalpha")
			{
				texPreset.formatAlpha = GetPixelFormatFromString(entry->GetAsString());
			}
			else if (entryName == "decal")
			{
				texPreset.decal = entry->GetAsBool();
			}
		}

	}
	for each (auto var in m_Presets)
	{
		m_FilteredPresets[""].insert(var.first);
		std::vector<string>& masks = var.second.fileMasks;
		for (size_t pos = 0; pos < masks.size(); ++pos)
		{
			m_FilteredPresets[masks[pos]].insert(var.first);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
STexturePreset::STexturePreset()
	: name("")
	, format(EditorSubstance::ePixelFormat_Unsupported)
	, formatAlpha(EditorSubstance::ePixelFormat_Unsupported)
	, mipmaps(false)
	, useAlpha(false)
	, decal(false)
{

}

}

#include <CrySerialization/Enum.h>
#include "SerializationSubstanceEnums.inl"
