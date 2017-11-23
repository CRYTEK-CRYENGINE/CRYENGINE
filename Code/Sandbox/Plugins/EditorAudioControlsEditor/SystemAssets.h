// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ImplConnection.h>
#include <SystemTypes.h>

#include <CryString/CryString.h>
#include <CrySystem/XML/IXml.h>
#include <CrySerialization/Forward.h>

namespace ACE
{
class CImplItem;

struct SRawConnectionData
{
	SRawConnectionData(XmlNodeRef const node, bool const isValid_)
		: xmlNode(node)
		, isValid(isValid_) {}

	XmlNodeRef xmlNode;
	bool       isValid; // indicates if the connection is valid for the currently loaded middle-ware
};

using XMLNodeList = std::vector<SRawConnectionData>;

class CSystemAsset
{
public:

	CSystemAsset(string const& name, ESystemItemType const type);

	ESystemItemType GetType() const { return m_type; }

	CSystemAsset*   GetParent() const { return m_pParent; }
	void            SetParent(CSystemAsset* const pParent);

	size_t          ChildCount() const { return m_children.size(); }
	CSystemAsset*   GetChild(size_t const index) const;
	void            AddChild(CSystemAsset* const pChildControl);
	void            RemoveChild(CSystemAsset const* const pChildControl);

	string          GetName() const { return m_name; }
	virtual void    SetName(string const& name);

	string          GetDescription() const { return m_description; }
	virtual void    SetDescription(string const& description);

	bool            IsHiddenDefault() const { return m_isHiddenDefault; }
	void            SetHiddenDefault(bool const isHiddenDefault);

	virtual bool    IsModified() const { return m_isModified; }
	virtual void    SetModified(bool const isModified, bool const isForced = false);

	bool            HasPlaceholderConnection() const { return m_hasPlaceholderConnection; }
	void            SetHasPlaceholderConnection(bool const hasPlaceholder) { m_hasPlaceholderConnection = hasPlaceholder; }
	
	bool            HasConnection() const { return m_hasConnection; }
	void            SetHasConnection(bool const hasConnection) { m_hasConnection = hasConnection; }
	
	bool            HasControl() const { return m_hasControl; }
	void            SetHasControl(bool const hasControl) { m_hasControl = hasControl; }

	virtual void    Serialize(Serialization::IArchive& ar);

protected:

	CSystemAsset*              m_pParent = nullptr;
	std::vector<CSystemAsset*> m_children;
	string                     m_name;
	string                     m_description = "";
	ESystemItemType const      m_type;
	bool                       m_isHiddenDefault = false;
	bool                       m_isModified = false;
	bool                       m_hasPlaceholderConnection = false;
	bool                       m_hasConnection = false;
	bool                       m_hasControl = false;
};

class CSystemControl final : public CSystemAsset
{
public:

	CSystemControl() = default;
	CSystemControl(string const& name, CID const id, ESystemItemType const type);
	~CSystemControl();

	CID           GetId() const            { return m_id; }

	virtual void  SetName(string const& name) override;

	virtual void  SetDescription(string const& description) override;

	Scope         GetScope() const { return m_scope; }
	void          SetScope(Scope const scope);

	bool          IsAutoLoad() const { return m_isAutoLoad; }
	void          SetAutoLoad(bool const isAutoLoad);

	float         GetRadius() const { return m_radius; }
	void          SetRadius(float const radius);

	float         GetOcclusionFadeOutDistance() const { return m_occlusionFadeOutDistance; }
	void          SetOcclusionFadeOutDistance(float const fadeOutArea);

	size_t        GetConnectionCount() const { return m_connectedControls.size(); }
	void          AddConnection(ConnectionPtr const pConnection);
	void          RemoveConnection(ConnectionPtr const pConnection);
	void          RemoveConnection(CImplItem* const pImplControl);
	void          ClearConnections();
	ConnectionPtr GetConnectionAt(int const index) const;
	ConnectionPtr GetConnection(CID const id) const;
	ConnectionPtr GetConnection(CImplItem const* const pImplControl) const;
	void          ReloadConnections();
	void          LoadConnectionFromXML(XmlNodeRef const xmlNode, int const platformIndex = -1);

	void          MatchRadiusToAttenuation();
	bool          IsMatchRadiusToAttenuationEnabled() const { return m_matchRadiusAndAttenuation; }
	void          SetMatchRadiusToAttenuationEnabled(bool const isEnabled) { m_matchRadiusAndAttenuation = isEnabled; }

	virtual void  Serialize(Serialization::IArchive& ar) override;

	// All the raw connection nodes. Used for reloading the data when switching middleware.
	void          AddRawXMLConnection(XmlNodeRef const xmlNode, bool const isValid, int const platformIndex = -1);
	XMLNodeList&  GetRawXMLConnections(int const platformIndex = -1);

private:

	void SignalControlAboutToBeModified();
	void SignalControlModified();
	void SignalConnectionAdded(CImplItem* const pImplControl);
	void SignalConnectionRemoved(CImplItem* const pImplControl);
	void SignalConnectionModified();

	CID                        m_id = ACE_INVALID_ID;
	Scope                      m_scope = 0;
	std::vector<ConnectionPtr> m_connectedControls;
	float                      m_radius = 0.0f;
	float                      m_occlusionFadeOutDistance = 0.0f;
	bool                       m_isAutoLoad = true;
	bool                       m_matchRadiusAndAttenuation = true;

	std::map<int, XMLNodeList> m_connectionNodes;
};

class CSystemLibrary final : public CSystemAsset
{
public:

	CSystemLibrary(string const& name);

	virtual void SetModified(bool const isModified, bool const isForced = false) override;
};

class CSystemFolder final : public CSystemAsset
{
public:

	CSystemFolder(string const& name);
};
} // namespace ACE
