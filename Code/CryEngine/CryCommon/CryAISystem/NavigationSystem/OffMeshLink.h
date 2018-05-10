// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace MNM
{

struct OffMeshLink : public _i_reference_target_t
{
public:

	enum LinkType
	{
		eLinkType_Invalid = -1,
		eLinkType_SmartObject,
		eLinkType_Custom,
	};

	virtual ~OffMeshLink() {};

protected:

	OffMeshLink(LinkType _linkType, EntityId _entityId)
		: m_linkType(_linkType)
		, m_entityId(_entityId)
	{
	}

	// Do not implement.
	OffMeshLink() /*= delete*/;

public:

	ILINE LinkType      GetLinkType() const               { return m_linkType; }
	ILINE void          SetLinkID(OffMeshLinkID linkID)   { m_linkID = linkID; }
	ILINE OffMeshLinkID GetLinkId() const                 { return m_linkID; }
	ILINE EntityId      GetEntityIdForOffMeshLink() const { return m_entityId; }

	template<class LinkDataClass>
	LinkDataClass* CastTo()
	{
		return (m_linkType == LinkDataClass::GetType()) ? static_cast<LinkDataClass*>(this) : NULL;
	}

	template<class LinkDataClass>
	const LinkDataClass* CastTo() const
	{
		return (m_linkType == LinkDataClass::GetType()) ? static_cast<const LinkDataClass*>(this) : NULL;
	}

	virtual bool         CanUse(const IEntity* pRequester, float* costMultiplier) const = 0;

	virtual OffMeshLink* Clone() const = 0;
	virtual Vec3         GetStartPosition() const = 0;
	virtual Vec3         GetEndPosition() const = 0;

	virtual bool         IsEnabled() const { return true; }

protected:
	LinkType      m_linkType;
	EntityId      m_entityId;
	OffMeshLinkID m_linkID;
};

DECLARE_SHARED_POINTERS(OffMeshLink);

} // namespace MNM
