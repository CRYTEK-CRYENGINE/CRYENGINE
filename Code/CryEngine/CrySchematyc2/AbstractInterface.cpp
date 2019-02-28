// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AbstractInterface.h"

#include <CrySchematyc2/Utils/StringUtils.h>

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CAbstractInterfaceFunction::CAbstractInterfaceFunction(const SGUID& guid, const SGUID& ownerGUID)
		: m_guid(guid)
		, m_ownerGUID(ownerGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAbstractInterfaceFunction::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAbstractInterfaceFunction::GetOwnerGUID() const
	{
		return m_ownerGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterfaceFunction::SetName(const char* name)
	{
		m_name = name;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterfaceFunction::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterfaceFunction::SetFileName(const char* fileName, const char* projectDir)
	{
		StringUtils::MakeProjectRelativeFileName(fileName, projectDir, m_fileName);
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterfaceFunction::GetFileName() const
	{
		return m_fileName.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterfaceFunction::SetAuthor(const char* author)
	{
		m_author = author;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterfaceFunction::GetAuthor() const
	{
		return m_author.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterfaceFunction::SetDescription(const char* szDescription)
	{
		m_description = szDescription;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterfaceFunction::GetDescription() const
	{
		return m_description.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterfaceFunction::AddInput(const char* name, const char* textDesc, const IAnyPtr& pValue)
	{
		CRY_ASSERT(pValue);
		if(pValue)
		{
			m_inputs.push_back(SParam(name, textDesc, pValue));

			CVariantVectorOutputArchive	archive(m_variantInputs);
			archive(*pValue, "", "");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CAbstractInterfaceFunction::GetInputCount() const
	{
		return m_inputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterfaceFunction::GetInputName(size_t iInput) const
	{
		return iInput < m_inputs.size() ? m_inputs[iInput].name.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterfaceFunction::GetInputDescription(size_t iInput) const
	{
		return iInput < m_inputs.size() ? m_inputs[iInput].description.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CAbstractInterfaceFunction::GetInputValue(size_t iInput) const
	{
		return iInput < m_inputs.size() ? m_inputs[iInput].pValue : IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CAbstractInterfaceFunction::GetVariantInputs() const
	{
		return m_variantInputs;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterfaceFunction::AddOutput(const char* name, const char* textDesc, const IAnyPtr& pValue)
	{
		CRY_ASSERT(pValue);
		if(pValue)
		{
			m_outputs.push_back(SParam(name, textDesc, pValue));

			CVariantVectorOutputArchive	archive(m_variantOutputs);
			archive(*pValue, "", "");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CAbstractInterfaceFunction::GetOutputCount() const
	{
		return m_outputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterfaceFunction::GetOutputName(size_t iOutput) const
	{
		return iOutput < m_outputs.size() ? m_outputs[iOutput].name.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterfaceFunction::GetOutputDescription(size_t iOutput) const
	{
		return iOutput < m_outputs.size() ? m_outputs[iOutput].description.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CAbstractInterfaceFunction::GetOutputValue(size_t iOutput) const
	{
		return iOutput < m_outputs.size() ? m_outputs[iOutput].pValue : IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CAbstractInterfaceFunction::GetVariantOutputs() const
	{
		return m_variantOutputs;
	}

	//////////////////////////////////////////////////////////////////////////
	CAbstractInterfaceFunction::SParam::SParam(const char* _name, const char* _szDescription, const IAnyPtr& _pValue)
		: name(_name)
		, description(_szDescription)
		, pValue(_pValue)
	{}

	//////////////////////////////////////////////////////////////////////////
	CAbstractInterface::CAbstractInterface(const SGUID& guid)
		: m_guid(guid)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CAbstractInterface::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterface::SetName(const char* name)
	{
		m_name = name;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterface::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterface::SetNamespace(const char* szNamespace)
	{
		m_namespace = szNamespace;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterface::GetNamespace() const
	{
		return m_namespace.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterface::SetFileName(const char* fileName, const char* projectDir)
	{
		StringUtils::MakeProjectRelativeFileName(fileName, projectDir, m_fileName);
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterface::GetFileName() const
	{
		return m_fileName.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterface::SetAuthor(const char* author)
	{
		m_author = author;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterface::GetAuthor() const
	{
		return m_author.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CAbstractInterface::SetDescription(const char* szDescription)
	{
		m_description = szDescription;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CAbstractInterface::GetDescription() const
	{
		return m_description.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	IAbstractInterfaceFunctionPtr CAbstractInterface::AddFunction(const SGUID& guid)
	{
		CAbstractInterfaceFunctionPtr	pFunction(new CAbstractInterfaceFunction(guid, m_guid));
		m_functions.push_back(pFunction);
		return pFunction;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CAbstractInterface::GetFunctionCount() const
	{
		return m_functions.size();
	}

	//////////////////////////////////////////////////////////////////////////
	IAbstractInterfaceFunctionConstPtr CAbstractInterface::GetFunction(size_t iFunction) const
	{
		return iFunction < m_functions.size() ? m_functions[iFunction] : IAbstractInterfaceFunctionConstPtr();
	}
}
