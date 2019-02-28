// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptTimer.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/IEnvInterface.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/SerializationUtils/SerializationUtils.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>

namespace Schematyc
{
CScriptTimer::CScriptTimer()
	: CScriptElementBase(EScriptElementFlags::None)
	, m_params(STimerDuration(1.0f), ETimerFlags::AutoStart)
{}

CScriptTimer::CScriptTimer(const CryGUID& guid, const char* szName)
	: CScriptElementBase(guid, szName, EScriptElementFlags::None)
	, m_params(STimerDuration(1.0f), ETimerFlags::AutoStart)
{}

void CScriptTimer::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const {}

void CScriptTimer::RemapDependencies(IGUIDRemapper& guidRemapper) {}

void CScriptTimer::ProcessEvent(const SScriptEvent& event)
{
	CScriptElementBase::ProcessEvent(event);

	switch (event.id)
	{
	case EScriptEventId::EditorAdd:
	case EScriptEventId::EditorPaste:
		{
			m_userDocumentation.SetCurrentUserAsAuthor();
			break;
		}
	}
}

void CScriptTimer::Serialize(Serialization::IArchive& archive)
{
	// #SchematycTODO : Shouldn't this be handled by CScriptElementBase itself?
	CScriptElementBase::Serialize(archive);
	CMultiPassSerializer::Serialize(archive);
	CScriptElementBase::SerializeExtensions(archive);
}

const char* CScriptTimer::GetAuthor() const
{
	return m_userDocumentation.author.c_str();
}

const char* CScriptTimer::GetDescription() const
{
	return m_userDocumentation.description.c_str();
}

STimerParams CScriptTimer::GetParams() const
{
	STimerParams params = m_params;
	ValidateDuration(params.duration, nullptr, true);
	return params;
}

void CScriptTimer::Load(Serialization::IArchive& archive, const ISerializationContext& context)
{
	SerializeParams(archive);

	archive(m_userDocumentation, "userDocumentation");
}

void CScriptTimer::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	SerializeParams(archive);

	archive(m_userDocumentation, "userDocumentation");
}

void CScriptTimer::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	SerializeParams(archive);

	archive(m_userDocumentation, "userDocumentation", "Documentation");
}

void CScriptTimer::SerializeParams(Serialization::IArchive& archive)
{
	// #SchematycTODO : Create STimerParams::Serialize() function?

	const ETimerUnits prevUnits = m_params.duration.units;
	archive(m_params.duration.units, "units", "Units");
	switch (m_params.duration.units)
	{
	case ETimerUnits::Frames:
		{
			if (m_params.duration.units != prevUnits)
			{
				m_params.duration = STimerDuration(uint32(1));
			}
			archive(m_params.duration.frames, "frames", "Duration");
			break;
		}
	case ETimerUnits::Seconds:
		{
			if (m_params.duration.units != prevUnits)
			{
				m_params.duration = STimerDuration(1.0f);
			}
			archive(m_params.duration.seconds, "seconds", "Duration");
			break;
		}
	case ETimerUnits::Random:
		{
			if (m_params.duration.units != prevUnits)
			{
				m_params.duration = STimerDuration(1.0f, 1.0f);
			}
			archive(m_params.duration.range.min, "min", "Minimum");
			archive(m_params.duration.range.max, "max", "Maximum");
			break;
		}
	}
	ValidateDuration(m_params.duration, &archive, false);
	archive(m_params.flags, "flags", "Flags");
}

void CScriptTimer::ValidateDuration(STimerDuration& duration, Serialization::IArchive* pArchive, bool bApplyCorrections) const
{
	switch (duration.units)
	{
	case ETimerUnits::Frames:
		{
			const uint32 min = 1;
			const uint32 max = 1000;
			if (duration.frames < min)
			{
				if (pArchive)
				{
					pArchive->warning(duration.frames, "Minimum delay is %d frames!", min);
				}
				if (bApplyCorrections)
				{
					duration.frames = min;
				}
			}
			else if (duration.frames > max)
			{
				if (pArchive)
				{
					pArchive->warning(duration.frames, "Maximum delay is %d frames!", max);
				}
				if (bApplyCorrections)
				{
					duration.frames = max;
				}
			}
			break;
		}
	case ETimerUnits::Seconds:
		{
			const float min = 0.2f;
			const float max = 1000.0f;
			if (duration.seconds < min)
			{
				if (pArchive)
				{
					pArchive->warning(duration.seconds, "Minimum delay is %f seconds!", min);
				}
				if (bApplyCorrections)
				{
					duration.seconds = min;
				}
			}
			else if (duration.seconds > max)
			{
				if (pArchive)
				{
					pArchive->warning(duration.seconds, "Maximum delay is %f seconds!", max);
				}
				if (bApplyCorrections)
				{
					duration.seconds = max;
				}
			}
			break;
		}
	case ETimerUnits::Random:
		{
			const float min = 0.2f;
			const float max = 1000.0f;
			if (duration.range.min < min)
			{
				if (pArchive)
				{
					pArchive->warning(duration.range.min, "Minimum delay is %f seconds!", min);
				}
				if (bApplyCorrections)
				{
					duration.range.min = min;
				}
			}
			if (duration.range.max > max)
			{
				if (pArchive)
				{
					pArchive->warning(duration.range.max, "Maximum delay is %f seconds!", max);
				}
				if (bApplyCorrections)
				{
					duration.range.max = max;
				}
			}
			if (duration.range.min > duration.range.max)
			{
				if (pArchive)
				{
					pArchive->warning(duration.range.max, "Minimum must never exceed maximum!");
				}
				if (bApplyCorrections)
				{
					duration.range.max = duration.range.min;
				}
			}
		}
	}
}
} // Schematyc
