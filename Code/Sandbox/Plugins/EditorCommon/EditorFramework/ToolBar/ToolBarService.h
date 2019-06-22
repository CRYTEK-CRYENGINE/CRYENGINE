// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "Util/UserDataUtil.h"

#include <CryCore/StlUtils.h>
#include <CrySandbox/CrySignal.h>

#include <QVariant>
#include <QVector>
#include <memory>

// EditorCommon
class CCommand;
class CEditor;
class QCommandAction;

// Qt
class QFileInfo;
class QToolBar;

// CryCommon
struct ICVar;

namespace Private_ToolbarManager
{
class CVarActionMapper;
}

class EDITOR_COMMON_API CToolBarService : public CUserData
{
public:
	class EDITOR_COMMON_API QItemDesc
	{
	public:
		enum Type
		{
			Command,
			CVar,
			Separator
		};

		virtual ~QItemDesc() {}
		virtual QVariant ToVariant() const = 0;
		virtual Type     GetType() const = 0;
	};

	class EDITOR_COMMON_API QSeparatorDesc : public QItemDesc
	{
	public:
		virtual QVariant ToVariant() const override { return "separator"; }
		virtual Type     GetType() const override   { return Separator; }
	};

	class EDITOR_COMMON_API QCommandDesc : public QItemDesc
	{
	public:
		QCommandDesc() {}
		QCommandDesc(const QVariantMap& variantMap, int version);
		QCommandDesc(const CCommand* pCommand);

		virtual QVariant ToVariant() const override;
		QCommandAction*  ToQCommandAction() const;
		virtual Type     GetType() const override { return Command; }

		void             SetName(const QString& name);
		void             SetIcon(const QString& path);

		const QString& GetName() const    { return name; }
		const QString& GetCommand() const { return command; }
		const QString& GetIcon() const    { return iconPath; }
		bool           IsCustom() const   { return m_IsCustom; }
		bool           IsDeprecated() const;

	private:
		void InitFromCommand(const CCommand* pCommand);

	public:
		CCrySignal<void()> commandChangedSignal;

	private:
		QString name;
		QString command;
		QString iconPath;
		bool    m_IsCustom     : 1;
		bool    m_IsDeprecated : 1;
	};

	class EDITOR_COMMON_API QCVarDesc : public QItemDesc
	{
	public:
		QCVarDesc() {}
		QCVarDesc(const QVariantMap& variantMap, int version);

		virtual Type     GetType() const override { return CVar; }
		virtual QVariant ToVariant() const override;

		void             SetCVar(const QString& path);
		void             SetCVarValue(const QVariant& cvarValue);
		void             SetIcon(const QString& path);

		const QString&  GetName() const   { return name; }
		const QVariant& GetValue() const  { return value; }
		const QString&  GetIcon() const   { return iconPath; }
		const bool      IsBitFlag() const { return isBitFlag; }
	public:
		CCrySignal<void()> cvarChangedSignal;

	private:
		QString  name;
		QString  iconPath;
		QVariant value;
		bool     isBitFlag;
	};

	class EDITOR_COMMON_API QToolBarDesc
	{
	public:
		static QString GetNameFromFileInfo(const QFileInfo& fileInfo);

		QToolBarDesc() : updated(false) {}

		void                                 Initialize(const QVariantList& commandList, int version);

		QVariant                             ToVariant() const;

		int                                  IndexOfItem(std::shared_ptr<QItemDesc> pItem);
		int                                  IndexOfCommand(const CCommand* pCommand);
		std::shared_ptr<QItemDesc>           GetItemDescAt(int idx) { return items[idx]; }
		std::shared_ptr<QItemDesc>           CreateItem(const QVariant& item, int version);

		void                                 MoveItem(int currIdx, int idx);
		void                                 InsertItem(const QVariant& itemVariant, int idx);
		void                                 InsertCommand(const CCommand* pCommand, int idx);
		void                                 InsertCVar(const QString& cvarName, int idx);
		void                                 InsertSeparator(int idx);

		void                                 RemoveItem(std::shared_ptr<QItemDesc> pItem);
		void                                 RemoveItem(int idx);

		const QString&                       GetName() const              { return m_name; }
		void                                 SetName(const QString& name);
		const QString&                       GetPath() const              { return m_path; }
		void                                 SetPath(const QString& path) { m_path = path; }
		QString                              GetObjectName() const        { return m_name + "ToolBar"; }

		QVector<std::shared_ptr<QItemDesc>>& GetItems()                   { return items; }

		// If a command is deprecated or replaced, the toolbar should be serialized back to disk and updated
		bool RequiresUpdate() const;
		void MarkAsUpdated()    { updated = true; }

		void OnCommandChanged() { toolBarChangedSignal(this); }
		CCrySignal<void(const QToolBarDesc*)> toolBarChangedSignal;

	private:
		void InsertItem(std::shared_ptr<QItemDesc> pItem, int idx);

	private:
		QString                             m_name;
		QString                             m_path;
		QVector<std::shared_ptr<QItemDesc>> items;
		QVector<int>                        separatorIndices;
		bool                                updated;
	};

	CToolBarService();
	~CToolBarService();

	std::shared_ptr<QToolBarDesc> CreateToolBarDesc(const CEditor* pEditor, const char* szName) const;
	bool                          SaveToolBar(const std::shared_ptr<QToolBarDesc>& pToolBarDesc) const;
	void                          RemoveToolBar(const std::shared_ptr<QToolBarDesc>& pToolBarDesc) const;

	std::set<string>              GetToolBarNames(const CEditor* pEditor) const;
	std::shared_ptr<QToolBarDesc> GetToolBarDesc(const CEditor* pEditor, const char* name) const;

	QVariantMap                   ToVariant(const CCommand* pCommand) const;
	void                          CreateToolBar(const std::shared_ptr<QToolBarDesc>& pToolBarDesc, QToolBar* pToolBar, const CEditor* pEditor = nullptr) const;

	std::vector<QToolBar*>        LoadToolBars(const CEditor* pEditor) const;

private:
	// Only needed because CEditorMainFrame is able to host toolbars. We'll need to keep it this way until we manage to unlink Level Editing from the QtMainFrame.
	friend class CEditorMainFrame;
	friend class CToolBarCustomizeDialog;
	void                          MigrateToolBars(const char* szSourceDirectory, const char* szDestinationDirectory) const;
	std::shared_ptr<QToolBarDesc> CreateToolBarDesc(const char* szEditorName, const char* szToolBarName) const;
	std::set<string>              GetToolBarNames(const char* szRelativePath) const;
	std::vector<QToolBar*>        LoadToolBars(const char* szRelativePath, const CEditor* pEditor = nullptr) const;
	std::shared_ptr<QToolBarDesc> GetToolBarDesc(const char* szRelativePath) const;
	// returns full path of directories that can host toolbars. Loading order should be as follows: User created/modified -> Project defaults -> Engine defaults
	std::vector<string>           GetToolBarDirectories(const char* szRelativePath) const;

	void                          FindToolBarsInDirAndExecute(const string& dirPath, std::function<void(const QFileInfo&)> callback) const;
	void                          GetToolBarNamesFromDir(const string& dirPath, std::set<string>& outResult) const;
	void                          LoadToolBarsFromDir(const string& dirPath, std::map<string, std::shared_ptr<QToolBarDesc>>& outToolBarDescriptors) const;
	std::shared_ptr<QToolBarDesc> LoadToolBar(const string& absolutePath) const;

	// Ideally we would pass in a CEditor* instead of a QWidget*. We'll need to keep it this way until we manage to unlink Level Editing from the QtMainFrame.
	std::vector<QToolBar*> CreateEditorToolBars(const std::map<string, std::shared_ptr<QToolBarDesc>>& toolBarDescriptors, const CEditor* pEditor = nullptr) const;
	QToolBar*              CreateEditorToolBar(const std::shared_ptr<QToolBarDesc>& toolBarDesc, const CEditor* pEditor = nullptr) const;

private:
	Private_ToolbarManager::CVarActionMapper* m_pCVarActionMapper;
};
