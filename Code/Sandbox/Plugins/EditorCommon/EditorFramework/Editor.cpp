// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "Editor.h"

#include "BroadcastManager.h"
#include "Commands/ICommandManager.h"
#include "Commands/QCommandAction.h"
#include "Controls/SaveChangesDialog.h"
#include "EditorContent.h"
#include "Events.h"
#include "Menu/AbstractMenu.h"
#include "Menu/MenuBarUpdater.h"
#include "Menu/MenuWidgetBuilders.h"
#include "PathUtils.h"
#include "PersonalizationManager.h"
#include "QtUtil.h"
#include "ToolBar/ToolBarCustomizeDialog.h"

#include <IEditor.h>

#include <QDesktopServices>
#include <QFile>
#include <QJsonDocument>
#include <QUrl>
#include <QVBoxLayout>
#include <QCloseEvent>

REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(editor, toggle_adaptive_layout, CCommandDescription("Enabled/disables adaptive layout for the focused editor"))
REGISTER_EDITOR_UI_COMMAND_DESC(editor, toggle_adaptive_layout, "Adaptive Layout", "", "", true)

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
	QTimer                  m_eventTimer;
	CDockableEditor&        m_dockableEditor;
	QMetaObject::Connection m_connection;
};

} // namespace Private_EditorFramework

CEditor::CEditor(QWidget* pParent /*= nullptr*/, bool bIsOnlyBackend /* = false */)
	: CEditorWidget(pParent)
	, m_broadcastManager(new CBroadcastManager())
	, m_bIsOnlybackend(bIsOnlyBackend)
	, m_dockingRegistry(nullptr)
	, m_pBroadcastManagerFilter(nullptr)
	, m_pActionAdaptiveLayout(nullptr)
	, m_isAdaptiveLayoutEnabled(true) // enabled by default for all editors who support this feature
{
	if (bIsOnlyBackend)
		return;

	m_pPaneMenu = new QMenu();

	setLayout(new QVBoxLayout());
	layout()->setMargin(0);
	layout()->setSpacing(0);
	m_pEditorContent = new CEditorContent(this);
	layout()->addWidget(m_pEditorContent);

	//Important so the focus is set to the CEditor when clicking on the menu.
	setFocusPolicy(Qt::StrongFocus);

	CBroadcastManager* const pGlobalBroadcastManager = GetIEditor()->GetGlobalBroadcastManager();
	pGlobalBroadcastManager->Connect(BroadcastEvent::AboutToQuit, this, &CEditor::OnMainFrameAboutToClose);

	InitActions();
	InitMenuDesc();

	m_pMenu.reset(new CAbstractMenu());
	m_pMenuUpdater.reset(new CMenuUpdater(m_pMenu.get(), m_pPaneMenu));

	//Help Menu is enabled by default
	AddToMenu(CEditor::MenuItems::HelpMenu);
	AddToMenu(CEditor::MenuItems::Help);
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

void CEditor::Initialize()
{
	if (SupportsAdaptiveLayout())
	{
		AddToMenu({ MenuItems::ViewMenu });
		CAbstractMenu* pViewMenu = GetMenu(MenuItems::ViewMenu);
		int section = pViewMenu->GetNextEmptySection();
		m_pActionAdaptiveLayout = pViewMenu->CreateCommandAction("editor.toggle_adaptive_layout", section);
		m_pActionAdaptiveLayout->setChecked(IsAdaptiveLayoutEnabled());
	}

	m_currentOrientation = GetDefaultOrientation();
	m_pEditorContent->Initialize();
}

void CEditor::InitMenuDesc()
{
	using namespace MenuDesc;
	m_pMenuDesc.reset(new CDesc<MenuItems>());
	m_pMenuDesc->Init(
		MenuDesc::AddMenu(MenuItems::FileMenu, 0, 0, "File",
		                  AddAction(MenuItems::New, 0, 0, [this]() { return GetAction("general.new"); }),
		                  AddAction(MenuItems::NewFolder, 0, 1, [this]() { return GetAction("general.new_folder"); }),
		                  AddAction(MenuItems::Open, 0, 2, [this]() { return GetAction("general.open"); }),
		                  AddAction(MenuItems::Close, 0, 3, [this]() { return GetAction("general.close"); }),
		                  AddAction(MenuItems::Save, 0, 4, [this]() { return GetAction("general.save"); }),
		                  AddAction(MenuItems::SaveAs, 0, 5, [this]() { return GetAction("general.save_as"); }),
		                  AddMenu(MenuItems::RecentFiles, 0, 6, "Recent Files")),
		MenuDesc::AddMenu(MenuItems::EditMenu, 0, 1, "Edit",
		                  AddAction(MenuItems::Undo, 0, 0, [this]() { return GetAction("general.undo"); }),
		                  AddAction(MenuItems::Redo, 0, 1, [this]() { return GetAction("general.redo"); }),
		                  AddAction(MenuItems::Copy, 1, 0, [this]() { return GetAction("general.copy"); }),
		                  AddAction(MenuItems::Cut, 1, 1, [this]() { return GetAction("general.cut"); }),
		                  AddAction(MenuItems::Paste, 1, 2, [this]() { return GetAction("general.paste"); }),
		                  AddAction(MenuItems::Rename, 1, 3, [this]() { return GetAction("general.rename"); }),
		                  AddAction(MenuItems::Delete, 1, 4, [this]() { return GetAction("general.delete"); }),
		                  AddAction(MenuItems::Find, 2, 0, [this]() { return GetAction("general.find"); }),
		                  AddAction(MenuItems::FindPrevious, 2, 1, [this]() { return GetAction("general.find_previous"); }),
		                  AddAction(MenuItems::FindNext, 2, 2, [this]() { return GetAction("general.find_next"); }),
		                  AddAction(MenuItems::SelectAll, 2, 3, [this]() { return GetAction("general.select_all"); }),
		                  AddAction(MenuItems::Duplicate, 3, 0, [this]() { return GetAction("general.duplicate"); })),
		MenuDesc::AddMenu(MenuItems::ViewMenu, 0, 2, "View",
		                  AddAction(MenuItems::ZoomIn, 0, 0, [this]() { return GetAction("general.zoom_in"); }),
		                  AddAction(MenuItems::ZoomOut, 0, 1, [this]() { return GetAction("general.zoom_out"); })),
		MenuDesc::AddMenu(MenuItems::ToolBarMenu, 0, 10, "Toolbars"),
		MenuDesc::AddMenu(MenuItems::WindowMenu, 0, 20, "Window"),
		MenuDesc::AddMenu(MenuItems::HelpMenu, 1, CAbstractMenu::EPriorities::ePriorities_Append, "Help",
		                  AddAction(MenuItems::Help, 0, 0, [this]() { return GetAction("general.help"); }))
		);

}

void CEditor::ForceRebuildMenu()
{
	m_pMenu->Build(MenuWidgetBuilders::CMenuBuilder(m_pPaneMenu));
}

void CEditor::SetContent(QWidget* content)
{
	CRY_ASSERT_MESSAGE(!m_dockingRegistry || m_pEditorContent->GetContent() != m_dockingRegistry, "CEditor: Internal docking system for %s will be replaced by content", GetEditorName());
	m_pEditorContent->SetContent(content);
}

void CEditor::SetContent(QLayout* content)
{
	CRY_ASSERT_MESSAGE(!m_dockingRegistry || m_pEditorContent->GetContent() != m_dockingRegistry, "CEditor: Internal docking system for %s will be replaced by content", GetEditorName());
	m_pEditorContent->SetContent(content);
}

void CEditor::InitActions()
{
	RegisterAction("general.find", &CEditor::OnFind);
	RegisterAction("general.find_previous", &CEditor::OnFindPrevious);
	RegisterAction("general.find_next", &CEditor::OnFindNext);
	RegisterAction("general.help", &CEditor::OnHelp);
	RegisterAction("editor.toggle_adaptive_layout", [this]()
	{
		SetAdaptiveLayoutEnabled(!IsAdaptiveLayoutEnabled());
		return true;
	});
	RegisterAction("toolbar.customize", [this]()
	{
		return m_pEditorContent->CustomizeToolBar();
	});
	RegisterAction("toolbar.toggle_lock", [this]()
	{
		return m_pEditorContent->ToggleToolBarLock();
	});
	RegisterAction("toolbar.insert_expanding_spacer", [this]()
	{
		return m_pEditorContent->AddExpandingSpacer();
	});
	RegisterAction("toolbar.insert_fixed_spacer", [this]()
	{
		return m_pEditorContent->AddFixedSpacer();
	});
}

void CEditor::AddToMenu(CAbstractMenu* pMenu, const char* command)
{
	assert(pMenu && command);
	if (!pMenu || !command)
		return;

	auto action = GetAction(command);
	if (action)
		pMenu->AddAction(action, 0, 0);
}

QCommandAction* CEditor::GetMenuAction(MenuItems item)
{
	return m_pMenuDesc->GetAction(item);
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

void CEditor::RegisterDockableWidget(QString name, std::function<QWidget*()> factory, bool isUnique /*= false*/, bool isInternal /*= false*/)
{
	if (!m_pBroadcastManagerFilter)
	{
		m_pBroadcastManagerFilter = new Private_EditorFramework::CBroadcastManagerFilter(GetBroadcastManager());
	}

	//This filter is needed because the widget may not always be in the child hierarchy of this broadcast manager
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
	if (state.contains("adaptiveLayout"))
	{
		SetAdaptiveLayoutEnabled(state["adaptiveLayout"].toBool());
	}
	if (m_dockingRegistry && state.contains("dockingState"))
	{
		m_dockingRegistry->SetState(state["dockingState"].toMap());
	}
	if (state.contains("editorContent"))
	{
		m_pEditorContent->SetState(state["editorContent"].toMap());
	}
}

QVariantMap CEditor::GetLayout() const
{
	QVariantMap result;
	if (m_dockingRegistry)
	{
		result.insert("dockingState", m_dockingRegistry->GetState());
	}

	result.insert("editorContent", m_pEditorContent->GetState());
	result.insert("adaptiveLayout", m_isAdaptiveLayoutEnabled);

	return result;
}

void CEditor::OnLayoutChange(const QVariantMap& state)
{
	SetProperty("dockLayout", state);
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

void CEditor::UpdateAdaptiveLayout()
{
	// If adaptive layout is disabled, set current orientation to default
	if (!IsAdaptiveLayoutEnabled())
	{
		m_currentOrientation = GetDefaultOrientation();
		return OnAdaptiveLayoutChanged();
	}

	Qt::Orientation newOrientation;
	if (width() > height())
		newOrientation = Qt::Horizontal;
	else
		newOrientation = Qt::Vertical;

	if (newOrientation == m_currentOrientation)
		return;

	// Before switching orientation, make sure the current size isn't smaller than the minimum size
	// supported by the new orientation of the editor. Otherwise we get layout flickering
	QSize minSize = m_pEditorContent->GetMinimumSizeForOrientation(newOrientation);
	QSize currentSize = size();
	if (currentSize.width() < minSize.width() || currentSize.height() < minSize.height())
		return;

	m_currentOrientation = newOrientation;
	OnAdaptiveLayoutChanged();
}

void CEditor::SetAdaptiveLayoutEnabled(bool enable)
{
	if (m_isAdaptiveLayoutEnabled == enable)
		return;

	m_isAdaptiveLayoutEnabled = enable;
	m_pActionAdaptiveLayout->setChecked(m_isAdaptiveLayoutEnabled);
	UpdateAdaptiveLayout();
}

void CEditor::OnAdaptiveLayoutChanged()
{
	signalAdaptiveLayoutChanged(m_currentOrientation);
}

void CEditor::resizeEvent(QResizeEvent* pEvent)
{
	QWidget::resizeEvent(pEvent);

	// Early out if adaptive layout is disabled or this editor doesn't support adaptive layout
	if (!IsAdaptiveLayoutEnabled())
		return;

	UpdateAdaptiveLayout();
}

void CEditor::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::GetBroadcastManager)
	{
		static_cast<GetBroadcastManagerEvent*>(pEvent)->SetManager(&GetBroadcastManager());
		pEvent->accept();
	}
	else
	{
		CEditorWidget::customEvent(pEvent);
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
	if (m_pReleaseMouseFilter)
	{
		m_pReleaseMouseFilter->deleteLater();
		m_pReleaseMouseFilter = nullptr;
	}
}

QMenu* CDockableEditor::GetPaneMenu() const
{
	return m_pPaneMenu;
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
