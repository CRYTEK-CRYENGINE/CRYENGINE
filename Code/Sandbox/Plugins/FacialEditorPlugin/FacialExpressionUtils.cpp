// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FacialExpressionUtils.h"
#include <CryAnimation/IFacialAnimation.h>
#include "FacialEdContext.h"

FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::FacialExpressionLibraryOrphanFinder(IFacialEffectorsLibrary* pLibrary)
	: m_pLibrary(pLibrary), m_bDirty(true)
{
}

FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::Iterator FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::Begin()
{
	FindOrphans();

	return Iterator(*this, SkipNonOrphans(m_entries.begin()));
}

FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::Iterator FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::End()
{
	FindOrphans();

	return Iterator(*this, m_entries.end());
}

IFacialEffector* FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::GetExpression(VectorSet<Entry>::iterator cursor)
{
	return (*cursor).pEffector;
}

VectorSet<FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::Entry>::iterator FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::GetNextOrphanIndex(VectorSet<Entry>::iterator cursor)
{
	if (cursor == m_entries.end())
		return cursor;
	++cursor;
	return SkipNonOrphans(cursor);
}

VectorSet<FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::Entry>::iterator FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::SkipNonOrphans(VectorSet<Entry>::iterator cursor)
{
	while (cursor != m_entries.end() &&
	       ((*cursor).count || ((*cursor).pEffector->GetFlags() & EFE_FLAG_UI_PREVIEW) || ((*cursor).pEffector->GetType() == EFE_TYPE_MORPH_TARGET)))
		++cursor;

	return cursor;
}

void FacialExpressionUtils::FacialExpressionLibraryOrphanFinder::FindOrphans()
{
	if (!m_bDirty)
		return;
	m_bDirty = false;
	if (!m_pLibrary)
		return;

	m_entries.clear();
	{
		EntryContainer::container_type entries;
		class Visitor : public IFacialEffectorsLibraryEffectorVisitor
		{
		public:
			Visitor(EntryContainer::container_type& entries) : entries(entries) {}
			EntryContainer::container_type& entries;

			virtual void VisitEffector(IFacialEffector* pEffector) { entries.push_back(Entry(pEffector)); }
		};
		Visitor visitor(entries);
		m_pLibrary->VisitEffectors(&visitor);
		m_entries.SwapElementsWithVector(entries);
	}

	for (EntryContainer::iterator it = m_entries.begin(), end = m_entries.end(); it != end; ++it)
	{
		for (int subEffectorIndex = 0, subEffectorCount = (*it).pEffector->GetSubEffectorCount(); subEffectorIndex < subEffectorCount; ++subEffectorIndex)
		{
			EntryContainer::iterator itChild = m_entries.find(Entry((*it).pEffector->GetSubEffector(subEffectorIndex)));
			++(*itChild).count;
		}
	}
}

IFacialEffector* FacialExpressionUtils::GetGarbageFacialExpressionFolder(IFacialEffectorsLibrary* pLibrary, bool createIfNecessary)
{
	IFacialEffector* pGarbageFolder = (pLibrary ? pLibrary->Find("_garbage") : 0);
	if (!pGarbageFolder && pLibrary && createIfNecessary)
	{
		pGarbageFolder = pLibrary->CreateEffector(EFE_TYPE_GROUP, "_garbage");
		pLibrary->GetRoot()->AddSubEffector(pGarbageFolder);
	}

	return pGarbageFolder;
}

int FacialExpressionUtils::AddOrphansToGarbageFacialExpressionFolder(IFacialEffectorsLibrary* pLibrary)
{
	int orphansAdded = 0;
	FacialExpressionLibraryOrphanFinder orphanFinder(pLibrary);
	for (FacialExpressionLibraryOrphanFinder::Iterator itOrphan = orphanFinder.Begin(), endOrphan = orphanFinder.End(); itOrphan != endOrphan; ++itOrphan)
	{
		IFacialEffector* pGarbageFolder = GetGarbageFacialExpressionFolder(pLibrary);
		if (pGarbageFolder && pGarbageFolder != *itOrphan && pLibrary->GetRoot() != *itOrphan)
		{
			pGarbageFolder->AddSubEffector(*itOrphan);
			++orphansAdded;
		}
	}

	return orphansAdded;
}

int FacialExpressionUtils::DeleteOrphans(IFacialEffectorsLibrary* pLibrary)
{
	int orphansDeleted = 0;
	for (;; )
	{
		FacialExpressionLibraryOrphanFinder orphanFinder(pLibrary);
		int deletedThisPass = 0;
		for (FacialExpressionLibraryOrphanFinder::Iterator itOrphan = orphanFinder.Begin(), endOrphan = orphanFinder.End(); itOrphan != endOrphan; ++itOrphan)
		{
			if (pLibrary->GetRoot() != *itOrphan)
			{
				pLibrary->RemoveEffector(*itOrphan);
				++deletedThisPass;
				++orphansDeleted;
			}
		}
		if (!deletedThisPass)
			break;
	}

	return orphansDeleted;
}

void FacialExpressionUtils::RemoveFromGarbage(IFacialEffectorsLibrary* pLibrary, IFacialEffector* pEffector)
{
	IFacialEffector* pGarbageFolder = GetGarbageFacialExpressionFolder(pLibrary, false);
	if (pGarbageFolder && pEffector)
	{
		pGarbageFolder->RemoveSubEffector(pEffector);
		if (pLibrary && pGarbageFolder->GetSubEffectorCount() == 0)
			pLibrary->RemoveEffector(pGarbageFolder);
	}
}

bool FacialExpressionUtils::IsExpressionInGarbage(IFacialEffectorsLibrary* pLibrary, IFacialEffector* pEffector)
{
	bool inGarbage = false;
	IFacialEffector* pGarbageFolder = GetGarbageFacialExpressionFolder(pLibrary, false);
	if (pGarbageFolder && pEffector)
	{
		for (int i = 0, end = pGarbageFolder->GetSubEffectorCount(); i < end; ++i)
			inGarbage = inGarbage || pGarbageFolder->GetSubEffector(i) == pEffector;
	}
	return inGarbage;
}

