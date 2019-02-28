// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ModelMesh.h" //embedded
#include "AttachmentVClothPreProcess.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////
// This class contain data which can be shared between different several SKIN-models of same type.
// It loads and contains all geometry and materials.
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkin : public ISkin
{
public:
	explicit CSkin(const string& strFileName, uint32 nLoadingFlags);
	virtual ~CSkin();
	void AddRef()
	{
		++m_nRefCounter;
	}
	void Release()
	{
		--m_nRefCounter;
		if (m_nKeepInMemory)
			return;
		if (m_nRefCounter == 0)
			delete this;
	}
	void DeleteIfNotReferenced()
	{
		if (m_nRefCounter <= 0)
			delete this;
	}
	int GetRefCounter() const
	{
		return m_nRefCounter;
	}

	virtual void         SetKeepInMemory(bool nKiM) { m_nKeepInMemory = nKiM; }
	uint32               GetKeepInMemory() const    { return m_nKeepInMemory; }

	virtual void         PrecacheMesh(bool bFullUpdate, int nRoundId, int nLod);

	virtual const char*  GetModelFilePath() const { return m_strFilePath.c_str(); }
	virtual uint32       GetNumLODs() const       { return m_arrModelMeshes.size(); }
	virtual IRenderMesh* GetIRenderMesh(uint32 nLOD) const;
	virtual uint32       GetTextureMemoryUsage2(ICrySizer* pSizer = 0) const;
	virtual uint32       GetMeshMemoryUsage(ICrySizer* pSizer = 0) const;
	virtual Vec3         GetRenderMeshOffset(uint32 nLOD) const
	{
		uint32 numMeshes = m_arrModelMeshes.size();
		if (nLOD >= numMeshes)
			return Vec3(ZERO);
		return m_arrModelMeshes[nLOD].m_vRenderMeshOffset;
	};

	int         SelectNearestLoadedLOD(int nLod);

	CModelMesh* GetModelMesh(uint32 nLOD)
	{
		return (nLOD < m_arrModelMeshes.size()) ? &m_arrModelMeshes[nLOD] : nullptr;
	}

	const CModelMesh* GetModelMesh(uint32 nLOD) const
	{
		return (nLOD < m_arrModelMeshes.size()) ? &m_arrModelMeshes[nLOD] : nullptr;
	}

	// this is the array that's returned from the RenderMesh
	TRenderChunkArray* GetRenderMeshMaterials()
	{
		if (m_arrModelMeshes[0].m_pIRenderMesh)
			return &m_arrModelMeshes[0].m_pIRenderMesh->GetChunks();
		else
			return NULL;
	}
	void       GetMemoryUsage(ICrySizer* pSizer) const;
	uint32     SizeOfModelData() const;

	IMaterial* GetIMaterial(uint32 nLOD) const
	{
		uint32 numModelMeshes = m_arrModelMeshes.size();
		if (nLOD >= numModelMeshes)
			return 0;
		return m_arrModelMeshes[nLOD].m_pIDefaultMaterial;
	}

	bool                         LoadNewSKIN(const char* szFilePath, uint32 nLoadingFlags);

	virtual const IVertexFrames* GetVertexFrames() const { return &m_arrModelMeshes[0].m_softwareMesh.GetVertexFrames(); }

	SAttachmentVClothPreProcessData const& GetVClothData() const { return m_VClothData; }
	bool HasVCloth() const { return (m_VClothData.m_listBendTrianglePairs.size()>0) || (m_VClothData.m_listBendTriangles.size()>0) || (m_VClothData.m_nndc.size()>0) || (m_VClothData.m_nndcNotAttachedOrderedIdx.size()>0); }
	void SetNeedsComputeSkinningBuffers() { m_needsComputeSkinningBuffers = true;  }
	bool NeedsComputeSkinningBuffers() const { return m_needsComputeSkinningBuffers; }
public:
	// this struct contains the minimal information to attach a SKIN to a base-SKEL
	struct SJointInfo
	{
		int32  m_idxParent;         //index of parent-joint. if the idx==-1 then this joint is the root. Usually this values are > 0
		uint32 m_nJointCRC32Lower;  //hash value for searching joint-names
		int32  m_nExtraJointID;     //offset for extra joints added programmatically
		QuatT  m_DefaultAbsolute;
		string m_NameModelSkin;     // the name of joint
		void   GetMemoryUsage(ICrySizer* pSizer) const {};
	};

	DynArray<SJointInfo> m_arrModelJoints;
	DynArray<CModelMesh> m_arrModelMeshes;
	uint32               m_nLoadingFlags;

private:
	int    m_nKeepInMemory;
	int    m_nRefCounter, m_nInstanceCounter;
	string m_strFilePath;

	bool m_needsComputeSkinningBuffers;
	SAttachmentVClothPreProcessData m_VClothData;
};
