// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/******************************************************************************
** Huffman.cpp
** 19/06/2012
** Jake Turner (Ported by Rob Jessop)
******************************************************************************/

#include "StdAfx.h"
#include "Huffman.h"

void HuffmanCoder::BitStreamBuilder::AddBits(uint32 value, uint32 numBits)
{
	if (numBits > 24)
	{
		AddBits(static_cast<uint8>((value >> 24) & 0x000000ff), numBits - 24);
		numBits = 24;
	}
	if (numBits > 16)
	{
		AddBits(static_cast<uint8>((value >> 16) & 0x000000ff), numBits - 16);
		numBits = 16;
	}
	if (numBits > 8)
	{
		AddBits(static_cast<uint8>((value >> 8) & 0x000000ff), numBits - 8);
		numBits = 8;
	}
	AddBits(static_cast<uint8>(value & 0x000000ff), numBits);
}

void HuffmanCoder::BitStreamBuilder::AddBits(uint8 value, uint32 numBits)
{
	if (m_mode != eM_WRITE)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Trying to write to a read only BitStreamBuilder");
		return;
	}
	uint8 mask;
	mask = (uint8)(1 << (numBits - 1));
	while (mask != 0)
	{
		//CryLogAlways("mask is %u", mask);
		if (mask & value)
		{
			//CryLogAlways("Buffer value was %u", *m_pBufferCursor.ptr);
			*(m_pBufferCursor.ptr) |= m_mask;
			//CryLogAlways("Buffer value now %u", *m_pBufferCursor.ptr);
		}
		//CryLogAlways("m_mask was %u", m_mask);
		m_mask = m_mask >> 1;
		//CryLogAlways("m_mask now %u", m_mask);
		if (m_mask == 0)
		{
			if (m_pBufferCursor.ptr == m_pBufferEnd.ptr)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Bit Stream has consumed the last byte of the buffer and is requesting another. This stream will be truncated here.");
				return;
			}
			//CryLogAlways("Buffer cursor was %u (%p)", *m_pBufferCursor.ptr, m_pBufferCursor.ptr);
			m_pBufferCursor.ptr++;
			//CryLogAlways("Buffer cursor now %u (%p)", *m_pBufferCursor.ptr, m_pBufferCursor.ptr);
			m_mask = 0x80;
		}
		mask = mask >> 1L;
	}
}

//Returns 1 or 0 for valid values. Returns 2 if the buffer has run out or is the wrong type of builder.
uint8 HuffmanCoder::BitStreamBuilder::GetBit()
{
	if (m_mode != eM_READ)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Trying to read from a write only BitStreamBuilder");
		return 2;
	}
	uint8 value = 0;

	if (m_mask == 0)
	{
		if (m_pBufferCursor.const_ptr == m_pBufferEnd.const_ptr)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Bit Stream has consumed the last byte of the buffer and is requesting another. This stream will be truncated here.");
			return 2;
		}
		//CryLogAlways("Buffer cursor was %u (%p)", *m_pBufferCursor.const_ptr, m_pBufferCursor.const_ptr);
		m_pBufferCursor.const_ptr++;
		//CryLogAlways("Buffer cursor now %u (%p)", *m_pBufferCursor.const_ptr, m_pBufferCursor.const_ptr);
		m_mask = 0x80;
	}
	if (m_mask & *(m_pBufferCursor.const_ptr))
	{
		value = 1;
	}
	//CryLogAlways("m_mask was %u", m_mask);
	m_mask = m_mask >> 1;
	//CryLogAlways("m_mask now %u", m_mask);

	return value;
}

void HuffmanCoder::Init()
{
	SAFE_DELETE_ARRAY(m_TreeNodes);
	SAFE_DELETE_ARRAY(m_Codes);
	m_Counts = new uint32[MAX_NUM_SYMBOLS];
	memset(m_Counts, 0, sizeof(uint32) * MAX_NUM_SYMBOLS);
	m_State = eHCS_OPEN;
}

//Adds the values of an array of chars to the counts
void HuffmanCoder::Update(const uint8* const pSource, const size_t numBytes)
{
	if (m_State != eHCS_OPEN)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Trying to update a Huffman Coder that has not been initialized, or has been finalized");
		return;
	}

	size_t i;
	for (i = 0; i < numBytes; i++)
	{
		const int symbol = pSource[i];
		m_Counts[symbol]++;
	}
}

void HuffmanCoder::Finalize()
{
	if (m_State != eHCS_OPEN)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Trying to finalize a Huffman Coder that has not been initialized, or has been finalized");
		return;
	}

	//Construct the tree
	m_TreeNodes = new HuffmanTreeNode[MAX_NUM_NODES];
	memset(m_TreeNodes, 0, sizeof(HuffmanTreeNode) * MAX_NUM_NODES);
	m_Codes = new HuffmanSymbolCode[MAX_NUM_CODES];
	memset(m_Codes, 0, sizeof(HuffmanSymbolCode) * MAX_NUM_CODES);

	ScaleCountsAndUpdateNodes();
	m_RootNode = BuildTree();
	ConvertTreeToCode(m_TreeNodes, m_Codes, 0, 0, m_RootNode);

	//Finalize the coder so that it won't accept any more strings
	m_State = eHCS_FINAL;

	//Counts are no longer needed
	SAFE_DELETE_ARRAY(m_Counts);
}

void HuffmanCoder::CompressInput(const uint8* const pInput, const size_t numBytes, uint8* const pOutput, size_t* const outputSize)
{
	BitStreamBuilder streamBuilder(pOutput, pOutput + (*outputSize));
	for (size_t i = 0; i < numBytes; i++)
	{
		const int symbol = pInput[i];
		const uint32 value = m_Codes[symbol].value;
		const uint32 numBits = m_Codes[symbol].numBits;
		/*char szBits[33];
		   memset(szBits, '0', 33);
		   for( uint32 j = 0; j < numBits; j++ )
		   {
		   if( (value & (uint32)(1<<j)) != 0 )
		   {
		    szBits[31-j] = '1';
		   }
		   else
		   {
		    szBits[31-j] = '0';
		   }
		   }
		   szBits[32] = 0;
		   CryLogAlways("%c - %s (%u)", value, szBits, numBits);*/
		streamBuilder.AddBits(value, numBits);
	}
	streamBuilder.AddBits(m_Codes[END_OF_STREAM].value, m_Codes[END_OF_STREAM].numBits);
	*outputSize = (streamBuilder.m_pBufferCursor.ptr - streamBuilder.m_pBufferStart.ptr) + 1;
}

size_t HuffmanCoder::UncompressInput(const uint8* const pInput, const size_t numBytes, uint8* const pOutput, const size_t maxOutputSize)
{
	size_t numOutputBytes = 0;
	BitStreamBuilder streamBuilder(pInput, pInput + numBytes);

	while (1)
	{
		int code;
		int node = m_RootNode;
		do
		{
			uint8 bitValue = streamBuilder.GetBit();
#if 0
			CryLogAlways("bit=%ld\n", bitValue);
#endif

			if (bitValue == 0)
			{
				node = m_TreeNodes[node].child0;
			}
			else
			{
				node = m_TreeNodes[node].child1;
			}
		}
		while (node > END_OF_STREAM);

		if (node == END_OF_STREAM)
		{
			pOutput[numOutputBytes] = '\0';
			break;
		}
		code = node;
#if 0
		{
			CryLogAlways("%c", code);
			if (code == '\0')
			{
				CryLogAlways("EOM");
			}
		}
#endif
		pOutput[numOutputBytes] = (char)code;
		numOutputBytes++;
		if (numOutputBytes >= maxOutputSize)
		{
			pOutput[maxOutputSize - 1] = '\0';
			break;
		}
	}

	return numOutputBytes;
}

//Private functions

void HuffmanCoder::ScaleCountsAndUpdateNodes()
{
	unsigned long maxCount = 0;
	int i;

	for (i = 0; i < MAX_NUM_SYMBOLS; i++)
	{
		const unsigned long count = m_Counts[i];
		if (count > maxCount)
		{
			maxCount = count;
		}
	}
	if (maxCount == 0)
	{
		m_Counts[0] = 1;
		maxCount = 1;
	}
	maxCount = maxCount / MAX_NUM_SYMBOLS;
	maxCount = maxCount + 1;
	for (i = 0; i < MAX_NUM_SYMBOLS; i++)
	{
		const unsigned long count = m_Counts[i];
		unsigned int scaledCount = (unsigned int)(count / maxCount);
		if ((scaledCount == 0) && (count != 0))
		{
			scaledCount = 1;
		}
		m_TreeNodes[i].count = scaledCount;
		m_TreeNodes[i].child0 = END_OF_STREAM;
		m_TreeNodes[i].child1 = END_OF_STREAM;
	}
	m_TreeNodes[END_OF_STREAM].count = 1;
	m_TreeNodes[END_OF_STREAM].child0 = END_OF_STREAM;
	m_TreeNodes[END_OF_STREAM].child1 = END_OF_STREAM;
}

//Jake's file IO code. Kept in case we make the compression and table generation an offline task
/* Format is: startSymbol, stopSymbol, count0, count1, count2, ... countN, ..., 0 */
/* When finding the start, stop symbols only break out if find more than 3 0's in the counts */
/*static void outputCounts(FILE* const pFile, const HuffmanTreeNode* const pNodes)
   {
   int first = 0;
   int last;
   int next;

   while ((first < MAX_NUM_SYMBOLS) && (pNodes[first].count == 0))
   {
    first++;
   }
   last = first;
   next = first;
   for (; first < MAX_NUM_SYMBOLS; first = next)
   {
    int i;
    last = first+1;
    while (1)
    {
      for (; last < MAX_NUM_SYMBOLS; last++)
      {
        if (pNodes[last].count == 0)
        {
          break;
        }
      }
      last--;
      for (next = last+1; next < MAX_NUM_SYMBOLS; next++)
      {
        if (pNodes[next].count != 0)
        {
          break;
        }
      }
      if (next == MAX_NUM_SYMBOLS)
      {
        break;
      }
      if ((next-last) > 3)
      {
        break;
      }
      last = next;
    }
    putc(first, pFile);
    putc(last, pFile);
    for (i = first; i <= last; i++)
    {
      const unsigned int count = pNodes[i].count;
      putc((int)count, pFile);
    }
   }
   putc(0xFF, pFile);
   putc(0xFF, pFile);
   putc((int)(pNodes[0xFF].count), pFile);
   }*/

/*static void inputCounts(FILE* const pFile, HuffmanTreeNode* const pNodes)
   {
   while (1)
   {
    int i;
    const int first = getc(pFile);
    const int last = getc(pFile);
    for (i = first; i <= last; i++)
    {
      const int count = getc(pFile);
      pNodes[i].count = (size_t)count;
      pNodes[i].child0 = END_OF_STREAM;
      pNodes[i].child1 = END_OF_STREAM;
    }
    if ((first == last) && (first == 0xFF))
    {
      break;
    }
   }
   pNodes[END_OF_STREAM].count = 1;
   pNodes[END_OF_STREAM].child0 = END_OF_STREAM;
   pNodes[END_OF_STREAM].child1 = END_OF_STREAM;
   }*/

int HuffmanCoder::BuildTree()
{
	int min1;
	int min2;
	int nextFree;

	m_TreeNodes[MAX_NODE].count = 0xFFFFFFF;
	for (nextFree = END_OF_STREAM + 1;; nextFree++)
	{
		int i;
		min1 = MAX_NODE;
		min2 = MAX_NODE;
		for (i = 0; i < nextFree; i++)
		{
			const unsigned int count = m_TreeNodes[i].count;
			if (count != 0)
			{
				if (count < m_TreeNodes[min1].count)
				{
					min2 = min1;
					min1 = i;
				}
				else if (count < m_TreeNodes[min2].count)
				{
					min2 = i;
				}
			}
		}
		if (min2 == MAX_NODE)
		{
			break;
		}
		m_TreeNodes[nextFree].count = m_TreeNodes[min1].count + m_TreeNodes[min2].count;

		m_TreeNodes[min1].savedCount = m_TreeNodes[min1].count;
		m_TreeNodes[min1].count = 0;

		m_TreeNodes[min2].savedCount = m_TreeNodes[min2].count;
		m_TreeNodes[min2].count = 0;

		m_TreeNodes[nextFree].child0 = min1;
		m_TreeNodes[nextFree].child1 = min2;
		m_TreeNodes[nextFree].savedCount = 0;
	}

	nextFree--;
	m_TreeNodes[nextFree].savedCount = m_TreeNodes[nextFree].count;

	return nextFree;
}

void HuffmanCoder::ConvertTreeToCode(const HuffmanTreeNode* const pNodes, HuffmanSymbolCode* const pCodes,
                                     const unsigned int value, const unsigned int numBits, const int node)
{
	unsigned int nextValue;
	unsigned int nextNumBits;
	if (node <= END_OF_STREAM)
	{
		pCodes[node].value = value;
		pCodes[node].numBits = numBits;
		return;
	}
	nextValue = value << 1;
	nextNumBits = numBits + 1;
	ConvertTreeToCode(pNodes, pCodes, nextValue, nextNumBits, pNodes[node].child0);
	nextValue = nextValue | 0x1;
	ConvertTreeToCode(pNodes, pCodes, nextValue, nextNumBits, pNodes[node].child1);
}

/*static void printChar(const int c)
   {
   if (c >= ' ' && c < 127)
   {
    printf("'%c'", c);
   }
   else
   {
    printf("0x%03X", c);
   }
   }

   static void printModel(const HuffmanTreeNode* const pNodes, const HuffmanSymbolCode* const pCodes)
   {
   int i;
   for (i = 0; i < MAX_NODE; i++)
   {
    const unsigned int count = pNodes[i].savedCount;
    if (count != 0)
    {
      printf("node=");
      printChar(i);
      printf(" count=%3d", count);
      printf(" child0=");
      printChar(pNodes[i].child0);
      printf(" child1=");
      printChar(pNodes[i].child1);
      if (pCodes && (i <= END_OF_STREAM))
      {
        printf(" Huffman code=");
        binaryFilePrint(stdout, pCodes[i].value, pCodes[i].numBits);
      }
      printf("\n");
    }
   }
   }*/
