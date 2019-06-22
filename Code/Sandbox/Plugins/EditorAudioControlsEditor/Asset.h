// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"

#include <CryString/CryString.h>
#include <CrySerialization/Forward.h>

namespace ACE
{
enum class EAssetFlags : CryAudio::EnumFlagsType
{
	None = 0,
	IsDefaultControl = BIT(0),
	IsInternalControl = BIT(1),
	IsHiddenInResourceSelector = BIT(2),
	IsModified = BIT(3),
	HasPlaceholderConnection = BIT(4),
	HasConnection = BIT(5),
	HasControl = BIT(6), };
CRY_CREATE_ENUM_FLAG_OPERATORS(EAssetFlags);

class CAsset
{
public:

	CAsset() = delete;
	CAsset(CAsset const&) = delete;
	CAsset(CAsset&&) = delete;
	CAsset& operator=(CAsset const&) = delete;
	CAsset& operator=(CAsset&&) = delete;

	virtual ~CAsset() {}

	ControlId     GetId() const     { return m_id; }
	EAssetType    GetType() const   { return m_type; }

	CAsset*       GetParent() const { return m_pParent; }
	void          SetParent(CAsset* const pParent);

	size_t        ChildCount() const { return m_children.size(); }
	CAsset*       GetChild(size_t const index) const;
	void          AddChild(CAsset* const pChildControl);
	void          RemoveChild(CAsset const* const pChildControl);

	string const& GetName() const { return m_name; }
	virtual void  SetName(string const& name);

	void          UpdateNameOnMove(CAsset* const pParent);

	string const& GetDescription() const { return m_description; }
	virtual void  SetDescription(string const& description);

	EAssetFlags   GetFlags() const                  { return m_flags; }
	void          SetFlags(EAssetFlags const flags) { m_flags = flags; }

	virtual void  SetModified(bool const isModified, bool const isForced = false);

	string        GetFullHierarchyName() const;

	bool          HasDefaultControlChildren(AssetNames& names) const;

	virtual void  Serialize(Serialization::IArchive& ar);

protected:

	explicit CAsset(string const& name, ControlId const id, EAssetType const type)
		: m_id(id)
		, m_pParent(nullptr)
		, m_name(name)
		, m_description("")
		, m_type(type)
		, m_flags(EAssetFlags::None)
	{}

	ControlId        m_id;
	CAsset*          m_pParent;
	Assets           m_children;
	string           m_name;
	string           m_description;
	EAssetType const m_type;
	EAssetFlags      m_flags;
};
} // namespace ACE
