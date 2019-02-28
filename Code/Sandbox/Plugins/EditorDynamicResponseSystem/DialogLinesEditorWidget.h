// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QDialogLineDatabaseModel;
class QAdvancedTreeView;
class QDeepFilterProxyModel;
class QLineEdit;

class CDialogLinesDatabaseImportExportHelper
{
public:
	bool ImportFromFile(const char* szSourceTsvFile);
	bool ExportToFile(const char* szTargetTsvFile);

protected:
	enum class eDataColumns
	{
		lineID = 0, //LineSet
		priority, //LineSet
		flags,  //LineSet
		maxQueueDuration, //LineSet
		text, //Line
		audioStart, //Line
		audioStop, //Line
		lipsyncAnimation, //Line
		standaloneFile, //Line
		pauseLength, //Line
		customData, //Line
		NUM_COLUMNS
	};

	bool SplitStringList(const wchar_t* szStringToSplit, int stringLength, const wchar_t delimeter1, const wchar_t delimeter2, std::vector<wstring>* outResult, bool bTrim, bool bBothDelimeter);

	std::vector<wstring> m_dataColumns = { L"lineID", L"priority", L"flags", L"maxQueueDuration", L"text", L"audioStart", L"audioStop", L"lipsyncAnimation", L"standaloneFile", L"pauseLength", L"customData" };
	std::vector<int> m_dataColumnsIndex = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
};

class CDialogLinesEditorWidget : public QWidget
{
public:
	CDialogLinesEditorWidget(QWidget* pParent);

private:
	void   Save();
	string GetDataFolder();

	QAdvancedTreeView*        m_pTree;
	QDeepFilterProxyModel*    m_pFilterModel;
	QDialogLineDatabaseModel* m_pModel;
	QLineEdit*                m_pFilterLineEdit;
	CDialogLinesDatabaseImportExportHelper m_importExportHelper;
};

