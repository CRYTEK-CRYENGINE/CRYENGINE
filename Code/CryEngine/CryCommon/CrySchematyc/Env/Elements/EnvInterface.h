// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Env/EnvElementBase.h"
#include "CrySchematyc/Env/Elements/IEnvInterface.h"
#include "CrySchematyc/Utils/Any.h"
#include "CrySchematyc/Utils/Assert.h"
#include "CrySchematyc/Utils/SharedString.h"

#define SCHEMATYC_MAKE_ENV_INTERFACE(guid, name)          Schematyc::EnvInterface::MakeShared(guid, name, SCHEMATYC_SOURCE_FILE_INFO)
#define SCHEMATYC_MAKE_ENV_INTERFACE_FUNCTION(guid, name) Schematyc::EnvInterfaceFunction::MakeShared(guid, name, SCHEMATYC_SOURCE_FILE_INFO)

namespace Schematyc
{

class CEnvInterface : public CEnvElementBase<IEnvInterface>
{
public:

	inline CEnvInterface(const CryGUID& guid, const char* szName, const SSourceFileInfo& sourceFileInfo)
		: CEnvElementBase(guid, szName, sourceFileInfo)
	{}

	// IEnvElement

	virtual bool IsValidScope(IEnvElement& scope) const override
	{
		switch (scope.GetType())
		{
		case EEnvElementType::Root:
		case EEnvElementType::Module:
		case EEnvElementType::Component:
		case EEnvElementType::Action:
			{
				return true;
			}
		default:
			{
				return false;
			}
		}
	}

	// ~IEnvElement
};

class CEnvInterfaceFunction : public CEnvElementBase<IEnvInterfaceFunction>
{
private:

	struct SParam
	{
		SParam(uint32 _id, const char* _szName, const char* _szDescription, const CAnyValuePtr& _pValue)
			: id(_id)
			, name(_szName)
			, description(_szDescription)
			, pValue(_pValue)
		{}

		uint32       id;
		string       name;
		string       description;
		CAnyValuePtr pValue;
	};

	typedef std::vector<SParam> Params;

public:

	inline CEnvInterfaceFunction(const CryGUID& guid, const char* szName, const SSourceFileInfo& sourceFileInfo)
		: CEnvElementBase(guid, szName, sourceFileInfo)
	{}

	// IEnvElement

	virtual bool IsValidScope(IEnvElement& scope) const override
	{
		switch (scope.GetType())
		{
		case EEnvElementType::Interface:
			{
				return true;
			}
		default:
			{
				return false;
			}
		}
	}

	// ~IEnvElement

	// IEnvInterfaceFunction

	virtual uint32 GetInputCount() const override
	{
		return m_inputs.size();
	}

	virtual const char* GetInputName(uint32 inputIdx) const override
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].name.c_str() : "";
	}

	virtual const char* GetInputDescription(uint32 inputIdx) const override
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].description.c_str() : "";
	}

	virtual CAnyConstPtr GetInputValue(uint32 inputIdx) const override
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].pValue.get() : nullptr;
	}

	virtual uint32 GetOutputCount() const override
	{
		return m_outputs.size();
	}

	virtual const char* GetOutputName(uint32 outputIdx) const override
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].name.c_str() : "";
	}

	virtual const char* GetOutputDescription(uint32 outputIdx) const override
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].description.c_str() : "";
	}

	virtual CAnyConstPtr GetOutputValue(uint32 outputIdx) const override
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].pValue.get() : nullptr;
	}

	// ~IEnvInterfaceFunction

	inline void AddInput(uint32 id, const char* szName, const char* szDescription, const CAnyConstRef& value)
	{
		// #SchematycTODO : Validate id and name!
		m_inputs.push_back(SParam(id, szName, szDescription, CAnyValue::CloneShared(value)));
	}

	inline void AddOutput(uint32 id, const char* szName, const char* szDescription, const CAnyConstRef& value)
	{
		// #SchematycTODO : Validate id and name!
		m_outputs.push_back(SParam(id, szName, szDescription, CAnyValue::CloneShared(value)));
	}

private:

	Params m_inputs;
	Params m_outputs;
};

namespace EnvInterface
{

inline std::shared_ptr<CEnvInterface> MakeShared(const CryGUID& guid, const char* szName, const SSourceFileInfo& sourceFileInfo)
{
	return std::make_shared<CEnvInterface>(guid, szName, sourceFileInfo);
}

} // EnvInterface

namespace EnvInterfaceFunction
{

inline std::shared_ptr<CEnvInterfaceFunction> MakeShared(const CryGUID& guid, const char* szName, const SSourceFileInfo& sourceFileInfo)
{
	return std::make_shared<CEnvInterfaceFunction>(guid, szName, sourceFileInfo);
}

} // EnvInterfaceFunction
} // Schematyc
