// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <DockedWidget.h>
#include <CrySerialization/IArchive.h>

class QTreeWidget;
class QSpinBox;
class QPushButton;
class QVBoxLayout;
class QTabWidget;
class QLabel;
class QAction;
class QLineEdit;
class QCheckBox;
namespace Explorer
{
class ExplorerPanel;
class ExplorerData;
class ExplorerFileList;
struct ExplorerEntry;
struct ExplorerEntryModifyEvent;
}

class ResponselessSignalsWidget;
class RecentResponsesWidget;
struct FileExplorerWithButtons;
struct CPropertyTreeWithOptions;

//--------------------------------------------------------------------------------------------------
struct SResponseSerializerHelper
{
	SResponseSerializerHelper() { m_filter = 0; }
	void Serialize(Serialization::IArchive& ar);
	DynArray<stack_string> m_signalNames;
	DynArray<stack_string> m_fileNames;
	stack_string GetFirstSignal() { if (m_signalNames.size() > 0) return m_signalNames[0]; else return "ALL"; }
	int                    m_filter;
	string                 currentActor;
};

//--------------------------------------------------------------------------------------------------
struct SRecentResponseSerializerHelper
{
	SRecentResponseSerializerHelper() : filter(0), maxNumber(100) {}
	void Serialize(Serialization::IArchive& ar);
	int                    maxNumber;
	int                    filter;
	DynArray<stack_string> m_signalNames;
};

//--------------------------------------------------------------------------------------------------
class CResponsesFileLoader
{
public:
	~CResponsesFileLoader();
	bool Load(const char* szFilename);
	bool Save(const char* szFilename);
	void Reset() { m_currentFile.clear(); }
	void Serialize(Serialization::IArchive& ar);

protected:
	string m_currentFile;
};

//--------------------------------------------------------------------------------------------------
class BaseResponseTreeWidget : public QWidget
{
public:
	BaseResponseTreeWidget();

	bool IsVisible() const    { return m_bVisible; }
	void SetVisible(bool val) { m_bVisible = val; }

protected:
	struct signalNameWithCounter
	{
		signalNameWithCounter(const char* szSignalName) : signalName(szSignalName), counter(1) {}
		string signalName;
		int    counter;
	};

	void FillWithSignals(DynArray<const char*>& elements);

	QVBoxLayout* m_pVLayout;
	QTreeWidget* m_pTree;
	QPushButton* m_pUpdateButton;
	bool         m_bVisible;

};

//--------------------------------------------------------------------------------------------------
class RecentResponsesWidget : public BaseResponseTreeWidget
{
public:
	RecentResponsesWidget();
	void UpdateElements();
	void UpdateTrees();

private:
	QPushButton* m_pCreateNewResponseButton;

	QCheckBox*   m_pShowResponselessSignals;
	QCheckBox*   m_pShowSignalsWithResponses;
};

//--------------------------------------------------------------------------------------------------
struct CPropertyTreeWithOptions : public QWidget
{
	CPropertyTreeWithOptions(QWidget* pParent);
	void SetCurrentSignal(const stack_string& signalName);

private:
	QAction*   m_pSendSignalButton;
	QLineEdit* m_pSignalNameEdit;
	QLineEdit* m_pSenderNameEdit;
};

//--------------------------------------------------------------------------------------------------
struct CPropertyTreeWithAutoupdateOption : public QWidget
{
	CPropertyTreeWithAutoupdateOption(QWidget* pParent);
	void SetAutoUpdateActive(bool bValue);

private:
	QAction* m_pAutoupdateButton;
};

//--------------------------------------------------------------------------------------------------
struct FileExplorerWithButtons : public QWidget
{
	FileExplorerWithButtons(QWidget* pParent, Explorer::ExplorerData* pExplorerData);

	bool                     m_bVisible;
	Explorer::ExplorerPanel* m_pExplorerPanel;
};

//--------------------------------------------------------------------------------------------------
class CDrsResponseEditorWindow : public CDockableWindow
{
	Q_OBJECT

public:
	CDrsResponseEditorWindow();
	~CDrsResponseEditorWindow();

	void OnVisibilityChanged(bool bVisible);

	//////////////////////////////////////////////////////////
	// CDockableWindow implementation
	virtual const char*                       GetPaneTitle() const override        { return "Dynamic Response System"; };
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	//////////////////////////////////////////////////////////

public slots:
	void                      UpdateAll();
	void                      OnFileExplorerSelectionChanged();
	void                      OnResponseFileWidgetVisibilityChanged(bool bVisible);
	void                      OnPropertyTreeChanged();

	Explorer::ExplorerPanel*  GetExplorerPanel();
	Explorer::ExplorerData*   GetExplorerData()         { return m_pExplorerData; }
	CPropertyTreeWithOptions* GetResponsePropertyTree() { return m_pResponseDockWidget->widget(); }
private:
	Explorer::ExplorerData*                 m_pExplorerData;
	Explorer::ExplorerFileList*             m_pExplorerFiles;

	DockedWidget<QTabWidget>*               m_pSignalViews;
	DockedWidget<CPropertyTreeWithOptions>* m_pResponseDockWidget;
	CPropertyTreeWithAutoupdateOption*      m_pPropertyTree;

	FileExplorerWithButtons*                m_pFileExplorerWidget;
	RecentResponsesWidget*                  m_pRecentSignalsWidget;
};

