// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorImpl.h"

#include "ImplConnections.h"
#include "ProjectLoader.h"

#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IProfileData.h>
#include <CrySystem/ISystem.h>
#include <CryCore/CryCrc32.h>
#include <CryCore/StlUtils.h>
#include <CrySerialization/IArchiveHost.h>

namespace ACE
{
namespace Wwise
{
string  const g_userSettingsFile = "%USER%/audiocontrolseditor_wwise.user";

class CStringAndHash
{
	//a small helper class to make comparison of the stored c-string faster, by checking a hash-representation first.
public:

	CStringAndHash(char const* pText)
		: szText(pText), hash(CCrc32::Compute(pText))
	{}

	operator string() const                      { return szText; }
	operator char const*() const                 { return szText; }
	bool operator==(const CStringAndHash& other) { return ((hash == other.hash) && (strcmp(szText, other.szText) == 0)); }

private:

	char const*  szText;
	uint32 const hash;
};

// XML tags
CStringAndHash const g_switchTag = "WwiseSwitch";
CStringAndHash const g_stateTag = "WwiseState";
CStringAndHash const g_fileTag = "WwiseFile";
CStringAndHash const g_parameterTag = "WwiseRtpc";
CStringAndHash const g_eventTag = "WwiseEvent";
CStringAndHash const g_auxBusTag = "WwiseAuxBus";
CStringAndHash const g_valueTag = "WwiseValue";

// XML attributes
string const g_nameAttribute = "wwise_name";
string const g_shiftAttribute = "wwise_value_shift";
string const g_multAttribute = "wwise_value_multiplier";
string const g_valueAttribute = "wwise_value";
string const g_localizedAttribute = "wwise_localised";
string const g_trueAttribute = "true";

//////////////////////////////////////////////////////////////////////////
EImpltemType TagToType(char const* szTag)
{
	CStringAndHash hashedTag(szTag);
	if (hashedTag == g_eventTag)
	{
		return EImpltemType::Event;
	}
	if (hashedTag == g_fileTag)
	{
		return EImpltemType::SoundBank;
	}
	if (hashedTag == g_parameterTag)
	{
		return EImpltemType::Parameter;
	}
	if (hashedTag == g_switchTag)
	{
		return EImpltemType::SwitchGroup;
	}
	if (hashedTag == g_stateTag)
	{
		return EImpltemType::StateGroup;
	}
	if (hashedTag == g_auxBusTag)
	{
		return EImpltemType::AuxBus;
	}
	return EImpltemType::Invalid;
}

//////////////////////////////////////////////////////////////////////////
string TypeToTag(EImpltemType const type)
{
	switch (type)
	{
	case EImpltemType::Event:
		return g_eventTag;
	case EImpltemType::Parameter:
		return g_parameterTag;
	case EImpltemType::Switch:
		return g_valueTag;
	case EImpltemType::AuxBus:
		return g_auxBusTag;
	case EImpltemType::SoundBank:
		return g_fileTag;
	case EImpltemType::State:
		return g_valueTag;
	case EImpltemType::SwitchGroup:
		return g_switchTag;
	case EImpltemType::StateGroup:
		return g_stateTag;
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
CImplItem* SearchForControl(CImplItem* const pImplItem, string const& name, ItemType const type)
{
	if ((pImplItem->GetName() == name) && (pImplItem->GetType() == type))
	{
		return pImplItem;
	}

	int const count = pImplItem->ChildCount();

	for (int i = 0; i < count; ++i)
	{
		CImplItem* const pFoundImplControl = SearchForControl(pImplItem->GetChildAt(i), name, type);

		if (pFoundImplControl != nullptr)
		{
			return pFoundImplControl;
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CImplSettings::SetProjectPath(char const* szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(g_userSettingsFile.c_str(), *this);
}

//////////////////////////////////////////////////////////////////////////
void CImplSettings::Serialize(Serialization::IArchive& ar)
{
	ar(m_projectPath, "projectPath", "Project Path");
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::CEditorImpl()
{
	Serialization::LoadJsonFile(m_implSettings, g_userSettingsFile.c_str());
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::~CEditorImpl()
{
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Reload(bool const preserveConnectionStatus)
{
	Clear();

	CProjectLoader(GetSettings()->GetProjectPath(), GetSettings()->GetSoundBanksPath(), m_rootControl);

	CreateControlCache(&m_rootControl);

	if (preserveConnectionStatus)
	{
		for (auto const& connection : m_connectionsByID)
		{
			if (connection.second > 0)
			{
				CImplItem* const pImplControl = GetControl(connection.first);

				if (pImplControl != nullptr)
				{
					pImplControl->SetConnected(true);
				}
			}
		}
	}
	else
	{
		m_connectionsByID.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::GetControl(CID const id) const
{
	if (id >= 0)
	{
		return stl::find_in_map(m_controlsCache, id, nullptr);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
char const* CEditorImpl::GetTypeIcon(CImplItem const* const pImplItem) const
{
	auto const type = static_cast<EImpltemType>(pImplItem->GetType());

	switch (type)
	{
	case EImpltemType::Event:
		return "icons:audio/wwise/event.ico";
	case EImpltemType::Parameter:
		return "icons:audio/wwise/gameparameter.ico";
	case EImpltemType::Switch:
		return "icons:audio/wwise/switch.ico";
	case EImpltemType::AuxBus:
		return "icons:audio/wwise/auxbus.ico";
	case EImpltemType::SoundBank:
		return "icons:audio/wwise/soundbank.ico";
	case EImpltemType::State:
		return "icons:audio/wwise/state.ico";
	case EImpltemType::SwitchGroup:
		return "icons:audio/wwise/switchgroup.ico";
	case EImpltemType::StateGroup:
		return "icons:audio/wwise/stategroup.ico";
	case EImpltemType::WorkUnit:
		return "icons:audio/wwise/workunit.ico";
	case EImpltemType::VirtualFolder:
		return "icons:audio/wwise/virtualfolder.ico";
	case EImpltemType::PhysicalFolder:
		return "icons:audio/wwise/physicalfolder.ico";
	}
	return "icons:Dialogs/dialog-error.ico";
}

//////////////////////////////////////////////////////////////////////////
string CEditorImpl::GetName() const
{
	return gEnv->pAudioSystem->GetProfileData()->GetImplName();
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsTypeCompatible(ESystemItemType const systemType, CImplItem const* const pImplItem) const
{
	auto const implType = static_cast<EImpltemType>(pImplItem->GetType());

	switch (systemType)
	{
	case ESystemItemType::Trigger:
		return implType == EImpltemType::Event;
	case ESystemItemType::Parameter:
		return implType == EImpltemType::Parameter;
	case ESystemItemType::Switch:
		return implType == EImpltemType::Invalid;
	case ESystemItemType::State:
		return (implType == EImpltemType::Switch) || (implType == EImpltemType::State) || (implType == EImpltemType::Parameter);
	case ESystemItemType::Environment:
		return (implType == EImpltemType::AuxBus) || (implType == EImpltemType::Switch) || (implType == EImpltemType::State) || (implType == EImpltemType::Parameter);
	case ESystemItemType::Preload:
		return implType == EImpltemType::SoundBank;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
ESystemItemType CEditorImpl::ImplTypeToSystemType(CImplItem const* const pImplItem) const
{
	auto const itemType = static_cast<EImpltemType>(pImplItem->GetType());

	switch (itemType)
	{
	case EImpltemType::Event:
		return ESystemItemType::Trigger;
	case EImpltemType::Parameter:
		return ESystemItemType::Parameter;
	case EImpltemType::Switch:
	case EImpltemType::State:
		return ESystemItemType::State;
	case EImpltemType::AuxBus:
		return ESystemItemType::Environment;
	case EImpltemType::SoundBank:
		return ESystemItemType::Preload;
	case EImpltemType::StateGroup:
	case EImpltemType::SwitchGroup:
		return ESystemItemType::Switch;
	}
	return ESystemItemType::Invalid;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionToControl(ESystemItemType const controlType, CImplItem* const pImplItem)
{
	if (pImplItem != nullptr)
	{
		if (static_cast<EImpltemType>(pImplItem->GetType()) == EImpltemType::Parameter)
		{
			switch (controlType)
			{
			case ESystemItemType::Parameter:
				{
					return std::make_shared<CParameterConnection>(pImplItem->GetId());
				}
			case ESystemItemType::State:
				{
					return std::make_shared<CStateToParameterConnection>(pImplItem->GetId());
				}
			}
		}

		return std::make_shared<CImplConnection>(pImplItem->GetId());
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionFromXMLNode(XmlNodeRef pNode, ESystemItemType const controlType)
{
	if (pNode != nullptr)
	{
		char const* const szTag = pNode->getTag();
		EImpltemType const type = TagToType(szTag);

		if (type != EImpltemType::Invalid)
		{
			string const name = pNode->getAttr(g_nameAttribute);
			string const localizedAttribute = pNode->getAttr(g_localizedAttribute);
			bool const isLocalized = (localizedAttribute.compareNoCase(g_trueAttribute) == 0);

			CImplItem* pImplControl = SearchForControl(&m_rootControl, name, static_cast<ItemType>(type));

			// If control not found, create a placeholder.
			// We want to keep that connection even if it's not in the middleware.
			// The user could be using the engine without the wwise project
			if (pImplControl == nullptr)
			{
				CID const id = GenerateID(name, isLocalized, &m_rootControl);
				pImplControl = new CImplControl(name, id, static_cast<ItemType>(type));
				pImplControl->SetLocalised(isLocalized);
				pImplControl->SetPlaceholder(true);

				m_controlsCache[id] = pImplControl;
			}

			// If it's a switch we actually connect to one of the states within the switch
			if ((type == EImpltemType::SwitchGroup) || (type == EImpltemType::StateGroup))
			{
				if (pNode->getChildCount() == 1)
				{
					pNode = pNode->getChild(0);

					if (pNode != nullptr)
					{
						string const childName = pNode->getAttr(g_nameAttribute);

						CImplItem* pStateControl = nullptr;
						size_t const count = pImplControl->ChildCount();

						for (size_t i = 0; i < count; ++i)
						{
							CImplItem* const pChild = pImplControl->GetChildAt(i);

							if ((pChild != nullptr) && (pChild->GetName() == childName))
							{
								pStateControl = pChild;
							}
						}

						if (pStateControl == nullptr)
						{
							CID const id = GenerateID(childName);
							pStateControl = new CImplControl(childName, id, static_cast<ItemType>(type == EImpltemType::SwitchGroup ? EImpltemType::Switch : EImpltemType::State));
							pStateControl->SetLocalised(false);
							pStateControl->SetPlaceholder(true);
							pImplControl->AddChild(pStateControl);
							pStateControl->SetParent(pImplControl);

							m_controlsCache[id] = pStateControl;
						}

						pImplControl = pStateControl;
					}
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] [Wwise] Error reading connection to Wwise control %s", name);
				}
			}

			if (pImplControl != nullptr)
			{
				if (type == EImpltemType::Parameter)
				{
					switch (controlType)
					{
					case ESystemItemType::Parameter:
						{
							ParameterConnectionPtr const pConnection = std::make_shared<CParameterConnection>(pImplControl->GetId());

							pNode->getAttr(g_multAttribute, pConnection->m_mult);
							pNode->getAttr(g_shiftAttribute, pConnection->m_shift);
							return pConnection;
						}
					case ESystemItemType::State:
						{
							StateConnectionPtr const pConnection = std::make_shared<CStateToParameterConnection>(pImplControl->GetId());
							pNode->getAttr(g_valueAttribute, pConnection->m_value);
							return pConnection;
						}
					}
				}

				return std::make_shared<CImplConnection>(pImplControl->GetId());
			}
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CEditorImpl::CreateXMLNodeFromConnection(ConnectionPtr const pConnection, ESystemItemType const controlType)
{
	CImplItem const* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		auto const itemType = static_cast<EImpltemType>(pImplControl->GetType());

		switch (static_cast<EImpltemType>(pImplControl->GetType()))
		{
		case EImpltemType::Switch:
		case EImpltemType::SwitchGroup:
		case EImpltemType::State:
		case EImpltemType::StateGroup:
			{
				CImplItem const* const pParent = pImplControl->GetParent();

				if (pParent != nullptr)
				{
					XmlNodeRef const pSwitchNode = GetISystem()->CreateXmlNode(TypeToTag(static_cast<EImpltemType>(pParent->GetType())));
					pSwitchNode->setAttr(g_nameAttribute, pParent->GetName());

					XmlNodeRef const pStateNode = pSwitchNode->createNode(g_valueTag);
					pStateNode->setAttr(g_nameAttribute, pImplControl->GetName());
					pSwitchNode->addChild(pStateNode);

					return pSwitchNode;
				}
			}
			break;

		case EImpltemType::Parameter:
			{
				XmlNodeRef pConnectionNode;
				pConnectionNode = GetISystem()->CreateXmlNode(TypeToTag(itemType));
				pConnectionNode->setAttr(g_nameAttribute, pImplControl->GetName());

				if (controlType == ESystemItemType::Parameter)
				{
					std::shared_ptr<const CParameterConnection> pParameterConnection = std::static_pointer_cast<const CParameterConnection>(pConnection);
					if (pParameterConnection->m_mult != 1.0f)
					{
						pConnectionNode->setAttr(g_multAttribute, pParameterConnection->m_mult);
					}
					if (pParameterConnection->m_shift != 0.0f)
					{
						pConnectionNode->setAttr(g_shiftAttribute, pParameterConnection->m_shift);
					}

				}
				else if (controlType == ESystemItemType::State)
				{
					std::shared_ptr<const CStateToParameterConnection> pStateConnection = std::static_pointer_cast<const CStateToParameterConnection>(pConnection);
					pConnectionNode->setAttr(g_valueAttribute, pStateConnection->m_value);
				}

				return pConnectionNode;
			}
			break;

		case EImpltemType::Event:
			{
				XmlNodeRef pConnectionNode;
				pConnectionNode = GetISystem()->CreateXmlNode(TypeToTag(itemType));
				pConnectionNode->setAttr(g_nameAttribute, pImplControl->GetName());
				return pConnectionNode;
			}
			break;

		case EImpltemType::AuxBus:
			{
				XmlNodeRef pConnectionNode;
				pConnectionNode = GetISystem()->CreateXmlNode(TypeToTag(itemType));
				pConnectionNode->setAttr(g_nameAttribute, pImplControl->GetName());
				return pConnectionNode;
			}
			break;

		case EImpltemType::SoundBank:
			{
				XmlNodeRef pConnectionNode = GetISystem()->CreateXmlNode(TypeToTag(itemType));
				pConnectionNode->setAttr(g_nameAttribute, pImplControl->GetName());
				if (pImplControl->IsLocalised())
				{
					pConnectionNode->setAttr(g_localizedAttribute, g_trueAttribute);
				}
				return pConnectionNode;
			}
			break;
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::EnableConnection(ConnectionPtr const pConnection)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		++m_connectionsByID[pImplControl->GetId()];
		pImplControl->SetConnected(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::DisableConnection(ConnectionPtr const pConnection)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		int connectionCount = m_connectionsByID[pImplControl->GetId()] - 1;

		if (connectionCount <= 0)
		{
			connectionCount = 0;
			pImplControl->SetConnected(false);
		}

		m_connectionsByID[pImplControl->GetId()] = connectionCount;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Clear()
{
	// Delete all the controls
	for (auto const& controlPair : m_controlsCache)
	{
		CImplItem const* const pImplControl = controlPair.second;

		if (pImplControl != nullptr)
		{
			delete pImplControl;
		}
	}

	m_controlsCache.clear();

	// Clean up the root control
	m_rootControl = CImplItem();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::CreateControlCache(CImplItem const* const pParent)
{
	if (pParent != nullptr)
	{
		size_t const count = pParent->ChildCount();

		for (size_t i = 0; i < count; ++i)
		{
			CImplItem* const pChild = pParent->GetChildAt(i);

			if (pChild != nullptr)
			{
				m_controlsCache[pChild->GetId()] = pChild;
				CreateControlCache(pChild);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CID CEditorImpl::GenerateID(string const& fullPathName) const
{
	return CCrc32::ComputeLowercase(fullPathName);
}

//////////////////////////////////////////////////////////////////////////
CID CEditorImpl::GenerateID(string const& controlName, bool isLocalized, CImplItem* pParent) const
{
	string pathName = (pParent != nullptr && !pParent->GetName().empty()) ? pParent->GetName() + CRY_NATIVE_PATH_SEPSTR + controlName : controlName;

	if (isLocalized)
	{
		pathName = PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + pathName;
	}

	return GenerateID(pathName);
}
} // namespace Wwise
} // namespace ACE
