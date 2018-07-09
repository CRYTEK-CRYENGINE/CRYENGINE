// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Shadow_Renderer.h"

#include "RenderView.h"

#include "CompiledRenderObject.h"

#include <functional>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
// sort operators for render items
struct SCompareItemPreprocess
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		uint64 sortValA(a.SortVal);
		uint64 sortValB(b.SortVal);
		uint64 batchFlagsA(a.nBatchFlags);
		uint64 batchFlagsB(b.nBatchFlags);

		uint64 sortAFusion = (batchFlagsA << 32) + (sortValA << 0);
		uint64 sortBFusion = (batchFlagsB << 32) + (sortValB << 0);

		if (sortAFusion != sortBFusion)
			return sortAFusion < sortBFusion;

		return a.rendItemSorter < b.rendItemSorter;
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareRendItem
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		int nMotionVectorsA = (a.ObjSort & FOB_HAS_PREVMATRIX);
		int nMotionVectorsB = (b.ObjSort & FOB_HAS_PREVMATRIX);
		if (nMotionVectorsA != nMotionVectorsB)
			return nMotionVectorsA > nMotionVectorsB;

		int nAlphaTestA = (a.ObjSort & FOB_ALPHATEST);
		int nAlphaTestB = (b.ObjSort & FOB_ALPHATEST);
		if (nAlphaTestA != nAlphaTestB)
			return nAlphaTestA < nAlphaTestB;

		if (a.SortVal != b.SortVal)         // Sort by shaders
			return a.SortVal < b.SortVal;

		if (a.pElem != b.pElem)               // Sort by geometry
			return a.pElem < b.pElem;

		return (a.ObjSort & 0xFFFF) < (b.ObjSort & 0xFFFF);   // Sort by distance
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareRendItemZPass
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		const int layerSize = 50;  // Note: ObjSort contains round(entityDist * 2) for meshes

		int nMotionVectorsA = (a.ObjSort & FOB_HAS_PREVMATRIX);
		int nMotionVectorsB = (b.ObjSort & FOB_HAS_PREVMATRIX);
		if (nMotionVectorsA != nMotionVectorsB)
			return nMotionVectorsA > nMotionVectorsB;

		int nAlphaTestA = (a.ObjSort & FOB_ALPHATEST);
		int nAlphaTestB = (b.ObjSort & FOB_ALPHATEST);
		if (nAlphaTestA != nAlphaTestB)
			return nAlphaTestA < nAlphaTestB;

		// Sort by depth/distance layers
		int depthLayerA = (a.ObjSort & 0xFFFF) / layerSize;
		int depthLayerB = (b.ObjSort & 0xFFFF) / layerSize;
		if (depthLayerA != depthLayerB)
			return depthLayerA < depthLayerB;

		if (a.SortVal != b.SortVal)    // Sort by shaders
			return a.SortVal < b.SortVal;

		// Sorting by geometry less important than sorting by shaders
		if (a.pElem != b.pElem)    // Sort by geometry
			return a.pElem < b.pElem;

		return (a.ObjSort & 0xFFFF) < (b.ObjSort & 0xFFFF);    // Sort by distance
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareItem_Decal
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		uint64 sortValA(a.SortVal);
		uint64 sortValB(b.SortVal);
		uint64 objSortA_Distance(a.ObjSort & 0xFFFF);
		uint64 objSortB_Distance(b.ObjSort & 0xFFFF);
		uint64 objSortA_ObjFlags(a.ObjSort & ~0xFFFF);
		uint64 objSortB_ObjFlags(b.ObjSort & ~0xFFFF);

		uint64 objSortAFusion = (objSortA_Distance << 48) + (sortValA << 16) + (objSortA_ObjFlags << 0);
		uint64 objSortBFusion = (objSortB_Distance << 48) + (sortValB << 16) + (objSortB_ObjFlags << 0);

		return objSortAFusion < objSortBFusion;
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareItem_NoPtrCompare
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		if (a.ObjSort != b.ObjSort)
			return a.ObjSort < b.ObjSort;

		float pSurfTypeA = ((float*)a.pElem->m_CustomData)[8];
		float pSurfTypeB = ((float*)b.pElem->m_CustomData)[8];
		if (pSurfTypeA != pSurfTypeB)
			return (pSurfTypeA < pSurfTypeB);

		pSurfTypeA = ((float*)a.pElem->m_CustomData)[9];
		pSurfTypeB = ((float*)b.pElem->m_CustomData)[9];
		if (pSurfTypeA != pSurfTypeB)
			return (pSurfTypeA < pSurfTypeB);

		pSurfTypeA = ((float*)a.pElem->m_CustomData)[11];
		pSurfTypeB = ((float*)b.pElem->m_CustomData)[11];
		return (pSurfTypeA < pSurfTypeB);
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareDist
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		if (a.fDist == b.fDist)
			return a.rendItemSorter.ParticleCounter() < b.rendItemSorter.ParticleCounter();

		return (a.fDist > b.fDist);
	}
};

///////////////////////////////////////////////////////////////////////////////
struct SCompareDistInverted
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		if (a.fDist == b.fDist)
			return a.rendItemSorter.ParticleCounter() > b.rendItemSorter.ParticleCounter();

		return (a.fDist < b.fDist);
	}
};

//////////////////////////////////////////////////////////////////////////
void SRendItem::mfSortPreprocess(SRendItem* First, int Num)
{
	std::sort(First, First + Num, SCompareItemPreprocess());
}

//////////////////////////////////////////////////////////////////////////
void SRendItem::mfSortForZPass(SRendItem* First, int Num)
{
	std::sort(First, First + Num, SCompareRendItemZPass());
}

//////////////////////////////////////////////////////////////////////////
void SRendItem::mfSortByLight(SRendItem* First, int Num, bool bSort, const bool bIgnoreRePtr, bool bSortDecals)
{
	if (bSort)
	{
		if (bIgnoreRePtr)
			std::sort(First, First + Num, SCompareItem_NoPtrCompare());
		else if (bSortDecals)
			std::sort(First, First + Num, SCompareItem_Decal());
		else
			std::sort(First, First + Num, SCompareRendItem());
	}
}

//////////////////////////////////////////////////////////////////////////
void SRendItem::mfSortByDist(SRendItem* First, int Num, bool bDecals, bool InvertedOrder)
{
	//Note: Temporary use stable sort for flickering hair (meshes within the same skin attachment don't have a deterministic sort order)
	if (bDecals)
		std::stable_sort(First, First + Num, SCompareItem_Decal());
	else if (InvertedOrder)
		std::stable_sort(First, First + Num, SCompareDistInverted());
	else
		std::stable_sort(First, First + Num, SCompareDist());
}
