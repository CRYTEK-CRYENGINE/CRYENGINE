// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DialogLinesEditorWidget.h"
#include "QDialogLineDatabaseModel.h"
#include <CryString/CryPath.h>

#include <codecvt>
#include <QStandardItemModel>
#include <QToolBar>
#include <QAction>
#include <QMenu>
#include <QAdvancedTreeView.h>
#include <QHeaderView>
#include <QBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>

#include <CrySerialization/IArchive.h>
#include <CrySerialization/IArchiveHost.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/File/ICryPak.h>
#include <ProxyModels/DeepFilterProxyModel.h>
#include <Util/FileUtil.h>

//--------------------------------------------------------------------------------------------------
CDialogLinesEditorWidget::CDialogLinesEditorWidget(QWidget* pParent)
	: QWidget(pParent)
{
	m_pTree = new QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), this);
	m_pFilterModel = new QDeepFilterProxyModel();
	m_pModel = new QDialogLineDatabaseModel(this);

	DRS::IDialogLineDatabase* pLineDatabase = gEnv->pDynamicResponseSystem->GetDialogLineDatabase();
	if (pLineDatabase)
	{
		m_pFilterModel->setSourceModel(m_pModel);
		m_pFilterModel->setDynamicSortFilter(true);
		m_pTree->setModel(m_pFilterModel);

		m_pTree->resizeColumnToContents(0);
		m_pTree->resizeColumnToContents(1);
		m_pTree->resizeColumnToContents(2);
		m_pTree->resizeColumnToContents(3);
		m_pTree->resizeColumnToContents(4);
		m_pTree->setSelectionBehavior(QAbstractItemView::SelectItems);
		m_pTree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

		m_pTree->setItemDelegate(new QDialogLineDelegate(this));

		// Context menu for cells
		m_pTree->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_pTree, &QTreeView::customContextMenuRequested, [&](QPoint pos)
		{
			auto pMenu = new QMenu(this);
			QModelIndex index = m_pTree->indexAt(pos);
			QAction* pAction = nullptr;
			bool bLineSetSelected = false;

			if (!index.isValid())
			{
			  pAction = pMenu->addAction(tr("Insert Line"));
			  connect(pAction, &QAction::triggered, [=]()
				{
					m_pModel->insertRow(m_pModel->rowCount(QModelIndex()), QModelIndex());
			  });
			}
			else
			{
			  index = m_pFilterModel->mapToSource(index); // convert to from proxy to real model
			  index = index.sibling(index.row(), 0);      // the parents column needs to be zero for insert to work
			  if (m_pModel->ItemType(index) == EItemType::DIALOG_LINE_SET)
			  {
			    pAction = pMenu->addAction(tr("Insert Line"));
			    connect(pAction, &QAction::triggered, [=]()
					{
						m_pModel->insertRow(index.row(), index.parent());
			    });

			    pAction = pMenu->addAction(tr("Add Line Variation"));
			    connect(pAction, &QAction::triggered, [=]()
					{
						m_pModel->insertRow(m_pModel->rowCount(index), index);
			    });

			    bLineSetSelected = true;
			  }
			  else
			  {
			    pAction = pMenu->addAction(tr("Add Line Variation"));
			    connect(pAction, &QAction::triggered, [=]()
					{
						m_pModel->insertRow(index.row(), index.parent());
			    });
			  }

			  if (!m_pTree->selectionModel()->selectedIndexes().empty())
			  {
			    pAction = pMenu->addAction(tr("Delete"));
			    connect(pAction, &QAction::triggered, [&]()
					{
						QModelIndexList indices = m_pTree->selectionModel()->selectedIndexes();
						if (!indices.empty())
						{
						  QModelIndex index = m_pFilterModel->mapToSource(indices[0]);
						  m_pModel->removeRow(index.row(), index.parent());
						}
			    });
			  }
			}

			pMenu->addSeparator();
			if (bLineSetSelected)
			{
			  pAction = pMenu->addAction(tr("Run Script on Selected"));
			  connect(pAction, &QAction::triggered, [=]()
				{
					if (!m_pModel->ExecuteScript(index.row(), m_pTree->selectionModel()->selectedIndexes().size()))
					{
					  CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "Could not run script on dialog line(s), make sure 'linescript.bat' exists in your game folder.");
					}
			  });
			}
			pAction = pMenu->addAction(tr("Run Script on all lines"));
			connect(pAction, &QAction::triggered, [=]()
			{
				if (!m_pModel->ExecuteScript(0, std::numeric_limits<int>::max()))
				{
				  CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "Could not run script on dialog line(s), make sure 'linescript.bat' exists in your game folder.");
				}
			});

			pMenu->popup(m_pTree->viewport()->mapToGlobal(pos));
		});
	}

	m_pFilterLineEdit = new QLineEdit(this);
	m_pFilterLineEdit->setPlaceholderText(tr("Search"));
	QPushButton* pImportButton = new QPushButton("Import .tsv");
	QPushButton* pExportButton = new QPushButton("Export .tsv");
	QPushButton* pSaveButton = new QPushButton("Save");
	QPushButton* pAutoSaveToggleButton = new QPushButton("AutoSave");
	QPushButton* pClearAllButton = new QPushButton("Clear all");
	pAutoSaveToggleButton->setCheckable(true);

	auto pHLayout = new QHBoxLayout();
	pHLayout->addWidget(pSaveButton);
	pHLayout->addWidget(pAutoSaveToggleButton);
	pHLayout->addWidget(pImportButton);
	pHLayout->addWidget(pExportButton);
	pHLayout->addWidget(pClearAllButton);
	pHLayout->addWidget(m_pFilterLineEdit);

	auto pLayout = new QVBoxLayout();
	pLayout->addLayout(pHLayout);
	pLayout->addWidget(m_pTree);

	setLayout(pLayout);

	connect(m_pFilterLineEdit, &QLineEdit::textChanged, [&](const QString& str)
	{
		m_pFilterModel->setFilterFixedString(str);
		m_pFilterModel->invalidate();
	});
	connect(pImportButton, &QPushButton::clicked, this, [this]()
	{
		string filename;
		if (CFileUtil::SelectSingleFile(this, EFILE_TYPE_ANY, filename, "tab separated file (*.txt) | *.txt", GetDataFolder().c_str()))
		{
		  if (m_importExportHelper.ImportFromFile(filename.c_str()))
		  {
		    m_pModel->ForceDataReload();
		  }
		  else
		  {
		    CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "Failed to import file: '%s'", filename.c_str());
		  }
		}
	});
	connect(pExportButton, &QPushButton::clicked, this, [this]()
	{
		string filename;
		if (CFileUtil::SelectSaveFile("tab separated file (*.txt) | *.txt", "txt", GetDataFolder().c_str(), filename))
		{
		  if (m_importExportHelper.ExportToFile(filename.c_str()))
		  {
		    m_pModel->ForceDataReload();
		  }
		  else
		  {
		    CryWarning(VALIDATOR_MODULE_DRS, VALIDATOR_WARNING, "Failed to export to file: '%s'", filename.c_str());
		  }
		}
	});
	connect(pSaveButton, &QPushButton::clicked, this, [this]()
	{
		Save();
	});
	connect(pClearAllButton, &QPushButton::clicked, this, [this]()
	{
		while (gEnv->pDynamicResponseSystem->GetDialogLineDatabase()->RemoveLineSet(0))
		{
		}

		m_pModel->ForceDataReload();
	});
	connect(pAutoSaveToggleButton, &QPushButton::toggled, this, [=](bool checked)
	{
		if (checked)
		{
		  connect(m_pModel, &QDialogLineDatabaseModel::dataChanged, this, &CDialogLinesEditorWidget::Save);
		  connect(m_pModel, &QDialogLineDatabaseModel::rowsInserted, this, &CDialogLinesEditorWidget::Save);
		  connect(m_pModel, &QDialogLineDatabaseModel::rowsRemoved, this, &CDialogLinesEditorWidget::Save);
		}
		else
		{
		  disconnect(m_pModel, &QDialogLineDatabaseModel::dataChanged, this, &CDialogLinesEditorWidget::Save);
		  disconnect(m_pModel, &QDialogLineDatabaseModel::rowsInserted, this, &CDialogLinesEditorWidget::Save);
		  disconnect(m_pModel, &QDialogLineDatabaseModel::rowsRemoved, this, &CDialogLinesEditorWidget::Save);
		}
	});

	pAutoSaveToggleButton->setChecked(true);
}

//--------------------------------------------------------------------------------------------------
void CDialogLinesEditorWidget::Save()
{
	gEnv->pDynamicResponseSystem->GetDialogLineDatabase()->Save(GetDataFolder());
}

//--------------------------------------------------------------------------------------------------
string CDialogLinesEditorWidget::GetDataFolder()
{
	string path = PathUtil::GetGameFolder();
	path += CRY_NATIVE_PATH_SEPSTR;
	ICVar* pCVar = gEnv->pConsole->GetCVar("drs_dataPath");
	if (pCVar)
		path += pCVar->GetString();
	else
		path += "libs" CRY_NATIVE_PATH_SEPSTR "DynamicResponseSystem";  //default path
	path += CRY_NATIVE_PATH_SEPSTR "Dialoglines" CRY_NATIVE_PATH_SEPSTR;
	return path;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CDialogLinesDatabaseImportExportHelper::ImportFromFile(const char* szSourceTsvFile)
{
	bool bSuccess = false;
	ICryPak* pPak = gEnv->pCryPak;
	FILE* pFile = pPak->FOpen(szSourceTsvFile, "rb");
	if (pFile)
	{
		const size_t fileSize = gEnv->pCryPak->FGetSize(pFile);
		uint8* pBuffer = new uint8[fileSize];
		wchar_t* pBufferAsWchar = (wchar_t*)(pBuffer + 2); //+2 to skip encoding bytes

		pPak->FReadRawAll(pBuffer, fileSize, pFile);
		pPak->FClose(pFile);

		std::vector<wstring> allLines;
		SplitStringList(pBufferAsWchar, fileSize / sizeof(wchar_t), L'\r', L'\n', &allLines, false, true);

		const int numLines = allLines.size();
		if (numLines > 1)
		{
			CHashedString lineId;

			DRS::IDialogLineDatabase* pLineDatabase = gEnv->pDynamicResponseSystem->GetDialogLineDatabase();
			std::vector<wstring> currentLineData;

			//we expect column 1 to contain the headers
			std::vector<wstring> allColumnsOfCurrentLine;
			SplitStringList(allLines[0].c_str(), allLines[0].length(), L'\t', L'\0', &allColumnsOfCurrentLine, true, false);

			for (int i = 0; i < m_dataColumns.size(); i++)
			{
				for (int j = 0; j < allColumnsOfCurrentLine.size(); j++)
				{
					if (allColumnsOfCurrentLine[j] == m_dataColumns[i])
					{
						m_dataColumnsIndex[i] = j; //if we found a column with data that we want to import we store the index.
					}
				}
			}

			currentLineData.resize(m_dataColumns.size());

			for (int i = 1; i < numLines; i++)
			{
				allColumnsOfCurrentLine.clear();
				SplitStringList(allLines[i].c_str(), allLines[i].length(), L'\t', L'\0', &allColumnsOfCurrentLine, true, false);

				for (int j = 0; j < (int)eDataColumns::NUM_COLUMNS; j++)
				{
					int mappedIndex = m_dataColumnsIndex[j];
					if (mappedIndex > -1 && mappedIndex < allColumnsOfCurrentLine.size())
						currentLineData[j] = allColumnsOfCurrentLine[mappedIndex];
					else
						currentLineData[j].clear();
				}

				if (!currentLineData[(int)eDataColumns::lineID].empty())
				{
					lineId = CHashedString(CryStringUtils::WStrToUTF8(currentLineData[(int)eDataColumns::lineID]).c_str());
				}
				if (lineId.IsValid())
				{
					bSuccess = true;
					DRS::IDialogLineSet* pLineSet = pLineDatabase->GetLineSetById(lineId);
					if (!pLineSet)
					{
						pLineSet = pLineDatabase->InsertLineSet();
						pLineSet->SetLineId(lineId);
						if (!currentLineData[(int)eDataColumns::priority].empty())
							pLineSet->SetPriority(std::stoi(currentLineData[(int)eDataColumns::priority].c_str()));
						if (!currentLineData[(int)eDataColumns::flags].empty())
							pLineSet->SetFlags(std::stoi(currentLineData[(int)eDataColumns::flags].c_str())); //not too nice, that we directly import a bitmask
						if (!currentLineData[(int)eDataColumns::maxQueueDuration].empty())
							pLineSet->SetMaxQueuingDuration(std::stof(currentLineData[(int)eDataColumns::maxQueueDuration].c_str()));
					}
					//else TODO: Discard existing lines?

					DRS::IDialogLine* pLine = pLineSet->InsertLine(-1);
					string subtitle = CryStringUtils::WStrToUTF8(currentLineData[(int)eDataColumns::text]).c_str();
					subtitle.replace("\"\"", "\"");
					if (subtitle.front() == '\"')
						subtitle.erase(0, 1);
					if (subtitle.back() == '\"')
						subtitle.pop_back();
					pLine->SetText(subtitle);
					pLine->SetStartAudioTrigger(CryStringUtils::WStrToUTF8(currentLineData[(int)eDataColumns::audioStart]).c_str());
					pLine->SetEndAudioTrigger(CryStringUtils::WStrToUTF8(currentLineData[(int)eDataColumns::audioStop]).c_str());
					pLine->SetLipsyncAnimation(CryStringUtils::WStrToUTF8(currentLineData[(int)eDataColumns::lipsyncAnimation]).c_str());
					pLine->SetStandaloneFile(CryStringUtils::WStrToUTF8(currentLineData[(int)eDataColumns::standaloneFile]).c_str());
					pLine->SetCustomData(CryStringUtils::WStrToUTF8(currentLineData[(int)eDataColumns::customData]).c_str());
					if (!currentLineData[(int)eDataColumns::pauseLength].empty())
						pLine->SetPauseLength(std::stof(currentLineData[(int)eDataColumns::pauseLength].c_str()));
				}
			}
		}

		delete[] pBuffer;
	}
	return bSuccess;
}

//--------------------------------------------------------------------------------------------------
bool CDialogLinesDatabaseImportExportHelper::ExportToFile(const char* szTargetTsvFile)
{
	ICryPak* pPak = gEnv->pCryPak;
	FILE* pFile = pPak->FOpen(szTargetTsvFile, "wb");
	if (pFile)
	{
		unsigned char utf8header[2] = { 0xff, 0xfe };
		pPak->FWrite(utf8header, sizeof(utf8header), 1, pFile);

		wstring currentLineSetData;
		wstring currentLineData;

		//write header column
		for (const auto& currentColumn : m_dataColumns)
		{
			currentLineData += currentColumn;
			currentLineData += L'\t';
		}
		currentLineData += L"\r\n";
		pPak->FWrite(currentLineData.c_str(), currentLineData.length() * sizeof(wchar_t), 1, pFile);

		DRS::IDialogLineDatabase* pLineDatabase = gEnv->pDynamicResponseSystem->GetDialogLineDatabase();
		const int numLineSets = pLineDatabase->GetLineSetCount();
		for (int i = 0; i < numLineSets; i++)
		{
			DRS::IDialogLineSet* pLineSet = pLineDatabase->GetLineSetByIndex(i);

			currentLineSetData = CryStringUtils::UTF8ToWStrSafe(pLineSet->GetLineId().GetText().c_str());
			currentLineSetData += L'\t';
			currentLineSetData += CryStringUtils::UTF8ToWStrSafe(CryStringUtils::toString(pLineSet->GetPriority()));
			currentLineSetData += L'\t';
			currentLineSetData += CryStringUtils::UTF8ToWStrSafe(CryStringUtils::toString(pLineSet->GetFlags()));
			currentLineSetData += L'\t';
			currentLineSetData += CryStringUtils::UTF8ToWStrSafe(CryStringUtils::toString(pLineSet->GetMaxQueuingDuration()));
			currentLineSetData += L'\t';

			const int numLines = pLineSet->GetLineCount();
			for (int j = 0; j < numLines; j++)
			{
				DRS::IDialogLine* pLine = pLineSet->GetLineByIndex(j);
				currentLineData = CryStringUtils::UTF8ToWStrSafe(pLine->GetText());
				currentLineData += L'\t';
				currentLineData += CryStringUtils::UTF8ToWStrSafe(pLine->GetStartAudioTrigger());
				currentLineData += L'\t';
				currentLineData += CryStringUtils::UTF8ToWStrSafe(pLine->GetEndAudioTrigger());
				currentLineData += L'\t';
				currentLineData += CryStringUtils::UTF8ToWStrSafe(pLine->GetLipsyncAnimation());
				currentLineData += L'\t';
				currentLineData += CryStringUtils::UTF8ToWStrSafe(pLine->GetStandaloneFile());
				currentLineData += L'\t';
				currentLineData += CryStringUtils::UTF8ToWStrSafe(CryStringUtils::toString(pLine->GetPauseLength()));
				currentLineData += L'\t';
				currentLineData += CryStringUtils::UTF8ToWStrSafe(pLine->GetCustomData());
				currentLineData += L"\r\n";

				pPak->FWrite(currentLineSetData.c_str(), currentLineSetData.length() * sizeof(wchar_t), 1, pFile);
				pPak->FWrite(currentLineData.c_str(), currentLineData.length() * sizeof(wchar_t), 1, pFile);

				currentLineSetData = L"\t\t\t\t";  //we only want the line SetData to be written once for all its variations
			}
		}

		pPak->FClose(pFile);
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
bool CDialogLinesDatabaseImportExportHelper::SplitStringList(const wchar_t* szStringToSplit, int stringLength, const wchar_t delimeter1, const wchar_t delimeter2, std::vector<wstring>* outResult, bool bTrim, bool bBothDelimeter)
{
	bool bWasFound = false;
	if (stringLength > 1)
	{
		const wchar_t* szCurrentPos = szStringToSplit;
		const wchar_t* szCurrentPosStart = szStringToSplit;
		for (int i = 0; i < stringLength; i++)
		{
			if (!bBothDelimeter)
			{
				if (*szCurrentPos == delimeter1 || *szCurrentPos == delimeter2)
				{
					bWasFound = true;
					outResult->push_back(wstring(szCurrentPosStart, szCurrentPos));
					szCurrentPosStart = szCurrentPos + 1;
				}
			}
			else
			{
				if (*szCurrentPos == delimeter1 && *(szCurrentPos + 1) == delimeter2)
				{
					bWasFound = true;
					outResult->push_back(wstring(szCurrentPosStart, szCurrentPos));
					szCurrentPosStart = szCurrentPos + 2;
				}
			}
			++szCurrentPos;
		}
		if (szCurrentPosStart + 2 < szCurrentPos)
		{
			outResult->push_back(wstring(szCurrentPosStart, szCurrentPos));
		}
	}

	if (bTrim)
	{
		for (wstring& currentString : * outResult)
		{
			currentString.Trim();
		}
	}

	return bWasFound;
}
