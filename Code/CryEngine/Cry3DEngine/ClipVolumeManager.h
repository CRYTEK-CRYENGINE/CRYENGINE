// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INCLUDE_CRY3DENGINE_CLIPVOLUMEMANAGER_H
#define __INCLUDE_CRY3DENGINE_CLIPVOLUMEMANAGER_H

class CClipVolume;

class CClipVolumeManager : public Cry3DEngineBase
{
	struct SClipVolumeInfo
	{
		CClipVolume* m_pVolume         = nullptr;
		int          m_updateFrameId   = 0;
		float        m_currentViewDist = std::numeric_limits<float>::max();
		bool         m_bActive         = false;

		SClipVolumeInfo(CClipVolume* pVolume) : m_pVolume(pVolume) {}
		bool operator==(const SClipVolumeInfo& other) const { return m_pVolume == other.m_pVolume; }
	};

public:
	static const uint8 InactiveVolumeStencilRef    = 0xFE;
	static const uint8 AffectsEverythingStencilRef = 0xFF;

	virtual ~CClipVolumeManager();

	virtual IClipVolume* CreateClipVolume();
	virtual bool         DeleteClipVolume(IClipVolume* pClipVolume);
	virtual bool         UpdateClipVolume(IClipVolume* pClipVolume, _smart_ptr<IRenderMesh> pRenderMesh, IBSPTree3D* pBspTree, const Matrix34& worldTM, uint8 viewDistRatio, bool bActive, uint32 flags, const char* szName);
	void                 TrimDeletedClipVolumes(int trimFrameId = std::numeric_limits<int>::max());

	void                 PrepareVolumesForRendering(const SRenderingPassInfo& passInfo);

	void                 UpdateEntityClipVolume(const Vec3& pos, IRenderNode* pRenderNode);

	bool                 IsClipVolumeRequired(IRenderNode* pRenderNode) const;
	CClipVolume*         GetClipVolumeByPos(const Vec3& pos, const IClipVolume* pIgnoreVolume = NULL) const;

	void GetMemoryUsage(class ICrySizer* pSizer) const;
	size_t GetClipVolumeCount() const { return m_clipVolumes.size(); }

private:
	std::vector<SClipVolumeInfo> m_clipVolumes;
	std::vector<SClipVolumeInfo> m_deletedClipVolumes;

};

#endif //__INCLUDE_CRY3DENGINE_CLIPVOLUMEMANAGER_H
