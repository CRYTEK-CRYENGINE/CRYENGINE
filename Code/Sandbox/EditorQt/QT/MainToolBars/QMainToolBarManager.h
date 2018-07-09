// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QObject>
#include <QVariant>
#include <CrySandbox/CrySignal.h>

class CEditorMainFrame;

class QMainToolBarManager : public QObject
{
	Q_OBJECT
public:

	class QItemDesc
	{
	public:
		enum Type
		{
			Command,
			CVar,
			Separator
		};

		virtual QVariant ToVariant() const = 0;
		virtual Type     GetType() const = 0;
	};

	class QSeparatorDesc : public QItemDesc
	{
	public:
		virtual QVariant ToVariant() const override { return "separator"; }
		virtual Type     GetType() const override   { return Separator; }
	};

	class QCommandDesc : public QItemDesc
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
		bool           IsCustom()         { return bIsCustom; }

	public:
		CCrySignal<void()> commandChangedSignal;

	private:
		QString name;
		QString command;
		QString iconPath;
		bool    bIsCustom;
	};

	class QCVarDesc : public QItemDesc
	{
	public:
		QCVarDesc() {}
		QCVarDesc(const QVariantMap& variantMap, int version);

		virtual Type     GetType() const override { return CVar; }
		virtual QVariant ToVariant() const override;

		void             SetCVar(const QString& path);
		void             SetCVarValue(const QVariant& cvarValue);
		void             SetIcon(const QString& path);

		const QString&  GetName() const  { return name; }
		const QVariant& GetValue() const { return value; }
		const QString&  GetIcon() const { return iconPath; }
		const bool      IsBitFlag() const { return isBitFlag; }
	public:
		CCrySignal<void()> cvarChangedSignal;

	private:
		QString  name;
		QString  iconPath;
		QVariant value;
		bool isBitFlag;
	};

	class QToolBarDesc
	{
	public:
		QToolBarDesc() {}
		QToolBarDesc(const QVariantList& commandList, int version);

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

		const QString&                       GetName() const           { return name; }
		void                                 SetName(const QString& n) { name = n; }
		QVector<std::shared_ptr<QItemDesc>>& GetItems()                { return items; }

		void                                 OnCommandChanged()        { toolBarChangedSignal(this); }
		CCrySignal<void(const QToolBarDesc*)> toolBarChangedSignal;

	private:
		void InsertItem(std::shared_ptr<QItemDesc> pItem, int idx);

	private:
		QString                             name;
		QVector<std::shared_ptr<QItemDesc>> items;
		QVector<int>                        separatorIndices;
	};

	QMainToolBarManager(CEditorMainFrame* pMainFrame = nullptr);
	~QMainToolBarManager();

	void                                                AddToolBar(const QString& name, const std::shared_ptr<QToolBarDesc> );
	void                                                UpdateToolBar(const std::shared_ptr<QToolBarDesc> toolBar);
	void                                                RemoveToolBar(const QString& name);

	const QMap<QString, std::shared_ptr<QToolBarDesc>>& GetToolBars() const             { return m_ToolBarsDesc; }
	std::shared_ptr<QToolBarDesc>                       GetToolBar(const QString& name) { return m_ToolBarsDesc[name]; }

	QVariantMap                                         ToVariant(const CCommand* pCommand) const;
	void                                                CreateMainFrameToolBars();
	void                                                CreateMainFrameToolBar(const QString& name, const std::shared_ptr<QToolBarDesc> toolBarDesc);
	void                                                CreateToolBar(const std::shared_ptr<QToolBarDesc> toolBarDesc, QToolBar* pToolBar);

protected:
	void SaveToolBar(const QString& name) const;
	void LoadAll();
	void LoadToolBarsFromDir(const QString& dirPath);

	void OnCVarActionDestroyed(ICVar* pCVar, QAction* pObject);
	void OnCVarChanged(ICVar* pCVar);

protected:
	QMap<QString, std::shared_ptr<QToolBarDesc>> m_ToolBarsDesc;
	CEditorMainFrame*                            m_pMainFrame;
	QHash<ICVar*, std::vector<QAction*>>         m_pCVarActions;
	std::vector<QMetaObject::Connection>         m_cvarActionConnections;
};

