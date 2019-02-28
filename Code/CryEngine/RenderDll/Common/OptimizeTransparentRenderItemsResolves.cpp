// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RenderView.h"
#include "CompiledRenderObject.h"

#include <CryCore/Containers/CryArray.h>
#include <CryMath/Cry_Geo.h>
#include <vector>

namespace renderitems_topological_sort
{

using bounds_t = TRect_tpl<uint16>;
struct node
{
	using edge_t = node * ;

	const SRendItem item;
	const bounds_t bounds;
	const bool requiresResolve;

	std::vector<edge_t> out;
	std::uint32_t in_edges_count = 0;

	node(const SRendItem &item, const bounds_t &bounds, const bool requiresResolve) : item(item), bounds(bounds), requiresResolve(requiresResolve) {}
};
using nodes_t = std::vector<node>;
using roots_t = LocalDynArray<node*, 8>;

inline bool distinct(const node &lhs, const node &rhs) noexcept
{
	return
		lhs.bounds.Min.x > rhs.bounds.Max.x ||
		rhs.bounds.Min.x > lhs.bounds.Max.x ||
		lhs.bounds.Min.y > rhs.bounds.Max.y ||
		rhs.bounds.Min.y > lhs.bounds.Max.y;
}

inline bounds_t merge_bounds(const bounds_t &lhs, const bounds_t &rhs) noexcept
{
	return bounds_t{
		std::min(lhs.Min.x, rhs.Min.x),
		std::min(lhs.Min.y, rhs.Min.y),
		std::max(lhs.Max.x, rhs.Max.x),
		std::max(lhs.Max.y, rhs.Max.y)
	};
}

inline void add_edge(node &from, node &to) noexcept
{
	from.out.emplace_back(&to);
	++to.in_edges_count;
}

inline void detach_edge(const node::edge_t &e) noexcept
{
	--e->in_edges_count;
}

inline void connect(nodes_t &nodes, roots_t &roots) noexcept
{
	// For all pairs (i,j) where j>i: i->j iff i overlaps j
	const auto end = nodes.data() + nodes.size();
	for (auto &i : nodes)
	{
		auto *j = &i + 1;
		for (; j != end; ++j)
		{
			if (!distinct(i, *j))
				add_edge(i, *j);
		}

		// Store roots
		if (!i.in_edges_count)
			roots.push_back(&i);
	}
}

template <typename F>
inline void process_root(const node* r, roots_t &new_roots, const F &f) noexcept
{
	f(*r);

	// Erase all of r's outwards edges and store new roots
	for (auto &edge : r->out)
	{
		detach_edge(edge);
		if (!edge->in_edges_count)
			new_roots.push_back(edge);
		CRY_ASSERT(edge->in_edges_count >= 0);
	}
}

template <typename F, typename R>
inline void topological(nodes_t &nodes, roots_t &roots, std::size_t min_resolve_area, const F &f, const R &r) noexcept
{
	roots_t roots_pending_resolve;
	std::vector<bounds_t> resolve_rects;

	for (; roots.size() || roots_pending_resolve.size();)
	{
		if (!roots.size())
		{
			// Resolve
			r(std::move(resolve_rects));
			// Process roots
			for (const auto *n : roots_pending_resolve)
				process_root(n, roots, f);

			roots_pending_resolve.clear();
			resolve_rects.clear();
		}

		// Process roots
		roots_t new_roots;
		for (auto *rt : roots)
		{
			const auto area = static_cast<std::size_t>(rt->bounds.GetWidth()) * rt->bounds.GetHeight();

			if (rt->requiresResolve && area >= min_resolve_area)
			{
				roots_pending_resolve.push_back(rt);
				resolve_rects.push_back(rt->bounds);
			}
			else
				process_root(rt, new_roots, f);
		}
		roots = std::move(new_roots);
	}
}

}

std::size_t CRenderView::OptimizeTransparentRenderItemsResolves(STransparentSegments &segments, RenderItems &renderItems, std::size_t resolves_count) const
{
	using namespace renderitems_topological_sort;

	PROFILE_FRAME(OptimizeTransparentRenderItemsResolves);

	const auto refractionMask = FB_REFRACTION | FB_RESOLVE_FULL;
	const auto max_resolves = CRendererCVars::CV_r_RefractionPartialResolveMaxResolveCount;
	const auto min_reslolve_area = CRendererCVars::CV_r_RefractionPartialResolveMinimalResolveArea;

	CRY_ASSERT_MESSAGE(CRendererCVars::CV_r_RefractionPartialResolveMode == 1 || CRendererCVars::CV_r_RefractionPartialResolveMode == 2,
		"Unknown value for r_RefractionPartialResolveMode, defaulting to 2.");

	if (CRendererCVars::CV_r_RefractionPartialResolveMode == 1)
	{
		for (auto i = 0; i < renderItems.size();)
		{
			const auto& item = renderItems[i];
			const bool needsResolve = !!(item.nBatchFlags & refractionMask);

			STransparentSegment segment;
			segment.rendItems.start = i;
			while (++i < renderItems.size() && !(renderItems[i].nBatchFlags & refractionMask)) {}
			segment.rendItems.end = i;

			if (needsResolve)
			{
				const bool forceFullResolve = !!(item.nBatchFlags & FB_RESOLVE_FULL);
				const auto bounds = item.pCompiledObject->m_aabb.IsEmpty() ?
					bounds_t{ 0,0,0,0 } :
					ComputeResolveViewport(item.pCompiledObject->m_aabb, forceFullResolve);
				segment.resolveRects.push_back(bounds);
			}

			segments.emplace_back(std::move(segment));
		}
	}
	else
	{
		// Create nodes
		std::vector<node> nodes;
		for (const auto &item : renderItems)
		{
			const bool forceFullResolve = !!(item.nBatchFlags & FB_RESOLVE_FULL);
			const auto bounds = item.pCompiledObject->m_aabb.IsEmpty() ?
				bounds_t{ 0,0,0,0 } :
				ComputeResolveViewport(item.pCompiledObject->m_aabb, forceFullResolve);
			nodes.emplace_back(node{ item, bounds, !!(item.nBatchFlags & refractionMask) });
		}

		// Create directed, acyclic graph(s)
		roots_t roots;
		connect(nodes, roots);

		// Topological sort
		std::size_t start = 0, end = 0;
		topological(nodes, roots, min_reslolve_area,
			[&](const node& n) { renderItems[end++] = n.item; },
			[&](std::vector<bounds_t> &&resolve_rects)
			{
				if (segments.size())
				{
					// Finish previous segment
					segments.back().rendItems.start = start;
					segments.back().rendItems.end = end;

					start = end;
				}

				// Append a segment
				STransparentSegment segment;
				segment.resolveRects = std::move(resolve_rects);
				segments.emplace_back(std::move(segment));
			}
		);

		CRY_ASSERT(end == renderItems.size());

		if (end > start)
		{
			if (!segments.size())
				segments.emplace_back(STransparentSegment{});
			segments.back().rendItems.start = start;
			segments.back().rendItems.end = end;
		}
	}

	// If we have an upper limit on maximum resolves, we do another pass to enforce it.
	if (max_resolves > 0)
	{
		// Iterate in reverse (from closest resolves to furthest away ones)
		auto it = segments.rbegin();
		for (; it != segments.rend() && resolves_count<max_resolves; ++it)
			if (it->resolveRects.size())
				++resolves_count;
		if (it != segments.rend())
		{
			// Merge the rest of the resolves and render-items lists into one
			bounds_t final_bounds = it->resolveRects[0];
			const auto final_rend_items_end = it->rendItems.end;
			for (auto jt = it; jt != segments.rend(); ++jt)
			{
				for (const auto& b : jt->resolveRects)
					final_bounds = merge_bounds(final_bounds, b);
				jt->resolveRects = {};
				jt->rendItems = {};

			}

			it->resolveRects.push_back(final_bounds);
			it->rendItems.end = final_rend_items_end;
		}
	}

	return resolves_count;
}

void CRenderView::StartOptimizeTransparentRenderItemsResolvesJob()
{
	if (!CRendererCVars::CV_r_Refraction || CRendererCVars::CV_r_RefractionPartialResolveMode == 0)
		return;

	gEnv->pJobManager->AddLambdaJob("OptimizeTransparentRenderItemsResolvesJob", [this]()
		{
			static constexpr ERenderListID refractiveLists[] = { EFSLIST_TRANSP_BW, EFSLIST_TRANSP_AW, EFSLIST_TRANSP_NEAREST };

			std::size_t resolves_count = 0;
			for (const auto &list : refractiveLists)
			{
				if (!HasResolveForList(list))
					continue;

				auto& segments = GetTransparentSegments(list);
				auto& renderItems = GetRenderItems(list);

				// Clear list
				segments = {};
				// Pre-compute segments
				resolves_count = OptimizeTransparentRenderItemsResolves(segments, renderItems, resolves_count);
			}
		},
		JobManager::eRegularPriority, &m_optimizeTransparentRenderItemsResolvesJobStatus);
}

void CRenderView::WaitForOptimizeTransparentRenderItemsResolvesJob() const
{
	CRY_PROFILE_FUNCTION_WAITING(PROFILE_RENDERER);
	if (m_optimizeTransparentRenderItemsResolvesJobStatus.IsRunning())
		m_optimizeTransparentRenderItemsResolvesJobStatus.Wait();
}
