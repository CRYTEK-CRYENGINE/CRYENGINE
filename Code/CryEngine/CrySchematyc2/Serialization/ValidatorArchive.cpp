// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ValidatorArchive.h"

#include <CrySchematyc2/Serialization/ISerializationContext.h>
#include <CrySchematyc2/Services/ILog.h>

namespace Schematyc2
{
	CValidatorArchive::CValidatorArchive(const SValidatorArchiveParams& params)
		: IValidatorArchive(Serialization::IArchive::OUTPUT | Serialization::IArchive::INPLACE | Serialization::IArchive::VALIDATION)
		, m_flags(params.flags)
		, m_warningCount(0)
		, m_errorCount(0)
	{}

	bool CValidatorArchive::operator () (bool& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (int8& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (uint8& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (int32& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (uint32& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (int64& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (uint64& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (float& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (Serialization::IString& value, const char* szName, const char* szLabel)
	{
		return true;
	}

	bool CValidatorArchive::operator () (const Serialization::SStruct& value, const char* szName, const char* szLabel)
	{
		value(*this);
		return true;
	}

	bool CValidatorArchive::operator() (Serialization::IContainer& value, const char* szName, const char* szLabel)
	{
		if(value.size())
		{
			do
			{
				value(*this, szName, szLabel);
			} while(value.next());
		}
		return true;
	}

	uint32 CValidatorArchive::GetWarningCount() const
	{
		return m_warningCount;
	}

	uint32 CValidatorArchive::GetErrorCount() const
	{
		return m_errorCount;
	}

	void CValidatorArchive::validatorMessage(bool bError, const void* handle, const Serialization::TypeID& type, const char* szMessage)
	{
		if(bError)
		{
			if((m_flags & EValidatorArchiveFlags::ForwardErrorsToLog) != 0)
			{
				SValidatorLink validatorLink;
				if(SerializationContext::GetValidatorLink(*this, validatorLink))
				{
					const CLogMessageMetaInfo metaInfo(ECryLinkCommand::Show, SLogMetaItemGUID(validatorLink.itemGUID), SLogMetaChildGUID(validatorLink.detailGUID));
					SCHEMATYC2_SYSTEM_METAINFO_ERROR(metaInfo, szMessage);
				}
				else
				{
					SCHEMATYC2_SYSTEM_ERROR(szMessage);
				}
			}
			++ m_errorCount;
		}
		else
		{
			if((m_flags & EValidatorArchiveFlags::ForwardWarningsToLog) != 0)
			{
				SValidatorLink validatorLink;
				if(SerializationContext::GetValidatorLink(*this, validatorLink))
				{
					const CLogMessageMetaInfo metaInfo(ECryLinkCommand::Show, SLogMetaItemGUID(validatorLink.itemGUID), SLogMetaChildGUID(validatorLink.detailGUID));
					SCHEMATYC2_SYSTEM_METAINFO_WARNING(metaInfo, szMessage);
				}
				else
				{
					SCHEMATYC2_SYSTEM_WARNING(szMessage);
				}
			}
			++ m_warningCount;
		}
	}
}
