// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MeshImporterEditor.h"
#include "MainDialog.h"
#include "DialogCHR/DialogCHR.h"
#include "DialogCAF.h"
#include "ImporterUtil.h"

// REMOVEME
#include <CryAnimation/ICryAnimation.h>

// EditorCommon
#include <AssetSystem/AssetManager.h>
#include <QtViewPane.h>

// EditorQt
#include <Util/BoostPythonHelpers.h>
#include <Commands/CommandManager.h>

// Qt
#include <QApplication>
#include <QCloseEvent>
#include <QToolBar>
#include <QVBoxLayout>

template<typename T> struct SEditorNames {};

template<> struct SEditorNames<CMainDialog>
{
	static const char* GetName()
	{
		return "Mesh";
	}
};
template<> struct SEditorNames<MeshImporter::CDialogCHR>
{
	static const char* GetName()
	{
		return "Skeleton";
	}
};
template<> struct SEditorNames<MeshImporter::CDialogCAF>
{
	static const char* GetName()
	{
		return "Animation";
	}
};

template<typename T>
class CEditorAdapterT : public CEditorAdapter
{
public:
	CEditorAdapterT(QWidget* pParent = nullptr)
		: CEditorAdapter(std::unique_ptr<MeshImporter::CBaseDialog>(new T()), pParent)
	{
		m_title.Format("Import %s", GetName());
	}

	static const char* GetName()
	{
		return SEditorNames<T>::GetName();
	}

	virtual const char* GetEditorName() const override { return m_title.c_str(); }
private:
	string m_title;
};

typedef CEditorAdapterT<CMainDialog>               MainDialogAdapter;
typedef CEditorAdapterT<MeshImporter::CDialogCHR>  DialogChrAdapter;
typedef CEditorAdapterT<MeshImporter::CDialogCAF>  DialogCafAdapter;

REGISTER_VIEWPANE_FACTORY_AND_MENU(MainDialogAdapter, MainDialogAdapter::GetName(), "Tools", false, "FBX Import")
REGISTER_VIEWPANE_FACTORY_AND_MENU(DialogChrAdapter, DialogChrAdapter::GetName(), "Tools", false, "FBX Import")
REGISTER_VIEWPANE_FACTORY_AND_MENU(DialogCafAdapter, DialogCafAdapter::GetName(), "Tools", false, "FBX Import")

static void PyImport()
{
	CommandEvent("meshimporter.import").SendToKeyboardFocus();
}

static void PyReimport()
{
	CommandEvent("meshimporter.reimport").SendToKeyboardFocus();
}

string GenerateCharacter(const QString& filePath);

string PyGenerateCharacter(const char* filePath)
{
	string str(filePath);

	if (str.empty())
	{
		return "";
	}

	// Unquote.
	if (str.at(0) == '"' && str.at(str.length() - 1))
	{
		str = str.substr(1, str.length() - 2);
	}

	return GenerateCharacter(QtUtil::ToQString(str));  // TODO: This gets deduced to CCommand1, with no args.
}

DECLARE_PYTHON_MODULE(meshimporter);

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyImport, meshimporter, import, "Import...", "meshimporter.import()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyReimport, meshimporter, reimport, "Reimport...", "meshimporter.reimport()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGenerateCharacter, meshimporter, generate_character, "Generate...", "meshimporter.generate_character filepath");
REGISTER_EDITOR_COMMAND_TEXT(meshimporter, import, "Import");

CEditorAdapter::CEditorAdapter(std::unique_ptr<MeshImporter::CBaseDialog> pDialog, QWidget* pParent)
	: CAssetEditor(ToStringList(MeshImporter::GetAssetTypesFromFileFormatFlags(pDialog->GetOpenFileFormatFlags())), pParent)
	, m_pDialog(std::move(pDialog))
{
	// Create file menu.
	AddToMenu(CEditor::MenuItems::FileMenu);

	AddToMenu(CEditor::MenuItems::Open);
	AddToMenu(CEditor::MenuItems::Save);
	AddToMenu(CEditor::MenuItems::SaveAs);
	AddToMenu("File", "meshimporter.import");

	m_pDialog->CreateMenu(this);

	QToolBar* const pToolBar = new QToolBar();
	{
		const int toolButtons = m_pDialog->GetToolButtons();

		if (toolButtons & MeshImporter::CBaseDialog::eToolButtonFlags_Import)
		{
			QAction* const pAct = GetAction("meshimporter.import");
			pAct->setIcon(CryIcon("icons:General/File_Import.ico"));
			pToolBar->addAction(pAct);
		}

		if (toolButtons & MeshImporter::CBaseDialog::eToolButtonFlags_Open)
		{
			pToolBar->addAction(GetAction("general.open"));
		}

		pToolBar->addSeparator();

		if (toolButtons & MeshImporter::CBaseDialog::eToolButtonFlags_Save)
		{
			pToolBar->addAction(GetAction("general.save"));
		}
	}

	QVBoxLayout* const pLayout = (QVBoxLayout*)layout();
	if (pToolBar)
	{
		pLayout->addWidget(pToolBar);
	}
	pLayout->addWidget(m_pDialog.get(), 1);
}

void CEditorAdapter::Host_AddMenu(const char* menu)
{
	GetRootMenu()->CreateMenu(menu);
}

void CEditorAdapter::Host_AddToMenu(const char* menu, const char* command)
{
	AddToMenu(menu, command);
}

IViewPaneClass::EDockingDirection CEditorAdapter::GetDockingDirection() const
{
	return IViewPaneClass::DOCK_FLOAT;
}

void CEditorAdapter::customEvent(QEvent* pEvent)
{
	CEditor::customEvent(pEvent);
	if (!pEvent->isAccepted() && pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(pEvent);
		const string& command = commandEvent->GetCommand();
		if (command == "meshimporter.import")
		{
			m_pDialog->OnImportFile();
			pEvent->accept();
		}
		else if (command == "meshimporter.reimport")
		{
			m_pDialog->OnReimportFile();
			pEvent->accept();
		}
	}
}

bool CEditorAdapter::OnAboutToCloseAsset(string& reason) const
{
	if (GetAssetBeingEdited() && !m_pDialog->MayUnloadScene())
	{
		reason = QtUtil::ToString(tr("Asset '%1' has unsaved modifications.").arg(GetAssetBeingEdited()->GetName()));
		return false;
	}
	return true;
}

bool CEditorAdapter::OnOpenAsset(CAsset* pAsset)
{
	if (!pAsset)
	{
		return false;
	}

	const bool bOpen = m_pDialog->Open(pAsset->GetMetadataFile());
	if (bOpen && GetAssetBeingEdited())
	{
		signalAssetClosed(GetAssetBeingEdited());
	}
	return bOpen;
}

bool CEditorAdapter::OnSaveAsset(CEditableAsset& /*editAsset*/)
{
	CRY_ASSERT_MESSAGE(false, "Not implemented");
	return false;
}

bool CEditorAdapter::OnSave()
{
	m_pDialog->OnSave();
	return true;
}

bool CEditorAdapter::OnSaveAs()
{
	m_pDialog->OnSaveAs();
	return true;
}

void CEditorAdapter::OnCloseAsset()
{
	m_pDialog->OnCloseAsset();
}

