// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/IConnection.h"
#include <PoolObject.h>

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
class CEventConnection final : public IConnection, public CryAudio::CPoolObject<CEventConnection, stl::PSyncNone>
{
public:

	enum class EActionType : CryAudio::EnumFlagsType
	{
		Start,
		Stop, };

	CEventConnection() = delete;
	CEventConnection(CEventConnection const&) = delete;
	CEventConnection(CEventConnection&&) = delete;
	CEventConnection& operator=(CEventConnection const&) = delete;
	CEventConnection& operator=(CEventConnection&&) = delete;

	explicit CEventConnection(ControlId const id)
		: m_id(id)
		, m_actionType(EActionType::Start)
		, m_loopCount(1)
		, m_isInfiniteLoop(false)
	{}

	virtual ~CEventConnection() override = default;

	// IConnection
	virtual ControlId GetID() const override         { return m_id; }
	virtual bool      HasProperties() const override { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~IConnection

	void        SetActionType(EActionType const type)  { m_actionType = type; }
	EActionType GetActionType() const                  { return m_actionType; }

	void        SetInfiniteLoop(bool const isInfinite) { m_isInfiniteLoop = isInfinite; }
	bool        IsInfiniteLoop() const                 { return m_isInfiniteLoop; }

	void        SetLoopCount(uint32 const loopCount)   { m_loopCount = loopCount; }
	uint32      GetLoopCount() const                   { return m_loopCount; }

private:

	ControlId const m_id;
	EActionType     m_actionType;
	bool            m_isInfiniteLoop;
	uint32          m_loopCount;
};
} // namespace PortAudio
} // namespace Impl
} // namespace ACE
