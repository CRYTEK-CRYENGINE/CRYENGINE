// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------

#ifndef __CCRYPOOLDEFRAG__
#define __CCRYPOOLDEFRAG__

namespace NCryPoolAlloc
{

template<class T>
class CDefragStacked : public T
{

	template<class TItem>
	ILINE bool DefragElement(TItem* pItem)
	{
		T::m_Items.Validate();
		if (pItem)
			for (; pItem->Next(); pItem = pItem->Next())
			{
				if (!pItem->IsFree())
					continue;
				if (pItem->Next()->Locked())
					continue;
				if (!pItem->Available(pItem->Next()->Align(), pItem->Next()->Align()))
					continue;
				T::m_Items.Validate(pItem);
				Stack(pItem);
				T::m_Items.Validate(pItem);
				Merge(pItem);
				T::m_Items.Validate();
				return true;
			}
		return false;
	}
public:
	ILINE bool Beat()
	{
		return T::Defragmentable() && DefragElement(T::m_Items.First());
	};

};

}

#endif
