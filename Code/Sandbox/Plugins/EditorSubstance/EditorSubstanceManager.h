// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QObject>
#include <AssetSystem/AssetType.h>
#include "CrySerialization/Serializer.h"
#include "CrySerialization/ClassFactory.h"
#include "SubstanceCommon.h"
#include "IEditor.h"
#include "substance/framework/typedefs.h"
#include "substance/framework/inputimage.h"

struct IFileChangeListener;
class ISubstancePreset;
class ISubstanceInstanceRenderer;
class ResourceCompilerCfgFile;

namespace SubstanceAir
{

struct RenderResult;
class GraphInstance;
class OutputInstanceBase;

}

namespace EditorSubstance 
{

class CPressetCreator;

namespace Renderers
{

class CInstanceRenderer;

}

enum CRY_SUBSTANCE_API EPixelFormat
{
	ePixelFormat_BC1,
	ePixelFormat_BC1a,
	ePixelFormat_BC3,
	ePixelFormat_BC4,
	ePixelFormat_BC5,
	ePixelFormat_BC5s,
	ePixelFormat_BC7,
	ePixelFormat_BC7t,
	ePixelFormat_A8R8G8B8,

	ePixelFormat_Unsupported
};

enum CRY_SUBSTANCE_API EColorSpace
{
	eColorSpaceSRGB,
	eColorSpaceLinear,
};

enum CRY_SUBSTANCE_API ETextureFlags
{
	TF_SRGB = 1,
	TF_DECAL = 2,
	TF_NORMALMAP = 4,
	TF_ALPHA = 8,
	TF_HAS_ATTACHED_ALPHA = 16
};

struct STexturePreset
{
	string                        name;
	EditorSubstance::EPixelFormat format;
	EditorSubstance::EPixelFormat formatAlpha;
	bool                          mipmaps;
	bool                          useAlpha;
	bool                          decal;
	EditorSubstance::EColorSpace  colorSpace;
	std::vector<string>           fileMasks;

	STexturePreset();
};

struct CRenderJobPair
{
	std::vector<ISubstancePreset*> presets;
	Renderers::CInstanceRenderer*  renderer;
};

class CManager : public QObject, public IEditorNotifyListener
{
	Q_OBJECT
public:
	static CManager*                      Instance();
	~CManager();
	void                                  Init();
	bool                                  IsEditing(const string& presetName);
	void                                  AddSubstanceArchiveContextMenu(CAsset* asset, CAbstractMenu* menu);
	void                                  AddSubstanceInstanceContextMenu(CAsset* asset, CAbstractMenu* menu);
	void                                  CreateInstance(const string& archiveName, const string& instanceName, const string& instanceGraph, const std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution) const;
	void                                  EnquePresetRender(ISubstancePreset* preset, ISubstanceInstanceRenderer* renderer);
	void                                  ForcePresetRegeneration(CAsset* asset);
	std::vector<string>                   GetGraphOutputs(const string& archiveName, const string& graphName);
	std::vector<string>                   GetArchiveGraphs(const string& archiveName);
	std::set<string>                      GetTexturePresetsForFile(const string& mask);
	STexturePreset                        GetConfigurationForPreset(const string& presetName);
	const SubstanceAir::InputImage::SPtr& GetInputImage(const ISubstancePreset* preset, const string& path);
	Renderers::CInstanceRenderer*         GetCompressedRenderer() { return m_pCompressedRenderer; };
	Renderers::CInstanceRenderer*         GetPreviewRenderer()    { return m_pPreviewRenderer; };

	ISubstancePreset*                     GetSubstancePreset(CAsset* asset);
	ISubstancePreset*                     GetPreviewPreset(const string& archiveName, const string& graphName);
	ISubstancePreset*                     GetPreviewPreset(ISubstancePreset* sourcePreset);
	void                                  PresetEditEnded(ISubstancePreset* preset);
	std::vector<SSubstanceOutput>         GetProjectDefaultOutputSettings();
	void                                  SaveProjectDefaultOutputSettings(const std::vector<SSubstanceOutput>& outputs);
protected slots:
	void                                  OnCreateInstance();
	void                                  OnCreatorAccepted();
	void                                  OnCreatorRejected();
	void                                  OnAssetModified(CAsset& asset);
	void                                  OnAssetsRemoved(const std::vector<CAsset*>& assets);
protected:
	CManager(QObject* parent = nullptr);
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
	void         LoadTexturePresets(const string& configPath);
private:
	class CSubstanceFileListener;

	struct SInputImageRefCount
	{
		std::unordered_set<SubstanceAir::UInt> usedByInstances;
		SubstanceAir::InputImage::SPtr         imageData;
	};

	IFileChangeListener*                            m_pFileListener;
	ResourceCompilerCfgFile*                        m_rcConfig;
	static CManager*                                s_pInstance;
	CPressetCreator*                                m_pPresetCreator;
	std::vector<CRenderJobPair>                     m_renderQueue;
	std::vector<ISubstancePreset*>                  m_presetsToDelete;
	CRenderJobPair                                  m_pushedRenderJob;
	int                                             m_currentRendererID;
	SubstanceAir::UInt                              m_renderUID;
	Renderers::CInstanceRenderer*                   m_pCompressedRenderer;
	Renderers::CInstanceRenderer*                   m_pPreviewRenderer;
	std::map<string, std::set<string>>              m_FilteredPresets;
	std::map<string, STexturePreset>                m_Presets;
	std::unordered_map<uint32, SInputImageRefCount> m_loadedImages;
};

}
