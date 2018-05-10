// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SegmentedCompressionSpace.h"

#if USE_MEMENTO_PREDICTORS
static void ParseLine(string text, std::vector<uint32>& parsed)
{
	int curPos = 0;
	// [AlexMcC|27/08/09] If this text coming from binaryxml, it might be
	// shared data. Make a new string to avoid smashing it
	string val = text.Tokenize(",", curPos);
	while (!val.empty())
	{
		parsed.push_back(atoi(val.c_str()));
		val = text.Tokenize(",", curPos);
	}
}

CSegmentedCompressionSpace::CSegmentedCompressionSpace()
	: m_steps(0)
	, m_dimensions(0)
	, m_outerSizeSteps(0)
	, m_outerSizeSpace(0)
	, m_chunkSize(0)
	, m_bits(0)
{
	ZeroMemory(&m_center, sizeof(m_center));
}

bool CSegmentedCompressionSpace::Load(const char* filename)
{
	XmlNodeRef root = gEnv->pSystem->LoadXmlFromFile(filename);
	if (!root || !root->getAttr("d", m_dimensions) || !root->getAttr("s", m_steps) || !root->getAttr("b", m_bits) || !root->getChildCount())
		return false;

	int expectSize = 1;
	for (int i = 0; i < m_dimensions; i++, expectSize *= m_steps)
		;
	m_chunkSize = expectSize * 3;

	std::vector<uint32> c;
	for (int i = 0; i < root->getChildCount(); i++)
	{
		XmlNodeRef row = root->getChild(i);

		if (i == 0)
		{
			if (!row->getAttr("oss", m_outerSizeSpace))
				return false;
			if (m_outerSizeSpace % m_steps)
				return false;
			int x = 1, n = 0;
			for (; x < m_outerSizeSpace; n++, x *= m_steps)
				;
			m_outerSizeSteps = n;

			c.resize(0);
			ParseLine(row->getAttr("c"), c);
			if (c.size() != m_dimensions)
				return false;
			for (int k = 0; k < m_dimensions; k++)
				m_center[k] = c[k];
		}

		XmlNodeRef counts = row->findChild("c");
		XmlNodeRef links = row->findChild("l");
		if (!counts || !links)
			return false;

		std::vector<uint32> vCounts, vLinks;
		ParseLine(counts->getContent(), vCounts);
		ParseLine(links->getContent(), vLinks);

		if (vCounts.size() != expectSize || vLinks.size() != expectSize)
			return false;

		uint32 low = 0;
		for (int n = 0; n < expectSize; n++)
		{
			m_data.push_back(low);
			m_data.push_back(vCounts[n]);
			low += vCounts[n];
			m_data.push_back(vLinks[n]);
		}
	}

	return true;
}

bool CSegmentedCompressionSpace::CanEncode(const int32* pValue, int dim) const
{
	int32 centered[MAX_DIMENSION];
	bool inCompressibleSpace = true;
	for (int i = 0; i < dim; i++)
	{
		centered[i] = pValue[i] - m_center[i];
		inCompressibleSpace &= (centered[i] < 1 + m_outerSizeSpace / 2) && (centered[i] > -m_outerSizeSpace / 2);
	}
	return inCompressibleSpace;
}

void CSegmentedCompressionSpace::Encode(CCommOutputStream& stm, const int32* pValue, int dim) const
{
	NET_ASSERT(dim <= MAX_DIMENSION);
	NET_ASSERT(dim == m_dimensions);

	int32 centered[MAX_DIMENSION];
	bool inCompressibleSpace = true;
	for (int i = 0; i < dim; i++)
	{
		centered[i] = pValue[i] - m_center[i];
		inCompressibleSpace &= (centered[i] < 1 + m_outerSizeSpace / 2) && (centered[i] > -m_outerSizeSpace / 2);
	}

	NET_ASSERT(inCompressibleSpace);

	uint32 chunk = 0;
	const uint32 numChunks = m_data.size() / m_chunkSize;
	int32 chunkSpaceSize = m_outerSizeSpace;

	int32 extremaSmall[MAX_DIMENSION];
	for (int i = 0; i < dim; i++)
		extremaSmall[i] = -chunkSpaceSize / 2;

	while (chunk < numChunks)
	{
		uint32 ofs = 0;
		uint32 ofsFactor = 1;
		NET_ASSERT(chunkSpaceSize % m_steps == 0);
		chunkSpaceSize /= m_steps;
		for (int i = 0; i < dim; i++)
		{
			for (int j = 0; j < m_steps; j++)
			{
				if (centered[i] < extremaSmall[i] + chunkSpaceSize * (j + 1))
				{
					extremaSmall[i] += chunkSpaceSize * j;
					ofs += ofsFactor * j;
					break;
				}
			}
			ofsFactor *= m_steps;
		}
		stm.Encode(GetTot(chunk), GetLow(chunk, ofs), GetSym(chunk, ofs));
		chunk = GetChild(chunk, ofs);
	}

	// encode residual
	if (chunkSpaceSize > 1)
	{
		for (int i = 0; i < dim; i++)
			stm.Encode(chunkSpaceSize, centered[i] - extremaSmall[i], 1);
	}
}

void CSegmentedCompressionSpace::Decode(CCommInputStream& stm, int32* pValue, int dim) const
{
	NET_ASSERT(dim <= MAX_DIMENSION);
	NET_ASSERT(dim == m_dimensions);

	uint32 chunk = 0;
	const uint32 numChunks = m_data.size() / m_chunkSize;
	int32 chunkSpaceSize = m_outerSizeSpace;

	int32 extremaSmall[MAX_DIMENSION];
	for (int i = 0; i < dim; i++)
		extremaSmall[i] = -chunkSpaceSize / 2;

	while (chunk < numChunks)
	{
		uint32 ofsDec = stm.Decode(GetTot(chunk));
		uint32 ofs;
		for (ofs = m_chunkSize / 3 - 1; GetLow(chunk, ofs) > ofsDec; ofs--)
			;
		stm.Update(GetTot(chunk), GetLow(chunk, ofs), GetSym(chunk, ofs));
		chunkSpaceSize /= m_steps;
		uint32 ofsFactor = 1;
		for (int i = 0; i < dim; i++)
		{
			uint32 bigOfsFactor = ofsFactor * m_steps;
			uint32 step = (ofs % bigOfsFactor) / ofsFactor;
			extremaSmall[i] += chunkSpaceSize * step;
			ofsFactor = bigOfsFactor;
		}
		chunk = GetChild(chunk, ofs);
	}

	if (chunkSpaceSize > 1)
	{
		for (int i = 0; i < dim; i++)
		{
			int32 x = stm.Decode(chunkSpaceSize);
			stm.Update(chunkSpaceSize, x, 1);
			extremaSmall[i] += x;
		}
	}

	for (int i = 0; i < dim; i++)
		pValue[i] = extremaSmall[i] + m_center[i];
}
#endif
