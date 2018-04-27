// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DocSerializationContext.h"
#include "Document.h"
#include "EditorContext.h"

//////////////////////////////////////////////////////////////////////////
// CUqsDocSerializationContext
//////////////////////////////////////////////////////////////////////////

SItemTypeName CUqsDocSerializationContext::GetItemTypeNameFromType(const UQS::Shared::CTypeInfo& typeInfo) const
{
	return m_editorContext.GetSerializationCache().GetItemTypeNameFromType(typeInfo);
}

const SDocumentSettings& CUqsDocSerializationContext::GetSettings() const
{
	return m_editorContext.GetSettings();
}

bool CUqsDocSerializationContext::ShouldForceSelectionHelpersEvaluation() const
{
	return m_bForceSelectionHelpersEvaluation;
}

UQSEditor::CParametersListContext* CUqsDocSerializationContext::GetParametersListContext() const
{
	if (!m_paramListContextStack.empty())
		return m_paramListContextStack.top();
	return nullptr;
}

UQSEditor::CSelectedGeneratorContext* CUqsDocSerializationContext::GetSelectedGeneratorContext() const
{
	if (!m_selectedGeneratorContextStack.empty())
		return m_selectedGeneratorContextStack.top();
	return nullptr;
}

void CUqsDocSerializationContext::PushParametersListContext(UQSEditor::CParametersListContext* pContext)
{
	m_paramListContextStack.push(pContext);
}

void CUqsDocSerializationContext::PopParametersListContext(UQSEditor::CParametersListContext* pContext)
{
	CRY_ASSERT(!m_paramListContextStack.empty() && m_paramListContextStack.top() == pContext);
	m_paramListContextStack.pop();
}

void CUqsDocSerializationContext::PushSelectedGeneratorContext(UQSEditor::CSelectedGeneratorContext* pContext)
{
	m_selectedGeneratorContextStack.push(pContext);
}

void CUqsDocSerializationContext::PopSelectedGeneratorContext(UQSEditor::CSelectedGeneratorContext* pContext)
{
	CRY_ASSERT(!m_selectedGeneratorContextStack.empty() && m_selectedGeneratorContextStack.top() == pContext);
	m_selectedGeneratorContextStack.pop();
}
