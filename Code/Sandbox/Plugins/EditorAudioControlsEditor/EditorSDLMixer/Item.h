// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IItem.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
enum class EItemType
{
	None,
	Event,
	Folder,
};

class CItem final : public IItem
{
public:

	explicit CItem(
	  string const& name,
	  ControlId const id,
	  EItemType const type,
	  EItemFlags const flags = EItemFlags::None,
	  EPakStatus const pakStatus = EPakStatus::None,
	  string const& filePath = "")
		: m_name(name)
		, m_id(id)
		, m_type(type)
		, m_flags(flags)
		, m_pakStatus(pakStatus)
		, m_filePath(filePath)
		, m_pParent(nullptr)
	{}

	virtual ~CItem() override = default;

	CItem() = delete;

	// IItem
	virtual ControlId     GetId() const override                        { return m_id; }
	virtual string const& GetName() const override                      { return m_name; }
	virtual float         GetRadius() const override                    { return 0.0f; }
	virtual size_t        GetNumChildren() const override               { return m_children.size(); }
	virtual IItem*        GetChildAt(size_t const index) const override { return m_children[index]; }
	virtual IItem*        GetParent() const override                    { return m_pParent; }
	virtual EItemFlags    GetFlags() const override                     { return m_flags; }
	// ~IItem

	EItemType     GetType() const                          { return m_type; }
	string const& GetFilePath() const                      { return m_filePath; }

	void          SetFlags(EItemFlags const flags)         { m_flags = flags; }

	void          SetPakStatus(EPakStatus const pakStatus) { m_pakStatus = pakStatus; }
	EPakStatus    GetPakStatus() const                     { return m_pakStatus; }

	void          AddChild(CItem* const pChild);
	void          Clear();

private:

	void SetParent(CItem* const pParent) { m_pParent = pParent; }

	ControlId const     m_id;
	EItemType const     m_type;
	string const        m_name;
	string const        m_filePath;
	std::vector<CItem*> m_children;
	CItem*              m_pParent;
	EItemFlags          m_flags;
	EPakStatus          m_pakStatus;
};
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE

