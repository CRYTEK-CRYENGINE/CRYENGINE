// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ConvertContext.h"
#include <Cry3DEngine/CGF/CryHeaders.h>  // MAX_STATOBJ_LODS_NUM

class CContentCGF;
struct CMaterialCGF;
class CMesh;
class CPhysicsInterface;

class CStaticObjectCompiler
{
public:
	CStaticObjectCompiler(CPhysicsInterface* pPhysicsInterface, bool bConsole, int logVerbosityLevel=0);
	~CStaticObjectCompiler();

	void SetSplitLods(bool bSplit);
	void SetUseMikkTB(bool bUseMikkTB);
	void SetUseOrbisVCO(bool bUseOrbisVCO);

	CContentCGF* MakeCompiledCGF(CContentCGF* pCGF, bool bSeparatedPhysics, bool bIgnoreTangentspaceErrors);

	static int GetSubMeshCount(const CContentCGF* pCGFLod0);
	static int GetJointCount(const CContentCGF* pCGF);

private:
	bool ProcessCompiledCGF(CContentCGF* pCGF);

	void AnalyzeSharedMeshes(CContentCGF* pCGF);
	bool CompileMeshes(CContentCGF* pCGF, bool bIgnoreTangentspaceErrors);

	bool SplitLODs(CContentCGF* pCGF);
	CContentCGF* MakeLOD(int nLodNum, const CContentCGF* pCGF);

	bool Physicalize(CContentCGF* pCompiledCGF, CContentCGF* pSrcCGF, bool bSeparatedPhysics);
	void CompileDeformablePhysData(CContentCGF* pCGF);
	void AnalyzeFoliage(CContentCGF* pCGF, struct CNodeCGF* pNodeCGF);
	void PrepareSkinData(CNodeCGF* pNode, const Matrix34& mtxSkelToMesh, CNodeCGF* pNodeSkel, float r, bool bSwapEndian);

	bool ValidateBoundingBoxes(CContentCGF* pCGF);
	void ValidateBreakableJoints(const CContentCGF* pCGF);

	bool MakeMergedCGF(CContentCGF* pCompiledCGF, CContentCGF* pCGF);

private:
	CPhysicsInterface* m_pPhysicsInterface;
	bool m_bSplitLODs;
	bool m_bOwnLod0;
	bool m_bConsole;
	bool m_bUseOrbisVCO;
	int m_logVerbosityLevel;

public:
	CContentCGF* m_pLODs[MAX_STATOBJ_LODS_NUM];
};
