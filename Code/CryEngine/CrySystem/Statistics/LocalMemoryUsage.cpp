// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   LocalMemoryUsage.cpp
//  Version:     v1.00
//  Created:     Istvan Kiss (Spidy) [11/5/2008]
//  Description: Visual helper for checking a local memory usage on maps
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

/*
   TODO:
    - Procedural vegetation

    - Permanent mip-map problem (streaming will collect a permanent mip-maps when travelling)
    - ? Foliage render nodes

    must make a decision about the local / global problem:
      - Particle effects (must check the calculation)
      - Characters (calculation is DONE)

    - StatObj LOD streaming (I think that there is no LOD streaming, but i'm not sure about this)
    - Terrain texture streaming (not detail textures!)	//It's not needed because this system using a fix-size pool
    - Animation streaming: completely different system, not need to measure now
    - IsoMesh streaming: completely different system, not need to measure now
 */

#include "StdAfx.h"

#include "LocalMemoryUsage.h"

#include <CrySystem/ITimer.h>
#include <CrySystem/IConsole.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryAnimation/ICryAnimation.h>

//#define MAX_LODS 6										// I want an engine-wide const :-(
#define MAX_SLOTS  100                // GetSlotCount() is not working :-(
#define BIG_NUMBER 1e20f

#pragma warning(push)
#pragma warning(disable:6262)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace LocalMemoryUsageDrawHelper
{
enum eDrawRectParams
{
	DSP_YDIR              = 0,
	DSP_XDIR              = 1,
	DSP_REVERSE_TRIANGLES = 2,
	DSP_REVERSE_ARROW     = 4,
};

void DrawRect(IRenderer* pRenderer, const CCamera* camera, const Vec3& p1, const Vec3& p3, const ColorB& color, int params = 0)
{
	Vec3 p2, p4, projectedPos;
	if (!(params & DSP_REVERSE_TRIANGLES))
	{
		if (params & DSP_XDIR)
		{
			p2 = Vec3(p1.x, p3.y, p1.z);
			p4 = Vec3(p3.x, p1.y, p3.z);
		}
		else
		{
			p2 = Vec3(p1.x, p3.y, p3.z);
			p4 = Vec3(p3.x, p1.y, p1.z);
		}
	}
	else
	{
		if (params & DSP_XDIR)
		{
			p4 = Vec3(p1.x, p3.y, p1.z);
			p2 = Vec3(p3.x, p1.y, p3.z);
		}
		else
		{
			p4 = Vec3(p1.x, p3.y, p3.z);
			p2 = Vec3(p3.x, p1.y, p1.z);
		}
	}

	if (camera->Project(p1, projectedPos) || camera->Project(p2, projectedPos) || camera->Project(p3, projectedPos) || camera->Project(p4, projectedPos))
	{
		IRenderAuxGeom* pRenderAuxGeom = pRenderer->GetIRenderAuxGeom();

		pRenderAuxGeom->DrawTriangle(p1, color, p2, color, p3, color);
		pRenderAuxGeom->DrawTriangle(p3, color, p4, color, p1, color);
	}
}

void DrawArrow(IRenderer* pRenderer, const CCamera* camera, Vec3 pStart, Vec3 pEnd, float width, float head, float lengthSub, const ColorB& color, int params = 0)
{
	Vec3 p1, p2, p3, p4, p5, p6, p7, projectedPos;

	if (params & DSP_REVERSE_ARROW)
	{
		std::swap(pStart, pEnd);
	}

	Vec3 vParallel((pEnd - pStart).GetNormalized());
	Vec3 vOrto = vParallel.Cross(Vec3(0, 0, 1)).GetNormalized();
	pStart += vParallel * lengthSub;
	pEnd -= vParallel * lengthSub;

	p1 = pStart - (vOrto * width * 0.5f);
	p2 = pStart + (vOrto * width * 0.5f);
	p3 = (pEnd - vParallel * head) - (vOrto * width * 0.5f);
	p4 = (pEnd - vParallel * head) + (vOrto * width * 0.5f);
	p5 = (pEnd - vParallel * head * 1.5f) - (vOrto * head * 0.9f);
	p6 = (pEnd - vParallel * head * 1.5f) + (vOrto * head * 0.9f);
	p7 = pEnd;

	if (camera->Project(p1, projectedPos) || camera->Project(p7, projectedPos))
	{
		IRenderAuxGeom* pRenderAuxGeom = pRenderer->GetIRenderAuxGeom();

		pRenderAuxGeom->DrawTriangle(p1, color, p2, color, p4, color);
		pRenderAuxGeom->DrawTriangle(p4, color, p3, color, p1, color);
		pRenderAuxGeom->DrawTriangle(p3, color, p4, color, p7, color);
		pRenderAuxGeom->DrawTriangle(p4, color, p6, color, p7, color);
		pRenderAuxGeom->DrawTriangle(p5, color, p3, color, p7, color);
	}
}

#define DRAWHELPER_CIRCLE_POINT_NR 32

static Vec3 s_CirclePoints[DRAWHELPER_CIRCLE_POINT_NR + 1];
static bool s_CircleDataInited = false;
void InitCircleData()
{
	s_CircleDataInited = true;
	float angleDiff = 2.0f * 3.1415927f / DRAWHELPER_CIRCLE_POINT_NR;

	for (int i = 0; i < DRAWHELPER_CIRCLE_POINT_NR + 1; i++)
	{
		s_CirclePoints[i] = Vec3(cos(i * angleDiff), sin(i * angleDiff), 0.0f);
	}
}

void DrawCircle(IRenderer* pRenderer, const CCamera* camera, const Vec3& origo, float radius, float ratio, const ColorB& color1, const ColorB& color2)
{
	if (!s_CircleDataInited)
	{
		InitCircleData();
	}
	float radius2 = radius * 0.90f;

	IRenderAuxGeom* pRenderAuxGeom = pRenderer->GetIRenderAuxGeom();

	Vec3 oldPoint = origo + s_CirclePoints[0] * radius;
	Vec3 oldPoint2 = origo + s_CirclePoints[0] * radius2;

	for (int i = 0; i < DRAWHELPER_CIRCLE_POINT_NR; i++)
	{
		Vec3 newPoint = origo + s_CirclePoints[i + 1] * radius;
		Vec3 newPoint2 = origo + s_CirclePoints[i + 1] * radius2;
		if (i < ratio * DRAWHELPER_CIRCLE_POINT_NR)
		{
			pRenderAuxGeom->DrawTriangle(origo, color1, oldPoint, color1, newPoint, color1);
		}
		else
		{
			//pRenderAuxGeom->DrawTriangle( origo, color2, oldPoint, color2, newPoint, color2 );
			pRenderAuxGeom->DrawTriangle(oldPoint2, color2, oldPoint, color2, newPoint, color2);
			pRenderAuxGeom->DrawTriangle(newPoint, color2, newPoint2, color2, oldPoint2, color2);
		}
		oldPoint = newPoint;
		oldPoint2 = newPoint2;
	}
}
}

/*
   namespace Distance
   {
   // Distance: AABB_AABB
   //----------------------------------------------------------------------------------
   // Calculate the closest distance of a AABB to an another AABB in 3d-space.
   // The function returns the squared distance.
   // optionally the closest point on the hull is calculated
   //
   // Example:
   //  float result = Distance::Point_AABBSq( aabb1, aabb2 );
   //----------------------------------------------------------------------------------

   ILINE float AABB_AABBSq( const AABB& aabb1, const AABB& aabb2 )
   {
    float fDist2 = 0;
    for(int i=0; i<3; ++i)
    {
      float max1 = aabb1.max[i];
      if(max1 < aabb2.min[i])
      {
        fDist2 += sqr(max1-aabb2.min[i]);
      }
      else
      {
        float min1 = aabb1.min[i];
        if(min1 > aabb2.max[i])
        {
          fDist2 += sqr(aabb2.max[i]-min1);
        }
      }
    }
    return fDist2;
   }

   // Distance: AABB_AABB_2D
   //----------------------------------------------------------------------------------
   // Calculate the closest distance of a AABB to an another AABB in 2d-space.
   // The function returns the squared distance.
   // optionally the closest point on the hull is calculated
   //
   // Example:
   //  float result = Distance::AABB_AABB2DSq( aabb1, aabb2 );
   //----------------------------------------------------------------------------------

   ILINE float AABB_AABB2DSq( const AABB& aabb1, const AABB& aabb2 )
   {
    float fDist2 = 0;
    for(int i=0; i<2; ++i)						//We are using only 2 of the dimensions!
    {
      float max1 = aabb1.max[i];
      if(max1 < aabb2.min[i])
      {
        fDist2 += sqr(max1-aabb2.min[i]);
      }
      else
      {
        float min1 = aabb1.min[i];
        if(min1 > aabb2.max[i])
        {
          fDist2 += sqr(aabb2.max[i]-min1);
        }
      }
    }
    return fDist2;
   }
   }
 */

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CLocalMemoryUsage::SResource::m_arrMemoryUsage[LOCALMEMORY_SECTOR_PER_PASS];
int CLocalMemoryUsage::SResource::m_arrPieces[LOCALMEMORY_SECTOR_PER_PASS];

// Note: Init() will be called to initialize the remainder of the resources
// cppcheck-suppress uninitMemberVar
CLocalMemoryUsage::SResource::SResource()
	: m_pLocalMemoryUsage(nullptr)
	, m_used(true)
{
}

void CLocalMemoryUsage::SResource::Init(CLocalMemoryUsage* pLocalMemoryUsage)
{
	m_pLocalMemoryUsage = pLocalMemoryUsage;

	StartChecking();
}

void CLocalMemoryUsage::SResource::StartChecking()
{
	const RectI& actProcessedSectors = m_pLocalMemoryUsage->m_actProcessedSectors;

	float* pMipFactor = m_arrMipFactor;
	for (int i = 0; i < actProcessedSectors.w * actProcessedSectors.h; i++)
	{
		*pMipFactor = BIG_NUMBER;
		pMipFactor++;
	}
	bool* pRowDirty = m_arrRowDirty;
	for (int i = 0; i < LOCALMEMORY_SECTOR_PER_PASS; i++)
	{
		*pRowDirty = false;
		pRowDirty++;
	}
}

void CLocalMemoryUsage::SResource::CheckOnAllSectorsP1(const AABB& bounding, float maxViewDist, float scale, float mipFactor)
{
	//CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	float modifiedmaxViewDist = maxViewDist;
	float modifiedmaxViewDistSqr = sqr(modifiedmaxViewDist);

	const RectI& actProcessedSectors = m_pLocalMemoryUsage->m_actProcessedSectors;

	int xMin = (int)max(0, int((bounding.min.x - modifiedmaxViewDist) / LOCALMEMORY_SECTOR_SIZE) - actProcessedSectors.x);
	int xMax = (int)min(actProcessedSectors.w, int((bounding.max.x + modifiedmaxViewDist) / LOCALMEMORY_SECTOR_SIZE + 1) - actProcessedSectors.x);
	int yMin = (int)max(0, int((bounding.min.y - modifiedmaxViewDist) / LOCALMEMORY_SECTOR_SIZE) - actProcessedSectors.y);
	int yMax = (int)min(actProcessedSectors.h, int((bounding.max.y + modifiedmaxViewDist) / LOCALMEMORY_SECTOR_SIZE + 1) - actProcessedSectors.y);

	AABB sectorBounding(AABB::RESET);
	float sectorDistanceSqr;
	float fDistYSqr;

	for (int y = yMin; y < yMax; y++)
	{
		float* pMipFactor = &m_arrMipFactor[actProcessedSectors.w * y + xMin];

		sectorBounding.min.y = (y + actProcessedSectors.y) * LOCALMEMORY_SECTOR_SIZE;
		sectorBounding.max.y = sectorBounding.min.y + LOCALMEMORY_SECTOR_SIZE;
		sectorBounding.min.x = (xMin + actProcessedSectors.x) * LOCALMEMORY_SECTOR_SIZE;

		assert(y < LOCALMEMORY_SECTOR_PER_PASS);
		m_arrRowDirty[y] = true;

		//Calculating Y distance sqr
		{
			fDistYSqr = 0;
			if (bounding.max.y < sectorBounding.min.y)
			{
				fDistYSqr += sqr(bounding.max.y - sectorBounding.min.y);
			}
			else
			{
				if (bounding.min.y > sectorBounding.max.y)
				{
					fDistYSqr += sqr(sectorBounding.max.y - bounding.min.y);
				}
			}
		}

		for (int x = xMin; x < xMax; x++)
		{
			sectorBounding.max.x = sectorBounding.min.x + LOCALMEMORY_SECTOR_SIZE;

			//sectorDistanceSqr = Distance::AABB_AABB2DSq(bounding, sectorBounding);

			//Adding X distance sqr
			{
				sectorDistanceSqr = fDistYSqr;
				if (bounding.max.x < sectorBounding.min.x)
				{
					sectorDistanceSqr += sqr(bounding.max.x - sectorBounding.min.x);
				}
				else
				{
					if (bounding.min.x > sectorBounding.max.x)
					{
						sectorDistanceSqr += sqr(sectorBounding.max.x - bounding.min.x);
					}
				}
			}

			if (sectorDistanceSqr < modifiedmaxViewDistSqr)
			{
				*pMipFactor = min(*pMipFactor, sectorDistanceSqr * scale * scale * mipFactor);
			}

			pMipFactor++;

			sectorBounding.min.x = sectorBounding.max.x;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CLocalMemoryUsage::STextureInfo::STextureInfo()
{
	m_pTexture = NULL;

	//m_size = 0;
	//m_xSize = 0;
	//m_ySize = 0;
	//m_numMips = 0;
}

CLocalMemoryUsage::STextureInfo::~STextureInfo()
{
	if (m_pTexture)
	{
		m_pTexture->Release();
	}
}

void CLocalMemoryUsage::STextureInfo::CheckOnAllSectorsP2()
{
	int x, y, nMip, nStreamableMipNr, memoryUsage, xOldMemoryUsage, xOldPieces;
	bool nonZero;
	ITexture* pTexture;

	const RectI& actProcessedSectors = m_pLocalMemoryUsage->m_actProcessedSectors;

	bool wasDirty = false;
	for (y = 0; y < actProcessedSectors.h; y++)
	{
		int* pMemoryUsage = m_arrMemoryUsage;
		int* pPieces = m_arrPieces;
		if (m_arrRowDirty[y] || (wasDirty))
		{
			SSector* sector = &m_pLocalMemoryUsage->m_arrSectors[(y + actProcessedSectors.y) * LOCALMEMORY_SECTOR_NR_X + actProcessedSectors.x];
			float* pMipFactor = &m_arrMipFactor[y * actProcessedSectors.w];
			xOldMemoryUsage = 0;
			xOldPieces = 0;
			for (x = 0; x < actProcessedSectors.w; x++)
			{
				nonZero = (*pMipFactor != BIG_NUMBER);
				if (nonZero)    // One more row
				{
					pTexture = m_pTexture;
					nMip = max(0, pTexture->StreamCalculateMipsSigned(*pMipFactor));
					memoryUsage = pTexture->GetStreamableMemoryUsage(nMip);
					nStreamableMipNr = pTexture->GetStreamableMipNumber() - nMip;
					sector->m_memoryUsage_Textures += memoryUsage;
					xOldMemoryUsage = memoryUsage;
					*pMemoryUsage = memoryUsage;
					xOldPieces = nStreamableMipNr;
					*pPieces = nStreamableMipNr;
				}
				else
				{
					xOldMemoryUsage = 0;
					*pMemoryUsage = 0;
					xOldPieces = 0;
					*pPieces = 0;
				}
				sector++;
				pMipFactor++;
				pMemoryUsage++;
				pPieces++;
			}
			wasDirty = m_arrRowDirty[y];
		}
		else
		{
			for (x = 0; x < actProcessedSectors.w; x++)
			{
				*pMemoryUsage = 0;
				pMemoryUsage++;
				*pPieces = 0;
				pPieces++;
			}
			wasDirty = false;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CLocalMemoryUsage::SMaterialInfo::SMaterialInfo()
{
}

void CLocalMemoryUsage::SMaterialInfo::AddTextureInfo(STextureInfoAndTilingFactor textureInfo)
{
	m_textures.push_back(textureInfo);
}

void CLocalMemoryUsage::SMaterialInfo::CheckOnAllSectorsP2()
{
	int x, y;

	const RectI& actProcessedSectors = m_pLocalMemoryUsage->m_actProcessedSectors;

	for (y = 0; y < actProcessedSectors.h; y++)
	{
		if (m_arrRowDirty[y])
		{
			float* pMipFactor = &m_arrMipFactor[y * actProcessedSectors.w];
			for (x = 0; x < actProcessedSectors.w; x++)
			{
				if (*pMipFactor != BIG_NUMBER)
				{
					//(Spidy) Send the material MinDistance values to textures
					for (TTextureVector::iterator it = m_textures.begin(); it != m_textures.end(); ++it)
					{
						STextureInfoAndTilingFactor& textureInfo = *it;
						textureInfo.m_pTextureInfo->m_arrRowDirty[y] = true;

						float& textureMipFactor = textureInfo.m_pTextureInfo->m_arrMipFactor[actProcessedSectors.w * y + x];
						textureMipFactor = min(textureMipFactor, *pMipFactor * textureInfo.m_tilingFactor);
					}
				}
				pMipFactor++;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CLocalMemoryUsage::SStatObjInfo::SStatObjInfo()
{
	m_streamableContentMemoryUsage = 0;

	m_bSubObject = false;
	/*
	   m_lodNr = 0;
	   m_vertices = 0;
	   m_indices = 0;
	   m_meshSize = 0;
	   m_physProxySize = 0;
	   m_physPrimitives = 0;
	   for( int i=0; i < MAX_LODS; i++)
	   {
	    m_indicesPerLod[i]=0;
	   }
	 */
}

void CLocalMemoryUsage::SStatObjInfo::CheckOnAllSectorsP2()
{
	int x, y, memoryUsage, pieces, xOldMemoryUsage, xOldPieces;
	bool nonZero;

	const RectI& actProcessedSectors = m_pLocalMemoryUsage->m_actProcessedSectors;

	bool wasDirty = false;
	for (y = 0; y < actProcessedSectors.h; y++)
	{
		int* pMemoryUsage = m_arrMemoryUsage;
		int* pPieces = m_arrPieces;
		if (m_arrRowDirty[y])
		{
			SSector* sector = &m_pLocalMemoryUsage->m_arrSectors[(y + actProcessedSectors.y) * LOCALMEMORY_SECTOR_NR_X + actProcessedSectors.x];
			float* pMipFactor = &m_arrMipFactor[y * actProcessedSectors.w];
			xOldMemoryUsage = 0;
			xOldPieces = 0;
			for (x = 0; x < actProcessedSectors.w; x++)
			{
				nonZero = (*pMipFactor != BIG_NUMBER);
				if (nonZero)    // One more row
				{
					/*
					          memoryUsage = 0;
					          float minDistance = sqrt_tpl(minDistanceSqr);
					          CStatObj* pStatObj = (CStatObj*)(m_pStatObj.get());					//(Spidy) Huh...

					          //Iterate on all subobjects, like CStatObj::UpdateStreamableComponents
					          float fSubObjectScale = 1.0f;
					          //float fSubObjectScale = matSubObject.GetColumn0().GetLength();	//TODO
					          int lod = pStatObj->GetLodFromScale(1.0f, fSubObjectScale, minDistance, false);
					          //(Spidy) az objMatrix scale es a fLodRatioNorm mar bele vannak szamolva a minDistance-ba!
					          memoryUsage = m_streamableContentMemoryUsage;
					 */
					memoryUsage = m_streamableContentMemoryUsage;
					pieces = 1;
					sector->m_memoryUsage_Geometry += memoryUsage;
					xOldMemoryUsage = memoryUsage;
					*pMemoryUsage = memoryUsage;
					xOldPieces = pieces;
					*pPieces = pieces;
				}
				else
				{
					xOldMemoryUsage = 0;
					*pMemoryUsage = 0;
					xOldPieces = 0;
					*pPieces = 0;
				}
				sector++;
				pMipFactor++;
				pMemoryUsage++;
				pPieces++;
			}
			wasDirty = m_arrRowDirty[y];
		}
		else
		{
			for (x = 0; x < actProcessedSectors.w; x++)
			{
				*pMemoryUsage = 0;
				pMemoryUsage++;
				*pPieces = 0;
				pPieces++;
			}
			wasDirty = false;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CLocalMemoryUsage::SSector::SSector()
{
	StartChecking();
}

void CLocalMemoryUsage::SSector::StartChecking()
{
	m_memoryUsage_Textures = 0;
	m_memoryUsage_Geometry = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CLocalMemoryUsage::CLocalMemoryUsage()
{
	REGISTER_CVAR(sys_LocalMemoryTextureLimit, 64, VF_NULL, "LocalMemoryUsage tool: Set the texture memory limit to check streaming (Mb/sec)");
	REGISTER_CVAR(sys_LocalMemoryGeometryLimit, 32, VF_NULL, "LocalMemoryUsage tool: Set the statobj geometry memory limit to check streaming (Mb/sec)");
	REGISTER_CVAR(sys_LocalMemoryTextureStreamingSpeedLimit, 10, VF_NULL, "LocalMemoryUsage tool: Texture streaming speed limit (approx, Mb/sec)");
	REGISTER_CVAR(sys_LocalMemoryGeometryStreamingSpeedLimit, 10, VF_NULL, "LocalMemoryUsage tool: Stat object geometry streaming speed limit (approx, Mb/sec)");

	REGISTER_CVAR(sys_LocalMemoryWarningRatio, 0.8f, VF_NULL, "LocalMemoryUsage tool: Warning ratio for streaming");
	REGISTER_CVAR(sys_LocalMemoryOuterViewDistance, 500.f, VF_NULL, "LocalMemoryUsage tool: View distance for debug draw");
	REGISTER_CVAR(sys_LocalMemoryInnerViewDistance, 100.f, VF_NULL, "LocalMemoryUsage tool: View distance for detailed debug draw");

	REGISTER_CVAR(sys_LocalMemoryStreamingSpeedObjectLength, 10.f, VF_NULL, "LocalMemoryUsage tool: Length of the streaming speed debug object");
	REGISTER_CVAR(sys_LocalMemoryStreamingSpeedObjectWidth, 1.5f, VF_NULL, "LocalMemoryUsage tool: Width of the streaming speed debug object");
	REGISTER_CVAR(sys_LocalMemoryObjectWidth, 6.f, VF_NULL, "LocalMemoryUsage tool: Width of the streaming buffer debug object");
	REGISTER_CVAR(sys_LocalMemoryObjectHeight, 2.f, VF_NULL, "LocalMemoryUsage tool: Height of the streaming buffer debug object");
	REGISTER_CVAR(sys_LocalMemoryObjectAlpha, 128, VF_NULL, "LocalMemoryUsage tool: Color alpha of the debug objects");

	REGISTER_CVAR(sys_LocalMemoryDiagramWidth, 0.5f, VF_NULL, "LocalMemoryUsage tool: Width of the diagrams OBSOLATE");

	REGISTER_CVAR(sys_LocalMemoryDiagramRadius, 2.5f, VF_NULL, "LocalMemoryUsage tool: Radius of the diagram");
	REGISTER_CVAR(sys_LocalMemoryDiagramDistance, -5.5f, VF_NULL, "LocalMemoryUsage tool: Distance of the diagram from the main debug object");
	REGISTER_CVAR(sys_LocalMemoryDiagramStreamingSpeedRadius, 1.0f, VF_NULL, "LocalMemoryUsage tool: Radius of the streaming speed diagram");
	REGISTER_CVAR(sys_LocalMemoryDiagramStreamingSpeedDistance, 2.2f, VF_NULL, "LocalMemoryUsage tool: Distance of the streaming speed diagram from the streaming speed debug line");
	REGISTER_CVAR(sys_LocalMemoryDiagramAlpha, 255, VF_NULL, "LocalMemoryUsage tool: Color alpha of the diagrams");

	REGISTER_CVAR(sys_LocalMemoryDrawText, 0, VF_NULL, "LocalMemoryUsage tool: If != 0, it will draw the numeric values");
	REGISTER_CVAR(sys_LocalMemoryLogText, 0, VF_NULL, "LocalMemoryUsage tool: If != 0, it will log the numeric values");

	REGISTER_CVAR(sys_LocalMemoryOptimalMSecPerSec, 100, VF_NULL, "LocalMemoryUsage tool: Optimal calculation time (MSec) per secundum");
	REGISTER_CVAR(sys_LocalMemoryMaxMSecBetweenCalls, 1000, VF_NULL, "LocalMemoryUsage tool: Maximal time difference (MSec) between calls");

	m_pStreamCgfPredicitionDistance = 0;
	m_pDebugDraw = 0;
	if (gEnv->pConsole)
	{
		m_pStreamCgfPredicitionDistance = gEnv->pConsole->GetCVar("e_StreamCgfPredicitionDistance");
		m_pDebugDraw = gEnv->pConsole->GetCVar("e_DebugDraw");
	}

	m_sectorNr.zero();

	m_actProcessedSectors = RectI(0, 0, 0, 0);
	m_actDrawedSectors = RectI(0, 0, 0, 0);

	m_AverageUpdateTime = 0.01f;

	m_sectorNr.x = LOCALMEMORY_SECTOR_NR_X;       //TODO Check size of the world
	m_sectorNr.y = LOCALMEMORY_SECTOR_NR_Y;
}

CLocalMemoryUsage::~CLocalMemoryUsage()
{
	DeleteGlobalData();
}

void CLocalMemoryUsage::DeleteGlobalData()
{
	m_globalTextures.clear();
	m_globalMaterials.clear();
	m_globalStatObjs.clear();
}

void CLocalMemoryUsage::OnRender(IRenderer* pRenderer, const CCamera* camera)
{
	if (!m_pDebugDraw || (m_pDebugDraw->GetIVal() != 17))
		return;

	const Matrix34& mat = camera->GetMatrix();
	Vec3 pos;
	Vec3 projectedPos;
	SSector* sector;

	SAuxGeomRenderFlags auxGeomRenderFlagsClose(e_Mode3D | e_AlphaBlended | e_DrawInFrontOn | e_FillModeSolid | e_CullModeBack | e_DepthWriteOff | e_DepthTestOff);
	SAuxGeomRenderFlags auxGeomRenderFlagsFar(e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeBack | e_DepthWriteOff | e_DepthTestOn);

	IRenderAuxGeom* pRenderAuxGeom = pRenderer->GetIRenderAuxGeom();

	Vec3 cameraPos = mat.GetTranslation();

	int xMin = max(0, int((cameraPos.x - sys_LocalMemoryOuterViewDistance) / LOCALMEMORY_SECTOR_SIZE));
	int xMax = min(LOCALMEMORY_SECTOR_PER_PASS - 1, int((cameraPos.x + sys_LocalMemoryOuterViewDistance) / LOCALMEMORY_SECTOR_SIZE + 1));
	int yMin = max(0, int((cameraPos.y - sys_LocalMemoryOuterViewDistance) / LOCALMEMORY_SECTOR_SIZE));
	int yMax = min(LOCALMEMORY_SECTOR_PER_PASS - 1, int((cameraPos.y + sys_LocalMemoryOuterViewDistance) / LOCALMEMORY_SECTOR_SIZE + 1));
	m_actDrawedSectors.x = xMin;
	m_actDrawedSectors.y = yMin;
	m_actDrawedSectors.w = xMax - xMin + 1;
	m_actDrawedSectors.h = yMax - yMin + 1;

	CTimeValue actTime = gEnv->pTimer->GetAsyncTime();
	float timeSin = sin_tpl(actTime.GetSeconds() * 10.0f) * 0.5f + 0.5f;

	ColorB color;
	ColorB colorOK(LOCALMEMORY_COLOR_OK, sys_LocalMemoryObjectAlpha);
	ColorB colorWarning(LOCALMEMORY_COLOR_WARNING, sys_LocalMemoryObjectAlpha);
	ColorB colorError(LOCALMEMORY_COLOR_ERROR, uint8(sys_LocalMemoryObjectAlpha * timeSin));
	ColorB colorTexture(LOCALMEMORY_COLOR_TEXTURE, sys_LocalMemoryDiagramAlpha);
	ColorB colorGeometry(LOCALMEMORY_COLOR_GEOMETRY, sys_LocalMemoryDiagramAlpha);
	ColorB colorBlack(LOCALMEMORY_COLOR_BLACK);
	f32 fColorOK[4] = { LOCALMEMORY_FCOLOR_OK, 1 };
	f32 fColorWarning[4] = { LOCALMEMORY_FCOLOR_WARNING, 1 };
	f32 fColorError[4] = { LOCALMEMORY_FCOLOR_ERROR, 1 };
	f32 fColorOther[4] = { LOCALMEMORY_FCOLOR_OTHER, 1 };

	f32* pColor;
	Vec3 v0, v1, v2, v3, v4, vTextureDiagram, vTextureDiagram2, vGeometryDiagram, vGeometryDiagram2;

	float localMemoryTextureLimit = sys_LocalMemoryTextureLimit * 1024.f * 1024.f;
	float localMemoryGeometryLimit = sys_LocalMemoryGeometryLimit * 1024.f * 1024.f;

	for (int y = yMin; y < yMax; y++)
	{
		sector = &m_arrSectors[xMin + y * LOCALMEMORY_SECTOR_NR_X];
		for (int x = xMin; x < xMax; x++)
		{
			pos.x = (x + 0.5f) * LOCALMEMORY_SECTOR_SIZE;
			pos.y = (y + 0.5f) * LOCALMEMORY_SECTOR_SIZE;
			pos.z = gEnv->p3DEngine->GetTerrainElevation(pos.x, pos.y);

			if (m_pDebugDraw->GetIVal() == 17)
			{
				float textureRatio = sector->m_memoryUsage_Textures / localMemoryTextureLimit;
				float geometryRatio = sector->m_memoryUsage_Geometry / localMemoryGeometryLimit;
				float maxRatio = max(textureRatio, geometryRatio);

				v0 = pos;
				v1 = pos + Vec3(-sys_LocalMemoryObjectWidth, -sys_LocalMemoryObjectWidth, sys_LocalMemoryObjectHeight);
				v2 = pos + Vec3(sys_LocalMemoryObjectWidth, -sys_LocalMemoryObjectWidth, sys_LocalMemoryObjectHeight);
				v3 = pos + Vec3(sys_LocalMemoryObjectWidth, sys_LocalMemoryObjectWidth, sys_LocalMemoryObjectHeight);
				v4 = pos + Vec3(-sys_LocalMemoryObjectWidth, sys_LocalMemoryObjectWidth, sys_LocalMemoryObjectHeight);

				if (camera->Project(v1, projectedPos) || camera->Project(v2, projectedPos) || camera->Project(v3, projectedPos) || camera->Project(v4, projectedPos))
				{
					vTextureDiagram = pos + Vec3(0.f, sys_LocalMemoryObjectWidth + sys_LocalMemoryDiagramDistance + sys_LocalMemoryDiagramRadius, sys_LocalMemoryObjectHeight);
					vGeometryDiagram = pos + Vec3(0.f, -sys_LocalMemoryObjectWidth - sys_LocalMemoryDiagramDistance - sys_LocalMemoryDiagramRadius, sys_LocalMemoryObjectHeight);

					if (maxRatio < sys_LocalMemoryWarningRatio)
					{
						color = colorOK;
						pColor = fColorOK;
					}
					else if (maxRatio < 1.0f)
					{
						color = colorWarning;
						pColor = fColorWarning;
					}
					else
					{
						color = colorError;
						pColor = fColorError;
					}

					bool close = (Distance::Point_Point2D(cameraPos, pos) < sys_LocalMemoryInnerViewDistance);
					if (close)
					{
						pRenderAuxGeom->SetRenderFlags(auxGeomRenderFlagsClose);

						static int xDebug = 14;
						static int yDebug = 19;

						if (sys_LocalMemoryDrawText)
						{
							camera->Project(pos, projectedPos);

							IRenderAuxText::Draw2dLabel(projectedPos.x, projectedPos.y, 1.2f, pColor, true, "Text: %.2fMb Geom: %.2fMb", sector->m_memoryUsage_Textures / (1024.f * 1024.f), sector->m_memoryUsage_Geometry / (1024.f * 1024.f));
							/* For sector debugging
							              IRenderAuxText::Draw2dLabel(projectedPos.x, projectedPos.y, 1.2f, pColor, true, "Text: %.1fMb Geom: %.1fMb - %d : %d", sector->m_memoryUsage_Textures/(1024.f*1024.f), sector->m_memoryUsage_Geometry/(1024.f*1024.f), x, y);

							              if( xDebug == x && yDebug == y )
							              {
							                float sx = projectedPos.x-80;
							                float sy = projectedPos.y;

							                for( TStatObjMap::iterator it = m_globalStatObjs.begin(); it != m_globalStatObjs.end(); ++it )
							                {
							                  SStatObjInfo *statObjInfo = &it->second;
							                  IStatObj* pStatObj = statObjInfo->m_pStatObj;

							                  int memoryUsage = 0;
							                  float mipFactor = statObjInfo->m_arrMipFactor[y * m_actProcessedSectors.w + x];
							                  if( mipFactor != BIG_NUMBER )
							                  {
							                    memoryUsage = statObjInfo->m_streamableContentMemoryUsage;
							                  }
							                  sy+=15.f;
							                  IRenderAuxText::Draw2dLabel(sx, sy, 1.2f, pColor, false, "Geom: %.2fMb %.4f Act:%s Orig:%s %s", memoryUsage/(1024.f*1024.f), mipFactor, pStatObj->GetFilePath(), statObjInfo->m_filePath, statObjInfo->m_bSubObject ? " SUBOBJECT" : "");
							                  //IRenderAuxText::Draw2dLabel(sx, sy, 1.2f, pColor, false, "Geom: %.2fMb %.4f", memoryUsage/(1024.f*1024.f), mipFactor);
							                }
							              }
							   //*/
						}
						if (sys_LocalMemoryLogText)
						{
							if (xDebug == x && yDebug == y)
							{
								for (TTextureMap::iterator it = m_globalTextures.begin(); it != m_globalTextures.end(); ++it)
								{
									STextureInfo* textureObjInfo = &it->second;
									ITexture* pTexture = textureObjInfo->m_pTexture;

									float mipFactor = textureObjInfo->m_arrMipFactor[y * m_actProcessedSectors.w + x];
									if (mipFactor != BIG_NUMBER)
									{
										CryLog("Texture: %s %dX%d mipfactor:%.4f", pTexture->GetName(), pTexture->GetWidth(), pTexture->GetHeight(), mipFactor);
									}
								}
								sys_LocalMemoryLogText = false;   //Log only once!
							}
						}
					}
					else
					{
						pRenderAuxGeom->SetRenderFlags(auxGeomRenderFlagsFar);
					}

					pRenderAuxGeom->DrawTriangle(v1, color, v2, color, v3, color);
					pRenderAuxGeom->DrawTriangle(v3, color, v4, color, v1, color);

					pRenderAuxGeom->DrawTriangle(v0, color, v1, color, v4, color);
					pRenderAuxGeom->DrawTriangle(v0, color, v2, color, v1, color);
					pRenderAuxGeom->DrawTriangle(v0, color, v3, color, v2, color);
					pRenderAuxGeom->DrawTriangle(v0, color, v4, color, v3, color);

					if (close)
					{
						LocalMemoryUsageDrawHelper::DrawCircle(pRenderer, camera, vTextureDiagram, sys_LocalMemoryDiagramRadius, textureRatio, colorTexture, colorBlack);
						LocalMemoryUsageDrawHelper::DrawCircle(pRenderer, camera, vGeometryDiagram, sys_LocalMemoryDiagramRadius, geometryRatio, colorGeometry, colorBlack);
					}
				}
			}

			float localMemoryTextureStreamingSpeedLimit = sys_LocalMemoryTextureStreamingSpeedLimit * 1024.f * 1024.f;
			float localMemoryStatObjStreamingSpeedLimit = sys_LocalMemoryGeometryStreamingSpeedLimit * 1024.f * 1024.f;

			sector++;
		}
	}
}

void CLocalMemoryUsage::OnUpdate()
{
	if (!m_pDebugDraw || (m_pDebugDraw->GetIVal() != 17))
		return;

	//CryLogAlways("OnUpdate start-----------------------------------------");
	CTimeValue actTime = gEnv->pTimer->GetAsyncTime();

	float callTimeDiff = (actTime - m_LastCallTime).GetSeconds();
	float neededCallTimeDiff = m_AverageUpdateTime / (sys_LocalMemoryOptimalMSecPerSec / 1000.f);

	if (callTimeDiff < neededCallTimeDiff && callTimeDiff < sys_LocalMemoryMaxMSecBetweenCalls / 1000.f)
		return;

	DeleteUnusedResources();

	m_LastCallTime = actTime;

	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "! LocalMemoryUsege::Update - Start");
		//StartChecking(RectI(0,0,m_sectorNr.x, m_sectorNr.y));
		StartChecking(m_actDrawedSectors);
	}

	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "! LocalMemoryUsege::Update - CollectGeometry P1");
		CollectGeometryP1();
	}
	{
		ITerrain* pTerrain = gEnv->p3DEngine->GetITerrain();

		if (pTerrain)
		{
			TMaterialMap m_detailTerrainTextureMaterials;

			int materialNumber = pTerrain->GetDetailTextureMaterials(NULL);
			PodArray<IMaterial*> materials(materialNumber, materialNumber);

			pTerrain->GetDetailTextureMaterials(&materials[0]);

			AABB bigAABB(1e8f);

			for (int i = 0; i < materialNumber; i++)
			{
				IMaterial* pMaterial = materials[i];

				CheckMeshMaterialP1(NULL, pMaterial, bigAABB, 1e8f, 1.0f);
			}
		}
	}

	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "! LocalMemoryUsege::Update - Materials P2");
		//We must iterate on materials first
		for (TMaterialMap::iterator it = m_globalMaterials.begin(); it != m_globalMaterials.end(); ++it)
		{
			it->second.CheckOnAllSectorsP2();
		}
	}
	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "! LocalMemoryUsege::Update - Textures P2");
		for (TTextureMap::iterator it = m_globalTextures.begin(); it != m_globalTextures.end(); ++it)
		{
			it->second.CheckOnAllSectorsP2();
		}
	}
	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "! LocalMemoryUsege::Update - StatObjects P2");
		for (TStatObjMap::iterator it = m_globalStatObjs.begin(); it != m_globalStatObjs.end(); ++it)
		{
			it->second.CheckOnAllSectorsP2();
		}
	}

	CTimeValue endTime = gEnv->pTimer->GetAsyncTime();
	m_AverageUpdateTime = LERP(m_AverageUpdateTime, (endTime - actTime).GetSeconds(), 0.5f);

	//CryLogAlways("OnUpdate end***");
}

void CLocalMemoryUsage::DeleteUnusedResources()
{
	CRY_PROFILE_REGION(PROFILE_SYSTEM, "! LocalMemoryUsege::Update - Deleting unused resources");

	// Clear the used flags
	for (TStatObjMap::iterator it = m_globalStatObjs.begin(); it != m_globalStatObjs.end(); ++it)
		it->second.m_used = false;

	// Iterate on used StatObjs
	int nCount;
	gEnv->p3DEngine->GetLoadedStatObjArray(NULL, nCount);
	std::vector<IStatObj*> objectsArray;
	objectsArray.resize(nCount);
	if (nCount > 0)
	{
		gEnv->p3DEngine->GetLoadedStatObjArray(&objectsArray[0], nCount);
	}

	for (std::vector<IStatObj*>::iterator it = objectsArray.begin(); it != objectsArray.end(); ++it)
	{
		IStatObj* pStatObj = *it;
		TStatObjMap::iterator findIt = m_globalStatObjs.find((INT_PTR)pStatObj);

		if (findIt != m_globalStatObjs.end() && strcmp(pStatObj->GetFilePath(), findIt->second.m_filePath) == 0 && !findIt->second.m_bSubObject)
		{
			findIt->second.m_used = true;
		}

		if (pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
		{
			for (int k = 0; k < pStatObj->GetSubObjectCount(); k++)
			{
				if (!pStatObj->GetSubObject(k))
					continue;

				IStatObj* pSubStatObj = pStatObj->GetSubObject(k)->pStatObj;

				if (pSubStatObj)
				{
					findIt = m_globalStatObjs.find((INT_PTR)pSubStatObj);

					if (findIt != m_globalStatObjs.end() && strcmp(pSubStatObj->GetFilePath(), findIt->second.m_filePath) == 0 && findIt->second.m_bSubObject)
					{
						findIt->second.m_used = true;
					}
				}
			}
		}
	}

	// Erase non-used StatObjs
	bool changed = true;
	while (changed)
	{
		changed = false;
		for (TStatObjMap::iterator it = m_globalStatObjs.begin(); it != m_globalStatObjs.end(); ++it)
		{
			if (!it->second.m_used)
			{
				m_globalStatObjs.erase(it);
				changed = true;
				break;
			}
		}
	}
	/*
	   /*
	   for( TTextureMap::iterator it = m_globalTextures.begin(); it != m_globalTextures.end(); ++it )
	    it->second.m_used = false;

	   bool changed = true;
	   while( changed )
	   {
	    changed = false;
	    for( TTextureMap::iterator it = m_globalTextures.begin(); it != m_globalTextures.end(); ++it )
	    {
	      if( !it->second.m_used )
	      {
	        m_globalTextures.erase(it);
	        changed = true;
	        break;
	      }
	    }
	   }

	   for( TMaterialMap::iterator it = m_globalMaterials.begin(); it != m_globalMaterials.end(); ++it )
	    it->second.m_used = false;

	   changed = true;
	   while( changed )
	   {
	    changed = false;
	    for( TMaterialMap::iterator it = m_globalMaterials.begin(); it != m_globalMaterials.end(); ++it )
	    {
	      if( !it->second.m_used )
	      {
	        m_globalMaterials.erase(it);
	        changed = true;
	        break;
	      }
	    }
	   }
	   //*/
}
void CLocalMemoryUsage::StartChecking(const RectI& actProcessedSectors)
{
	m_actProcessedSectors = actProcessedSectors;

	for (int y = 0; y < m_sectorNr.y; y++)
	{
		SSector* sector = &m_arrSectors[y * LOCALMEMORY_SECTOR_NR_X];
		for (int x = 0; x < m_sectorNr.x; x++)
		{
			sector->StartChecking();
			sector++;
		}
	}

	{
		for (TTextureMap::iterator it = m_globalTextures.begin(); it != m_globalTextures.end(); ++it)
		{
			//CRY_PROFILE_REGION(PROFILE_SYSTEM, "LocalMemoryUsege::StartChecking - Textures");
			it->second.StartChecking();
		}
	}
	{
		for (TMaterialMap::iterator it = m_globalMaterials.begin(); it != m_globalMaterials.end(); ++it)
		{
			//CRY_PROFILE_REGION(PROFILE_SYSTEM, "LocalMemoryUsege::StartChecking - Materials");
			it->second.StartChecking();
		}
	}
	{
		for (TStatObjMap::iterator it = m_globalStatObjs.begin(); it != m_globalStatObjs.end(); ++it)
		{
			//CRY_PROFILE_REGION(PROFILE_SYSTEM, "LocalMemoryUsege::StartChecking - StatObjects");
			it->second.StartChecking();
		}
	}
}

void CLocalMemoryUsage::CollectGeometryP1()
{
	ISystem* pSystem = GetISystem();
	I3DEngine* p3DEngine = pSystem->GetI3DEngine();

	// iterate through all instances
	std::vector<IRenderNode*> renderNodes;

	uint32 dwCount = 0;

	dwCount += p3DEngine->GetObjectsByType(eERType_Light);
	dwCount += p3DEngine->GetObjectsByType(eERType_Brush);
	dwCount += p3DEngine->GetObjectsByType(eERType_Vegetation);
	dwCount += p3DEngine->GetObjectsByType(eERType_Decal);
	dwCount += p3DEngine->GetObjectsByType(eERType_Road);
	dwCount += p3DEngine->GetObjectsByType(eERType_Character);
	dwCount += p3DEngine->GetObjectsByType(eERType_MovableBrush);

	if (dwCount > 0)
	{
		renderNodes.resize(dwCount + 1);
		dwCount = 0;

		dwCount += p3DEngine->GetObjectsByType(eERType_Light, &renderNodes[dwCount]);
		dwCount += p3DEngine->GetObjectsByType(eERType_Brush, &renderNodes[dwCount]);
		dwCount += p3DEngine->GetObjectsByType(eERType_Vegetation, &renderNodes[dwCount]);
		dwCount += p3DEngine->GetObjectsByType(eERType_Decal, &renderNodes[dwCount]);
		dwCount += p3DEngine->GetObjectsByType(eERType_Road, &renderNodes[dwCount]);
		dwCount += p3DEngine->GetObjectsByType(eERType_Character, &renderNodes[dwCount]);
		dwCount += p3DEngine->GetObjectsByType(eERType_MovableBrush, &renderNodes[dwCount]);

		AABB objBox;

		for (uint32 dwI = 0; dwI < dwCount; ++dwI)
		{
			IRenderNode* pRenderNode = renderNodes[dwI];

			if (pRenderNode->m_fWSMaxViewDist < 0.01f)
			{
				continue;
			}

			if ((pRenderNode->GetRenderNodeType() == eERType_Vegetation) && (pRenderNode->GetRndFlags() & ERF_PROCEDURAL))
			{
				continue;                                           //Procedural generated vegetation. TODO we must calculate it by other way
			}

			float objScale = 1.f;
			if (pRenderNode->GetRenderNodeType() == eERType_Decal)
			{
				IDecalRenderNode* pDecal = (IDecalRenderNode*)pRenderNode;
				objScale = max(0.001f, pDecal->GetMatrix().GetColumn0().GetLength());
			}
			else if (pRenderNode->GetRenderNodeType() == eERType_Vegetation)
			{
				IVegetation* pVeg = (IVegetation*)pRenderNode;
				objScale = max(0.001f, pVeg->GetScale());
			}
			else if (pRenderNode->GetRenderNodeType() == eERType_Brush)
			{
				objScale = max(0.001f, ((IBrush*)pRenderNode)->GetMatrix().GetColumn0().GetLength());
			}
			else if (pRenderNode->GetRenderNodeType() == eERType_FogVolume)
			{
				objScale = max(0.001f, ((IFogVolumeRenderNode*)pRenderNode)->GetMatrix().GetColumn0().GetLength());
			}

			float maxViewDist = pRenderNode->m_fWSMaxViewDist;
			pRenderNode->FillBBox(objBox);

			IMaterial* pRenderNodeMat = pRenderNode->GetMaterialOverride();

			{
				IStatObj* pStatObj = pRenderNode->GetEntityStatObj();

				if (pStatObj != NULL)
				{
					CheckStatObjMaterialP1(pStatObj, pRenderNodeMat, objBox, maxViewDist, objScale);
				}
				else
				{
					CheckMeshMaterialP1(pRenderNode->GetRenderMesh(0), pRenderNodeMat, objBox, maxViewDist, objScale);
				}
			}

			Matrix34A matParent;

			if (IStatObj* pStatObj = pRenderNode->GetEntityStatObj(0, &matParent, true))
			{
				CheckStatObjP1(pStatObj, pRenderNode, objBox, maxViewDist, objScale);

				IMaterial* pStatObjMat = pStatObj->GetMaterial();
				CheckStatObjMaterialP1(pStatObj, pRenderNodeMat ? pRenderNodeMat : pStatObjMat, objBox, maxViewDist, objScale);
			}

			if (ICharacterInstance* pCharacter = pRenderNode->GetEntityCharacter(&matParent, true))
			{
				CheckCharacterP1(pCharacter, pRenderNode, pRenderNodeMat, objBox, maxViewDist, objScale, 2);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PREFAST_SUPPRESS_WARNING(6262);
CLocalMemoryUsage::SStatObjInfo* CLocalMemoryUsage::CheckStatObjP1(IStatObj* pStatObj, IRenderNode* pRenderNode, AABB bounding, float maxViewDist, float scale)
{
	if (!pStatObj)
		return NULL;

	if (strncmp(pStatObj->GetFilePath(), "%level%", 7) == 0)     //HACK %LEVEL% brushes
		return NULL;

	SStatObjInfo* pStatObjInfo = NULL;

	TStatObjMap::iterator iter = m_globalStatObjs.find((INT_PTR)pStatObj);
	if (iter != m_globalStatObjs.end())
	{
		pStatObjInfo = &iter->second;
		pStatObjInfo->m_streamableContentMemoryUsage = pStatObj->GetStreamableContentMemoryUsage(); //TODO TEMP
	}
	else
	{
		{
			CRY_PROFILE_REGION(PROFILE_SYSTEM, "! CLocalMemoryUsage::CheckStatObjP1 HASH");
			PREFAST_SUPPRESS_WARNING(6262)
			pStatObjInfo = &m_globalStatObjs[(INT_PTR)pStatObj];
		}

		pStatObjInfo->Init(this);
		pStatObjInfo->m_streamableContentMemoryUsage = pStatObj->GetStreamableContentMemoryUsage();
		//pStatObjInfo->m_pStatObj = pStatObj;
		pStatObjInfo->m_filePath = pStatObj->GetFilePath();
		pStatObjInfo->m_bSubObject = pStatObj->IsSubObject();
		//CollectStatObjInfo_Recursive( pStatObjInfo, pStatObj );
	}

	pStatObjInfo->CheckOnAllSectorsP1(bounding, maxViewDist, scale, 1.0f);

	return pStatObjInfo;
}
/*
   void CLocalMemoryUsage::CollectStatObjInfo_Recursive( SStatObjInfo* statObjInfo, IStatObj *pStatObj )
   {
   //	We collected this material previously
   //	if(pStatObj->GetMaterial())
   //	{
   //		statObjInfo->AddMaterialInfo( CheckMaterialP1(pStatObj->GetMaterial()) );
   //	}

   bool bMultiSubObj = (pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND) != 0;

   if (!bMultiSubObj)
   {
    CollectGeometryInfo( statObjInfo, pStatObj );
   }
   else
   {
    for (int k = 0; k < pStatObj->GetSubObjectCount(); k++)
    {
      if (!pStatObj->GetSubObject(k))
        continue;

      IStatObj *pSubStatObj = pStatObj->GetSubObject(k)->pStatObj;

      if(pSubStatObj)
      {
        CollectStatObjInfo_Recursive( statObjInfo, pSubStatObj );	//(Spidy) Must be recursive
      }
    }
   }
   }
   void CLocalMemoryUsage::CollectGeometryInfo( SStatObjInfo* statObjInfo, IStatObj *pStatObj )
   {
   //////////////////////////////////////////////////////////////////////////
   // Iterate LODs.
   //////////////////////////////////////////////////////////////////////////

   IStatObj *pLod0 = 0;
   for (int lod = 0; lod < MAX_LODS; lod++)
   {
    IStatObj *pLod = pStatObj->GetLodObject(lod);
    if (pLod)
    {
      if (!pLod0 && pLod->GetRenderMesh())
        pLod0 = pLod;

      statObjInfo->m_lodNr = max(statObjInfo->m_lodNr, lod);			// Assign last existing lod.

      if (pLod->GetRenderMesh())
      {
        statObjInfo->m_indicesPerLod[lod] += pLod->GetRenderMesh()->GetSysIndicesCount();
        statObjInfo->m_meshSize += pLod->GetRenderMesh()->GetMemoryUsage(0,IRenderMesh::MEM_USAGE_COMBINED);

        //IMaterial *pMaterial = pLod->GetMaterial();
        //statObjInfo->AddMaterialInfo( CheckMaterialP1( pMaterial ) );	//(Spidy) We must use all textures in material, not only the rendered ones!
      }
    }
   }

   if (pLod0 && pLod0->GetRenderMesh())
   {
    IRenderMesh *pRenderMesh = pLod0->GetRenderMesh();
    statObjInfo->m_vertices += pRenderMesh->GetVertCount();
    statObjInfo->m_indices += pRenderMesh->GetSysIndicesCount();
   }

   for(int j = 0; j < 4; j++)
   {
    if (pStatObj->GetPhysGeom(j))
    {
      CrySizerImpl physMeshSizer;
      pStatObj->GetPhysGeom(j)->pGeom->GetMemoryStatistics(&physMeshSizer);
      statObjInfo->m_physProxySize += physMeshSizer.GetTotalSize();
      statObjInfo->m_physPrimitives += pStatObj->GetPhysGeom(j)->pGeom->GetPrimitiveCount();
    }
   }
   }
 */

void CLocalMemoryUsage::CheckCharacterP1(ICharacterInstance* pCharacter, IRenderNode* pRenderNode, IMaterial* pSlotMat, AABB bounding, float maxViewDist, float scale, int nMaxDepth)
{
	if (!nMaxDepth || !pCharacter)
		return;
	nMaxDepth--;

	IAttachmentManager* pAttMan = pCharacter->GetIAttachmentManager();
	if (pAttMan)
	{
		if (ICharacterInstance* pCharInstance = pAttMan->GetSkelInstance())
		{
			if (pCharInstance != pCharacter)
				CheckCharacterP1(pCharacter, pRenderNode, pSlotMat, bounding, maxViewDist, scale, nMaxDepth);
		}

		int nCount = pAttMan->GetAttachmentCount();
		for (int i = 0; i < nCount; i++)
		{
			if (IAttachment* pAtt = pAttMan->GetInterfaceByIndex(i))
			{
				if (IAttachmentObject* pAttObj = pAtt->GetIAttachmentObject())
				{
					IMaterial* pAttMatOverride = (IMaterial*)pAttObj->GetReplacementMaterial();
					IMaterial* pAttMat = pSlotMat ? pSlotMat : (pAttMatOverride ? pAttMatOverride : (IMaterial*)pAttObj->GetBaseMaterial());

					ICharacterInstance* pSkelInstance = pAttObj->GetICharacterInstance();
					if (pSkelInstance)
						CheckCharacterP1(pCharacter, pRenderNode, pAttMat, bounding, maxViewDist, scale, nMaxDepth);

					IAttachmentSkin* pIAttachmentSkin = pAttObj->GetIAttachmentSkin();
					if (pIAttachmentSkin)
					{
						IMaterial* pCharObjMat = (IMaterial*)pIAttachmentSkin->GetISkin()->GetIMaterial(0);
						ISkin* pISkin = pIAttachmentSkin->GetISkin();
						IRenderMesh* pIRenderMesh = pISkin->GetIRenderMesh(-1); //get the baseLOD
						CheckMeshMaterialP1(pIRenderMesh, pSlotMat ? pSlotMat : pCharObjMat, bounding, maxViewDist, scale);
					}

					if (IStatObj* pStatObj = pAttObj->GetIStatObj())
					{
						if (!pStatObj || pStatObj->GetFlags() & STATIC_OBJECT_HIDDEN)
							continue;

						CheckStatObjP1(pStatObj, pRenderNode, bounding, maxViewDist, scale);

						IMaterial* pStatObjMat = pStatObj->GetMaterial();
						CheckStatObjMaterialP1(pStatObj, pAttMat ? pAttMat : pStatObjMat, bounding, maxViewDist, scale);
					}
				}
			}
		}

		IMaterial* pCharObjMat = pCharacter->GetIMaterial();
		IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
		IRenderMesh* pIRenderMesh = rIDefaultSkeleton.GetIRenderMesh(); //get the baseLOD
		CheckMeshMaterialP1(pIRenderMesh, pSlotMat ? pSlotMat : pCharObjMat, bounding, maxViewDist, scale);
	}

	// joints
	if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
	{
		// check StatObj attachments
		uint32 numJoints = pCharacter->GetIDefaultSkeleton().GetJointCount();
		for (uint32 i = 0; i < numJoints; i++)
		{
			IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(i);
			if (!pStatObj || pStatObj->GetFlags() & STATIC_OBJECT_HIDDEN)
				continue;

			CheckStatObjP1(pStatObj, pRenderNode, bounding, maxViewDist, scale);
			IMaterial* pStatObjMat = pStatObj->GetMaterial();
			CheckStatObjMaterialP1(pStatObj, pSlotMat ? pSlotMat : pStatObjMat, bounding, maxViewDist, scale);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PREFAST_SUPPRESS_WARNING(6262);
void CLocalMemoryUsage::CheckMaterialP1(IMaterial* pMaterial, AABB bounding, float maxViewDist, float scale, float mipFactor)
{
	if (pMaterial)
	{
		SMaterialInfo* pMaterialInfo = NULL;

		TMaterialMap::iterator iter = m_globalMaterials.find((INT_PTR)pMaterial);
		if (iter != m_globalMaterials.end())
		{
			pMaterialInfo = &iter->second;
		}
		else
		{
			{
				CRY_PROFILE_REGION(PROFILE_SYSTEM, "! CLocalMemoryUsage::CheckMaterialP1 HASH");
				PREFAST_SUPPRESS_WARNING(6262)
				pMaterialInfo = &m_globalMaterials[(INT_PTR)pMaterial];
			}
			pMaterialInfo->Init(this);
			//pMaterialInfo->m_pMaterial=pMaterial;
			CollectMaterialInfo_Recursive(pMaterialInfo, pMaterial);
		}

		pMaterialInfo->CheckOnAllSectorsP1(bounding, maxViewDist, scale, mipFactor);
	}
}

PREFAST_SUPPRESS_WARNING(6262);
void CLocalMemoryUsage::CheckChunkMaterialP1(IMaterial* pMaterial, AABB bounding, float maxViewDist, float scale, CRenderChunk* pRenderChunk)
{
	IMaterial* pCurrentMaterial = pMaterial;

	if (pRenderChunk != NULL)
	{
		if (pRenderChunk->m_nMatID < pMaterial->GetSubMtlCount())
		{
			pCurrentMaterial = pMaterial->GetSubMtl(pRenderChunk->m_nMatID);
		}

		if (pCurrentMaterial != NULL)
		{
			CheckMaterialP1(pCurrentMaterial, bounding, maxViewDist, scale, pRenderChunk->m_texelAreaDensity);
		}
	}
	else
	{
		CheckMaterialP1(pCurrentMaterial, bounding, maxViewDist, scale, 1.f);
	}
}

PREFAST_SUPPRESS_WARNING(6262);
void CLocalMemoryUsage::CheckMeshMaterialP1(IRenderMesh* pRenderMesh, IMaterial* pMaterial, AABB bounding, float maxViewDist, float scale)
{
	if (pRenderMesh)
	{
		if (pMaterial)
		{
			TRenderChunkArray* pChunks = &pRenderMesh->GetChunks();

			if (pChunks != NULL)
			{
				for (unsigned int i = 0; i < pChunks->size(); i++)
				{
					CheckChunkMaterialP1(pMaterial, bounding, maxViewDist, scale, &(*pChunks)[i]);
				}
			}

			pChunks = &pRenderMesh->GetChunksSkinned();
			for (unsigned int i = 0; i < pChunks->size(); i++)
			{
				CheckChunkMaterialP1(pMaterial, bounding, maxViewDist, scale, &(*pChunks)[i]);
			}
		}
	}
	else
	{
		if (pMaterial)
		{
			CheckChunkMaterialP1(pMaterial, bounding, maxViewDist, scale, NULL);
		}
	}
}

void CLocalMemoryUsage::CheckStatObjMaterialP1(IStatObj* pStatObj, IMaterial* pMaterial, AABB bounding, float maxViewDist, float scale)
{
	if (pStatObj)
	{
		for (int i = 0; i < pStatObj->GetSubObjectCount(); i++)
		{
			CheckStatObjMaterialP1(pStatObj->GetSubObject(i)->pStatObj, pMaterial, bounding, scale, maxViewDist);
		}

		CheckMeshMaterialP1(pStatObj->GetRenderMesh(), pMaterial, bounding, maxViewDist, scale);
	}
}

void CLocalMemoryUsage::CollectMaterialInfo_Recursive(SMaterialInfo* materialInfo, IMaterial* pMaterial)
{
	SShaderItem& rItem = pMaterial->GetShaderItem();

	uint32 dwSubMatCount = pMaterial->GetSubMtlCount();

	for (uint32 dwSubMat = 0; dwSubMat < dwSubMatCount; ++dwSubMat)
	{
		IMaterial* pSub = pMaterial->GetSubMtl(dwSubMat);

		if (pSub)
			CollectMaterialInfo_Recursive(materialInfo, pSub);
	}

	// this pMaterial
	if (rItem.m_pShaderResources)
	{
		for (uint32 dwI = 0; dwI < EFTT_MAX; ++dwI)
		{
			SEfResTexture* pTex = rItem.m_pShaderResources->GetTexture(dwI);

			if (pTex && pTex->m_Sampler.m_pITex)
			{
				ITexture* pTexture = pTex->m_Sampler.m_pITex;
				if (pTexture && pTexture->GetStreamableMipNumber() > 0)
				{
					STextureInfoAndTilingFactor textureInfo;

					textureInfo.m_pTextureInfo = GetTextureInfo(pTexture);
					textureInfo.m_tilingFactor = pTex->GetTiling(0) * pTex->GetTiling(1);

					materialInfo->AddTextureInfo(textureInfo);
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PREFAST_SUPPRESS_WARNING(6262)
CLocalMemoryUsage::STextureInfo * CLocalMemoryUsage::GetTextureInfo(ITexture * pTexture)
{
	if (!pTexture)
		return NULL;

	TTextureMap::iterator iter = m_globalTextures.find((INT_PTR)pTexture);
	if (iter != m_globalTextures.end())
	{
		return &iter->second;
	}

	//CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	STextureInfo* pTextureInfo;
	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "! CLocalMemoryUsage::CheckStatObjP1 HASH");
		PREFAST_SUPPRESS_WARNING(6262)
		pTextureInfo = &m_globalTextures[(INT_PTR)pTexture];
	}
	pTextureInfo->Init(this);
	pTextureInfo->m_pTexture = pTexture;
	pTexture->AddRef();

	//Collect informations
	//pTextureInfo->m_size = pTexture->GetDeviceDataSize();
	//pTextureInfo->m_xSize = pTexture->GetWidth();
	//pTextureInfo->m_ySize = pTexture->GetHeight();
	//pTextureInfo->m_numMips = pTexture->GetNumMips();

	return pTextureInfo;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma warning(pop)
