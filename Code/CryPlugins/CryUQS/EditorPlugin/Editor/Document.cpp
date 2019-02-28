// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Document.h"

#include <CrySerialization/IArchive.h>
#include <CrySerialization/CryStrings.h>
#include <CrySerialization/StringList.h>
#include <Serialization/PropertyTree/PropertyTree.h>

#include "Blueprints.h"
#include "EditorContext.h"
#include "DocSerializationContext.h"

//////////////////////////////////////////////////////////////////////////

class CQueryBlueprintShuttle : public UQS::Editor::IEditorServiceConsumer
{
public:
	CQueryBlueprintShuttle(const CUqsEditorContext& editorContext);

	// IEditorServiceConsumer
	virtual void OnTextualQueryBlueprintLoaded(const UQS::Core::ITextualQueryBlueprint& loadedTextualQueryBlueprint) override;
	virtual void OnTextualQueryBlueprintGettingSaved(UQS::Core::ITextualQueryBlueprint& textualQueryBlueprintToFillBeforeSaving) override;
	virtual void OnTextualQueryBlueprintGettingValidated(UQS::Core::ITextualQueryBlueprint& textualQueryBlueprintToFillBeforeValidating) override;
	// ~IEditorServiceConsumer

	std::unique_ptr<UQSEditor::CQueryBlueprint> m_pQueryBlueprintForLoading;
	const UQSEditor::CQueryBlueprint*           m_pQueryBlueprintForSaving;
	const UQSEditor::CQueryBlueprint*           m_pQueryBlueprintForValidating;
	const CUqsEditorContext&                    m_editorContext;
};

CQueryBlueprintShuttle::CQueryBlueprintShuttle(const CUqsEditorContext& editorContext)
	: m_pQueryBlueprintForLoading()
	, m_pQueryBlueprintForSaving(nullptr)
	, m_pQueryBlueprintForValidating(nullptr)
	, m_editorContext(editorContext)
{}

void CQueryBlueprintShuttle::OnTextualQueryBlueprintLoaded(const UQS::Core::ITextualQueryBlueprint& loadedTextualQueryBlueprint)
{
	assert(!m_pQueryBlueprintForLoading);
	m_pQueryBlueprintForLoading.reset(new UQSEditor::CQueryBlueprint);
	CUqsDocSerializationContext serializationContext(m_editorContext);
	m_pQueryBlueprintForLoading->BuildSelfFromITextualQueryBlueprint(loadedTextualQueryBlueprint, serializationContext);
}

void CQueryBlueprintShuttle::OnTextualQueryBlueprintGettingSaved(UQS::Core::ITextualQueryBlueprint& textualQueryBlueprintToFillBeforeSaving)
{
	assert(m_pQueryBlueprintForSaving);
	m_pQueryBlueprintForSaving->BuildITextualQueryBlueprintFromSelf(textualQueryBlueprintToFillBeforeSaving);
}

void CQueryBlueprintShuttle::OnTextualQueryBlueprintGettingValidated(UQS::Core::ITextualQueryBlueprint& textualQueryBlueprintToFillBeforeValidating)
{
	assert(m_pQueryBlueprintForValidating);
	m_pQueryBlueprintForValidating->BuildITextualQueryBlueprintFromSelf(textualQueryBlueprintToFillBeforeValidating);
}

//////////////////////////////////////////////////////////////////////////

CUqsQueryDocument::CUqsQueryDocument(const CUqsEditorContext& editorContext)
	: m_pTree(nullptr)
	, m_pQueryBlueprint()
	, m_settings(editorContext.GetSettings())
	, m_editorContext(editorContext)
	, m_bNeverSaved(true)
{}

CUqsQueryDocument::~CUqsQueryDocument()
{
	DetachFromTree();
}

bool CUqsQueryDocument::CreateNew(const char* szQueryName, stack_string& outErrorMessage)
{
	UQS::Shared::CUqsString error;

	m_pQueryBlueprint.reset(nullptr);  // not really necessary

	CQueryBlueprintShuttle shuttle(m_editorContext);
	const bool bResult = UQS::Core::IHubPlugin::GetHub().GetEditorService().CreateNewTextualQueryBlueprint(szQueryName, shuttle, error);

	if (bResult)
	{
		assert(shuttle.m_pQueryBlueprintForLoading);
		m_pQueryBlueprint = std::move(shuttle.m_pQueryBlueprintForLoading);

		// NOTE pavloi 2016.07.04: query name was sanitized and potentially slightly different from szQueryName
		m_queryName = m_pQueryBlueprint->GetName();
	}
	else
	{
		outErrorMessage = error.c_str();
		CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, "CUqsQueryDocument::CreateNew: %s", error.c_str());
		// TODO: propagate the error message up to the caller (the message might be something like "file not found" or "not well-formed xml")
	}

	ReoutputTree();

	m_bNeverSaved = true;

	return bResult;
}

void CUqsQueryDocument::Delete()
{
	DetachFromTree();

	if (!m_queryName.empty())
	{
		UQS::Shared::CUqsString error;
		const bool bResult = UQS::Core::IHubPlugin::GetHub().GetEditorService().RemoveQueryBlueprint(m_queryName.c_str(), error);
		if (!bResult)
		{
			CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, "CUqsQueryDocument::Delete: %s", error.c_str());
		}

		m_queryName.clear();
	}

	m_pQueryBlueprint.reset();
}

bool CUqsQueryDocument::Load(const char* szQueryName)
{
	UQS::Shared::CUqsString error;

	m_pQueryBlueprint.reset(nullptr);  // not really necessary

	m_queryName = szQueryName;

	CQueryBlueprintShuttle shuttle(m_editorContext);
	const bool bResult = UQS::Core::IHubPlugin::GetHub().GetEditorService().LoadTextualQueryBlueprint(szQueryName, shuttle, error);

	if (bResult)
	{
		m_bNeverSaved = false;
		assert(shuttle.m_pQueryBlueprintForLoading);
		m_pQueryBlueprint = std::move(shuttle.m_pQueryBlueprintForLoading);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, "CUqsQueryDocument::Load: %s", error.c_str());
		m_pQueryBlueprint.reset(new UQSEditor::CQueryBlueprint);
		// TODO: propagate the error message up to the caller (the message might be something like "file not found" or "not well-formed xml")
	}

	ReoutputTree();

	return bResult;
}

bool CUqsQueryDocument::Save()
{
	UQS::Shared::CUqsString error;

	if (m_pQueryBlueprint)
	{
		CQueryBlueprintShuttle shuttle(m_editorContext);
		shuttle.m_pQueryBlueprintForSaving = m_pQueryBlueprint.get();

		const bool bResult = UQS::Core::IHubPlugin::GetHub().GetEditorService().SaveTextualQueryBlueprint(m_queryName.c_str(), shuttle, error);
		// TODO: if saving failed, then propagate the error message up to the caller (the message might be something like "file could not be written")
		if (bResult)
		{
			m_bNeverSaved = false;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, "CUqsQueryDocument::Save: %s", error.c_str());
		}

		return bResult;
	}
	return false;
}

void CUqsQueryDocument::Serialize(Serialization::IArchive& archive)
{
	if (m_pQueryBlueprint)
	{
		UQSEditor::CQueryBlueprint& bp = *m_pQueryBlueprint;

		SDocumentSettings oldSettings = m_settings;
		if (!archive.isEdit())
		{
			m_settings.bUseSelectionHelpers = false;
		}

		CUqsDocSerializationContext serializationContext(m_editorContext);
		Serialization::SContext context(archive, &serializationContext);

		archive(bp, "query", "Query");

		if (!archive.isEdit())
		{
			m_settings = oldSettings;
		}
	}
}

void CUqsQueryDocument::AttachToTree(PropertyTree* pTree)
{
	if (m_pTree != pTree)
	{
		DetachFromTree();

		m_pTree = pTree;

		BeforeManualRevert();
		pTree->attach(Serialization::SStruct(*this));
		AfterManualRevertDone();
	}
}

void CUqsQueryDocument::DetachFromTree()
{
	if (m_pTree)
	{
		m_pTree->detach();
		m_pTree = nullptr;
	}
}

void CUqsQueryDocument::ApplySettings(const SDocumentSettings& settings)
{
	const bool bChanged = (m_settings != settings);

	if (bChanged)
	{
		m_settings = settings;

		ReoutputTree();
	}
}

void CUqsQueryDocument::ReoutputTree()
{
	if (m_pTree)
	{
		BeforeManualRevert();
		m_pTree->revert();
		m_pTree->updateHeights(true); // TODO pavloi 2016.04.05: hack to make sure, that labels are recalculated
		AfterManualRevertDone();
	}
}

void CUqsQueryDocument::BeforeManualRevert()
{
	if (m_pQueryBlueprint)
	{
		if (m_settings.bUseSelectionHelpers)
		{
			CUqsDocSerializationContext serializationContext(m_editorContext);
			serializationContext.SetShouldForceSelectionHelpersEvaluation(true);
			m_pQueryBlueprint->PrepareHelpers(serializationContext);
		}
	}
}

void CUqsQueryDocument::AfterManualRevertDone()
{
}

void CUqsQueryDocument::OnTreeChanged()
{
	if (m_pQueryBlueprint)
	{
		m_pQueryBlueprint->ClearErrors();

		CQueryBlueprintShuttle shuttle(m_editorContext);
		shuttle.m_pQueryBlueprintForValidating = m_pQueryBlueprint.get();

		UQS::Core::IEditorService& editorService = UQS::Core::IHubPlugin::GetHub().GetEditorService();
		editorService.ValidateTextualQueryBlueprint(shuttle);
	}
}
