// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Asset.h"

#include <IConnection.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
struct IItem;
} // namespace Impl

class CControl final : public CAsset
{
public:

	explicit CControl(string const& name, ControlId const id, EAssetType const type);
	~CControl();

	CControl() = delete;

	// CAsset
	virtual void SetName(string const& name) override;
	virtual void SetDescription(string const& description) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CAsset

	ControlId         GetId() const    { return m_id; }

	Scope             GetScope() const { return m_scope; }
	void              SetScope(Scope const scope);

	bool              IsAutoLoad() const { return m_isAutoLoad; }
	void              SetAutoLoad(bool const isAutoLoad);

	float             GetRadius() const { return m_radius; }
	void              SetRadius(float const radius);

	ControlIds const& GetSelectedConnections() const                           { return m_selectedConnectionIds; }
	void              SetSelectedConnections(ControlIds selectedConnectionIds) { m_selectedConnectionIds = selectedConnectionIds; }

	size_t            GetConnectionCount() const                               { return m_connections.size(); }
	void              AddConnection(ConnectionPtr const pConnection);
	void              RemoveConnection(ConnectionPtr const pConnection);
	void              RemoveConnection(Impl::IItem* const pIItem);
	void              ClearConnections();
	ConnectionPtr     GetConnectionAt(size_t const index) const;
	ConnectionPtr     GetConnection(ControlId const id) const;
	ConnectionPtr     GetConnection(Impl::IItem const* const pIItem) const;
	void              BackupAndClearConnections();
	void              ReloadConnections();
	void              LoadConnectionFromXML(XmlNodeRef const xmlNode, int const platformIndex = -1);

private:

	void MatchRadiusToAttenuation();

	void SignalControlAboutToBeModified();
	void SignalControlModified();
	void SignalConnectionAdded(Impl::IItem* const pIItem);
	void SignalConnectionRemoved(Impl::IItem* const pIItem);
	void SignalConnectionModified();

	ControlId const            m_id;
	Scope                      m_scope = 0;
	std::vector<ConnectionPtr> m_connections;
	float                      m_radius = 0.0f;
	bool                       m_isAutoLoad = true;

	using XMLNodes = std::vector<XmlNodeRef>;
	std::map<int, XMLNodes> m_rawConnections;
	ControlIds              m_selectedConnectionIds;
};
} // namespace ACE
