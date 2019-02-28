// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/StringList.h>

class CUqsQueryDocument;
class CUqsEditorContext;
struct SDocumentSettings;
struct SItemTypeName;
class CUqsDatabaseSerializationCache;

namespace UQSEditor
{
class CParametersListContext;
class CSelectedGeneratorContext;
} // namespace UQSEditor

class CUqsDocSerializationContext
{
public:
	explicit CUqsDocSerializationContext(const CUqsEditorContext& editorContext)
		: m_editorContext(editorContext)
		, m_bForceSelectionHelpersEvaluation(false)
		, m_paramListContextStack()
		, m_selectedGeneratorContextStack()
	{}

	SItemTypeName                           GetItemTypeNameFromType(const UQS::Shared::CTypeInfo& typeInfo) const;

	const SDocumentSettings&              GetSettings() const;

	bool                                  ShouldForceSelectionHelpersEvaluation() const;
	void                                  SetShouldForceSelectionHelpersEvaluation(const bool value) { m_bForceSelectionHelpersEvaluation = value; }

	UQSEditor::CParametersListContext*    GetParametersListContext() const;
	UQSEditor::CSelectedGeneratorContext* GetSelectedGeneratorContext() const;

private:
	friend class UQSEditor::CParametersListContext;
	void PushParametersListContext(UQSEditor::CParametersListContext* pContext);
	void PopParametersListContext(UQSEditor::CParametersListContext* pContext);

	friend class UQSEditor::CSelectedGeneratorContext;
	void PushSelectedGeneratorContext(UQSEditor::CSelectedGeneratorContext* pContext);
	void PopSelectedGeneratorContext(UQSEditor::CSelectedGeneratorContext* pContext);

private:
	const CUqsEditorContext&                          m_editorContext;
	bool                                              m_bForceSelectionHelpersEvaluation;
	std::stack<UQSEditor::CParametersListContext*>    m_paramListContextStack;
	std::stack<UQSEditor::CSelectedGeneratorContext*> m_selectedGeneratorContextStack;
};
