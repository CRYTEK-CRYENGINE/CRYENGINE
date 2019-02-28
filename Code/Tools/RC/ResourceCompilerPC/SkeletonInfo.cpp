// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "SkeletonInfo.h"
#include "../CryEngine/Cry3DEngine/CGF/CGFLoader.h"
#include "CGF/CGFSaver.h"
#include "CGA/SkeletonHelpers.h"


namespace
{
struct RCLoaderCGFListener : ILoaderCGFListener
{
	virtual void Warning( const char *format )
	{
		RCLogWarning(format);
	}

	virtual void Error( const char *format )
	{
		RCLogError(format);
	}
};
}


CSkeletonInfo::CSkeletonInfo()
{
}


CSkeletonInfo::~CSkeletonInfo()
{
}


bool CSkeletonInfo::LoadFromChr(const char * name)
{
	RCLoaderCGFListener listener;
	CLoaderCGF cgfLoader;

	std::auto_ptr<CContentCGF> pCGF;
	CChunkFile chunkFile;
	pCGF.reset(cgfLoader.LoadCGF( name,chunkFile,&listener ));
	if (!pCGF.get())
	{
		RCLogError( "%s: Failed to load geometry file %s - %s",__FUNCTION__,name,cgfLoader.GetLastError() );
		return false;
	}

	m_SkinningInfo = *pCGF->GetSkinningInfo();
	m_SkinningInfo.m_arrMorphTargets.clear();

	return true;
}


bool CSkeletonInfo::LoadFromCga(const char * name)
{
	RCLoaderCGFListener listener;
	CLoaderCGF cgfLoader;

	std::auto_ptr<CContentCGF> pCGF;
	CChunkFile chunkFile;
	pCGF.reset(cgfLoader.LoadCGF( name,chunkFile,&listener ));
	if (!pCGF.get())
	{
		RCLog( "%s: Failed to load geometry file %s - %s",__FUNCTION__,name,cgfLoader.GetLastError() );
		return false;
	}

	const int nodeCount = pCGF->GetNodeCount();

	m_SkinningInfo.m_arrBonesDesc.resize(nodeCount);

	for (int i = 0; i < nodeCount; ++i)
	{
		const CNodeCGF* const pNode = pCGF->GetNode(i);
		const CNodeCGF* const pParentNode = pNode->pParent;

		// Get parent's index
		// TODO: following code is not fast, maybe a better method exists to find the parent
		int p;
		for (p = 0; p < i; ++p)
		{
			if (pParentNode == pCGF->GetNode(p))
			{
				break;
			}
		}
		if (p >= i)
		{
			if (pParentNode == 0)
			{
				if (i != 0)
				{
					RCLog( "%s: Failed to load geometry file %s - multiple root nodes", __FUNCTION__, name);
					m_SkinningInfo.m_arrBonesDesc.clear();
					return false;
				}
			}
			else
			{
				RCLog( "%s: Failed to load geometry file %s - a forward-referencing parent", __FUNCTION__, name);
				m_SkinningInfo.m_arrBonesDesc.clear();
				return false;
			}
		}

		CryBoneDescData& cb = m_SkinningInfo.m_arrBonesDesc[i];

		cry_strcpy(cb.m_arrBoneName, pNode->name);
		cb.m_nControllerID = SkeletonHelpers::ComputeControllerId(cb.m_arrBoneName);

		cb.m_nOffsetParent = p - i;
		// TODO: should we store proper value here? (Animation Compressor doesn't need it)
		cb.m_numChildren = -1;
		// TODO: should we store proper value here? (Animation Compressor doesn't need it)
		cb.m_nOffsetChildren = 0;

		cb.m_DefaultB2W = pNode->worldTM;
		cb.m_DefaultW2B = pNode->worldTM.GetInverted();
	}

	m_SkinningInfo.m_arrMorphTargets.clear();

	return true;
}
