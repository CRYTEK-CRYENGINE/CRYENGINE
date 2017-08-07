// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IAudioSystemEditor.h"
#include "IAudioConnection.h"
#include "IAudioSystemItem.h"
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>

namespace ACE
{

enum EFmodItemType
{
	eFmodItemType_Invalid           = 0,
	eFmodItemType_Folder            = BIT(0),
	eFmodItemType_Event             = BIT(1),
	eFmodItemType_EventParameter    = BIT(2),
	eFmodItemType_Snapshot          = BIT(3),
	eFmodItemType_SnapshotParameter = BIT(4),
	eFmodItemType_Bank              = BIT(5),
	eFmodItemType_Return            = BIT(6),
	eFmodItemType_Group             = BIT(7),
};

class CFmodFolder final : public IAudioSystemItem
{
public:
	CFmodFolder(const string& name, CID id) : IAudioSystemItem(name, id, eFmodItemType_Folder) {}
	virtual bool     IsConnected() const override { return true; }
	virtual ItemType GetType() const override     { return eFmodItemType_Folder; }
};

class CFmodGroup final : public IAudioSystemItem
{
public:
	CFmodGroup(const string& name, CID id) : IAudioSystemItem(name, id, eFmodItemType_Group) {}
	virtual bool     IsConnected() const override { return true; }
	virtual ItemType GetType() const override     { return eFmodItemType_Group; }
};

class CImplementationSettings_fmod final : public IImplementationSettings
{
public:
	CImplementationSettings_fmod()
		: m_projectPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "fmod_project")
		, m_soundBanksPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "fmod") {}
	virtual const char* GetSoundBanksPath() const { return m_soundBanksPath.c_str(); }
	virtual const char* GetProjectPath() const    { return m_projectPath.c_str(); }
	virtual void        SetProjectPath(const char* szPath);

	void                Serialize(Serialization::IArchive& ar)
	{
		ar(m_projectPath, "projectPath", "Project Path");
	}

private:
	string       m_projectPath;
	const string m_soundBanksPath;
};

class CAudioSystemEditor_fmod final : public IAudioSystemEditor
{
public:
	CAudioSystemEditor_fmod();
	virtual ~CAudioSystemEditor_fmod();

	//////////////////////////////////////////////////////////
	// IAudioSystemEditor implementation
	/////////////////////////////////////////////////////////
	virtual void                     Reload(bool bPreserveConnectionStatus = true) override;
	virtual IAudioSystemItem*        GetRoot() override { return &m_root; }
	virtual IAudioSystemItem*        GetControl(CID id) const override;
	virtual EItemType                ImplTypeToATLType(ItemType type) const override;
	virtual TImplControlTypeMask     GetCompatibleTypes(EItemType controlType) const override;
	virtual ConnectionPtr            CreateConnectionToControl(EItemType controlType, IAudioSystemItem* pMiddlewareControl) override;
	virtual ConnectionPtr            CreateConnectionFromXMLNode(XmlNodeRef pNode, EItemType controlType) override;
	virtual XmlNodeRef               CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EItemType eATLControlType) override;
	virtual const char*              GetTypeIcon(ItemType type) const override;
	virtual string                   GetName() const override;
	virtual void                     EnableConnection(ConnectionPtr pConnection) override;
	virtual void                     DisableConnection(ConnectionPtr pConnection) override;
	virtual IImplementationSettings* GetSettings() override { return &m_settings; }
	//////////////////////////////////////////////////////////

private:
	IAudioSystemItem* CreateItem(EFmodItemType type, IAudioSystemItem* pParent, const string& name);
	CID               GetId(EFmodItemType type, const string& name, IAudioSystemItem* pParent) const;
	string            GetFullPathName(IAudioSystemItem* pItem) const;
	string            GetTypeName(EFmodItemType type) const;

	void              ParseFolder(const string& folderPath);
	void              ParseFile(const string& filepath);

	IAudioSystemItem* GetContainer(const string& id, EFmodItemType type);
	IAudioSystemItem* LoadContainer(XmlNodeRef pNode, EFmodItemType type, const string& relationshipParamName);
	IAudioSystemItem* LoadFolder(XmlNodeRef pNode);
	IAudioSystemItem* LoadGroup(XmlNodeRef pNode);

	IAudioSystemItem* LoadItem(XmlNodeRef pNode, EFmodItemType type);
	IAudioSystemItem* LoadEvent(XmlNodeRef pNode);
	IAudioSystemItem* LoadSnapshot(XmlNodeRef pNode);
	IAudioSystemItem* LoadReturn(XmlNodeRef pNode);
	IAudioSystemItem* LoadParameter(XmlNodeRef pNode, EFmodItemType type, IAudioSystemItem& parentEvent);

	IAudioSystemItem* GetItemFromPath(const string& fullpath);
	IAudioSystemItem* CreatePlaceholderFolderPath(const string& path);

	std::map<CID, int>                  m_connectionsByID;

	IAudioSystemItem                    m_root;
	std::vector<IAudioSystemItem*>      m_items;
	std::map<string, IAudioSystemItem*> m_containerIdMap;
	CImplementationSettings_fmod        m_settings;
};
}
