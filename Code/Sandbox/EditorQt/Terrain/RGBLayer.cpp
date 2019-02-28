// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RGBLayer.h"
#include "GameEngine.h"
#include "Util/PakFile.h"
#include <CrySystem/ITimer.h>
#include "Layer.h"
#include <CryMemory/CrySizer.h>
#include "Util/Clipboard.h"

#include "Terrain/TerrainManager.h"
#include "Util/ImagePainter.h"

#include "QT/Widgets/QWaitProgress.h"
#include "Controls/QuestionDialog.h"

//////////////////////////////////////////////////////////////////////////
CRGBLayer::CRGBLayer(const char* szFilename) : m_TerrainRGBFileName(szFilename), m_dwTileResolution(0), m_dwTileCountX(0), m_dwTileCountY(0),
	m_dwCurrentTileMemory(0), m_bPakOpened(false), m_bInfoDirty(false), m_bNextSerializeForceSizeSave(false)
{
	assert(szFilename);

	FreeData();
}

//////////////////////////////////////////////////////////////////////////
CRGBLayer::~CRGBLayer()
{
	FreeData();
}

void CRGBLayer::SetSubImageStretched(const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, CImageEx& rInImage, const bool bFiltered)
{
	assert(fSrcLeft >= 0.0f && fSrcLeft <= 1.0f);
	assert(fSrcTop >= 0.0f && fSrcTop <= 1.0f);
	assert(fSrcRight >= 0.0f && fSrcRight <= 1.0f);
	assert(fSrcBottom >= 0.0f && fSrcBottom <= 1.0f);
	assert(fSrcRight >= fSrcLeft);
	assert(fSrcBottom >= fSrcTop);

	float fSrcWidth = fSrcRight - fSrcLeft;
	float fSrcHeight = fSrcBottom - fSrcTop;

	uint32 dwDestWidth = rInImage.GetWidth();
	uint32 dwDestHeight = rInImage.GetHeight();

	uint32 dwTileX1 = (uint32)(fSrcLeft * m_dwTileCountX);
	uint32 dwTileY1 = (uint32)(fSrcTop * m_dwTileCountY);
	uint32 dwTileX2 = (uint32)ceil(fSrcRight * m_dwTileCountX);
	uint32 dwTileY2 = (uint32)ceil(fSrcBottom * m_dwTileCountY);

	for (uint32 dwTileY = dwTileY1; dwTileY < dwTileY2; dwTileY++)
		for (uint32 dwTileX = dwTileX1; dwTileX < dwTileX2; dwTileX++)
		{
			CTerrainTextureTiles* tile = LoadTileIfNeeded(dwTileX, dwTileY);
			assert(tile);
			assert(tile->m_pTileImage);
			uint32 dwTileSize = tile->m_pTileImage->GetWidth();

			float fSx = (-fSrcLeft + float(dwTileX) / m_dwTileCountX) / fSrcWidth;
			float fSy = (-fSrcTop + float(dwTileY) / m_dwTileCountY) / fSrcHeight;

			for (uint32 y = 0; y < dwTileSize; y++)
				for (uint32 x = 0; x < dwTileSize; x++)
				{
					float fX = float(x) / dwTileSize / m_dwTileCountX / fSrcWidth + fSx;
					float fY = float(y) / dwTileSize / m_dwTileCountY / fSrcHeight + fSy;

					if (0.0f <= fX && fX < 1.0f && 0.0f <= fY && fY < 1.0f)
					{
						uint32 dwX = (uint32)(fX * dwDestWidth);
						uint32 dwY = (uint32)(fY * dwDestHeight);

						uint32 dwX1 = dwX + 1;
						uint32 dwY1 = dwY + 1;

						if (dwX1 < dwDestWidth && dwY1 < dwDestHeight)
						{
							float fLerpX = fX * dwDestWidth - dwX;
							float fLerpY = fY * dwDestHeight - dwY;

							ColorB colS[4];

							colS[0] = rInImage.ValueAt(dwX, dwY);
							colS[1] = rInImage.ValueAt(dwX + 1, dwY);
							colS[2] = rInImage.ValueAt(dwX, dwY + 1);
							colS[3] = rInImage.ValueAt(dwX + 1, dwY + 1);

							ColorB colTop, colBottom;

							colTop.lerpFloat(colS[0], colS[1], fLerpX);
							colBottom.lerpFloat(colS[2], colS[3], fLerpX);

							ColorB colRet;

							colRet.lerpFloat(colTop, colBottom, fLerpY);

							tile->m_pTileImage->ValueAt(x, y) = colRet.pack_abgr8888();

						}
						else
							tile->m_pTileImage->ValueAt(x, y) = rInImage.ValueAt(dwX, dwY);
					}
				}
			tile->m_bDirty = true;
		}
}

unsigned int SampleImage(const CImageEx* pImage, float x, float y);

void         CRGBLayer::SetSubImageTransformed(const CImageEx* pImage, const Matrix34& transform)
{
	Vec3 p0 = transform.TransformPoint(Vec3(0, 0, 0));
	Vec3 p1 = transform.TransformPoint(Vec3(1, 0, 0));
	Vec3 p2 = transform.TransformPoint(Vec3(0, 1, 0));
	Vec3 p3 = transform.TransformPoint(Vec3(1, 1, 0));

	float fSrcLeft = clamp_tpl(min(min(p0.x, p1.x), min(p2.x, p3.x)), 0.0f, 1.0f);
	float fSrcRight = clamp_tpl(max(max(p0.x, p1.x), max(p2.x, p3.x)), 0.0f, 1.0f);
	float fSrcTop = clamp_tpl(min(min(p0.y, p1.y), min(p2.y, p3.y)), 0.0f, 1.0f);
	float fSrcBottom = clamp_tpl(max(max(p0.y, p1.y), max(p2.y, p3.y)), 0.0f, 1.0f);

	float fSrcWidth = fSrcRight - fSrcLeft;
	float fSrcHeight = fSrcBottom - fSrcTop;

	uint32 dwTileX1 = (uint32)(fSrcLeft * m_dwTileCountX);
	uint32 dwTileY1 = (uint32)(fSrcTop * m_dwTileCountY);
	uint32 dwTileX2 = (uint32)ceil(fSrcRight * m_dwTileCountX);
	uint32 dwTileY2 = (uint32)ceil(fSrcBottom * m_dwTileCountY);

	Matrix34 invTransform = transform.GetInverted();

	for (uint32 dwTileY = dwTileY1; dwTileY < dwTileY2; dwTileY++)
	{
		for (uint32 dwTileX = dwTileX1; dwTileX < dwTileX2; dwTileX++)
		{
			CTerrainTextureTiles* tile = LoadTileIfNeeded(dwTileX, dwTileY);
			assert(tile);
			assert(tile->m_pTileImage);
			uint32 dwTileSize = tile->m_pTileImage->GetWidth();

			for (uint32 y = 0; y < dwTileSize; y++)
			{
				for (uint32 x = 0; x < dwTileSize; x++)
				{
					float tx = ((float)x / dwTileSize + dwTileX) / m_dwTileCountX;
					float ty = ((float)y / dwTileSize + dwTileY) / m_dwTileCountY;
					Vec3 uvw = invTransform.TransformPoint(Vec3(tx, ty, 0));
					if (uvw.x < 0 || uvw.y < 0 || uvw.x > 1 || uvw.y > 1)
						continue;
					tile->m_pTileImage->ValueAt(x, y) = SampleImage(pImage, uvw.x, uvw.y);
				}
			}
			tile->m_bDirty = true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::GetSubImageStretched(const float fSrcLeft, const float fSrcTop, const float fSrcRight,
                                     const float fSrcBottom, CImageEx& rOutImage, const bool bFiltered)
{
	assert(fSrcLeft >= 0.0f && fSrcLeft <= 1.0f);
	assert(fSrcTop >= 0.0f && fSrcTop <= 1.0f);
	assert(fSrcRight >= 0.0f && fSrcRight <= 1.0f);
	assert(fSrcBottom >= 0.0f && fSrcBottom <= 1.0f);
	assert(fSrcRight >= fSrcLeft);
	assert(fSrcBottom >= fSrcTop);

	uint32 dwDestWidth = rOutImage.GetWidth();
	uint32 dwDestHeight = rOutImage.GetHeight();

	int iBorderIncluded = bFiltered ? 1 : 0;

	float fScaleX = (fSrcRight - fSrcLeft) / (float)(dwDestWidth - iBorderIncluded);
	float fScaleY = (fSrcBottom - fSrcTop) / (float)(dwDestHeight - iBorderIncluded);

	for (uint32 dwDestY = 0; dwDestY < dwDestHeight; ++dwDestY)
	{
		float fSrcY = fSrcTop + dwDestY * fScaleY;

		if (bFiltered) // check is not in the inner loop because of performance reasons
		{
			for (uint32 dwDestX = 0; dwDestX < dwDestWidth; ++dwDestX)
			{
				float fSrcX = fSrcLeft + dwDestX * fScaleX;

				rOutImage.ValueAt(dwDestX, dwDestY) = GetFilteredValueAt(fSrcX, fSrcY);
			}
		}
		else
		{
			for (uint32 dwDestX = 0; dwDestX < dwDestWidth; ++dwDestX)
			{
				float fSrcX = fSrcLeft + dwDestX * fScaleX;

				rOutImage.ValueAt(dwDestX, dwDestY) = GetValueAt(fSrcX, fSrcY);
			}
		}
	}
}

bool CRGBLayer::ChangeTileResolution(const uint32 dwTileX, const uint32 dwTileY, uint32 dwNewSize)
{
	assert(dwTileX < m_dwTileCountX);
	assert(dwTileY < m_dwTileCountY);

	CTerrainTextureTiles* tile = LoadTileIfNeeded(dwTileX, dwTileY);
	assert(tile);

	if (!tile)
		return false;   // to avoid crash

	assert(tile->m_pTileImage);

	CImageEx newImage;

	uint32 dwOldSize = tile->m_pTileImage->GetWidth();
	//uint32 dwNewSize = bRaise ? dwOldSize*2 : dwOldSize/2;

	if (dwNewSize < 8)
		return false;   // not lower than this

	if (!newImage.Allocate(dwNewSize, dwNewSize))
		return false;

	float fBorderX = 0; // 0.5f/(float)(dwOldSize*m_dwTileCountX);
	float fBorderY = 0; // 0.5f/(float)(dwOldSize*m_dwTileCountY);

	// copy over with filtering
	GetSubImageStretched((float)dwTileX / (float)m_dwTileCountX + fBorderX, (float)dwTileY / (float)m_dwTileCountY + fBorderY,
	                     (float)(dwTileX + 1) / (float)m_dwTileCountX - fBorderX, (float)(dwTileY + 1) / (float)m_dwTileCountY - fBorderY, newImage, true);

	tile->m_pTileImage->Attach(newImage);
	tile->m_bDirty = true;

	tile->m_dwSize = dwNewSize;

	return true;
}

uint32 CRGBLayer::GetValueAt(const float fpx, const float fpy)
{
	uint32* value = nullptr;
	if (GetValueAt(fpx, fpy, value))
	{
		return *value;
	}
	else
	{
		return 0;
	}
}

bool CRGBLayer::GetValueAt(const float fpx, const float fpy, uint32*& value)
{
	assert(fpx >= 0.0f && fpx <= 1.0f);
	assert(fpy >= 0.0f && fpy <= 1.0f);

	uint32 dwTileX = (uint32)(fpx * m_dwTileCountX);
	uint32 dwTileY = (uint32)(fpy * m_dwTileCountY);

	CTerrainTextureTiles* tile = LoadTileIfNeeded(dwTileX, dwTileY);
	assert(tile);

	if (!tile)
		return false;   // to avoid crash

	assert(tile->m_pTileImage);

	uint32 dwTileSize = tile->m_pTileImage->GetWidth();
	//	uint32 dwTileSizeReduced = dwTileSize-1;
	uint32 dwTileOffsetX = dwTileX * dwTileSize;
	uint32 dwTileOffsetY = dwTileY * dwTileSize;

	float fX = fpx * (dwTileSize * m_dwTileCountX), fY = fpy * (dwTileSize * m_dwTileCountY);

	uint32 dwLocalX = (uint32)(fX - dwTileOffsetX);
	uint32 dwLocalY = (uint32)(fY - dwTileOffsetY);

	// no bilinear filter ------------------------
	assert(dwLocalX < tile->m_pTileImage->GetWidth());
	assert(dwLocalY < tile->m_pTileImage->GetHeight());
	value = &tile->m_pTileImage->ValueAt(dwLocalX, dwLocalY);
	return true;
}

uint32 CRGBLayer::GetFilteredValueAt(const float fpx, const float fpy)
{
	assert(fpx >= 0.0f && fpx <= 1.0f);
	assert(fpy >= 0.0f && fpy <= 1.0f);

	uint32 dwTileX = (uint32)(fpx * m_dwTileCountX);
	uint32 dwTileY = (uint32)(fpy * m_dwTileCountY);

	float fX = (fpx * m_dwTileCountX - dwTileX), fY = (fpy * m_dwTileCountY - dwTileY);

	// adjust lookup to keep as few tiles in memory as possible
	if (dwTileX != 0 && fX <= 0.000001f)
	{
		--dwTileX;
		fX = 0.99999f;
	}

	if (dwTileY != 0 && fY <= 0.000001f)
	{
		--dwTileY;
		fY = 0.99999f;
	}

	CTerrainTextureTiles* tile = LoadTileIfNeeded(dwTileX, dwTileY);
	assert(tile);

	assert(tile->m_pTileImage);

	uint32 dwTileSize = tile->m_pTileImage->GetWidth();

	fX *= (dwTileSize - 1);
	fY *= (dwTileSize - 1);

	uint32 dwLocalX = (uint32)(fX);
	uint32 dwLocalY = (uint32)(fY);

	float fLerpX = fX - dwLocalX;     // 0..1
	float fLerpY = fY - dwLocalY;     // 0..1

	// bilinear filter ----------------------

	assert(dwLocalX < tile->m_pTileImage->GetWidth() - 1);
	assert(dwLocalY < tile->m_pTileImage->GetHeight() - 1);

	ColorF colS[4];

	colS[0] = tile->m_pTileImage->ValueAt(dwLocalX, dwLocalY);
	colS[1] = tile->m_pTileImage->ValueAt(dwLocalX + 1, dwLocalY);
	colS[2] = tile->m_pTileImage->ValueAt(dwLocalX, dwLocalY + 1);
	colS[3] = tile->m_pTileImage->ValueAt(dwLocalX + 1, dwLocalY + 1);

	ColorF colTop, colBottom;

	colTop.lerpFloat(colS[0], colS[1], fLerpX);
	colBottom.lerpFloat(colS[2], colS[3], fLerpX);

	ColorF colRet;

	colRet.lerpFloat(colTop, colBottom, fLerpY);

	return colRet.pack_abgr8888();
}

uint32 CRGBLayer::GetUnfilteredValueAt(float fpx, float fpy)
{
	assert(fpx >= 0.0f && fpx <= 1.0f);
	assert(fpy >= 0.0f && fpy <= 1.0f);

	uint32 dwTileX = (uint32)(fpx * m_dwTileCountX);
	uint32 dwTileY = (uint32)(fpy * m_dwTileCountY);

	float fX = (fpx * m_dwTileCountX - dwTileX), fY = (fpy * m_dwTileCountY - dwTileY);

	CTerrainTextureTiles* tile = LoadTileIfNeeded(dwTileX, dwTileY);
	assert(tile);

	assert(tile->m_pTileImage);

	uint32 dwTileSize = tile->m_pTileImage->GetWidth();

	fX *= (dwTileSize - 1);
	fY *= (dwTileSize - 1);

	uint32 dwLocalX = (uint32)(fX);
	uint32 dwLocalY = (uint32)(fY);

	assert(dwLocalX < tile->m_pTileImage->GetWidth() - 1);
	assert(dwLocalY < tile->m_pTileImage->GetHeight() - 1);

	ColorB col = tile->m_pTileImage->ValueAt(dwLocalX, dwLocalY);
	return col.pack_abgr8888();
}

void CRGBLayer::SetValueAt(const float fpx, const float fpy, const uint32 dwValue)
{
	uint32* value = nullptr;
	if (GetValueAt(fpx, fpy, value))
	{
		*value = dwValue;
	}
}

void CRGBLayer::SetValueAt(const uint32 dwX, const uint32 dwY, const uint32 dwValue)
{
	uint32 dwTileX = (uint32)dwX / m_dwTileResolution;
	uint32 dwTileY = (uint32)dwY / m_dwTileResolution;

	CTerrainTextureTiles* tile = LoadTileIfNeeded(dwTileX, dwTileY);
	assert(tile);

	if (!tile)
		return;       // should not be needed

	assert(tile->m_pTileImage);

	tile->m_bDirty = true;

	uint32 dwLocalX = dwX - dwTileX * m_dwTileResolution;
	uint32 dwLocalY = dwY - dwTileY * m_dwTileResolution;

	assert(dwLocalX < tile->m_pTileImage->GetWidth());
	assert(dwLocalY < tile->m_pTileImage->GetHeight());

	tile->m_pTileImage->ValueAt(dwLocalX, dwLocalY) = dwValue;
}

void CRGBLayer::FreeTile(CTerrainTextureTiles& rTile)
{
	delete rTile.m_pTileImage;
	rTile.m_pTileImage = 0;

	rTile.m_bDirty = false;
	rTile.m_timeLastUsed = CTimeValue();

	// re-calculate memory usage
	m_dwCurrentTileMemory = 0;
	for (std::vector<CTerrainTextureTiles>::iterator it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it)
		m_dwCurrentTileMemory += ((*it).m_pTileImage) ? ((*it).m_pTileImage->GetSize()) : 0;
}

void CRGBLayer::ConsiderGarbageCollection()
{
	while (m_dwCurrentTileMemory > m_dwMaxTileMemory)
	{
		CTerrainTextureTiles* pOldestTile = FindOldestTileToFree();

		if (pOldestTile)
			FreeTile(*pOldestTile);
		else
			return;
	}
}

void CRGBLayer::FreeData()
{
	ClosePakForLoading();

	std::vector<CTerrainTextureTiles>::iterator it;

	for (it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it)
	{
		CTerrainTextureTiles& ref = *it;

		FreeTile(ref);
	}

	m_TerrainTextureTiles.clear();

	m_dwTileCountX = 0;
	m_dwTileCountY = 0;
	m_dwTileResolution = 0;
	m_dwCurrentTileMemory = 0;
}

CRGBLayer::CTerrainTextureTiles* CRGBLayer::FindOldestTileToFree()
{
	std::vector<CTerrainTextureTiles>::iterator it;
	uint32 dwI = 0;

	CTerrainTextureTiles* pRet = 0;

	for (it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it, ++dwI)
	{
		CTerrainTextureTiles& ref = *it;

		if (ref.m_pTileImage)                 // something to free
			if (!ref.m_bDirty)                  // hasn't changed
			{
				if (pRet == 0 || ref.m_timeLastUsed < pRet->m_timeLastUsed)
					pRet = &ref;
			}
	}

	return pRet;
}

namespace
{
CryCriticalSection csLoadTileIfNeeded;
}

bool CRGBLayer::OpenPakForLoading()
{
	AUTO_LOCK(csLoadTileIfNeeded);

	if (m_bPakOpened)
		return true;

	const string pakFilename = GetFullFileName();

	ICryPak* pIPak = GetIEditorImpl()->GetSystem()->GetIPak();
	m_bPakOpened = pIPak->IsFileExist(pakFilename) && pIPak->OpenPack(pakFilename);

	// if the pak file wasn't created yet
	if (!m_bPakOpened)
	{
		// create PAK file so we don't get errors when loading
		SaveAndFreeMemory(true);

		m_bPakOpened = pIPak->OpenPack(pakFilename);
	}

	return m_bPakOpened;
}

bool CRGBLayer::ClosePakForLoading()
{
	CRY_ASSERT(GetIEditorImpl() && GetIEditorImpl()->GetSystem());

	AUTO_LOCK(csLoadTileIfNeeded);

	// Here we need to unconditionally close the pak file, even if it was opened somewhere else, e.g. see CGameEngine::LoadLevel.
	ICryPak* pIPak = GetIEditorImpl()->GetSystem()->GetIPak();
	const string pakFilename = GetFullFileName();
	m_bPakOpened = !pIPak->ClosePack(pakFilename);

	if (m_bPakOpened)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to close pak file %s", pakFilename.c_str());
	}
	return !m_bPakOpened;
}

CRGBLayer::CTerrainTextureTiles* CRGBLayer::GetTilePtr(const uint32 dwTileX, const uint32 dwTileY)
{
	assert(dwTileX < m_dwTileCountX);
	assert(dwTileY < m_dwTileCountY);

	if (dwTileX >= m_dwTileCountX || dwTileY >= m_dwTileCountY)
		return 0;     // to avoid crash

	return &m_TerrainTextureTiles[dwTileX + dwTileY * m_dwTileCountX];
}

CRGBLayer::CTerrainTextureTiles* CRGBLayer::LoadTileIfNeeded(const uint32 dwTileX, const uint32 dwTileY, bool bNoGarbageCollection)
{
	CTerrainTextureTiles* pTile = GetTilePtr(dwTileX, dwTileY);

	if (!pTile)
		return 0;

	if (!pTile->m_pTileImage)
	{
		AUTO_LOCK(csLoadTileIfNeeded);

		pTile->m_timeLastUsed = GetIEditorImpl()->GetSystem()->GetITimer()->GetAsyncCurTime();

		if (!pTile->m_pTileImage)
		{
			CryLog("Loading RGB Layer Tile: TilePos=(%d, %d) MemUsage=%.1fMB", dwTileX, dwTileY, float(m_dwCurrentTileMemory) / 1024.f / 1024.f);   // debugging

			if (!bNoGarbageCollection)
				ConsiderGarbageCollection();

			string pathRel = GetIEditorImpl()->GetGameEngine()->GetLevelPath();
			CImageEx* pTileImage = pTile->m_pTileImage;

			if (OpenPakForLoading())
			{
				CMemoryBlock mem;
				uint32 dwWidth = 0, dwHeight = 0;
				uint8* pSrc8 = 0;
				int bpp = 3;

				CCryFile file;

				char szTileName[128];

				cry_sprintf(szTileName, "Tile%d_%d.raw", dwTileX, dwTileY);

				if (file.Open(PathUtil::AddBackslash(pathRel.GetString()) + szTileName, "rb"))
				{
					if (file.ReadType(&dwWidth)
					    && file.ReadType(&dwHeight)
					    && dwWidth > 0 && dwHeight > 0)
					{
						assert(dwWidth == dwHeight);

						if (mem.Allocate(dwWidth * dwHeight * bpp))
						{
							if (!file.ReadRaw(mem.GetBuffer(), dwWidth * dwHeight * bpp))
							{
								CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Error reading tile raw data from %s!", szTileName);
								dwWidth = 0;
							}
							pSrc8 = (uint8*)mem.GetBuffer();
						}
						else
						{
							// error
							assert(0);
						}
					}
					else
					{
						// error
						assert(0);
					}

					file.Close();   // need to close the file to be able tot close the pak
				}
				else
				{
					//				CryLog("CRGBLayer::LoadTileIfNeeded not found in pak ...");		// debugging
				}

				if (pSrc8 && dwWidth > 0 && dwHeight > 0)
				{
					if (!pTileImage)
						pTileImage = new CImageEx();

					pTile->m_dwSize = dwWidth;

					if (pTileImage->Allocate(dwWidth, dwHeight))
					{
						uint8* pDst8 = (uint8*)pTileImage->GetData();

						for (uint32 dwI = 0; dwI < dwWidth * dwHeight; ++dwI)
						{
							*pDst8++ = *pSrc8++;
							*pDst8++ = *pSrc8++;
							*pDst8++ = *pSrc8++;
							*pDst8++ = 0;
						}
					}
					else
					{
						// error
						assert(0);
					}
				}
			}

			// still not there - then create an empty tile
			if (!pTileImage)
			{
				pTileImage = new CImageEx();

				pTile->m_dwSize = m_dwTileResolution;

				pTileImage->Allocate(m_dwTileResolution, m_dwTileResolution);
				pTileImage->Fill(0xff);

				// for more convenience in the beginning:
				CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(0);

				if (pLayer)
				{
					pLayer->PrecacheTexture();

					CImagePainter painter;

					uint32 dwTileSize = pTileImage->GetWidth();
					uint32 dwTileSizeReduced = dwTileSize - 1;
					uint32 dwTileOffsetX = dwTileX * dwTileSizeReduced;
					uint32 dwTileOffsetY = dwTileY * dwTileSizeReduced;

					painter.FillWithPattern(*pTileImage, dwTileOffsetX, dwTileOffsetY, pLayer->m_texture);
				}

			}

			pTile->m_bDirty = false;

			pTile->m_pTileImage = pTileImage;

			// re-calculate memory usage
			m_dwCurrentTileMemory = 0;
			for (std::vector<CTerrainTextureTiles>::iterator it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it)
				m_dwCurrentTileMemory += ((*it).m_pTileImage) ? ((*it).m_pTileImage->GetSize()) : 0;

			//		CryLog("CRGBLayer::LoadTileIfNeeded ... %d",m_dwCurrentTileMemory);		// debugging
		}
	}

	return pTile;
}

bool CRGBLayer::IsDirty() const
{
	if (m_bInfoDirty)
		return true;

	std::vector<CTerrainTextureTiles>::const_iterator it;

	for (it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it)
	{
		const CTerrainTextureTiles& ref = *it;

		if (ref.m_pTileImage)
			if (ref.m_bDirty)
				return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
string CRGBLayer::GetFullFileName()
{
	string pathRel = GetIEditorImpl()->GetGameEngine()->GetLevelPath();
	string pathPak = PathUtil::Make(pathRel, m_TerrainRGBFileName);

	return pathPak;
}

//////////////////////////////////////////////////////////////////////////
bool CRGBLayer::WouldSaveSucceed()
{
	if (!IsDirty())
	{
		return true;    // no need to save
	}

	if (!ClosePakForLoading())
	{
		return false;
	}

	const string pakFilename = GetFullFileName();
	if (!CFileUtil::OverwriteFile(pakFilename.c_str()))
	{
		return false;
	}

	// Create pak file
	{
		CPakFile pakFile;
		if (!pakFile.Open(pakFilename.c_str()))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Unable to create pak file %s.", pakFilename.c_str());
			return false;   // save would fail
		}
		pakFile.Close();
	}
	return true;      // save would work
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::Offset(int iTilesX, int iTilesY)
{
	int yStart, yEnd, yStep;
	if (iTilesY < 0)
	{
		yStart = 0;
		yEnd = (int) m_dwTileCountY;
		yStep = 1;
	}
	else
	{
		yStart = (int) m_dwTileCountY - 1;
		yEnd = -1;
		yStep = -1;
	}
	int xStart, xEnd, xStep;
	if (iTilesX < 0)
	{
		xStart = 0;
		xEnd = (int) m_dwTileCountX;
		xStep = 1;
	}
	else
	{
		xStart = (int) m_dwTileCountX - 1;
		xEnd = -1;
		xStep = -1;
	}
	for (int y = yStart; y != yEnd; y += yStep)
	{
		int sy = y - iTilesY;
		for (int x = xStart; x != xEnd; x += xStep)
		{
			int sx = x - iTilesX;
			CTerrainTextureTiles* pTile = GetTilePtr(x, y);
			FreeTile(*pTile);
			CTerrainTextureTiles* pSrc = (sy >= 0 && sy < m_dwTileCountY && sx >= 0 && sx < m_dwTileCountX) ? GetTilePtr(sx, sy) : 0;
			if (pSrc)
			{
				*pTile = *pSrc;
				// not using FreeTile(pSrc) because we don't want the image to be deleted
				pSrc->m_pTileImage = 0;
				pSrc->m_bDirty = false;
				pSrc->m_timeLastUsed = CTimeValue();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::LoadAll()
{
	if (!OpenPakForLoading())
		return;
	for (uint32 y = 0; y < m_dwTileCountY; ++y)
	{
		for (uint32 x = 0; x < m_dwTileCountX; ++x)
		{
			LoadTileIfNeeded(x, y, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::Resize(uint32 dwTileCountX, uint32 dwTileCountY, uint32 dwTileResolution)
{
	CImageEx** pImages = new CImageEx*[dwTileCountX * dwTileCountY];
	uint32 x, y;
	for (y = 0; y < dwTileCountY; ++y)
	{
		for (x = 0; x < dwTileCountX; ++x)
		{
			CImageEx*& pImg = pImages[y * dwTileCountX + x];
			pImg = new CImageEx();
			pImg->Allocate(dwTileResolution, dwTileResolution);
			float l = ((float) x) / dwTileCountX;
			float t = ((float) y) / dwTileCountY;
			float r = ((float) x + 1) / dwTileCountX;
			float b = ((float) y + 1) / dwTileCountY;
			GetSubImageStretched(l, t, r, b, *pImg);
		}
	}
	FreeData();
	AllocateTiles(dwTileCountX, dwTileCountY, dwTileResolution);
	for (y = 0; y < dwTileCountY; ++y)
	{
		for (x = 0; x < dwTileCountX; ++x)
		{
			CTerrainTextureTiles* pTile = GetTilePtr(x, y);
			pTile->m_pTileImage = pImages[y * dwTileCountX + x];
			pTile->m_dwSize = dwTileResolution;
			pTile->m_bDirty = true;
		}
	}
	delete pImages;
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::CleanupCache()
{
	ClosePakForLoading();

	std::vector<CTerrainTextureTiles>::iterator it;

	for (it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it)
	{
		CTerrainTextureTiles& ref = *it;

		if (ref.m_pTileImage)
			if (!ref.m_bDirty)
				FreeTile(ref);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CRGBLayer::SaveAndFreeMemory(const bool bForceFileCreation)
{
	if (!ClosePakForLoading())
	{
		return false;
	}

	if (!bForceFileCreation && !IsDirty())
	{
		return true;
	}

	// create pak file
	const string pakFilename = GetFullFileName();
	CPakFile pakFile;
	if (!pakFile.Open(pakFilename))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to open pak file %s for writing.", pakFilename.c_str());
		return false;
	}

	std::vector<CTerrainTextureTiles>::iterator it;
	uint32 dwI = 0;

	for (it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it, ++dwI)
	{
		CTerrainTextureTiles& ref = *it;

		uint32 dwTileX = dwI % m_dwTileCountX;
		uint32 dwTileY = dwI / m_dwTileCountX;

		if (ref.m_pTileImage)
		{
			if (ref.m_bDirty)
			{
				CMemoryBlock mem;
				ExportSegment(mem, dwTileX, dwTileY, false);

				char szTileName[20];

				cry_sprintf(szTileName, "Tile%d_%d.raw", dwTileX, dwTileY);

				if (!pakFile.UpdateFile(PathUtil::AddBackslash(GetIEditorImpl()->GetGameEngine()->GetLevelPath().GetString()) + szTileName, mem))
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to update tile file %s.", szTileName);
				}
			}

			// free tile
			FreeTile(ref);
		}
	}

	pakFile.Close();

	return true;
}

void CRGBLayer::ClipTileRect(CRect& inoutRect) const
{
	if ((int)(inoutRect.left) < 0)
		inoutRect.left = 0;

	if ((int)(inoutRect.top) < 0)
		inoutRect.top = 0;

	if ((int)(inoutRect.right) > m_dwTileCountX)
		inoutRect.right = m_dwTileCountX;

	if ((int)(inoutRect.bottom) > m_dwTileCountY)
		inoutRect.bottom = m_dwTileCountY;
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::PaintBrushWithPatternTiled(const float fpx, const float fpy, SEditorPaintBrush& brush, const CImageEx& imgPattern)
{
	assert(brush.fRadius >= 0.0f && brush.fRadius <= 1.0f);

	float fOldRadius = brush.fRadius;

	uint32 dwMaxRes = CalcMaxLocalResolution(0, 0, 1, 1);

	CRect recTiles = CRect(
	  (uint32)floor((fpx - brush.fRadius - 2.0f / dwMaxRes) * m_dwTileCountX),
	  (uint32)floor((fpy - brush.fRadius - 2.0f / dwMaxRes) * m_dwTileCountY),
	  (uint32)ceil((fpx + brush.fRadius + 2.0f / dwMaxRes) * m_dwTileCountX),
	  (uint32)ceil((fpy + brush.fRadius + 2.0f / dwMaxRes) * m_dwTileCountY));

	ClipTileRect(recTiles);

	CImagePainter painter;

	for (uint32 dwTileY = recTiles.top; dwTileY < recTiles.bottom; ++dwTileY)
		for (uint32 dwTileX = recTiles.left; dwTileX < recTiles.right; ++dwTileX)
		{
			CTerrainTextureTiles* tile = LoadTileIfNeeded(dwTileX, dwTileY);
			assert(tile);

			assert(tile->m_pTileImage);

			tile->m_bDirty = true;

			uint32 dwTileSize = tile->m_pTileImage->GetWidth();
			uint32 dwTileSizeReduced = dwTileSize - 1;    // usable area in tile is limited because of bilinear filtering
			uint32 dwTileOffsetX = dwTileX * dwTileSizeReduced;
			uint32 dwTileOffsetY = dwTileY * dwTileSizeReduced;

			//			float fX=fpx*(dwTileSizeReduced*m_dwTileCountX)+0.5f, fY=fpy*(dwTileSizeReduced*m_dwTileCountY)+0.5f;
			float fScaleX = (dwTileSizeReduced * m_dwTileCountX), fScaleY = (dwTileSizeReduced * m_dwTileCountY);

			brush.fRadius = fOldRadius * (dwTileSize * m_dwTileCountX);

			painter.PaintBrushWithPattern(fpx, fpy, *tile->m_pTileImage, dwTileOffsetX, dwTileOffsetY, fScaleX, fScaleY, brush, imgPattern);
			/*
			      // debug edges
			      tile->m_pTileImage->ValueAt(0,0)=0x00ff0000;
			      tile->m_pTileImage->ValueAt(dwTileSize-1,0)=0x00ff00ff;
			      tile->m_pTileImage->ValueAt(0,dwTileSize-1)=0x0000ff00;
			      tile->m_pTileImage->ValueAt(dwTileSize-1,dwTileSize-1)=0x000000ff;
			 */
		}

	// fix borders - minor quality improvement (can be optimized)
	for (uint32 dwTileY = recTiles.top; dwTileY < recTiles.bottom; ++dwTileY)
		for (uint32 dwTileX = recTiles.left; dwTileX < recTiles.right; ++dwTileX)
		{
			assert(dwTileX < m_dwTileCountX);
			assert(dwTileY < m_dwTileCountY);

			CTerrainTextureTiles* tile1 = LoadTileIfNeeded(dwTileX, dwTileY);
			assert(tile1);
			assert(tile1->m_pTileImage);
			assert(tile1->m_bDirty);

			uint32 dwTileSize1 = tile1->m_pTileImage->GetWidth();

			if (dwTileX != recTiles.left)  // vertical border between tile2 and tile 1
			{
				CTerrainTextureTiles* tile2 = LoadTileIfNeeded(dwTileX - 1, dwTileY);
				assert(tile2);
				assert(tile2->m_pTileImage);
				assert(tile2->m_bDirty);

				uint32 dwTileSize2 = tile2->m_pTileImage->GetWidth();
				uint32 dwTileSizeMax = max(dwTileSize1, dwTileSize2);

				for (uint32 dwI = 0; dwI < dwTileSizeMax; ++dwI)
				{
					uint32& dwC2 = tile2->m_pTileImage->ValueAt(dwTileSize2 - 1, (dwI * (dwTileSize2 - 1)) / (dwTileSizeMax - 1));
					uint32& dwC1 = tile1->m_pTileImage->ValueAt(0, (dwI * (dwTileSize1 - 1)) / (dwTileSizeMax - 1));

					uint32 dwAvg = ColorB::ComputeAvgCol_Fast(dwC1, dwC2);

					dwC1 = dwAvg;
					dwC2 = dwAvg;
				}
			}

			if (dwTileY != recTiles.top) // horizontal border between tile2 and tile 1
			{
				CTerrainTextureTiles* tile2 = LoadTileIfNeeded(dwTileX, dwTileY - 1);
				assert(tile2);
				assert(tile2->m_pTileImage);
				assert(tile2->m_bDirty);

				uint32 dwTileSize2 = tile2->m_pTileImage->GetWidth();
				uint32 dwTileSizeMax = max(dwTileSize1, dwTileSize2);

				for (uint32 dwI = 0; dwI < dwTileSizeMax; ++dwI)
				{
					uint32& dwC2 = tile2->m_pTileImage->ValueAt((dwI * (dwTileSize2 - 1)) / (dwTileSizeMax - 1), dwTileSize2 - 1);
					uint32& dwC1 = tile1->m_pTileImage->ValueAt((dwI * (dwTileSize1 - 1)) / (dwTileSizeMax - 1), 0);

					uint32 dwAvg = ColorB::ComputeAvgCol_Fast(dwC1, dwC2);

					dwC1 = dwAvg;
					dwC2 = dwAvg;
				}
			}
		}

	brush.fRadius = fOldRadius;
}

uint32 CRGBLayer::GetTileResolution(const uint32 dwTileX, const uint32 dwTileY)
{
	CTerrainTextureTiles* tile = GetTilePtr(dwTileX, dwTileY);
	assert(tile);

	if (!tile->m_dwSize)   // not size info yet - load the tile
	{
		tile = LoadTileIfNeeded(dwTileX, dwTileY);
		assert(tile);

		m_bInfoDirty = true;    // save is required to update the dwSize

		assert(tile->m_pTileImage);
	}

	assert(tile->m_dwSize);

	return tile->m_dwSize;
}

uint32 CRGBLayer::CalcMaxLocalResolution(const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom)
{
	assert(fSrcRight >= fSrcLeft);
	assert(fSrcBottom >= fSrcTop);

	CRect recTiles = CRect((uint32)floor(fSrcLeft * m_dwTileCountX), (uint32)floor(fSrcTop * m_dwTileCountY),
	                       (uint32)ceil(fSrcRight * m_dwTileCountX), (uint32)ceil(fSrcBottom * m_dwTileCountY));

	ClipTileRect(recTiles);

	uint32 dwRet = 0;

	for (uint32 dwTileY = recTiles.top; dwTileY < recTiles.bottom; ++dwTileY)
		for (uint32 dwTileX = recTiles.left; dwTileX < recTiles.right; ++dwTileX)
		{
			uint32 dwSize = GetTileResolution(dwTileX, dwTileY);
			assert(dwSize);

			uint32 dwLocalWidth = dwSize * m_dwTileCountX;
			uint32 dwLocalHeight = dwSize * m_dwTileCountY;

			if (dwRet < dwLocalWidth)
				dwRet = dwLocalWidth;
			if (dwRet < dwLocalHeight)
				dwRet = dwLocalHeight;
		}

	return dwRet;
}

bool CRGBLayer::IsAllocated() const
{
	return m_TerrainTextureTiles.size() != 0;
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::Serialize(XmlNodeRef& node, bool bLoading)
{
	////////////////////////////////////////////////////////////////////////
	// Save or restore the class
	////////////////////////////////////////////////////////////////////////
	if (bLoading)
	{
		m_dwTileCountX = m_dwTileCountY = m_dwTileResolution = 0;

		// Texture
		node->getAttr("TileCountX", m_dwTileCountX);
		node->getAttr("TileCountY", m_dwTileCountY);
		node->getAttr("TileResolution", m_dwTileResolution);

		if (m_dwTileCountX && m_dwTileCountY && m_dwTileResolution)          // if info is valid
			AllocateTiles(m_dwTileCountX, m_dwTileCountY, m_dwTileResolution);
		else
			FreeData();

		XmlNodeRef mainNode = node->findChild("RGBLayer");

		if (mainNode)    // old nodes might not have this info
		{
			XmlNodeRef tiles = mainNode->findChild("Tiles");
			if (tiles)
			{
				int numObjects = tiles->getChildCount();

				for (int i = 0; i < numObjects; i++)
				{
					XmlNodeRef tile = tiles->getChild(i);

					uint32 dwX = 0, dwY = 0, dwSize = 0;

					tile->getAttr("X", dwX);
					tile->getAttr("Y", dwY);
					tile->getAttr("Size", dwSize);

					CTerrainTextureTiles* pPtr = GetTilePtr(dwX, dwY);
					assert(pPtr);

					if (pPtr)
						pPtr->m_dwSize = dwSize;
				}
			}
		}
	}
	else
	{
		// Storing
		node = XmlHelpers::CreateXmlNode("TerrainTexture");

		// Texture
		node->setAttr("TileCountX", m_dwTileCountX);
		node->setAttr("TileCountY", m_dwTileCountY);
		node->setAttr("TileResolution", m_dwTileResolution);

		SaveAndFreeMemory();

		XmlNodeRef mainNode = node->newChild("RGBLayer");
		XmlNodeRef tiles = mainNode->newChild("Tiles");

		for (uint32 dwY = 0; dwY < m_dwTileCountY; ++dwY)
			for (uint32 dwX = 0; dwX < m_dwTileCountX; ++dwX)
			{
				XmlNodeRef obj = tiles->newChild("tile");

				CTerrainTextureTiles* pPtr = GetTilePtr(dwX, dwY);
				assert(pPtr);

				uint32 dwSize = pPtr->m_dwSize;

				if (dwSize || m_bNextSerializeForceSizeSave)
				{
					obj->setAttr("X", dwX);
					obj->setAttr("Y", dwY);
					obj->setAttr("Size", dwSize);
				}
			}

		m_bNextSerializeForceSizeSave = false;

		m_bInfoDirty = false;
	}
}

void CRGBLayer::AllocateTiles(const uint32 dwTileCountX, const uint32 dwTileCountY, const uint32 dwTileResolution)
{
	assert(dwTileCountX);
	assert(dwTileCountY);

	// free
	m_TerrainTextureTiles.clear();
	m_TerrainTextureTiles.resize(dwTileCountX * dwTileCountY);

	m_dwTileCountX = dwTileCountX;
	m_dwTileCountY = dwTileCountY;
	m_dwTileResolution = dwTileResolution;

	CryLog("CRGBLayer::AllocateTiles %dx%d tiles %d => %dx%d texture", dwTileCountX, dwTileCountY, dwTileResolution, dwTileCountX * dwTileResolution, dwTileCountY * dwTileResolution);    // debugging
}

void CRGBLayer::SetSubImageRGBLayer(const uint32 dwDstX, const uint32 dwDstY, const CImageEx& rTileImage)
{
	uint32 dwWidth = rTileImage.GetWidth();
	uint32 dwHeight = rTileImage.GetHeight();

	for (uint32 dwTexelY = 0; dwTexelY < dwHeight; ++dwTexelY)
		for (uint32 dwTexelX = 0; dwTexelX < dwWidth; ++dwTexelX)
			SetValueAt(dwDstX + dwTexelX, dwDstY + dwTexelY, rTileImage.ValueAt(dwTexelX, dwTexelY));
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::GetMemoryUsage(ICrySizer* pSizer)
{
	std::vector<CTerrainTextureTiles>::iterator it;

	for (it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it)
	{
		CTerrainTextureTiles& ref = *it;

		if (ref.m_pTileImage)                  // something to free
			pSizer->Add((char*)ref.m_pTileImage->GetData(), ref.m_pTileImage->GetSize());
	}

	pSizer->Add(m_TerrainTextureTiles);

	pSizer->Add(*this);
}

//////////////////////////////////////////////////////////////////////////
uint32 CRGBLayer::CalcMinRequiredTextureExtend()
{
	uint32 dwMaxLocalExtend = 0;

	for (uint32 dwTileY = 0; dwTileY < m_dwTileCountY; ++dwTileY)
		for (uint32 dwTileX = 0; dwTileX < m_dwTileCountX; ++dwTileX)
			dwMaxLocalExtend = max(dwMaxLocalExtend, GetTileResolution(dwTileX, dwTileY));

	return max(m_dwTileCountX, m_dwTileCountY) * dwMaxLocalExtend;
}

//////////////////////////////////////////////////////////////////////////
void CRGBLayer::Debug()
{
	std::vector<CTerrainTextureTiles>::iterator it;

	uint32 dwI = 0;

	for (it = m_TerrainTextureTiles.begin(); it != m_TerrainTextureTiles.end(); ++it, ++dwI)
	{
		CTerrainTextureTiles& ref = *it;

		if (ref.m_pTileImage)
		{
			char szName[256];

			cry_sprintf(szName, "RGBLayerTile%dx%d.bmp", dwI % m_dwTileCountX, dwI / m_dwTileCountX);
			CImageUtil::SaveBitmap(szName, *ref.m_pTileImage);
		}
	}
}

/*
   bool CRGBLayer::IsSaveNeeded() const
   {
   std::vector<CTerrainTextureTiles>::const_iterator it, end=m_TerrainTextureTiles.end();

   for(it=m_TerrainTextureTiles.begin();it!=end;++it,++dwI)
   {
    const CTerrainTextureTiles &ref = *it;

    if(ref->m_bDirty)
      return true;
   }

   return false;
   }
 */

//////////////////////////////////////////////////////////////////////////
bool CRGBLayer::RefineTiles()
{
	CRGBLayer out("TerrainTexture2.pak");

	assert(m_dwTileCountX);
	assert(m_dwTileCountY);
	assert(m_dwTileResolution / 2);

	out.AllocateTiles(m_dwTileCountX * 2, m_dwTileCountY * 2, m_dwTileResolution / 2);

	std::vector<CTerrainTextureTiles>::iterator it, end = m_TerrainTextureTiles.end();
	uint32 dwI = 0;

	for (it = m_TerrainTextureTiles.begin(); it != end; ++it, ++dwI)
	{
		CTerrainTextureTiles& ref = *it;

		uint32 dwTileX = dwI % m_dwTileCountX;
		uint32 dwTileY = dwI / m_dwTileCountY;

		LoadTileIfNeeded(dwTileX, dwTileY);

		assert(ref.m_dwSize);

		for (uint32 dwY = 0; dwY < 2; ++dwY)
			for (uint32 dwX = 0; dwX < 2; ++dwX)
			{
				CTerrainTextureTiles* pOutTile = out.GetTilePtr(dwTileX * 2 + dwX, dwTileY * 2 + dwY);
				assert(pOutTile);

				pOutTile->m_dwSize = ref.m_dwSize / 2;
				pOutTile->m_pTileImage = new CImageEx();
				pOutTile->m_pTileImage->Allocate(pOutTile->m_dwSize, pOutTile->m_dwSize);
				pOutTile->m_timeLastUsed = GetIEditorImpl()->GetSystem()->GetITimer()->GetAsyncCurTime();
				pOutTile->m_bDirty = true;
				m_bInfoDirty = true;

				float fSrcLeft = (float)(dwTileX * 2 + dwX) / (float)(m_dwTileCountX * 2);
				float fSrcTop = (float)(dwTileY * 2 + dwY) / (float)(m_dwTileCountY * 2);
				float fSrcRight = (float)(dwTileX * 2 + dwX + 1) / (float)(m_dwTileCountX * 2);
				float fSrcBottom = (float)(dwTileY * 2 + dwY + 1) / (float)(m_dwTileCountY * 2);

				GetSubImageStretched(fSrcLeft, fSrcTop, fSrcRight, fSrcBottom, *pOutTile->m_pTileImage, false);
			}

		if (!out.SaveAndFreeMemory(true))
		{
			assert(0);
			return false;
		}
	}

	if (!ClosePakForLoading())
	{
		return false;
	}

	string path = GetFullFileName();
	string path2 = out.GetFullFileName();

	MoveFile(path, PathUtil::ReplaceExtension((const char*)path, "bak"));
	MoveFile(path2, path);

	AllocateTiles(m_dwTileCountX * 2, m_dwTileCountY * 2, m_dwTileResolution);
	m_bNextSerializeForceSizeSave = true;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CRGBLayer::ImportExportBlock(const char* pFileName, int nSrcLeft, int nSrcTop, int nSrcRight, int nSrcBottom, uint32* outSquare, bool bIsImport)
{
	uint32 dwCountX = GetTileCountX();
	uint32 dwCountY = GetTileCountY();

	int iX1 = nSrcLeft;
	int iY1 = nSrcTop;
	int iX2 = nSrcRight;
	int iY2 = nSrcBottom;

	if (iX2 >= dwCountX)
		iX2 = dwCountX - 1;
	if (iY2 >= dwCountY)
		iY2 = dwCountY - 1;

	if (0.001f + iX2 > nSrcRight && iX2 > iX1)
		iX2--;
	if (0.001f + iY2 > nSrcBottom && iY2 > iY1)
		iY2--;

	uint32 maxRes = 0;
	for (int iX = iX1; iX <= iX2; iX++)
		for (int iY = iY1; iY <= iY2; iY++)
		{
			if (iX < 0 || iY < 0)
				continue;

			uint32 dwRes = GetTileResolution((uint32)iX, (uint32)iY);
			if (dwRes > maxRes)
				maxRes = dwRes;
		}

	int x1 = int (nSrcLeft * maxRes);
	int y1 = int (nSrcTop * maxRes);
	int x2 = int (nSrcRight * maxRes);
	int y2 = int (nSrcBottom * maxRes);

	// loading big image by tile
	bool bIsPerParts = false;

	*outSquare = (x2 - x1) * (y2 - y1);

	CImageEx img;
	if (bIsImport)
	{
		if (pFileName && *pFileName)
		{
			// if image is big load by parts
			if (x2 - x1 > 2048 || y2 - y1 > 2048)
				bIsPerParts = true;

			if (!bIsPerParts)
			{
				CImageEx newImage;
				if (!CImageUtil::LoadBmp(pFileName, newImage))
				{
					CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Error: Can't load BMP file. Probably out of memory."));
					return false;
				}
				img.RotateOrt(newImage, 3);
				img.SwapRedAndBlue();
			}

		}
		else
		{
			CClipboard cb;
			if (!cb.GetImage(img))
				return false;
		}
	}
	else
		img.Allocate(x2 - x1, y2 - y1);

	int ststx1;
	int ststy1;
	bool bIsStst = true;

	int nTotalSectors = (iX2 - iX1 + 1) * (iY2 - iY1 + 1);
	int nCurrSectorNum = 0;
	CWaitProgress progress("Import/Export Terrain Surface Texture");

	for (int iX = iX1; iX <= iX2; iX++)
		for (int iY = iY1; iY <= iY2; iY++)
		{
			if (iX < 0 || iY < 0 || iX >= dwCountX || iY >= dwCountY)
				continue;

			if (!progress.Step((100 * nCurrSectorNum) / nTotalSectors))
				return false;

			// need for big areas
			if (!nCurrSectorNum && !(nCurrSectorNum % 16))
				SaveAndFreeMemory(true);

			nCurrSectorNum++;

			CTerrainTextureTiles* pTile = LoadTileIfNeeded((uint32)iX, (uint32)iY);
			if (!pTile)
				continue;

			pTile->m_bDirty = true;

			uint32 dwRes = GetTileResolution((uint32)iX, (uint32)iY);

			int stx1 = x1 - iX * maxRes;
			if (stx1 < 0) stx1 = 0;
			int sty1 = y1 - iY * maxRes;
			if (sty1 < 0) sty1 = 0;

			int stx2 = x2 - iX * maxRes;
			if (stx2 > maxRes) stx2 = maxRes;
			int sty2 = y2 - iY * maxRes;
			if (sty2 > maxRes) sty2 = maxRes;

			if (bIsPerParts)
			{
				if (bIsStst)
				{
					ststx1 = stx1;
					ststy1 = sty1;
					bIsStst = false;
				}
				RECT rc = { iX* maxRes + stx1 - iX1 * maxRes - ststx1, iY * maxRes + sty1 - iY1 * maxRes - ststy1, iX * maxRes + stx2 - iX1 * maxRes - ststx1, iY * maxRes + sty2 - iY1 * maxRes - ststy1 };
				if (!CImageUtil::LoadBmp(pFileName, img, rc))
				{
					CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Error: Can't load BMP file. Probably out of memory."));
					return false;
				}
				img.SwapRedAndBlue();
			}

			if (dwRes == maxRes)
			{
				for (int x = stx1; x < stx2; x++)
					for (int y = sty1; y < sty2; y++)
					{
						if (bIsImport)
						{
							if (bIsPerParts)
								pTile->m_pTileImage->ValueAt(x, y) = img.ValueAt(x - stx1, y - sty1);
							else
							{
								if (x + iX * maxRes - x1 < img.GetWidth() && y + iY * maxRes - y1 < img.GetHeight())
									pTile->m_pTileImage->ValueAt(x, y) = img.ValueAt(x + iX * maxRes - x1, y + iY * maxRes - y1);
							}
						}
						else
							img.ValueAt(x + iX * maxRes - x1, y + iY * maxRes - y1) = pTile->m_pTileImage->ValueAt(x, y);
					}
			}
			else
			{
				CImageEx part;
				part.Allocate(stx2 - stx1, sty2 - sty1);
				if (!bIsImport)
					GetSubImageStretched(((float)stx1 / maxRes + iX) / dwCountX, ((float)sty1 / maxRes + iY) / dwCountY,
					                     ((float)stx2 / maxRes + iX) / dwCountX, ((float)sty2 / maxRes + iY) / dwCountY, part, true);

				for (int x = stx1; x < stx2; x++)
					for (int y = sty1; y < sty2; y++)
					{
						if (bIsImport)
						{
							if (bIsPerParts)
								part.ValueAt(x - stx1, y - sty1) = img.ValueAt(x - stx1, y - sty1);
							else
								part.ValueAt(x - stx1, y - sty1) = img.ValueAt(x + iX * maxRes - x1, y + iY * maxRes - y1);
						}
						else
							img.ValueAt(x + iX * maxRes - x1, y + iY * maxRes - y1) = part.ValueAt(x - stx1, y - sty1);
					}
				if (bIsImport)
					SetSubImageStretched(((float)stx1 / maxRes + iX) / dwCountX, ((float)sty1 / maxRes + iY) / dwCountY,
					                     ((float)stx2 / maxRes + iX) / dwCountX, ((float)sty2 / maxRes + iY) / dwCountY, part, true);

			}
		}

	if (!bIsImport)
	{

		if (pFileName && *pFileName)
		{
			img.SwapRedAndBlue();
			CImageEx newImage;
			newImage.RotateOrt(img, 1);
			CImageUtil::SaveBitmap(pFileName, newImage);
		}
		else
		{
			CClipboard cb;
			cb.PutImage(img);
		}
	}

	return true;
}

bool CRGBLayer::ExportSegment(CMemoryBlock& mem, uint32 dwTileX, uint32 dwTileY, bool bCompress)
{
	CTerrainTextureTiles* pTile = LoadTileIfNeeded(dwTileX, dwTileY);
	if (!pTile || !pTile->m_pTileImage)
	{
		assert(0);
		return false;
	}

	uint32 dwWidth = pTile->m_pTileImage->GetWidth(), dwHeight = pTile->m_pTileImage->GetHeight();

	assert(dwWidth);
	assert(dwHeight);

	CMemoryBlock tmpMem;
	CMemoryBlock& tmp = bCompress ? tmpMem : mem;

	const int bpp = 3;
	tmp.Allocate(sizeof(uint32) + sizeof(uint32) + dwWidth * dwHeight * bpp);

	uint32* pMem32 = (uint32*)tmp.GetBuffer();
	uint8* pDst8 = (uint8*)tmp.GetBuffer() + sizeof(uint32) + sizeof(uint32);
	uint8* pSrc8 = (uint8*)pTile->m_pTileImage->GetData();

	pMem32[0] = dwWidth;
	pMem32[1] = dwHeight;

	for (uint32 j = 0; j < dwWidth * dwHeight; ++j)
	{
		*pDst8++ = *pSrc8++;
		*pDst8++ = *pSrc8++;
		*pDst8++ = *pSrc8++;
		pSrc8++;
	}

	if (bCompress)
		tmp.Compress(mem);

	return true;
}

CImageEx* CRGBLayer::GetTileImage(int tileX, int tileY, bool setDirtyFlag /* = true */)
{
	CTerrainTextureTiles* tile = LoadTileIfNeeded(tileX, tileY);
	assert(tile != 0);
	if (setDirtyFlag)
		tile->m_bDirty = true;
	return tile->m_pTileImage;
}

void CRGBLayer::UnloadTile(int tileX, int tileY)
{
	CTerrainTextureTiles* pTile = GetTilePtr(tileX, tileY);
	if (!pTile)
		return;
	FreeTile(*pTile);
}

void TransferSubimageFast(CImageEx& source, const Rectf& srcRect, CImageEx& dest, const Rectf& destRect)
{
	// Images are expected to be in the same format, square and with size = 2^n, rectangles are also square and in relative coordinates.
	Recti srcRectI, destRectI;
	srcRectI.Min.x = (int)(srcRect.Min.x * source.GetWidth());  // Exchange x and y in for correct subtile coordinates
	srcRectI.Min.y = (int)(srcRect.Min.y * source.GetHeight());
	srcRectI.Max.x = (int)(srcRect.Max.x * source.GetWidth());
	srcRectI.Max.y = (int)(srcRect.Max.y * source.GetHeight());
	destRectI.Min.x = (int)(destRect.Min.x * dest.GetWidth());
	destRectI.Min.y = (int)(destRect.Min.y * dest.GetHeight());
	destRectI.Max.x = (int)(destRect.Max.x * dest.GetWidth());
	destRectI.Max.y = (int)(destRect.Max.y * dest.GetHeight());
	int nSrcRectSize = srcRectI.GetWidth();
	int nDestRectSize = destRectI.GetWidth();
	int nSrcImageSize = source.GetWidth();
	int nDestImageSize = dest.GetWidth();

	uint32* pSrcData = source.GetData() + srcRectI.Min.x + srcRectI.Min.y * nSrcImageSize;
	uint32* pDestData = dest.GetData() + destRectI.Min.x + destRectI.Min.y * nDestImageSize;

	if (nSrcRectSize > nDestRectSize)
	{
		// Downscaling
		int nStride = nSrcRectSize / nDestRectSize;
		for (int row = 0; row < nDestRectSize; ++row)
		{
			uint32* pSrcRow = pSrcData + row * nSrcImageSize * nStride;
			uint32* pDestRow = pDestData + row * nDestImageSize;
			for (int col = 0; col < nDestRectSize; ++col)
			{
				*(pDestRow++) = *pSrcRow;
				pSrcRow += nStride;
			}
		}
	}
	else if (nDestRectSize > nSrcRectSize)
	{
		// Upscaling
		int nScale = nDestRectSize / nSrcRectSize;
		for (int row = 0; row < nSrcRectSize; ++row)
		{
			uint32* pSrcRow = pSrcData + row * nSrcImageSize;
			for (int y = 0; y < nScale; ++y)
			{
				uint32* pDestRow = pDestData + (row * nScale + y) * nDestImageSize;
				for (int col = 0; col < nSrcRectSize; ++col)
				{
					for (int x = 0; x < nScale; ++x)
						*(pDestRow++) = *pSrcRow;
					++pSrcRow;
				}
			}
		}
	}
	else
	{
		// Copying
		for (int row = 0; row < nDestRectSize; ++row)
			memmove(pDestData + row * nDestImageSize, pSrcData + row * nSrcImageSize, nSrcRectSize * 4);
	}
}

void CRGBLayer::GetSubimageFast(const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, CImageEx& rOutImage)
{
	Rectf rectTiles(fSrcLeft * GetTileCountX(), fSrcTop * GetTileCountY(), fSrcRight * GetTileCountX(), fSrcBottom * GetTileCountY());
	float fTilesToRect = 1.0f / rectTiles.GetWidth();

	int nMinXTiles = (int)rectTiles.Min.x;
	int nMinYTiles = (int)rectTiles.Min.y;
	int nMaxXTiles = (int)(rectTiles.Max.x - 0.00001); // Don't consider the last incident tile to the right
	int nMaxYTiles = (int)(rectTiles.Max.y - 0.00001); // and to the bottom if only its boundary is touched.

	for (int nTileX = nMinXTiles; nTileX <= nMaxXTiles; ++nTileX)
		for (int nTileY = nMinYTiles; nTileY <= nMaxYTiles; ++nTileY)
		{
			CImageEx* pTileImage = GetTileImage(nTileX, nTileY, false);
			Rectf::Vec origin(nTileX, nTileY);
			Rectf srcRect;
			srcRect.Min.x = MAX(0.0f, rectTiles.Min.x - origin.x);
			srcRect.Min.y = MAX(0.0f, rectTiles.Min.y - origin.y);
			srcRect.Max.x = MIN(1.0f, rectTiles.Max.x - origin.x);
			srcRect.Max.y = MIN(1.0f, rectTiles.Max.y - origin.y);
			Rectf destRect;
			destRect.Min.x = MAX(0.0f, (origin.x - rectTiles.Min.x) * fTilesToRect);
			destRect.Min.y = MAX(0.0f, (origin.y - rectTiles.Min.y) * fTilesToRect);
			destRect.Max.x = MIN(1.0f, (origin.x + 1.0f - rectTiles.Min.x) * fTilesToRect);
			destRect.Max.y = MIN(1.0f, (origin.y + 1.0f - rectTiles.Min.y) * fTilesToRect);
			TransferSubimageFast(*pTileImage, srcRect, rOutImage, destRect);
		}
}

void CRGBLayer::SetSubimageFast(const float fDestLeft, const float fDestTop, const float fDestRight, const float fDestBottom, CImageEx& rSrcImage)
{
	Rectf rectTiles(fDestLeft * GetTileCountX(), fDestTop * GetTileCountY(), fDestRight * GetTileCountX(), fDestBottom * GetTileCountY());
	float fTilesToRect = 1.0f / rectTiles.GetWidth();

	int nMinXTiles = (int)rectTiles.Min.x;
	int nMinYTiles = (int)rectTiles.Min.y;
	int nMaxXTiles = (int)(rectTiles.Max.x - 0.00001); // Don't consider the last incident tile to the right
	int nMaxYTiles = (int)(rectTiles.Max.y - 0.00001); // and to the bottom if only its boundary is touched.

	for (int nTileX = nMinXTiles; nTileX <= nMaxXTiles; ++nTileX)
		for (int nTileY = nMinYTiles; nTileY <= nMaxYTiles; ++nTileY)
		{
			CImageEx* pTileImage = GetTileImage(nTileX, nTileY, false);
			Rectf::Vec origin(nTileX, nTileY);
			Rectf destRect;
			destRect.Min.x = MAX(0.0f, rectTiles.Min.x - origin.x);
			destRect.Min.y = MAX(0.0f, rectTiles.Min.y - origin.y);
			destRect.Max.x = MIN(1.0f, rectTiles.Max.x - origin.x);
			destRect.Max.y = MIN(1.0f, rectTiles.Max.y - origin.y);
			Rectf srcRect;
			srcRect.Min.x = MAX(0.0f, (origin.x - rectTiles.Min.x) * fTilesToRect);
			srcRect.Min.y = MAX(0.0f, (origin.y - rectTiles.Min.y) * fTilesToRect);
			srcRect.Max.x = MIN(1.0f, (origin.x + 1.0f - rectTiles.Min.x) * fTilesToRect);
			srcRect.Max.y = MIN(1.0f, (origin.y + 1.0f - rectTiles.Min.y) * fTilesToRect);
			TransferSubimageFast(rSrcImage, srcRect, *pTileImage, destRect);
		}
}

