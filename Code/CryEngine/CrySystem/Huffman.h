// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/******************************************************************************
** Huffman.h
** 18/06/2012
** Jake Turner (ported by Rob Jessop)
******************************************************************************/

#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

class HuffmanCoder
{
private:
	struct HuffmanTreeNode
	{
		uint32 count;
		uint32 savedCount;
		int    child0;
		int    child1;
	};

	struct HuffmanSymbolCode
	{
		uint32 value;
		uint32 numBits;
	};

	struct BitStreamBuilder
	{
		enum EModes
		{
			eM_WRITE,
			eM_READ
		};
		union buf_ptr
		{
			uint8*       ptr;
			const uint8* const_ptr;
		};
		EModes  m_mode;
		uint8   m_mask;
		buf_ptr m_pBufferStart;
		buf_ptr m_pBufferCursor;
		buf_ptr m_pBufferEnd;   //Pointer to the last byte in the buffer

		BitStreamBuilder(uint8* pBufferStart, uint8* pBufferEnd)
			: m_mode(eM_WRITE), m_mask(0x80)
		{
			m_pBufferStart.ptr = pBufferStart;
			m_pBufferCursor.ptr = pBufferStart;
			m_pBufferEnd.ptr = pBufferEnd;
		}
		BitStreamBuilder(const uint8* pBufferStart, const uint8* pBufferEnd)
			: m_mode(eM_READ), m_mask(0x80)
		{
			m_pBufferStart.const_ptr = pBufferStart;
			m_pBufferCursor.const_ptr = pBufferStart;
			m_pBufferEnd.const_ptr = pBufferEnd;
		}

		void  AddBits(uint32 value, uint32 numBits);
		void  AddBits(uint8 value, uint32 numBits);
		//Returns 1 or 0 for valid values. Returns 2 if the buffer has run out or is the wrong type of builder.
		uint8 GetBit();
	};

	const static int MAX_SYMBOL_VALUE = (255);
	const static int MAX_NUM_SYMBOLS = (MAX_SYMBOL_VALUE + 1);
	const static int END_OF_STREAM = (MAX_NUM_SYMBOLS);
	const static int MAX_NUM_CODES = (MAX_NUM_SYMBOLS + 1);
	const static int MAX_NUM_NODES = (MAX_NUM_CODES * 2);
	const static int MAX_NODE = (MAX_NUM_NODES - 1);

	enum EHuffmanCoderState
	{
		eHCS_NEW,   //Has been created, Init not called
		eHCS_OPEN,  //Init has been called, tree not yet constructed. Can accept new data.
		eHCS_FINAL  //Finalize has been called. Can no longer accept data, but can encode/decode.
	};

	HuffmanTreeNode*   m_TreeNodes;
	HuffmanSymbolCode* m_Codes;
	uint32*            m_Counts;
	int                m_RootNode;
	uint32             m_RefCount;
	EHuffmanCoderState m_State;

public:
	HuffmanCoder() : m_TreeNodes(NULL), m_Codes(NULL), m_Counts(NULL), m_State(eHCS_NEW), m_RootNode(0), m_RefCount(0) {}
	~HuffmanCoder()
	{
		SAFE_DELETE_ARRAY(m_TreeNodes);
		SAFE_DELETE_ARRAY(m_Codes);
		SAFE_DELETE_ARRAY(m_Counts);
	}

	//A bit like an MD5 generator, has three phases.
	//Clears the existing data
	void Init();
	//Adds the values of an array of chars to the counts
	void Update(const uint8* const pSource, const size_t numBytes);
	//Construct the coding tree using the counts
	void Finalize();

	//We typically create a Huffman Coder per localized string table loaded. Since we can and do unload strings at runtime, it's useful to keep a ref count of each coder.
	inline void   AddRef()   { m_RefCount++; }
	inline void   DecRef()   { m_RefCount = m_RefCount > 0 ? m_RefCount - 1 : 0; }
	inline uint32 RefCount() { return m_RefCount; }

	void          CompressInput(const uint8* const pInput, const size_t numBytes, uint8* const pOutput, size_t* const outputSize);
	size_t        UncompressInput(const uint8* const pInput, const size_t numBytes, uint8* const pOutput, const size_t maxOutputSize);

	void          GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));

		if (m_Counts != NULL)
		{
			pSizer->AddObject(m_Counts, sizeof(uint32), MAX_NUM_SYMBOLS);
		}
		if (m_TreeNodes != NULL)
		{
			pSizer->AddObject(m_TreeNodes, sizeof(HuffmanTreeNode), MAX_NUM_NODES);
		}
		if (m_Codes != NULL)
		{
			pSizer->AddObject(m_Codes, sizeof(HuffmanSymbolCode), MAX_NUM_CODES);
		}
	}

private:
	void ScaleCountsAndUpdateNodes();
	int  BuildTree();
	void ConvertTreeToCode(const HuffmanTreeNode* const pNodes, HuffmanSymbolCode* const pCodes, const unsigned int value, const unsigned int numBits, const int node);
};

#endif //__HUFFMAN_H__
