// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GraphView.h"

#include <CrySystem/ITimer.h>

#include "GdiplusUtils.h"
#include "GraphViewFormatter.h"
#include "PluginUtils.h"

#pragma warning(disable: 4355)

namespace Schematyc2
{
	const float						CDefaultGraphViewPainter::NODE_BEVEL															= 15.0f;
	const BYTE						CDefaultGraphViewPainter::NODE_HEADER_ALPHA												= 200;
	const BYTE						CDefaultGraphViewPainter::NODE_HEADER_ALPHA_DISABLED							= 80;
	const float						CDefaultGraphViewPainter::NODE_HEADER_TEXT_BORDER_X								= 8.0f;
	const float						CDefaultGraphViewPainter::NODE_HEADER_TEXT_BORDER_Y								= 2.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_HEADER_TEXT_COLOR									= Gdiplus::Color(60, 60, 60);
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_ERROR_FILL_COLOR										= Gdiplus::Color(210, 66, 66);
	const char*						CDefaultGraphViewPainter::NODE_ERROR_TEXT													= "Error!";
	const float						CDefaultGraphViewPainter::NODE_ERROR_TEXT_BORDER_X								= 8.0f;
	const float						CDefaultGraphViewPainter::NODE_ERROR_TEXT_BORDER_Y								= 2.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_ERROR_TEXT_COLOR										= Gdiplus::Color(60, 60, 60);
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_WARNING_FILL_COLOR									= Gdiplus::Color(255, 255, 0);
	const char*						CDefaultGraphViewPainter::NODE_WARNING_TEXT												= "Warning!";
	const float						CDefaultGraphViewPainter::NODE_WARNING_TEXT_BORDER_X							= 8.0f;
	const float						CDefaultGraphViewPainter::NODE_WARNING_TEXT_BORDER_Y							= 2.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_WARNING_TEXT_COLOR									= Gdiplus::Color(60, 60, 60);
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_CONTENTS_FILL_COLOR									= Gdiplus::Color(200, 40, 40, 40);
	const float						CDefaultGraphViewPainter::NODE_CONTENTS_TEXT_BORDER_X							= 8.0f;
	const float						CDefaultGraphViewPainter::NODE_CONTENTS_TEXT_BORDER_Y							= 2.0f;
	const float						CDefaultGraphViewPainter::NODE_CONTENTS_TEXT_MAX_WIDTH							= 300.0f;
	const float						CDefaultGraphViewPainter::NODE_CONTENTS_TEXT_MAX_HEIGHT							= 200.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_CONTENTS_TEXT_COLOR									= Gdiplus::Color(255, 255, 255);
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_BODY_FILL_COLOR										= Gdiplus::Color(200, 40, 40, 40);
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_BODY_FILL_COLOR_DISABLED						= Gdiplus::Color(80, 40, 40, 40);
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_BODY_OUTLINE_COLOR									= Gdiplus::Color(200, 40, 40, 40);
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_BODY_OUTLINE_COLOR_HIGHLIGHT				= Gdiplus::Color(250, 232, 12);
	const float						CDefaultGraphViewPainter::NODE_BODY_OUTLINE_WIDTH									= 1.0f;
	const float						CDefaultGraphViewPainter::NODE_INPUT_OUPUT_HORZ_SPACING						= 40.0f;
	const float						CDefaultGraphViewPainter::NODE_INPUT_OUPUT_VERT_SPACING						= 5.0f;
	const float						CDefaultGraphViewPainter::NODE_INPUT_ICON_BORDER									= 5.0f;
	const float						CDefaultGraphViewPainter::NODE_INPUT_ICON_WIDTH										= 11.0f;
	const float						CDefaultGraphViewPainter::NODE_INPUT_ICON_HEIGHT									= 11.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_INPUT_ICON_OUTLINE_COLOR						= Gdiplus::Color(200, 20, 20, 20);
	const float						CDefaultGraphViewPainter::NODE_INPUT_ICON_OUTLINE_WIDTH						= 1.0f;
	const float						CDefaultGraphViewPainter::NODE_INPUT_NAME_BORDER									= 2.0f;
	const float						CDefaultGraphViewPainter::NODE_INPUT_NAME_WIDTH_MAX								= 400.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_INPUT_NAME_COLOR										=	Gdiplus::Color(240, 240, 240);
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_ICON_BORDER									= 5.0f;
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_ICON_WIDTH									= 11.0f;
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_ICON_HEIGHT									= 11.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_OUTPUT_ICON_OUTLINE_COLOR					= Gdiplus::Color(200, 20, 20, 20);
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_ICON_OUTLINE_WIDTH					= 1.0f;
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_NAME_BORDER									= 2.0f;
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_NAME_WIDTH_MAX							= 400.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_OUTPUT_NAME_COLOR									=	Gdiplus::Color(240, 240, 240);
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_SPACER_OFFSET								= 3.0f;
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_SPACER_WIDTH								= 80.0f;
	const float						CDefaultGraphViewPainter::NODE_OUTPUT_SPACER_LINE_WIDTH						= 1.0f;
	const Gdiplus::Color	CDefaultGraphViewPainter::NODE_OUTPUT_SPACER_COLOR								= Gdiplus::Color(140, 120, 120, 120);
	const float						CDefaultGraphViewPainter::LINK_WIDTH															=	2.0f;
	const float						CDefaultGraphViewPainter::LINK_CURVE_OFFSET												= 45.0f;
	const float						CDefaultGraphViewPainter::LINK_DODGE_X														= 15.0f;
	const float						CDefaultGraphViewPainter::LINK_DODGE_Y														= 15.0f;
	const float						CDefaultGraphViewPainter::ALPHA_HIGHLIGHT_MIN											= 0.3f;
	const float						CDefaultGraphViewPainter::ALPHA_HIGHLIGHT_MAX											= 1.0f;
	const float						CDefaultGraphViewPainter::ALPHA_HIGHLIGHT_SPEED										= 0.8f;
	const Gdiplus::Color	CDefaultGraphViewPainter::SELECTION_FILL_COLOR										= Gdiplus::Color(100, 0, 142, 212);
	const Gdiplus::Color	CDefaultGraphViewPainter::SELECTION_OUTLINE_COLOR									= Gdiplus::Color(0, 162, 232);

	const float						CGraphView::MIN_ZOOM							= 0.28f;
	const float						CGraphView::MAX_ZOOM							= 1.6f;
	const float						CGraphView::DELTA_ZOOM						= 0.001f;
	const float						CGraphView::DEFAULT_NODE_SPACING	= 40.0f;

	namespace Assets
	{
		static const wchar_t*						FONT_NAME		= L"Tahoma";
		static const float							FONT_SIZE		= 8.0f;
		static const Gdiplus::FontStyle	FONT_STYLE	= Gdiplus::FontStyleBold;

		//////////////////////////////////////////////////////////////////////////
		static inline const Gdiplus::Font& GetFont()
		{
			static const Gdiplus::Font	font(FONT_NAME, FONT_SIZE, FONT_STYLE);
			return font;
		}
	}

	static const UINT UPDATE_TIMER_EVENT_ID = 1;

	//////////////////////////////////////////////////////////////////////////
	SGraphViewFormatSettings::SGraphViewFormatSettings()
		: horzSpacing(40.0f)
		, vertSpacing(10.0f)
		, bSnapToGrid(true)
	{}

	//////////////////////////////////////////////////////////////////////////
	void SGraphViewFormatSettings::Serialize(Serialization::IArchive& archive)
	{
		archive(horzSpacing, "horzSpacing", "Horizontal Spacing");
		archive(vertSpacing, "vertSpacing", "Vertical Spacing");
		archive(bSnapToGrid, "bSnapToGrid", "Snap To Grid");
	}

	//////////////////////////////////////////////////////////////////////////
	void SGraphViewSettings::Serialize(Serialization::IArchive& archive)
	{
		archive(format, "format", "Format");
	}

	//////////////////////////////////////////////////////////////////////////
	CGraphViewNode::CGraphViewNode(const SGraphViewGrid& grid, Gdiplus::PointF pos)
		: m_grid(grid)
		, m_enabled(false)
		, m_selected(false)
		, m_statusFlags(EGraphViewNodeStatusFlags::None)
		, m_paintRect(grid.SnapPos(pos), Gdiplus::SizeF()) // #SchematycTODO : Might make more sense to default initialize this.
	{}

	//////////////////////////////////////////////////////////////////////////
	CGraphViewNode::~CGraphViewNode() {}

	//////////////////////////////////////////////////////////////////////////
	const SGraphViewGrid& CGraphViewNode::GetGrid() const
	{
		return m_grid;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphViewNode::SetPos(Gdiplus::PointF pos, bool bSnapToGrid)
	{
		if(bSnapToGrid)
		{
			pos = m_grid.SnapPos(pos);
		}
		m_paintRect.X	= pos.X;
		m_paintRect.Y	= pos.Y;
		OnMove(m_paintRect);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CGraphViewNode::GetPos() const
	{
		return Gdiplus::PointF(m_paintRect.X, m_paintRect.Y);
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphViewNode::SetPaintRect(Gdiplus::RectF paintRect)
	{
		m_paintRect = paintRect;
		OnMove(m_paintRect);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::RectF CGraphViewNode::GetPaintRect() const
	{
		return m_paintRect;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphViewNode::Enable(bool enable)
	{
		m_enabled = enable;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CGraphViewNode::IsEnabled() const
	{
		return m_enabled;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphViewNode::Select(bool select)
	{
		m_selected = select;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CGraphViewNode::IsSelected() const
	{
		return m_selected;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphViewNode::SetStatusFlags(EGraphViewNodeStatusFlags statusFlags)
	{
		m_statusFlags = statusFlags;
	}

	//////////////////////////////////////////////////////////////////////////
	EGraphViewNodeStatusFlags CGraphViewNode::GetStatusFlags() const
	{
		return m_statusFlags;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphViewNode::FindInput(Gdiplus::PointF pos) const
	{
		// #SchematycTODO : Do we need to double check that the paint rects are valid? Maybe we should just reference the ports by name to be safe?
		for(TRectVector::const_iterator iBeginInputPaintRect = m_inputPaintRects.begin(), iEndInputPaintRect = m_inputPaintRects.end(), iInputPaintRect = iBeginInputPaintRect; iInputPaintRect != iEndInputPaintRect; ++ iInputPaintRect)
		{
			if(iInputPaintRect->Contains(pos))
			{
				return iInputPaintRect - iBeginInputPaintRect;
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphViewNode::SetInputPaintRect(size_t iInput, Gdiplus::RectF paintRect)
	{
		if(iInput >= m_inputPaintRects.size())
		{
			m_inputPaintRects.resize(iInput + 1);
		}
		m_inputPaintRects[iInput] = paintRect;
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::RectF CGraphViewNode::GetInputPaintRect(size_t iInput) const
	{
		return iInput < m_inputPaintRects.size() ? m_inputPaintRects[iInput] : Gdiplus::RectF();
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CGraphViewNode::GetInputLinkPoint(size_t iInput) const
	{
		if(iInput < m_inputPaintRects.size())
		{
			const Gdiplus::RectF	paintRect = m_inputPaintRects[iInput];
			return Gdiplus::PointF(paintRect.GetLeft(), paintRect.Y + (paintRect.Height / 2.0f));
		}
		else
		{
			return Gdiplus::PointF();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphViewNode::FindOutput(Gdiplus::PointF pos) const
	{
		// #SchematycTODO : Do we need to double check that the paint rects are valid? Maybe we should just reference the ports by name to be safe?
		for(TRectVector::const_iterator iBeginOutputPaintRect = m_outputPaintRects.begin(), iEndOutputPaintRect = m_outputPaintRects.end(), iOutputPaintRect = iBeginOutputPaintRect; iOutputPaintRect != iEndOutputPaintRect; ++ iOutputPaintRect)
		{
			if(iOutputPaintRect->Contains(pos))
			{
				return iOutputPaintRect - iBeginOutputPaintRect;
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphViewNode::SetOutputPaintRect(size_t iOutput, Gdiplus::RectF paintRect)
	{
		if(iOutput >= m_outputPaintRects.size())
		{
			m_outputPaintRects.resize(iOutput + 1);
		}
		m_outputPaintRects[iOutput] = paintRect;
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::RectF CGraphViewNode::GetOutputPaintRect(size_t iOutput) const
	{
		return iOutput < m_outputPaintRects.size() ? m_outputPaintRects[iOutput] : Gdiplus::RectF();
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CGraphViewNode::GetOutputLinkPoint(size_t iOutput) const
	{
		if(iOutput < m_outputPaintRects.size())
		{
			const Gdiplus::RectF	paintRect = m_outputPaintRects[iOutput];
			return Gdiplus::PointF(paintRect.GetRight(), paintRect.Y + (paintRect.Height / 2.0f));
		}
		else
		{
			return Gdiplus::PointF();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CGraphViewNode::GetName() const
	{
		return "";
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CGraphViewNode::GetContents() const
	{
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::Color CGraphViewNode::GetHeaderColor() const
	{
		return Gdiplus::Color();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphViewNode::GetInputCount() const
	{
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphViewNode::FindInput(const char* name) const
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CGraphViewNode::GetInputName(size_t iInput) const
	{
		return "";
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptGraphPortFlags CGraphViewNode::GetInputFlags(size_t iInput) const
	{
		return EScriptGraphPortFlags::None;
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::Color CGraphViewNode::GetInputColor(size_t iInput) const
	{
		return Gdiplus::Color();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphViewNode::GetOutputCount() const
	{
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphViewNode::FindOutput(const char* name) const
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CGraphViewNode::GetOutputName(size_t iOutput) const
	{
		return "";
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptGraphPortFlags CGraphViewNode::GetOutputFlags(size_t iOutput) const
	{
		return EScriptGraphPortFlags::None;
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::Color CGraphViewNode::GetOutputColor(size_t iInput) const
	{
		return Gdiplus::Color();
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphViewNode::OnMove(Gdiplus::RectF paintRect) {}

	//////////////////////////////////////////////////////////////////////////
	SGraphViewLink::SGraphViewLink(CGraphView* _pGraphView, const CGraphViewNodeWeakPtr& _pSrcNode, const char* _srcOutputName, const CGraphViewNodeWeakPtr& _pDstNode, const char* _dstInputName)
		: pGraphView(_pGraphView)
		, pSrcNode(_pSrcNode)
		, srcOutputName(_srcOutputName)
		, pDstNode(_pDstNode)
		, dstInputName(_dstInputName)
		, selected(false)
	{}

	//////////////////////////////////////////////////////////////////////////
	SDefaultGraphViewPainterSettings::SDefaultGraphViewPainterSettings()
		: backgroundColor(60, 60, 60, 255)
	{
		gridLineColors[0] = ColorB(68, 68, 68, 255);
		gridLineColors[1] = ColorB(48, 48, 48, 255);
	}

	//////////////////////////////////////////////////////////////////////////
	void SDefaultGraphViewPainterSettings::Serialize(Serialization::IArchive& archive)
	{
		archive(backgroundColor, "backgroundColor", "Background Color");
		archive(gridLineColors, "gridLineColors", "Grid Line Colors");
	}

	//////////////////////////////////////////////////////////////////////////
	Serialization::SStruct CDefaultGraphViewPainter::GetSettings()
	{
		return Serialization::SStruct(m_settings);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::Color CDefaultGraphViewPainter::GetBackgroundColor() const
	{
		return GdiplusUtils::CreateColor(m_settings.backgroundColor);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintGrid(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid) const
	{
		const int left = static_cast<int>(grid.bounds.X);
		const int right = static_cast<int>(grid.bounds.X + grid.bounds.Width);
		const int top = static_cast<int>(grid.bounds.Y);
		const int bottom = static_cast<int>(grid.bounds.Y + grid.bounds.Height);
		const int horzSpacing = static_cast<int>(grid.spacing.X);
		const int vertSpacing = static_cast<int>(grid.spacing.Y);

		Gdiplus::Pen pen0(GdiplusUtils::CreateColor(m_settings.gridLineColors[0]), 1.0f);
		Gdiplus::Pen pen1(GdiplusUtils::CreateColor(m_settings.gridLineColors[1]), 1.0f);

		for(int x = left + horzSpacing; x < right; x += static_cast<int>(grid.spacing.X))
		{
			if(x % (horzSpacing * 10))
			{
				graphics.DrawLine(&pen0, x, top, x, bottom);
			}
			else
			{
				graphics.DrawLine(&pen1, x, top, x, bottom);
			}
		}

		for(int y = top + vertSpacing; y < bottom; y += static_cast<int>(grid.spacing.Y))
		{
			if(y % (vertSpacing * 10))
			{
				graphics.DrawLine(&pen0, left, y, right, y);
			}
			else
			{
				graphics.DrawLine(&pen1, left, y, right, y);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::UpdateNodePaintRect(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid, CGraphViewNode& node) const
	{
		// Calculate node header and status sizes.
		const Gdiplus::PointF headerSize = CalculateNodeHeaderSize(graphics, node);
		const Gdiplus::PointF statusSize = CalculateNodeStatusSize(graphics, node);
		const Gdiplus::PointF headerAndStatusSize(std::max(headerSize.X, statusSize.X), headerSize.Y + statusSize.Y);
		const Gdiplus::PointF contentsSize = CalculateNodeContentsSize(graphics, node);
		const Gdiplus::PointF headerAndStatusAndContentsSize(std::max(headerAndStatusSize.X, contentsSize.X), headerAndStatusSize.Y + contentsSize.Y);
		// Calculate total size of all node inputs.
		Gdiplus::PointF totalInputSize;
		const size_t    inputCount = node.GetInputCount();
		for(size_t inputIdx = 0; inputIdx < inputCount; ++ inputIdx)
		{
			const Gdiplus::PointF	inputSize = CalculateNodeInputSize(graphics, node.GetInputName(inputIdx));
			if(inputSize.X > totalInputSize.X)
			{
				totalInputSize.X = inputSize.X;
			}
			totalInputSize.Y += inputSize.Y + NODE_INPUT_OUPUT_VERT_SPACING;
		}
		totalInputSize.Y += NODE_INPUT_OUPUT_VERT_SPACING;
		// Calculate total size of all node outputs.
		Gdiplus::PointF totalOutputSize;
		const size_t    outputCount = node.GetOutputCount();
		for(size_t outputIdx = 0; outputIdx < outputCount; ++ outputIdx)
		{
			const Gdiplus::PointF	output = CalculateNodeOutputSize(graphics, node.GetOutputName(outputIdx));
			if(output.X > totalOutputSize.X)
			{
				totalOutputSize.X = output.X;
			}
			totalOutputSize.Y += output.Y + NODE_INPUT_OUPUT_VERT_SPACING;
		}
		totalOutputSize.Y += NODE_INPUT_OUPUT_VERT_SPACING;
		// Update node paint rect.
		const Gdiplus::SizeF paintSize(std::max(headerAndStatusAndContentsSize.X, totalInputSize.X + NODE_INPUT_OUPUT_HORZ_SPACING + totalOutputSize.X), headerAndStatusAndContentsSize.Y + std::max(totalInputSize.Y, totalOutputSize.Y));
		const Gdiplus::RectF paintRect(node.GetPos(), grid.SnapSize(paintSize));
		node.SetPaintRect(paintRect);
		// Update node input paint rects.
		Gdiplus::PointF inputPos(paintRect.X, paintRect.Y + headerAndStatusAndContentsSize.Y + NODE_INPUT_OUPUT_VERT_SPACING);
		for(size_t inputIdx = 0; inputIdx < inputCount; ++ inputIdx)
		{
			UpdateNodeInputPaintRect(graphics, node, inputIdx, inputPos, totalInputSize.X);
			inputPos.Y += node.GetInputPaintRect(inputIdx).Height + NODE_INPUT_OUPUT_VERT_SPACING;
		}
		// Update node output paint rects.
		Gdiplus::PointF outputPos(paintRect.X + paintRect.Width - totalOutputSize.X, paintRect.Y + headerAndStatusAndContentsSize.Y + NODE_INPUT_OUPUT_VERT_SPACING);
		for(size_t outputIdx = 0; outputIdx < outputCount; ++ outputIdx)
		{
			UpdateNodeOutputPaintRect(graphics, node, outputIdx, outputPos, totalOutputSize.X);
			outputPos.Y += node.GetOutputPaintRect(outputIdx).Height + NODE_INPUT_OUPUT_VERT_SPACING;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNode(Gdiplus::Graphics& graphics, const SGraphViewGrid& grid, CGraphViewNode& node) const
	{
		// Paint node body, header and outline
		const Gdiplus::RectF  paintRect = node.GetPaintRect();
		const Gdiplus::PointF headerSize = CalculateNodeHeaderSize(graphics, node);
		const Gdiplus::PointF statusSize = CalculateNodeStatusSize(graphics, node);
		const Gdiplus::PointF contentsSize = CalculateNodeContentsSize(graphics, node);
		PaintNodeBody(graphics, node, Gdiplus::RectF(paintRect.X, paintRect.Y + headerSize.Y + statusSize.Y + contentsSize.Y, paintRect.Width, paintRect.Height - headerSize.Y - statusSize.Y - contentsSize.Y));
		PaintNodeHeader(graphics, node, Gdiplus::RectF(paintRect.X, paintRect.Y, paintRect.Width, headerSize.Y));
		PaintNodeContents(graphics, node, Gdiplus::RectF(paintRect.X, paintRect.Y + headerSize.Y, paintRect.Width, contentsSize.Y));
		PaintNodeStatus(graphics, node, Gdiplus::RectF(paintRect.X, paintRect.Y + headerSize.Y + contentsSize.Y, paintRect.Width, headerSize.Y));
		PaintNodeOutline(graphics, node, paintRect);
		// Paint node inputs.
		for(size_t inputIdx = 0, inputCount = node.GetInputCount(); inputIdx < inputCount; ++ inputIdx)
		{
			PaintNodeInput(graphics, node, inputIdx);
		}
		// Paint node outputs.
		for(size_t outputIdx = 0, outputCount = node.GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
		{
			PaintNodeOutput(graphics, node, outputIdx);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::RectF CDefaultGraphViewPainter::PaintLink(Gdiplus::Graphics& graphics, Gdiplus::PointF startPoint, Gdiplus::PointF endPoint, Gdiplus::Color color, bool highlight, float scale) const
	{
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Part of temporary fix for slow graphs with lots of links, until we can optimize the intersection tests properly.
		if((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) 
		{
			scale *= 1.75f;
		}
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Animate color?
		if(highlight)
		{
			const float	time = gEnv->pTimer->GetAsyncTime().GetSeconds();
			const float	sinA = sin_tpl(time * ALPHA_HIGHLIGHT_SPEED * gf_PI2);
			const float	alpha = ALPHA_HIGHLIGHT_MIN + clamp_tpl(ALPHA_HIGHLIGHT_MIN + (sinA * ((ALPHA_HIGHLIGHT_MAX - ALPHA_HIGHLIGHT_MIN) * 0.5f)), 0.0f, 1.0f);
			color = Gdiplus::Color::MakeARGB(static_cast<BYTE>(alpha * 255.0f), color.GetRed(), color.GetGreen(), color.GetBlue());
		}
		// Calculate control points.
		const bool			leftToRight = startPoint.X < endPoint.X;
		const float			verticalSign = startPoint.Y < endPoint.Y ? 1.0f : -1.0f;
		Gdiplus::PointF	midPoint((startPoint.X + endPoint.X) / 2.0f, (startPoint.Y + endPoint.Y) / 2.0f);
		const size_t		controlPointCount = 8;
		Gdiplus::PointF	controlPoints[controlPointCount];
		controlPoints[0] = startPoint;
		{
			const float	offset = fabs_tpl(midPoint.X - startPoint.X) * 0.5f;
			controlPoints[1].X = startPoint.X + offset;
			if(leftToRight)
			{
				controlPoints[1].Y = startPoint.Y;
			}
			else
			{
				const float	extent = fabs_tpl(midPoint.Y - startPoint.Y) * 0.95f;
				controlPoints[1].Y = startPoint.Y + clamp_tpl(offset * 0.5f * verticalSign, -extent, extent);
			}
		}
		controlPoints[2].X	= Lerp(controlPoints[1].X, midPoint.X, 0.75f);
		controlPoints[2].Y	= Lerp(controlPoints[1].Y, midPoint.Y, 0.75f);
		controlPoints[3]		= midPoint;
		controlPoints[7]		= endPoint;
		{
			const float	offset = fabs_tpl(midPoint.X - endPoint.X) * 0.5f;
			controlPoints[6].X = endPoint.X - offset;
			if(leftToRight)
			{
				controlPoints[6].Y = endPoint.Y;
			}
			else
			{
				const float	extent = fabs_tpl(midPoint.Y - endPoint.Y) * 0.95f;
				controlPoints[6].Y = endPoint.Y - clamp_tpl(offset * 0.5f * verticalSign, -extent, extent);
			}
		}
		controlPoints[5].X	= Lerp(controlPoints[6].X, midPoint.X, 0.75f);
		controlPoints[5].Y	= Lerp(controlPoints[6].Y, midPoint.Y, 0.75f);
		controlPoints[4]		= midPoint;
		// Catch weird glitch when control points are close to parallel on y axis.
		for(size_t i = 1; i < controlPointCount; ++ i)
		{
			if(fabs_tpl(controlPoints[i].Y - controlPoints[0].Y) <= 1.0f)
			{
				controlPoints[i].Y = controlPoints[0].Y;
			}
		}
		// Draw beziers.
		const float		width = LINK_WIDTH * scale;
		Gdiplus::Pen	pen(color, width);
		graphics.DrawBezier(&pen, controlPoints[0], controlPoints[1], controlPoints[2], controlPoints[3]);
		graphics.DrawBezier(&pen, controlPoints[4], controlPoints[5], controlPoints[6], controlPoints[7]);
#if FALSE
		// Debug draw control points.
		for(size_t i = 0; i < controlPointCount; ++ i)
		{
			graphics.DrawEllipse(&pen, Gdiplus::RectF(controlPoints[i].X - 2.0f, controlPoints[i].Y - 2.0f, 4.0f, 4.0f));
		}
#endif
		// Calculate paint rectangle.
		// N.B. Technically this not how you calculate the bounds of a bezier, but it's sufficient for our purposes.
		Gdiplus::PointF	pmin = controlPoints[0];
		Gdiplus::PointF	pmax = controlPoints[0];
		for(size_t i = 1; i < controlPointCount; ++ i)
		{
			if(controlPoints[i].X < pmin.X)
			{
				pmin.X = controlPoints[i].X;
			}
			if(controlPoints[i].Y < pmin.Y)
			{
				pmin.Y = controlPoints[i].Y;
			}
			if(controlPoints[i].X > pmax.X)
			{
				pmax.X = controlPoints[i].X;
			}
			if(controlPoints[i].Y > pmax.Y)
			{
				pmax.Y = controlPoints[i].Y;
			}
		}
		return Gdiplus::RectF(pmin.X - width, pmin.Y - width, (pmax.X - pmin.X) + (width * 2.0f), (pmax.Y - pmin.Y) + (width * 2.0f));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintSelectionRect(Gdiplus::Graphics& graphics, Gdiplus::RectF rect) const
	{
		Gdiplus::SolidBrush		brush(SELECTION_FILL_COLOR);
		graphics.FillRectangle(&brush, rect);

		Gdiplus::Pen	pen(SELECTION_OUTLINE_COLOR, 1.0f);
		graphics.DrawRectangle(&pen, rect);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CDefaultGraphViewPainter::CalculateNodeHeaderSize(Gdiplus::Graphics& graphics, const CGraphViewNode& node) const
	{
		CStringW headerText(node.GetName());
		if(headerText.IsEmpty())
		{
			headerText = " ";
		}
		Gdiplus::RectF headerTextRect;
		graphics.MeasureString(headerText, headerText.GetLength(), &Assets::GetFont(), Gdiplus::PointF(), &headerTextRect);
		return Gdiplus::PointF(NODE_HEADER_TEXT_BORDER_X + headerTextRect.Width + NODE_HEADER_TEXT_BORDER_X, NODE_HEADER_TEXT_BORDER_Y + headerTextRect.Height + NODE_HEADER_TEXT_BORDER_Y);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CDefaultGraphViewPainter::CalculateNodeStatusSize(Gdiplus::Graphics& graphics, const CGraphViewNode& node) const
	{
		Gdiplus::RectF                  statusTextRect;
		const EGraphViewNodeStatusFlags statusFlags = node.GetStatusFlags();
		if((statusFlags & EGraphViewNodeStatusFlags::ContainsErrors) != 0)
		{
			const CStringW statusText(NODE_ERROR_TEXT);
			graphics.MeasureString(statusText, statusText.GetLength(), &Assets::GetFont(), Gdiplus::PointF(), &statusTextRect);
			statusTextRect.Width  += NODE_ERROR_TEXT_BORDER_X + NODE_ERROR_TEXT_BORDER_X;
			statusTextRect.Height += NODE_ERROR_TEXT_BORDER_Y + NODE_ERROR_TEXT_BORDER_Y;
		}
		else if((statusFlags & EGraphViewNodeStatusFlags::ContainsWarnings) != 0)
		{
			const CStringW statusText(NODE_WARNING_TEXT);
			graphics.MeasureString(statusText, statusText.GetLength(), &Assets::GetFont(), Gdiplus::PointF(), &statusTextRect);
			statusTextRect.Width  += NODE_WARNING_TEXT_BORDER_X + NODE_WARNING_TEXT_BORDER_X;
			statusTextRect.Height += NODE_WARNING_TEXT_BORDER_Y + NODE_WARNING_TEXT_BORDER_Y;
		}
		return Gdiplus::PointF(statusTextRect.Width, statusTextRect.Height);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CDefaultGraphViewPainter::CalculateNodeContentsSize(Gdiplus::Graphics& graphics, const CGraphViewNode& node) const
	{
		if (const char * const nodeContents = node.GetContents())
		{
			Gdiplus::RectF contentsTextRect;
			const CStringW contentsText(nodeContents);
			const Gdiplus::RectF layoutRect(0.0f, 0.0f, NODE_CONTENTS_TEXT_MAX_WIDTH, NODE_CONTENTS_TEXT_MAX_HEIGHT);
			graphics.MeasureString(contentsText, contentsText.GetLength(), &Assets::GetFont(), layoutRect, Gdiplus::StringFormat::GenericTypographic(), &contentsTextRect);
			contentsTextRect.Width  += NODE_CONTENTS_TEXT_BORDER_X + NODE_CONTENTS_TEXT_BORDER_X;
			contentsTextRect.Height += NODE_CONTENTS_TEXT_BORDER_Y + NODE_CONTENTS_TEXT_BORDER_Y;
			return Gdiplus::PointF(contentsTextRect.Width, contentsTextRect.Height);
		}
		return Gdiplus::PointF();
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CDefaultGraphViewPainter::CalculateNodeInputSize(Gdiplus::Graphics& graphics, const char* name) const
	{
		const CStringW	stringW(name != NULL ? name : "");
		Gdiplus::RectF	nameRect;
		graphics.MeasureString(stringW, stringW.GetLength(), &Assets::GetFont(), Gdiplus::PointF(), &nameRect);

		const Gdiplus::PointF	iconSize(NODE_INPUT_ICON_BORDER + NODE_INPUT_ICON_WIDTH + NODE_INPUT_ICON_BORDER, NODE_INPUT_ICON_BORDER + NODE_INPUT_ICON_HEIGHT + NODE_INPUT_ICON_BORDER);
		const Gdiplus::PointF	nameSize(NODE_INPUT_NAME_BORDER + nameRect.Width + NODE_INPUT_NAME_BORDER, NODE_INPUT_NAME_BORDER + nameRect.Height + NODE_INPUT_NAME_BORDER);
		return Gdiplus::PointF(iconSize.X + nameSize.X, std::max(iconSize.Y, nameSize.Y));
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CDefaultGraphViewPainter::CalculateNodeOutputSize(Gdiplus::Graphics& graphics, const char* name) const
	{
		const CStringW	stringW(name != NULL ? name : "");
		Gdiplus::RectF	nameRect;
		graphics.MeasureString(stringW, stringW.GetLength(), &Assets::GetFont(), Gdiplus::PointF(), &nameRect);

		const Gdiplus::PointF	iconSize(NODE_OUTPUT_ICON_BORDER + NODE_OUTPUT_ICON_WIDTH + NODE_OUTPUT_ICON_BORDER, NODE_OUTPUT_ICON_BORDER + NODE_OUTPUT_ICON_HEIGHT + NODE_OUTPUT_ICON_BORDER);
		const Gdiplus::PointF	nameSize(NODE_OUTPUT_NAME_BORDER + nameRect.Width + NODE_OUTPUT_NAME_BORDER, NODE_OUTPUT_NAME_BORDER + nameRect.Height + NODE_OUTPUT_NAME_BORDER);
		return Gdiplus::PointF(iconSize.X + nameSize.X, std::max(iconSize.Y, nameSize.Y));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeBody(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const
	{
		const float								bevels[4] = { 0.0f, 0.0f, NODE_BEVEL, NODE_BEVEL };
		const Gdiplus::SolidBrush	brush(node.IsEnabled() ? NODE_BODY_FILL_COLOR : NODE_BODY_FILL_COLOR_DISABLED);
		GdiplusUtils::FillBeveledRect(graphics, paintRect, bevels, brush);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeHeader(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const
	{
		const Gdiplus::Color headerColor = node.GetHeaderColor();
		const BYTE           alpha = node.IsEnabled() ? NODE_HEADER_ALPHA : NODE_HEADER_ALPHA_DISABLED;
		const Gdiplus::Color fillColorLeft = Gdiplus::Color(alpha, headerColor.GetRed(), headerColor.GetGreen(), headerColor.GetBlue());
		const Gdiplus::Color fillColorRight = GdiplusUtils::LerpColor(fillColorLeft, Gdiplus::Color(alpha, 255, 255, 255), 0.6f);

		const float                        bevels[4] = { NODE_BEVEL, NODE_BEVEL, 0.0f, 0.0f };
		const Gdiplus::LinearGradientBrush brush(paintRect, fillColorLeft, fillColorRight, Gdiplus::LinearGradientModeForwardDiagonal);
		GdiplusUtils::FillBeveledRect(graphics, paintRect, bevels, brush);

		const CStringW            headerText(node.GetName());
		const Gdiplus::SolidBrush textBrush(NODE_HEADER_TEXT_COLOR);
		graphics.DrawString(headerText, headerText.GetLength(), &Assets::GetFont(), Gdiplus::PointF(paintRect.X + NODE_HEADER_TEXT_BORDER_X, paintRect.Y + NODE_HEADER_TEXT_BORDER_Y), &textBrush);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeContents(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const
	{
		if (const char * const nodeContents = node.GetContents())
		{
			Gdiplus::SolidBrush brush(NODE_CONTENTS_FILL_COLOR);
			graphics.FillRectangle(&brush, paintRect);
			const CStringW            contentsText(nodeContents);
			const Gdiplus::SolidBrush textBrush(NODE_CONTENTS_TEXT_COLOR);
			const Gdiplus::PointF     pos(paintRect.X + NODE_CONTENTS_TEXT_BORDER_X, paintRect.Y + NODE_CONTENTS_TEXT_BORDER_Y);
			const Gdiplus::RectF      layoutRect(pos.X, pos.Y, NODE_CONTENTS_TEXT_MAX_WIDTH, NODE_CONTENTS_TEXT_MAX_HEIGHT);
			graphics.DrawString(contentsText, contentsText.GetLength(), &Assets::GetFont(), layoutRect, Gdiplus::StringFormat::GenericTypographic(), &textBrush);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeStatus(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const
	{
		const EGraphViewNodeStatusFlags statusFlags = node.GetStatusFlags();
		if((statusFlags & EGraphViewNodeStatusFlags::ContainsErrors) != 0)
		{
			Gdiplus::SolidBrush brush(NODE_ERROR_FILL_COLOR);
			graphics.FillRectangle(&brush, paintRect);

			const CStringW            statusText(NODE_ERROR_TEXT);
			const Gdiplus::SolidBrush textBrush(NODE_ERROR_TEXT_COLOR);
			graphics.DrawString(statusText, statusText.GetLength(), &Assets::GetFont(), Gdiplus::PointF(paintRect.X + NODE_ERROR_TEXT_BORDER_X, paintRect.Y + NODE_ERROR_TEXT_BORDER_Y), &textBrush);
		}
		else if((statusFlags & EGraphViewNodeStatusFlags::ContainsWarnings) != 0)
		{
			Gdiplus::SolidBrush brush(NODE_WARNING_FILL_COLOR);
			graphics.FillRectangle(&brush, paintRect);

			const CStringW            statusText(NODE_WARNING_TEXT);
			const Gdiplus::SolidBrush textBrush(NODE_WARNING_TEXT_COLOR);
			graphics.DrawString(statusText, statusText.GetLength(), &Assets::GetFont(), Gdiplus::PointF(paintRect.X + NODE_WARNING_TEXT_BORDER_X, paintRect.Y + NODE_WARNING_TEXT_BORDER_Y), &textBrush);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeOutline(Gdiplus::Graphics& graphics, CGraphViewNode& node, Gdiplus::RectF paintRect) const
	{
		Gdiplus::Color outlineColor = NODE_BODY_OUTLINE_COLOR;
		if(node.IsSelected())
		{
			const float time = gEnv->pTimer->GetAsyncTime().GetSeconds();
			const float sinA = sin_tpl(time * ALPHA_HIGHLIGHT_SPEED * gf_PI2);
			const float alpha = ALPHA_HIGHLIGHT_MIN + clamp_tpl(ALPHA_HIGHLIGHT_MIN + (sinA * ((ALPHA_HIGHLIGHT_MAX - ALPHA_HIGHLIGHT_MIN) * 0.5f)), 0.0f, 1.0f);
			outlineColor = Gdiplus::Color::MakeARGB(static_cast<BYTE>(alpha * 255.0f), NODE_BODY_OUTLINE_COLOR_HIGHLIGHT.GetRed(), NODE_BODY_OUTLINE_COLOR_HIGHLIGHT.GetGreen(), NODE_BODY_OUTLINE_COLOR_HIGHLIGHT.GetBlue());
		}

		const float        bevels[4] = { NODE_BEVEL, NODE_BEVEL, NODE_BEVEL, NODE_BEVEL };
		const Gdiplus::Pen pen(outlineColor, NODE_BODY_OUTLINE_WIDTH);
		GdiplusUtils::PaintBeveledRect(graphics, paintRect, bevels, pen);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::UpdateNodeInputPaintRect(Gdiplus::Graphics& graphics, CGraphViewNode& node, size_t iInput, Gdiplus::PointF pos, float maxWidth) const
	{
		const CStringW	stringW(node.GetInputName(iInput));
		Gdiplus::RectF	nameRect;
		graphics.MeasureString(stringW, stringW.GetLength(), &Assets::GetFont(), Gdiplus::PointF(), &nameRect);

		const Gdiplus::PointF	iconSize(NODE_INPUT_ICON_BORDER + NODE_INPUT_ICON_WIDTH + NODE_INPUT_ICON_BORDER, NODE_INPUT_ICON_BORDER + NODE_INPUT_ICON_HEIGHT + NODE_INPUT_ICON_BORDER);
		const Gdiplus::PointF	nameSize(NODE_INPUT_NAME_BORDER + nameRect.Width + NODE_INPUT_NAME_BORDER, NODE_INPUT_NAME_BORDER + nameRect.Height + NODE_INPUT_NAME_BORDER);
		const Gdiplus::RectF	paintRect(pos.X, pos.Y, iconSize.X + nameSize.X, std::max(iconSize.Y, nameSize.Y));
		node.SetInputPaintRect(iInput, paintRect);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeInput(Gdiplus::Graphics& graphics, CGraphViewNode& node, size_t iInput) const
	{
		const Gdiplus::RectF	paintRect = node.GetInputPaintRect(iInput);
		const Gdiplus::PointF	iconSize(NODE_INPUT_ICON_BORDER + NODE_INPUT_ICON_WIDTH + NODE_INPUT_ICON_BORDER, NODE_INPUT_ICON_BORDER + NODE_INPUT_ICON_HEIGHT + NODE_INPUT_ICON_BORDER);
		Gdiplus::PointF	iconPos(paintRect.X + NODE_INPUT_ICON_BORDER, paintRect.Y + NODE_INPUT_ICON_BORDER);
		if(iconSize.Y < paintRect.Height)
		{
			iconPos.Y += (paintRect.Height - iconSize.Y) * 0.5f;
		}
		PaintNodeInputIcon(graphics, iconPos, node.GetInputColor(iInput), (node.GetInputFlags(iInput) & EScriptGraphPortFlags::Execute) != 0);

		const CStringW				stringW(node.GetInputName(iInput));
		const Gdiplus::RectF	alignedNameRect(paintRect.X + iconSize.X, paintRect.Y, paintRect.Width - iconSize.X, paintRect.Height);
		Gdiplus::StringFormat	stringFormat;
		stringFormat.SetAlignment(Gdiplus::StringAlignmentNear);
		stringFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		const Gdiplus::SolidBrush	nameBrush(NODE_OUTPUT_NAME_COLOR);
		graphics.DrawString(stringW, stringW.GetLength(), &Assets::GetFont(), alignedNameRect, &stringFormat, &nameBrush);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeInputIcon(Gdiplus::Graphics& graphics, Gdiplus::PointF pos, Gdiplus::Color color, bool active) const
	{
		const Gdiplus::SolidBrush	fillBrush(color);
		const Gdiplus::Pen				pen(NODE_INPUT_ICON_OUTLINE_COLOR, NODE_INPUT_ICON_OUTLINE_WIDTH);
		if(active)
		{
			const Gdiplus::PointF	points[] =
			{
				Gdiplus::PointF(pos.X, pos.Y),
				Gdiplus::PointF(pos.X + NODE_INPUT_ICON_WIDTH, pos.Y + (NODE_INPUT_ICON_HEIGHT * 0.5f)),
				Gdiplus::PointF(pos.X, pos.Y + NODE_INPUT_ICON_HEIGHT)
			};
			graphics.FillPolygon(&fillBrush, points, CRY_ARRAY_COUNT(points));
		}
		else
		{
			graphics.FillEllipse(&fillBrush, pos.X, pos.Y, NODE_INPUT_ICON_WIDTH, NODE_INPUT_ICON_HEIGHT);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::UpdateNodeOutputPaintRect(Gdiplus::Graphics& graphics, CGraphViewNode& node, size_t iOutput, Gdiplus::PointF pos, float maxWidth) const
	{
		const CStringW	stringW(node.GetOutputName(iOutput));
		Gdiplus::RectF	nameRect;
		graphics.MeasureString(stringW, stringW.GetLength(), &Assets::GetFont(), Gdiplus::PointF(), &nameRect);

		const Gdiplus::PointF	iconSize(NODE_OUTPUT_ICON_BORDER + NODE_OUTPUT_ICON_WIDTH + NODE_OUTPUT_ICON_BORDER, NODE_OUTPUT_ICON_BORDER + NODE_OUTPUT_ICON_HEIGHT + NODE_OUTPUT_ICON_BORDER);
		const Gdiplus::PointF	nameSize(NODE_OUTPUT_NAME_BORDER + nameRect.Width + NODE_OUTPUT_NAME_BORDER, NODE_OUTPUT_NAME_BORDER + nameRect.Height + NODE_OUTPUT_NAME_BORDER);
		const Gdiplus::RectF	paintRect(pos.X + maxWidth - (iconSize.X + nameSize.X), pos.Y, iconSize.X + nameSize.X, std::max(iconSize.Y, nameSize.Y));
		node.SetOutputPaintRect(iOutput, paintRect);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeOutput(Gdiplus::Graphics& graphics, CGraphViewNode& node, size_t iOutput) const
	{
		const Gdiplus::RectF	paintRect = node.GetOutputPaintRect(iOutput);
		const Gdiplus::PointF	iconSize(NODE_OUTPUT_ICON_BORDER + NODE_OUTPUT_ICON_WIDTH + NODE_OUTPUT_ICON_BORDER, NODE_OUTPUT_ICON_BORDER + NODE_OUTPUT_ICON_HEIGHT + NODE_OUTPUT_ICON_BORDER);
		Gdiplus::PointF				iconPos(paintRect.X + paintRect.Width - NODE_OUTPUT_ICON_WIDTH - NODE_OUTPUT_ICON_BORDER, paintRect.Y + NODE_OUTPUT_ICON_BORDER);
		if(iconSize.Y < paintRect.Height)
		{
			iconPos.Y += (paintRect.Height - iconSize.Y) * 0.5f;
		}
		PaintNodeOutputIcon(graphics, iconPos, node.GetOutputColor(iOutput), (node.GetOutputFlags(iOutput) & EScriptGraphPortFlags::Execute) != 0);

		const CStringW				stringW(node.GetOutputName(iOutput));
		const Gdiplus::RectF	alignedNameRect(paintRect.X, paintRect.Y, paintRect.Width - iconSize.X, paintRect.Height);
		Gdiplus::StringFormat	stringFormat;
		stringFormat.SetAlignment(Gdiplus::StringAlignmentFar);
		stringFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		const Gdiplus::SolidBrush	nameBrush(NODE_OUTPUT_NAME_COLOR);
		graphics.DrawString(stringW, stringW.GetLength(), &Assets::GetFont(), alignedNameRect, &stringFormat, &nameBrush);

		if(((node.GetOutputFlags(iOutput) & EScriptGraphPortFlags::SpacerAbove) != 0) && (iOutput > 0))
		{
			const Gdiplus::Pen	pen(NODE_OUTPUT_SPACER_COLOR, NODE_OUTPUT_SPACER_LINE_WIDTH);
			const float					right = paintRect.GetRight() - NODE_OUTPUT_SPACER_OFFSET;
			const float					left = right - NODE_OUTPUT_SPACER_WIDTH;
			const float					top = paintRect.GetTop() - (NODE_INPUT_OUPUT_VERT_SPACING * 0.5f);
			graphics.DrawLine(&pen, Gdiplus::PointF(left, top), Gdiplus::PointF(right, top));
		}

		if(((node.GetOutputFlags(iOutput) & EScriptGraphPortFlags::SpacerBelow) != 0) && (iOutput < (node.GetOutputCount() - 1)))
		{
			const Gdiplus::Pen	pen(NODE_OUTPUT_SPACER_COLOR, NODE_OUTPUT_SPACER_LINE_WIDTH);
			const float					right = paintRect.GetRight() - NODE_OUTPUT_SPACER_OFFSET;
			const float					left = right - NODE_OUTPUT_SPACER_WIDTH;
			const float					bottom = paintRect.GetBottom() + (NODE_INPUT_OUPUT_VERT_SPACING * 0.5f);
			graphics.DrawLine(&pen, Gdiplus::PointF(left, bottom), Gdiplus::PointF(right, bottom));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDefaultGraphViewPainter::PaintNodeOutputIcon(Gdiplus::Graphics& graphics, Gdiplus::PointF pos, Gdiplus::Color color, bool active) const
	{
		const Gdiplus::SolidBrush	fillBrush(color);
		const Gdiplus::Pen				pen(NODE_OUTPUT_ICON_OUTLINE_COLOR, NODE_OUTPUT_ICON_OUTLINE_WIDTH);
		if(active)
		{
			const Gdiplus::PointF	points[] =
			{
				Gdiplus::PointF(pos.X, pos.Y),
				Gdiplus::PointF(pos.X + NODE_OUTPUT_ICON_WIDTH, pos.Y + (NODE_OUTPUT_ICON_HEIGHT * 0.5f)),
				Gdiplus::PointF(pos.X, pos.Y + NODE_OUTPUT_ICON_HEIGHT)
			};
			graphics.FillPolygon(&fillBrush, points, CRY_ARRAY_COUNT(points));
		}
		else
		{
			graphics.FillEllipse(&fillBrush, pos.X, pos.Y, NODE_OUTPUT_ICON_WIDTH, NODE_OUTPUT_ICON_HEIGHT);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CGraphView, CWnd)
		ON_WM_KILLFOCUS()
		ON_WM_PAINT()
		ON_WM_MOUSEMOVE()
		ON_WM_MOUSEWHEEL()
		ON_WM_LBUTTONDOWN()
		ON_WM_LBUTTONUP()
		ON_WM_RBUTTONDOWN()
		ON_WM_RBUTTONUP()
		ON_WM_MBUTTONDOWN()
		ON_WM_MBUTTONUP()
		ON_WM_KEYDOWN()
		ON_WM_KEYUP()
		ON_WM_SETCURSOR()
		ON_WM_TIMER()
	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	BOOL CGraphView::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
	{
		if(!m_hWnd)
		{
			// Create view.
			LPCTSTR	lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
			VERIFY(CreateEx(NULL, lpClassName, "SchematycView", dwStyle, CRect(0, 0, 100, 100), pParentWnd, nID));
			if(m_hWnd)
			{
				// Create timer.
				CWnd::SetTimer(UPDATE_TIMER_EVENT_ID, 1000 / 30, NULL);
				// Register drop target.
				m_dropTarget.Register(this);
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::Refresh()
	{
		__super::Invalidate(TRUE);
	}

	//////////////////////////////////////////////////////////////////////////
	SGraphViewSettings& CGraphView::GetSettings()
	{
		return m_settings;
	}

	//////////////////////////////////////////////////////////////////////////
	const SGraphViewGrid& CGraphView::GetGrid() const
	{
		return m_grid;
	}

	//////////////////////////////////////////////////////////////////////////
	const IGraphViewPainterPtr& CGraphView::GetPainter() const
	{
		return m_pPainter;
	}

	//////////////////////////////////////////////////////////////////////////
	TGraphViewLinkVector& CGraphView::GetLinks()
	{
		return m_links;
	}

	//////////////////////////////////////////////////////////////////////////
	const TGraphViewLinkVector& CGraphView::GetLinks() const
	{
		return m_links;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::DoQuickSearch(const char* defaultFilter, const CGraphViewNodePtr& pNode, size_t iNodeOutput, const CPoint* pPoint)
	{
		SET_LOCAL_RESOURCE_SCOPE
		CPoint	cursorPoint;
		GetCursorPos(&cursorPoint);
		if(const IQuickSearchOptions*	pQuickSearchOptions = GetQuickSearchOptions(cursorPoint, pNode, iNodeOutput))
		{
			CQuickSearchDlg	quickSearchDlg(this, CPoint(cursorPoint.x - 20, cursorPoint.y - 20), "Nodes", defaultFilter, nullptr, *pQuickSearchOptions);
			if(quickSearchDlg.DoModal() == IDOK)
			{
				SetFocus();	// Ensure we regain focus from quick search dialog.
				OnQuickSearchResult(pPoint ? *pPoint : cursorPoint, pNode, iNodeOutput, quickSearchDlg.GetResult());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CGraphView::CGraphView(const SGraphViewGrid& grid, const IGraphViewPainterPtr& pPainter)
		: m_grid(grid)
		, m_pPainter(pPainter ? pPainter : IGraphViewPainterPtr(new CDefaultGraphViewPainter()))
		, m_enabled(false)
		, m_prevMousePoint(0, 0)
		, m_zoom(1.0f)
		, m_iSelectedLink(INVALID_INDEX)
		, m_dropTarget(*this)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::Enable(bool enable)
	{
		if(enable != m_enabled)
		{
			ExitDragState();
		}
		m_enabled = enable;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CGraphView::IsEnabled() const
	{
		return m_enabled;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::ClearSelection()
	{
		ClearNodeSelection();
		ClearLinkSelection();
		OnSelection(CGraphViewNodePtr(), nullptr);
		__super::Invalidate(TRUE);
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::SetScrollOffset(Gdiplus::PointF scrollOffset)
	{
		m_scrollOffset = scrollOffset;
		OnMove(m_scrollOffset);
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::AddNode(const CGraphViewNodePtr& pNode, bool select, bool scrollToFit)
	{
		CRY_ASSERT(pNode);
		if(pNode)
		{
			m_nodes.push_back(pNode);
			if(select)
			{
				ClearNodeSelection();
				SelectNode(pNode);
			}
			if(scrollToFit)
			{
				CPaintDC					paintDC(this);
				Gdiplus::Graphics	mainGraphics(paintDC.GetSafeHdc());
				m_pPainter->UpdateNodePaintRect(mainGraphics, m_grid, *pNode);
				ScrollToFit(pNode->GetPaintRect());
			}
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::RemoveNode(CGraphViewNodePtr pNode)
	{
		CRY_ASSERT(pNode != NULL);
		if(pNode != NULL)
		{
			// Make sure node is not selected.
			ClearSelection();
			// Remove links connected to node.
			RemoveLinksConnectedToNode(pNode);
			// Remove node.
			TGraphViewNodePtrVector::iterator	iEndNode = m_nodes.end();
			TGraphViewNodePtrVector::iterator	iEraseNode = std::find(m_nodes.begin(), iEndNode, pNode);
			CRY_ASSERT(iEraseNode != iEndNode);
			if(iEraseNode != iEndNode)
			{
				m_nodes.erase(iEraseNode);
				OnNodeRemoved(*pNode);
			}
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CGraphViewNodePtr CGraphView::FindNode(Gdiplus::PointF pos)
	{
		for(TGraphViewNodePtrVector::const_iterator iNode = m_nodes.begin(), iEndNode = m_nodes.end(); iNode != iEndNode; ++ iNode)
		{
			const CGraphViewNodePtr&	pNode = *iNode;
			if(pNode->GetPaintRect().Contains(pos))
			{
				return pNode;
			}
		}
		return CGraphViewNodePtr();
	}

	//////////////////////////////////////////////////////////////////////////
	TGraphViewNodePtrVector& CGraphView::GetNodes()
	{
		return m_nodes;
	}

	//////////////////////////////////////////////////////////////////////////
	const TGraphViewNodePtrVector& CGraphView::GetNodes() const
	{
		return m_nodes;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::ClearNodeSelection()
	{
		for(TGraphViewNodePtrVector::iterator iSelectedNode = m_selectedNodes.begin(), iEndSelectedNode = m_selectedNodes.end(); iSelectedNode != iEndSelectedNode; ++ iSelectedNode)
		{
			(*iSelectedNode)->Select(false);
		}
		m_selectedNodes.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::SelectNode(const CGraphViewNodePtr& pNode)
	{
		if((pNode != NULL) && (pNode->IsSelected() == false))
		{
			ClearLinkSelection();
			m_selectedNodes.push_back(pNode);
			pNode->Select(true);
			OnSelection(pNode, nullptr);
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::SelectNodesInRect(const Gdiplus::RectF& rect)
	{
		ClearSelection();
		for(TGraphViewNodePtrVector::iterator iNode = m_nodes.begin(), iEndNode = m_nodes.end(); iNode != iEndNode; ++ iNode)
		{
			CGraphViewNodePtr	pNode = *iNode;
			if(rect.IntersectsWith(pNode->GetPaintRect()) != 0)
			{
				SelectNode(pNode);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	const TGraphViewNodePtrVector& CGraphView::GetSelectedNodes() const
	{
		return m_selectedNodes;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::MoveSelectedNodes(Gdiplus::PointF transform, bool bSnapToGrid)
	{
		for(TGraphViewNodePtrVector::iterator iNode = m_selectedNodes.begin(), iEndNode = m_selectedNodes.end(); iNode != iEndNode; ++ iNode)
		{
			CGraphViewNodePtr	pNode = *iNode;
			pNode->SetPos(pNode->GetPos() + transform, m_settings.format.bSnapToGrid && bSnapToGrid);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CGraphView::CreateLink(const CGraphViewNodePtr& pSrcNode, const char* srcOutputName, const CGraphViewNodePtr& pDstNode, const char* dstInputName)
	{
		CRY_ASSERT((pSrcNode != NULL) && (srcOutputName != NULL) && (pDstNode != NULL) && (dstInputName != NULL));
		if((pSrcNode != NULL) && (srcOutputName != NULL) && (pDstNode != NULL) && (dstInputName != NULL))
		{
			if(CanCreateLink(*pSrcNode, srcOutputName, *pDstNode, dstInputName) == true)
			{
				m_links.push_back(SGraphViewLink(this, pSrcNode, srcOutputName, pDstNode, dstInputName));
				OnLinkCreated(m_links.back());
				__super::Invalidate(TRUE);
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::AddLink(const CGraphViewNodePtr& pSrcNode, const char* srcOutputName, const CGraphViewNodePtr& pDstNode, const char* dstInputName)
	{
		CRY_ASSERT((pSrcNode != NULL) && (srcOutputName != NULL) && (pDstNode != NULL) && (dstInputName != NULL));
		if((pSrcNode != NULL) && (srcOutputName != NULL) && (pDstNode != NULL) && (dstInputName != NULL))
		{
			m_links.push_back(SGraphViewLink(this, pSrcNode, srcOutputName, pDstNode, dstInputName));
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::RemoveLink(size_t iLink)
	{
		const size_t	linkCount = m_links.size();
		CRY_ASSERT(iLink < linkCount);
		if(iLink < linkCount)
		{
			if(m_iSelectedLink == iLink)
			{
				m_iSelectedLink = INVALID_INDEX;
			}
			OnLinkRemoved(m_links[iLink]);
			m_links.erase(m_links.begin() + iLink);
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::RemoveLinksConnectedToNode(const CGraphViewNodePtr& pNode)
	{
		if(!m_links.empty())
		{
			TGraphViewLinkVector::iterator	iEndLink = m_links.end();
			TGraphViewLinkVector::iterator	iLastLink = iEndLink - 1;
			for(TGraphViewLinkVector::iterator iLink = m_links.begin(); iLink <= iLastLink; )
			{
				SGraphViewLink& link = *iLink;
				if((link.pSrcNode.lock() == pNode) || (link.pDstNode.lock() == pNode))
				{
					OnLinkRemoved(link);
					if(iLink < iLastLink)
					{
						*iLink = *iLastLink;
					}
					-- iLastLink;
				}
				else
				{
					++ iLink;
				}				
			}
			m_links.erase(iLastLink + 1, iEndLink);
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::RemoveLinksConnectedToNodeInput(const CGraphViewNodePtr& pNode, const char* inputName)
	{
		CRY_ASSERT(inputName != NULL);
		if((inputName != NULL) && (m_links.empty() == false))
		{
			TGraphViewLinkVector::iterator	iEndLink = m_links.end();
			TGraphViewLinkVector::iterator	iLastLink = iEndLink - 1;
			for(TGraphViewLinkVector::iterator iLink = m_links.begin(); iLink <= iLastLink; )
			{
				SGraphViewLink& link = *iLink;
				if((link.pDstNode.lock() == pNode) && (strcmp(link.dstInputName.c_str(), inputName) == 0))
				{
					OnLinkRemoved(link);
					if(iLink < iLastLink)
					{
						*iLink = *iLastLink;
					}
					-- iLastLink;
				}
				else
				{
					++ iLink;
				}				
			}
			m_links.erase(iLastLink + 1, iEndLink);
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::RemoveLinksConnectedToNodeOutput(const CGraphViewNodePtr& pNode, const char* outputName)
	{
		CRY_ASSERT(outputName != NULL);
		if((outputName != NULL) && (m_links.empty() == false))
		{
			TGraphViewLinkVector::iterator	iEndLink = m_links.end();
			TGraphViewLinkVector::iterator	iLastLink = iEndLink - 1;
			for(TGraphViewLinkVector::iterator iLink = m_links.begin(); iLink <= iLastLink; )
			{
				SGraphViewLink& link = *iLink;
				if((link.pSrcNode.lock() == pNode) && (strcmp(link.srcOutputName.c_str(), outputName) == 0))
				{
					OnLinkRemoved(link);
					if(iLink < iLastLink)
					{
						*iLink = *iLastLink;
					}
					-- iLastLink;
				}
				else
				{
					++ iLink;
				}				
			}
			m_links.erase(iLastLink + 1, iEndLink);
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::FindLinks(const CGraphViewNodePtr& pDstNode, const char* inputName, TSizeTVector& output) const
	{
		CRY_ASSERT(inputName != NULL);
		if(inputName != NULL)
		{
			for(TGraphViewLinkVector::const_iterator iBeginLink = m_links.begin(), iEndLink = m_links.end(), iLink = iBeginLink; iLink != iEndLink; ++ iLink)
			{
				const SGraphViewLink&	link = *iLink;
				if((link.pDstNode.lock() == pDstNode) && (strcmp(link.dstInputName.c_str(), inputName) == 0))
				{
					output.push_back(iLink - iBeginLink);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphView::FindLink(const CGraphViewNodePtr& pSrcNode, const char* srcOutputName, const CGraphViewNodePtr& pDstNode, const char* dstInputName)
	{
		for(TGraphViewLinkVector::iterator iBeginLink = m_links.begin(), iEndLink = m_links.end(), iLink = iBeginLink; iLink != iEndLink; ++ iLink)
		{
			SGraphViewLink&	link = *iLink;
			if((link.pSrcNode.lock() == pSrcNode) && (strcmp(link.srcOutputName, srcOutputName) == 0) && (link.pDstNode.lock() == pDstNode) &&  (strcmp(link.dstInputName, dstInputName) == 0))
			{
				return iLink - iBeginLink;
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphView::FindLink(Gdiplus::PointF point)
	{
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Part of temporary fix for slow graphs with lots of links, until we can optimize the intersection tests properly.
		if((GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0)
		{
			return INVALID_INDEX;
		}
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		for(TGraphViewLinkVector::iterator iBeginLink = m_links.begin(), iEndLink = m_links.end(), iLink = iBeginLink; iLink != iEndLink; ++ iLink)
		{
			SGraphViewLink&	link = *iLink;
			if(link.paintRect.Contains(point))
			{
				const CGraphViewNodePtr	pSrcNode = link.pSrcNode.lock();
				const CGraphViewNodePtr	pDstNode = link.pDstNode.lock();
				CRY_ASSERT(pSrcNode && pDstNode);
				if(pSrcNode && pDstNode)
				{
					const size_t	iSrcOutput = pSrcNode->FindOutput(link.srcOutputName);
					const size_t	iDstInput = pDstNode->FindInput(link.dstInputName);
					CRY_ASSERT((iSrcOutput != INVALID_INDEX) && (iDstInput != INVALID_INDEX));
					if((iSrcOutput != INVALID_INDEX) && (iDstInput != INVALID_INDEX))
					{
						// N.B. This hit test is far from optimal and it may be better to implement something along the lines of what is proposed here: https://msdn.microsoft.com/en-us/library/ms969920.aspx.
						Gdiplus::Bitmap	bitmap(static_cast<INT>(link.paintRect.Width), static_cast<INT>(link.paintRect.Height), PixelFormat4bppIndexed);
						if(Gdiplus::Graphics* pGraphics = Gdiplus::Graphics::FromImage(&bitmap))
						{
							const Gdiplus::Color	backgroundColor(255, 0, 0, 0);
							pGraphics->Clear(backgroundColor);

							const Gdiplus::PointF	topLeft(link.paintRect.X, link.paintRect.Y);
							Gdiplus::PointF				startPoint = pSrcNode->GetOutputLinkPoint(iSrcOutput);
							Gdiplus::PointF				endPoint = pDstNode->GetInputLinkPoint(iDstInput);
							Gdiplus::PointF				testPoint = point;
							startPoint.X	-= topLeft.X;
							startPoint.Y	-= topLeft.Y;
							endPoint.X		-= topLeft.X;
							endPoint.Y		-= topLeft.Y;
							testPoint.X		-= topLeft.X;
							testPoint.Y		-= topLeft.Y;
							
							m_pPainter->PaintLink(*pGraphics, startPoint, endPoint, pSrcNode->GetOutputColor(iSrcOutput), link.selected, 1.2f);

							Gdiplus::Color					testColor = backgroundColor;
							const Gdiplus::Status		testStatus = bitmap.GetPixel(static_cast<INT>(testPoint.X), static_cast<INT>(testPoint.Y), &testColor);

							delete pGraphics;

							if((testStatus == Gdiplus::Ok) && (testColor.GetValue() != backgroundColor.GetValue()))
							{
								return iLink - iBeginLink;
							}
						}
					}
				}
			}
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::ClearLinkSelection()
	{
		if(m_iSelectedLink != INVALID_INDEX)
		{
			m_links[m_iSelectedLink].selected	= false;
			m_iSelectedLink										= INVALID_INDEX;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::SelectLink(size_t iLink)
	{
		CRY_ASSERT(iLink != INVALID_INDEX);
		if((iLink != INVALID_INDEX) && (iLink != m_iSelectedLink))
		{
			ClearSelection();
			m_iSelectedLink										= iLink;
			m_links[m_iSelectedLink].selected	= true;
			OnSelection(CGraphViewNodePtr(), &m_links[m_iSelectedLink]);
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CGraphView::GetSelectedLink() const
	{
		return m_iSelectedLink;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::Clear()
	{
		m_nodes.clear();
		m_links.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CGraphView::ClientToGraph(LONG x, LONG y) const
	{
		return Gdiplus::PointF((x + m_scrollOffset.X) / m_zoom, (y + m_scrollOffset.Y) / m_zoom);
	}
	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CGraphView::ClientToGraph(Gdiplus::PointF pos) const
	{
		return Gdiplus::PointF((pos.X + m_scrollOffset.X) / m_zoom, (pos.Y + m_scrollOffset.Y) / m_zoom);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CGraphView::ClientToGraphTransform(Gdiplus::PointF transform) const
	{
		return Gdiplus::PointF(transform.X / m_zoom, transform.Y / m_zoom);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::RectF CGraphView::ClientToGraph(Gdiplus::RectF rect) const
	{
		return Gdiplus::RectF((rect.X + m_scrollOffset.X) / m_zoom, (rect.Y + m_scrollOffset.Y) / m_zoom, rect.Width / m_zoom, rect.Height / m_zoom);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CGraphView::GraphToClient(Gdiplus::PointF pos) const
	{
		return Gdiplus::PointF((pos.X * m_zoom) - m_scrollOffset.X, (pos.Y * m_zoom) - m_scrollOffset.Y);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::PointF CGraphView::GraphToClientTransform(Gdiplus::PointF transform) const
	{
		return Gdiplus::PointF(transform.X * m_zoom, transform.Y * m_zoom);
	}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::RectF CGraphView::GraphToClient(Gdiplus::RectF rect) const
	{
		return Gdiplus::RectF((rect.X * m_zoom) - m_scrollOffset.X, (rect.Y * m_zoom) - m_scrollOffset.Y, rect.Width * m_zoom, rect.Height * m_zoom);
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::ScrollToFit(Gdiplus::RectF rect)
	{
		CRect	clientRect;
		GetClientRect(&clientRect);
		Gdiplus::RectF	graphViewRect = ClientToGraph(Gdiplus::RectF(static_cast<Gdiplus::REAL>(clientRect.left), static_cast<Gdiplus::REAL>(clientRect.top), 
																	 static_cast<Gdiplus::REAL>(clientRect.Width()), static_cast<Gdiplus::REAL>(clientRect.Height())));
		const float			dx = graphViewRect.GetRight() - rect.GetRight();
		if(dx < 0.0f)
		{
			m_scrollOffset.X += (-dx + DEFAULT_NODE_SPACING) * m_zoom;
			OnMove(m_scrollOffset);
		}
		// #SchematycTODO : Check left, top and bottom of view too?
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CenterPosition(Gdiplus::PointF point)
	{
		CRect	clientRect;
		GetClientRect(&clientRect);
		Gdiplus::RectF	graphViewRect = ClientToGraph(Gdiplus::RectF(static_cast<Gdiplus::REAL>(clientRect.left), static_cast<Gdiplus::REAL>(clientRect.top), 
																	 static_cast<Gdiplus::REAL>(clientRect.Width()), static_cast<Gdiplus::REAL>(clientRect.Height())));
		const float	dx = graphViewRect.X - point.X + (graphViewRect.Width/2);
		const float dy = graphViewRect.Y - point.Y + (graphViewRect.Height/2);

		m_scrollOffset.X += (-dx + DEFAULT_NODE_SPACING) * m_zoom;
		m_scrollOffset.Y += (-dy + DEFAULT_NODE_SPACING) * m_zoom;

		OnMove(m_scrollOffset);
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CGraphView::PreTranslateMessage(MSG* pMsg)
	{
		if((pMsg->message == WM_KEYDOWN) && (GetFocus() == this))
		{
			OnKeyDown((UINT)pMsg->wParam, LOWORD(pMsg->lParam), HIWORD(pMsg->lParam));
			return TRUE;
		}
		else
		{
			return __super::PreTranslateMessage(pMsg);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnDraw(CDC* pDC) {}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnKillFocus(CWnd* pNewWnd)
	{
		ExitDragState();
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnPaint()
	{
		if(m_pPainter)
		{
			CPaintDC				paintDC(this);
			const CRect			paintRect = paintDC.m_ps.rcPaint;
			Gdiplus::Bitmap	backBuffer(paintRect.Width(), paintRect.Height());
			if(Gdiplus::Graphics* pBackBufferGraphics = Gdiplus::Graphics::FromImage(&backBuffer))
			{
				pBackBufferGraphics->TranslateTransform(-m_scrollOffset.X, -m_scrollOffset.Y);
				pBackBufferGraphics->ScaleTransform(m_zoom, m_zoom);
				pBackBufferGraphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
				pBackBufferGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
				pBackBufferGraphics->Clear(m_pPainter->GetBackgroundColor());

				if(m_enabled)
				{
					m_pPainter->PaintGrid(*pBackBufferGraphics, m_grid);

					for(TGraphViewNodePtrVector::iterator iNode = m_nodes.begin(), iEndNode = m_nodes.end(); iNode != iEndNode; ++ iNode)
					{
						m_pPainter->UpdateNodePaintRect(*pBackBufferGraphics, m_grid, *(*iNode));
					}

					for(TGraphViewLinkVector::iterator iLink = m_links.begin(), iEndLink = m_links.end(); iLink != iEndLink; ++ iLink)
					{
						SGraphViewLink&         link = *iLink;
						const CGraphViewNodePtr pSrcNode = link.pSrcNode.lock();
						const CGraphViewNodePtr pDstNode = link.pDstNode.lock();
						if(pSrcNode && pDstNode)
						{
							const size_t srcOutputIdx = pSrcNode->FindOutput(link.srcOutputName);
							const size_t dstInputIdx = pDstNode->FindInput(link.dstInputName);
							if((srcOutputIdx != INVALID_INDEX) && (dstInputIdx != INVALID_INDEX))
							{
								link.paintRect = m_pPainter->PaintLink(*pBackBufferGraphics, pSrcNode->GetOutputLinkPoint(srcOutputIdx), pDstNode->GetInputLinkPoint(dstInputIdx), pSrcNode->GetOutputColor(srcOutputIdx), link.selected, 1.0f);
							}
						}
					}

					if(m_pDragState != NULL)
					{
						m_pDragState->Paint(*this, *pBackBufferGraphics, *m_pPainter);
					}

					for(TGraphViewNodePtrVector::reverse_iterator iNode = m_nodes.rbegin(), iEndNode = m_nodes.rend(); iNode != iEndNode; ++ iNode)
					{
						m_pPainter->PaintNode(*pBackBufferGraphics, m_grid, *(*iNode));
					}
				}

				Gdiplus::Graphics	mainGraphics(paintDC.GetSafeHdc());
				mainGraphics.DrawImage(&backBuffer, paintRect.left, paintRect.top, paintRect.Width(), paintRect.Height());

				delete pBackBufferGraphics;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnMouseMove(UINT nFlags, CPoint point)
	{
		if(m_enabled && (GetFocus() == this))
		{
			if(m_pDragState)
			{
				m_pDragState->Update(*this, Gdiplus::PointF(static_cast<float>(point.x), static_cast<float>(point.y)));
				__super::Invalidate(TRUE);
			}
			else if((point.x != m_prevMousePoint.x) || (point.y != m_prevMousePoint.y))
			{
				if((nFlags & MK_LBUTTON) != 0)
				{
					OnLeftMouseButtonDrag(m_prevMousePoint);
				}
				else if((nFlags & MK_RBUTTON) != 0)
				{
					OnRightMouseButtonDrag(m_prevMousePoint);
				}
				else if((nFlags & MK_MBUTTON) != 0)
				{
					OnMiddleMouseButtonDrag(m_prevMousePoint);
				}
			}
		}
		m_prevMousePoint = point;
		__super::OnMouseMove(nFlags, point);
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CGraphView::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
	{
		// Store previous focus.
		CPoint	cursorPoint;
		GetCursorPos(&cursorPoint);
		ScreenToClient(&cursorPoint);
		const Gdiplus::PointF	prevClientFocus((Gdiplus::REAL)cursorPoint.x, (Gdiplus::REAL)cursorPoint.y);
		const Gdiplus::PointF	prevGraphFocus = ClientToGraph(prevClientFocus);
		// Increment zoom.
		m_zoom = clamp_tpl(m_zoom + (DELTA_ZOOM * zDelta), MIN_ZOOM, MAX_ZOOM);
		// Scroll to previous focus point.
		const Gdiplus::PointF	newClientFocus = GraphToClient(prevGraphFocus);
		m_scrollOffset.X	+= newClientFocus.X - prevClientFocus.X;
		m_scrollOffset.Y	+= newClientFocus.Y - prevClientFocus.Y;
		OnMove(m_scrollOffset);
		// Force re-draw.
		__super::Invalidate(TRUE);
		return TRUE;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnLButtonDown(UINT nFlags, CPoint point) 
	{
		SetFocus();
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnLButtonUp(UINT nFlags, CPoint point) 
	{
		if(m_pDragState != NULL)
		{
			ExitDragState();
		}
		else
		{
			ClearSelection();
			const Gdiplus::PointF	graphPos = ClientToGraph(point.x, point.y);
			CGraphViewNodePtr			pNode = FindNode(graphPos);
			if(pNode != NULL)
			{
				SelectNode(pNode);
			}
			else
			{
				const size_t	iLink = FindLink(graphPos);
				if(iLink != INVALID_INDEX)
				{
					SelectLink(iLink);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	afx_msg void CGraphView::OnRButtonDown(UINT nFlags, CPoint point)
	{
		SetFocus();
	}

	//////////////////////////////////////////////////////////////////////////
	afx_msg void CGraphView::OnRButtonUp(UINT nFlags, CPoint point)
	{
		if(m_pDragState != NULL)
		{
			ExitDragState();
		}
		else
		{
			CMenu	popupMenu;
			popupMenu.CreatePopupMenu();
			const Gdiplus::PointF	graphPos = ClientToGraph(point.x, point.y);
			CGraphViewNodePtr			pNode = FindNode(graphPos);
			const size_t					iNodeInput = pNode ? pNode->FindInput(graphPos) : INVALID_INDEX;
			const size_t					iNodeOutput = pNode ? pNode->FindOutput(graphPos) : INVALID_INDEX;
			ClientToScreen(&point);
			GetPopupMenuItems(popupMenu, pNode, iNodeInput, iNodeOutput, point);
			const int	popupMenuItemCount = popupMenu.GetMenuItemCount();
			if(popupMenuItemCount == 1)
			{
				OnPopupMenuResult(popupMenu.GetMenuItemID(0), pNode, iNodeInput, iNodeOutput, point);
			}
			else if(popupMenuItemCount > 0)
			{
				int	popupMenuItem = popupMenu.TrackPopupMenuEx(TPM_RETURNCMD, point.x, point.y, this, NULL);
				OnPopupMenuResult(popupMenuItem, pNode, iNodeInput, iNodeOutput, point);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	afx_msg void CGraphView::OnMButtonDown(UINT nFlags, CPoint point)
	{
		SetFocus();
	}

	//////////////////////////////////////////////////////////////////////////
	afx_msg void CGraphView::OnMButtonUp(UINT nFlags, CPoint point)
	{
		if(m_pDragState != NULL)
		{
			ExitDragState();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
	{
		if((GetFocus() == this) && m_enabled && (nRepCnt == 1))
		{
			const bool bCtrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			const bool bShiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			if(bCtrlDown)
			{
				switch(nChar)
				{
				case 'F':
					{
						FormatSelectedNodes();
						return;
					}
				}
			}
			else if (bShiftDown)
			{
				switch (nChar)
				{
				case 'W':
					{
						OnUpArrowPressed();
						return;
					}
				case 'S':
				{
					OnDownArrowPressed();
					return;
				}
				}
			}
			else
			{
				switch(nChar)
				{
					///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					// Part of temporary fix for slow graphs with lots of links, until we can optimize the intersection tests properly.
				case VK_SHIFT:
					{
						UpdateCursor();
						return;
					}
					///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				case VK_DELETE:
					{
						OnDelete();
						return;
					}
				case VK_UP:
					{
						OnUpArrowPressed();
						return;
					}
				case VK_DOWN:
					{
						OnDownArrowPressed();
						return;
					}
				}
				if((nChar >= 'A') && (nChar <= 'Z'))
				{ 
					const char defaultFilter[2] = { static_cast<char>(nChar), '\0' };
					if(m_selectedNodes.size() == 1)
					{
						CGraphViewNodePtr pSelectedNode = m_selectedNodes[0];
						Gdiplus::RectF    paintRect = pSelectedNode->GetPaintRect();
						Gdiplus::PointF   nextPos = GraphToClient(Gdiplus::PointF(paintRect.GetRight() + DEFAULT_NODE_SPACING, paintRect.GetTop()));
						CPoint            screenPoint(static_cast<int>(nextPos.X), static_cast<int>(nextPos.Y));
						ClientToScreen(&screenPoint);
						DoQuickSearch(defaultFilter, pSelectedNode, 0, &screenPoint); // Rather than passing zero it would be nice to pass the index of the first/selected execution output.
					}
					else
					{
						DoQuickSearch(defaultFilter);
					}
					return;
				}
			}
		}
		__super::OnKeyDown(nChar, nRepCnt, nFlags);
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
	{
		if((GetFocus() == this) && m_enabled && (nRepCnt == 1))
		{
			switch(nChar)
			{
			///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// Part of temporary fix for slow graphs with lots of links, until we can optimize the intersection tests properly.
			case VK_SHIFT:
				{
					SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
					return;
				}
			///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			}
		}
		__super::OnKeyUp(nChar, nRepCnt, nFlags);
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CGraphView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
	{
		if(UpdateCursor())
		{
			return TRUE;
		}
		return __super::OnSetCursor(pWnd, nHitTest, message);
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnMove(Gdiplus::PointF scrollOffset) {}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnSelection(const CGraphViewNodePtr& pSelectedNode, SGraphViewLink* pSelectedLink) {}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnLinkModify(const SGraphViewLink& link) {}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnNodeRemoved(const CGraphViewNode& node) {}

	//////////////////////////////////////////////////////////////////////////
	bool CGraphView::CanCreateLink(const CGraphViewNode& srcNode, const char* srcOutputName, const CGraphViewNode& dstNode, const char* dstInputName) const
	{
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnLinkCreated(const SGraphViewLink& link) {}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnLinkRemoved(const SGraphViewLink& link) {}
	
	//////////////////////////////////////////////////////////////////////////
	DROPEFFECT CGraphView::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		return DROPEFFECT_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CGraphView::OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::GetPopupMenuItems(CMenu& popupMenu, const CGraphViewNodePtr& pNode, size_t iNodeInput, size_t iNodeOutput, CPoint point)
	{
		if(pNode)
		{
			popupMenu.AppendMenu(MF_STRING, EPopupMenuItem::REMOVE_NODE, "Remove Node");
		}
		else
		{
			popupMenu.AppendMenu(MF_STRING, EPopupMenuItem::QUICK_SEARCH, "Quick Search");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnPopupMenuResult(BOOL popupMenuItem, const CGraphViewNodePtr& pNode, size_t iNodeInput, size_t iNodeOutput, CPoint point)
	{
		switch(popupMenuItem)
		{
			case EPopupMenuItem::QUICK_SEARCH:
			{
				DoQuickSearch();
				break;
			}
			case EPopupMenuItem::REMOVE_NODE:
			{
				CRY_ASSERT(pNode != NULL);
				RemoveNode(pNode);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	const IQuickSearchOptions* CGraphView::GetQuickSearchOptions(CPoint point, const CGraphViewNodePtr& pNode, size_t iNodeOutput)
	{
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnQuickSearchResult(CPoint point, const CGraphViewNodePtr& pNode, size_t iNodeOutput, size_t iSelectedOption) {}

	//////////////////////////////////////////////////////////////////////////
	bool CGraphView::ShouldUpdate() const
	{
		return GetFocus() == this;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnUpdate() {}

	//////////////////////////////////////////////////////////////////////////
	CGraphView::CScrollDragState::CScrollDragState(Gdiplus::PointF pos)
		: m_prevPos(pos)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CScrollDragState::Update(CGraphView& graphView, Gdiplus::PointF pos)
	{
		if((pos.X != m_prevPos.X) || (pos.Y != m_prevPos.Y))
		{
			graphView.Scroll(pos - m_prevPos);
			m_prevPos = pos;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CScrollDragState::Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter) {}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CScrollDragState::Exit(CGraphView& graphView) {}

	//////////////////////////////////////////////////////////////////////////
	CGraphView::CMoveDragState::CMoveDragState(Gdiplus::PointF pos)
		: m_prevPos(pos)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CMoveDragState::Update(CGraphView& graphView, Gdiplus::PointF pos)
	{
		graphView.MoveSelectedNodes(graphView.ClientToGraphTransform(pos - m_prevPos), false);
		m_prevPos = pos;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CMoveDragState::Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter) {}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CMoveDragState::Exit(CGraphView& graphView)
	{
		graphView.MoveSelectedNodes(Gdiplus::PointF(), true);
	}

	//////////////////////////////////////////////////////////////////////////
	CGraphView::CLinkDragState::CLinkDragState(const CGraphViewNodeWeakPtr& pSrcNode, const char* srcOutputName, Gdiplus::PointF start, Gdiplus::PointF end, Gdiplus::Color color, bool newLink)
		: m_pSrcNode(pSrcNode)
		, m_srcOutputName(srcOutputName)
		, m_start(start)
		, m_end(end)
		, m_color(color)
		, m_newLink(newLink)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CLinkDragState::Update(CGraphView& graphView, Gdiplus::PointF pos)
	{
		m_end = pos;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CLinkDragState::Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter)
	{
		if(CGraphViewNodePtr pSrcNode = m_pSrcNode.lock())
		{
			painter.PaintLink(graphics, m_start, graphView.ClientToGraph(m_end), m_color, true, 1.0f);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CLinkDragState::Exit(CGraphView& graphView)
	{
		const Gdiplus::PointF	graphPos = graphView.ClientToGraph(m_end);
		if(CGraphViewNodePtr pDstNode = graphView.FindNode(graphPos))
		{
			const size_t	iDstInput = pDstNode->FindInput(graphPos);
			if(iDstInput != INVALID_INDEX)
			{
				graphView.CreateLink(m_pSrcNode.lock(), m_srcOutputName.c_str(), pDstNode, pDstNode->GetInputName(iDstInput));
			}
		}
		else if(m_newLink)
		{
			CGraphViewNodePtr	pSrcNode = m_pSrcNode.lock();
			CRY_ASSERT(pSrcNode);
			if(pSrcNode)
			{
				graphView.DoQuickSearch(NULL, pSrcNode, pSrcNode->FindOutput(m_srcOutputName.c_str()));
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CGraphView::CSelectDragState::CSelectDragState(Gdiplus::PointF pos)
		: m_startPos(pos)
		, m_endPos(pos)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CSelectDragState::Update(CGraphView& graphView, Gdiplus::PointF pos)
	{
		m_endPos = pos;
		graphView.SelectNodesInRect(graphView.ClientToGraph(GetSelectionRect()));
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CSelectDragState::Paint(CGraphView& graphView, Gdiplus::Graphics& graphics, IGraphViewPainter& painter)
	{
		painter.PaintSelectionRect(graphics, graphView.ClientToGraph(GetSelectionRect()));
	}
	
	//////////////////////////////////////////////////////////////////////////
	void CGraphView::CSelectDragState::Exit(CGraphView& graphView) {}

	//////////////////////////////////////////////////////////////////////////
	Gdiplus::RectF CGraphView::CSelectDragState::GetSelectionRect() const
	{
		Gdiplus::RectF	selectionRect;
		selectionRect.X				= std::min(m_startPos.X, m_endPos.X);
		selectionRect.Y				= std::min(m_startPos.Y, m_endPos.Y);
		selectionRect.Width		= fabs_tpl(m_endPos.X - m_startPos.X);
		selectionRect.Height	= fabs_tpl(m_endPos.Y - m_startPos.Y);
		return selectionRect;
	}

	//////////////////////////////////////////////////////////////////////////
	CGraphView::CDropTarget::CDropTarget(CGraphView& graphView)
		: m_graphView(graphView)
	{}

	//////////////////////////////////////////////////////////////////////////
	DROPEFFECT CGraphView::CDropTarget::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		return m_graphView.OnDragOver(pWnd, pDataObject, dwKeyState, point);
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CGraphView::CDropTarget::OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		return m_graphView.OnDrop(pWnd, pDataObject, dropEffect, point);
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnTimer(UINT_PTR nIDEvent)
	{
		if(nIDEvent == UPDATE_TIMER_EVENT_ID)
		{
			if(ShouldUpdate())
			{
				OnUpdate();
				//if(!m_selectedNodes.empty() || (m_iSelectedLink != INVALID_INDEX))	// Removed as part of temporary fix for slow graphs with lots of links, until we can optimize the intersection tests properly.
				{
					__super::Invalidate(TRUE);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnLeftMouseButtonDrag(CPoint point)
	{
		const Gdiplus::PointF	clientPos(static_cast<float>(point.x), static_cast<float>(point.y));
		const Gdiplus::PointF	graphPos = ClientToGraph(clientPos);
		if(CGraphViewNodePtr pNode = FindNode(graphPos))
		{
			if(m_selectedNodes.size() < 2)
			{
				ClearSelection();
				SelectNode(pNode);
			}
			const size_t iSrcOutput = pNode->FindOutput(graphPos);
			if(iSrcOutput != INVALID_INDEX)
			{
				m_pDragState = IGraphViewDragStatePtr(new CLinkDragState(pNode, pNode->GetOutputName(iSrcOutput), pNode->GetOutputLinkPoint(iSrcOutput), clientPos, pNode->GetOutputColor(iSrcOutput), true));
			}
			else
			{
				const size_t iDstInput = pNode->FindInput(graphPos);
				if(iDstInput != INVALID_INDEX)
				{
					const char*		dstInputName = pNode->GetInputName(iDstInput);
					TSizeTVector	links;
					FindLinks(pNode, dstInputName, links);
					if(links.size() == 1)
					{
						const size_t					iLink = links.front();
						const SGraphViewLink&	link = m_links[iLink];
						if(CGraphViewNodePtr pSrcNode = link.pSrcNode.lock())
						{
							const size_t	iLinkSrcOutput = pSrcNode->FindOutput(link.srcOutputName);
							CRY_ASSERT(iLinkSrcOutput != INVALID_INDEX);
							if(iLinkSrcOutput != INVALID_INDEX)
							{
								m_pDragState = IGraphViewDragStatePtr(new CLinkDragState(link.pSrcNode, link.srcOutputName, pSrcNode->GetOutputLinkPoint(iLinkSrcOutput), clientPos, pSrcNode->GetOutputColor(iLinkSrcOutput), false));
							}
						}
						RemoveLink(iLink);
					}
				}
				else
				{
					const Gdiplus::RectF	nodePaintRect = pNode->GetPaintRect();
					m_pDragState = IGraphViewDragStatePtr(new CMoveDragState(clientPos));
				}
			}
		}
		else
		{
			m_pDragState = IGraphViewDragStatePtr(new CSelectDragState(clientPos));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnRightMouseButtonDrag(CPoint point)
	{
		m_pDragState = IGraphViewDragStatePtr(new CScrollDragState(Gdiplus::PointF(static_cast<float>(point.x), static_cast<float>(point.y))));
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnMiddleMouseButtonDrag(CPoint point)
	{
		m_pDragState = IGraphViewDragStatePtr(new CScrollDragState(Gdiplus::PointF(static_cast<float>(point.x), static_cast<float>(point.y))));
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::Scroll(Gdiplus::PointF delta)
	{
		m_scrollOffset.X	= clamp_tpl(m_scrollOffset.X - delta.X, m_grid.bounds.X, m_grid.bounds.GetRight());
		m_scrollOffset.Y	= clamp_tpl(m_scrollOffset.Y - delta.Y, m_grid.bounds.Y, m_grid.bounds.GetBottom());
		OnMove(m_scrollOffset);
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::ExitDragState()
	{
		if(m_pDragState != NULL)
		{
			IGraphViewDragStatePtr	pDragState = m_pDragState;
			m_pDragState.reset();
			pDragState->Exit(*this);
			__super::Invalidate(TRUE);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::OnDelete()
	{
		if(m_selectedNodes.empty() == false)
		{
			// TODO: Switch to CryMessageBox when porting to Qt, we cannot use it right now due to a MFC / Qt conflict.
			if (::MessageBoxA(CView::GetSafeHwnd(), "Remove selected node(s)?", "Remove Node(s)", 1) == IDOK)
			{
				TGraphViewNodePtrVector	removeNodes = m_selectedNodes;
				for(TGraphViewNodePtrVector::iterator iRemoveNode = removeNodes.begin(), iEndRemoveNode = removeNodes.end(); iRemoveNode != iEndRemoveNode; ++ iRemoveNode)
				{
					RemoveNode(*iRemoveNode);
				}
			}
		}
		else if(m_iSelectedLink)
		{
			// TODO: Switch to CryMessageBox when porting to Qt, we cannot use it right now due to a MFC / Qt conflict.
			if (::MessageBoxA(CView::GetSafeHwnd(), "Remove selected link?", "Remove Link", 1) == IDOK)
			{
				RemoveLink(m_iSelectedLink);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CGraphView::UpdateCursor()
	{
		if(!m_pDragState)
		{
			CPoint	cursorPoint;
			GetCursorPos(&cursorPoint);
			ScreenToClient(&cursorPoint);
			const Gdiplus::PointF	graphPos = ClientToGraph(Gdiplus::PointF(static_cast<float>(cursorPoint.x), static_cast<float>(cursorPoint.y)));
			if(FindNode(graphPos) || (FindLink(graphPos) != INVALID_INDEX))
			{
				SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CGraphView::FormatSelectedNodes()
	{
		if(GetSchematyc()->IsExperimentalFeatureEnabled("GraphNodeFormat"))
		{
			CGraphViewFormatter formatter(*this, m_settings.format);
			formatter.FormatNodes(m_selectedNodes);
		}
	}
}
