// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Geo.h>
#include <CryCore/CryTypeInfo.h>
//#include "../CryEngine/Cry3DEngine/CGF/ChunkFile.h"
#include <Cry3DEngine/CGF/CGFContent.h>

class CChunkData;
struct SVClothInfoCGF;

class CSaverVCloth
{
public:
	CSaverVCloth(CChunkData& chunkData, const SVClothInfoCGF* pVClothInfo, bool swapEndian);

	void WriteChunkHeader();
	void WriteChunkVertices();
	void WriteTriangleData();
	void WriteNndcNotAttachedOrdered();
	void WriteLinks();

private:

	CChunkData* m_pChunkData;
	const SVClothInfoCGF* m_pVClothInfo;
	bool m_bSwapEndian;

};

