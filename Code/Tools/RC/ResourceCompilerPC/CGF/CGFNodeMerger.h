// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CContentCGF;
class CMesh;
struct CMaterialCGF;
struct CNodeCGF;

namespace CGFNodeMerger
{
	bool SetupMeshSubsets(const CContentCGF& contentCgf, CMesh& mesh, CMaterialCGF* pMaterialCGF, string& errorMessage);
	bool MergeNodes(const CContentCGF* pCGF, const std::vector<CNodeCGF*>& nodes, string& errorMessage, CMesh* pOutMesh);
};
