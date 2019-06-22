// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QToolTabManager.h"

#include "IEditorImpl.h"
#include "QT/QtMainFrame.h"

#include <QMfcApp/qwinhost.h>

#include <EditorFramework/PersonalizationManager.h>
#include <FileDialogs/SystemFileDialog.h>
#include <QtUtil.h>

#include <CrySystem/IProjectManager.h>
#include <CrySystem/UserAnalytics/IUserAnalytics.h>

#include <QCloseEvent>
#include <QDesktopWidget>
#include <QJsonDocument>
#include <QLayout>

#include <mutex>

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

namespace Private_ToolTabManager
{
CTabPaneManager* s_pGlobalToolTabManager = 0;

static const char* szAppDataLayoutDir = "Layouts";
static const char* szDefaultLayoutDir = "Editor/Layouts";
static const char* szDefaultLayout = "Editor/Layouts/Default Layout.json";
static const char* szUserLayout = "Layout.json";

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
	QDir dir(UserDataUtil::GetUserPath("").c_str());
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
	QString userDataPath(UserDataUtil::GetUserPath(szAppDataLayoutDir).c_str());
	dir.mkpath(userDataPath);
	dir.cd(userDataPath);

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

void FindSubPanes(IPane* pPane, const char* paneClassName, std::vector<IPane*>& result)
{
	for (IPane* pSubPane : pPane->GetSubPanes())
	{
		if (strcmp(pSubPane->GetPaneTitle(), paneClassName) == 0)
		{
			result.push_back(pSubPane);
		}
		else
		{
			FindSubPanes(pSubPane, paneClassName, result);
		}
	}
}

}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ToolTabManager::PyLoadLayoutFromFile, layout, load,
                                     "Loads a layout from file.",
                                     "layout.load(str path)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ToolTabManager::PySaveLayoutToFile, layout, save,
                                     "Saves current layout to a file.",
                                     "layout.save(str absolutePath)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ToolTabManager::PyResetLayout, layout, reset,
                                     "Reset Layout",
                                     "");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ToolTabManager::PyLoadLayoutDlg, layout, load_dlg,
                                     "Load Layout...", "");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ToolTabManager::PySaveLayoutAs, layout, save_as,
                                     "Save Layout As...", "");

//////////////////////////////////////////////////////////////////////////
CTabPaneManager::CTabPaneManager(QWidget* const pParent)
	: CUserData({ Private_ToolTabManager::szAppDataLayoutDir, Private_ToolTabManager::szUserLayout })
	, m_pParent(pParent)
{
	Private_ToolTabManager::s_pGlobalToolTabManager = this;
	m_bToolsDirty = false;
	m_layoutLoaded = false;
}

CTabPaneManager::~CTabPaneManager()
{
	CloseAllPanes();
	Private_ToolTabManager::s_pGlobalToolTabManager = nullptr;
}

CTabPaneManager* CTabPaneManager::GetInstance()
{
	return Private_ToolTabManager::s_pGlobalToolTabManager;
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

QTabPane* CTabPaneManager::CreateTabPane(const char* paneClassName, const char* title, int nOverrideDockDirection, bool bLoadLayoutPersonalization)
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, paneClassName);
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
		}
		else if (!title)
		{
			title = pViewPaneClass->GetPaneTitle();
		}

		// Set default title
		pPane->m_title = title;
		pPane->setObjectName(CreateObjectName(title));
		pPane->setWindowTitle(QWidget::tr(title));

		pPane->m_category = pViewPaneClass->Category();

		// Initialize after content has default title
		if (pContentWidget)
		{
			pContentWidget->Initialize();

			// Load personalization after content is actually initialized
			if (bLoadLayoutPersonalization)
			{
				pContentWidget->LoadLayoutPersonalization();
			}
		}
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
	maxRc.setLeft(GetSystemMetrics(SM_XVIRTUALSCREEN));
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
	if (paneRect.height() < 10)
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

	IPane::s_signalPaneCreated(pPane->m_pane);

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

CWnd* CTabPaneManager::OpenMFCPane(const char* sPaneClassName)
{
	IClassDesc* pClassDesc = GetIEditorImpl()->GetClassFactory()->FindClass(sPaneClassName);
	if (!pClassDesc || ESYSTEM_CLASS_VIEWPANE != pClassDesc->SystemClassID())
	{
		return nullptr;
	}

	IViewPaneClass* pViewPaneClass = (IViewPaneClass*)pClassDesc;

	QTabPane* pTool = nullptr;
	// Check if view view pane class support only 1 pane at a time.
	if (pViewPaneClass->SinglePane())
	{
		pTool = FindTabPaneByClass(sPaneClassName);
	}

	if (!pTool)
	{
		pTool = CreateTabPane(sPaneClassName, NULL, -1, true);
	}

	if (!pTool)
	{
		return nullptr;
	}

	FocusTabPane(pTool);

	return pTool->m_MfcWnd;
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

	if (!tool)
	{
		tool = CreateTabPane(sPaneClassName);
	}

	if (!tool)
	{
		return nullptr;
	}

	FocusTabPane(tool);

	return tool->m_pane;
}

void CTabPaneManager::FocusTabPane(QTabPane* pPane)
{
	QWidget* pTopMostParent = pPane;

	while (QWidget* pParent = pTopMostParent->parentWidget())
	{
		pTopMostParent = pParent;
	}

	pTopMostParent->setWindowState((pTopMostParent->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);

	pPane->setFocus();
	pPane->activateWindow();
	GetToolManager()->bringToFront(pPane);
}

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

IPane* CTabPaneManager::FindPane(const std::function<bool(IPane*, const string& /*className*/)>& predicate)
{
	QList<QTabPane*> tools = FindTabPanes();
	for (int i = 0; i < tools.count(); i++)
	{
		QTabPane* tool = tools.at(i);
		if (tool->m_pane && predicate(tool->m_pane, tool->m_class))
		{
			return tool->m_pane;
		}
	}
	return nullptr;
}

std::vector<IPane*> CTabPaneManager::FindAllPanelsByClass(const char* paneClassName)
{
	using namespace Private_ToolTabManager;
	if (!CEditorMainFrame::GetInstance())
	{
		return {};
	}
	if (!GetToolManager())
	{
		return {};
	}
	std::vector<IPane*> result;
	QList<QTabPane*> tools = FindTabPanes();
	for (int i = 0; i < tools.count(); i++)
	{
		QTabPane* tool = tools.at(i);
		if (0 == strcmp(tool->m_class, paneClassName))
		{
			result.push_back(tool->m_pane);
		}
		else
		{
			FindSubPanes(tool->m_pane, paneClassName, result);
		}
	}
	return result;
}

void CTabPaneManager::SaveLayout()
{
	QJsonDocument doc(QJsonDocument::fromVariant(GetState()));
	UserDataUtil::Save(Private_ToolTabManager::szUserLayout, doc.toJson());
}

bool CTabPaneManager::LoadUserLayout()
{
	QVariant state = UserDataUtil::Load(Private_ToolTabManager::szUserLayout);

	if (!state.isValid())
		return false;

	SetState(state);
	m_layoutLoaded = true;

	return true;
}

bool CTabPaneManager::LoadLayout(const char* filePath)
{
	LOADING_TIME_PROFILE_AUTO_SESSION("sandbox_load_layout");
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

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
	using namespace Private_ToolTabManager;
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
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, fullFilename);
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
	using namespace Private_ToolTabManager;
	QStringList filter;
	filter << "*.json";

	QDir dir(UserDataUtil::GetUserPath(szAppDataLayoutDir).c_str());
	if (dir.exists())
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
	using namespace Private_ToolTabManager;
	QStringList filter;
	filter << "*.json";

	QString projectPath(GetIEditorImpl()->GetProjectManager()->GetCurrentProjectDirectoryAbsolute());
	projectPath = projectPath + "/" + szDefaultLayoutDir;
	QDir dir(projectPath);
	return dir.entryInfoList(filter, QDir::Files);
}

QFileInfoList CTabPaneManager::GetAppLayouts()
{
	using namespace Private_ToolTabManager;
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
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	bool bNeedsToDelete = false;
	while (!m_panes.empty())
	{
		bNeedsToDelete = true;
		auto it = m_panes.begin();
		(*it)->close();
	}

	if (bNeedsToDelete)
	{
		//This must be called to ensure the deferred delete of child widgets is called synchronously
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

void CTabPaneManager::OnIdle()
{
	if (m_bToolsDirty)
	{
		CreateContentInPanes();
		m_bToolsDirty = false;
	}
}

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

IPane* CTabPaneManager::CreatePaneContents(QTabPane* pTool)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	pTool->m_bViewCreated = true;

	IClassDesc* pClassDesc = GetIEditorImpl()->GetClassFactory()->FindClass(pTool->m_class);
	if (!pClassDesc || ESYSTEM_CLASS_VIEWPANE != pClassDesc->SystemClassID())
	{
		return nullptr;
	}

	IViewPaneClass* pViewPaneClass = (IViewPaneClass*)pClassDesc;
	IPane* pPane = pViewPaneClass->CreatePane();
	if (pPane)
	{
		QWidget* const pContentWidget = pPane->GetWidget();
		pTool->layout()->addWidget(pContentWidget);
		pTool->m_pane = pPane;

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
			CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "Loading MFC Tool");
			assert(pRuntimeClass->IsDerivedFrom(RUNTIME_CLASS(CWnd)) || pRuntimeClass == RUNTIME_CLASS(CWnd));
			pTool->m_MfcWnd = (CWnd*)pRuntimeClass->CreateObject();
			assert(pTool->m_MfcWnd);
			mfcHostWidget->setWindow(pTool->m_MfcWnd->GetSafeHwnd());
		}
	}
	return pPane;
}

QToolWindowManager* CTabPaneManager::GetToolManager() const
{
	return CEditorMainFrame::GetInstance()->GetToolManager();
}

void CTabPaneManager::LayoutLoaded()
{
	m_layoutLoaded = true;
}

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

void CTabPaneManager::SetState(const QVariant& state)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

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

		// We do not want to clear the tool manager at the application star-up time.
		// For some reason this changes the value of AfxGetMainWnd(), which can break the initialization of MFC tools.
		if (!GetToolManager()->toolWindows().empty())
		{
			GetToolManager()->clear();
		}

		for (QVariantMap::const_iterator iter = openToolsMap.begin(); iter != openToolsMap.end(); ++iter)
		{
			QString key = iter.key();
			CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, key.toStdString().c_str());
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

void CTabPaneManager::PushUserEvent(const char* szEventName, const char* szTitle, const void* pAddress)
{
	UserAnalytics::Attributes attributes;
	attributes.AddAttribute("title", szTitle);
	char addressBuffer[32];
	cry_sprintf(addressBuffer, "0x%p", pAddress);
	attributes.AddAttribute("address", addressBuffer);
	USER_ANALYTICS_EVENT_ARG(szEventName, &attributes);
}

QTabPane::QTabPane()
	: QBaseTabPane()
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

void QTabPane::showContextMenu(const QPoint& point)
{
	QMenu menu(this);
	connect(menu.addAction(tr("Close")), &QAction::triggered, [this]() { close(); });

	menu.exec(QPoint(point.x(), point.y()));
}

void QTabPane::focusInEvent(QFocusEvent* pEvent)
{
	if (m_pane && m_pane->GetWidget())
	{
		// Since QTabPane is just a wrapper class for proper panels and editors
		// delegate focus to direct child whenever focus is received.
		QWidget* pWidget = m_pane->GetWidget();
		pWidget->setFocus();
	}
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
