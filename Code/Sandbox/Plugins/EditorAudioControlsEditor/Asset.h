// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <SharedData.h>

#include <CryString/CryString.h>
#include <CrySerialization/Forward.h>

namespace ACE
{
enum class EAssetFlags
{
	None                     = 0,
	IsDefaultControl         = BIT(0),
	IsInternalControl        = BIT(1),
	IsModified               = BIT(2),
	HasPlaceholderConnection = BIT(3),
	HasConnection            = BIT(4),
	HasControl               = BIT(5),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EAssetFlags);

class CAsset
{
public:

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

	explicit CAsset(string const& name, EAssetType const type);

	CAsset() = delete;

	CAsset*          m_pParent = nullptr;
	Assets           m_children;
	string           m_name;
	string           m_description = "";
	EAssetType const m_type;
	EAssetFlags      m_flags;
};
} // namespace ACE
