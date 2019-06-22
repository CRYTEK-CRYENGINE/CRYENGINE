// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SubstanceCommon.h"
#include <EditorFramework/Editor.h>

namespace EditorSubstance
{
	namespace OutputEditor {
		class CSubstanceOutputEditorWidget;
	}

	class CProjectDefaultsPresetsEditor : public CDockableEditor
	{
		Q_OBJECT
	public:
		CProjectDefaultsPresetsEditor(QWidget* parent = nullptr);

		bool OnSave();
	protected:
		void RegisterActions();
		virtual const char* GetEditorName() const override { return "Substance Graph Default Mapping Editor"; }
		bool TryClose();

		bool OnClose();

		virtual void closeEvent(QCloseEvent *) override;

		virtual bool CanQuit(std::vector<string>& unsavedChanges) override;

	private:
		OutputEditor::CSubstanceOutputEditorWidget* m_editorWidget;
		bool m_modified;
	};

}
