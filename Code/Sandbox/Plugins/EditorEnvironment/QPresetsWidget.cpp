// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "QPresetsWidget.h"

#include <QSizePolicy>
#include <QLayout>
#include <QAdvancedTreeView.h>
#include <QHeaderView>
#include <QMenu>
#include <QFileDialog>
#include <QInputDialog>
#include <QTemporaryFile>
#include <QFileInfo>

#include "PresetsModel.h"

#include <IEditor.h>
#include <Cry3DEngine/I3DEngine.h>
#include "ISourceControl.h"
#include <Cry3DEngine/ITimeOfDay.h>
#include "IUndoObject.h"
#include <CryIcon.h>
#include <FileDialogs/SystemFileDialog.h>
#include <FileDialogs/EngineFileDialog.h>
#include "Controls/QuestionDialog.h"
#include <QAdvancedItemDelegate.h>

namespace
{
const char* szPresetsLibsPath = "libs/environmentpresets/";

ITimeOfDay* GetTimeOfDay()
{
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	assert(pTimeOfDay);
	return pTimeOfDay;
}

void SavePresetsInNode(const QPresetsWidget* pPresetsWidget, ITimeOfDay* pTimeOfDay, CPresetsModelNode* pNode)
{
	if (pNode->type == CPresetsModelNode::EType_Leaf)
	{
		if (pTimeOfDay->SavePreset(pNode->path.c_str()))
		{
			pNode->flags.SetFlags(CPresetsModelNode::EFlags_Modified, false);
			pNode->flags.SetFlags(CPresetsModelNode::EFlags_InFolder, true);
			pPresetsWidget->SignalEntryNodeDataChanged(pNode);
		}
		else
		{
			CryLogAlways("ERROR: failed to save Environment Preset '%s'", pNode->path.c_str());
		}
	}
	else
	{
		const size_t nChildrenCount = pNode->children.size();
		for (size_t i = 0; i < nChildrenCount; ++i)
			SavePresetsInNode(pPresetsWidget, pTimeOfDay, pNode->children[i]);
	}
}

class CPresetAsTempFileUndo
{
public:
	CPresetAsTempFileUndo(const char* name) : m_pTempFile(nullptr), m_presetName(name), m_bOk(false)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			QFileInfo tfile(m_presetName.c_str());
			QString presetName = tfile.baseName();
			const std::string filename = presetName.toStdString();

			const QString tempFileName = QDir::tempPath() + "/" + filename.c_str();
			QTemporaryFile* pTempFile = new QTemporaryFile(tempFileName);
			if (pTempFile->open())
			{
				const std::string tfilename = pTempFile->fileName().toStdString();
				m_bOk = pTimeOfDay->ExportPreset(name, tfilename.c_str());
				m_pTempFile = pTempFile;
			}
		}
	}

	~CPresetAsTempFileUndo()
	{
		delete m_pTempFile;
	}

	bool        isOk() const          { return m_bOk; }
	const char* getPresetName() const { return m_presetName.c_str(); }

protected:
	QTemporaryFile* m_pTempFile;
	std::string     m_presetName;
	bool            m_bOk;
};

template<class TUndoOp>
class CPresetUndo
{
public:
	CPresetUndo(QPresetsWidget* pWidget, const char* name, const char* desc) : m_pUndoCommand(nullptr), m_description(desc)
	{
		if (!CUndo::IsRecording())
		{
			GetIEditor()->GetIUndoManager()->Begin();
			m_pUndoCommand = new TUndoOp(pWidget, name);
		}
	}

	~CPresetUndo()
	{
		if (m_pUndoCommand)
		{
			if (m_pUndoCommand->isOk())
			{
				GetIEditor()->GetIUndoManager()->RecordUndo(m_pUndoCommand);
				string undoDesc;
				undoDesc.Format("EnvironmentEditor: %s '%s'", m_description, m_pUndoCommand->getPresetName());
				GetIEditor()->GetIUndoManager()->Accept(undoDesc);
			}
			else
			{
				GetIEditor()->GetIUndoManager()->Cancel();
			}
		}
	}
private:
	TUndoOp*    m_pUndoCommand;
	const char* m_description;
};
}

class QPresetsWidget::CPresetChangedUndoCommand : public IUndoObject
{
public:
	CPresetChangedUndoCommand(QPresetsWidget* pWidget, const char* newName, const char* oldName)
		: m_pPresetsWidget(pWidget), m_newName(newName), m_oldName(oldName)
	{
	}

	virtual int         GetSize()        { return sizeof(*this); }
	virtual const char* GetDescription() { return "QPresetsWidget: Preset selected"; }

	virtual void        Undo(bool bUndo)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			if (pTimeOfDay->SetCurrentPreset(m_oldName.c_str()))
			{
				m_pPresetsWidget->RefreshPresetList();
			}
		}
	}

	virtual void Redo()
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			if (pTimeOfDay->SetCurrentPreset(m_newName.c_str()))
			{
				m_pPresetsWidget->RefreshPresetList();
			}
		}
	}

protected:
	QPresetsWidget* m_pPresetsWidget;
	std::string     m_newName;
	std::string     m_oldName;
};

class QPresetsWidget::CPresetRemovedUndoCommand : public IUndoObject, public CPresetAsTempFileUndo
{
public:
	CPresetRemovedUndoCommand(QPresetsWidget* pWidget, const char* name)
		: CPresetAsTempFileUndo(name), m_pPresetsWidget(pWidget)
	{
	}

	virtual int         GetSize()        { return sizeof(*this); }
	virtual const char* GetDescription() { return "QPresetsWidget: Preset removed"; }

	virtual void        Undo(bool bUndo)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			const char* sPresetName = getPresetName();
			if (pTimeOfDay->AddNewPreset(sPresetName))
			{
				const std::string filename = m_pTempFile->fileName().toStdString();
				pTimeOfDay->ImportPreset(sPresetName, filename.c_str());
				m_pPresetsWidget->RefreshPresetList();
			}
		}
	}

	virtual void Redo()
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			if (pTimeOfDay->RemovePreset(getPresetName()))
			{
				m_pPresetsWidget->RefreshPresetList();
			}
		}
	}

protected:
	QPresetsWidget* m_pPresetsWidget;
};

class QPresetsWidget::CPresetResetUndoCommand : public IUndoObject, public CPresetAsTempFileUndo
{
public:
	CPresetResetUndoCommand(QPresetsWidget* pWidget, const char* name)
		: CPresetAsTempFileUndo(name), m_pPresetsWidget(pWidget), m_presetName(name)
	{
	}

	virtual int         GetSize()        { return sizeof(*this); }
	virtual const char* GetDescription() { return "QPresetsWidget: Preset removed"; }

	virtual void        Undo(bool bUndo)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			const std::string filename = m_pTempFile->fileName().toStdString();
			if (pTimeOfDay->ImportPreset(getPresetName(), filename.c_str()))
			{
				m_pPresetsWidget->SignalCurrentPresetChanged();
			}
		}
	}

	virtual void Redo()
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			pTimeOfDay->ResetPreset(getPresetName());
			m_pPresetsWidget->SignalCurrentPresetChanged();
		}
	}

protected:
	QPresetsWidget* m_pPresetsWidget;
	std::string     m_presetName;
};

class QPresetsWidget::CPresetAddUndoCommand : public IUndoObject
{
public:
	CPresetAddUndoCommand(QPresetsWidget* pWidget, const std::string& name, const std::string& selectedName)
		: m_pPresetsWidget(pWidget), m_presetName(name), m_selectedPresetName(selectedName)
	{
	}

	virtual int         GetSize()        { return sizeof(*this); }
	virtual const char* GetDescription() { return "QPresetsWidget: Preset added"; }

	virtual void        Undo(bool bUndo)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			const char* presetName = m_presetName.c_str();
			if (pTimeOfDay->RemovePreset(presetName))
			{
				const char* selectedName = m_selectedPresetName.c_str();
				pTimeOfDay->SetCurrentPreset(selectedName);
				m_pPresetsWidget->RefreshPresetList();
			}
		}
	}

	virtual void Redo()
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			const char* presetName = m_presetName.c_str();
			if (pTimeOfDay->AddNewPreset(presetName))
			{
				m_pPresetsWidget->RefreshPresetList();
			}
		}
	}

protected:
	QPresetsWidget*   m_pPresetsWidget;
	const std::string m_presetName;
	const std::string m_selectedPresetName;
};

class QPresetsWidget::CPresetAddExistingUndoCommand : public IUndoObject
{
public:
	CPresetAddExistingUndoCommand(QPresetsWidget* pWidget, const std::string& presetPath, const std::string& selectedName)
		: m_pPresetsWidget(pWidget), m_presetPath(presetPath), m_selectedPresetName(selectedName)
	{
	}

	virtual int         GetSize()        { return sizeof(*this); }
	virtual const char* GetDescription() { return "QPresetsWidget: Preset added"; }

	virtual void        Undo(bool bUndo)
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			if (pTimeOfDay->RemovePreset(m_presetPath.c_str()))
			{
				pTimeOfDay->SetCurrentPreset(m_selectedPresetName.c_str());
				m_pPresetsWidget->RefreshPresetList();
			}
		}
	}

	virtual void Redo()
	{
		ITimeOfDay* pTimeOfDay = GetTimeOfDay();
		if (pTimeOfDay)
		{
			if (pTimeOfDay->LoadPreset(m_presetPath.c_str()))
			{
				m_pPresetsWidget->RefreshPresetList();
			}
		}
	}

protected:
	QPresetsWidget*   m_pPresetsWidget;
	const std::string m_presetPath;
	const std::string m_selectedPresetName;
};

//////////////////////////////////////////////////////////////////////////

QPresetsWidget::QPresetsWidget()
	: m_presetsView(nullptr)
	, m_presetsModel(nullptr)
	, m_addNewPresetAction(nullptr)
	, m_addExistingPresetAction(nullptr)
	, m_currentlySelectedNode(nullptr)
{
	CreateUi();
	CreateActions();
	RefreshPresetList();
}

QPresetsWidget::~QPresetsWidget()
{

}

void QPresetsWidget::OnNewScene()
{
	RefreshPresetList();
}

void QPresetsWidget::SaveAllPresets() const
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		SavePresetsInNode(this, pTimeOfDay, m_presetsViewRoot.get());
	}
}

void QPresetsWidget::CreateUi()
{
	setMinimumWidth(100);
	setMaximumWidth(400);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

	QVBoxLayout* rootVerticalLayput = new QVBoxLayout;
	rootVerticalLayput->setMargin(0);
	rootVerticalLayput->setSizeConstraint(QLayout::SetMinAndMaxSize);
	setLayout(rootVerticalLayput);

	QAdvancedTreeView* treeView = new QAdvancedTreeView;
	treeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	treeView->setDragEnabled(false);
	treeView->setAcceptDrops(false);
	treeView->setDropIndicatorShown(false);
	treeView->setIndentation(16);

	// reset here, so model always has valid root node
	m_presetsViewRoot.reset(new CPresetsModelNode(CPresetsModelNode::EType_Group, "RootNode", "", nullptr));

	m_presetsModel = new CPresetsModel(*this, this);
	treeView->setModel(m_presetsModel);

	treeView->setSelectionMode(QAbstractItemView::SingleSelection);
	treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	treeView->setSortingEnabled(true);
	treeView->setItemDelegate(new QAdvancedItemDelegate(treeView));

	QHeaderView* pHeader = treeView->header();
	pHeader->setContextMenuPolicy(Qt::CustomContextMenu);
	pHeader->setStretchLastSection(false);
	pHeader->setSectionResizeMode(0, QHeaderView::Stretch);

	const int columnCount = m_presetsModel->columnCount(QModelIndex());
	for (int i = 1; i < columnCount; ++i)
	{
		pHeader->setSectionResizeMode(i, QHeaderView::Interactive);
		pHeader->resizeSection(i, 24);
	}

	connect(treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QPresetsWidget::OnSignalTreeSelectionChanged);
	connect(treeView, &QTreeView::customContextMenuRequested, this, &QPresetsWidget::OnContextMenu);

	rootVerticalLayput->addWidget(treeView);

	m_presetsView = treeView;
}

void QPresetsWidget::CreateActions()
{
	m_addNewPresetAction = new QAction("New...", this);
	connect(m_addNewPresetAction, &QAction::triggered, this, &QPresetsWidget::AddNewPreset);

	m_addExistingPresetAction = new QAction("Existing...", this);
	connect(m_addExistingPresetAction, &QAction::triggered, this, &QPresetsWidget::AddExistingPreset);
}

void QPresetsWidget::RefreshPresetList()
{
	m_currentlySelectedNode = nullptr;
	SignalBeginResetModel();
	m_presetsViewRoot.reset(new CPresetsModelNode(CPresetsModelNode::EType_Group, "RootNode", "", nullptr));
	SignalEndResetModel();

	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		const int nPresetCount = pTimeOfDay->GetPresetCount();
		std::vector<ITimeOfDay::SPresetInfo> presets;

		if (nPresetCount == 0)
			return;

		presets.resize(nPresetCount);
		pTimeOfDay->GetPresetsInfos(&presets[0], nPresetCount);

		for (size_t i = 0; i < presets.size(); ++i)
		{
			ITimeOfDay::SPresetInfo& preset = presets[i];
			CPresetsModelNode* pCurrentGroup = m_presetsViewRoot.get();
			const char* szCurrentPath = preset.m_pName;
			while (true)
			{
				string name;
				string path;

				const char* szFolderName = szCurrentPath;
				const char* szFolderNameEnd = strchr(szCurrentPath, '/');
				if (szFolderNameEnd != 0)
				{
					szCurrentPath = szFolderNameEnd + 1;
					name.assign(szFolderName, szFolderNameEnd);
					path.assign(preset.m_pName, szFolderNameEnd);

					CPresetsModelNode::ChildNodes& children = pCurrentGroup->children;
					const auto& result = std::find_if(std::begin(children), std::end(children), [&name](CPresetsModelNode* other){ return name == other->name; });
					if (result == std::end(pCurrentGroup->children))
					{
						CPresetsModelNode* pNewGroup = new CPresetsModelNode(CPresetsModelNode::EType_Group, name.c_str(), path.c_str(), pCurrentGroup);
						SignalBeginAddEntryNode(pNewGroup);
						children.push_back(pNewGroup);
						SignalEndAddEntryNode(pNewGroup);
						pCurrentGroup = pNewGroup;
					}
					else
					{
						pCurrentGroup = *result;
					}
				}
				else
				{
					name = szFolderName;
					path = preset.m_pName;
					CPresetsModelNode* pNewItem = new CPresetsModelNode(CPresetsModelNode::EType_Leaf, name.c_str(), path.c_str(), pCurrentGroup);
					SignalBeginAddEntryNode(pNewItem);
					const bool uniqueInserted = stl::push_back_unique(pCurrentGroup->children, pNewItem);
					SignalEndAddEntryNode(pNewItem);

					const char* szPresetPath = pNewItem->path.c_str();
					pNewItem->flags.SetFlags(CPresetsModelNode::EFlags_InFolder, gEnv->pCryPak->IsFileExist(szPresetPath, ICryPak::eFileLocation_OnDisk));
					pNewItem->flags.SetFlags(CPresetsModelNode::EFlags_InPak, gEnv->pCryPak->IsFileExist(szPresetPath, ICryPak::eFileLocation_InPak));
					SignalEntryNodeDataChanged(pNewItem);

					if (preset.m_bCurrent)
					{
						SelectByNode(pNewItem);
						m_currentlySelectedNode = pNewItem;
						SignalCurrentPresetChanged();
					}
					break;
				}
			} // while(true)
		}   // for(presets)
	}
}

void QPresetsWidget::SelectByNode(CPresetsModelNode* pEntryNode)
{
	if (pEntryNode)
	{
		const QModelIndex idx = m_presetsModel->ModelIndexFromNode(pEntryNode);
		m_presetsView->scrollTo(idx);
		m_presetsView->selectionModel()->select(idx, QItemSelectionModel::SelectCurrent);
	}
}

void QPresetsWidget::AddNewPreset()
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		CEngineFileDialog::RunParams runParams;
		runParams.title = tr("Add new preset");
		runParams.buttonLabel = tr("Add");
		runParams.initialDir = QString::fromLocal8Bit(szPresetsLibsPath);
		runParams.extensionFilters << CExtensionFilter(QObject::tr("XML Files"), "xml");
		QString fileName = CEngineFileDialog::RunGameSave(runParams, this);

		if (!fileName.isEmpty())
		{
			CUndo undo("EnvironmentEditor: added preset");

			const std::string sPresetPath = fileName.toStdString();
			if (pTimeOfDay->AddNewPreset(sPresetPath.c_str()))
			{
				RefreshPresetList();

				if (CUndo::IsRecording())
					CUndo::Record(new CPresetAddUndoCommand(this, sPresetPath, m_currentlySelectedNode->path.c_str()));
			}
			else
			{
				CQuestionDialog::SWarning(tr("Error"), tr("Failed to add preset"), QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::NoButton);
				undo.Cancel();
			}
		}
	}
}

void QPresetsWidget::AddExistingPreset()
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		CEngineFileDialog::RunParams runParams;
		runParams.title = tr("Add existing preset");
		runParams.initialDir = QString::fromLocal8Bit(szPresetsLibsPath);
		runParams.extensionFilters << CExtensionFilter(QObject::tr("XML Files"), "xml");
		QString fileName = CEngineFileDialog::RunGameOpen(runParams, this);

		if (!fileName.isEmpty())
		{
			CUndo undo("EnvironmentEditor: added preset");

			const std::string sPresetPath = fileName.toStdString();
			if (sPresetPath.empty())
			{
				CQuestionDialog::SWarning(tr("Error"), tr("Preset should be in the game folder"), QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::NoButton);
				return;
			}

			if (pTimeOfDay->LoadPreset(sPresetPath.c_str()))
			{
				RefreshPresetList();
				if (CUndo::IsRecording())
					CUndo::Record(new CPresetAddExistingUndoCommand(this, sPresetPath, m_currentlySelectedNode->path.c_str()));
			}
			else
			{
				const QString title = QString("Failed to load preset '%1'\n").arg(fileName);
				CQuestionDialog::SWarning(tr("Error"), title, QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::NoButton);
				undo.Cancel();
			}
		}
	}
}

void QPresetsWidget::ResetPreset(const char* szName)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		if (CQuestionDialog::SQuestion(tr("Reset values"), tr("Are you sure you want to reset all values to their default values?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No, QDialogButtonBox::StandardButton::No) == QDialogButtonBox::StandardButton::Yes)
		{
			CPresetUndo<CPresetResetUndoCommand> undo(this, szName, "reset preset");
			pTimeOfDay->ResetPreset(szName);
			SignalCurrentPresetChanged();
		}
	}
}

void QPresetsWidget::RemovePreset(const char* szName)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		if (CQuestionDialog::SQuestion(tr("Remove preset"), tr("Are you sure?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No, QDialogButtonBox::StandardButton::No) == QDialogButtonBox::StandardButton::Yes)
		{
			CPresetUndo<CPresetRemovedUndoCommand> undo(this, szName, "remove preset");
			pTimeOfDay->RemovePreset(szName);
			RefreshPresetList();
		}
	}
}

void QPresetsWidget::OpenPreset(const char* szName)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		char fullPath[ICryPak::g_nMaxPath];
		const char* szTemp = gEnv->pCryPak->AdjustFileName(szPresetsLibsPath, fullPath, 0);
		const QString dir(fullPath);

		CSystemFileDialog::RunParams runParams;
		runParams.title = tr("Import Preset");
		runParams.initialDir = dir;
		runParams.extensionFilters << CExtensionFilter(QObject::tr("XML Files"), "xml");

		const QString fileName = CSystemFileDialog::RunImportFile(runParams, this);
		const std::string importPath = fileName.toStdString();

		if (!fileName.isEmpty())
		{
			if (pTimeOfDay->ImportPreset(szName, importPath.c_str()))
			{
				SignalCurrentPresetChanged();
			}
			else
			{
				const QString title = QString("Failed to open preset '%1' from '%2'").arg(szName, fileName);
				CQuestionDialog::SWarning(tr("Error"), title);
			}
		}
	}
}

void QPresetsWidget::SavePresetAs(const char* szName)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (pTimeOfDay)
	{
		char fullPath[ICryPak::g_nMaxPath];
		const char* szTemp = gEnv->pCryPak->AdjustFileName(szPresetsLibsPath, fullPath, 0);
		const QString dir(fullPath);

		CSystemFileDialog::RunParams runParams;
		runParams.title = tr("Export Preset");
		runParams.initialDir = dir;
		runParams.extensionFilters << CExtensionFilter(QObject::tr("XML Files"), "xml");

		const QString fileName = CSystemFileDialog::RunExportFile(runParams, this);
		const std::string exportPath = fileName.toStdString();

		if (!exportPath.empty())
		{
			if (!pTimeOfDay->ExportPreset(szName, exportPath.c_str()))
			{
				const QString title = QString("Failed to export preset '%1' as '%2'").arg(szName, fileName);
				CQuestionDialog::SWarning(tr("Error"), title);
			}
		}
	}
}

void QPresetsWidget::OnSignalTreeSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	ITimeOfDay* pTimeOfDay = GetTimeOfDay();
	if (!pTimeOfDay)
		return;

	CPresetsModelNode* pSelectedEntry = nullptr;

	QItemSelection selection = m_presetsView->selectionModel()->selection();

	const QModelIndexList& indices = selection.indexes();
	if (indices.size() > 0)
	{
		const QModelIndex& index = indices[0];
		pSelectedEntry = CPresetsModel::GetEntryNode(index);
	}

	CPresetsModelNode* pPrevSelectedEntry = nullptr;
	const QModelIndexList& oldIndices = deselected.indexes();
	if (oldIndices.size() > 0)
	{
		const QModelIndex& index = oldIndices[0];
		pPrevSelectedEntry = CPresetsModel::GetEntryNode(index);
	}

	if (pPrevSelectedEntry && !pSelectedEntry)
	{
		// selected group - return selection to leaf
		m_presetsView->selectionModel()->select(deselected, QItemSelectionModel::SelectCurrent);
	}

	if (pSelectedEntry && pPrevSelectedEntry)
	{
		CUndo undo("EnvironmentEditor: Select preset");
		m_currentlySelectedNode = pSelectedEntry;
		if (pTimeOfDay->SetCurrentPreset(pSelectedEntry->path))
		{
			SignalCurrentPresetChanged();
			if (CUndo::IsRecording() && pPrevSelectedEntry)
				CUndo::Record(new CPresetChangedUndoCommand(this, pSelectedEntry->path, pPrevSelectedEntry->path));
			else
				undo.Cancel();
		}
		else
		{
			undo.Cancel();
		}
	}
}

void QPresetsWidget::OnContextMenu(const QPoint& point)
{
	QMenu menu(this);

	const QModelIndex index = m_presetsView->indexAt(point);
	if (index.isValid())
	{
		CPresetsModelNode* pNode = CPresetsModel::GetEntryNode(index);

		if (pNode->type == CPresetsModelNode::EType_Leaf)
		{
			const string& presetName = pNode->path;
			QAction* importPresetAction = new QAction("Open...", &menu);
			connect(importPresetAction, &QAction::triggered, [presetName, this]() { OpenPreset(presetName.c_str()); });
			menu.addAction(importPresetAction);

			QAction* savePresetAction = new QAction(CryIcon("icons:General/File_Save.ico"), "Save", &menu);
			connect(savePresetAction, &QAction::triggered,
			        [presetName, this]()
			{
				ITimeOfDay* pTimeOfDay = GetTimeOfDay();
				if (pTimeOfDay)
				{
				  if (pTimeOfDay->SavePreset(presetName.c_str()))
				  {
				    m_currentlySelectedNode->flags.SetFlags(CPresetsModelNode::EFlags_Modified, false);
				    m_currentlySelectedNode->flags.SetFlags(CPresetsModelNode::EFlags_InFolder, true);
				    SignalEntryNodeDataChanged(m_currentlySelectedNode);
				  }
				}
			}
			        );
			menu.addAction(savePresetAction);

			QAction* exportPresetAction = new QAction("Save as...", &menu);
			connect(exportPresetAction, &QAction::triggered, [presetName, this]() { SavePresetAs(presetName.c_str()); });
			menu.addAction(exportPresetAction);

			QAction* resetPresetAction = new QAction("Reset...", &menu);
			connect(resetPresetAction, &QAction::triggered, [presetName, this]() { ResetPreset(presetName.c_str()); });
			menu.addAction(resetPresetAction);

			QAction* removePresetAction = new QAction("Delete...", &menu);
			connect(removePresetAction, &QAction::triggered, [presetName, this]() { RemovePreset(presetName.c_str()); });
			menu.addAction(removePresetAction);

			menu.addSeparator();
		}
	}

	QMenu* addMenu = menu.addMenu(CryIcon("icons:General/Element_Add.ico"), "Add");
	addMenu->addAction(m_addNewPresetAction);
	addMenu->addAction(m_addExistingPresetAction);

	const QPoint globalPosition = m_presetsView->viewport()->mapToGlobal(point);
	menu.exec(globalPosition);
}

const char* QPresetsWidget::GetSelectedItem() const
{
	const QModelIndexList selectedIndexes = m_presetsView->selectionModel()->selection().indexes();
	if (!selectedIndexes.isEmpty())
	{
		QModelIndex index = selectedIndexes.first();
		CPresetsModelNode* pNode = CPresetsModel::GetEntryNode(index);
		return pNode->path.c_str();
	}

	return nullptr;
}

