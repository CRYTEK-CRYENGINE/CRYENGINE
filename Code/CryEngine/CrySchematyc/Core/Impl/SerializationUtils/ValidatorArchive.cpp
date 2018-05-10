// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ValidatorArchive.h"

#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/Utils/Assert.h>
#include <CrySchematyc/Services/ILog.h>

namespace Schematyc
{
CValidatorArchive::CValidatorArchive(const SValidatorArchiveParams& params)
	: IValidatorArchive(Serialization::IArchive::OUTPUT | Serialization::IArchive::INPLACE | Serialization::IArchive::VALIDATION)
	, m_flags(params.flags)
{}

bool CValidatorArchive::operator()(bool& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(int8& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(uint8& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(int32& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(uint32& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(int64& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(uint64& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(float& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(Serialization::IString& value, const char* szName, const char* szLabel)
{
	return true;
}

bool CValidatorArchive::operator()(const Serialization::SStruct& value, const char* szName, const char* szLabel)
{
	value(*this);
	return true;
}

bool CValidatorArchive::operator()(Serialization::IContainer& value, const char* szName, const char* szLabel)
{
	if (value.size())
	{
		do
		{
			value(*this, szName, szLabel);
		}
		while (value.next());
	}
	return true;
}

void CValidatorArchive::Validate(const Validator& validator) const
{
	SCHEMATYC_CORE_ASSERT(validator);
	if (validator)
	{
		for (const string& warning : m_warnings)
		{
			validator(EValidatorMessageType::Warning, warning.c_str());
		}
		for (const string& error : m_errors)
		{
			validator(EValidatorMessageType::Error, error.c_str());
		}
	}
}

uint32 CValidatorArchive::GetWarningCount() const
{
	return m_warnings.size();
}

uint32 CValidatorArchive::GetErrorCount() const
{
	return m_errors.size();
}

void CValidatorArchive::validatorMessage(bool bError, const void* handle, const Serialization::TypeID& type, const char* szMessage)
{
	if (bError)
	{
		if (m_flags.Check(EValidatorArchiveFlags::ForwardErrorsToLog))
		{
			SValidatorLink validatorLink;
			if (SerializationContext::GetValidatorLink(*this, validatorLink))
			{
				CLogMetaData logMetaData;
				logMetaData.Set(ELogMetaField::LinkCommand, CryLinkUtils::ECommand::Show);
				logMetaData.Set(ELogMetaField::ElementGUID, validatorLink.elementGUID);
				logMetaData.Set(ELogMetaField::DetailGUID, validatorLink.detailGUID);
				SCHEMATYC_LOG_SCOPE(logMetaData);
				SCHEMATYC_CORE_WARNING(szMessage);
			}
			else
			{
				SCHEMATYC_CORE_ERROR(szMessage);
			}
		}
		m_errors.push_back(szMessage);
	}
	else
	{
		if (m_flags.Check(EValidatorArchiveFlags::ForwardWarningsToLog))
		{
			SValidatorLink validatorLink;
			if (SerializationContext::GetValidatorLink(*this, validatorLink))
			{
				CLogMetaData logMetaData;
				logMetaData.Set(ELogMetaField::LinkCommand, CryLinkUtils::ECommand::Show);
				logMetaData.Set(ELogMetaField::ElementGUID, validatorLink.elementGUID);
				logMetaData.Set(ELogMetaField::DetailGUID, validatorLink.detailGUID);
				SCHEMATYC_LOG_SCOPE(logMetaData);
				SCHEMATYC_CORE_WARNING(szMessage);
			}
			else
			{
				SCHEMATYC_CORE_WARNING(szMessage);
			}
		}
		m_warnings.push_back(szMessage);
	}
}
} // Schematyc
