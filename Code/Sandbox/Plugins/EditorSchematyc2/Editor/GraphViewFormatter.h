// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "GraphView.h"

namespace Schematyc2
{
	class CGraphViewFormatter
	{
	public:

		CGraphViewFormatter(const CGraphView& graphView, const SGraphViewFormatSettings& settings);

		void FormatNodes(TGraphViewNodePtrVector& nodes) const;

	private:

		bool IsBeginNode(const CGraphViewNodeConstPtr& pNode) const;
		void FormatNodeForwardsRecursive(TGraphViewNodePtrVector& unformattedNodes, TGraphViewNodePtrVector& formattedNodes, const CGraphViewNodePtr& pNode) const;
		void FormatNodeBackwardsRecursive(TGraphViewNodePtrVector& unformattedNodes, TGraphViewNodePtrVector& formattedNodes, const CGraphViewNodePtr& pNode) const;
		void FormatNode(TGraphViewNodePtrVector& unformattedNodes, TGraphViewNodePtrVector& formattedNodes, const CGraphViewNodePtr& pNode, Gdiplus::PointF pos) const;

	private:

		const CGraphView&               m_graphView;
		const SGraphViewFormatSettings& m_settings;
	};
}
