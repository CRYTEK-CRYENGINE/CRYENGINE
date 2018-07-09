// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "EditorFramework/Editor.h"
#include "Controls/EditorDialog.h"


#include <QMainWindow>
#include <QSplitter>

class CAsset;
struct SSubstanceOutput;
class CInspector;
class CBroadcastManager;
class ISubstancePreset;
namespace SubstanceAir
{
	struct RenderResult;
}

namespace EditorSubstance
{
	namespace Renderers
	{
		struct SPreviewGeneratedOutputData;
	}
	namespace OutputEditor {
		class CSubstanceOutputEditorWidget;
	 }
	
	//	class CGraphViewModel;
		
		class CSubstanceOutputEditorDialog : public CEditorDialog
		{
			Q_OBJECT;
		public:
			CSubstanceOutputEditorDialog(ISubstancePreset* preset, QWidget* pParent = nullptr);
			~CSubstanceOutputEditorDialog();
			void LoadUpdatedOutputSettings(std::vector<SSubstanceOutput>& outputs);
		protected:
			void customEvent(QEvent *e);
			void OnPreviewUpdated(SubstanceAir::RenderResult* result, EditorSubstance::Renderers::SPreviewGeneratedOutputData* data);
			void UpdatePreviews();
		private:
			virtual void keyPressEvent(QKeyEvent * e) override;

			virtual void showEvent(QShowEvent* event) override;

			virtual void hideEvent(QHideEvent *) override;

		private:
			OutputEditor::CSubstanceOutputEditorWidget* m_editorWidget;
			CBroadcastManager* m_pBroadcastManager;
			ISubstancePreset* m_pPreset;
		};
}


