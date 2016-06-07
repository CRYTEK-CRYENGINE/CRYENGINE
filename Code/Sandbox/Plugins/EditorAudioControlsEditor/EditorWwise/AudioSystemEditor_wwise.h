// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IAudioSystemEditor.h"
#include "IAudioConnection.h"
#include "IAudioSystemItem.h"
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>

namespace ACE
{
class IAudioConnectionInspectorPanel;

class CRtpcConnection : public IAudioConnection
{
public:
	explicit CRtpcConnection(CID nID)
		: IAudioConnection(nID)
		, mult(1.0f)
		, shift(0.0f)
	{}

	virtual ~CRtpcConnection() {}

	virtual bool HasProperties() override { return true; }

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(mult, "mult", "Multiply");
		ar(shift, "shift", "Shift");
	}

	float mult;
	float shift;
};
typedef std::shared_ptr<CRtpcConnection> RtpcConnectionPtr;

class CStateToRtpcConnection : public IAudioConnection
{
public:
	explicit CStateToRtpcConnection(CID nID)
		: IAudioConnection(nID)
		, value(0.0f)
	{}

	virtual ~CStateToRtpcConnection() {}

	virtual bool HasProperties() override { return true; }

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(value, "value", "Value");
	}

	float value;
};
typedef std::shared_ptr<CStateToRtpcConnection> StateConnectionPtr;

class CImplementationSettings_wwise final : public IImplementationSettings
{
public:
	CImplementationSettings_wwise()
		: m_projectPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "wwise_project")
		, m_soundBanksPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "wwise") { }
	virtual const char* GetSoundBanksPath() const { return m_soundBanksPath.c_str(); }
	virtual const char* GetProjectPath() const    { return m_projectPath.c_str(); }
	virtual void        SetProjectPath(const char* szPath);

	void                Serialize(Serialization::IArchive& ar)
	{
		ar(m_projectPath, "projectPath", "Project Path");
	}

private:
	string m_projectPath;
	const string m_soundBanksPath;
};

class CAudioSystemEditor_wwise final : public IAudioSystemEditor
{
	friend class CAudioWwiseLoader;

public:
	CAudioSystemEditor_wwise();
	~CAudioSystemEditor_wwise();

	//////////////////////////////////////////////////////////
	// IAudioSystemEditor implementation
	/////////////////////////////////////////////////////////
	virtual void                     Reload(bool bPreserveConnectionStatus = true) override;
	virtual IAudioSystemItem*        GetRoot() override { return &m_rootControl; }
	virtual IAudioSystemItem*        GetControl(CID id) const override;
	virtual EACEControlType          ImplTypeToATLType(ItemType type) const override;
	virtual TImplControlTypeMask     GetCompatibleTypes(EACEControlType eATLControlType) const override;
	virtual ConnectionPtr            CreateConnectionToControl(EACEControlType eATLControlType, IAudioSystemItem* pMiddlewareControl) override;
	virtual ConnectionPtr            CreateConnectionFromXMLNode(XmlNodeRef pNode, EACEControlType eATLControlType) override;
	virtual XmlNodeRef               CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EACEControlType eATLControlType) override;
	virtual const char*              GetTypeIcon(ItemType type) const override;
	virtual string                   GetName() const override;
	virtual void                     EnableConnection(ConnectionPtr pConnection) override;
	virtual void                     DisableConnection(ConnectionPtr pConnection) override;
	virtual IImplementationSettings* GetSettings() override { return &m_settings; }
	//////////////////////////////////////////////////////////

private:
	IAudioSystemItem* GetControlByName(const string& sName, bool bIsLocalised = false, IAudioSystemItem* pParent = nullptr) const;
	IAudioSystemItem* CreateControl(const SControlDef& controlDefinition);

	// Gets the ID of the control given its name. As controls can have the same name
	// if they're under different parents, the name of the parent is also needed (if there is one)
	CID  GetID(const string& sName) const;
	void UpdateConnectedStatus();

	IAudioSystemItem m_rootControl;

	typedef std::shared_ptr<IAudioSystemItem> TControlPtr;
	typedef std::map<CID, TControlPtr>        TControlMap;
	TControlMap m_controls;

	typedef std::map<CID, int> TConnectionsMap;
	TConnectionsMap               m_connectionsByID;
	CImplementationSettings_wwise m_settings;
};
}
