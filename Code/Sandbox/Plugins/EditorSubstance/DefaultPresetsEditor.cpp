// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DefaultPresetsEditor.h"
#include "OutputEditor/GraphViewModel.h"
#include "SubstanceCommon.h"
#include "SandboxPlugin.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QString>

#include "FilePathUtil.h"
#include "OutputEditor\EditorWidget.h"
#include "EditorSubstanceManager.h"
#include <CryCore/CryCrc32.h>
#include "Controls/QuestionDialog.h"


namespace EditorSubstance
{
	REGISTER_VIEWPANE_FACTORY_AND_MENU(CProjectDefaultsPresetsEditor, "Substance Graph Default Mapping Editor", "Substance", true, "Substance")

		CProjectDefaultsPresetsEditor::CProjectDefaultsPresetsEditor(QWidget* pParent/* = nullptr*/)
		: CDockableEditor(pParent)
		, m_modified(false)
	{
		std::vector<SSubstanceOutput> outputs = CManager::Instance()->GetProjectDefaultOutputSettings();
		std::unordered_set<uint32> origOutputCRC;
		std::vector<SSubstanceOutput> originalOutputs;
		for (const SSubstanceOutput& output : outputs)
		{
			for (const string& name : output.sourceOutputs)
			{
				uint32 crc = CCrc32::ComputeLowercase(name);
				if(origOutputCRC.count(crc) == 0)
				{
					originalOutputs.resize(originalOutputs.size() + 1);
					originalOutputs.back().name = name;
					origOutputCRC.insert(CCrc32::ComputeLowercase(name));
				}

			}
		}
		m_editorWidget = new OutputEditor::CSubstanceOutputEditorWidget(originalOutputs, outputs, false, this);
		SetContent(m_editorWidget);
		AddToMenu(CEditor::MenuItems::FileMenu);
		AddToMenu(CEditor::MenuItems::Save);
		QObject::connect(m_editorWidget->GetGraphViewModel(), &OutputEditor::CGraphViewModel::SignalOutputsChanged, this, [=] { 
			m_modified = true;
		});

	}
	
	
	bool CProjectDefaultsPresetsEditor::OnSave()
	{
		std::vector<SSubstanceOutput> outputs;
		m_editorWidget->LoadUpdatedOutputSettings(outputs);
		CManager::Instance()->SaveProjectDefaultOutputSettings(outputs);
		m_modified = false;
		return true;
	}

	bool CProjectDefaultsPresetsEditor::TryClose()
	{
		if (!m_modified)
		{
			return true;
		}

		string reason;
		bool bClose = true;
		if (!GetIEditor()->IsMainFrameClosing())
		{
		
				// Show generic modification message.
			reason = QtUtil::ToString(tr("Substance default graph mapping was modified, but not saved."));

			const QString title = tr("Closing %1").arg(GetEditorName());
			const auto button = CQuestionDialog::SQuestion(title, QtUtil::ToQString(reason), QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel);
			switch (button)
			{
			case QDialogButtonBox::Save:
				OnSave();
				bClose = true;
				break;
			case QDialogButtonBox::Discard:
				bClose = true;
				break;
			case QDialogButtonBox::Cancel:
				bClose = false;
				break;
			default:
				CRY_ASSERT(0 && "Unknown button");
				bClose = false;
				break;
			}
		}

		if (bClose)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	bool CProjectDefaultsPresetsEditor::OnClose()
	{
		if (!m_modified)
		{
			return true;
		}

		TryClose();

		return true;
	}

	void CProjectDefaultsPresetsEditor::closeEvent(QCloseEvent *e)
	{
		if (TryClose())
		{
			e->accept();
		}
		else
		{
			e->ignore();
		}
	}

	bool CProjectDefaultsPresetsEditor::CanQuit(std::vector<string>& unsavedChanges)
	{
		if (m_modified)
		{
			unsavedChanges.push_back("Default Substance graph output mapping was modified, but not saved.");
			return false;
		}
		return true;
	}

}
