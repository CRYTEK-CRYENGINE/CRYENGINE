// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IAudioSystemEditor.h"
#include "IAudioConnection.h"
#include "IAudioSystemItem.h"
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

class CSdlMixerConnection : public IAudioConnection
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

	virtual ~CSdlMixerConnection() {}

	virtual bool HasProperties() { return true; }

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(type, "action", "Action");
		ar(bPanningEnabled, "panning", "Enable Panning");

		if (ar.openBlock("DistanceAttenuation", "+Distance Attenuation"))
		{
			ar(bAttenuationEnabled, "attenuation", "Enable");
			if (bAttenuationEnabled)
			{
				if (ar.isInput())
				{
					float minAtt = minAttenuation;
					float maxAtt = maxAttenuation;
					ar(minAtt, "min_att", "Min Distance");
					ar(maxAtt, "max_att", "Max Distance");

					if (minAtt > maxAtt)
					{
						if (minAtt != minAttenuation)
						{
							maxAtt = minAtt;
						}
						else
						{
							minAtt = maxAtt;
						}
					}
					minAttenuation = minAtt;
					maxAttenuation = maxAtt;
				}
				else
				{
					ar(minAttenuation, "min_att", "Min Distance");
					ar(maxAttenuation, "max_att", "Max Distance");
				}
			}
			else
			{
				ar(minAttenuation, "min_att", "!Min Distance");
				ar(maxAttenuation, "max_att", "!Max Distance");
			}
			ar.closeBlock();
		}

		ar(Serialization::Range(volume, -96.0f, 0.0f), "vol", "Volume (dB)");

		if (ar.openBlock("Looping", "+Looping"))
		{
			ar(bInfiniteLoop, "infinite", "Infinite");
			if (bInfiniteLoop)
			{
				ar(loopCount, "loop_count", "!Count");
			}
			else
			{
				ar(loopCount, "loop_count", "Count");
			}
			ar.closeBlock();
		}
	}

	ESdlMixerConnectionType type;
	float                   minAttenuation;
	float                   maxAttenuation;
	float                   volume;
	uint                    loopCount;
	bool                    bPanningEnabled;
	bool                    bAttenuationEnabled;
	bool                    bInfiniteLoop;
};

typedef std::shared_ptr<CSdlMixerConnection> SdlConnectionPtr;

class CImplementationSettings_sdlmixer final : public IImplementationSettings
{
public:
	CImplementationSettings_sdlmixer()
		: m_projectPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "sdlmixer") {}
	virtual const char* GetSoundBanksPath() const { return PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "sdlmixer"; }
	virtual const char* GetProjectPath() const    { return m_projectPath.c_str(); }
	virtual void        SetProjectPath(const char* szPath);
	;
	void                Serialize(Serialization::IArchive& ar)
	{
		ar(m_projectPath, "projectPath", "Project Path");
	}

private:
	string m_projectPath;
};

class CAudioSystemEditor_sdlmixer final : public IAudioSystemEditor
{
	friend CSdlMixerProjectLoader;

public:
	CAudioSystemEditor_sdlmixer();
	virtual ~CAudioSystemEditor_sdlmixer();

	CID               GetId(const string& sName) const;
	IAudioSystemItem* CreateControl(const SControlDef& controlDefinition);

	//////////////////////////////////////////////////////////
	// IAudioSystemEditor implementation
	/////////////////////////////////////////////////////////
	virtual void                     Reload(bool bPreserveConnectionStatus = true) override;
	virtual IAudioSystemItem*        GetRoot() override { return &m_root; }
	virtual IAudioSystemItem*        GetControl(CID id) const override;
	virtual EACEControlType          ImplTypeToATLType(ItemType type) const override;
	virtual TImplControlTypeMask     GetCompatibleTypes(EACEControlType eATLControlType) const override;
	virtual ConnectionPtr            CreateConnectionToControl(EACEControlType eATLControlType, IAudioSystemItem* pMiddlewareControl) override;
	virtual ConnectionPtr            CreateConnectionFromXMLNode(XmlNodeRef pNode, EACEControlType eATLControlType) override;
	virtual XmlNodeRef               CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EACEControlType eATLControlType) override;
	virtual const char*              GetTypeIcon(ItemType type) const override;
	virtual string                   GetName() const override;
	virtual IImplementationSettings* GetSettings() override { return &m_settings; }
	//////////////////////////////////////////////////////////

private:

	static const string              s_controlNameTag;
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
	typedef std::map<CID, std::vector<SdlConnectionPtr>> SdlMixerConnections;
	SdlMixerConnections              m_connectionsByID;
	std::vector<IAudioSystemItem*>   m_controls;
	CImplementationSettings_sdlmixer m_settings;
};
}
