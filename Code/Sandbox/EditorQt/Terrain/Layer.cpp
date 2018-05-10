// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Layer.h"

#include "TerrainGrid.h"
#include "CryEditDoc.h"           // will be removed
#include <CryMemory/CrySizer.h>   // ICrySizer
#include "SurfaceType.h"          // CSurfaceType
#include "FilePathUtil.h"
#include <Preferences/ViewportPreferences.h>

#include "Terrain/TerrainManager.h"
#include "Controls/QuestionDialog.h"

//! Size of the texture preview
#define LAYER_TEX_PREVIEW_CX    128
//! Size of the texture preview
#define LAYER_TEX_PREVIEW_CY    128

#define MAX_TEXTURE_SIZE        (1024 * 1024 * 2)

#define DEFAULT_MASK_RESOLUTION 4096

// Static member variables
UINT CLayer::m_iInstanceCount = 0;

static const char* szReplaceMe = "%ENGINE%/EngineAssets/TextureMsg/ReplaceMe.tif";

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLayer::CLayer() : m_cLayerFilterColor(1, 1, 1), m_fLayerTiling(1), m_fSpecularAmount(0), m_fSortOrder(0), m_fUseRemeshing(0)
{
	////////////////////////////////////////////////////////////////////////
	// Initialize member variables
	////////////////////////////////////////////////////////////////////////
	// One more instance
	m_iInstanceCount++;

	m_bAutoGen = true;
	m_dwLayerId = e_undefined;    // not set yet

	// Create a layer name based on the instance count
	m_strLayerName.Format("Layer %i", m_iInstanceCount);

	// Init member variables
	m_LayerStart = 0;
	m_LayerEnd = 1024;
	m_strLayerTexPath = "";
	m_cTextureDimensions.cx = 0;
	m_cTextureDimensions.cy = 0;

	m_minSlopeAngle = 0;
	m_maxSlopeAngle = 90;
	m_bNeedUpdate = true;
	m_bCompressedMaskValid = false;

	m_guid = CryGUID::Create();

	// Create the DCs
	m_dcLayerTexPrev.CreateCompatibleDC(NULL);

	// Create the bitmap
	VERIFY(m_bmpLayerTexPrev.CreateBitmap(LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CX, 1, 32, NULL));
	m_dcLayerTexPrev.SelectObject(&m_bmpLayerTexPrev);

	// Layer is used
	m_bLayerInUse = true;
	m_bSelected = false;

	m_numSectors = 0;

	AllocateMaskGrid();
}

uint32 CLayer::GetCurrentLayerId()
{
	return m_dwLayerId;
}

uint32 CLayer::GetOrRequestLayerId()
{
	if (m_dwLayerId == e_undefined)    // not set yet
	{
		bool bFree[e_undefined];

		for (uint32 id = 0; id < e_undefined; id++)
			bFree[id] = true;

		GetIEditorImpl()->GetTerrainManager()->MarkUsedLayerIds(bFree);
		GetIEditorImpl()->GetHeightmap()->MarkUsedLayerIds(bFree);

		for (uint32 id = 0; id < e_undefined; id++)
			if (bFree[id])
			{
				m_dwLayerId = id;
				CryLog("GetOrRequestLayerId() '%s' m_dwLayerId=%d", GetLayerName(), m_dwLayerId);
				GetIEditorImpl()->GetDocument()->SetModifiedFlag(TRUE);
				break;
			}
	}

	assert(m_dwLayerId < e_undefined);

	return m_dwLayerId;
}

CLayer::~CLayer()
{
	////////////////////////////////////////////////////////////////////////
	// Clean up on exit
	////////////////////////////////////////////////////////////////////////

	CCryEditDoc* doc = GetIEditorImpl()->GetDocument();

	SetSurfaceType(NULL);

	m_iInstanceCount--;

	// Make sure the DCs are freed correctly
	m_dcLayerTexPrev.SelectObject((CBitmap*) NULL);

	// Free layer mask data
	m_layerMask.Release();
}

string CLayer::GetTextureFilename()
{
	return string(PathFindFileName(LPCTSTR(m_strLayerTexPath)));
}

string CLayer::GetTextureFilenameWithPath() const
{
	return m_strLayerTexPath;
}

//////////////////////////////////////////////////////////////////////////
void CLayer::DrawLayerTexturePreview(LPRECT rcPos, CDC* pDC)
{
	////////////////////////////////////////////////////////////////////////
	// Draw the preview of the layer texture
	////////////////////////////////////////////////////////////////////////

	ASSERT(rcPos);
	ASSERT(pDC);
	CBrush brshGray;

	if (m_bmpLayerTexPrev.m_hObject)
	{
		pDC->SetStretchBltMode(HALFTONE);
		pDC->StretchBlt(rcPos->left, rcPos->top, rcPos->right - rcPos->left,
		                rcPos->bottom - rcPos->top, &m_dcLayerTexPrev, 0, 0,
		                LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CY, SRCCOPY);
	}
	else
	{
		brshGray.CreateSysColorBrush(COLOR_BTNFACE);
		pDC->FillRect(rcPos, &brshGray);
	}
}

void CLayer::Serialize(CXmlArchive& xmlAr)
{
	CCryEditDoc* doc = GetIEditorImpl()->GetDocument();

	////////////////////////////////////////////////////////////////////////
	// Save or restore the class
	////////////////////////////////////////////////////////////////////////
	if (xmlAr.bLoading)
	{
		// We need an update
		InvalidateMask();

		////////////////////////////////////////////////////////////////////////
		// Loading
		////////////////////////////////////////////////////////////////////////

		XmlNodeRef layer = xmlAr.root;

		layer->getAttr("Name", m_strLayerName);

		//for compatibility purpose we check existing of "GUID" attribute, if not GUID generated in constructor will be used
		if (layer->haveAttr("GUID"))
			layer->getAttr("GUID", m_guid);

		// Texture
		layer->getAttr("Texture", m_strLayerTexPath);
		/*
		    // only for testing - can slow down loading time
		    {
		      bool bQualityLoss;

		      CImage img;

		      if(CImageUtil::LoadImage( m_strLayerTexPath,img,&bQualityLoss))
		      {
		        if(bQualityLoss)
		          GetISystem()->Warning(VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,VALIDATOR_FLAG_TEXTURE,m_strLayerTexPath,"Layer texture format can introduce quality loss - use lossless format or quality can suffer a lot");
		      }
		    }
		 */
		layer->getAttr("TextureWidth", m_cTextureDimensions.cx);
		layer->getAttr("TextureHeight", m_cTextureDimensions.cy);

		layer->getAttr("Material", m_materialName);

		// Parameters (Altitude, Slope...)
		layer->getAttr("AltStart", m_LayerStart);
		layer->getAttr("AltEnd", m_LayerEnd);
		layer->getAttr("MinSlopeAngle", m_minSlopeAngle);
		layer->getAttr("MaxSlopeAngle", m_maxSlopeAngle);

		// In use flag
		layer->getAttr("InUse", m_bLayerInUse);
		layer->getAttr("AutoGenMask", m_bAutoGen);

		if (m_dwLayerId > e_undefined)
		{
			CLogFile::WriteLine("ERROR: LayerId is out of range - value was clamped");
			m_dwLayerId = e_undefined;
		}

		{
			string sSurfaceType;

			layer->getAttr("SurfaceType", sSurfaceType);

			int iSurfaceType = GetIEditorImpl()->GetTerrainManager()->FindSurfaceType(sSurfaceType);
			if (iSurfaceType >= 0)
			{
				SetSurfaceType(GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypePtr(iSurfaceType));
			}
			else if (sSurfaceType != "")
			{
				AssignMaterial(sSurfaceType);
			}
		}

		if (!layer->getAttr("LayerId", m_dwLayerId))
		{
			if (m_pSurfaceType)
				m_dwLayerId = m_pSurfaceType->GetSurfaceTypeID();

			char str[256];
			cry_sprintf(str, "LayerId missing - old level format - generate value from detail layer %d", m_dwLayerId);
			CLogFile::WriteLine(str);
		}

		{
			Vec3 vFilterColor(1, 1, 1);

			layer->getAttr("FilterColor", vFilterColor);

			m_cLayerFilterColor = vFilterColor;
		}

		{
			m_fUseRemeshing = 0;
			layer->getAttr("UseRemeshing", m_fUseRemeshing);
		}

		{
			m_fLayerTiling = 1;
			layer->getAttr("LayerTiling", m_fLayerTiling);
		}

		{
			m_fSpecularAmount = 0;
			layer->getAttr("SpecularAmount", m_fSpecularAmount);
		}

		{
			m_fSortOrder = 0;
			layer->getAttr("SortOrder", m_fSortOrder);
		}

		void* pData;
		int nSize;
		if (xmlAr.pNamedData && xmlAr.pNamedData->GetDataBlock(string("Layer_") + m_strLayerName, pData, nSize))
		{
			// Load it
			if (!LoadTexture((DWORD*)pData, m_cTextureDimensions.cx, m_cTextureDimensions.cy))
			{
				Warning("Failed to load texture for layer %s", (const char*)m_strLayerName);
			}
		}
		else if (xmlAr.pNamedData)
		{
			// Try loading texture from external file,
			if (!m_strLayerTexPath.IsEmpty() && !LoadTexture(m_strLayerTexPath))
			{
				GetISystem()->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, VALIDATOR_FLAG_FILE, (const char*)m_strLayerTexPath, "Failed to load texture for layer %s", (const char*)m_strLayerName);
			}
		}

		if (!m_bAutoGen)
		{
			int maskWidth = 0, maskHeight = 0;
			layer->getAttr("MaskWidth", maskWidth);
			layer->getAttr("MaskHeight", maskHeight);

			m_maskResolution = maskWidth;
			if (m_maskResolution == 0)
				m_maskResolution = GetNativeMaskResolution();

			if (xmlAr.pNamedData)
			{
				bool bCompressed = true;
				CMemoryBlock* memBlock = xmlAr.pNamedData->GetDataBlock(string("LayerMask_") + m_strLayerName, bCompressed);
				if (memBlock)
				{
					m_compressedMask = *memBlock;
					m_bCompressedMaskValid = true;
				}
				else
				{
					// No compressed block, fallback to back-compatability mode.
					if (xmlAr.pNamedData->GetDataBlock(string("LayerMask_") + m_strLayerName, pData, nSize))
					{
						CByteImage mask;
						mask.Attach((unsigned char*)pData, maskWidth, maskHeight);
						if (maskWidth == DEFAULT_MASK_RESOLUTION)
						{
							m_layerMask.Allocate(m_maskResolution, m_maskResolution);
							m_layerMask.Copy(mask);
						}
						else
						{
							GenerateLayerMask(mask, m_maskResolution, m_maskResolution);
						}
					}
				}
			}
		}
	}
	else
	{
		////////////////////////////////////////////////////////////////////////
		// Storing
		////////////////////////////////////////////////////////////////////////

		XmlNodeRef layer = xmlAr.root;

		// Name
		layer->setAttr("Name", m_strLayerName);

		//GUID
		layer->setAttr("GUID", m_guid);

		// Texture
		layer->setAttr("Texture", m_strLayerTexPath);
		layer->setAttr("TextureWidth", m_cTextureDimensions.cx);
		layer->setAttr("TextureHeight", m_cTextureDimensions.cy);

		layer->setAttr("Material", m_materialName);

		// Parameters (Altitude, Slope...)
		layer->setAttr("AltStart", m_LayerStart);
		layer->setAttr("AltEnd", m_LayerEnd);
		layer->setAttr("MinSlopeAngle", m_minSlopeAngle);
		layer->setAttr("MaxSlopeAngle", m_maxSlopeAngle);

		// In use flag
		layer->setAttr("InUse", m_bLayerInUse);

		// Auto mask or explicit mask.
		layer->setAttr("AutoGenMask", m_bAutoGen);
		layer->setAttr("LayerId", m_dwLayerId);

		{
			string sSurfaceType = "";

			CSurfaceType* pSurfaceType = m_pSurfaceType;

			if (pSurfaceType)
				sSurfaceType = pSurfaceType->GetName();

			layer->setAttr("SurfaceType", sSurfaceType);
		}

		{
			Vec3 vFilterColor = Vec3(m_cLayerFilterColor.r, m_cLayerFilterColor.g, m_cLayerFilterColor.b);

			layer->setAttr("FilterColor", vFilterColor);
		}

		{
			layer->setAttr("UseRemeshing", m_fUseRemeshing);
		}

		{
			layer->setAttr("LayerTiling", m_fLayerTiling);
		}

		{
			layer->setAttr("SpecularAmount", m_fSpecularAmount);
		}

		{
			layer->setAttr("SortOrder", m_fSortOrder);
		}

		int layerTexureSize = m_cTextureDimensions.cx * m_cTextureDimensions.cy * sizeof(DWORD);

		/*		if (layerTexureSize <= MAX_TEXTURE_SIZE)
		    {
		      PrecacheTexture();
		      xmlAr.pNamedData->AddDataBlock( string("Layer_")+m_strLayerName,m_texture.GetData(),m_texture.GetSize() );
		    }
		 */

		if (!m_bAutoGen)
		{
			//////////////////////////////////////////////////////////////////////////
			// Save old stuff...
			//////////////////////////////////////////////////////////////////////////
			layer->setAttr("MaskWidth", m_maskResolution);
			layer->setAttr("MaskHeight", m_maskResolution);
			if (m_maskFile.IsEmpty())
				layer->setAttr("MaskFileName", m_maskFile);

			if (!m_bCompressedMaskValid && m_layerMask.IsValid())
			{
				CompressMask();
			}
			if (m_bCompressedMaskValid)
			{
				// Store uncompressed block of data.
				if (xmlAr.pNamedData)
					xmlAr.pNamedData->AddDataBlock(string("LayerMask_") + m_strLayerName, m_compressedMask);
			}
			else
			{
				// no mask.
			}

			//////////////////////////////////////////////////////////////////////////
		}
	}
}

void CLayer::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("layer", "Layer"))
	{
		float minHeight = m_LayerStart;
		float maxHeight = m_LayerEnd;
		float minAngle = m_minSlopeAngle;
		float maxAngle = m_maxSlopeAngle;
		ColorF filterColor = m_cLayerFilterColor;
		string materialName;
		string textureName;
		string texturePath = m_strLayerTexPath;

		if (m_materialName.GetBuffer())
			materialName = m_materialName.GetBuffer();

		if (texturePath.GetBuffer())
			textureName = texturePath.GetBuffer();

		ar(filterColor, "filtercolor", "Filter Color");
		ar(minHeight, "minheight", "Min Height");
		ar(maxHeight, "maxheight", "Max Height");
		
		// Limit the slope values from 0.0 to slightly less than 90. We later calculate the slope as
		// tan(angle) and tan(90) will result in disaster
		const float slopeLimitDeg = 90.f - 0.01;
		ar(yasli::Range(minAngle, 0.0f, slopeLimitDeg), "minangle", "Min Angle");
		ar(yasli::Range(maxAngle, 0.0f, slopeLimitDeg), "maxangle", "Max Angle");
		ar(Serialization::TextureFilename(textureName), "texture", "Texture");
		ar(Serialization::MaterialPicker(materialName), "material", "Material");

		if (ar.isInput())
		{
			SetLayerStart(minHeight);
			SetLayerEnd(maxHeight);
			SetLayerMinSlopeAngle(minAngle);
			SetLayerMaxSlopeAngle(maxAngle);
			SetLayerFilterColor(filterColor);

			if (m_materialName != materialName.c_str())
			{
				m_materialName = materialName.c_str();
				AssignMaterial(materialName.c_str());
				if (GetIEditorImpl()->GetTerrainManager())
					GetIEditorImpl()->GetTerrainManager()->ReloadSurfaceTypes();
			}

			if (texturePath != textureName.c_str())
				LoadTexture(textureName.c_str());
		}
		ar.closeBlock();
	}
}

CLayer* CLayer::Duplicate() const
{
	CLayer* pNewLayer = new CLayer;
	pNewLayer->SetLayerName(GetLayerName());
	pNewLayer->LoadTexture(GetTextureFilenameWithPath());
	pNewLayer->AssignMaterial(GetMaterialName());
	pNewLayer->SetLayerFilterColor(GetLayerFilterColor());
	pNewLayer->SetLayerStart(GetLayerStart());
	pNewLayer->SetLayerEnd(GetLayerEnd());
	pNewLayer->SetLayerMaxSlopeAngle(GetLayerMaxSlopeAngle());
	pNewLayer->SetLayerMinSlopeAngle(GetLayerMinSlopeAngle());
	pNewLayer->GetOrRequestLayerId();
	return pNewLayer;
}

//////////////////////////////////////////////////////////////////////////
void CLayer::SetAutoGen(bool bAutoGen)
{
	bool prev = m_bAutoGen;
	m_bAutoGen = bAutoGen;

	if (prev != m_bAutoGen)
	{
		InvalidateMask();
		if (!m_bAutoGen)
		{
			m_maskResolution = GetNativeMaskResolution();
			// Not autogenerated layer must keep its mask.
			m_layerMask.Allocate(m_maskResolution, m_maskResolution);
			m_layerMask.Clear();
			SetAllSectorsValid();
		}
		else
		{
			// Release layer mask.
			m_layerMask.Release();
			InvalidateAllSectors();
		}
	}
};

void CLayer::FillWithColor(COLORREF col, int width, int height)
{
	m_cTextureDimensions = CSize(width, height);

	// Allocate new memory to hold the bitmap data
	m_cTextureDimensions = CSize(width, height);
	m_texture.Allocate(width, height);
	uint32* pTex = m_texture.GetData();
	for (int i = 0; i < width * height; i++)
	{
		*pTex++ = col;
	}
}

bool CLayer::LoadTexture(LPCTSTR lpBitmapName, UINT iWidth, UINT iHeight)
{
	////////////////////////////////////////////////////////////////////////
	// Load a layer texture out of a ressource
	////////////////////////////////////////////////////////////////////////

	CBitmap bmpLoad;
	BOOL bReturn;

	ASSERT(lpBitmapName);
	ASSERT(iWidth);
	ASSERT(iHeight);

	// Load the bitmap
	bReturn = bmpLoad.Attach(::LoadBitmap(AfxGetInstanceHandle(), lpBitmapName));

	if (!bReturn)
	{
		ASSERT(bReturn);
		return false;
	}

	// Save the bitmap's dimensions
	m_cTextureDimensions = CSize(iWidth, iHeight);

	// Free old tetxure data

	// Allocate new memory to hold the bitmap data
	m_cTextureDimensions = CSize(iWidth, iHeight);
	m_texture.Allocate(iWidth, iHeight);

	// Retrieve the bits from the bitmap
	VERIFY(bmpLoad.GetBitmapBits(m_texture.GetSize(), m_texture.GetData()));

	// Convert from BGR tp RGB
	BGRToRGB();

	Update3dengineInfo();

	return true;
}

inline bool IsPower2(int n)
{
	for (int i = 0; i < 30; i++)
	{
		if (n == (1 << i))
			return true;
	}
	return false;
}

bool CLayer::LoadTexture(string strFileName)
{
	CryLog("Loading layer texture (%s)...", (const char*)strFileName);

	// Save the filename
	m_strLayerTexPath = strFileName;
	if (m_strLayerTexPath.IsEmpty())
		m_strLayerTexPath = strFileName;

	bool bQualityLoss;
	bool bError = false;

	// if file doesn't exist don't try to load it
	if (!CFileUtil::FileExists(m_strLayerTexPath) || !CImageUtil::LoadImage(m_strLayerTexPath, m_texture, &bQualityLoss))
	{
		// check if this a .tif file then try to load a .dds file
		if (strcmp(PathUtil::GetExt(m_strLayerTexPath), "tif") != 0 ||
		    !CImageUtil::LoadImage(PathUtil::ReplaceExtension(m_strLayerTexPath.GetString(), "dds"), m_texture, &bQualityLoss))
		{
			CryLog("Error loading layer texture (%s)...", (const char*)m_strLayerTexPath);
			bError = true;
		}
	}

	/* Vlad: bQualityLoss ignored during development
	   if (!bError && bQualityLoss)
	   {
	   Warning( "Layer texture format introduces quality loss - use lossless format or quality can suffer a lot" );
	   bError=true;
	   }*/

	if (!bError && (!IsPower2(m_texture.GetWidth()) || !IsPower2(m_texture.GetHeight())))
	{
		Warning("Selected Layer Texture must have power of 2 size.");
		bError = true;
	}

	if (bError && !CImageUtil::LoadImage(szReplaceMe, m_texture, &bQualityLoss))
	{
		m_texture.Allocate(4, 4);
		m_texture.Fill(0xff);
	}

	// Store the size
	m_cTextureDimensions = CSize(m_texture.GetWidth(), m_texture.GetHeight());

	CBitmap bmpLoad;
	CDC dcLoad;
	// Create the DC for blitting from the loaded bitmap
	VERIFY(dcLoad.CreateCompatibleDC(NULL));

	CImageEx inverted;
	inverted.Allocate(m_texture.GetWidth(), m_texture.GetHeight());
	for (int y = 0; y < m_texture.GetHeight(); y++)
	{
		for (int x = 0; x < m_texture.GetWidth(); x++)
		{
			uint32 val = m_texture.ValueAt(x, y);
			inverted.ValueAt(x, y) = 0xFF000000 | RGB(GetBValue(val), GetGValue(val), GetRValue(val));
		}
	}

	bmpLoad.CreateBitmap(m_texture.GetWidth(), m_texture.GetHeight(), 1, 32, inverted.GetData());

	// Select it into the DC
	dcLoad.SelectObject(&bmpLoad);

	// Copy it to the preview bitmap
	m_dcLayerTexPrev.SetStretchBltMode(COLORONCOLOR);
	m_dcLayerTexPrev.StretchBlt(0, 0, LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CY, &dcLoad,
	                            0, 0, m_texture.GetWidth(), m_texture.GetHeight(), SRCCOPY);
	dcLoad.DeleteDC();

	CImageEx filteredImage;
	filteredImage.Allocate(m_texture.GetWidth(), m_texture.GetHeight());
	for (int y = 0; y < m_texture.GetHeight(); y++)
	{
		for (int x = 0; x < m_texture.GetWidth(); x++)
		{
			uint32 val = m_texture.ValueAt(x, y);

			ColorB& colorB = *(ColorB*)&val;

			ColorF colorF(colorB.r / 255.f, colorB.g / 255.f, colorB.b / 255.f);
			//      colorF = colorF*GetLayerFilterColor();
			colorB = colorF;

			filteredImage.ValueAt(x, y) = val;
		}
	}

	m_previewImage.Allocate(LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CY);
	CImageUtil::ScaleToFit(filteredImage, m_previewImage);

	Update3dengineInfo();
	signalPropertiesChanged(this);

	if (GetIEditorImpl()->GetTerrainManager())
		GetIEditorImpl()->GetTerrainManager()->signalLayersChanged();
	return !bError;
}

void CLayer::Update3dengineInfo()
{
	if (GetIEditorImpl()->Get3DEngine())
	{
		IMaterial* pMat = NULL;
		CSurfaceType* pSrfType = 0;
		int iSurfaceTypeId = 0;
		if (pSrfType = m_pSurfaceType)
		{
			pMat = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->LoadMaterial(pSrfType->GetMaterial());
			iSurfaceTypeId = pSrfType->GetSurfaceTypeID();
		}

		GetIEditorImpl()->Get3DEngine()->SetTerrainLayerBaseTextureData(m_dwLayerId,
		                                                            (byte*)m_texture.GetData(), m_texture.GetWidth(), m_strLayerTexPath,
		                                                            pMat, 1.0f, GetLayerTiling(), iSurfaceTypeId, pSrfType ? pSrfType->GetDetailTextureScale().x : 0.f,
		                                                            GetLayerSpecularAmount(), GetLayerSortOrder(), /*GetLayerFilterColor()*/ Col_White, GetLayerUseRemeshing(),
		                                                            (m_bSelected));
	}
}

bool CLayer::LoadTexture(DWORD* pBitmapData, UINT iWidth, UINT iHeight)
{
	////////////////////////////////////////////////////////////////////////
	// Load a texture from an array into the layer
	////////////////////////////////////////////////////////////////////////

	CDC dcLoad;
	CBitmap bmpLoad;
	DWORD* pPixData = NULL, * pPixDataEnd = NULL;

	if (iWidth == 0 || iHeight == 0)
	{
		return false;
	}

	// Allocate new memory to hold the bitmap data
	m_cTextureDimensions = CSize(iWidth, iHeight);
	m_texture.Allocate(iWidth, iHeight);

	// Copy the image data into the layer
	memcpy(m_texture.GetData(), pBitmapData, m_texture.GetSize());

	////////////////////////////////////////////////////////////////////////
	// Generate the texture preview
	////////////////////////////////////////////////////////////////////////

	// Set the loop pointers
	pPixData = pBitmapData;
	pPixDataEnd = &pBitmapData[GetTextureWidth() * GetTextureHeight()];

	// Switch R and B
	while (pPixData != pPixDataEnd)
	{
		// Extract the bits, shift them, put them back and advance to the next pixel
		*pPixData++ = ((*pPixData & 0x00FF0000) >> 16) |
		              (*pPixData & 0x0000FF00) | ((*pPixData & 0x000000FF) << 16);
	}

	// Create the DC for blitting from the loaded bitmap
	VERIFY(dcLoad.CreateCompatibleDC(NULL));

	// Create the matching bitmap
	if (!bmpLoad.CreateBitmap(iWidth, iHeight, 1, 32, pBitmapData))
	{
		ASSERT(FALSE);
		return false;
	}

	// Select it into the DC
	dcLoad.SelectObject(&bmpLoad);

	// Copy it to the preview bitmap
	m_dcLayerTexPrev.SetStretchBltMode(COLORONCOLOR);
	m_dcLayerTexPrev.StretchBlt(0, 0, LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CY, &dcLoad,
	                            0, 0, iWidth, iHeight, SRCCOPY);

	Update3dengineInfo();

	return true;
}

bool CLayer::LoadMask(const string& strFileName)
{
	assert(m_layerMask.IsValid());
	if (!m_layerMask.IsValid())
		return false;
	////////////////////////////////////////////////////////////////////////
	// Load a BMP texture into the layer mask.
	////////////////////////////////////////////////////////////////////////
	CryLog("Loading mask texture (%s)...", (const char*)strFileName);

	m_maskFile = PathUtil::AbsolutePathToCryPakPath(strFileName.GetString());
	if (m_maskFile.IsEmpty())
		m_maskFile = strFileName;

	// Load the bitmap
	CImageEx maskRGBA;
	if (!CImageUtil::LoadImage(m_maskFile, maskRGBA))
	{
		CryLog("Error loading layer mask (%s)...", (const char*)m_maskFile);
		return false;
	}

	CByteImage mask;
	mask.Allocate(maskRGBA.GetWidth(), maskRGBA.GetHeight());

	unsigned char* dest = mask.GetData();
	unsigned int* src = maskRGBA.GetData();
	int size = maskRGBA.GetWidth() * maskRGBA.GetHeight();
	for (int i = 0; i < size; i++)
	{
		*dest = *src & 0xFF;
		src++;
		dest++;
	}
	GenerateLayerMask(mask, m_layerMask.GetWidth(), m_layerMask.GetHeight());
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLayer::GenerateLayerMask(CByteImage& mask, int width, int height)
{
	// Mask is not valid anymore.
	InvalidateMask();

	// Check if mask image is same size.
	if (width == mask.GetWidth() && height == mask.GetHeight())
	{
		m_layerMask.Copy(mask);
		return;
	}

	if (m_layerMask.GetWidth() != width || m_layerMask.GetHeight() != height)
	{
		// Allocate new memory for the layer mask
		m_layerMask.Allocate(width, height);
	}

	// Scale mask image to the size of destination layer mask.
	CImageUtil::ScaleToFit(mask, m_layerMask);

	//	m_currWidth = width;
	//	m_currHeight = height;

	//SMOOTH	if (m_layerMask.GetWidth() != mask.GetWidth() || m_layerMask.GetHeight() != mask.GetHeight())
	//SMOOTH	{
	//SMOOTH		if (m_bSmooth)
	//SMOOTH			CImageUtil::SmoothImage( m_layerMask,2 );
	//SMOOTH	}

	// All sectors of mask are valid.
	SetAllSectorsValid();

	// Free compressed mask.
	m_compressedMask.Free();
	m_bCompressedMaskValid = false;
}

void CLayer::GenerateWaterLayer16(float* pHeightmapPixels, UINT iHeightmapWidth, UINT iHeightmapHeight, float waterLevel)
{
	////////////////////////////////////////////////////////////////////////
	// Generate a layer that is used to render the water in the preview
	////////////////////////////////////////////////////////////////////////

	uint8* pLayerEnd = NULL;
	uint8* pLayer = NULL;

	m_bAutoGen = false;

	ASSERT(!IsBadWritePtr(pHeightmapPixels, iHeightmapWidth *
	                      iHeightmapHeight * sizeof(float)));

	CLogFile::WriteLine("Generating the water layer...");

	// Allocate new memory for the layer mask
	m_layerMask.Allocate(iHeightmapWidth, iHeightmapHeight);

	uint8* pLayerMask = m_layerMask.GetData();

	// Set the loop pointers
	pLayerEnd = &pLayerMask[iHeightmapWidth * iHeightmapHeight];
	pLayer = pLayerMask;

	// Generate the layer
	while (pLayer != pLayerEnd)
		*pLayer++ = ((*pHeightmapPixels++) <= waterLevel) ? 127 : 0;

	//SMOOTH
	/*
	   if (m_bSmooth)
	   {
	    // Smooth the layer
	    for (j=1; j<iHeightmapHeight - 1; j++)
	    {
	      // Precalculate for better speed
	      iCurPos = j * iHeightmapWidth + 1;

	      for (i=1; i<iHeightmapWidth - 1; i++)
	      {
	        // Next pixel
	        iCurPos++;

	        // Smooth it out
	        pLayerMask[iCurPos] = int(
	          (pLayerMask[iCurPos] +
	          pLayerMask[iCurPos + 1]+
	          pLayerMask[iCurPos + iHeightmapWidth] +
	          pLayerMask[iCurPos + iHeightmapWidth + 1] +
	          pLayerMask[iCurPos - 1]+
	          pLayerMask[iCurPos - iHeightmapWidth] +
	          pLayerMask[iCurPos - iHeightmapWidth - 1] +
	          pLayerMask[iCurPos - iHeightmapWidth + 1] +
	          pLayerMask[iCurPos + iHeightmapWidth - 1])
	 * 0.11111111111f);
	      }
	    }
	   }
	 */

	//	m_currWidth = iHeightmapWidth;
	//	m_currHeight = iHeightmapHeight;

	m_bNeedUpdate = false;
}

void CLayer::BGRToRGB()
{
	////////////////////////////////////////////////////////////////////////
	// Convert the layer data from BGR to RGB
	////////////////////////////////////////////////////////////////////////
	PrecacheTexture();

	DWORD* pPixData = NULL, * pPixDataEnd = NULL;

	// Set the loop pointers
	pPixData = (DWORD*)m_texture.GetData();
	pPixDataEnd = pPixData + (m_texture.GetWidth() * m_texture.GetHeight());

	// Switch R and B
	while (pPixData != pPixDataEnd)
	{
		// Extract the bits, shift them, put them back and advance to the next pixel
		*pPixData++ = ((*pPixData & 0x00FF0000) >> 16) |
		              (*pPixData & 0x0000FF00) | ((*pPixData & 0x000000FF) << 16);
	}
}

void CLayer::ExportTexture(string strFileName)
{
	////////////////////////////////////////////////////////////////////////
	// Save the texture data of this layer into a BMP file
	////////////////////////////////////////////////////////////////////////

	DWORD* pTempBGR = NULL;
	DWORD* pPixData = NULL, * pPixDataEnd = NULL;

	CLogFile::WriteLine("Exporting texture from layer data to BMP...");

	PrecacheTexture();

	if (!m_texture.IsValid())
		return;

	// Make a copy of the layer data
	CImageEx image;
	image.Allocate(GetTextureWidth(), GetTextureHeight());
	pTempBGR = (DWORD*)image.GetData();
	memcpy(pTempBGR, m_texture.GetData(), m_texture.GetSize());

	// Set the loop pointers
	pPixData = pTempBGR;
	pPixDataEnd = &pTempBGR[GetTextureWidth() * GetTextureHeight()];

	/*
	   // Switch R and B
	   while (pPixData != pPixDataEnd)
	   {
	   // Extract the bits, shift them, put them back and advance to the next pixel
	 * pPixData++ = ((* pPixData & 0x00FF0000) >> 16) |
	    (* pPixData & 0x0000FF00) | ((* pPixData & 0x000000FF) << 16);
	   }
	 */

	// Write the bitmap
	CImageUtil::SaveImage(strFileName, image);
}

void CLayer::ExportMask(const string& strFileName)
{
	////////////////////////////////////////////////////////////////////////
	// Save the texture data of this layer into a BMP file
	////////////////////////////////////////////////////////////////////////
	DWORD* pTempBGR = NULL;

	CLogFile::WriteLine("Exporting layer mask to BMP...");

	CByteImage layerMask;
	uint8* pLayerMask = NULL;

	int w, h;

	if (m_bAutoGen)
	{
		int nativeResolution = GetNativeMaskResolution();
		w = h = nativeResolution;
		// Create mask at native texture resolution.
		CRect rect(0, 0, nativeResolution, nativeResolution);
		CFloatImage hmap;
		hmap.Allocate(nativeResolution, nativeResolution);
		layerMask.Allocate(nativeResolution, nativeResolution);
		// Get hmap at native resolution.
		GetIEditorImpl()->GetHeightmap()->GetData(rect, hmap.GetWidth(), CPoint(0, 0), hmap, true, true);
		AutogenLayerMask(rect, nativeResolution, CPoint(0, 0), hmap, layerMask);

		pLayerMask = layerMask.GetData();
	}
	else
	{
		PrecacheMask();
		pLayerMask = m_layerMask.GetData();
		w = m_layerMask.GetWidth();
		h = m_layerMask.GetHeight();
	}

	if (w == 0 || h == 0 || pLayerMask == 0)
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Cannot export empty mask"));
		return;
	}

	// Make a copy of the layer data
	CImageEx image;
	image.Allocate(w, h);
	pTempBGR = (DWORD*)image.GetData();
	for (int i = 0; i < w * h; i++)
	{
		uint32 col = pLayerMask[i];
		pTempBGR[i] = col | col << 8 | col << 16;
	}

	// Write the mask
	CImageUtil::SaveImage(strFileName, image);
}

void CLayer::ReleaseMask()
{
	m_layerMask.Release();
}

void CLayer::ReleaseTempResources()
{
	// Free layer mask data
	if (IsAutoGen())
		m_layerMask.Release();
	else
	{
		//CompressMask();
	}
	//	m_currWidth = 0;
	//	m_currHeight = 0;

	// If texture image is too big, release it.
	if (m_texture.GetSize() > MAX_TEXTURE_SIZE)
		m_texture.Release();
}

void CLayer::PrecacheTexture()
{
	if (m_texture.IsValid())
		return;

	if (m_strLayerTexPath.IsEmpty())
	{
		m_cTextureDimensions = CSize(4, 4);
		m_texture.Allocate(m_cTextureDimensions.cx, m_cTextureDimensions.cy);
		m_texture.Fill(0xff);
	}
	else if (!CImageUtil::LoadImage(m_strLayerTexPath, m_texture))
	{
		Error("Error loading layer texture (%s)...", (const char*)m_strLayerTexPath);
		m_cTextureDimensions = CSize(4, 4);
		m_texture.Allocate(m_cTextureDimensions.cx, m_cTextureDimensions.cy);
		m_texture.Fill(0xff);
		return;
	}
	m_cTextureDimensions.cx = m_texture.GetWidth();
	m_cTextureDimensions.cx = m_texture.GetHeight();
	// Convert from BGR tp RGB
	//BGRToRGB();
}

//////////////////////////////////////////////////////////////////////////
void CLayer::InvalidateAllSectors()
{
	// Fill all sectors with 0, (clears all flags).
	if (0 < m_maskGrid.size())
		memset(&m_maskGrid[0], 0, m_maskGrid.size() * sizeof(m_maskGrid[0]));
}

//////////////////////////////////////////////////////////////////////////
void CLayer::SetAllSectorsValid()
{
	// Fill all sectors with 0xFF, (Set all flags).
	if (0 < m_maskGrid.size())
		memset(&m_maskGrid[0], 0xFF, m_maskGrid.size() * sizeof(m_maskGrid[0]));
}

//////////////////////////////////////////////////////////////////////////
void CLayer::InvalidateMaskSector(CPoint sector)
{
	GetSector(sector) = 0;
	m_bCompressedMaskValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CLayer::InvalidateMask()
{
	m_bNeedUpdate = true;

	InvalidateAllSectors();
	if (m_scaledMask.IsValid())
		m_scaledMask.Release();
	/*
	   if (m_layerMask.IsValid
	   if (m_bCompressedMaskValid)
	   m_compressedMask.Free();
	   m_bCompressedMaskValid = false;
	   m_compressedMask.Free();
	 */

	Update3dengineInfo();

	signalPropertiesChanged(this);
};

//////////////////////////////////////////////////////////////////////////
void CLayer::PrecacheMask()
{
	if (!m_bCompressedMaskValid)
		return;

	//if (IsAutoGen())
	//return;
	if (!m_layerMask.IsValid())
	{
		m_layerMask.Allocate(m_maskResolution, m_maskResolution);
		if (!m_layerMask.IsValid())
		{
			Warning("Layer %s compressed mask have invalid resolution %d.", (const char*)m_strLayerName, m_maskResolution);
			m_layerMask.Allocate(GetNativeMaskResolution(), GetNativeMaskResolution());
			return;
		}
		if (m_bCompressedMaskValid)
		{
			bool bUncompressOk = m_layerMask.Uncompress(m_compressedMask);
			m_compressedMask.Free();
			m_bCompressedMaskValid = false;
			if (!bUncompressOk)
			{
				Warning("Layer %s compressed mask have invalid resolution.", (const char*)m_strLayerName);
			}
		}
		else
		{
			// No compressed layer data.
			m_layerMask.Clear();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CByteImage& CLayer::GetMask()
{
	PrecacheMask();
	return m_layerMask;
}

//////////////////////////////////////////////////////////////////////////
bool CLayer::UpdateMaskForSector(CPoint sector, const CRect& sectorRect, const int resolution, const CPoint vTexOffset,
                                 const CFloatImage& hmap, CByteImage& mask)
{
	////////////////////////////////////////////////////////////////////////
	// Update the layer mask. The heightmap bits are supplied for speed
	// reasons, repeated memory allocations during batch generations of
	// layers are to slow
	////////////////////////////////////////////////////////////////////////
	PrecacheTexture();
	PrecacheMask();

	uint8& sectorFlags = GetSector(sector);
	if (sectorFlags & SECTOR_MASK_VALID && !m_bNeedUpdate)
	{
		if (m_layerMask.GetWidth() == resolution)
		{
			mask.Attach(m_layerMask);
			return true;
		}
	}
	m_bNeedUpdate = false;

	if (!IsAutoGen())
	{
		if (!m_layerMask.IsValid())
			return false;

		if (resolution == m_layerMask.GetWidth())
		{
			mask.Attach(m_layerMask);
		}
		else if (resolution == m_scaledMask.GetWidth())
		{
			mask.Attach(m_scaledMask);
		}
		else
		{
			m_scaledMask.Allocate(resolution, resolution);
			CImageUtil::ScaleToFit(m_layerMask, m_scaledMask);
			//SMOOTH			if (m_bSmooth)
			//SMOOTH				CImageUtil::SmoothImage( m_scaledMask,2 );
			mask.Attach(m_scaledMask);
		}

		// Mark this sector as valid.
		sectorFlags |= SECTOR_MASK_VALID;
		// All valid.
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Auto generate mask.
	//////////////////////////////////////////////////////////////////////////

	// If layer mask differ in size, invalidate all sectors.
	if (resolution != m_layerMask.GetWidth())
	{
		m_layerMask.Allocate(resolution, resolution);
		m_layerMask.Clear();
		InvalidateAllSectors();
	}

	// Mark this sector as valid.
	sectorFlags |= SECTOR_MASK_VALID;

	mask.Attach(m_layerMask);

	float hVal = 0.0f;

	AutogenLayerMask(sectorRect, resolution, vTexOffset, hmap, m_layerMask);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLayer::UpdateMask(const CFloatImage& hmap, CByteImage& mask)
{
	PrecacheTexture();
	PrecacheMask();

	int resolution = hmap.GetWidth();

	if (!m_bNeedUpdate && m_layerMask.GetWidth() == resolution)
	{
		mask.Attach(m_layerMask);
		return true;
	}
	m_bNeedUpdate = false;

	if (!IsAutoGen())
	{
		if (!m_layerMask.IsValid())
			return false;

		if (resolution == m_layerMask.GetWidth())
		{
			mask.Attach(m_layerMask);
		}
		else if (resolution == m_scaledMask.GetWidth())
		{
			mask.Attach(m_scaledMask);
		}
		else
		{
			m_scaledMask.Allocate(resolution, resolution);
			CImageUtil::ScaleToFit(m_layerMask, m_scaledMask);
			//SMOOTH			if (m_bSmooth)
			//SMOOTH				CImageUtil::SmoothImage( m_scaledMask,2 );
			mask.Attach(m_scaledMask);
		}
		// All valid.
		SetAllSectorsValid();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Auto generate mask.
	//////////////////////////////////////////////////////////////////////////

	// If layer mask differ in size, invalidate all sectors.
	if (resolution != m_layerMask.GetWidth())
	{
		m_layerMask.Allocate(resolution, resolution);
		m_layerMask.Clear();
	}

	mask.Attach(m_layerMask);

	CRect rect(0, 0, resolution, resolution);
	AutogenLayerMask(rect, hmap.GetWidth(), CPoint(0, 0), hmap, m_layerMask);
	SetAllSectorsValid();

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLayer::AutogenLayerMask(const CRect& rc, const int resolution, const CPoint vHTexOffset, const CFloatImage& hmap, CByteImage& mask)
{
	CRect rect = rc;

	assert(hmap.IsValid());
	assert(mask.IsValid());
	//	assert( hmap.GetWidth() == mask.GetWidth() );

	// Inflate rectangle.
	rect.InflateRect(1, 1, 1, 1);

	// clip within mask - 1 pixel
	rect.left = max((int)rect.left, 1);
	rect.top = max((int)rect.top, 1);
	rect.right = min((int)rect.right, resolution - 1);
	rect.bottom = min((int)rect.bottom, resolution - 1);

	// clip within heightmap - 1 pixel
	rect.left = max((int)rect.left, 1 + (int)vHTexOffset.x);
	rect.top = max((int)rect.top, 1 + (int)vHTexOffset.y);
	rect.right = min((int)rect.right, hmap.GetWidth() - 1 + (int)vHTexOffset.x);
	rect.bottom = min((int)rect.bottom, hmap.GetHeight() - 1 + (int)vHTexOffset.y);

	// Set the loop pointers
	uint8* pLayerMask = mask.GetData();
	float* pHmap = hmap.GetData();

	// We need constant random numbers
	srand(0);

	float MinAltitude = m_LayerStart;
	float MaxAltitude = m_LayerEnd;

	float fMinSlope = tan(m_minSlopeAngle / 90.1f * g_PI / 2.0f); // 0..90 -> 0..~1/0
	float fMaxSlope = tan(m_maxSlopeAngle / 90.1f * g_PI / 2.0f); // 0..90 -> 0..~1/0

	// Scan the heightmap for pixels that belong to this layer
	unsigned int x, y;
	unsigned int dwMaskWidth = resolution;
	int iHeightmapWidth = hmap.GetWidth();
	float hVal = 0.0f;

	// /8 because of the way we calculate the slope
	// /256 because the terrain height ranges from
	float fInvHeightScale = (float)resolution
	                        / (float)GetIEditorImpl()->GetHeightmap()->GetWidth()
	                        / (float)GetIEditorImpl()->GetHeightmap()->GetUnitSize() / 8.0f;

	for (y = rect.top; y < rect.bottom; y++)
	{
		for (x = rect.left; x < rect.right; x++)
		{
			unsigned int hpos = (x - vHTexOffset.x) + (y - vHTexOffset.y) * iHeightmapWidth;
			unsigned int lpos = x + y * dwMaskWidth;

			// Get the height value from the heightmap
			float* h = &pHmap[hpos];
			hVal = *h;

			if (hVal < MinAltitude || hVal > MaxAltitude)
			{
				pLayerMask[lpos] = 0;
				continue;
			}

			// Calculate the slope for this point
			float fs = (
			  fabs((*(h + 1)) - hVal) +
			  fabs((*(h - 1)) - hVal) +
			  fabs((*(h + iHeightmapWidth)) - hVal) +
			  fabs((*(h - iHeightmapWidth)) - hVal) +
			  fabs((*(h + iHeightmapWidth + 1)) - hVal) +
			  fabs((*(h - iHeightmapWidth - 1)) - hVal) +
			  fabs((*(h + iHeightmapWidth - 1)) - hVal) +
			  fabs((*(h - iHeightmapWidth + 1)) - hVal));

			// Compensate the smaller slope for bigger heightfields
			float fSlope = fs * fInvHeightScale;

			// Check if the current point belongs to the layer
			if (fSlope >= fMinSlope && fSlope <= fMaxSlope)
				pLayerMask[lpos] = 0xFF;
			else
				pLayerMask[lpos] = 0;
		}
	}
	rect.DeflateRect(1, 1, 1, 1);

	//SMOOTH
	/*	if (m_bSmooth)
	   {
	    // Smooth the layer
	    for (y = rect.top; y < rect.bottom; y++)
	    {
	      unsigned int lpos = y*dwMaskWidth + rect.left;

	      for (x = rect.left; x < rect.right; x++,lpos++)
	      {
	        // Smooth it out
	        pLayerMask[lpos] =(
	          (uint32)pLayerMask[lpos] +
	          pLayerMask[lpos + 1] +
	          pLayerMask[lpos + dwMaskWidth] +
	          pLayerMask[lpos + dwMaskWidth + 1] +
	          pLayerMask[lpos - 1] +
	          pLayerMask[lpos - dwMaskWidth] +
	          pLayerMask[lpos - dwMaskWidth - 1] +
	          pLayerMask[lpos - dwMaskWidth + 1] +
	          pLayerMask[lpos + dwMaskWidth - 1]) / 9;
	      }
	    }
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CLayer::AllocateMaskGrid()
{
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	SSectorInfo si;
	pHeightmap->GetSectorsInfo(si);
	m_numSectors = si.numSectors;
	m_maskGrid.resize(si.numSectors * si.numSectors);

	if (0 < m_maskGrid.size())
		memset(&m_maskGrid[0], 0, m_maskGrid.size() * sizeof(m_maskGrid[0]));

	m_maskResolution = si.surfaceTextureSize;
}

//////////////////////////////////////////////////////////////////////////
uint8& CLayer::GetSector(CPoint sector)
{
	int p = sector.x + sector.y * m_numSectors;
	assert(p >= 0 && p < m_maskGrid.size());
	return m_maskGrid[p];
}

//////////////////////////////////////////////////////////////////////////
void CLayer::SetLayerId(const uint32 dwLayerId)
{
	assert(dwLayerId < e_undefined);

	m_dwLayerId = dwLayerId < e_undefined ? dwLayerId : e_undefined;

	//	CryLog("SetLayerId() '%s' m_dwLayerId=%d",GetLayerName(),m_dwLayerId);
}

//////////////////////////////////////////////////////////////////////////
void CLayer::CompressMask()
{
	if (m_bCompressedMaskValid)
		return;

	m_bCompressedMaskValid = false;
	m_compressedMask.Free();

	// Compress mask.
	if (m_layerMask.IsValid())
	{
		m_layerMask.Compress(m_compressedMask);
		m_bCompressedMaskValid = true;
	}
	m_layerMask.Release();
	m_scaledMask.Release();
}

//////////////////////////////////////////////////////////////////////////
int CLayer::GetSize() const
{
	int size = sizeof(*this);
	size += m_texture.GetSize();
	size += m_layerMask.GetSize();
	size += m_scaledMask.GetSize();
	size += m_compressedMask.GetSize();
	size += m_maskGrid.size() * sizeof(unsigned char);
	return size;
}

//////////////////////////////////////////////////////////////////////////
//! Export layer block.
void CLayer::ExportBlock(const CRect& rect, CXmlArchive& xmlAr)
{
	// ignore autogenerated layers.
	//if (m_bAutoGen)
	//return;

	XmlNodeRef node = xmlAr.root;

	PrecacheMask();
	if (!m_layerMask.IsValid())
		return;

	node->setAttr("Name", m_strLayerName);
	node->setAttr("MaskWidth", m_layerMask.GetWidth());
	node->setAttr("MaskHeight", m_layerMask.GetHeight());

	CRect subRc(0, 0, m_layerMask.GetWidth(), m_layerMask.GetHeight());
	subRc &= rect;

	node->setAttr("X1", subRc.left);
	node->setAttr("Y1", subRc.top);
	node->setAttr("X2", subRc.right);
	node->setAttr("Y2", subRc.bottom);

	if (!subRc.IsRectEmpty())
	{
		CByteImage subImage;
		subImage.Allocate(subRc.Width(), subRc.Height());
		m_layerMask.GetSubImage(subRc.left, subRc.top, subRc.Width(), subRc.Height(), subImage);

		xmlAr.pNamedData->AddDataBlock(string("LayerMask_") + m_strLayerName, subImage.GetData(), subImage.GetSize());
	}
}

//! Import layer block.
void CLayer::ImportBlock(CXmlArchive& xmlAr, const CPoint& offset, int nRot)
{
	// ignore autogenerated layers.
	//if (m_bAutoGen)
	//return;

	XmlNodeRef node = xmlAr.root;

	PrecacheMask();
	if (!m_layerMask.IsValid())
		return;

	CRect subRc;
	CRect dstRc;

	node->getAttr("X1", subRc.left);
	node->getAttr("Y1", subRc.top);
	node->getAttr("X2", subRc.right);
	node->getAttr("Y2", subRc.bottom);

	CPoint src((subRc.left + subRc.right) / 2, (subRc.top + subRc.bottom) / 2);
	CPoint dst = src + offset;

	CPoint dstMin = dst - CPoint(subRc.Width() / 2, subRc.Height() / 2);
	if (nRot == 1 || nRot == 3)
		dstMin = dst - CPoint(subRc.Height() / 2, subRc.Width() / 2);

	void* pData;
	int nSize;
	if (xmlAr.pNamedData->GetDataBlock(string("LayerMask_") + m_strLayerName, pData, nSize))
	{
		CByteImage subImage;
		CByteImage subImageRot;
		subImage.Attach((unsigned char*)pData, subRc.Width(), subRc.Height());

		if (nRot)
			subImageRot.RotateOrt(subImage, nRot);

		m_layerMask.SetSubImage(dstMin.x, dstMin.y, nRot ? subImageRot : subImage);
	}
}

//////////////////////////////////////////////////////////////////////////
int CLayer::GetNativeMaskResolution() const
{
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	SSectorInfo si;
	pHeightmap->GetSectorsInfo(si);
	return si.surfaceTextureSize;
}

//////////////////////////////////////////////////////////////////////////
CBitmap& CLayer::GetTexturePreviewBitmap()
{
	return m_bmpLayerTexPrev;
}

//////////////////////////////////////////////////////////////////////////
CImageEx& CLayer::GetTexturePreviewImage()
{
	return m_previewImage;
}

void CLayer::GetMemoryUsage(ICrySizer* pSizer)
{
	pSizer->Add(*this);

	pSizer->Add((char*)m_compressedMask.GetBuffer(), m_compressedMask.GetSize());
	pSizer->Add(m_maskGrid);
	pSizer->Add(m_layerMask);
	pSizer->Add(m_scaledMask);
}

//////////////////////////////////////////////////////////////////////////
void CLayer::AssignMaterial(const string& materialName)
{
	m_materialName = materialName;
	bool bFound = false;
	CCryEditDoc* doc = GetIEditorImpl()->GetDocument();
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypeCount(); i++)
	{
		CSurfaceType* pSrfType = GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypePtr(i);
		if (stricmp(pSrfType->GetMaterial(), materialName) == 0)
		{
			SetSurfaceType(pSrfType);
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		// If this is the last reference of this particular surface type, then we
		// simply change its data without fearing that anything else will change.
		if (m_pSurfaceType && m_pSurfaceType->GetLayerReferenceCount() == 1)
		{
			m_pSurfaceType->SetMaterial(materialName);
			m_pSurfaceType->SetName(materialName);
		}
		else if (GetIEditorImpl()->GetTerrainManager()->GetSurfaceTypeCount() < MAX_SURFACE_TYPE_ID_COUNT)
		{
			// Create a new surface type.
			CSurfaceType* pSrfType = new CSurfaceType;

			SetSurfaceType(pSrfType);

			pSrfType->SetMaterial(materialName);
			pSrfType->SetName(materialName);
			GetIEditorImpl()->GetTerrainManager()->AddSurfaceType(pSrfType);

			pSrfType->AssignUnusedSurfaceTypeID();
		}
		else
		{
			Warning("Maximum of %d different detail textures are supported.", MAX_SURFACE_TYPE_ID_COUNT);
		}
	}

	Update3dengineInfo();
}

//////////////////////////////////////////////////////////////////////////
void CLayer::SetSurfaceType(CSurfaceType* pSurfaceType)
{
	if (pSurfaceType != m_pSurfaceType)
	{
		// Unreference previous surface type.
		_smart_ptr<CSurfaceType> pPrev = m_pSurfaceType;
		if (m_pSurfaceType)
			m_pSurfaceType->RemoveLayerReference();
		m_pSurfaceType = pSurfaceType;
		if (m_pSurfaceType)
			m_pSurfaceType->AddLayerReference();

		if (pPrev && pPrev->GetLayerReferenceCount() < 1)
		{
			// Old unreferenced surface type must be deleted.
			GetIEditorImpl()->GetTerrainManager()->RemoveSurfaceType((CSurfaceType*)pPrev);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CLayer::GetEngineSurfaceTypeId() const
{
	if (m_pSurfaceType)
		return m_pSurfaceType->GetSurfaceTypeID();
	return e_undefined;
}

void CLayer::SetSelected(bool bSelected)
{
	m_bSelected = bSelected;

	if (m_bSelected)
		Update3dengineInfo();
}

string CLayer::GetLayerPath() const
{
	CSurfaceType* pSurfaceType = GetSurfaceType();

	if (pSurfaceType)
	{
		string sName = pSurfaceType->GetName();
		string sNameLower = sName;
		sNameLower.MakeLower();
		string sTag = "materials/terrain/";
		int nInd = sNameLower.Find(sTag);

		if (nInd == 0)
			sName = sName.Mid(sTag.GetLength());

		nInd = sName.ReverseFind('/');

		if (nInd != -1)
			return sName.Mid(0, nInd);
	}

	return "";
}

