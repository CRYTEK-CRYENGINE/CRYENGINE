// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Asset.h"

#include <PoolObject.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
struct IItem;
} // namespace Impl

struct IConnection;

class CControl final : public CAsset, public CryAudio::CPoolObject<CControl, stl::PSyncNone>
{
public:

	CControl() = delete;
	CControl(CControl const&) = delete;
	CControl(CControl&&) = delete;
	CControl& operator=(CControl const&) = delete;
	CControl& operator=(CControl&&) = delete;

	explicit CControl(string const& name, ControlId const id, EAssetType const type)
		: CAsset(name, id, type)
		, m_contextId(CryAudio::GlobalContextId)
		, m_isAutoLoad(true)
	{}

	virtual ~CControl() override;

	// CAsset
	virtual void SetName(string const& name) override;
	virtual void SetDescription(string const& description) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CAsset

	CryAudio::ContextId GetContextId() const { return m_contextId; }
	void                SetContextId(CryAudio::ContextId const contextId);

	bool                IsAutoLoad() const { return m_isAutoLoad; }
	void                SetAutoLoad(bool const isAutoLoad);

	ControlIds const&   GetSelectedConnections() const                           { return m_selectedConnectionIds; }
	void                SetSelectedConnections(ControlIds selectedConnectionIds) { m_selectedConnectionIds = selectedConnectionIds; }

	size_t              GetConnectionCount() const                               { return m_connections.size(); }
	void                AddConnection(IConnection* const pIConnection);
	void                RemoveConnection(Impl::IItem* const pIItem);
	void                ClearConnections();
	IConnection*        GetConnectionAt(size_t const index) const;
	IConnection*        GetConnection(ControlId const id) const;
	void                BackupAndClearConnections();
	void                ReloadConnections();
	void                LoadConnectionFromXML(XmlNodeRef const& node);

private:

	void SignalOnBeforeControlModified();
	void SignalOnAfterControlModified();
	void SignalConnectionAdded();
	void SignalConnectionRemoved();
	void SignalConnectionModified();

	CryAudio::ContextId       m_contextId;
	std::vector<IConnection*> m_connections;
	bool                      m_isAutoLoad;

	std::vector<XmlNodeRef>   m_rawConnections;
	ControlIds                m_selectedConnectionIds;
};
} // namespace ACE
