// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define LV_MAX_COUNT                255
#define LV_CELL_MAX_LIGHTS          64
#define LV_MAX_LIGHTS               2048

#define LV_WORLD_BUCKET_SIZE        512
#define LV_LIGHTS_WORLD_BUCKET_SIZE 512

#define LV_WORLD_SIZEX              128
#define LV_WORLD_SIZEY              128
#define LV_WORLD_SIZEZ              64

#define LV_CELL_SIZEX               4
#define LV_CELL_SIZEY               4
#define LV_CELL_SIZEZ               8

#define LV_CELL_RSIZEX              (1.0f / (float)LV_CELL_SIZEX)
#define LV_CELL_RSIZEY              (1.0f / (float)LV_CELL_SIZEY)
#define LV_CELL_RSIZEZ              (1.0f / (float)LV_CELL_SIZEZ)

#define LV_LIGHT_CELL_SIZE          32
#define LV_LIGHT_CELL_R_SIZE        (1.0f / (float)LV_LIGHT_CELL_SIZE)

#define LV_DLF_LIGHTVOLUMES_MASK    (DLF_DISABLED | DLF_FAKE | DLF_AMBIENT | DLF_DEFERRED_CUBEMAPS)

class CLightVolumesMgr : public Cry3DEngineBase
{
public:
	CLightVolumesMgr()
	{
		Init();
	}

	void   Init();
	void   Reset();
	uint16 RegisterVolume(const Vec3& vPos, f32 fRadius, uint8 nClipVolumeRef, const SRenderingPassInfo& passInfo);
	void   RegisterLight(const SRenderLight& pDL, uint32 nLightID, const SRenderingPassInfo& passInfo);
	void   Update(const SRenderingPassInfo& passInfo);
	void   Clear(const SRenderingPassInfo& passInfo);
	void   GetLightVolumes(threadID nThreadID, SLightVolume*& pLightVols, uint32& nNumVols);
	void   DrawDebug(const SRenderingPassInfo& passInfo);

private:
	struct SLightVolInfo
	{
		SLightVolInfo() : vVolume(ZERO, 0.0f), nNextVolume(0), nClipVolumeID(0)
		{
		};
		SLightVolInfo(const Vec3& pPos, float fRad, uint8 clipVolumeID)
			: vVolume(pPos, fRad), nNextVolume(0), nClipVolumeID(clipVolumeID)
		{
		};

		Vec4   vVolume;       // xyz: position, w: radius
		uint16 nNextVolume;   // index of next light volume for this hash bucket (0 if none)
		uint8  nClipVolumeID; // clip volume stencil ref
	};

	struct SLightCell
	{
		SLightCell() : nLightCount(0)
		{
		};

		uint16 nLightID[LV_CELL_MAX_LIGHTS];
		uint8  nLightCount;
	};

	inline int32 GetIntHash(const int32 k, const int32 nBucketSize = LV_WORLD_BUCKET_SIZE) const
	{
		static const uint32 nHashBits = 9;
		static const uint32 nGoldenRatio32bits = 2654435761u; // (2^32) / (golden ratio)
		return (k * nGoldenRatio32bits) >> (32 - nHashBits);  // ref: knuths integer multiplicative hash function
	}

	inline uint16 GetWorldHashBucketKey(const int32 x, const int32 y, const int32 z, const int32 nBucketSize = LV_WORLD_BUCKET_SIZE) const
	{
		const uint32 nPrimeNum = 101;//0xd8163841;
		return (((GetIntHash(x) + nPrimeNum) * nPrimeNum + GetIntHash(y)) * nPrimeNum + GetIntHash(z)) & (nBucketSize - 1);
	}

	void AddLight(const SRenderLight& pLight, const SLightVolInfo* __restrict pVolInfo, SLightVolume& pVolume);

	typedef DynArray<SLightVolume> LightVolumeVector;

private:
	LightVolumeVector                           m_pLightVolumes[RT_COMMAND_BUF_COUNT]; // Final light volume list. <todo> move light list creation to renderer to avoid double-buffering this
	CThreadSafeRendererContainer<SLightVolInfo> m_pLightVolsInfo;                      // World cells data
	SLightCell m_pWorldLightCells[LV_LIGHTS_WORLD_BUCKET_SIZE];                        // 2D World cell buckets for light sources ids
	uint16     m_nWorldCells[LV_WORLD_BUCKET_SIZE];                                    // World cell buckets for light volumes
	bool       m_bUpdateLightVolumes : 1;
};
