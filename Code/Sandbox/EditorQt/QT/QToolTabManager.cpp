// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QToolTabManager.h"

#include "QtViewPane.h"

#include "QT/QtMainFrame.h"
#include "QMfcApp/qwinhost.h"
#include "QT/Widgets/QViewPaneHost.h"
#include "QToolWindowManager/QToolWindowManager.h"
#include "LevelEditor/LevelFileUtils.h"

#include <QCloseEvent>
#include <QDesktopWidget>
#include <QLayout>
#include <QJsonDocument>
#include <QTextCodec>
#include "FileDialogs/SystemFileDialog.h"

#include "Util/BoostPythonHelpers.h"
#include "QtViewPane.h"
#include <QtUtil.h>
#include <CrySystem/IProjectManager.h>

#include <CrySystem/UserAnalytics/IUserAnalytics.h>

#include <mutex>

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class QWinHostPane : public QWinHost
{
public:
	QSize defaultSize;
	QSize minimumSize;
	QWinHostPane() : QWinHost() {}
	virtual QSize sizeHint() const
	{
		return defaultSize;
	}
	virtual QSize minimumSizeHint() const
	{
		return minimumSize;
	}
};

namespace
{
CTabPaneManager* s_pGlobalToolTabManager = 0;

static const char* szAppDataLayoutDir = "Layouts";
static const char* szDefaultLayoutDir = "Editor/Layouts";
static const char* szDefaultLayout = "Editor/Layouts/Default Layout.json";

void PyLoadLayoutFromFile(const char* fullFilename)
{
	CTabPaneManager::GetInstance()->LoadLayout(fullFilename);
}

void PySaveLayoutToFile(const char* fullFilename)
{
	CTabPaneManager::GetInstance()->SaveLayoutToFile(fullFilename);
}

void PyResetLayout()
{
	CTabPaneManager::GetInstance()->LoadDefaultLayout();
}

void PyLoadLayoutDlg()
{
	QDir dir(QtUtil::GetAppDataFolder());
	dir.cd(szAppDataLayoutDir);

	CSystemFileDialog::RunParams runParams;
	runParams.title = CEditorMainFrame::tr("Load Layout");
	runParams.initialDir = dir.absolutePath();
	runParams.extensionFilters << CExtensionFilter(CEditorMainFrame::tr("JSON Files"), "json");

	const QString filename = CSystemFileDialog::RunImportFile(runParams, CEditorMainFrame::GetInstance());
	if (!filename.isEmpty())
	{
		PyLoadLayoutFromFile(filename.toStdString().c_str());
	}
}

void PySaveLayoutAs()
{
	QDir dir(QtUtil::GetAppDataFolder());
	// This will build the folder structure required if it doesn't exist yet
	dir.mkpath(QtUtil::GetAppDataFolder() + "/" + szAppDataLayoutDir);
	dir.cd(szAppDataLayoutDir);

	CSystemFileDialog::RunParams runParams;
	runParams.title = CEditorMainFrame::tr("Save Layout");
	runParams.initialDir = dir.absolutePath();
	runParams.extensionFilters << CExtensionFilter(CEditorMainFrame::tr("JSON Files"), "json");

	const QString path = CSystemFileDialog::RunExportFile(runParams, CEditorMainFrame::GetInstance());
	if (!path.isEmpty())
	{
		PySaveLayoutToFile(path.toStdString().c_str());
	}
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyLoadLayoutFromFile, layout, load,
                                     "Loads a layout from file.",
                                     "layout.load(str path)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySaveLayoutToFile, layout, save,
                                     "Saves current layout to a file.",
                                     "layout.save(str absolutePath)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyResetLayout, layout, reset,
                                     "Reset Layout",
                                     "");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyLoadLayoutDlg, layout, load_dlg,
                                     "Load Layout...", "");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySaveLayoutAs, layout, save_as,
                                     "Save Layout As...", "");

//////////////////////////////////////////////////////////////////////////
CTabPaneManager::CTabPaneManager(QWidget* const pParent)
	: m_pParent(pParent)
{
	s_pGlobalToolTabManager = this;
	m_bToolsDirty = false;
	m_layoutLoaded = false;
}

CTabPaneManager::~CTabPaneManager()
{
	CloseAllPanes();
	s_pGlobalToolTabManager = nullptr;
}

//////////////////////////////////////////////////////////////////////////
CTabPaneManager* CTabPaneManager::GetInstance()
{
	return s_pGlobalToolTabManager;
}

void CTabPaneManager::OnTabPaneMoved(QWidget* tabPane, bool visible)
{
	if (visible)
	{
		QVariantMap m = GetIEditor()->GetPersonalizationManager()->GetState("SpawnLocation");
		QString className = qobject_cast<QTabPane*>(tabPane)->m_class;
		if (m_layoutLoaded || !m[className].isValid())
		{
			QVariantMap toolPath;
			QString s = GetToolManager()->getToolPath(tabPane);
			toolPath["path"] = s;
			if (!s.contains("/"))
			{
				toolPath["geometry"] = tabPane->window()->saveGeometry().toBase64();
			}
			m[className] = toolPath;
			GetIEditor()->GetPersonalizationManager()->SetState("SpawnLocation", m);
		}
	}
}

QString CTabPaneManager::CreateObjectName(const char* title)
{
	QString s(title);
	QString result = s;
	while (FindTabPaneByName(result))
	{
		int i = qrand();
		result = QString::asprintf("%s#%d", s.toStdString().c_str(), i);
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////
QTabPane* CTabPaneManager::CreateTabPane(const char* paneClassName, const char* title, int nOverrideDockDirection, bool bLoadLayoutPersonalization)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(paneClassName);
	QTabPane* pPane = 0;
	bool isToolAlreadyCreated = false;

	IClassDesc* pClassDesc = GetIEditorImpl()->GetClassFactory()->FindClass(paneClassName);
	if (!pClassDesc || ESYSTEM_CLASS_VIEWPANE != pClassDesc->SystemClassID())
	{
		return nullptr;
	}

	IViewPaneClass* pViewPaneClass = (IViewPaneClass*)pClassDesc;

	// Check if view view pane class support only 1 pane at a time.
	if (pViewPaneClass->SinglePane())
	{

		for (size_t i = 0; i < m_panes.size(); i++)
		{
			pPane = m_panes[i];

			if (pPane->m_class.compareNoCase(paneClassName) == 0 && pPane->m_bViewCreated)
			{
				isToolAlreadyCreated = true;
				break;
			}
		}
	}

	if (!isToolAlreadyCreated)
	{
		pPane = new QTabPane();
		pPane->setParent(m_pParent);
		m_panes.push_back(pPane);
		pPane->m_class = paneClassName;

		// User Analytics - do not move or modify
		PushUserEvent("Sandbox::OpenTool", paneClassName, pPane);
	}

	int dockDir = IViewPaneClass::DOCK_FLOAT;

	IPane* pContentWidget = nullptr;

	// Only do this if pane is not single, else everything here has already been done
	if (!isToolAlreadyCreated)
	{
		pContentWidget = CreatePaneContents(pPane);

		if (pContentWidget)
		{
			if (!title)
			{
				title = pContentWidget->GetPaneTitle();
			}
			dockDir = pContentWidget->GetDockingDirection();

			if (bLoadLayoutPersonalization)
			{
				pContentWidget->LoadLayoutPersonalization();
			}
		}
		else if (!title)
		{
			title = pViewPaneClass->GetPaneTitle();
		}

		pPane->m_title = title;
		pPane->setObjectName(CreateObjectName(title));
		pPane->setWindowTitle(QWidget::tr(title));

		pPane->m_category = pViewPaneClass->Category();
	}
	else
	{
		pContentWidget = pPane->m_pane;

		if (pContentWidget)
		{
			dockDir = pContentWidget->GetDockingDirection();
		}
	}

	bool bDockDirection = false;

	QToolWindowAreaTarget toolAreaTarget(QToolWindowAreaReference::Floating);

	if (nOverrideDockDirection != -1)
	{
		dockDir = nOverrideDockDirection;
	}

	switch (dockDir)
	{
	case IViewPaneClass::DOCK_DEFAULT:
		toolAreaTarget = QToolWindowAreaTarget(QToolWindowAreaReference::Floating);
		break;
	case IViewPaneClass::DOCK_TOP:
		toolAreaTarget = QToolWindowAreaTarget(QToolWindowAreaReference::HSplitTop);
		bDockDirection = true;
		break;
	case IViewPaneClass::DOCK_BOTTOM:
		toolAreaTarget = QToolWindowAreaTarget(QToolWindowAreaReference::HSplitBottom);
		bDockDirection = true;
		break;
	case IViewPaneClass::DOCK_LEFT:
		toolAreaTarget = QToolWindowAreaTarget(QToolWindowAreaReference::VSplitLeft);
		bDockDirection = true;
		break;
	case IViewPaneClass::DOCK_RIGHT:
		toolAreaTarget = QToolWindowAreaTarget(QToolWindowAreaReference::VSplitRight);
		bDockDirection = true;
		break;
	case IViewPaneClass::DOCK_FLOAT:
		toolAreaTarget = QToolWindowAreaTarget(QToolWindowAreaReference::Floating);
		break;
	}

	if (bDockDirection)
	{
		QTabPane* pReferencePane = FindTabPaneByTitle("Perspective");
		if (pReferencePane)
		{
			IToolWindowArea* toolArea = GetToolManager()->areaOf(pReferencePane);
			if (toolArea)
			{
				toolAreaTarget = QToolWindowAreaTarget(toolArea, toolAreaTarget.reference);
			}
			else
			{
				toolAreaTarget = QToolWindowAreaTarget(QToolWindowAreaReference::Floating);
			}
		}
	}

	QRect paneRect = QRect(0, 0, 800, 600);
	if (pContentWidget)
	{
		paneRect = pContentWidget->GetPaneRect();
	}
	if (m_panesHistory.find(pPane->m_title) != m_panesHistory.end())
	{
		const SPaneHistory& paneHistory = m_panesHistory.find(pPane->m_title)->second;
		paneRect = paneHistory.rect;
	}

	QRect maxRc;
	maxRc.setLeft( GetSystemMetrics(SM_XVIRTUALSCREEN));
	maxRc.setTop(GetSystemMetrics(SM_YVIRTUALSCREEN));
	maxRc.setRight(maxRc.left() + GetSystemMetrics(SM_CXVIRTUALSCREEN));
	maxRc.setBottom(maxRc.top() + GetSystemMetrics(SM_CYVIRTUALSCREEN));

	QSize minimumSize = QSize();
	if (pContentWidget)
	{
		minimumSize = pContentWidget->GetMinSize();
	}

	// Clip to virtual desktop.
	paneRect = paneRect.intersected(maxRc);

	// Ensure it is at least 10x10
	if (paneRect.width() < 10)
	{
		paneRect.setRight(paneRect.left() + 10);
	}
	if (paneRect.width() < 10)
	{
		paneRect.setBottom(paneRect.top() + 10);
	}

	pPane->m_defaultSize = QSize(paneRect.width(), paneRect.height());
	pPane->m_minimumSize = minimumSize;

	QVariantMap toolPath;
	QVariantMap spawnLocationMap = GetIEditor()->GetPersonalizationManager()->GetState("SpawnLocation");

	if (!m_layoutLoaded)
	{
		toolAreaTarget = QToolWindowAreaTarget(QToolWindowAreaReference::Hidden);
	}
	else
	{
		if (spawnLocationMap[paneClassName].isValid())
		{
			toolPath = spawnLocationMap[paneClassName].toMap();
			if (!toolPath["geometry"].isValid())
			{
				QToolWindowAreaTarget t = GetToolManager()->targetFromPath(toolPath["path"].toString());
				if (t.reference != QToolWindowAreaReference::Floating)
				{
					//Previous location was found, use that one
					toolAreaTarget = t;
				}
			}
		}
	}

	GetToolManager()->addToolWindow(pPane, toolAreaTarget);

	if (toolAreaTarget.reference == QToolWindowAreaReference::Floating)
	{
		bool alreadyPlaced = false;
		if (toolPath["geometry"].isValid())
		{
			alreadyPlaced = pPane->window()->restoreGeometry(QByteArray::fromBase64(toolPath["geometry"].toByteArray()));
		}
		if (!alreadyPlaced)
		{
			QWidget* w = pPane;
			while (!w->isWindow())
			{
				w = w->parentWidget();
			}
			static int i = 0;
			w->move(QPoint(32, 32) * (i + 1));
			i = (i + 1) % 10;
		}
		//Pane had a temporary location saved when it was created, so make sure it's restored to the value we started with
		OnTabPaneMoved(pPane, true);
	}

	m_bToolsDirty = true;

	return pPane;
}

IPane* CTabPaneManager::CreatePane(const char* paneClassName, const char* title /*= 0*/, int nOverrideDockDirection /*= -1*/)
{
	auto tabPane = CreateTabPane(paneClassName, title, nOverrideDockDirection, false);
	return tabPane ? tabPane->m_pane : nullptr;
}

void CTabPaneManager::BringToFront(IPane* pane)
{
	auto tabPane = FindTabPane(pane);
	if (tabPane)
		GetToolManager()->bringToFront(tabPane);
}

//////////////////////////////////////////////////////////////////////////
CWnd* CTabPaneManager::OpenMFCPane(const char* sPaneClassName)
{
	IClassDesc* pClassDesc = GetIEditorImpl()->GetClassFactory()->FindClass(sPaneClassName);
	if (!pClassDesc || ESYSTEM_CLASS_VIEWPANE != pClassDesc->SystemClassID())
	{
		return nullptr;
	}

	IViewPaneClass* pViewPaneClass = (IViewPaneClass*)pClassDesc;

	// Check if view view pane class support only 1 pane at a time.
	if (pViewPaneClass->SinglePane())
	{
		QTabPane* tool = FindTabPaneByClass(sPaneClassName);
		if (tool)
		{
			// first activate then focus - else focus will not work
			GetToolManager()->bringToFront(tool);
			tool->activateWindow();
			tool->setFocus();
			return tool->m_MfcWnd;
		}
	}

	QTabPane* tool = CreateTabPane(sPaneClassName, NULL, -1, true);

	if (!tool)
	{
		return nullptr;
	}

	tool->setFocus();
	tool->activateWindow();

	return tool->m_MfcWnd;
}

CWnd* CTabPaneManager::FindMFCPane(const char* sPaneClassName)
{
	QTabPane* tool = FindTabPaneByClass(sPaneClassName);
	if (tool)
	{
		return tool->m_MfcWnd;
	}
	return nullptr;
}

QTabPane* CTabPaneManager::FindTabPane(IPane* pane)
{
	auto it = std::find_if(m_panes.begin(), m_panes.end(), [pane](QTabPane* tabPane) { return tabPane->m_pane == pane; });
	if (it != m_panes.end())
		return *it;
	else
		return nullptr;
}

IPane* CTabPaneManager::OpenOrCreatePane(const char* sPaneClassName)
{
	QTabPane* tool = FindTabPaneByClass(sPaneClassName);

	if (tool)
	{
		// first activate then focus - else focus will not work
		GetToolManager()->bringToFront(tool);
		tool->activateWindow();
		tool->setFocus();
		return tool->m_pane;
	}

	tool = CreateTabPane(sPaneClassName);

	if (!tool)
	{
		return nullptr;
	}

	tool->setFocus();
	tool->activateWindow();

	return tool->m_pane;
}

//////////////////////////////////////////////////////////////////////////
bool CTabPaneManager::CloseTabPane(QTabPane* tool)
{
	bool bDeleted = stl::find_and_erase(m_panes, tool);
	CRY_ASSERT(bDeleted); //if this is false we have tried to delete the tool twice, or delete a tool that wasn't registered
	if (tool && bDeleted)
	{
		// User Analytics - do not move or modify
		PushUserEvent("Sandbox::CloseTool", tool->m_title, tool);

		GetToolManager()->releaseToolWindow(tool, true);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
QTabPane* CTabPaneManager::FindTabPaneByName(const QString& name)
{
	QList<QWidget*> tools = GetToolManager()->toolWindows();
	foreach(QWidget * tool, tools)
	{
		QTabPane* toolPane = qobject_cast<QTabPane*>(tool);
		if (toolPane && (name.isEmpty() || name == tool->objectName()))
		{
			return toolPane;
		}
	}
	return nullptr;
}

QList<QTabPane*> CTabPaneManager::FindTabPanes(const QString& name)
{
	QList<QTabPane*> result;
	QList<QWidget*> tools = GetToolManager()->toolWindows();
	foreach(QWidget * tool, tools)
	{
		QTabPane* toolPane = qobject_cast<QTabPane*>(tool);
		if (toolPane && (name.isEmpty() || name == tool->objectName()))
		{
			result.append(toolPane);
		}
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////
QTabPane* CTabPaneManager::FindTabPaneByTitle(const char* title)
{
	QList<QTabPane*> tools = FindTabPanes();
	for (int i = 0; i < tools.count(); i++)
	{
		QTabPane* tool = tools.at(i);
		if (0 == strcmp(tool->m_title.c_str(), title))
		{
			return tool;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
QTabPane* CTabPaneManager::FindTabPaneByCategory(const char* sPaneCategory)
{
	QList<QTabPane*> tools = FindTabPanes();
	for (int i = 0; i < tools.count(); i++)
	{
		QTabPane* tool = tools.at(i);
		if (0 == strcmp(tool->m_category, sPaneCategory))
		{
			return tool;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
QTabPane* CTabPaneManager::FindTabPaneByClass(const char* paneClassName)
{
	if (!CEditorMainFrame::GetInstance())
	{
		return nullptr;
	}
	if (!GetToolManager())
	{
		return nullptr;
	}
	QList<QTabPane*> tools = FindTabPanes();
	for (int i = 0; i < tools.count(); i++)
	{
		QTabPane* tool = tools.at(i);
		if (0 == strcmp(tool->m_class, paneClassName))
		{
			return tool;
		}
	}
	return nullptr;
}

IPane* CTabPaneManager::FindPaneByClass(const char* paneClassName)
{
	auto tabPane = FindTabPaneByClass(paneClassName);
	if (tabPane)
		return tabPane->m_pane;
	else
		return nullptr;
}

IPane* CTabPaneManager::FindPaneByTitle(const char* title)
{
	auto tabPane = FindTabPaneByTitle(title);
	if (tabPane)
		return tabPane->m_pane;
	else
		return nullptr;
}

void CTabPaneManager::SaveLayout()
{
	QString userLayout = QtUtil::GetAppDataFolder();
	QDir(userLayout).mkpath(userLayout);
	userLayout += "/Layout.json";
	SaveLayoutToFile(userLayout.toStdString().c_str());
}

bool CTabPaneManager::LoadUserLayout()
{
	QString userLayout = QtUtil::GetAppDataFolder();
	userLayout += "/Layout.json";
	return LoadLayoutFromFile(userLayout.toStdString().c_str());
}

bool CTabPaneManager::LoadLayout(const char* filePath)
{
	LOADING_TIME_PROFILE_AUTO_SESSION("sandbox_load_layout");
	LOADING_TIME_PROFILE_SECTION;

	if (QFile(filePath).exists())
		return LoadLayoutFromFile(filePath);

	QString projectPath(GetIEditorImpl()->GetProjectManager()->GetCurrentProjectDirectoryAbsolute());
	projectPath = projectPath + "/" + filePath;
	if (QFile(projectPath).exists())
		return LoadLayoutFromFile(projectPath.toUtf8());

	return LoadLayoutFromFile(PathUtil::Make(PathUtil::GetEnginePath(), filePath).c_str());
}

bool CTabPaneManager::LoadDefaultLayout()
{
	// Check project folder first
	QString projectPath(GetIEditorImpl()->GetProjectManager()->GetCurrentProjectDirectoryAbsolute());
	projectPath = projectPath + "/" + szDefaultLayout;
	QFile file(projectPath);
	if (file.exists())
		return LoadLayoutFromFile(projectPath.toUtf8());

	return LoadLayoutFromFile(PathUtil::Make(PathUtil::GetEnginePath(), szDefaultLayout).c_str());
}

void CTabPaneManager::SaveLayoutToFile(const char* fullFilename)
{
	QFile file(fullFilename);
	if (!file.open(QIODevice::WriteOnly))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to open path: %s", fullFilename);
		return;
	}

	QJsonDocument doc(QJsonDocument::fromVariant(GetState()));
	file.write(doc.toJson());
}

bool CTabPaneManager::LoadLayoutFromFile(const char* fullFilename)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(fullFilename);
	QFile file(fullFilename);
	if (!file.open(QIODevice::ReadOnly))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, "Failed to open path: %s", fullFilename);
		return false;
	}

	QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
	QVariant layout = doc.toVariant();

	if (layout.isValid())
	{
		SetState(layout);
		m_layoutLoaded = true;
		return true;
	}
	else
	{
		return false;
	}
}

QFileInfoList CTabPaneManager::GetUserLayouts()
{
	QStringList filter;
	filter << "*.json";

	QDir dir(QtUtil::GetAppDataFolder());
	if (dir.cd(szAppDataLayoutDir))
	{
		return dir.entryInfoList(filter, QDir::Files);
	}
	else
	{
		return QFileInfoList();
	}
}

QFileInfoList CTabPaneManager::GetProjectLayouts()
{
	QStringList filter;
	filter << "*.json";

	QString projectPath(GetIEditorImpl()->GetProjectManager()->GetCurrentProjectDirectoryAbsolute());
	projectPath = projectPath + "/" + szDefaultLayoutDir;
	QDir dir(projectPath);
	return dir.entryInfoList(filter, QDir::Files);
}

QFileInfoList CTabPaneManager::GetAppLayouts()
{
	QStringList filter;
	filter << "*.json";

	QDir dir(PathUtil::GetEnginePath().c_str());
	if (dir.cd(szDefaultLayoutDir))
	{
		return dir.entryInfoList(filter, QDir::Files);
	}
	else
	{
		return QFileInfoList();
	}
}

void CTabPaneManager::CloseAllPanes()
{
	LOADING_TIME_PROFILE_SECTION;

	bool bNeedsToDelete = false;
	while (!m_panes.empty())
	{
		bNeedsToDelete = true;
		auto it = m_panes.begin();
		(*it)->close();
	}

	if (bNeedsToDelete)
	{
		//This must be called to ensure the deffered delete of child widgets is called synchronously
		//as destruction code will often rely on child widgets being deleted before the parent
		qApp->processEvents();
	}
}

void CTabPaneManager::StoreHistory(QTabPane* tool)
{
	SPaneHistory paneHistory;
	QRect rc = tool->frameGeometry();
	QPoint p = rc.topLeft();

	paneHistory.rect = QRect(p.x(), p.y(), p.x() + rc.width(), p.y() + rc.height());
	paneHistory.dockDir = 0;
	m_panesHistory[tool->m_title] = paneHistory;
}

//////////////////////////////////////////////////////////////////////////
void CTabPaneManager::OnIdle()
{
	if (m_bToolsDirty)
	{
		CreateContentInPanes();
		m_bToolsDirty = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTabPaneManager::CreateContentInPanes()
{
	for (size_t i = 0; i < m_panes.size(); i++)
	{
		QTabPane* pTool = m_panes[i];

		if (!pTool->m_bViewCreated)
		{
			CreatePaneContents(pTool);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IPane* CTabPaneManager::CreatePaneContents(QTabPane* pTool)
{
	LOADING_TIME_PROFILE_SECTION;
	pTool->m_bViewCreated = true;

	IClassDesc* pClassDesc = GetIEditorImpl()->GetClassFactory()->FindClass(pTool->m_class);
	if (!pClassDesc || ESYSTEM_CLASS_VIEWPANE != pClassDesc->SystemClassID())
	{
		return nullptr;
	}

	IViewPaneClass* pViewPaneClass = (IViewPaneClass*)pClassDesc;
	IPane* pWidget = pViewPaneClass->CreatePane();
	if (pWidget)
	{
		QWidget* const pContentWidget = pWidget->GetWidget();

		pTool->layout()->addWidget(pContentWidget);
		pTool->m_pane = pWidget;

		QObject::connect(pContentWidget, &QWidget::windowTitleChanged, pTool, &QWidget::setWindowTitle);
		QObject::connect(pContentWidget, &QWidget::windowIconChanged, pTool, &QWidget::setWindowIcon);
	}
	else
	{
		QWinHostPane* mfcHostWidget = new QWinHostPane();
		mfcHostWidget->defaultSize = pTool->m_defaultSize;
		mfcHostWidget->minimumSize = pTool->m_minimumSize;
		pTool->layout()->addWidget(mfcHostWidget);

		CRuntimeClass* pRuntimeClass = pViewPaneClass->GetRuntimeClass();
		if (pRuntimeClass && mfcHostWidget)
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("Loading MFC Tool");
			assert(pRuntimeClass->IsDerivedFrom(RUNTIME_CLASS(CWnd)) || pRuntimeClass == RUNTIME_CLASS(CWnd));
			pTool->m_MfcWnd = (CWnd*)pRuntimeClass->CreateObject();
			assert(pTool->m_MfcWnd);
			mfcHostWidget->setWindow(pTool->m_MfcWnd->GetSafeHwnd());
		}
	}
	return pWidget;
}

//////////////////////////////////////////////////////////////////////////
QToolWindowManager* CTabPaneManager::GetToolManager() const
{
	return CEditorMainFrame::GetInstance()->GetToolManager();
}

//////////////////////////////////////////////////////////////////////////
void CTabPaneManager::LayoutLoaded()
{
	m_layoutLoaded = true;
}

//////////////////////////////////////////////////////////////////////////
QVariant CTabPaneManager::GetState() const
{
	QVariantMap stateMap;
	for (size_t i = 0; i < m_panes.size(); i++)
	{
		QTabPane* pTool = m_panes[i];
		if (GetToolManager()->areaOf(pTool))
		{
			QVariantMap toolData;
			toolData.insert("class", pTool->m_class.c_str());
			if (pTool->m_pane)
			{
				toolData.insert("state", pTool->m_pane->GetState());
			}
			stateMap[pTool->objectName()] = toolData;
		}
	}

	QWidget* mainFrameWindow = CEditorMainFrame::GetInstance();
	while (mainFrameWindow->parentWidget())
	{
		mainFrameWindow = mainFrameWindow->parentWidget();
	}

	QVariant mainWindowStateVar = QtUtil::ToQVariant(CEditorMainFrame::GetInstance()->saveState());
	QVariant mainWindowGeomVar = QtUtil::ToQVariant(mainFrameWindow->saveGeometry());
	QVariant toolsLayoutVar = GetToolManager()->saveState();

	QVariantMap state;
	state.insert("MainWindowGeometry", mainWindowGeomVar);
	state.insert("MainWindowState", mainWindowStateVar);
	state.insert("ToolsLayout", toolsLayoutVar);
	state.insert("Windows", stateMap);
	return state;
}

//////////////////////////////////////////////////////////////////////////
void CTabPaneManager::SetState(const QVariant& state)
{
	LOADING_TIME_PROFILE_SECTION;

	// During the layout load, Qt events must be processed in order to cleanup after all tools.
	// If the layout is loaded with a toolbar button, repeated clicking may cause this method to be called while the initial load is still in progress.
	// This can cause issues for some tools, especially MFC-based tools. We catch this case with a mutex in order to ignore these recursive layout load requests.
	static std::mutex reentry_protection;
	auto mutexLock = std::unique_lock<std::mutex>(reentry_protection, std::defer_lock);
	
	if (!state.isValid() || state.type() != QVariant::Map || !mutexLock.try_lock())
	{
		return;
	}

	QToolWindowManager::QTWMNotifyLock notifyLock = GetToolManager()->getNotifyLock(m_layoutLoaded);

	QVariantMap varMap = state.value<QVariantMap>();
	QVariant mainWindowStateVar = varMap.value("MainWindowState");
	QVariant mainWindowGeomVar = varMap.value("MainWindowGeometry");
	QVariant toolsLayoutVar = varMap.value("ToolsLayout");
	QVariantMap openToolsMap = varMap.value("Windows", varMap.value("OpenTools")).toMap();

	if (mainWindowStateVar.isValid())
	{
		GetToolManager()->hide();
		GetToolManager()->clear();

		for (QVariantMap::const_iterator iter = openToolsMap.begin(); iter != openToolsMap.end(); ++iter)
		{
			QString key = iter.key();
			LOADING_TIME_PROFILE_SECTION_ARGS(key.toStdString().c_str());
			QVariantMap v = iter.value().toMap();
			string className = v.value("class").toString().toStdString().c_str();
			QVariantMap state = v.value("state", QVariantMap()).toMap();

			QTabPane* pane = CreateTabPane(className, nullptr);
			if (pane)
			{
				pane->setObjectName(key);
				if (!state.empty() && pane->m_pane)
				{
					pane->m_pane->SetState(state);
				}
			}
		}

		GetToolManager()->restoreState(toolsLayoutVar);

		if (m_layoutLoaded)
		{
			// Don't change main window geometry after initial load, but make sure the window state is restored
			CEditorMainFrame::GetInstance()->restoreState(QtUtil::ToQByteArray(mainWindowStateVar));
			return;
		}

		QWidget* mainFrameWindow = CEditorMainFrame::GetInstance();
		while (mainFrameWindow->parentWidget())
		{
			mainFrameWindow = mainFrameWindow->parentWidget();
		}

		CEditorMainFrame::GetInstance()->restoreState(QtUtil::ToQByteArray(mainWindowStateVar));
		mainFrameWindow->restoreGeometry(QtUtil::ToQByteArray(mainWindowGeomVar));
		if (mainFrameWindow->windowState() == Qt::WindowMaximized)
		{
			QDesktopWidget* desktop = QApplication::desktop();
			mainFrameWindow->setGeometry(desktop->availableGeometry(mainFrameWindow));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTabPaneManager::PushUserEvent(const char* szEventName, const char* szTitle, const void* pAddress)
{
	UserAnalytics::Attributes attributes;
	attributes.AddAttribute("title", szTitle);
	char addressBuffer[32];
	cry_sprintf(addressBuffer, "0x%p", pAddress);
	attributes.AddAttribute("address", addressBuffer);
	USER_ANALYTICS_EVENT_ARG(szEventName, &attributes);
}

//////////////////////////////////////////////////////////////////////////
QTabPane::QTabPane()
{
	m_bViewCreated = false;
	m_pane = nullptr;
	m_MfcWnd = nullptr;

	setLayout(new QVBoxLayout);
	setFrameStyle(StyledPanel | Plain);
	layout()->setContentsMargins(0, 0, 0, 0);
	connect(this, &QTabPane::customContextMenuRequested, this, &QTabPane::showContextMenu);
	setAttribute(Qt::WA_DeleteOnClose);
}

//////////////////////////////////////////////////////////////////////////
void QTabPane::showContextMenu(const QPoint& point)
{
	QMenu menu(this);
	connect(menu.addAction(tr("Close")), &QAction::triggered, [this]() { close(); });

	menu.exec(QPoint(point.x(), point.y()));
}

#define WM_FRAME_CAN_CLOSE (WM_APP + 1000)

void QTabPane::closeEvent(QCloseEvent* event)
{
	if (m_pane)
	{
		bool isClosed = m_pane->GetWidget()->close();//Note: we may need a better event than close to handle closing tabs and unsaved changes situations
		event->setAccepted(isClosed);
		if (isClosed)
		{
			CTabPaneManager::GetInstance()->CloseTabPane(this);
		}
	}
	else if (m_MfcWnd)
	{
		BOOL bCanClose = TRUE;
		::SendMessage(m_MfcWnd->GetSafeHwnd(), WM_FRAME_CAN_CLOSE, (WPARAM)&bCanClose, 0);
		event->setAccepted(bCanClose);
		if (bCanClose)
		{
			m_MfcWnd->DestroyWindow();
			CTabPaneManager::GetInstance()->CloseTabPane(this);
			m_MfcWnd = NULL;
		}
	}
}

QTabPane::~QTabPane()
{
}

