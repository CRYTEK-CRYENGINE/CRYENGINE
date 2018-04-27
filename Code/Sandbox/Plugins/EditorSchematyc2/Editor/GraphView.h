// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move CDefaultGraphViewPainter (along with port colors from function and graph views) to a separate file.

#pragma once

#include <CrySchematyc2/Script/IScriptGraph.h> // #SchematycTODO : Can we remove this include? Right now it's only needed for access to EScriptGraphPortFlags.

#include "QuickSearchDlg.h"

namespace Schematyc2
{
	class CGraphView;

	typedef std::vector<size_t> TSizeTVector;

	struct SGraphViewFormatSettings
	{
		SGraphViewFormatSettings();

		void Serialize(Serialization::IArchive& archive);

		float horzSpacing;
		float vertSpacing;
		bool  bSnapToGrid;
	};

	struct SGraphViewSettings
	{
		void Serialize(Serialization::IArchive& archive);

		SGraphViewFormatSettings format;
	};

	struct SGraphViewGrid // #SchematycTODO : Replace floats with int32s!!!
	{
		inline SGraphViewGrid(Gdiplus::PointF _spacing, Gdiplus::RectF _bounds)
			: spacing(_spacing)
			, bounds(_bounds)
		{}

		inline float Snap(float value, float spacing) const
		{
			const float	mod = fmodf(value, spacing);
			if(mod > (spacing * 0.5f))
			{
				return value + (spacing - mod);
			}
			else if(mod < (spacing * -0.5f))
			{
				return value - (spacing + mod);
			}
			else
			{
				return value - mod;
			}
		}

		inline Gdiplus::PointF SnapPos(Gdiplus::PointF pos) const
		{
			pos.X	= Snap(pos.X, spacing.X);
			pos.Y	= Snap(pos.Y, spacing.Y);
			return pos;
		}

		inline Gdiplus::SizeF SnapSize(Gdiplus::SizeF size) const
		{
			size.Width	= size.Width + (spacing.X - fmodf(size.Width, spacing.X));
			size.Height	= size.Height + (spacing.X - fmodf(size.Height, spacing.X));
			return size;
		}

		inline Gdiplus::RectF SnapRect(Gdiplus::RectF rect) const
		{
			Gdiplus::PointF	pos;
			rect.GetLocation(&pos);
			pos = SnapPos(pos);

			Gdiplus::SizeF	size;
			rect.GetSize(&size);
			size = SnapSize(size);

			return Gdiplus::RectF(pos, size);
		}

		Gdiplus::PointF	spacing;
		Gdiplus::RectF	bounds;
	};

	enum class EGraphViewNodeStatusFlags
	{
		None             = 0,
		ContainsWarnings = BIT(0),
		ContainsErrors   = BIT(1)
	};

	DECLARE_ENUM_CLASS_FLAGS(EGraphViewNodeStatusFlags)

	class CGraphViewNode
	{
	public:

		CGraphViewNode(const SGraphViewGrid& grid, Gdiplus::PointF pos);
		virtual ~CGraphViewNode();

		const SGraphViewGrid&	GetGrid() const;
		void SetPos(Gdiplus::PointF pos, bool bSnapToGrid);
		Gdiplus::PointF GetPos() const;
		void SetPaintRect(Gdiplus::RectF paintRect);
		Gdiplus::RectF GetPaintRect() const;
		void Enable(bool enable);
		bool IsEnabled() const;
		void Select(bool select);
		bool IsSelected() const;
		void SetStatusFlags(EGraphViewNodeStatusFlags statusFlags);
		EGraphViewNodeStatusFlags GetStatusFlags() const;
		size_t FindInput(Gdiplus::PointF pos) const;
		void SetInputPaintRect(size_t iInput, Gdiplus::RectF paintRect);
		Gdiplus::RectF GetInputPaintRect(size_t iInput) const;
		Gdiplus::PointF GetInputLinkPoint(size_t iInput) const;
		size_t FindOutput(Gdiplus::PointF pos) const;
		void SetOutputPaintRect(size_t iOutput, Gdiplus::RectF paintRect);
		Gdiplus::RectF GetOutputPaintRect(size_t iOutput) const;
		Gdiplus::PointF GetOutputLinkPoint(size_t iOutput) const;

		virtual const char* GetName() const;
		virtual const char* GetContents() const;
		virtual Gdiplus::Color GetHeaderColor() const;
		virtual size_t GetInputCount() const;
		virtual size_t FindInput(const char* name) const;
		virtual const char* GetInputName(size_t iInput) const;
		virtual EScriptGraphPortFlags GetInputFlags(size_t iInput) const;
		virtual Gdiplus::Color GetInputColor(size_t iInput) const;
		virtual size_t GetOutputCount() const;
		virtual size_t FindOutput(const char* name) const;
		virtual const char* GetOutputName(size_t iOutput) const;
		virtual EScriptGraphPortFlags GetOutputFlags(size_t iOutput) const;
		virtual Gdiplus::Color GetOutputColor(size_t iInput) const;

	protected:

		virtual void OnMove(Gdiplus::RectF paintRect);

	private:

		typedef std::vector<Gdiplus::RectF> TRectVector;

		const SGraphViewGrid&	m_grid;
		bool									m_enabled;
		bool									m_selected;
		EGraphViewNodeStatusFlags m_statusFlags;
		Gdiplus::RectF				m_paintRect;
		TRectVector						m_inputPaintRects;
		TRectVector						m_outputPaintRects;
	};

	DECLARE_SHARED_POINTERS(CGraphViewNode)

	typedef std::vector<CGraphViewNodePtr> TGraphViewNodePtrVector;

	// #SchematycTODO : Convert SGraphViewLink to a class?
	struct SGraphViewLink
	{
		SGraphViewLink(CGraphView* _pGraphView, const CGraphViewNodeWeakPtr& _pSrcNode, const char* _srcOutputName, const CGraphViewNodeWeakPtr& _pDstNode, const char* _dstInputName);

		CGraphView*						pGraphView;
		CGraphViewNodeWeakPtr	pSrcNode;
		string								srcOutputName;
		CGraphViewNodeWeakPtr	pDstNode;
		string								dstInputName;
		Gdiplus::RectF				paintRect;
		bool									selected;
	};

	typedef std::vector<SGraphViewLink> TGraphViewLinkVector;

	struct IGraphViewPainter
	{
		virtual ~IGraphViewPainter() {}

		virtual Serialization::SStruct GetSettings() = 0; // #SchematycTODO : Replace Serialization::SStruct with IAny?
		virtual Gdiplus::Color GetBackgroundColor() const = 0;
		virtual void PaintGrid(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid) const = 0;
		virtual void UpdateNodePaintRect(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid, CGraphViewNode& node) const = 0;
		virtual void PaintNode(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid, CGraphViewNode& node) const = 0;
		virtual Gdiplus::RectF PaintLink(Gdiplus::Graphics& graphics, Gdiplus::PointF startPoint, Gdiplus::PointF endPoint, Gdiplus::Color color, bool highlight, float scale) const = 0;
		virtual void PaintSelectionRect(Gdiplus::Graphics& graphics, Gdiplus::RectF rect) const = 0;
	};

	DECLARE_SHARED_POINTERS(IGraphViewPainter)

	struct IGraphViewDragState
	{
		virtual ~IGraphViewDragState() {}

		virtual void Update(CGraphView& graphView, Gdiplus::PointF pos) = 0;
		virtual void Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter) = 0;
		virtual void Exit(CGraphView& graphView) = 0;
	};

	DECLARE_SHARED_POINTERS(IGraphViewDragState)

	struct SDefaultGraphViewPainterSettings
	{
		SDefaultGraphViewPainterSettings();

		void Serialize(Serialization::IArchive& archive);

		ColorB backgroundColor;
		ColorB gridLineColors[2];
	};

	class CDefaultGraphViewPainter : public IGraphViewPainter
	{
	public:

		// IGraphViewPainter
		virtual Serialization::SStruct GetSettings() override;
		virtual Gdiplus::Color GetBackgroundColor() const override;
		virtual void PaintGrid(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid) const override;
		virtual void UpdateNodePaintRect(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid, CGraphViewNode& node) const override;
		virtual void PaintNode(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid, CGraphViewNode& node) const override;
		virtual Gdiplus::RectF PaintLink(Gdiplus::Graphics& graphics, Gdiplus::PointF startPoint, Gdiplus::PointF endPoint, Gdiplus::Color color, bool highlight, float scale) const override;
		virtual void PaintSelectionRect(Gdiplus::Graphics& graphics, Gdiplus::RectF rect) const override;
		// ~IGraphViewPainter

	private:

		Gdiplus::PointF CalculateNodeHeaderSize(Gdiplus::Graphics& graphics, const CGraphViewNode& node) const;
		Gdiplus::PointF CalculateNodeStatusSize(Gdiplus::Graphics& graphics, const CGraphViewNode& node) const;
		Gdiplus::PointF CalculateNodeContentsSize(Gdiplus::Graphics& graphics, const CGraphViewNode& node) const;
		Gdiplus::PointF CalculateNodeInputSize(Gdiplus::Graphics& graphics, const char* name) const;
		Gdiplus::PointF CalculateNodeOutputSize(Gdiplus::Graphics& graphics, const char* name) const;
		void PaintNodeBody(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const;
		void PaintNodeHeader(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const;
		void PaintNodeContents(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const;
		void PaintNodeStatus(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const;
		void PaintNodeOutline(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const;
		void UpdateNodeInputPaintRect(Gdiplus::Graphics& graphics, CGraphViewNode& node, size_t iInput, Gdiplus::PointF pos, float maxWidth) const;
		void PaintNodeInput(Gdiplus::Graphics& graphics, CGraphViewNode& node, size_t iInput) const;
		void PaintNodeInputIcon(Gdiplus::Graphics& graphics, Gdiplus::PointF pos, Gdiplus::Color color, bool active) const;
		void UpdateNodeOutputPaintRect(Gdiplus::Graphics& graphics, CGraphViewNode& node, size_t iOutput, Gdiplus::PointF pos, float maxWidth) const;
		void PaintNodeOutput(Gdiplus::Graphics& graphics, CGraphViewNode& node, size_t iOutput) const;
		void PaintNodeOutputIcon(Gdiplus::Graphics& graphics, Gdiplus::PointF pos, Gdiplus::Color color, bool active) const;

		static const float						NODE_BEVEL;
		static const BYTE							NODE_HEADER_ALPHA;
		static const BYTE							NODE_HEADER_ALPHA_DISABLED;
		static const float						NODE_HEADER_TEXT_BORDER_X;
		static const float						NODE_HEADER_TEXT_BORDER_Y;
		static const Gdiplus::Color		NODE_HEADER_TEXT_COLOR;
		static const Gdiplus::Color		NODE_ERROR_FILL_COLOR;
		static const char*						NODE_ERROR_TEXT;
		static const float						NODE_ERROR_TEXT_BORDER_X;
		static const float						NODE_ERROR_TEXT_BORDER_Y;
		static const Gdiplus::Color		NODE_ERROR_TEXT_COLOR;
		static const Gdiplus::Color		NODE_WARNING_FILL_COLOR;
		static const char*						NODE_WARNING_TEXT;
		static const float						NODE_WARNING_TEXT_BORDER_X;
		static const float						NODE_WARNING_TEXT_BORDER_Y;
		static const Gdiplus::Color		NODE_WARNING_TEXT_COLOR;
		static const Gdiplus::Color		NODE_CONTENTS_FILL_COLOR;
		static const float						NODE_CONTENTS_TEXT_BORDER_X;
		static const float						NODE_CONTENTS_TEXT_BORDER_Y;
		static const float						NODE_CONTENTS_TEXT_MAX_WIDTH;
		static const float						NODE_CONTENTS_TEXT_MAX_HEIGHT;
		static const Gdiplus::Color		NODE_CONTENTS_TEXT_COLOR;
		static const Gdiplus::Color		NODE_BODY_FILL_COLOR;
		static const Gdiplus::Color		NODE_BODY_FILL_COLOR_DISABLED;
		static const Gdiplus::Color		NODE_BODY_OUTLINE_COLOR;
		static const Gdiplus::Color		NODE_BODY_OUTLINE_COLOR_HIGHLIGHT;
		static const float						NODE_BODY_OUTLINE_WIDTH;
		static const float						NODE_INPUT_OUPUT_HORZ_SPACING;
		static const float						NODE_INPUT_OUPUT_VERT_SPACING;
		static const float						NODE_INPUT_ICON_BORDER;
		static const float						NODE_INPUT_ICON_WIDTH;
		static const float						NODE_INPUT_ICON_HEIGHT;
		static const Gdiplus::Color		NODE_INPUT_ICON_OUTLINE_COLOR;
		static const float						NODE_INPUT_ICON_OUTLINE_WIDTH;
		static const float						NODE_INPUT_NAME_BORDER;
		static const float						NODE_INPUT_NAME_WIDTH_MAX;
		static const Gdiplus::Color		NODE_INPUT_NAME_COLOR;
		static const float						NODE_OUTPUT_ICON_BORDER;
		static const float						NODE_OUTPUT_ICON_WIDTH;
		static const float						NODE_OUTPUT_ICON_HEIGHT;
		static const Gdiplus::Color		NODE_OUTPUT_ICON_OUTLINE_COLOR;
		static const float						NODE_OUTPUT_ICON_OUTLINE_WIDTH;
		static const float						NODE_OUTPUT_NAME_BORDER;
		static const float						NODE_OUTPUT_NAME_WIDTH_MAX;
		static const Gdiplus::Color		NODE_OUTPUT_NAME_COLOR;
		static const float						NODE_OUTPUT_SPACER_OFFSET;
		static const float						NODE_OUTPUT_SPACER_WIDTH;
		static const float						NODE_OUTPUT_SPACER_LINE_WIDTH;
		static const Gdiplus::Color		NODE_OUTPUT_SPACER_COLOR;
		static const float						LINK_WIDTH;
		static const float						LINK_CURVE_OFFSET;
		static const float						LINK_DODGE_X;
		static const float						LINK_DODGE_Y;
		static const float						ALPHA_HIGHLIGHT_MIN;
		static const float						ALPHA_HIGHLIGHT_MAX;
		static const float						ALPHA_HIGHLIGHT_SPEED;
		static const Gdiplus::Color		SELECTION_FILL_COLOR;
		static const Gdiplus::Color		SELECTION_OUTLINE_COLOR;

		SDefaultGraphViewPainterSettings m_settings;
	};

	class CGraphView : public CView
	{
		friend struct SGraphViewLink;

		DECLARE_MESSAGE_MAP()

	public:

		virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		virtual void Refresh();
		SGraphViewSettings& GetSettings();
		const SGraphViewGrid& GetGrid() const;
		const IGraphViewPainterPtr& GetPainter() const;
		TGraphViewLinkVector& GetLinks();
		const TGraphViewLinkVector& GetLinks() const;

		void DoQuickSearch(const char* defaultFilter = NULL, const CGraphViewNodePtr& pNode = CGraphViewNodePtr(), size_t iNodeOutput = INVALID_INDEX, const CPoint* pPoint = NULL);

	protected:

		struct EPopupMenuItem
		{
			enum
			{
				QUICK_SEARCH = 1,
				REMOVE_NODE,
				CUSTOM
			};
		};

		CGraphView(const SGraphViewGrid& grid, const IGraphViewPainterPtr& pPainter = IGraphViewPainterPtr());

		void Enable(bool enable);
		bool IsEnabled() const;
		void ClearSelection();
		void SetScrollOffset(Gdiplus::PointF scrollOffset);
		void AddNode(const CGraphViewNodePtr& pNode, bool select, bool scrollToFit);
		void RemoveNode(CGraphViewNodePtr pNode);
		CGraphViewNodePtr FindNode(Gdiplus::PointF pos);
		TGraphViewNodePtrVector& GetNodes();
		const TGraphViewNodePtrVector& GetNodes() const;
		void ClearNodeSelection();
		void SelectNode(const CGraphViewNodePtr& pNode);
		void SelectNodesInRect(const Gdiplus::RectF& rect);
		const TGraphViewNodePtrVector& GetSelectedNodes() const;
		void MoveSelectedNodes(Gdiplus::PointF clientTransform, bool bSnapToGrid);
		bool CreateLink(const CGraphViewNodePtr& pSrcNode, const char* srcOutputName, const CGraphViewNodePtr& pDstNode, const char* dstInputName);
		void AddLink(const CGraphViewNodePtr& pSrcNode, const char* srcOutputName, const CGraphViewNodePtr& pDstNode, const char* dstInputName);
		void RemoveLink(size_t iLink);
		void RemoveLinksConnectedToNode(const CGraphViewNodePtr& pNode);
		void RemoveLinksConnectedToNodeInput(const CGraphViewNodePtr& pNode, const char* inputName);
		void RemoveLinksConnectedToNodeOutput(const CGraphViewNodePtr& pNode, const char* outputName);
		void FindLinks(const CGraphViewNodePtr& pDstNode, const char* inputName, TSizeTVector& output) const;
		size_t FindLink(Gdiplus::PointF point);
		size_t FindLink(const CGraphViewNodePtr& pSrcNode, const char* srcOutputName, const CGraphViewNodePtr& pDstNode, const char* dstInputName);
		void ClearLinkSelection();
		void SelectLink(size_t iLink);
		size_t GetSelectedLink() const;
		void Clear();

		Gdiplus::PointF ClientToGraph(LONG x, LONG y) const;
		Gdiplus::PointF ClientToGraph(Gdiplus::PointF pos) const;
		Gdiplus::PointF ClientToGraphTransform(Gdiplus::PointF transform) const;
		Gdiplus::RectF ClientToGraph(Gdiplus::RectF rect) const;
		Gdiplus::PointF GraphToClient(Gdiplus::PointF pos) const;
		Gdiplus::PointF GraphToClientTransform(Gdiplus::PointF transform) const;
		Gdiplus::RectF GraphToClient(Gdiplus::RectF rect) const;
		void ScrollToFit(Gdiplus::RectF rect);
		void CenterPosition(Gdiplus::PointF point);

		virtual BOOL PreTranslateMessage(MSG* pMsg);
		virtual void OnDraw(CDC* pDC);

		afx_msg void OnKillFocus(CWnd* pNewWnd);
		afx_msg void OnPaint();
		afx_msg void OnMouseMove(UINT nFlags, CPoint point);
		afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint point);
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
		afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
		afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
		afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
		afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
		afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

		virtual void OnMove(Gdiplus::PointF scrollOffset);
		virtual void OnSelection(const CGraphViewNodePtr& pSelectedNode, SGraphViewLink* pSelectedLink);
		virtual void OnUpArrowPressed() {}
		virtual void OnDownArrowPressed() {}
		virtual void OnLinkModify(const SGraphViewLink& link);
		virtual void OnNodeRemoved(const CGraphViewNode& node);
		virtual bool CanCreateLink(const CGraphViewNode& srcNode, const char* srcOutputName, const CGraphViewNode& dstNode, const char* dstInputName) const;
		virtual void OnLinkCreated(const SGraphViewLink& link);
		virtual void OnLinkRemoved(const SGraphViewLink& link);
		virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
		virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
		virtual void GetPopupMenuItems(CMenu& popupMenu, const CGraphViewNodePtr& pNode, size_t iNodeInput, size_t iNodeOutput, CPoint point);
		virtual void OnPopupMenuResult(BOOL popupMenuItem, const CGraphViewNodePtr& pNode, size_t iNodeInput, size_t iNodeOutput, CPoint point);
		virtual const IQuickSearchOptions* GetQuickSearchOptions(CPoint point, const CGraphViewNodePtr& pNode, size_t iNodeOutput);
		virtual void OnQuickSearchResult(CPoint point, const CGraphViewNodePtr& pNode, size_t iNodeOutput, size_t iSelectedOption);
		virtual bool ShouldUpdate() const;
		virtual void OnUpdate();

	private:

		class CScrollDragState : public IGraphViewDragState
		{
		public:

			CScrollDragState(Gdiplus::PointF pos);

			// IDragState
			virtual void Update(CGraphView& graphView, Gdiplus::PointF pos);
			virtual void Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter);
			virtual void Exit(CGraphView& graphView);
			// ~IDragState

		private:

			Gdiplus::PointF	m_prevPos;
		};

		class CMoveDragState : public IGraphViewDragState
		{
		public:

			CMoveDragState(Gdiplus::PointF pos);

			// IDragState
			virtual void Update(CGraphView& graphView, Gdiplus::PointF pos);
			virtual void Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter);
			virtual void Exit(CGraphView& graphView);
			// ~IDragState

		private:

			Gdiplus::PointF	m_prevPos;
		};

		class CLinkDragState : public IGraphViewDragState
		{
		public:

			CLinkDragState(const CGraphViewNodeWeakPtr& pSrcNode, const char* srcOutputName, Gdiplus::PointF start, Gdiplus::PointF end, Gdiplus::Color color, bool newLink);

			// IDragState
			virtual void Update(CGraphView& graphView, Gdiplus::PointF pos);
			virtual void Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter);
			virtual void Exit(CGraphView& graphView);
			// ~IDragState

		private:

			CGraphViewNodeWeakPtr	m_pSrcNode;
			string								m_srcOutputName;
			Gdiplus::PointF				m_start;
			Gdiplus::PointF				m_end;
			Gdiplus::Color				m_color;
			bool									m_newLink;
		};

		class CSelectDragState : public IGraphViewDragState
		{
		public:

			CSelectDragState(Gdiplus::PointF pos);

			// IDragState
			virtual void Update(CGraphView& graphView, Gdiplus::PointF pos);
			virtual void Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter);
			virtual void Exit(CGraphView& graphView);
			// ~IDragState

		private:

			Gdiplus::RectF GetSelectionRect() const;

			Gdiplus::PointF	m_startPos;
			Gdiplus::PointF	m_endPos;
		};

		class CDropTarget : public COleDropTarget
		{
		public:

			CDropTarget(CGraphView& graphView);

			// COleDropTarget
			virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
			virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
			// ~COleDropTarget

		private:

			CGraphView&	m_graphView;
		};

		afx_msg void OnTimer(UINT_PTR nIDEvent);

		void OnLeftMouseButtonDrag(CPoint point);
		void OnRightMouseButtonDrag(CPoint point);
		void OnMiddleMouseButtonDrag(CPoint point);
		void Scroll(Gdiplus::PointF delta);
		void ExitDragState();
		void OnDelete();
		bool UpdateCursor();
		void FormatSelectedNodes();

		SGraphViewSettings      m_settings;
		SGraphViewGrid          m_grid;
		IGraphViewPainterPtr    m_pPainter;
		bool                    m_enabled;
		TGraphViewNodePtrVector m_nodes;
		TGraphViewLinkVector    m_links;
		Gdiplus::PointF         m_scrollOffset;
		CPoint                  m_prevMousePoint;
		float                   m_zoom;
		IGraphViewDragStatePtr  m_pDragState;
		TGraphViewNodePtrVector m_selectedNodes;
		size_t                  m_iSelectedLink;
		CDropTarget             m_dropTarget;

		static const float MIN_ZOOM;
		static const float MAX_ZOOM;
		static const float DELTA_ZOOM;
		static const float DEFAULT_NODE_SPACING;
	};
}
