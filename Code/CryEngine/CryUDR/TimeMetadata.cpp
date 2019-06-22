// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{
		bool CTimeMetadata::Initialize()
		{
			// We cannot assert here because the default constructor is used by yasli serialization.
			if (gEnv && gEnv->pTimer)
			{
				m_timestamp = CUDRSystem::GetInstance().GetElapsedTime();
				m_frameID = gEnv->nMainFrameID;
				return true;
			}
			return false;
		}

		uint32 CTimeMetadata::GetFrameID() const
		{
			return m_frameID;
		}

		CTimeValue CTimeMetadata::GetTimestamp() const
		{
			return m_timestamp;
		}

		bool CTimeMetadata::IsInFrameInterval(uint32 frameIDStart, uint32 frameIDEnd) const
		{
			const bool isValid = IsValid();
			if (isValid)
			{
				return frameIDStart >= m_frameID && m_frameID <= frameIDEnd;
			}
			CRY_ASSERT_MESSAGE(isValid, "Function should not be called on a non-valid Metadata object.");
			return false;
		}

		bool CTimeMetadata::IsInTimeInterval(CTimeValue timeStart, CTimeValue timeEnd) const
		{
			const bool isValid = IsValid();
			if (isValid)
			{
				return timeStart >= m_timestamp && m_timestamp <= timeEnd;
			}
			CRY_ASSERT_MESSAGE(isValid, "Function should not be called on a non-valid Metadata object.");
			return false;
		}

		bool CTimeMetadata::IsValid() const
		{
			return m_timestamp.IsValid() && m_frameID != 0;
		}

		void CTimeMetadata::Serialize(Serialization::IArchive& ar)
		{
			ar(m_frameID, "m_frameID");

			int64 timestampValue;

			if (ar.isInput())
			{
				ar(timestampValue, "m_timestamp");
				m_timestamp.SetValue(timestampValue);
			}
			else if (ar.isOutput())
			{
				timestampValue = m_timestamp.GetValue();
				ar(timestampValue, "m_timestamp");
			}
		}

		bool CTimeMetadata::operator <(const CTimeMetadata& rhs) const
		{
			CRY_ASSERT_MESSAGE(IsValid() && rhs.IsValid(), "Both operands must be valid.");
			return m_frameID < rhs.m_frameID || (m_frameID == rhs.m_frameID && m_timestamp < rhs.m_timestamp);
		}

		bool CTimeMetadata::operator >(const CTimeMetadata& rhs) const
		{
			CRY_ASSERT_MESSAGE(IsValid() && rhs.IsValid(), "Both operands must be valid.");
			return m_frameID > rhs.m_frameID || (m_frameID == rhs.m_frameID && m_timestamp > rhs.m_timestamp);
		}

		bool CTimeMetadata::operator >=(const CTimeMetadata& rhs) const
		{
			CRY_ASSERT_MESSAGE(IsValid() && rhs.IsValid(), "Both operands must be valid.");
			return m_frameID > rhs.m_frameID || (m_frameID == rhs.m_frameID && m_timestamp >= rhs.m_timestamp);
		}

		bool CTimeMetadata::operator <=(const CTimeMetadata& rhs) const
		{
			CRY_ASSERT_MESSAGE(IsValid() && rhs.IsValid(), "Both operands must be valid.");
			return m_frameID < rhs.m_frameID || (m_frameID == rhs.m_frameID && m_timestamp <= rhs.m_timestamp);
		}

		bool CTimeMetadata::operator ==(const CTimeMetadata& rhs) const
		{
			CRY_ASSERT_MESSAGE(IsValid() && rhs.IsValid(), "Both operands must be valid.");
			return m_frameID == rhs.m_frameID && m_timestamp == rhs.m_timestamp;
		}

		bool CTimeMetadata::operator !=(const CTimeMetadata& rhs) const
		{
			CRY_ASSERT_MESSAGE(IsValid() && rhs.IsValid(), "Both operands must be valid.");
			return m_frameID != rhs.m_frameID && m_timestamp != rhs.m_timestamp;
		}

	}
}
