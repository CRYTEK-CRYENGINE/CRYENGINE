// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:
   -------------------------------------------------------------------------
   History:
   - 02/11/2005   12:34 : Created by Jan Mueller
*************************************************************************/
#include "StdAfx.h"

#include "Config.h"

#if USE_ARITHSTREAM

	#include "ArithAlphabet.h"
	#include "Protocol/Serialize.h"

void CArithAlphabetMTF::WriteSymbol(CCommOutputStream& stm, uint16 nSymbol)
{
	nSymbol = m_nSymbols - MoveSymbolToFront(nSymbol) - 1;
	uint16 nTot, nLow, nSym;
	GetFrequency(nSymbol, nTot, nLow, nSym);
	stm.Encode(nTot, nLow, nSym);
}

float CArithAlphabetMTF::EstimateSymbolSizeInBits(uint16 nSymbol) const
{
	uint16* pArray = (uint16*)(m_pData + 0 * m_nSymbols);
	uint8* pSym = m_pData + 4 * m_nSymbols;
	uint16 i;
	for (i = 0; pArray[i] != nSymbol; i++)
		;
	i = m_nSymbols - i - 1;
	return CCommOutputStream::EstimateArithSizeInBits(m_nTot, pSym[i] + 1);
}

uint16 CArithAlphabetMTF::ReadSymbol(CCommInputStream& stm)
{
	uint16 nProb = stm.Decode(m_nTot);

	uint16* pArray = (uint16*)(m_pData + 0 * m_nSymbols);
	uint16* pLow = (uint16*)(m_pData + 2 * m_nSymbols);
	uint8* pSym = m_pData + 4 * m_nSymbols;

	uint16 nPos;
	for (nPos = m_nSymbols - 1; pLow[nPos] > nProb; --nPos)
		;
	NET_ASSERT(pLow[nPos] <= nProb);
	NET_ASSERT(pLow[nPos] + pSym[nPos] + 1 > nProb);
	stm.Update(m_nTot, pLow[nPos], pSym[nPos] + 1);
	IncSymbol(nPos);

	nPos = m_nSymbols - nPos - 1;

	uint16 nSymbol = pArray[nPos];
	for (; nPos != 0; --nPos)
		pArray[nPos] = pArray[nPos - 1];
	pArray[0] = nSymbol;

	return nSymbol;
}

void CArithAlphabetOrder0::WriteSymbol(CCommOutputStream& stm, unsigned nSymbol)
{
	stm.Encode(
	  m_nTot,
	  GetLow(nSymbol),
	  GetSym(nSymbol));
	IncCount(nSymbol);
}

unsigned CArithAlphabetOrder0::ReadSymbol(CCommInputStream& stm)
{
	unsigned nSymbol;
	unsigned nProb = stm.Decode(m_nTot);

	unsigned nBegin = 0;
	unsigned nEnd = m_nSymbols;
	while (true)
	{
		nSymbol = (nBegin + nEnd) / 2;

		if (nProb < GetLow(nSymbol))
			nEnd = nSymbol;
		else if (nProb >= unsigned(GetLow(nSymbol) + GetSym(nSymbol)))
			nBegin = nSymbol + 1;
		else
			break;
	}

	stm.Update(
	  m_nTot,
	  GetLow(nSymbol),
	  GetSym(nSymbol));
	IncCount(nSymbol);

	return nSymbol;
}

#endif
