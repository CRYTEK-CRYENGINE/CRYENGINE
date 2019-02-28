// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "Editor.h"

#include "Events.h"

#include <QDesktopServices>
#include <QFile>
#include <QJsonDocument>
#include <QMenuBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QCloseEvent>

#include "PersonalizationManager.h"
#include "ICommandManager.h"
#include "Controls/SaveChangesDialog.h"
#include "Menu/AbstractMenu.h"
#include "Menu/MenuBarUpdater.h"
#include "Menu/MenuWidgetBuilders.h"

#include "QtUtil.h"
#include "FilePathUtil.h"

namespace Private_EditorFramework
{
	static const int s_maxRecentFiles = 10;

	class CBroadcastManagerFilter : public QObject
	{
	public:
		explicit CBroadcastManagerFilter(CBroadcastManager& broadcastManager)
			: m_broadcastManager(broadcastManager)
		{
		}

	protected:
		virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override
		{
			if (pEvent->type() == SandboxEvent::GetBroadcastManager)
			{
				static_cast<GetBroadcastManagerEvent*>(pEvent)->SetManager(&m_broadcastManager);
				pEvent->accept();
				return true;
			}
			else
			{
				return QObject::eventFilter(pObject, pEvent);
			}
		}

	private:
		CBroadcastManager& m_broadcastManager;
	};

	class CReleaseMouseFilter : public QObject
	{
	public:
		explicit CReleaseMouseFilter(CDockableEditor& dockableEditor)
			: m_dockableEditor(dockableEditor)
		{
			m_connection = connect(&m_eventTimer, &QTimer::timeout, [this]()
			{ 
				m_dockableEditor.SaveLayoutPersonalization();  
			});
			m_eventTimer.setSingleShot(true);

			connect(&dockableEditor, &QObject::destroyed, [this]()
			{
				disconnect(m_connection);
			});
		}

	protected:
		virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override
		{	
			if (pEvent->type() == QEvent::MouseButtonRelease)
			{
				m_eventTimer.start(1000);
			}
			return QObject::eventFilter(pObject, pEvent);            
	}

	private:
		QTimer           m_eventTimer;
		CDockableEditor& m_dockableEditor;
		QMetaObject::Connection m_connection;
	};

} // namespace Private_EditorFramework

CEditor::CEditor(QWidget* pParent /*= nullptr*/, bool bIsOnlyBackend /* = false */)
	: QWidget(pParent)
	, m_pMenuBar(nullptr)
	, m_broadcastManager(new CBroadcastManager())
	, m_bIsOnlybackend(bIsOnlyBackend)
	, m_dockingRegistry(nullptr)
	, m_pBroadcastManagerFilter(nullptr)
{
	if (bIsOnlyBackend)
		return;

	m_pMenuBar = new QMenuBar();

	setLayout(new QVBoxLayout());
	layout()->setContentsMargins(1, 1, 1, 1);
	layout()->addWidget(m_pMenuBar);

	//Important so the focus is set to the CEditor when clicking on the menu.
	setFocusPolicy(Qt::StrongFocus);

	CBroadcastManager* const pGlobalBroadcastManager = GetIEditor()->GetGlobalBroadcastManager();
	pGlobalBroadcastManager->Connect(BroadcastEvent::AboutToQuit, this, &CEditor::OnMainFrameAboutToClose);

	InitMenuDesc();

	m_pMenu.reset(new CAbstractMenu());
	m_pMenuBarBar.reset(new CMenuBarUpdater(m_pMenu.get(), m_pMenuBar));
}

CEditor::~CEditor()
{
	//Deleting the broadcast manager deferred as children may be trying to detach from the broadcast manager on delete.
	//Note: If we observe that children are still being deleted after the broadcast manager,
	//we can call deleteLater() on top level children here BEFORE calling it on the broadcast manager to ensure order.
	m_broadcastManager->deleteLater();
	GetIEditor()->GetGlobalBroadcastManager()->DisconnectObject(this);

	if (m_pBroadcastManagerFilter)
	{
		m_pBroadcastManagerFilter->deleteLater();
		m_pBroadcastManagerFilter = nullptr;
	}
}

void CEditor::InitMenuDesc()
{
	using namespace MenuDesc;
	// #TODO: Make this static?
	m_pMenuDesc.reset(new CDesc<MenuItems>());
	m_pMenuDesc->Init(
		MenuDesc::AddMenu ( MenuItems::FileMenu, 0, 0, "File",
			AddAction(MenuItems::New,    0, 0, GetAction("general.new")),
			AddAction(MenuItems::Open,   0, 1, GetAction("general.open")),
			AddAction(MenuItems::Close,  0, 2, GetAction("general.close")),
			AddAction(MenuItems::Save,   0, 3, GetAction("general.save")),
			AddAction(MenuItems::SaveAs, 0, 4, GetAction("general.save_as")),
			AddMenu(MenuItems::RecentFiles, 0, 5, "Recent Files")
		),
		MenuDesc::AddMenu ( MenuItems::EditMenu, 0, 1, "Edit",
			AddAction ( MenuItems::Undo, 0, 0, GetAction("general.undo") ),
			AddAction ( MenuItems::Redo, 0, 1, GetAction("general.redo") ),

			AddAction ( MenuItems::Copy,   1, 0, GetAction("general.copy") ),
			AddAction ( MenuItems::Cut,    1, 1, GetAction("general.cut") ),
			AddAction ( MenuItems::Paste,  1, 2, GetAction("general.paste") ),
			AddAction ( MenuItems::Delete, 1, 3, GetAction("general.delete") ),

			AddAction ( MenuItems::Find,         2, 0, GetAction("general.find") ),
			AddAction ( MenuItems::FindPrevious, 2, 1, GetAction("general.find_previous") ),
			AddAction ( MenuItems::FindNext,     2, 2, GetAction("general.find_next") ),
			AddAction ( MenuItems::SelectAll,    2, 3, GetAction("general.select_all") ),

			AddAction ( MenuItems::Duplicate, 3, 0, GetAction("general.duplicate") )
		),
		MenuDesc::AddMenu ( MenuItems::ViewMenu, 0, 2, "View",
			AddAction ( MenuItems::ZoomIn,  0, 0, GetAction("general.zoom_in") ),
			AddAction ( MenuItems::ZoomOut, 0, 1, GetAction("general.zoom_out") )
		),
		MenuDesc::AddMenu ( MenuItems::WindowMenu, 0, 20, "Window"
		),
		MenuDesc::AddMenu ( MenuItems::HelpMenu,   0, 21, "Help",
			AddAction ( MenuItems::Help, 0, 0, GetAction("general.help") )
		)
	);

}

void CEditor::SetContent(QWidget* content)
{
	//TODO : only one content can be set
	content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout()->addWidget(content);
}

void CEditor::SetContent(QLayout* content)
{
	//TODO : only one content can be set
	QWidget* w = new QWidget();
	w->setLayout(content);
	layout()->addWidget(w);
}

void CEditor::AddToMenu(CAbstractMenu* pMenu, const char* command)
{
	assert(pMenu && command);
	if (!pMenu || !command)
		return;

	auto action = GetAction(command);
	if(action)
		pMenu->AddAction(action, 0 , 0);
}

QAction* CEditor::GetAction(const char* command)
{
	QAction* pAction = GetIEditor()->GetICommandManager()->GetAction(command);

	if (!pAction)
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR_DBGBRK, "Command not found");

	return pAction;
}

bool CEditor::OnHelp()
{
	return EditorUtils::OpenHelpPage(GetEditorName());
}

void CEditor::AddToMenu(MenuItems item)
{
	m_pMenuDesc->AddItem(m_pMenu.get(), item);

	if (item == MenuItems::RecentFiles)
	{
		CAbstractMenu* pRecentFilesMenu = GetMenu(MenuItems::RecentFiles);
		pRecentFilesMenu->signalAboutToShow.Connect([pRecentFilesMenu, this]()
		{
			PopulateRecentFilesMenu(pRecentFilesMenu);
		});
	}
}

void CEditor::AddToMenu(const MenuItems* items, int count)
{
	for (int i = 0; i < count; i++)
	{
		AddToMenu(items[i]);
	}
}

void CEditor::AddToMenu(const std::vector<MenuItems>& items)
{
	for (const MenuItems& item : items)
	{
		AddToMenu(item);
	}
}

void CEditor::AddToMenu(const char* menuName, const char* command)
{
	AddToMenu(GetMenu(menuName), command);
}

CAbstractMenu* CEditor::GetRootMenu()
{
	return m_pMenu.get();
}

CAbstractMenu* CEditor::GetMenu(const char* menuName)
{
	auto pMenu = m_pMenu->FindMenu(menuName);
	if (!pMenu)
		pMenu = m_pMenu->CreateMenu(menuName);
	
	return pMenu;
}

CAbstractMenu* CEditor::GetMenu(MenuItems menuItem)
{
	const string name = m_pMenuDesc->GetMenuName(menuItem);
	return !name.empty() ? m_pMenu->FindMenuRecursive(name.c_str()) : nullptr;
}

CAbstractMenu* CEditor::GetMenu(const QString& menuName)
{
	return GetMenu(menuName.toStdString().c_str());
}

void CEditor::EnableDockingSystem()
{
	if (m_dockingRegistry)
	{
		return;
	}

	//Add window menu in the correct position beforehand
	AddToMenu(MenuItems::WindowMenu);

	m_dockingRegistry = new CDockableContainer(this, GetProperty("dockLayout").toMap());
	connect(m_dockingRegistry, &CDockableContainer::OnLayoutChange, this, &CEditor::OnLayoutChange);
	m_dockingRegistry->SetDefaultLayoutCallback([=](CDockableContainer* sender) { CreateDefaultLayout(sender); });
	m_dockingRegistry->SetMenu(GetMenu(MenuItems::WindowMenu));
	SetContent(m_dockingRegistry);
}

void CEditor::RegisterDockableWidget(QString name, std::function<QWidget * ()> factory, bool isUnique, bool isInternal)
{
	using namespace Private_EditorFramework;

	if(!m_pBroadcastManagerFilter)
	{
		m_pBroadcastManagerFilter = new Private_EditorFramework::CBroadcastManagerFilter(GetBroadcastManager());
	}

	//This filter is needed because the widget may not alway be in the child hierarchy of this broadcast manager
	QPointer<QObject> pFilter(m_pBroadcastManagerFilter);
	auto wrapperFactory = [name, factory, pFilter]() -> QWidget*
	{
		QWidget* const pWidget = factory();
		CRY_ASSERT(pWidget);
		pWidget->setWindowTitle(name);
		pWidget->installEventFilter(pFilter.data());
		return pWidget;
	};

	m_dockingRegistry->Register(name, wrapperFactory, isUnique, isInternal);
}

void CEditor::SetLayout(const QVariantMap& state)
{
	if (m_dockingRegistry && state.contains("dockingState"))
	{
		m_dockingRegistry->SetState(state["dockingState"].toMap());
	}
}

QVariantMap CEditor::GetLayout() const
{
	QVariantMap result;
	if (m_dockingRegistry)
	{
		result.insert("dockingState", m_dockingRegistry->GetState());
	}
	return result;
}

void CEditor::OnLayoutChange(const QVariantMap& state)
{
	SetProperty("dockLayout", state);
}

QMenuBar* CEditor::GetMenuBar()
{
	return m_pMenuBar;
}

void CEditor::OnMainFrameAboutToClose(BroadcastEvent& event)
{
	if (event.type() == BroadcastEvent::AboutToQuit)
	{
		std::vector<string> changedFiles;
		if (!CanQuit(changedFiles))
		{
			AboutToQuitEvent& aboutToQuitEvent = (AboutToQuitEvent&)event;
			aboutToQuitEvent.AddChangeList(GetEditorName(), changedFiles);

			event.ignore();
		}
	}
}

void CEditor::AddRecentFile(const QString& filePath)
{
	auto recent = GetRecentFiles();

	int index = recent.indexOf(filePath);
	if (index > -1)
	{
		recent.removeAt(index);
	}

	recent.push_front(filePath);

	if (recent.size() > Private_EditorFramework::s_maxRecentFiles)
		recent = recent.mid(0, Private_EditorFramework::s_maxRecentFiles);

	SetProjectProperty("Recent Files", recent);
}

QStringList CEditor::GetRecentFiles()
{
	QVariant property = GetProjectProperty("Recent Files");
	return property.toStringList();
}

void CEditor::PopulateRecentFilesMenu(CAbstractMenu* menu)
{
	menu->Clear();
	QStringList recentPaths = GetRecentFiles();

	for (int i = 0; i < recentPaths.size(); ++i)
	{
		const QString& path = recentPaths[i];
		auto action = menu->CreateAction(path);
		connect(action, &QAction::triggered, [=]() { OnOpenFile(path); });
	}
}

void CEditor::SetProperty(const QString& propName, const QVariant& value)
{
	GetIEditor()->GetPersonalizationManager()->SetProperty(GetEditorName(), propName, value);
}

const QVariant& CEditor::GetProperty(const QString& propName)
{
	return GetIEditor()->GetPersonalizationManager()->GetProperty(GetEditorName(), propName);
}

void CEditor::SetProjectProperty(const QString& propName, const QVariant& value)
{
	GetIEditor()->GetPersonalizationManager()->SetProjectProperty(GetEditorName(), propName, value);
}

const QVariant& CEditor::GetProjectProperty(const QString& propName)
{
	return GetIEditor()->GetPersonalizationManager()->GetProjectProperty(GetEditorName(), propName);
}

void CEditor::SetPersonalizationState(const QVariantMap& state)
{
	GetIEditor()->GetPersonalizationManager()->SetState(GetEditorName(), state);
}

const QVariantMap& CEditor::GetPersonalizationState()
{
	return GetIEditor()->GetPersonalizationManager()->GetState(GetEditorName());
}

void CEditor::customEvent(QEvent* event)
{
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		//Note: this could be optimized this with a hash map
		//TODO : better system of macros and registration of those commands in EditorCommon (or move all of this in Editor)
		const string& command = commandEvent->GetCommand();
		if (command == "general.new")
			event->setAccepted(OnNew());
		else if (command == "general.save")
			event->setAccepted(OnSave());
		else if (command == "general.save_as")
			event->setAccepted(OnSaveAs());
		else if (command == "general.open")
			event->setAccepted(OnOpen());
		else if (command == "general.close")
			event->setAccepted(OnClose());
		else if (command == "general.copy")
			event->setAccepted(OnCopy());
		else if (command == "general.cut")
			event->setAccepted(OnCut());
		else if (command == "general.paste")
			event->setAccepted(OnPaste());
		else if (command == "general.delete")
			event->setAccepted(OnDelete());
		else if (command == "general.find")
			event->setAccepted(OnFind());
		else if (command == "general.find_previous")
			event->setAccepted(OnFindPrevious());
		else if (command == "general.find_next")
			event->setAccepted(OnFindNext());
		else if (command == "general.duplicate")
			event->setAccepted(OnDuplicate());
		else if (command == "general.select_all")
			event->setAccepted(OnSelectAll());
		else if (command == "general.help")
			event->setAccepted(OnHelp());
		else if (command == "general.zoom_in")
			event->setAccepted(OnZoomIn());
		else if (command == "general.zoom_out")
			event->setAccepted(OnZoomOut());
	}
	else if (event->type() == SandboxEvent::GetBroadcastManager)
	{
		static_cast<GetBroadcastManagerEvent*>(event)->SetManager(&GetBroadcastManager());
		event->accept();
	}
	else
	{
		QWidget::customEvent(event);
	}
}

CBroadcastManager& CEditor::GetBroadcastManager()
{
	return *m_broadcastManager;
}

CDockableEditor::CDockableEditor(QWidget* pParent)
    : CEditor(pParent)
{
	m_pReleaseMouseFilter = new Private_EditorFramework::CReleaseMouseFilter(*this);
	setAttribute(Qt::WA_DeleteOnClose);
}

CDockableEditor::~CDockableEditor()
{
	if(m_pReleaseMouseFilter)
	{
		m_pReleaseMouseFilter->deleteLater();
		m_pReleaseMouseFilter = nullptr;
	}
}

void CDockableEditor::LoadLayoutPersonalization()
{
	auto personalization = GetPersonalizationState();

	QVariant layout = personalization.value("layout");
	if (layout.isValid())
	{
		SetLayout(layout.toMap());
	}
}

void CDockableEditor::SaveLayoutPersonalization()
{
	auto layout = GetLayout();
	auto personalization = GetPersonalizationState();

	personalization.insert("layout", layout);
	SetPersonalizationState(personalization);
}

void CDockableEditor::Raise()
{
	GetIEditor()->RaiseDockable(this);
}

void CDockableEditor::Highlight()
{
	//TODO : implement this !
}

void CDockableEditor::InstallReleaseMouseFilter(QObject* object)
{
	object->installEventFilter(m_pReleaseMouseFilter);

	QList<QWidget*> childWidgets = object->findChildren<QWidget*>();
	for (QWidget* pChild : childWidgets)
	{
		InstallReleaseMouseFilter(pChild);
	}
}
