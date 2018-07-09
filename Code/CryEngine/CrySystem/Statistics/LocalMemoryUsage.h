// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   LocalMemoryUsage.h
//  Version:     v1.00
//  Created:     Istvan Kiss (Spidy) [11/5/2008]
//  Description: Visual helper for checking a local memory usage on maps
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _LOCAL_MEMORY_USAGE_H
#define _LOCAL_MEMORY_USAGE_H

#include <CryMath/Cry_Geo.h>
#include <CryMemory/ILocalMemoryUsage.h>
#include <CrySystem/TimeValue.h>

#define LOCALMEMORY_SECTOR_SIZE     32.f        //Size of one sector
#define LOCALMEMORY_SECTOR_NR_X     128         //4096 / LOCALMEMORY_SECTOR_SIZE	Sector number of the world (x)
#define LOCALMEMORY_SECTOR_NR_Y     128         //4096 / LOCALMEMORY_SECTOR_SIZE  Sector number of the world (y)
#define LOCALMEMORY_SECTOR_PER_PASS 128         //4096 / 32												Sector nr for one calculation pass. Now it is the whole world;

//Sector number must be = LOCALMEMORY_SECTOR_PER_PASS * n

#define LOCALMEMORY_COLOR_OK       0, 255, 0
#define LOCALMEMORY_COLOR_WARNING  255, 255, 0
#define LOCALMEMORY_COLOR_ERROR    255, 0, 0
#define LOCALMEMORY_COLOR_TEXTURE  0, 0, 255
#define LOCALMEMORY_COLOR_GEOMETRY 0, 0, 0
#define LOCALMEMORY_COLOR_BLACK    0, 0, 0, 192
#define LOCALMEMORY_FCOLOR_OK      0, 1, 0
#define LOCALMEMORY_FCOLOR_WARNING 1, 1, 0
#define LOCALMEMORY_FCOLOR_ERROR   1, 0, 0
#define LOCALMEMORY_FCOLOR_OTHER   0, 0, 1

struct IMaterial;
struct IStatObj;
struct ICharacterInstance;
struct IRenderMesh;
struct IRenderNode;
struct CRenderChunk;
class CCamera;

#include <CryCore/Containers/VectorMap.h>

struct CLocalMemoryUsage : public ILocalMemoryUsage
{
private:

	struct STextureInfo;
	struct SMaterialInfo;
	struct SStatObjInfo;

	typedef std::unordered_map<INT_PTR, STextureInfo>  TTextureMap;                     // Type: hash map of textures
	typedef std::unordered_map<INT_PTR, SMaterialInfo> TMaterialMap;                    // Type: hash map of materials
	typedef std::unordered_map<INT_PTR, SStatObjInfo>  TStatObjMap;                     // Type: hash map of stat objects

	//*******************

	struct SResource
	{
		CLocalMemoryUsage* m_pLocalMemoryUsage;                                       // Inner pointer to the singleton (we can use gEnv->pLocalMemoryUsage with type cast instead this)

		float              m_arrMipFactor[LOCALMEMORY_SECTOR_PER_PASS * LOCALMEMORY_SECTOR_PER_PASS]; // Mipmap factor based on minimum distance of the resource from the sector (bounding box distance)
		bool               m_arrRowDirty[LOCALMEMORY_SECTOR_PER_PASS];                                // Dirty flag for m_arrMipFactor rows
		bool               m_used;

		static int         m_arrMemoryUsage[LOCALMEMORY_SECTOR_PER_PASS];             // Memory usage of the resource in sectors (last row)
		static int         m_arrPieces[LOCALMEMORY_SECTOR_PER_PASS];                  // Pieces nr of the resource in sectors (last row)

		virtual void StartChecking();                                                                                               // Called every frame first
		void         CheckOnAllSectorsP1(const AABB& bounding, float maxViewDist, float viewDistMultiplierForLOD, float mipFactor); // Calculates the minimum of sector bounding vs renderobject bounding distances

		SResource();
		virtual void Init(CLocalMemoryUsage* pLocalMemoryUsage);
		//If we will needed a destructor, it must be virtual!
	};

	//*******************

	struct STextureInfo : public SResource
	{
		ITexture* m_pTexture;                             // Texture pointer
		//int m_size;																			// For later use - Used memory
		//int m_xSize;																		// For later use - Texture x size
		//int m_ySize;																		// For later use - Texture y size
		//int m_numMips;																	// For later use - Number of mip maps

		void CheckOnAllSectorsP2();                       // Calculate memory usage using distances

		STextureInfo();
		~STextureInfo();
	};

	//*******************

	struct STextureInfoAndTilingFactor
	{
		STextureInfo* m_pTextureInfo;
		float         m_tilingFactor;
	};

	typedef std::vector<STextureInfoAndTilingFactor> TTextureVector;              // Type: vector of textures

	struct SMaterialInfo : public SResource
	{
		//_smart_ptr<IMaterial> m_pMaterial;							// For later use
		TTextureVector m_textures;                        // Used textures

		SMaterialInfo();

		void AddTextureInfo(STextureInfoAndTilingFactor texture);// Add a new texture (unique)
		void CheckOnAllSectorsP2();                       // Forward distance informations to textures
	};

	//*******************

	struct SStatObjInfo : public SResource
	{
		int    m_streamableContentMemoryUsage;            // Total streamable memory usage
		//IStatObj *m_pStatObj;						//TODO kiszedni
		string m_filePath;                                // The original file name and path of the StatObj
		bool   m_bSubObject;                              // Is the original StatObj a subobject?
		/*
		    int m_lodNr;																			// For later use
		    int m_vertices;
		    int m_meshSize;
		    int m_physProxySize;
		    int m_physPrimitives;
		    int m_indices;
		    int m_indicesPerLod[MAX_LODS];
		 */

		SStatObjInfo();
		void CheckOnAllSectorsP2();                       // Calculate memory usage using distances
	};

	//*******************

	struct SSector
	{
		SSector();

		void StartChecking();                             // Called every frame first

		int  m_memoryUsage_Textures;                      // Sum of texture memory usage

		int  m_memoryUsage_Geometry;                      // Sum of geometry memory usage
	};

	//*******************
	PREFAST_SUPPRESS_WARNING(6262);
	TTextureMap  m_globalTextures;                      // Hash map of resources: textures
	PREFAST_SUPPRESS_WARNING(6262);
	TMaterialMap m_globalMaterials;                     // Hash map of resources: materials
	PREFAST_SUPPRESS_WARNING(6262);
	TStatObjMap  m_globalStatObjs;                      // Hash map of resources: stat objects

	ICVar*       m_pStreamCgfPredicitionDistance;       // Config: additional object distance for streaming
	ICVar*       m_pDebugDraw;                          // Config: debug draw mode

	int          sys_LocalMemoryTextureLimit;                // Config: texture limit for streaming
	int          sys_LocalMemoryGeometryLimit;               // Config: stat object geometry limit for streaming
	int          sys_LocalMemoryTextureStreamingSpeedLimit;  // Config: texture streaming speed limit (approx)
	int          sys_LocalMemoryGeometryStreamingSpeedLimit; // Config: stat object geometry streaming speed limit (approx)

	float        sys_LocalMemoryWarningRatio;           // Config: Warning ratio for streaming
	float        sys_LocalMemoryOuterViewDistance;      // Config: View distance for debug draw
	float        sys_LocalMemoryInnerViewDistance;      // Config: View distance for detailed debug draw

	float        sys_LocalMemoryObjectWidth;            // Config: Width of the debug object
	float        sys_LocalMemoryObjectHeight;           // Config: Height of the debug object
	int          sys_LocalMemoryObjectAlpha;            // Config: Color alpha of the debug object

	float        sys_LocalMemoryStreamingSpeedObjectLength; // Config: Length of the streaming speed debug object
	float        sys_LocalMemoryStreamingSpeedObjectWidth;  // Config: Width of the streaming speed debug object

	float        sys_LocalMemoryDiagramWidth;           // Config: Width of the diagram

	float        sys_LocalMemoryDiagramRadius;                 // Config: Radius of the diagram
	float        sys_LocalMemoryDiagramDistance;               // Config: Distance of the diagram from the main debug object
	float        sys_LocalMemoryDiagramStreamingSpeedRadius;   // Config: Radius of the streaming speed diagram
	float        sys_LocalMemoryDiagramStreamingSpeedDistance; // Config: Distance of the streaming speed diagram from the streaming speed debug line
	int          sys_LocalMemoryDiagramAlpha;                  // Config: Color alpha of the diagram

	int          sys_LocalMemoryDrawText;               // Config: If != 0, it will draw the numeric values
	int          sys_LocalMemoryLogText;                // Config: If != 0, it will log the numeric values

	int          sys_LocalMemoryOptimalMSecPerSec;      // Config: Optimal calculation time (MSec) per secundum
	int          sys_LocalMemoryMaxMSecBetweenCalls;    // Config: Maximal time difference (MSec) between calls

	RectI        m_actProcessedSectors;                 // Processed sectors (now it is the whole world)
	RectI        m_actDrawedSectors;                    // Processed sectors (now it is the whole world)

	Vec2i        m_sectorNr;                            // Sector x and y number

	float        m_AverageUpdateTime;                   // Average Update() time (MSec)
	CTimeValue   m_LastCallTime;                        // Last call time of Update()

	SSector      m_arrSectors[LOCALMEMORY_SECTOR_NR_X * LOCALMEMORY_SECTOR_NR_Y]; // Sectors

public:

	CLocalMemoryUsage();
	~CLocalMemoryUsage();

	virtual void OnRender(IRenderer* pRenderer, const CCamera* camera);
	virtual void OnUpdate();
	virtual void DeleteGlobalData();                      // Deletes the m_globalXXX hash maps

private:
	void DeleteUnusedResources();                         // Delete the non-used resources (especially StatObjs)
	void StartChecking(const RectI& actProcessedSectors); // Called every frame first
	void CollectGeometryP1();                             // Calculates the minimum of sector bounding vs renderobject bounding distances

	// Get / create StatObjInfo then run Pass1
	SStatObjInfo* CheckStatObjP1(IStatObj* pStatObj, IRenderNode* pRenderNode, AABB bounding, float maxViewDist, float scale);
	//void CollectStatObjInfo_Recursive( SStatObjInfo* statObjInfo, IStatObj *pStatObj );
	//void CollectGeometryInfo( SStatObjInfo* statObjInfo, IStatObj *pStatObj );

	void CheckCharacterP1(ICharacterInstance* pCharacter, IRenderNode* pRenderNode, IMaterial* pSlotMat, AABB bounding, float maxViewDist, float scale, int nMaxDepth);

	// Get / create MaterialInfo then run Pass1
	void CheckMaterialP1(IMaterial* pMaterial, AABB bounding, float maxViewDist, float scale, float mipFactor);
	void CheckChunkMaterialP1(IMaterial* pMaterial, AABB bounding, float maxViewDist, float scale, CRenderChunk* pRenderChunk);
	void CheckMeshMaterialP1(IRenderMesh* pRenderMesh, IMaterial* pMaterial, AABB bounding, float maxViewDist, float scale);
	void CheckStatObjMaterialP1(IStatObj* pStatObj, IMaterial* pMaterial, AABB bounding, float maxViewDist, float scale);
	// Collects textures used by the material
	void CollectMaterialInfo_Recursive(SMaterialInfo* materialInfo, IMaterial* pMaterial);

	// Get / create TextureInfo
	STextureInfo* GetTextureInfo(ITexture* pTexture);
};

#undef MAX_LODS
#endif//_LOCAL_MEMORY_USAGE_H
