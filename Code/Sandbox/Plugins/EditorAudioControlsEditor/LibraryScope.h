// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/SharedData.h"
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
struct SLibraryScope final
{
	SLibraryScope()
	{
		nodes[0] = GetISystem()->CreateXmlNode(CryAudio::g_szTriggersNodeTag);
		nodes[1] = GetISystem()->CreateXmlNode(CryAudio::g_szParametersNodeTag);
		nodes[2] = GetISystem()->CreateXmlNode(CryAudio::g_szSwitchesNodeTag);
		nodes[3] = GetISystem()->CreateXmlNode(CryAudio::g_szEnvironmentsNodeTag);
		nodes[4] = GetISystem()->CreateXmlNode(CryAudio::g_szPreloadsNodeTag);
		nodes[5] = GetISystem()->CreateXmlNode(CryAudio::g_szSettingsNodeTag);
	}

	XmlNodeRef GetXmlNode(EAssetType const type) const
	{
		XmlNodeRef node;

		switch (type)
		{
		case EAssetType::Trigger:
			{
				node = nodes[0];
				break;
			}
		case EAssetType::Parameter:
			{
				node = nodes[1];
				break;
			}
		case EAssetType::Switch:
			{
				node = nodes[2];
				break;
			}
		case EAssetType::Environment:
			{
				node = nodes[3];
				break;
			}
		case EAssetType::Preload:
			{
				node = nodes[4];
				break;
			}
		case EAssetType::Setting:
			{
				node = nodes[5];
				break;
			}
		default:
			{
				node = nullptr;
				break;
			}
		}

		return node;
	}

	XmlNodeRef nodes[6]; // Trigger, Parameter, Switch, Environment, Preload, Setting
	bool       isDirty = false;
	uint32     numTriggers = 0;
	uint32     numParameters = 0;
	uint32     numSwitches = 0;
	uint32     numStates = 0;
	uint32     numEnvironments = 0;
	uint32     numPreloads = 0;
	uint32     numSettings = 0;
	uint32     numFiles = 0;
};
} // namespace ACE
