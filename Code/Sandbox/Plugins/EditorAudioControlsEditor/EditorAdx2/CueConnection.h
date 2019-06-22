// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/IConnection.h"

#include <PoolObject.h>

namespace ACE
{
namespace Impl
{
namespace Adx2
{
class CCueConnection final : public IConnection, public CryAudio::CPoolObject<CCueConnection, stl::PSyncNone>
{
public:

	CCueConnection() = delete;
	CCueConnection(CCueConnection const&) = delete;
	CCueConnection(CCueConnection&&) = delete;
	CCueConnection& operator=(CCueConnection const&) = delete;
	CCueConnection& operator=(CCueConnection&&) = delete;

	enum class EActionType : CryAudio::EnumFlagsType
	{
		Start,
		Stop,
		Pause,
		Resume, };

	explicit CCueConnection(ControlId const id, string const& cueSheetName, EActionType const actionType)
		: m_id(id)
		, m_cueSheetName(cueSheetName)
		, m_actionType(actionType)
	{}

	explicit CCueConnection(ControlId const id)
		: m_id(id)
		, m_cueSheetName("")
		, m_actionType(EActionType::Start)
	{}

	virtual ~CCueConnection() override = default;

	// CBaseConnection
	virtual ControlId GetID() const override final   { return m_id; }
	virtual bool      HasProperties() const override { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	string const& GetCueSheetName() const { return m_cueSheetName; }
	EActionType   GetActionType() const   { return m_actionType; }

private:

	ControlId const m_id;
	string const    m_cueSheetName;
	EActionType     m_actionType;
};
} // namespace Adx2
} // namespace Impl
} // namespace ACE
