// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Rename ScriptBrowser!

#pragma once

#include <queue>

#include <QAbstractItemModel>
#include <QTreeView>
#include <QWidget>
#include <CrySchematyc2/Script/IScriptRegistry.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

class QBoxLayout;
class QItemSelection;
class QLineEdit;
class QMenu;
class QParentWndWidget;
class QPushButton;
class QSplitter;

namespace Schematyc2
{
	class CBrowserFilter;

	namespace BrowserUtils
	{
		class CFileChangeListener;
	}

	class CBrowserRules
	{
	public:

		bool CanAddScriptElement(EScriptElementType scriptElementType, const SGUID& scopeGUID) const;
		const char* GetScriptElementFilter(EScriptElementType scriptElementType) const;
	};

	struct EBrowserColumn
	{
		enum : int
		{
			Name = 0,
			Status,
			Count
		};
	};

	enum class EBrowserItemType
	{
		Root = 0,
		Filter,
		ScriptElement
	};

	enum class EBrowserItemStatusFlags // #SchematycTODO : Rename EBrowserItemFlags?
	{
		None             = 0,

		Local            = BIT(0),
		InPak            = BIT(1),
		ReadOnly         = BIT(2),
		SourceControl    = BIT(3),
		CheckedOut       = BIT(4),
		
		ContainsWarnings = BIT(5),
		ContainsErrors   = BIT(6),

		FileMask         = Local | InPak | ReadOnly | SourceControl | CheckedOut,
		ValidationMask   = ContainsWarnings | ContainsErrors
	};

	DECLARE_ENUM_CLASS_FLAGS(EBrowserItemStatusFlags)

	class CBrowserModel : public QAbstractItemModel
	{
		Q_OBJECT

	public:

		class CItem;

		DECLARE_SHARED_POINTERS(CItem)

		typedef std::vector<CItemPtr> Items; // #SchematycTODO : Do we really need to use shared pointers?

		class CItem
		{
		public:

			CItem(EBrowserItemType type, const char* szName, const char* szIcon = nullptr);
			CItem(IScriptElement* pScriptElement, const char* szName, const char* szIcon);

			EBrowserItemType GetType() const;

			void SetName(const char* szName);
			const char* GetName() const;
			const char* GetPath() const;
			const char* GetIcon() const;

			void SetStatusFlags(EBrowserItemStatusFlags statusFlags);
			EBrowserItemStatusFlags GetStatusFlags() const;
			const char* GetStatusText() const;
			const char* GetStatusIcon() const;
			void SetStatusToolTip(const char* szStatusToolTip);
			const char* GetStatusToolTip() const;
			QVariant GetColor() const;

			IScriptElement* GetScriptElement() const;

			CItem* GetParent();
			void AddChild(const CItemPtr& pChild);
			void RemoveChild(CItem* pChild);
			int GetChildCount() const;
			int GetChildIdx(CItem* pChild);
			CItem* GetChildByIdx(int childIdx);
			CItem* GetChildByTypeAndName(EBrowserItemType type, const char* szName);
			CItem* GetChildByPath(const char* szPath);
			void SortChildren();

			void Validate();
			void OnChildValidate(EBrowserItemStatusFlags statusFlags);

		private:

			EBrowserItemType        m_type;
			string                  m_name;
			string                  m_path;
			string                  m_icon;
			EBrowserItemStatusFlags m_statusFlags;
			string                  m_statusToolTip;
			IScriptElement*         m_pScriptElement;
			CItem*                  m_pParent;
			Items                   m_children;
		};

		typedef std::unordered_map<SGUID, CItem*> ScriptElementItems;

	private:

		enum class ETaskStatus
		{
			Done,
			Repeat
		};

		struct ITask
		{
			virtual ~ITask() {}

			// #SchematycTODO : Add some kind of delay system?

			virtual ETaskStatus Execute(CBrowserModel& model) = 0;
		};

		DECLARE_SHARED_POINTERS(ITask)

		typedef std::queue<ITaskPtr> Tasks;

		class CSourceControlRefreshTask : public ITask
		{
		public:

			CSourceControlRefreshTask(const SGUID& guid);

			// ITask
			ETaskStatus Execute(CBrowserModel& model) override;
			// ~ITask

		public:

			SGUID m_guid;
		};

	public:

		CBrowserModel(QObject* pParent);

		// QAbstractItemModel
		QModelIndex index(int row, int column, const QModelIndex& parent) const override;
		QModelIndex parent(const QModelIndex& index) const override;
		int rowCount(const QModelIndex& index) const override;
		int columnCount(const QModelIndex& parent) const override;
		bool hasChildren(const QModelIndex &parent) const override;
		QVariant data(const QModelIndex& index, int role) const override;
		bool setData(const QModelIndex &index, const QVariant &value, int role) override;
		QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
		Qt::ItemFlags flags(const QModelIndex& index ) const override;
		// ~QAbstractItemModel

		void ProcessTasks();
		void Reset();

		QModelIndex ItemToIndex(CItem* pItem, int column = 0) const;
		CItem* ItemFromIndex(const QModelIndex& index) const;
		CItem* GetScriptElementItem(const SGUID& guid);

	public slots:

	private:

		void Populate();
		void OnScriptRegistryChange(EScriptRegistryChange change, IScriptElement* pScriptElement);
		void OnScriptElementAdded(IScriptElement& scriptElement);
		void OnScriptElementRemoved(IScriptElement& scriptElement);
		CItemPtr CreateScriptElementItem(IScriptElement& scriptElement, CItem* pParentItem);
		void ScheduleTask(const ITaskPtr& pTask);

	private:

		CBrowserRules                   m_rules;
		CItemPtr                        m_pRootItem;
		ScriptElementItems              m_scriptElementItems;
		Tasks                           m_tasks;
		TemplateUtils::CConnectionScope m_connectionScope;
	};

	class CBrowserTreeView : public QTreeView
	{
		Q_OBJECT

	public:

		CBrowserTreeView(QWidget* pParent);

	signals:

		void keyPress(QKeyEvent* pKeyEvent, bool& bEventHandled);

	protected:

		virtual void keyPressEvent(QKeyEvent* pKeyEvent) override;
	};

	typedef TemplateUtils::CSignalv2<void (IScriptElement*)> BrowserSelectionSignal;

	struct SBrowserSignals
	{
		BrowserSelectionSignal selection;
	};

	class CBrowserWidget : public QWidget
	{
		Q_OBJECT

	public:

		CBrowserWidget(QWidget* pParent);

		~CBrowserWidget();

		void InitLayout();
		void Reset();
		
		SBrowserSignals& Signals();

	public slots:

		void OnBackButtonClicked();
		void OnSearchFilterChanged(const QString& text);
		void OnTreeViewDoubleClicked(const QModelIndex& index);
		void OnTreeViewCustomContextMenuRequested(const QPoint& position);
		void OnTreeViewKeyPress(QKeyEvent* pKeyEvent, bool& bEventHandled);
		void OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
		void OnAddItem();
		void OnRenameItem();
		void OnRemoveItem();
		void OnSave();
		void OnAddToSourceControl();
		void OnGetLatestRevision();
		void OnCheckOut();
		void OnRevert();
		void OnDiffAgainstLatestRevision();
		void OnShowInExplorer();
		void OnExtract();

	protected:

		virtual void keyPressEvent(QKeyEvent* pKeyEvent) override;
		virtual void timerEvent(QTimerEvent* pTimerEvent) override;

	private:

		void ExpandAll();
		void PopulateFilterMenu(QMenu& menu);
		void PopulateAddMenu(QMenu& menu);
		void SetFocus(const QModelIndex& index);
		bool HandleKeyPress(QKeyEvent* pKeyEvent);

		QModelIndex GetTreeViewSelection() const;
		QModelIndex TreeViewToModelIndex(const QModelIndex& index) const;
		QModelIndex TreeViewFromModelIndex(const QModelIndex& index) const;

	private:

		QBoxLayout*       m_pMainLayout;
		QBoxLayout*       m_pFilterLayout;
		QBoxLayout*       m_pControlLayout;
		QMenu*            m_pFilterButtonMenu;
		QPushButton*      m_pFilterButton;
		QLineEdit*        m_pSearchFilter;
		QPushButton*      m_pBackButton;
		QMenu*            m_pAddButtonMenu;
		QPushButton*      m_pAddButton;
		CBrowserTreeView* m_pTreeView;
		CBrowserModel*    m_pModel;
		CBrowserFilter*   m_pFilter;
		SBrowserSignals   m_signals;
	};

	class CBrowserWnd : public CWnd
	{
		DECLARE_MESSAGE_MAP()

	public:

		CBrowserWnd();

		~CBrowserWnd();

		void Init();
		void InitLayout();
		void Reset();

		SBrowserSignals& Signals();

	private:

		afx_msg void OnSize(UINT nType, int cx, int cy);

		QParentWndWidget* m_pParentWndWidget;
		CBrowserWidget*   m_pBrowserWidget;
		QBoxLayout*       m_pLayout;
	};
}
