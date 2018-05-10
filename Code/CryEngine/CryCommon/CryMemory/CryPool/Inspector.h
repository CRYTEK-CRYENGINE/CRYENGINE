// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------

#ifndef __CCRYPOOLINSPECTOR__
#define __CCRYPOOLINSPECTOR__

namespace NCryPoolAlloc
{

template<class TAllocator>
class CInspector : public TAllocator
{
	enum {EITableSize = 30};
	size_t m_Allocations[EITableSize];
	size_t m_Alignment[EITableSize];
	char   m_LogFileName[1024];
	size_t m_AllocCount;
	size_t m_FreeCount;
	size_t m_ResizeCount;
	size_t m_FailAllocCount;
	size_t m_FailFreeCount;
	size_t m_FailResizeCount;

	void WriteOut(const char* pFileName, uint32 Stack, const char* pFormat, ...) const
	{
		/*
		                              if(!pFileName)
		                              {
		                                if(!*m_LogFileName)
		                                  return;
		                                pFileName	=	m_LogFileName;
		                              }
		                              FILE*		File	=	fopen(pFileName,"a");
		                              if(File)
		                              {

		                                char Buffer[1024];
		                                for(uint32 a=0;a<Stack;a++)
		                                  Buffer[a]=' ';
		                                va_list args;
		                                va_start(args,pFormat);
		                                cry_vsprintf(Buffer+Stack,sizeof(Buffer)-Stack,pFormat,args);
		                                fwrite(Buffer,1,strlen(Buffer),File);
		                                fclose(File);
		                                va_end(args);
		                              }
		 */
	}
	size_t Bit(size_t C) const
	{
		size_t Count = 0;
		C >>= 1;
		while (C)
		{
			Count++;
			C >>= 1;
		}
		return Count >= EITableSize ? EITableSize - 1 : Count;
	}
public:
	CInspector()
	{
		for (size_t a = 0; a < EITableSize; a++)
			m_Allocations[a] = m_Alignment[a] = 0;

		m_LogFileName[0] = 0;
		m_AllocCount = 0;
		m_FreeCount = 0;
		m_ResizeCount = 0;
		m_FailAllocCount = 0;
		m_FailFreeCount = 0;
		m_FailResizeCount = 0;
	}

	bool LogFileName(const char* pFileName)
	{
		const size_t Size = strlen(pFileName) + 1;
		if (Size > sizeof(m_LogFileName))
		{
			m_LogFileName[0] = 0;
			return false;
		}
		memcpy(m_LogFileName, pFileName, Size);
		WriteOut(0, "[log start]\n");
		return true;
	}
	void SaveStats(const char* pFileName) const
	{
		WriteOut(pFileName, 0, "stats:\n");

		WriteOut(pFileName, 1, "Counter calls|fails\n");
		WriteOut(pFileName, 2, "Alloc: %6d|%6d\n", m_AllocCount, m_FailAllocCount);
		WriteOut(pFileName, 2, "Free:  %6d|%6d\n", m_FreeCount, m_FailFreeCount);
		WriteOut(pFileName, 2, "Resize:%6d|%6d\n", m_ResizeCount, m_FailResizeCount);

		WriteOut(pFileName, 1, "Allocations:\n");
		for (size_t a = 0; a < EITableSize; a++)
			WriteOut(pFileName, 2, "%9dByte: %8d\n", 1 << a, m_Allocations[a]);

		WriteOut(pFileName, 1, "Alignment:\n");
		for (size_t a = 0; a < EITableSize; a++)
			WriteOut(pFileName, 2, "%9dByte: %8d\n", 1 << a, m_Alignment[a]);
	}

	template<class T>
	ILINE T Allocate(size_t Size, size_t Align = 1)
	{
		m_AllocCount++;
		m_Allocations[Bit(Size)]++;
		m_Alignment[Bit(Align)]++;
		T pData = TAllocator::template Allocate<T>(Size, Align);
		WriteOut(0, 0, "[A|%d|%d|%d]", (int)pData, Size, Align);
		if (!pData)
		{
			m_FailAllocCount++;
			WriteOut(0, 0, "[failed]", Size, Align);
		}
		return pData;
	}

	template<class T>
	ILINE bool Free(T pData, bool ForceBoundsCheck = false)
	{
		m_FreeCount++;
		const bool Ret = TAllocator::Free(pData, ForceBoundsCheck);
		WriteOut(0, 0, "[F|%d|%d|%d]", (int)pData, (int)ForceBoundsCheck, (int)Ret);
		m_FailFreeCount += !Ret;
		return Ret;
	}
	//template<class T>
	//ILINE	bool								Free(T pData)
	//													{
	//														m_FreeCount++;
	//														const bool Ret	= TAllocator::Free(pData);
	//														WriteOut(0,0,"[F|%d|%d|%d]",(int)pData,(int)-1,(int)Ret);
	//														m_FailFreeCount+=!Ret;
	//														return Ret;
	//													}

	template<class T>
	ILINE bool Resize(T** pData, size_t Size, size_t Alignment)
	{
		m_ResizeCount++;
		const bool Ret = TAllocator::Resize(pData, Size, Alignment);
		WriteOut(0, 0, "[R|%d|%d|%d]", (int)*pData, (int)-1, (int)Ret);
		m_FailResizeCount += !Ret;
		return Ret;
	}

	template<class T>
	ILINE size_t FindBiggest(const T* pItem)
	{
		size_t Biggest = 0;
		while (pItem)
		{
			if (pItem->IsFree() && pItem->MemSize() > Biggest)
				Biggest = pItem->MemSize();
			pItem = pItem->Next();
		}
		return Biggest;
	}

	ILINE size_t BiggestFreeBlock()
	{
		return FindBiggest(TAllocator::m_Items.First());
	}

	ILINE uint8* FirstItem()
	{
		return TAllocator::m_Items.First()->Data();
	}
};

}

#endif
