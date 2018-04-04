// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __INCLUDE_CRY3DENGINE_CLIPVOLUMEMANAGER_H
#define __INCLUDE_CRY3DENGINE_CLIPVOLUMEMANAGER_H

class CClipVolume;

class CClipVolumeManager : public Cry3DEngineBase
{
	struct SClipVolumeInfo
	{
		CClipVolume* m_pVolume;
		bool         m_bActive;

		SClipVolumeInfo()
			: m_pVolume(nullptr)
			, m_bActive(false) {}

		SClipVolumeInfo(CClipVolume* pVolume)
			: m_pVolume(pVolume)
			, m_bActive(false) {}

		bool operator==(const SClipVolumeInfo& other) const { return m_pVolume == other.m_pVolume; }
	};

public:
	static const uint8 InactiveVolumeStencilRef = 0xFD;
	static const uint8 AffectsEverythingStencilRef = 0xFE;

	virtual ~CClipVolumeManager();

	virtual IClipVolume* CreateClipVolume();
	virtual bool         DeleteClipVolume(IClipVolume* pClipVolume);
	virtual bool         UpdateClipVolume(IClipVolume* pClipVolume, _smart_ptr<IRenderMesh> pRenderMesh, IBSPTree3D* pBspTree, const Matrix34& worldTM, bool bActive, uint32 flags, const char* szName);

	void                 PrepareVolumesForRendering(const SRenderingPassInfo& passInfo);

	void                 UpdateEntityClipVolume(const Vec3& pos, IRenderNode* pRenderNode);
	void                 UnregisterRenderNode(IRenderNode* pRenderNode);

	bool                 IsClipVolumeRequired(IRenderNode* pRenderNode) const;
	CClipVolume*         GetClipVolumeByPos(const Vec3& pos, const IClipVolume* pIgnoreVolume = NULL) const;

	void GetMemoryUsage(class ICrySizer* pSizer) const;
	size_t GetClipVolumeCount() const { return m_ClipVolumes.size(); }

private:
	PodArray<SClipVolumeInfo> m_ClipVolumes;
};

#endif //__INCLUDE_CRY3DENGINE_CLIPVOLUMEMANAGER_H
