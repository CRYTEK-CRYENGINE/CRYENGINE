// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Rename Schematyc_DocScriptGraphView.h!
// #SchematycTODO : Reduce number of calls to IDocGraph::RefreshAvailableNodes?

#pragma once

#include <QWidget>
#include <CrySerialization/Forward.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCreationMenu.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include "PluginUtils.h"
#include "GraphView.h"

class QBoxLayout;
class QSplitter;
class QPushButton;
class QPropertyTree;
class QParentWndWidget;
class QWinHost;

namespace Schematyc2
{
	class CDocGraphViewNode : public CGraphViewNode
	{
	public:

		CDocGraphViewNode(const SGraphViewGrid& grid, Gdiplus::PointF pos, TScriptFile& scriptFile, IScriptGraph& scriptGraph, IScriptGraphNode& scriptGraphNode);

		// CGraphViewNode
		virtual const char* GetName() const override;
		virtual const char* GetContents() const override;
		virtual Gdiplus::Color GetHeaderColor() const override;
		virtual size_t GetInputCount() const override;
		virtual size_t FindInput(const char* name) const override;
		virtual const char* GetInputName(size_t iInput) const override;
		virtual EScriptGraphPortFlags GetInputFlags(size_t iInput) const override;
		virtual Gdiplus::Color GetInputColor(size_t iInput) const override;
		virtual size_t GetOutputCount() const override;
		virtual size_t FindOutput(const char* name) const override;
		virtual const char* GetOutputName(size_t iOutput) const override;
		virtual EScriptGraphPortFlags GetOutputFlags(size_t iOutput) const override;
		virtual Gdiplus::Color GetOutputColor(size_t iInput) const override;
		// ~CGraphViewNode

		IScriptGraphNode& GetScriptGraphNode();
		EScriptGraphNodeType GetType() const;
		SGUID GetGUID() const;
		SGUID GetRefGUID() const;
		SGUID GetGraphGUID() const;
		CAggregateTypeId GetInputTypeId(size_t inputIdx) const;
		CAggregateTypeId GetOutputTypeId(size_t outputIdx) const;
		void Serialize(Serialization::IArchive& archive);
		const TScriptFile& GetScriptFile() const { return m_scriptFile; }

	protected:

		virtual void OnMove(Gdiplus::RectF paintRect);

	private:

		TScriptFile& m_scriptFile;
		IScriptGraph&              m_scriptGraph;
		IScriptGraphNode&          m_scriptGraphNode;
	};

	DECLARE_SHARED_POINTERS(CDocGraphViewNode)

	struct SDocGraphViewSelection
	{
		SDocGraphViewSelection(const CDocGraphViewNodePtr& _pNode, SGraphViewLink* _pLink);

		CDocGraphViewNodePtr pNode;
		SGraphViewLink*      pLink;
	};

	struct SGotoSelection
	{
		SGotoSelection(const SGUID& gotoNode, const SGUID& gobackNode)
			: gotoNode(gotoNode)
			, gobackNode(gobackNode)
		{}

		SGUID gotoNode;
		SGUID gobackNode;
	};

	typedef TemplateUtils::CSignalv2<void (const SDocGraphViewSelection&)> DocGraphViewSelectionSignal;
	typedef TemplateUtils::CSignalv2<void (TScriptFile&)>    DocGraphViewModificationSignal;
	typedef TemplateUtils::CSignalv2<void()>                               DocGotoSelectionSignal;
	typedef TemplateUtils::CSignalv2<void(const SGotoSelection&)>          DocGotoSelectionChangedSignal;

	struct SDocGraphViewSignals
	{
		DocGraphViewSelectionSignal    selection;
		DocGraphViewModificationSignal modification;
		DocGotoSelectionSignal         gotoSelection;
		DocGotoSelectionSignal         goBackFromSelection;
		DocGotoSelectionChangedSignal  gotoSelectionChanged;
	};

	class CDocGraphView : public CGraphView
	{
		DECLARE_MESSAGE_MAP()

	public:
		
		CDocGraphView();

		// CGraphView
		virtual void Refresh() override;
		// ~CGraphView

		void Load(TScriptFile* pScriptFIle, IScriptGraph* pScriptGraph);
		TScriptFile* GetScriptFile() const;
		IScriptGraph* GetScriptGraph() const;
		void SelectAndFocusNode(const SGUID& nodeGuid);
		SDocGraphViewSignals& Signals();

	protected:

		// CGraphView
		virtual void OnMove(Gdiplus::PointF scrollOffset) override;
		virtual void OnSelection(const CGraphViewNodePtr& pSelectedNode, SGraphViewLink* pSelectedLink) override;
		virtual void OnUpArrowPressed() override;
		virtual void OnDownArrowPressed() override;
		virtual void OnLinkModify(const SGraphViewLink& link) override;
		virtual void OnNodeRemoved(const CGraphViewNode& node);
		virtual bool CanCreateLink(const CGraphViewNode& srcNode, const char* szScOutputName, const CGraphViewNode& dstNode, const char* szDstInputName) const override;
		virtual void OnLinkCreated(const SGraphViewLink& link) override;
		virtual void OnLinkRemoved(const SGraphViewLink& link) override;
		virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point) override;
		virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point) override;
		void GetPopupMenuItems(CMenu& popupMenu, const CGraphViewNodePtr& pNode, size_t nodeInputIdx, size_t nodeOutputIdx, CPoint point) override;
		virtual void OnPopupMenuResult(BOOL popupMenuItem, const CGraphViewNodePtr& pNode, size_t nodeInputIdx, size_t nodeOutputIdx, CPoint point) override;
		virtual const IQuickSearchOptions* GetQuickSearchOptions(CPoint point, const CGraphViewNodePtr& pNode, size_t nodeOutputIdx) override;
		virtual void OnQuickSearchResult(CPoint point, const CGraphViewNodePtr& pNode, size_t nodeOutputIdx, size_t optionIdx) override;
		// ~CGraphView

	private:

		struct EPopupMenuItemEx
		{
			enum
			{
				ADD_OUTPUT = EPopupMenuItem::CUSTOM,
				REMOVE_OUTPUT,
				REFERENCES,
				DEFINITION
			};
		};

		class CQuickSearchOptions : public IQuickSearchOptions, public IScriptGraphNodeCreationMenu
		{
		public:

			// IQuickSearchOptions
			virtual const char* GetToken() const override;
			virtual size_t GetCount() const override;
			virtual const char* GetLabel(size_t optionIdx) const override;
			virtual const char* GetDescription(size_t optionIdx) const override;
			virtual const char* GetWikiLink(size_t optionIdx) const override;
			// IQuickSearchOptions

			// IScriptGraphNodeCreationMenuCommand
			virtual bool AddOption(const char* szLabel, const char* szDescription, const char* szWikiLink, const IScriptGraphNodeCreationMenuCommandPtr& pCommand) override;
			// ~IScriptGraphNodeCreationMenuCommand

			void Refresh(IScriptGraph& scriptGraph, const CAggregateTypeId& inputTypeId);
			EScriptGraphNodeType GetNodeType(size_t optionIdx) const;
			SGUID GetNodeContextGUID(size_t optionIdx) const;
			SGUID GetNodeRefGUID(size_t optionIdx) const;
			IScriptGraphNodePtr ExecuteCommand(size_t optionIdx, const Vec2& pos);

		private:

			struct SOption
			{
				SOption(const char* _szLabel, EScriptGraphNodeType _nodeType, const SGUID& _nodeContextGUID, const SGUID& _nodeRefGUID);
				SOption(const char* _szLabel, const char* _szDescription, const char* _szWikiLink, const IScriptGraphNodeCreationMenuCommandPtr& _pCommand);

				string                                 label;
				string                                 description;
				string                                 wikiLink;
				EScriptGraphNodeType                   nodeType;
				SGUID                                  nodeContextGUID;
				SGUID                                  nodeRefGUID;
				IScriptGraphNodeCreationMenuCommandPtr pCommand;
			};

			typedef std::vector<SOption> Options;

			Options m_options;
		};

		typedef std::vector<SGUID> SequenceNodes;

		EVisitStatus VisitGraphNode(IScriptGraphNode& graphNode);
		
		bool CanAddNode(EScriptGraphNodeType type) const;
		bool CanAddNode(const SGUID& refGUID) const;
		bool CanAddNode(EScriptGraphNodeType type, const SGUID& refGUID) const;
		CDocGraphViewNodePtr AddNode(EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Gdiplus::PointF pos);
		CDocGraphViewNodePtr AddNode(IScriptGraphNode& scriptGraphNode);
		CDocGraphViewNodePtr GetNode(const SGUID& guid) const;
		bool AddLink(const IScriptGraphLink& scriptGraphLink);

		void InvalidateDoc();
		void UnrollSequence();
		void OnDocLinkRemoved(const IScriptGraphLink& link);

		TScriptFile*                    m_pScriptFile; // #SchematycTODO : Do we really need to store this now that we can access the document from the script graph?
		IScriptGraph*                   m_pScriptGraph;
		bool                            m_bRemovingLinks; // #SchematycTODO : This is a little hacky and we need to define a clear chain of communication between doc and doc view.
		SequenceNodes                   m_sequenceNodes;
		CQuickSearchOptions             m_quickSearchOptions;
		TemplateUtils::CConnectionScope m_connectionScope;
		SDocGraphViewSignals            m_signals;
	};

	class CGraphSettingsWidget : public QWidget
	{
		Q_OBJECT

	public:

		CGraphSettingsWidget(QWidget* pParent);

		void SetGraphView(CGraphView* pGraphView);
		void InitLayout();
		void Serialize(Serialization::IArchive& archive);

	private:

		CGraphView*    m_pGraphView;
		QBoxLayout*    m_pLayout;
		QPropertyTree* m_pPropertyTree;
	};

	class CDocGraphWidget : public QWidget
	{
		Q_OBJECT

	public:

		CDocGraphWidget(QWidget* pParent, CWnd* pParentWnd);

		void InitLayout();
		void LoadSettings(const XmlNodeRef& xml);
		XmlNodeRef SaveSettings(const char* szName);
		void Refresh();
		void LoadGraph(TScriptFile* pScriptFile, IScriptGraph* pScriptGraph);
		TScriptFile* GetScriptFile() const;
		IScriptGraph* GetScriptGraph() const;
		void SelectAndFocusNode(const SGUID& nodeGuid);
		SDocGraphViewSignals& Signals();

	public slots:

		void OnShowHideSettingsButtonClicked();

	private:

		QBoxLayout*           m_pMainLayout;
		QSplitter*            m_pSplitter;
		CGraphSettingsWidget* m_pSettings;
		QWinHost*             m_pGraphViewHost;
		CDocGraphView*        m_pGraphView;
		QBoxLayout*           m_pControlLayout;
		QPushButton*          m_pShowHideSettingsButton;
	};

	class CDocGraphWnd : public CWnd
	{
		DECLARE_MESSAGE_MAP()

	public:

		CDocGraphWnd();

		~CDocGraphWnd();

		void Init();
		void InitLayout();
		void LoadSettings(const XmlNodeRef& xml);
		XmlNodeRef SaveSettings(const char* szName);
		void Refresh();
		void LoadGraph(TScriptFile* pScriptFile, IScriptGraph* pScriptGraph);
		TScriptFile* GetScriptFile() const;
		IScriptGraph* GetScriptGraph() const;
		void SelectAndFocusNode(const SGUID& nodeGuid);
		SDocGraphViewSignals& Signals();

	private:

		afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
		afx_msg void OnSize(UINT nType, int cx, int cy);

		QParentWndWidget* m_pParentWndWidget;
		CDocGraphWidget*  m_pDocGraphWidget;
		QBoxLayout*       m_pLayout;
	};
}
