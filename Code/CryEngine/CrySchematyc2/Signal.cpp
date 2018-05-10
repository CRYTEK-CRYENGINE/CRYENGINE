// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Signal.h"

#include <CrySchematyc2/Utils/StringUtils.h>

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CSignal::CSignal(const SGUID& guid, const SGUID& senderGUID, const char* szName)
		: m_guid(guid)
		, m_senderGUID(senderGUID)
		, m_name(szName)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CSignal::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	void CSignal::SetSenderGUID(const SGUID& senderGUID)
	{
		m_senderGUID = senderGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CSignal::GetSenderGUID() const
	{
		return m_senderGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	void CSignal::SetName(const char* szName)
	{
		m_name = szName;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CSignal::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CSignal::SetNamespace(const char* szNamespace)
	{
		m_namespace = szNamespace;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CSignal::GetNamespace() const
	{
		return m_namespace.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CSignal::SetFileName(const char* szFileName, const char* szProjectDir)
	{
		StringUtils::MakeProjectRelativeFileName(szFileName, szProjectDir, m_fileName);
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CSignal::GetFileName() const
	{
		return m_fileName.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CSignal::SetAuthor(const char* szAuthor)
	{
		m_author = szAuthor;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CSignal::GetAuthor() const
	{
		return m_author.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CSignal::SetDescription(const char* szDescription)
	{
		m_description = szDescription;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CSignal::GetDescription() const
	{
		return m_description.c_str();
	}

	void CSignal::SetWikiLink(const char* szWikiLink)
	{
		m_wikiLink = szWikiLink;
	}

	 const char* CSignal::GetWikiLink() const
	 {
		return m_wikiLink.c_str();
	 }


	//////////////////////////////////////////////////////////////////////////
	size_t CSignal::GetInputCount() const
	{
		return m_inputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CSignal::GetInputName(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].name.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CSignal::GetInputDescription(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].description.c_str() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CSignal::GetInputValue(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].pValue : IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	TVariantConstArray CSignal::GetVariantInputs() const
	{
		return m_variantInputs;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CSignal::AddInput_Protected(const char* szName, const char* szDescription, const IAny& value)
	{
		m_inputs.push_back(SInput(szName, szDescription, value.Clone()));
		CVariantVectorOutputArchive	archive(m_variantInputs);
		archive(const_cast<IAny&>(value), "", "");
		return m_inputs.size() - 1;
	}

	//////////////////////////////////////////////////////////////////////////
	CSignal::SInput::SInput(const char* _szName, const char* _szDescription, const IAnyPtr& _pValue)
		: name(_szName)
		, description(_szDescription)
		, pValue(_pValue)
	{}
}
