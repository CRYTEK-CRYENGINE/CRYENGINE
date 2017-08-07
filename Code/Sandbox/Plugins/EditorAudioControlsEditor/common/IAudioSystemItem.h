// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>
#include <CryString/CryString.h>
#include "IAudioConnection.h"
#include "ACETypes.h"

namespace ACE
{
class IAudioSystemItem
{
public:
	IAudioSystemItem() = default;
	IAudioSystemItem(const string& name, CID id, ItemType type)
		: m_name(name)
		, m_id(id)
		, m_type(type)
	{
	}

	virtual ~IAudioSystemItem() = default;

	// unique id for this control
	CID              GetId() const          { return m_id; }
	void             SetId(CID id)          { m_id = id; }

	virtual ItemType GetType() const        { return m_type; }
	void             SetType(ItemType type) { m_type = type; }

	string           GetName() const        { return m_name; }
	void             SetName(const string& name)
	{
		if (name != m_name)
		{
			m_name = name;
		}
	}

	virtual bool      IsPlaceholder() const                       { return m_bPlaceholder; }
	void              SetPlaceholder(bool bIsPlaceholder)         { m_bPlaceholder = bIsPlaceholder; }

	virtual bool      IsLocalised() const                         { return m_bLocalised; }
	void              SetLocalised(bool bIsLocalised)             { m_bLocalised = bIsLocalised; }

	virtual bool      IsConnected() const                         { return m_bConnected; }
	void              SetConnected(bool bConnected)               { m_bConnected = bConnected; }

	size_t            ChildCount() const                          { return m_children.size(); }
	void              AddChild(IAudioSystemItem* const pChild)    { m_children.push_back(pChild); pChild->SetParent(this); }
	void              RemoveChild(IAudioSystemItem* const pChild) { stl::find_and_erase(m_children, pChild); pChild->SetParent(nullptr); }
	IAudioSystemItem* GetChildAt(size_t const index) const        { return m_children[index]; }
	void              SetParent(IAudioSystemItem* pParent)        { m_parent = pParent; }
	IAudioSystemItem* GetParent() const                           { return m_parent; }

	void              SetRadius(float radius)                     { m_radius = radius; }
	float             GetRadius() const                           { return m_radius; }

private:
	CID                            m_id = ACE_INVALID_ID;
	ItemType                       m_type = AUDIO_SYSTEM_INVALID_TYPE;
	string                         m_name;
	bool                           m_bPlaceholder = false;
	bool                           m_bLocalised = false;
	bool                           m_bConnected = false;
	std::vector<IAudioSystemItem*> m_children;
	IAudioSystemItem*              m_parent = nullptr;
	float                          m_radius = 0.0f;
};
} // namespace ACE
