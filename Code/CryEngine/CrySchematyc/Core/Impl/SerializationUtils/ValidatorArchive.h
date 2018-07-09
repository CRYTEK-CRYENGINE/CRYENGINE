// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/SerializationUtils/IValidatorArchive.h>

namespace Schematyc
{
class CValidatorArchive : public IValidatorArchive
{
private:

	typedef std::vector<string> Messages;

public:

	CValidatorArchive(const SValidatorArchiveParams& params);

	// Serialization::IArchive
	virtual bool operator()(bool& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(int8& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(uint8& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(int32& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(uint32& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(int64& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(uint64& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(float& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(const Serialization::SStruct& value, const char* szName = "", const char* szLabel = nullptr) override;
	virtual bool operator()(Serialization::IContainer& value, const char* szName = "", const char* szLabel = nullptr) override;
	// ~Serialization::IArchive

	// IValidatorArchive
	virtual void   Validate(const Validator& validator) const override;
	virtual uint32 GetWarningCount() const override;
	virtual uint32 GetErrorCount() const override;
	// ~IValidatorArchive

	using IValidatorArchive::operator();

protected:

	// Serialization::IArchive
	virtual void validatorMessage(bool bError, const void* handle, const Serialization::TypeID& type, const char* szMessage) override;
	// ~Serialization::IArchive

private:

	ValidatorArchiveFlags m_flags;
	Messages              m_warnings;
	Messages              m_errors;
};
} // Schematyc
