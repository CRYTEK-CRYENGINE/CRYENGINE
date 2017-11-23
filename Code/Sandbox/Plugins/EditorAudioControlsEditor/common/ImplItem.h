// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SystemTypes.h"

#include <CryCore/Platform/platform.h>
#include <CryString/CryString.h>

namespace ACE
{
class CImplItem
{
public:

	CImplItem() = default;
	CImplItem(string const& name, CID const id, ItemType const type)
		: m_name(name)
		, m_id(id)
		, m_type(type)
	{
	}

	virtual ~CImplItem() = default;

	// unique id for this control
	CID        GetId() const                            { return m_id; }
	void       SetId(CID const id)                      { m_id = id; }

	ItemType   GetType() const                          { return m_type; }
	void       SetType(ItemType const type)             { m_type = type; }

	string     GetName() const                          { return m_name; }
	void       SetName(string const& name)              { m_name = name; }

	bool       IsPlaceholder() const                    { return m_isPlaceholder; }
	void       SetPlaceholder(bool const isPlaceholder) { m_isPlaceholder = isPlaceholder; }

	bool       IsLocalised() const                      { return m_isLocalised; }
	void       SetLocalised(bool const isLocalised)     { m_isLocalised = isLocalised; }

	bool       IsConnected() const                      { return m_isConnected; }
	void       SetConnected(bool const isConnected)     { m_isConnected = isConnected; }

	bool       IsContainer() const                      { return m_isContainer; }
	void       SetContainer(bool const isContainer)     { m_isContainer = isContainer; }

	size_t     ChildCount() const                       { return m_children.size(); }
	void       AddChild(CImplItem* const pChild)        { m_children.emplace_back(pChild); pChild->SetParent(this); }
	void       RemoveChild(CImplItem* const pChild)     { stl::find_and_erase(m_children, pChild); pChild->SetParent(nullptr); }
	CImplItem* GetChildAt(size_t const index) const     { return m_children[index]; }
	CImplItem* GetParent() const                        { return m_pParent; }
	void       SetParent(CImplItem* const pParent)      { m_pParent = pParent; }

	float      GetRadius() const                        { return m_radius; }
	void       SetRadius(float const radius)            { m_radius = radius; }

private:

	CID                     m_id = ACE_INVALID_ID;
	ItemType                m_type = AUDIO_SYSTEM_INVALID_TYPE;
	string                  m_name;
	bool                    m_isPlaceholder = false;
	bool                    m_isLocalised = false;
	bool                    m_isConnected = false;
	bool                    m_isContainer = false;
	std::vector<CImplItem*> m_children;
	CImplItem*              m_pParent = nullptr;
	float                   m_radius = 0.0f;
};
} // namespace ACE
