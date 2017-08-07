// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IAudioSystemEditor.h>
#include <IAudioConnection.h>
#include <IAudioSystemItem.h>
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>

#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/ClassFactory.h>
#include "SDLMixerProjectLoader.h"
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace ACE
{

enum ESdlMixerConnectionType
{
	eSdlMixerConnectionType_Start = 0,
	eSdlMixerConnectionType_Stop,
	eSdlMixerConnectionType_Num_Types
};

class CSdlMixerConnection final : public IAudioConnection
{
public:
	explicit CSdlMixerConnection(CID id)
		: IAudioConnection(id)
		, type(eSdlMixerConnectionType_Start)
		, bPanningEnabled(true)
		, bAttenuationEnabled(true)
		, minAttenuation(0.0f)
		, maxAttenuation(100.0f)
		, volume(-14.0f)
		, loopCount(1)
		, bInfiniteLoop(false)
	{}

	virtual bool HasProperties() override { return true; }

	virtual void Serialize(Serialization::IArchive& ar) override;

	ESdlMixerConnectionType type;
	float                   minAttenuation;
	float                   maxAttenuation;
	float                   volume;
	uint                    loopCount;
	bool                    bPanningEnabled;
	bool                    bAttenuationEnabled;
	bool                    bInfiniteLoop;
};

using SdlConnectionPtr = std::shared_ptr<CSdlMixerConnection>;

class CImplementationSettings_sdlmixer final : public IImplementationSettings
{
public:
	CImplementationSettings_sdlmixer()
		: m_projectPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "sdlmixer")
		, m_soundBanksPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "sdlmixer") {}
	virtual const char* GetSoundBanksPath() const override { return m_soundBanksPath.c_str(); }
	virtual const char* GetProjectPath() const override    { return m_projectPath.c_str(); }
	virtual void        SetProjectPath(const char* szPath) override;

	void                Serialize(Serialization::IArchive& ar)
	{
		ar(m_projectPath, "projectPath", "Project Path");
	}

private:
	string       m_projectPath;
	const string m_soundBanksPath;
};

class CAudioSystemEditor_sdlmixer final : public IAudioSystemEditor
{

public:
	CAudioSystemEditor_sdlmixer();
	virtual ~CAudioSystemEditor_sdlmixer() override;

	//////////////////////////////////////////////////////////
	// IAudioSystemEditor implementation
	/////////////////////////////////////////////////////////
	virtual void                     Reload(bool bPreserveConnectionStatus = true) override;
	virtual IAudioSystemItem*        GetRoot() override { return &m_root; }
	virtual IAudioSystemItem*        GetControl(CID id) const override;
	virtual EItemType                ImplTypeToATLType(ItemType type) const override;
	virtual TImplControlTypeMask     GetCompatibleTypes(EItemType eATLControlType) const override;
	virtual ConnectionPtr            CreateConnectionToControl(EItemType eATLControlType, IAudioSystemItem* pMiddlewareControl) override;
	virtual ConnectionPtr            CreateConnectionFromXMLNode(XmlNodeRef pNode, EItemType eATLControlType) override;
	virtual XmlNodeRef               CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EItemType eATLControlType) override;
	virtual void                     EnableConnection(ConnectionPtr pConnection) override;
	virtual void                     DisableConnection(ConnectionPtr pConnection) override;
	virtual const char*              GetTypeIcon(ItemType type) const override;
	virtual string                   GetName() const override;
	virtual IImplementationSettings* GetSettings() override { return &m_settings; }
	//////////////////////////////////////////////////////////

private:

	CID  GetId(const string& sName) const;
	void CreateControlCache(IAudioSystemItem* pParent);
	void Clear();

	static const string              s_itemNameTag;
	static const string              s_pathNameTag;
	static const string              s_eventConnectionTag;
	static const string              s_sampleConnectionTag;
	static const string              s_connectionTypeTag;
	static const string              s_panningEnabledTag;
	static const string              s_attenuationEnabledTag;
	static const string              s_attenuationDistMin;
	static const string              s_attenuationDistMax;
	static const string              s_volumeTag;
	static const string              s_loopCountTag;

	IAudioSystemItem                 m_root;
	using SdlMixerConnections = std::map<CID, std::vector<SdlConnectionPtr>>;
	SdlMixerConnections              m_connectionsByID;
	std::vector<IAudioSystemItem*>   m_controlsCache;
	CImplementationSettings_sdlmixer m_settings;
};

static CAudioSystemEditor_sdlmixer* s_pSdlMixerInterface = nullptr;
} // namespace ACE
