// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorService.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		bool CEditorService::LoadTextualQueryBlueprint(const char* szQueryBlueprintName, Editor::IEditorServiceConsumer& callback, Shared::IUqsString& error) const
		{
			if (DataSource::IEditorLibraryProvider* pEditorLibraryProvider = g_pHub->GetEditorLibraryProvider())
			{
				CTextualQueryBlueprint loadedTextualQueryBlueprint;
				const bool loadedSuccessfully = pEditorLibraryProvider->LoadTextualQueryBlueprint(szQueryBlueprintName, loadedTextualQueryBlueprint, error);
				if (loadedSuccessfully)
				{
					callback.OnTextualQueryBlueprintLoaded(loadedTextualQueryBlueprint);
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				error.Format("There's no IEditorLibraryProvider installed in the IHub");
				return false;
			}
		}

		bool CEditorService::SaveTextualQueryBlueprint(const char* szQueryBlueprintName, Editor::IEditorServiceConsumer& callback, Shared::IUqsString& error) const
		{
			if (DataSource::IEditorLibraryProvider* pEditorLibraryProvider = g_pHub->GetEditorLibraryProvider())
			{
				CTextualQueryBlueprint textualQueryBlueprint;
				callback.OnTextualQueryBlueprintGettingSaved(textualQueryBlueprint);

				const bool savedSuccessfully = pEditorLibraryProvider->SaveTextualQueryBlueprint(szQueryBlueprintName, textualQueryBlueprint, error);
				return savedSuccessfully;
			}
			else
			{
				error.Format("There's no IEditorLibraryProvider installed in the IHub");
				return false;
			}
		}

		bool CEditorService::CreateNewTextualQueryBlueprint(const char* szQueryBlueprintName, Editor::IEditorServiceConsumer& callback, Shared::IUqsString& error) const
		{
			if (DataSource::IEditorLibraryProvider* pEditorLibraryProvider = g_pHub->GetEditorLibraryProvider())
			{
				CTextualQueryBlueprint textualQueryBlueprint;
				const bool createdSuccessfully = pEditorLibraryProvider->CreateNewTextualQueryBlueprint(szQueryBlueprintName, textualQueryBlueprint, error);
				if (createdSuccessfully)
				{
					callback.OnTextualQueryBlueprintLoaded(textualQueryBlueprint);
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				error.Format("There's no IEditorLibraryProvider installed in the IHub");
				return false;
			}
		}

		bool CEditorService::RemoveQueryBlueprint(const char* szQueryBlueprintName, Shared::IUqsString& error) const
		{
			if (DataSource::IEditorLibraryProvider* pEditorLibraryProvider = g_pHub->GetEditorLibraryProvider())
			{
				return pEditorLibraryProvider->RemoveQueryBlueprint(szQueryBlueprintName, error);
			}
			else
			{
				error.Format("There's no IEditorLibraryProvider installed in the IHub");
				return false;
			}
		}

		bool CEditorService::ValidateTextualQueryBlueprint(Editor::IEditorServiceConsumer& callback) const
		{
			CTextualQueryBlueprint textualQueryBlueprint;
			callback.OnTextualQueryBlueprintGettingValidated(textualQueryBlueprint);

			CQueryBlueprint actualQueryBlueprintForValidation;
			return actualQueryBlueprintForValidation.Resolve(textualQueryBlueprint);   // this will add error messages to all attached ISyntaxErrorCollectors
		}

	}
}
