// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ToolBarService.h"

#include "Commands/CustomCommand.h"
#include "Commands/CommandManager.h"
#include "Commands/QCommandAction.h"
#include "CryIcon.h"
#include "EditorFramework/Editor.h"
#include "FileUtils.h"
#include "PathUtils.h"
#include "QtUtil.h"
#include "Util/UserDataUtil.h"

#include <CryString/CryPath.h>
#include <CrySystem/IProjectManager.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QToolBar>
#include <QToolButton>

namespace Private_ToolbarManager
{
static const char* szUserToolbarsPath = "ToolBars";
static const char* s_defaultPath = "Editor/ToolBars";
static const char* s_actionPropertyName = "cvarDescValue";
static const char* s_actionCVarBitFlagName = "cvarDescIsBitFlag";
static const int s_version = 2;
}

#define ADD_TO_VARIANT_MAP(var, map) map[ # var] = var

// In charge of mapping cvar actions.
// - When a cvar is triggered, make sure the action is executed and it's state is reflected
// - When an action is destroyed, remove cvar callback
namespace Private_ToolbarManager
{
class CVarActionMapper : public QObject
{
public:
	~CVarActionMapper()
	{
		for (const QMetaObject::Connection& connection : m_cvarActionConnections)
			disconnect(connection);
	}

	QAction* AddCVar(std::shared_ptr<CToolBarService::QCVarDesc> pCVarDesc)
	{
		const QString& iconPath = pCVarDesc->GetIcon();
		CryIcon icon(iconPath.isEmpty() ? "icons:General/File.ico" : iconPath);
		QAction* pAction = new QAction(icon, QString("%1 %2").arg(pCVarDesc->GetName(), pCVarDesc->GetValue().toString()), nullptr);
		pAction->setCheckable(true);
		string cvarName = QtUtil::ToString(pCVarDesc->GetName());
		ICVar* pCVar = gEnv->pConsole->GetCVar(cvarName.c_str());

		if (!pCVar)
		{
			// If no cvar was found and the name was valid, then issue warning
			if (!cvarName.empty())
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Invalid CVar for action");
			}
			return pAction;
		}

		std::vector<QAction*> actions;
		auto ite = m_cvarActions.find(pCVar);
		if (ite != m_cvarActions.end())
		{
			actions = ite->second;
		}
		else
		{
			pCVar->AddOnChange([this, pCVar]
				{
					this->OnCVarChanged(pCVar);
				});
		}

		pAction->setProperty(Private_ToolbarManager::s_actionPropertyName, pCVarDesc->GetValue());
		pAction->setProperty(Private_ToolbarManager::s_actionCVarBitFlagName, pCVarDesc->IsBitFlag());
		actions.push_back(pAction);
		m_cvarActions.insert_or_assign(pCVar, actions);

		m_cvarActionConnections.push_back(connect(pAction, &QAction::destroyed, [this, pCVar](QObject* pObject)
			{
				this->OnCVarActionDestroyed(pCVar, static_cast<QAction*>(pObject));
			}));

		OnCVarChanged(pCVar);

		connect(pAction, &QAction::triggered, [this, pAction, pCVarDesc, pCVar](bool bChecked)
			{
				if (!pCVar)
					return;

				QVariant variant = pCVarDesc->GetValue();
				switch (pCVar->GetType())
				{
				case ECVarType::Int:
					{
					  int currValue = pCVar->GetIVal();
					  int newValue = variant.toInt();

					  bool isBitFlag = pCVarDesc->IsBitFlag();

					  if (isBitFlag)
							pCVar->Set(currValue & newValue ? currValue & ~newValue : currValue | newValue);
					  else
							pCVar->Set(pCVar->GetIVal() == newValue ? 0 : newValue);
					}
					break;
				case ECVarType::Float:
					pCVar->Set(pCVar->GetFVal() == variant.toFloat() ? 0.0f : variant.toFloat());
					break;
				case ECVarType::String:
					pCVar->Set(variant.toString().toStdString().c_str());
				case ECVarType::Int64:
					CRY_ASSERT_MESSAGE(false, "CEditorToolBarService::CreateToolBar int64 cvar not implemented");
					break;
				default:
					break;
				}

				OnCVarChanged(pCVar);
			});

		return pAction;
	}

	void OnCVarChanged(ICVar* pCVar)
	{
		auto ite = m_cvarActions.find(pCVar);
		if (ite == m_cvarActions.end())
			return;

		std::vector<QAction*>& actions = ite->second;

		switch (pCVar->GetType())
		{
		case ECVarType::Int:
			for (QAction* pAction : actions)
			{
				bool isBitFlag = pAction->property(Private_ToolbarManager::s_actionCVarBitFlagName).toBool();
				int value = pAction->property(Private_ToolbarManager::s_actionPropertyName).toInt();
				pAction->setChecked(isBitFlag ? value & pCVar->GetIVal() : value == pCVar->GetIVal());
			}
			break;
		case ECVarType::Float:
			for (QAction* pAction : actions)
				pAction->setChecked(pAction->property(Private_ToolbarManager::s_actionPropertyName) == pCVar->GetFVal());
			break;
		case ECVarType::String:
			for (QAction* pAction : actions)
				pAction->setChecked(pAction->property(Private_ToolbarManager::s_actionPropertyName) == QString(pCVar->GetString()));
			break;
		case ECVarType::Int64:
			for (QAction* pAction : actions)
			{
				bool isBitFlag = pAction->property(Private_ToolbarManager::s_actionCVarBitFlagName).toBool();
				int64 value = pAction->property(Private_ToolbarManager::s_actionPropertyName).toLongLong();
				pAction->setChecked(isBitFlag ? value & pCVar->GetI64Val() : value == pCVar->GetI64Val());
			}
			break;
		default:
			break;
		}
	}

	void OnCVarActionDestroyed(ICVar* pCVar, QAction* pObject)
	{
		auto ite = m_cvarActions.find(pCVar);
		if (ite == m_cvarActions.end())
			return;

		std::vector<QAction*>& actions = ite->second;
		auto actionIte = std::remove(actions.begin(), actions.end(), pObject);
		if (actionIte != actions.end())
			actions.erase(actionIte);
	}

	// Many actions can be associated with a cvar since an action can only have on and off state and a cvar can have many possible values
	std::unordered_map<ICVar*, std::vector<QAction*>> m_cvarActions;
	// Keep track of all connections to actions so we can disconnect on destruction to prevent callbacks triggering on destroyed actions
	std::vector<QMetaObject::Connection>              m_cvarActionConnections;
};
}

CToolBarService::QCommandDesc::QCommandDesc(const QVariantMap& variantMap, int version)
	: command(variantMap["command"].toString())
	, m_IsDeprecated(false)
{
	if (version == 1)
	{
		name = variantMap["uiName"].toString();
		iconPath = variantMap["icon"].toString();
		m_IsCustom = variantMap["custom"].toBool();
	}
	else
	{
		name = variantMap["name"].toString();
		iconPath = variantMap["iconPath"].toString();
		m_IsCustom = variantMap["bIsCustom"].toBool();
	}

	// Check to see if the command needs to be remapped
	m_IsDeprecated = IsDeprecated();
	if (m_IsDeprecated)
	{
		CCommand* pCommand = GetIEditor()->GetICommandManager()->GetCommand(QtUtil::ToString(command).c_str());
		// Discard serialized data and get all relevant data directly from new command
		if (pCommand)
		{
			InitFromCommand(pCommand);
		}
	}
}

CToolBarService::QCommandDesc::QCommandDesc(const CCommand* pCommand)
	: m_IsDeprecated(false)
{
	InitFromCommand(pCommand);
}

void CToolBarService::QCommandDesc::InitFromCommand(const CCommand* pCommand)
{
	const CUiCommand* pUiCommand = static_cast<const CUiCommand*>(pCommand);
	CUiCommand::UiInfo* pInfo = pUiCommand->GetUiInfo();

	name = QtUtil::ToQString(pCommand->GetName());
	command = QtUtil::ToQString(pCommand->GetCommandString());
	iconPath = QtUtil::ToQString(pInfo->icon);
	m_IsCustom = pCommand->IsCustomCommand();
}

QVariant CToolBarService::QCommandDesc::ToVariant() const
{
	QCommandAction* pAction = ToQCommandAction();

	QVariantMap map;
	ADD_TO_VARIANT_MAP(name, map);
	ADD_TO_VARIANT_MAP(command, map);

	// Don't serialize icon path unless it's an icon override chosen by the user
	if (pAction && (m_IsCustom || pAction->UiInfo::icon != QtUtil::ToString(iconPath)))
	{
		ADD_TO_VARIANT_MAP(iconPath, map);
	}

	ADD_TO_VARIANT_MAP(m_IsCustom, map);
	map["type"] = Command;
	return map;
}

QCommandAction* CToolBarService::QCommandDesc::ToQCommandAction() const
{
	CCommand* pCommand = GetIEditor()->GetICommandManager()->GetCommand(QtUtil::ToString(command).c_str());
	if (!pCommand && IsCustom())
	{
		string commandNameStr = QtUtil::ToString(name);
		string commandStr = QtUtil::ToString(command);
		CCustomCommand* pCustomCommand = new CCustomCommand(commandNameStr, commandStr);
		pCustomCommand->Register();
		pCommand = pCustomCommand;
	}

	if (!pCommand || !pCommand->CanBeUICommand())
		return nullptr;

	CUiCommand* pUiCommand = static_cast<CUiCommand*>(pCommand);
	CUiCommand::UiInfo* pInfo = pUiCommand->GetUiInfo();

	if (!pInfo)
		return nullptr;

	QCommandAction* pAction = static_cast<QCommandAction*>(pInfo);

	return pAction;
}

bool CToolBarService::QCommandDesc::IsDeprecated() const
{
	return m_IsDeprecated || GetIEditor()->GetICommandManager()->IsCommandDeprecated(QtUtil::ToString(command).c_str());
}

void CToolBarService::QCommandDesc::SetName(const QString& n)
{
	name = n;
	commandChangedSignal();
}

void CToolBarService::QCommandDesc::SetIcon(const QString& path)
{
	iconPath = path;
	commandChangedSignal();
}

//////////////////////////////////////////////////////////////////////////

CToolBarService::QCVarDesc::QCVarDesc(const QVariantMap& variantMap, int version)
	: name(variantMap["name"].toString())
	, iconPath(variantMap["iconPath"].toString())
	, value(variantMap["value"])
	, isBitFlag(false)
{
	if (variantMap.contains("isBitFlag"))
		isBitFlag = variantMap["isBitFlag"].toBool();
}

QVariant CToolBarService::QCVarDesc::ToVariant() const
{
	QVariantMap map;
	ADD_TO_VARIANT_MAP(name, map);
	ADD_TO_VARIANT_MAP(iconPath, map);
	ADD_TO_VARIANT_MAP(value, map);
	ADD_TO_VARIANT_MAP(isBitFlag, map);
	map["type"] = CVar;
	return map;
}

void CToolBarService::QCVarDesc::SetCVar(const QString& cvar)
{
	name = cvar;
	cvarChangedSignal();
}

void CToolBarService::QCVarDesc::SetCVarValue(const QVariant& cvarValue)
{
	value = cvarValue;
	cvarChangedSignal();
}

void CToolBarService::QCVarDesc::SetIcon(const QString& path)
{
	iconPath = path;
	cvarChangedSignal();
}

//////////////////////////////////////////////////////////////////////////

void CToolBarService::QToolBarDesc::Initialize(const QVariantList& commandList, int version)
{
	for (const QVariant& commandVar : commandList)
	{
		std::shared_ptr<QItemDesc> pItem = CreateItem(commandVar, version);
		if (!pItem)
			continue;

		items.push_back(pItem);
	}
}

std::shared_ptr<CToolBarService::QItemDesc> CToolBarService::QToolBarDesc::CreateItem(const QVariant& itemVariant, int version)
{
	if (itemVariant.type() == QVariant::String)
	{
		if (itemVariant.toString() == "separator")
		{
			return std::make_shared<QSeparatorDesc>();
		}
	}

	QVariantMap itemMap = itemVariant.toMap();
	if (itemMap["type"].toInt() == QItemDesc::Command)
	{
		std::shared_ptr<QCommandDesc> pCommand = std::make_shared<QCommandDesc>(itemMap, version);
		pCommand->commandChangedSignal.Connect(this, &QToolBarDesc::OnCommandChanged);
		return pCommand;
	}
	else if (itemMap["type"].toInt() == QItemDesc::CVar)
	{
		std::shared_ptr<QCVarDesc> pCVar = std::make_shared<QCVarDesc>(itemMap, version);
		pCVar->cvarChangedSignal.Connect(this, &QToolBarDesc::OnCommandChanged);
		return pCVar;
	}

	return nullptr;
}

QVariant CToolBarService::QToolBarDesc::ToVariant() const
{
	QVariantList commandsVarList;
	for (std::shared_ptr<QItemDesc> pItem : items)
	{
		commandsVarList.push_back(pItem->ToVariant());
	}

	return commandsVarList;
}

int CToolBarService::QToolBarDesc::IndexOfItem(std::shared_ptr<QItemDesc> pItem)
{
	return items.indexOf(pItem);
}

void CToolBarService::QToolBarDesc::MoveItem(int currIdx, int idx)
{
	if (idx == -1) // Move to the end
		idx = items.size() - 1;

	std::shared_ptr<QItemDesc> pItem = items.takeAt(currIdx);

	if (currIdx < idx)
		--idx;

	items.insert(idx, pItem);
}

int CToolBarService::QToolBarDesc::IndexOfCommand(const CCommand* pCommand)
{
	for (auto i = 0; i < items.size(); ++i)
	{
		const std::shared_ptr<QItemDesc> pItem = items[i];
		if (pItem->GetType() != CToolBarService::QItemDesc::Command)
			continue;

		const std::shared_ptr<QCommandDesc> pCommandDesc = std::static_pointer_cast<QCommandDesc>(pItem);
		if (pCommandDesc->GetCommand() == QtUtil::ToQString(pCommand->GetCommandString()))
			return i;
	}

	return -1;
}

void CToolBarService::QToolBarDesc::InsertItem(const QVariant& itemVariant, int idx)
{
	InsertItem(CreateItem(itemVariant, Private_ToolbarManager::s_version), idx);
}

void CToolBarService::QToolBarDesc::InsertItem(std::shared_ptr<QItemDesc> pItem, int idx)
{
	if (idx < 0)
	{
		items.push_back(pItem);
		return;
	}

	items.insert(idx, pItem);
}

void CToolBarService::QToolBarDesc::InsertCommand(const CCommand* pCommand, int idx)
{
	int currIdx = IndexOfCommand(pCommand);

	// If the item is already in the toolbar than ignore
	if (currIdx != -1)
		return;

	std::shared_ptr<QCommandDesc> pCommandDesc = std::make_shared<QCommandDesc>(pCommand);
	InsertItem(pCommandDesc, idx);
	pCommandDesc->commandChangedSignal.Connect(this, &QToolBarDesc::OnCommandChanged);
}

void CToolBarService::QToolBarDesc::InsertCVar(const QString& cvarName, int idx)
{
	std::shared_ptr<QCVarDesc> pCVar = std::make_shared<QCVarDesc>();
	pCVar->cvarChangedSignal.Connect(this, &QToolBarDesc::OnCommandChanged);
	InsertItem(pCVar, idx);
	return;
}

void CToolBarService::QToolBarDesc::InsertSeparator(int idx)
{
	InsertItem(std::make_shared<QSeparatorDesc>(), idx);
}

void CToolBarService::QToolBarDesc::RemoveItem(std::shared_ptr<QItemDesc> pItem)
{
	int idx = items.indexOf(pItem);
	if (idx < 0)
		return;

	items.remove(idx);
}

void CToolBarService::QToolBarDesc::RemoveItem(int idx)
{
	items.remove(idx);
}

QString CToolBarService::QToolBarDesc::GetNameFromFileInfo(const QFileInfo& fileInfo)
{
	return fileInfo.baseName();
}

void CToolBarService::QToolBarDesc::SetName(const QString& name)
{
	// First, check path and fix if necessary
	int index = m_path.lastIndexOf(m_name);
	if (index > -1)
	{
		m_path = m_path.replace(index, m_name.length(), name);
	}

	m_name = name;
}

bool CToolBarService::QToolBarDesc::RequiresUpdate() const
{
	if (updated)
	{
		return false;
	}

	for (auto i = 0; i < items.size(); ++i)
	{
		const std::shared_ptr<QItemDesc> pItem = items[i];
		if (pItem->GetType() != CToolBarService::QItemDesc::Command)
			continue;

		const std::shared_ptr<QCommandDesc> pCommandDesc = std::static_pointer_cast<QCommandDesc>(pItem);
		if (pCommandDesc->IsDeprecated())
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

CToolBarService::CToolBarService()
	: CUserData({ Private_ToolbarManager::szUserToolbarsPath })
	, m_pCVarActionMapper(new Private_ToolbarManager::CVarActionMapper())
{
}

CToolBarService::~CToolBarService()
{
	m_pCVarActionMapper->deleteLater();
}

void CToolBarService::MigrateToolBars(const char* szSourceDirectory, const char* szDestinationDirectory) const
{
	std::map<string, std::shared_ptr<QToolBarDesc>> toolBarDescriptors;
	std::vector<string> sourceDirectories = GetToolBarDirectories(szSourceDirectory);
	std::vector<string> destinationDirectories = GetToolBarDirectories(szDestinationDirectory);

	auto directoryCount = sourceDirectories.size();
	for (auto i = 0; i < directoryCount; ++i)
	{
		FileUtils::MoveFiles(sourceDirectories[i].c_str(), destinationDirectories[i].c_str());
	}
}

std::shared_ptr<CToolBarService::QToolBarDesc> CToolBarService::CreateToolBarDesc(const CEditor* pEditor, const char* szName) const
{
	return CreateToolBarDesc(pEditor->GetEditorName(), szName);
}

std::shared_ptr<CToolBarService::QToolBarDesc> CToolBarService::CreateToolBarDesc(const char* szEditorName, const char* szToolBarName) const
{
	std::shared_ptr<QToolBarDesc> pToolBarDesc = std::make_shared<QToolBarDesc>();
	pToolBarDesc->SetName(szToolBarName);
	pToolBarDesc->SetPath(PathUtil::Make(szEditorName, szToolBarName, ".json").c_str());

	return pToolBarDesc;
}

bool CToolBarService::SaveToolBar(const std::shared_ptr<QToolBarDesc>& pToolBarDesc) const
{
	QVariantMap toDisk;
	toDisk["version"] = Private_ToolbarManager::s_version;
	toDisk["toolBar"] = pToolBarDesc->ToVariant();

	QJsonDocument doc(QJsonDocument::fromVariant(toDisk));

	string toolBarPath = QtUtil::ToString(pToolBarDesc->GetPath());
	return UserDataUtil::Save(PathUtil::Make(Private_ToolbarManager::szUserToolbarsPath, toolBarPath.c_str()).c_str(), doc.toJson());
}

void CToolBarService::RemoveToolBar(const std::shared_ptr<QToolBarDesc>& pToolBarDesc) const
{
	const string name = QtUtil::ToString(pToolBarDesc->GetPath());
	const string fullPath = UserDataUtil::GetUserPath(PathUtil::Make(Private_ToolbarManager::szUserToolbarsPath, name.c_str(), ".json").c_str());
	FileUtils::Remove(fullPath.c_str());
}

std::set<string> CToolBarService::GetToolBarNames(const CEditor* pEditor) const
{
	return GetToolBarNames(pEditor->GetEditorName());
}

std::shared_ptr<CToolBarService::QToolBarDesc> CToolBarService::GetToolBarDesc(const CEditor* pEditor, const char* szName) const
{
	// Need editor or editorName
	return GetToolBarDesc(PathUtil::Make(pEditor->GetEditorName(), szName, ".json"));
}

std::shared_ptr<CToolBarService::QToolBarDesc> CToolBarService::GetToolBarDesc(const char* szRelativePath) const
{
	std::vector<string> toolBarDirectories = GetToolBarDirectories(szRelativePath);
	for (const string& toolBarDir : toolBarDirectories)
	{
		std::shared_ptr<QToolBarDesc> pToolBarDesc = LoadToolBar(toolBarDir);

		if (!pToolBarDesc)
			continue;

		return pToolBarDesc;
	}

	string message;
	message.Format("Failed to load tool bar descriptor: %s", szRelativePath);
	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, message.c_str());

	return nullptr;
}

std::vector<QToolBar*> CToolBarService::LoadToolBars(const CEditor* pEditor) const
{
	return LoadToolBars(pEditor->GetEditorName(), pEditor);
}

std::vector<string> CToolBarService::GetToolBarDirectories(const char* szRelativePath) const
{
	std::vector<string> result;
	string editorToolbarPath = PathUtil::Make(Private_ToolbarManager::s_defaultPath, szRelativePath);

	// User tool-bars
	result.push_back(PathUtil::Make(UserDataUtil::GetUserPath(Private_ToolbarManager::szUserToolbarsPath).c_str(), szRelativePath));
	// Game project specific tool-bars
	result.push_back(PathUtil::Make(GetIEditor()->GetProjectManager()->GetCurrentProjectDirectoryAbsolute(), editorToolbarPath));
	// Engine defaults
	result.push_back(PathUtil::Make(PathUtil::GetEnginePath().c_str(), editorToolbarPath));

	return result;
}

std::vector<QToolBar*> CToolBarService::LoadToolBars(const char* szRelativePath, const CEditor* pEditor /* = nullptr*/) const
{
	std::map<string, std::shared_ptr<QToolBarDesc>> toolBarDescriptors;
	std::vector<string> toolBarDirectories = GetToolBarDirectories(szRelativePath);

	for (const string& toolBarDir : toolBarDirectories)
	{
		LoadToolBarsFromDir(toolBarDir, toolBarDescriptors);
	}

	return CreateEditorToolBars(toolBarDescriptors, pEditor);
}

std::set<string> CToolBarService::GetToolBarNames(const char* szRelativePath) const
{
	std::set<string> result;
	std::vector<string> toolBarDirectories = GetToolBarDirectories(szRelativePath);

	for (const string& toolBarDir : toolBarDirectories)
	{
		GetToolBarNamesFromDir(toolBarDir, result);
	}

	return result;
}

std::vector<QToolBar*> CToolBarService::CreateEditorToolBars(const std::map<string, std::shared_ptr<QToolBarDesc>>& toolBarDescriptors, const CEditor* pEditor /* = nullptr*/) const
{
	std::vector<QToolBar*> toolBars;
	for (const std::pair<string, std::shared_ptr<QToolBarDesc>>& toolBarPair : toolBarDescriptors)
	{
		toolBars.push_back(CreateEditorToolBar(toolBarPair.second, pEditor));
	}

	return toolBars;
}

QToolBar* CToolBarService::CreateEditorToolBar(const std::shared_ptr<QToolBarDesc>& pToolBarDesc, const CEditor* pEditor /* = nullptr*/) const
{
	// Create the toolbar if the mainframe doesn't have one already
	QToolBar* pToolBar = new QToolBar();
	pToolBar->setWindowTitle(pToolBarDesc->GetName());
	pToolBar->setObjectName(pToolBarDesc->GetObjectName());

	CreateToolBar(pToolBarDesc, pToolBar, pEditor);

	// If this toolbar has been modified, then save it to disk
	if (pToolBarDesc->RequiresUpdate())
	{
		SaveToolBar(pToolBarDesc);
		pToolBarDesc->MarkAsUpdated();
	}

	return pToolBar;
}

void CToolBarService::FindToolBarsInDirAndExecute(const string& dirPath, std::function<void(const QFileInfo& fileInfo)> callback) const
{
	QDir dir(dirPath.c_str());
	if (!dir.exists())
		return;

	QFileInfoList infoList = dir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

	for (QFileInfo& fileInfo : infoList)
	{
		if (fileInfo.isDir())
		{
			// Currently unsupported, just ignore and continue
			continue;
		}

		callback(fileInfo);
	}
}

std::shared_ptr<CToolBarService::QToolBarDesc> CToolBarService::LoadToolBar(const string& absolutePath) const
{
	QFileInfo fileInfo(absolutePath.c_str());
	QFile file(absolutePath.c_str());
	if (!file.open(QIODevice::ReadOnly))
	{
		return nullptr;
	}

	QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
	QVariantMap variantMap = doc.toVariant().toMap();
	int loadedVersion = variantMap["version"].toInt();

	const string dirName = QtUtil::ToString(fileInfo.absoluteDir().dirName());
	const string fileName = QtUtil::ToString(fileInfo.fileName());
	const string toolBarRelativePath = PathUtil::Make(dirName.c_str(), fileName.c_str());

	if (loadedVersion == 1)
	{
		QVariantMap toolBarVariant = variantMap["toolBar"].toMap();
		std::shared_ptr<QToolBarDesc> pToolBarDesc = std::make_shared<QToolBarDesc>();
		pToolBarDesc->Initialize(toolBarVariant["Commands"].toList(), loadedVersion);
		pToolBarDesc->SetName(QToolBarDesc::GetNameFromFileInfo(fileInfo));
		pToolBarDesc->SetPath(toolBarRelativePath.c_str());
		return pToolBarDesc;
	}

	QVariantList toolBarVariant = variantMap["toolBar"].toList();
	std::shared_ptr<QToolBarDesc> pToolBarDesc = std::make_shared<QToolBarDesc>();
	pToolBarDesc->Initialize(toolBarVariant, loadedVersion);
	pToolBarDesc->SetName(QToolBarDesc::GetNameFromFileInfo(fileInfo));
	pToolBarDesc->SetPath(toolBarRelativePath.c_str());

	return pToolBarDesc;
}

void CToolBarService::LoadToolBarsFromDir(const string& dirPath, std::map<string, std::shared_ptr<QToolBarDesc>>& outToolBarDescriptors) const
{
	FindToolBarsInDirAndExecute(dirPath, [this, &outToolBarDescriptors](const QFileInfo& fileInfo)
	{
		// If toolbar has already been loaded by a higher prio location, then just return. Loading order should be as follows:
		// User created/modified -> Project defaults -> Engine defaults
		string toolBarName = QtUtil::ToString(QToolBarDesc::GetNameFromFileInfo(fileInfo));
		auto ite = outToolBarDescriptors.find(toolBarName);
		if (ite != outToolBarDescriptors.end())
			return;

		std::shared_ptr<QToolBarDesc> pToolBarDesc = LoadToolBar(QtUtil::ToString(fileInfo.absoluteFilePath()));
		if (!pToolBarDesc)
			return;

		outToolBarDescriptors.insert({ toolBarName, pToolBarDesc });
	});
}

void CToolBarService::GetToolBarNamesFromDir(const string& dirPath, std::set<string>& outResult) const
{
	FindToolBarsInDirAndExecute(dirPath, [&outResult](const QFileInfo& fileInfo)
	{
		outResult.insert(QtUtil::ToString(fileInfo.baseName()));
	});
}

void CToolBarService::CreateToolBar(const std::shared_ptr<QToolBarDesc>& pToolBarDesc, QToolBar* pToolBar, const CEditor* pEditor /* = nullptr*/) const
{
	for (std::shared_ptr<QItemDesc> pItemDesc : pToolBarDesc->GetItems())
	{
		switch (pItemDesc->GetType())
		{
		case QItemDesc::Command:
			{
				std::shared_ptr<QCommandDesc> pCommandDesc = std::static_pointer_cast<QCommandDesc>(pItemDesc);
				QCommandAction* pAction = nullptr;
				if (pEditor)
				{
					pAction = pEditor->GetAction(QtUtil::ToString(pCommandDesc->GetCommand()));
					if (!pAction)
					{
						pAction = pEditor->GetAction_Deprecated(QtUtil::ToString(pCommandDesc->GetCommand()));
					}
				}
				else
				{
					pAction = pCommandDesc->ToQCommandAction();
				}

				if (!pAction)
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to create tool bar action for command: %s", pCommandDesc->GetCommand().toStdString().c_str());
					continue;
				}

				const QString& iconPath = pCommandDesc->GetIcon();
				if (!iconPath.isEmpty())
				{
					pAction->UiInfo::icon = QtUtil::ToString(iconPath);
					pAction->setIcon(CryIcon(iconPath));
				}
				pToolBar->addAction(pAction);
				break;
			}
		case QItemDesc::CVar:
			{
				pToolBar->addAction(m_pCVarActionMapper->AddCVar(std::static_pointer_cast<QCVarDesc>(pItemDesc)));
				break;
			}
		case QItemDesc::Separator:
			pToolBar->addSeparator();
			break;
		default:
			break;
		}
	}
}
