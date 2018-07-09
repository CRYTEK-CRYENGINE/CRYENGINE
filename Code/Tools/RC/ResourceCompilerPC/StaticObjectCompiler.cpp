// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "StaticObjectCompiler.h"
#include "StatCGFPhysicalize.h"
#include "../../CryEngine/Cry3DEngine/MeshCompiler/MeshCompiler.h"
#include "CGF/CGFNodeMerger.h"
#include "StringHelpers.h"
#include "Util.h"
#include <vco.h>

static const char* const s_consolesLod0MarkerStr = "consoles_lod0";

static bool NodeHasProperty(const CNodeCGF* pNode, const char* property)
{
	return strstr(StringHelpers::MakeLowerCase(pNode->properties).c_str(), property) != 0;
}

static bool HasNodeWithConsolesLod0(const CContentCGF* pCGF)
{
	const string lodNamePrefix(CGF_NODE_NAME_LOD_PREFIX);
	const int nodeCount = pCGF->GetNodeCount();
	for (int i = 0; i < nodeCount; ++i)
	{
		const CNodeCGF* const pNode = pCGF->GetNode(i);
		if (!pNode)
		{
			continue;
		}

		if (!StringHelpers::StartsWithIgnoreCase(string(pNode->name), lodNamePrefix))
		{
			continue;
		}

		if (NodeHasProperty(pNode, s_consolesLod0MarkerStr))
		{
			return true;
		}
	}
	return false;
}

static CNodeCGF* FindNodeByName(CContentCGF* pCGF, const char* name)
{
	if (name == 0)
	{
		assert(name);
		return 0;
	}
	const int nodeCount = pCGF->GetNodeCount();
	for (int i = 0; i < nodeCount; ++i)
	{
		CNodeCGF* const pNode = pCGF->GetNode(i);
		if (pNode && strcmp(pNode->name, name) == 0)
		{
			return pNode;
		}
	}
	return 0;
}

static void ReportDuplicatedMeshNodeNames(const CContentCGF* pCGF)
{
	assert(pCGF);

	const string ignoreNamePrefix("$");

	std::set<const char*, stl::less_stricmp<const char*> > names;

	const int nodeCount = pCGF->GetNodeCount();

	for (int i = 0; i < nodeCount; ++i)
	{
		const CNodeCGF* const pNode = pCGF->GetNode(i);
		if (pNode == 0)
		{
			assert(pNode);
			continue;
		}

		const char* const pName = &pNode->name[0];

		if (pName[0] == 0)
		{
			RCLogWarning("Node with empty name found in %s", pCGF->GetFilename());
			continue;
		}

		if (StringHelpers::StartsWithIgnoreCase(string(pName), ignoreNamePrefix))
		{
			continue;
		}

		if (pNode->pMesh == 0)
		{
			continue;
		}

		if (names.find(pName) == names.end())
		{
			names.insert(pName);
		}
		else
		{
			RCLogWarning(
				"Duplicated mesh node name %s found in %s. Please make sure that all mesh nodes have unique names.",
				pName, pCGF->GetFilename());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CStaticObjectCompiler::CStaticObjectCompiler(CPhysicsInterface *pPhysicsInterface, bool bConsole, int logVerbosityLevel)
	: m_pPhysicsInterface(pPhysicsInterface)
	, m_bConsole(bConsole)
	, m_bUseOrbisVCO(false)
	, m_logVerbosityLevel(logVerbosityLevel)
{
	m_bSplitLODs = false;
	m_bOwnLod0 = false;

	for (int i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
	{
		m_pLODs[i] = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
CStaticObjectCompiler::~CStaticObjectCompiler()
{
	for (int i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
	{
		if (i != 0 || m_bOwnLod0)
		{
			delete m_pLODs[i];
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStaticObjectCompiler::SetSplitLods(bool bSplit)
{
	m_bSplitLODs = bSplit;
}

//////////////////////////////////////////////////////////////////////////
void CStaticObjectCompiler::SetUseOrbisVCO(bool bUseOrbisVCO)
{
	m_bUseOrbisVCO = bUseOrbisVCO;
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* CStaticObjectCompiler::MakeCompiledCGF(CContentCGF *pCGF, bool bSeparatedPhysics, bool bIgnoreTangentspaceErrors)
{
	if (pCGF->GetExportInfo()->bCompiledCGF)
	{
		const bool ok = ProcessCompiledCGF(pCGF);
		if (!ok)
		{
			return 0;
		}
		return pCGF;
	}

	if (m_logVerbosityLevel > 2)
	{
		RCLog("Compiling CGF");
	}

	m_bOwnLod0 = true;
	MakeLOD(0, pCGF);

	CContentCGF* const pCompiledCGF = m_pLODs[0];

	// Setup mesh subsets for the original CGF.
	for (int i = 0, num = pCGF->GetNodeCount(); i < num; ++i)
	{
		CNodeCGF* const pNode = pCGF->GetNode(i);
		if (pNode->pMesh)
		{
			string errorMessage;
			if (!CGFNodeMerger::SetupMeshSubsets(*pCGF, *pNode->pMesh, pNode->pMaterial, errorMessage))
			{
				RCLogError("%s: %s", __FUNCTION__, errorMessage.c_str());
				return 0;
			}
		}
	}

	if (pCGF->GetExportInfo()->bMergeAllNodes)
	{
		if (m_logVerbosityLevel > 2)
		{
			RCLog("Merging nodes");
		}
		if (!MakeMergedCGF(pCompiledCGF, pCGF))
		{
			return 0;
		}
	}
	else
	{
		for (int i = 0, num = pCGF->GetNodeCount(); i < num; ++i)
		{
			CNodeCGF* const pNodeCGF = pCGF->GetNode(i);
			pCompiledCGF->AddNode(pNodeCGF);
		}
	}

	// Compile meshes in all nodes.
	{
		if (m_logVerbosityLevel > 2)
		{
			RCLog("Compiling meshes");
		}
		if (!CompileMeshes(pCompiledCGF, bIgnoreTangentspaceErrors))
		{
			return 0;
		}
	}

	// Try to find shared meshes.
	{
		if (m_logVerbosityLevel > 2)
		{
			RCLog("Searching for shared meshes");
		}
		AnalyzeSharedMeshes(pCompiledCGF);
	}

	{
		if (m_logVerbosityLevel > 2)
		{
			RCLog("Physicalizing");
		}
		const bool ok = Physicalize(pCompiledCGF, pCGF, bSeparatedPhysics);
		if (!ok)
		{
			return 0;
		}

		if (m_logVerbosityLevel > 2)
		{
			RCLog("Compiling deformable physics data");
		}
		CompileDeformablePhysData(pCompiledCGF);
	}

	if (!ValidateBoundingBoxes(pCompiledCGF))
	{
		return 0;
	}

	// Try to split LODs
	if (m_bSplitLODs)
	{
		if (m_logVerbosityLevel > 2)
		{
			RCLog("Splitting to LODs");
		}
		const bool ok = SplitLODs(pCompiledCGF);
		if (!ok)
		{
			return 0;
		}
	}

	{
		if (m_logVerbosityLevel > 2)
		{
			RCLog("Validating breakable joints");
		}
		ValidateBreakableJoints(pCGF);
	}

	return pCompiledCGF;
}

//////////////////////////////////////////////////////////////////////////
void CStaticObjectCompiler::ValidateBreakableJoints(const CContentCGF* pCGF)
{
	// Warn in case we have too many sub-meshes.

	const int subMeshCount = GetSubMeshCount(m_pLODs[0]);
	const int jointCount = GetJointCount(pCGF);

	enum { kBreakableSubMeshLimit = 64 };

	if (jointCount > 0 && subMeshCount > kBreakableSubMeshLimit)
	{
		RCLogError("Breakable CGF contains %d sub-meshes (%d is the maximum): %s", 
			subMeshCount, (int)kBreakableSubMeshLimit, pCGF->GetFilename());
	}
}

//////////////////////////////////////////////////////////////////////////
void CStaticObjectCompiler::CompileDeformablePhysData(CContentCGF *pCompiledCGF)
{
	for (int i = 0; i < pCompiledCGF->GetNodeCount(); ++i)
	{
		if (pCompiledCGF->GetNode(i)->type == CNodeCGF::NODE_MESH)
		{
			AnalyzeFoliage(pCompiledCGF, pCompiledCGF->GetNode(i));
			break;
		}
	}

	const string skeletonPrefix = "skeleton_";

	for (int i = 0; i < pCompiledCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* pSkeletonNode = pCompiledCGF->GetNode(i);
		if (StringHelpers::StartsWithIgnoreCase(pSkeletonNode->name, skeletonPrefix))
		{
			const char* meshName = pSkeletonNode->name + skeletonPrefix.size();
			CNodeCGF* pMeshNode = FindNodeByName(pCompiledCGF, meshName);
			bool bKeepSkeleton = true; // always keep for PC
			
			if (!pMeshNode)
			{
				RCLogError("Unable to find mesh node \"%s\" for \"%s\"", meshName, pSkeletonNode->name);
				continue;
			}
			
			if (pMeshNode->pMesh == 0)
			{
				RCLogError("Node %s: Has corresponding skeleton, but no mesh. Disabling deformation.", pMeshNode->name);
				bKeepSkeleton = false;
			}
			
			if (m_bConsole)
			{
				if (NodeHasProperty(pSkeletonNode, "consoles_deformable")) // enabled for consoles only by UDP
				{
					if (HasNodeWithConsolesLod0(pCompiledCGF))
					{
						RCLogWarning("Node %s: %s and consoles_deformable may not be used together. Disabling object deformation.",
												 pMeshNode->name, s_consolesLod0MarkerStr);
						bKeepSkeleton = false;
					}
				}
				else
				{
					bKeepSkeleton = false;
				}
			}

			if (bKeepSkeleton)
			{
				float r = 0.0f;
				if (const char *ptr = strstr(pSkeletonNode->properties, "skin_dist"))
				{
					for(;*ptr && (*ptr<'0' || *ptr>'9');ptr++);
					if (*ptr)
						r = (float)atof(ptr);
				}
				PrepareSkinData(pMeshNode, pMeshNode->localTM.GetInverted()*pSkeletonNode->localTM, pSkeletonNode, r, false);
			}
			else
			{
				SAFE_DELETE_ARRAY(pMeshNode->pSkinInfo);
				pCompiledCGF->RemoveNode(pSkeletonNode);
				--i;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CStaticObjectCompiler::CompileMeshes( CContentCGF *pCGF, bool bIgnoreTangentspaceErrors )
{
	// Compile Meshes in all nodes.
	for (int i=0; i<pCGF->GetNodeCount(); i++)
	{
		CNodeCGF *pNodeCGF = pCGF->GetNode(i);

		if (pNodeCGF->pMesh)
		{
			if (m_logVerbosityLevel > 2)
			{
				RCLog("Compiling geometry in node '%s'", pNodeCGF->name);
			}

			mesh_compiler::CMeshCompiler meshCompiler;

			int nMeshCompileFlags = mesh_compiler::MESH_COMPILE_TANGENTS | mesh_compiler::MESH_COMPILE_VALIDATE;
			if (pCGF->GetExportInfo()->bUseCustomNormals)
			{
				nMeshCompileFlags |= mesh_compiler::MESH_COMPILE_USE_CUSTOM_NORMALS;
			}
			if (!pNodeCGF->bPhysicsProxy)
			{
				nMeshCompileFlags |= mesh_compiler::MESH_COMPILE_OPTIMIZE;
			}
			if (bIgnoreTangentspaceErrors)
			{
				nMeshCompileFlags |= mesh_compiler::MESH_COMPILE_IGNORE_TANGENT_SPACE_ERRORS;
			}

			if (!meshCompiler.Compile( *pNodeCGF->pMesh, nMeshCompileFlags))
			{
				RCLogError("Failed to compile geometry in node '%s' in file %s - %s",pNodeCGF->name,pCGF->GetFilename(),meshCompiler.GetLastError());
				return false;
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CStaticObjectCompiler::AnalyzeSharedMeshes( CContentCGF *pCGF )
{
	// Check if any duplicate meshes exist, and try to share them.
	mesh_compiler::CMeshCompiler meshCompiler;
	uint32 numNodes = pCGF->GetNodeCount();
	for (int i = 0; i < numNodes-1; i++)
	{
		CNodeCGF *pNode1 = pCGF->GetNode(i);
		if (!pNode1->pMesh || pNode1->pSharedMesh)
			continue;

		if (!(pNode1->pMesh->GetVertexCount() && pNode1->pMesh->GetFaceCount()))
			continue;

		if (pNode1->bPhysicsProxy)
			continue;

		for (int j = i+1; j < numNodes; j++)
		{
			CNodeCGF *pNode2 = pCGF->GetNode(j);
			if (pNode1 == pNode2 || !pNode2->pMesh)
				continue;

			if (pNode2->bPhysicsProxy)
				continue;

			if (pNode1->properties == pNode2->properties)
			{
				if (pNode1->pMesh != pNode2->pMesh && meshCompiler.CompareMeshes(*pNode1->pMesh, *pNode2->pMesh))
				{
					// Meshes are same, share them.
					delete pNode2->pMesh;
					pNode2->pMesh = pNode1->pMesh;
					pNode2->pSharedMesh = pNode1;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CStaticObjectCompiler::ValidateBoundingBoxes(CContentCGF* pCGF)
{
	static const float maxValidObjectRadius = 10000000000.0f;
	bool ok = true;
	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		const CNodeCGF* const pNode = pCGF->GetNode(i);
		if (pNode->type == CNodeCGF::NODE_MESH && pNode->pMesh && pNode->pMesh->GetVertexCount() == 0 && pNode->pMesh->GetIndexCount() == 0)
		{
			const float radius = pNode->pMesh->m_bbox.GetRadius();
			if (radius <= 0 || radius > maxValidObjectRadius || !_finite(radius))
			{
				RCLogWarning(
					"Node '%s' in file %s has an invalid bounding box, the engine will fail to load this object. Check that the node has valid geometry and is not empty.",
					pNode->name, pCGF->GetFilename());
				ok = false;
			}
		}
	}
	return ok;
}

//////////////////////////////////////////////////////////////////////////
bool CStaticObjectCompiler::Physicalize(CContentCGF* pCompiledCGF, CContentCGF* pSrcCGF, bool bSeparatedPhysics)
{
	if (!m_pPhysicsInterface->GetGeomManager())
	{
		return false;
	}

	for (int i = 0; i < pCompiledCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const pNode = pCompiledCGF->GetNode(i);
		if (!pNode->pMesh)
		{
			continue;
		}
		if (pNode->bPhysicsProxy || !bSeparatedPhysics)
		{
			if (m_pPhysicsInterface->Physicalize(pNode, pCompiledCGF) == CPhysicsInterface::ePR_Fail)
			{
				return false;
			}
		}
	}

	const bool bCga = StringHelpers::EqualsIgnoreCase(PathUtil::GetExt(pCompiledCGF->GetFilename()), "cga");

	for (int i = 0; i < pCompiledCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const pNode = pCompiledCGF->GetNode(i);
		if (!pNode->pMesh)
		{
			continue;
		}
		if (!pNode->bPhysicsProxy && !bSeparatedPhysics)
		{
			if (!m_pPhysicsInterface->DeletePhysicalProxySubsets(pNode, bCga))
			{
				RCLogError( "Failed to physicalize geometry in file %s", pSrcCGF->GetFilename());
				return false;
			}
		}
	}

	m_pPhysicsInterface->ProcessBreakablePhysics(pCompiledCGF, pSrcCGF);
	return true;
}

//////////////////////////////////////////////////////////////////////////

inline int check_mask(unsigned int *pMask, int i) 
{
	return pMask[i>>5]>>(i&31) & 1;
}
inline void set_mask(unsigned int *pMask, int i) 
{
	pMask[i>>5] |= 1u<<(i&31);
}
inline void clear_mask(unsigned int *pMask, int i) 
{
	pMask[i>>5] &= ~(1u<<(i&31));
}


int UpdatePtTriDist(const Vec3 *vtx, const Vec3 &n, const Vec3 &pt, float &minDist,float &minDenom)
{
	float rvtx[3]={ (vtx[0]-pt).len2(),(vtx[1]-pt).len2(),(vtx[2]-pt).len2() }, elen2[2],dist,denom;
	int i=idxmin3(rvtx), bInside[2];
	Vec3 edge[2],dp=pt-vtx[i];

	edge[0] = vtx[incm3(i)]-vtx[i]; elen2[0] = edge[0].len2();
	edge[1] = vtx[decm3(i)]-vtx[i]; elen2[1] = edge[1].len2();
	bInside[0] = isneg((dp^edge[0])*n);
	bInside[1] = isneg((edge[1]^dp)*n);
	rvtx[i] = rvtx[i]*elen2[bInside[0]] - sqr(Util::getMax(0.0f,dp*edge[bInside[0]]))*(bInside[0]|bInside[1]); 
	denom = elen2[bInside[0]];

	if (bInside[0]&bInside[1]) {
		if (edge[0]*edge[1]<0) {
			edge[0] = vtx[decm3(i)]-vtx[incm3(i)];
			dp = pt-vtx[incm3(i)];
			if ((dp^edge[0])*n>0)	{
				dist=rvtx[incm3(i)]*edge[0].len2()-sqr(dp*edge[0]); denom=edge[0].len2();
				goto found;
			}
		}
		dist=sqr((pt-vtx[0])*n), denom=n.len2();
	}	else
		dist = rvtx[i];

	found:
	if (dist*minDenom < minDist*denom) {
		minDist=dist; minDenom=denom;
		return 1;
	}
	return 0;
}


struct SBranchPt 
{
	Vec3 pt;
	Vec2 tex;
	int itri;
	float minDist,minDenom;
};

struct SBranch 
{
	SBranchPt *pt;
	int npt;
	float len;
};


void CStaticObjectCompiler::AnalyzeFoliage(CContentCGF *pCGF, CNodeCGF *pNodeCGF)
{
	if (!pCGF || !pNodeCGF)
	{
		return;
	}
	if (!pNodeCGF->pMesh)
	{
		if (pNodeCGF->pSharedMesh != 0)
		{
			RCLogError("AnalyzeFoilage: node with null mesh passed. A CGF with stripped mesh?");
		}
		return;
	}
	if (!m_pPhysicsInterface->GetGeomManager())
	{
		return;
	}

	int i,j;

	SFoliageInfoCGF& fi = *pCGF->GetFoliageInfo();

	if (fi.nSpines)
	{
		for(i=1;i<fi.nSpines;i++) // spines 1..n-1 use the same buffer, so make sure they don't delete it
		{
			fi.pSpines[i].pVtx=0;
			fi.pSpines[i].pSegDim=0;
		}
		SAFE_DELETE_ARRAY(fi.pSpines); 
		SAFE_DELETE_ARRAY(fi.pBoneMapping);
		fi.nSpines = 0; 
	}
	fi.chunkBoneIds.clear();

	const CMesh& mesh = *pNodeCGF->pMesh;

	assert(mesh.m_pPositionsF16 == 0);

	const int nMeshVtx = mesh.GetVertexCount();
	const Vec3* const pMeshVtx = mesh.m_pPositions;

	const int nMeshIdx = mesh.GetIndexCount();
	const vtx_idx* const pMeshIdx = mesh.m_pIndices; 

	const SMeshTexCoord* const pMeshTex = mesh.m_pTexCoord;

	char sname[128];
	char* pname;
	{
		if (!cry_sprintf(sname, "%s_", pNodeCGF->name))
		{
			RCLogWarning("String buffer overflow in AnalyzeFoilage. Node name is too long.");
			return;
		}

		pname = sname + strlen(sname);

		if (!cry_strcpy(pname, sizeof(sname) - (pname - sname), "branch1_1"))
		{
			RCLogWarning("String buffer overflow in AnalyzeFoilage. Node name is too long.");
			return;
		}

		for (i=pCGF->GetNodeCount()-1; i>=0 && stricmp((const char*)pCGF->GetNode(i)->name,sname); i--); 

		if (i < 0)
		{
			pname = sname;
		}
	}

	// iterate through branch points branch#_#, build a branch structure
	SBranch* pBranches = 0;
	int nBranches=0;
	for (;;)
	{
		SBranch branch;
		branch.pt=0;
		branch.npt=0;
		branch.len=0;

		for (;;)
		{
			if (!cry_sprintf(pname, sizeof(sname) - (pname - sname), "branch%d_%d", nBranches+1, branch.npt+1))
			{
				RCLogWarning("String buffer overflow in AnalyzeFoilage. Node name is too long.");
				if(pBranches)
				{
					for(int i=0; i<nBranches; ++i)
					{
						free(pBranches[i].pt);
					}
					free(pBranches);
				}
				return;
			}
			for(i=pCGF->GetNodeCount()-1; i>=0 && stricmp((const char*)pCGF->GetNode(i)->name,sname); i--);

			if (i<0)
			{
				break;
			}

			const Vec3 pt(pCGF->GetNode(i)->localTM.m03, pCGF->GetNode(i)->localTM.m13, pCGF->GetNode(i)->localTM.m23);

			if ((branch.npt & 15)==0)
			{
				branch.pt = static_cast<SBranchPt*>(realloc(branch.pt, (branch.npt+16)*sizeof(SBranchPt)));
			}

			branch.pt[branch.npt].minDist = 1;
			branch.pt[branch.npt].minDenom = 0;
			branch.pt[branch.npt].itri = -1;
			branch.pt[branch.npt++].pt = pt;
			branch.len += (pt-branch.pt[Util::getMax(0,branch.npt-2)].pt).len();
		}

		if (branch.npt<2)
		{
			if (branch.pt)
			{
				free(branch.pt);
			}
			break;
		}

		if ((nBranches & 15)==0)
		{
			void* const p = realloc(pBranches,(nBranches+16)*sizeof(SBranch));
			if (!p)
			{
				RCLogError("AnalyzeFoilage: failed to reallocate memory");
				return;
			}
			pBranches = static_cast<SBranch*>(p);
		}

		pBranches[nBranches++] = branch;

		// make sure the segments have equal length
		const float len = branch.len / (branch.npt-1);
		for(i=1;i<branch.npt;i++)
		{
			branch.pt[i].pt = branch.pt[i-1].pt + (branch.pt[i].pt - branch.pt[i-1].pt).normalized() * len;
		}
	}

	if (nBranches==0)
	{
		return;
	}

	SSpineRC spine;
	memset(&spine, 0, sizeof spine);

	fi.pBoneMapping = new SMeshBoneMapping_uint8[fi.nSkinnedVtx=nMeshVtx];
	memset(fi.pBoneMapping, 0, nMeshVtx*sizeof(fi.pBoneMapping[0]));
	for(i=0; i<nMeshVtx; i++)
	{
		fi.pBoneMapping[i].weights[0] = 255;
	}

	int* const pIdxSorted = new int[nMeshVtx];

	int* const pForeignIdx = new int[nMeshIdx/3];

	uint* const pUsedTris = new uint[i = ((nMeshIdx-1)>>5)+1];
	memset(pUsedTris,0,i*sizeof(int));

	uint* const pUsedVtx = new uint[i = ((nMeshVtx-1)>>5)+1];
	memset(pUsedVtx,0,i*sizeof(int));

	// iterate through all triangles, track the closest for each branch point
	for(int iss=0; iss<mesh.GetSubSetCount(); iss++) 
	{
		if (mesh.m_subsets[iss].nPhysicalizeType == PHYS_GEOM_TYPE_NONE)
		{
			for(i=mesh.m_subsets[iss].nFirstIndexId; i<mesh.m_subsets[iss].nFirstIndexId+mesh.m_subsets[iss].nNumIndices; i+=3)
			{
				Vec3 pt[3];
				for(j=0;j<3;j++)
				{
					pt[j] = pMeshVtx[pMeshIdx[i+j]];
				}
				const Vec3 n = pt[1]-pt[0] ^ pt[2]-pt[0];
				for(int ibran=0; ibran<nBranches; ibran++)
				{
					for(int ivtx0=0; ivtx0<pBranches[ibran].npt; ivtx0++)
					{
						if (UpdatePtTriDist(pt,n, pBranches[ibran].pt[ivtx0].pt, pBranches[ibran].pt[ivtx0].minDist, pBranches[ibran].pt[ivtx0].minDenom))
						{
							pBranches[ibran].pt[ivtx0].itri = i;
						}
					}
				}
			}
		}
	}


	for(int ibran=0;ibran<nBranches;ibran++)
	{
		for(i=0;i<pBranches[ibran].npt;i++)
		{
			if(pBranches[ibran].pt[i].itri<0)
			{
				continue;
			}

			// get the closest point on the triangle for each vertex, get tex coords from it

			Vec3 pt[4];
			for(j=0;j<3;j++) 
			{
				pt[j] = pMeshVtx[pMeshIdx[pBranches[ibran].pt[i].itri+j]];
			}
			pt[3] = pBranches[ibran].pt[i].pt;
			const Vec3 n = pt[1]-pt[0] ^ pt[2]-pt[0];

			for(j=0;j<3 && (pt[incm3(j)]-pt[j] ^ pt[3]-pt[j])*n>0;j++);

			Vec2 uv, uvA, uvB;
			float s = 0;
			float t = 0;

			if (j==3)	
			{	// the closest pt is in triangle interior
				const float denom = 1.0f/n.len2(); 
				pt[3] -= n*(n*(pt[3]-pt[0]))*denom;
				for(j=0; j<3; j++)
				{
					const float k = ((pt[3]-pt[incm3(j)] ^ pt[3]-pt[decm3(j)])*n)*denom;
					pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri+j]].GetUV(uv);

					s += uv.x*k;
					t += uv.y*k;
				}
			}
			else
			{
				for(j=0;j<3 && !inrange((pt[incm3(j)]-pt[j])*(pt[3]-pt[j]), 0.0f,(pt[incm3(j)]-pt[j]).len2());j++);

				if (j<3) 
				{	// the closest pt in on an edge
					const float k = ((pt[3]-pt[j])*(pt[incm3(j)]-pt[j]))/(pt[incm3(j)]-pt[j]).len2();
					pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri+      j ]].GetUV(uvA);
					pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri+incm3(j)]].GetUV(uvB);

					s = uvA.x * (1 - k) + uvB.x * k;
					t = uvA.y * (1 - k) + uvB.y * k;
				}
				else 
				{	// the closest pt is a vertex
					float dist[3];
					for(j=0;j<3;j++)
					{
						dist[j] = (pt[j]-pt[3]).len2();
					}

					j = idxmin3(dist);
					pMeshTex[pMeshIdx[pBranches[ibran].pt[i].itri+j]].GetUV(uv);

					s = uv.x;
					t = uv.y;
				}
			}

			pBranches[ibran].pt[i].tex.set(s,t);
		}
	}

	int nVtx = 0;
	int nBones;

	for(int iss=0,nGlobalBones=1; iss<mesh.GetSubSetCount(); iss++,nGlobalBones+=nBones-1) 
	{
		if (mesh.m_subsets[iss].nPhysicalizeType != PHYS_GEOM_TYPE_NONE)
		{
			nBones = 1;
			continue;
		}

		// fill the foreign idx list
		for(i=0,j=mesh.m_subsets[iss].nFirstIndexId/3; j*3<mesh.m_subsets[iss].nFirstIndexId+mesh.m_subsets[iss].nNumIndices; j++,i++)
		{
			pForeignIdx[i] = j;
		}

		IGeometry* const pPhysGeom = m_pPhysicsInterface->GetGeomManager()->CreateMesh(
			pMeshVtx, const_cast<vtx_idx*>(pMeshIdx)+mesh.m_subsets[iss].nFirstIndexId, 
			0,pForeignIdx, mesh.m_subsets[iss].nNumIndices/3,
			mesh_shared_vtx|mesh_SingleBB|mesh_keep_vtxmap);

		const mesh_data* const pmd = (const mesh_data*)pPhysGeom->GetData();
		primitives::box bbox;
		pPhysGeom->GetBBox(&bbox);
		const float e = sqr((bbox.size.x+bbox.size.y+bbox.size.z)*0.0002f);
		nBones = 1;
		fi.chunkBoneIds.push_back(0);

		for(int isle=0; isle<pmd->nIslands; isle++)
		{
			if (pmd->pIslands[isle].nTris<4)
			{
				continue;
			}
			
			spine.nVtx = 0;
			spine.pVtx = 0;
			spine.iAttachSpine = -1;
			spine.iAttachSeg = -1;

			{
				int ivtx = 0;
				int ibran0 = 0;
				int ibran1 = nBranches;
			
				for (;;)
				{
					int itri;
					int ibran;
					Vec2 tex[4];
					for(itri=pmd->pIslands[isle].itri,i=j=0; i<pmd->pIslands[isle].nTris; itri=pmd->pTri2Island[itri].inext,i++)
					{
						for(j=0;j<3;j++)
						{
							pMeshTex[pMeshIdx[pmd->pForeignIdx[itri]*3+j]].GetUV(tex[j]);
						}

						const float k = tex[1]-tex[0] ^ tex[2]-tex[0];
						const float s = fabs_tpl(k);

						for(ibran=ibran0,j=0; ibran<ibran1; ibran++)
						{
							for(j=0; j<3 && sqr_signed(tex[incm3(j)]-tex[j] ^ pBranches[ibran].pt[ivtx].tex-tex[j])*k>-e*s*(tex[incm3(j)]-tex[j]).GetLength2(); j++);

							if (j==3)
							{
								goto foundtri;
							}
						}
					}
					break; // if no tri has the next point in texture space, skip the rest
foundtri:
					static const int nMaxBones = 72;
					if (nBones+pBranches[ibran].npt-1 > nMaxBones)
					{
						break;
					}

					// output phys vtx
					if (!spine.pVtx)
					{
						spine.pVtx = new Vec3[pBranches[ibran].npt];
					}
					const float denom = 1.0f/(tex[1]-tex[0] ^ tex[2]-tex[0]);
					tex[3] = pBranches[ibran].pt[ivtx].tex;
					Vec3 pt(ZERO);
					for(j=0; j<3; j++)
					{
						pt += pmd->pVertices[pmd->pIndices[itri*3+j]].scale((tex[3]-tex[incm3(j)]^tex[3]-tex[decm3(j)]))*denom;
					}
					spine.pVtx[ivtx++] = pt;

					ibran0 = ibran;
					ibran1 = ibran+1;

					if (ivtx>=pBranches[ibran].npt)
					{
						break;
					}
				}

				if (ivtx<3)
				{
					if (spine.pVtx)
					{
						delete[] spine.pVtx;
					}
					continue;
				}

				spine.nVtx = ivtx;
				spine.pSegDim = new Vec4[spine.nVtx];
				memset(spine.pSegDim, 0, sizeof(Vec4) * spine.nVtx);
			}

			// enforce equal length for phys rope segs
			{
				float len = 0;
				for(i=0; i<spine.nVtx-1; i++)
				{
					len += (spine.pVtx[i+1]-spine.pVtx[i]).len();
				}
				spine.len = len;
				len /= (spine.nVtx-1);
				for(i=1; i<spine.nVtx; i++)
				{
					spine.pVtx[i] = spine.pVtx[i-1]+(spine.pVtx[i]-spine.pVtx[i-1]).normalized()*len;
				}
			}

			// append island's vertices to the vertex index list
			int ivtx0=nVtx;
			{
				Vec3 n(ZERO);
				for(int itri=pmd->pIslands[isle].itri,i=0; i<pmd->pIslands[isle].nTris; itri=pmd->pTri2Island[itri].inext,i++) 
				{
					for(int iedge=0;iedge<3;iedge++)
					{
						if (!check_mask(pUsedVtx, pMeshIdx[pmd->pForeignIdx[itri]*3+iedge]))
						{
							set_mask(pUsedVtx, pIdxSorted[nVtx++]=pMeshIdx[pmd->pForeignIdx[itri]*3+iedge]);
						}
					}
					const Vec3 facenorm =
						pMeshVtx[pMeshIdx[pmd->pForeignIdx[itri]*3+1]].sub(pMeshVtx[pMeshIdx[pmd->pForeignIdx[itri]*3]]) ^
						pMeshVtx[pMeshIdx[pmd->pForeignIdx[itri]*3+2]].sub(pMeshVtx[pMeshIdx[pmd->pForeignIdx[itri]*3]]);
					n += facenorm*float(sgnnz(facenorm.z));
				}
				spine.navg = n.normalized();
			}

			Vec3 nprev = spine.pVtx[1]-spine.pVtx[0];
			for(int ivtx=0; ivtx<spine.nVtx-1; ivtx++) 
			{	// generate a separating plane between current seg and next seg	and move vertices that are sliced by it to the front
				const Vec3 edge = spine.pVtx[ivtx+1]-spine.pVtx[ivtx];
				Vec3 n = edge;
				const Vec3 axisx = (n^spine.navg).normalized();
				const Vec3 axisy = axisx^n;
				if (ivtx<spine.nVtx-2)
				{
					n += spine.pVtx[ivtx+2]-spine.pVtx[ivtx+1]; 
				}
				const float denom = edge * n;
				const float denomPrev = edge * nprev;
				if (fabsf(denom) < 1e-6f || fabsf(denomPrev) < 1e-6f)
				{
					RCLogWarning("Potential divide by zero detected and skipped in file: %s", pCGF->GetFilename());
					continue;
				}
				const float rcpDenom = 1.0f / denom;
				const float rcpDenomPrev = 1.0f / denomPrev;
				for(i=ivtx0;i<nVtx;i++)
				{
					float t1 = spine.pVtx[ivtx+1].sub(pMeshVtx[pIdxSorted[i]]) * n;

					if (t1 <= 0)
					{
						continue;
					}

					float t;

					t = (pMeshVtx[pIdxSorted[i]].sub(spine.pVtx[ivtx]))*axisx;
					spine.pSegDim[ivtx].x=Util::getMin(t,spine.pSegDim[ivtx].x);
					spine.pSegDim[ivtx].y=Util::getMax(t,spine.pSegDim[ivtx].y);

					t = (pMeshVtx[pIdxSorted[i]].sub(spine.pVtx[ivtx]))*axisy;
					spine.pSegDim[ivtx].z=Util::getMin(t,spine.pSegDim[ivtx].z);
					spine.pSegDim[ivtx].w=Util::getMax(t,spine.pSegDim[ivtx].w);

					const float t0 = (spine.pVtx[ivtx].sub(pMeshVtx[pIdxSorted[i]])*nprev)*rcpDenomPrev;
					t1 *= rcpDenom;

					t = Util::getClamped(1.0f-fabs_tpl(t0+t1)*0.5f/(t1-t0), 0.0f, 1.0f);
					j = isneg(t0+t1);

					fi.pBoneMapping[pIdxSorted[i]].boneIds[0] = nBones+Util::getMin(spine.nVtx-2,ivtx+j);
					fi.pBoneMapping[pIdxSorted[i]].boneIds[1] = nBones+Util::getMax(0,ivtx-1+j);
					const uint8 weight = Util::getClamped(FtoI(t * 255), 1, 254);
					fi.pBoneMapping[pIdxSorted[i]].weights[j] = weight;
					fi.pBoneMapping[pIdxSorted[i]].weights[j^1] = 255 - weight;

					j=pIdxSorted[ivtx0];
					pIdxSorted[ivtx0++]=pIdxSorted[i];
					pIdxSorted[i]=j;					
				}
				nprev = n;
				fi.chunkBoneIds.push_back(nGlobalBones+nBones-1+ivtx);
			}
			for(i=ivtx0;i<nVtx;i++)
			{
				fi.pBoneMapping[pIdxSorted[i]].boneIds[0] = nBones+spine.nVtx-2;
				fi.pBoneMapping[pIdxSorted[i]].weights[0] = 255;
			}

			if ((fi.nSpines & 7)==0)
			{
				SSpineRC* const pSpines = fi.pSpines;
				fi.pSpines = new SSpineRC[fi.nSpines+8];
				memcpy(fi.pSpines, pSpines, fi.nSpines*sizeof(SSpineRC));
				for(j=0;j<fi.nSpines;j++)
				{
					pSpines[j].pVtx=0;
					pSpines[j].pSegDim=0;
				}
				if (pSpines)
				{
					delete[] pSpines;
				}
			}
			fi.pSpines[fi.nSpines++] = spine;
			nBones += spine.nVtx-1;			
		} // for: isle

		pPhysGeom->Release();
	} // for: iss

	// for every spine, check if its base is close enough to a point on another spine 
	for(int ispine=0; ispine<fi.nSpines; ispine++)
	{
		const Vec3 pt = fi.pSpines[ispine].pVtx[0];
		for(int ispineDst=0; ispineDst<fi.nSpines; ispineDst++)
		{
			if (ispine==ispineDst)
			{
				continue;
			}

			float mindist = sqr(fi.pSpines[ispine].len*0.05f); 
			for(i=0,j=-1; i<fi.pSpines[ispineDst].nVtx-1; i++) 
			{	// find the point on this seg closest to pt
				float t;
				if ((t=(fi.pSpines[ispineDst].pVtx[i+1]-pt).len2())<mindist)
				{
					// end vertex
					mindist=t;
					j=i;
				}
				const Vec3 edge = fi.pSpines[ispineDst].pVtx[i+1]-fi.pSpines[ispineDst].pVtx[i];
				const Vec3 edge1 = pt-fi.pSpines[ispineDst].pVtx[i];
				if (inrange(edge1*edge, edge.len2()*-0.3f*((i-1)>>31),edge.len2()) && (t=(edge^edge1).len2()) < mindist*edge.len2())
				{
					// point on edge
					mindist=t/edge.len2();
					j=i;
				}
			}
			if (j>=0)
			{	// attach ispine to a point on iSpineDst
				fi.pSpines[ispine].iAttachSpine = ispineDst;
				fi.pSpines[ispine].iAttachSeg = j;
				break;
			}
		}
	}			

	delete[] pUsedVtx;
	delete[] pUsedTris;
	delete[] pForeignIdx;
	delete[] pIdxSorted;

	spine.pVtx = 0;
	spine.pSegDim=0;
}

//////////////////////////////////////////////////////////////////////////
void CStaticObjectCompiler::PrepareSkinData(CNodeCGF *pNode, const Matrix34 &mtxSkelToMesh, CNodeCGF *pNodeSkel, float r, bool bSwapEndian)
{
	assert(pNode->pMesh->m_pPositionsF16 == 0);

	int i,j,nVtx;
	Vec3 vtx[4];
	geom_world_data gwd[2];
	geom_contact *pcontact;
	Vec3 *pVtx = pNode->pMesh->m_pPositions;
	Matrix34 mtxMeshToSkel = mtxSkelToMesh.GetInverted();
	IGeomManager *pGeoman = m_pPhysicsInterface->GetGeomManager();
	if (!pGeoman)
	{
		return;
	}
	CMemStream stm(&pNodeSkel->physicalGeomData[0][0], pNodeSkel->physicalGeomData[0].size(), bSwapEndian);
	phys_geometry *pSkelGeom = pNodeSkel->pMesh ? pGeoman->LoadPhysGeometry(stm, pNodeSkel->pMesh->m_pPositions,pNodeSkel->pMesh->m_pIndices,0) : 
														 pGeoman->LoadPhysGeometry(stm,0,0,0);
	IGeometry *pSphere,*pSphereBig,*pPhysSkel=pSkelGeom->pGeom;

	gwd[1].scale = mtxSkelToMesh.GetColumn0().len();
	gwd[1].offset = mtxSkelToMesh.GetTranslation();
	gwd[1].R = Matrix33(mtxSkelToMesh)*(1.0f/gwd[1].scale);
	vtx[0] = pNode->pMesh->m_bbox.GetSize();
	primitives::sphere sph; sph.center.zero(); sph.r=r>0.0f ? r:Util::getMin(vtx[0].x,vtx[0].y,vtx[0].z);
	pSphere = pGeoman->CreatePrimitive(primitives::sphere::type, &sph);
	sph.r*=3.0f; pSphereBig = pGeoman->CreatePrimitive(primitives::sphere::type, &sph);
	mesh_data *md = (mesh_data*)pPhysSkel->GetData();
	CrySkinVtx *pSkinInfo = new CrySkinVtx[(nVtx=pNode->pMesh->GetVertexCount())+1];
	memset(pSkinInfo, 0, (nVtx+1)*sizeof(CrySkinVtx));
	pSkinInfo[nVtx].w[0] = r;
	pNode->pSkinInfo = pSkinInfo;

	for(i=0; i<nVtx; i++) 
	{
    Vec3 v = pVtx[i];
		gwd[0].offset = v;
		if (pSphere->Intersect(pPhysSkel, gwd,gwd+1, 0, pcontact) ||
				pSphereBig->Intersect(pPhysSkel, gwd,gwd+1, 0, pcontact))
		{
			for(j=0;j<3;j++) 
				vtx[j] = mtxSkelToMesh*md->pVertices[pSkinInfo[i].idx[j] = md->pIndices[pcontact->iPrim[1]*3+j]];
			Vec3 n = vtx[1]-vtx[0] ^ vtx[2]-vtx[0];
			float rnlen = 1.0f/n.len();	n *= rnlen;
			vtx[3] = v-n*(pSkinInfo[i].w[3]=n*(v-vtx[0]));
			Vec3 edge = (vtx[1]+vtx[2]-vtx[0]*2).normalized();
			pSkinInfo[i].M = Matrix33(edge,n^edge,n).T();
			for(j=0,n*=rnlen; j<3; j++)
				pSkinInfo[i].w[j] = (vtx[inc_mod3[j]]-vtx[3]^vtx[dec_mod3[j]]-vtx[3])*n;
		}	else
			pSkinInfo[i].idx[0] = -1;	
	}	
	pGeoman->UnregisterGeometry(pSkelGeom);
	pSphereBig->Release(); pSphere->Release();
}

//////////////////////////////////////////////////////////////////////////
bool CStaticObjectCompiler::ProcessCompiledCGF(CContentCGF* pCGF)
{
	assert(pCGF->GetExportInfo()->bCompiledCGF);

	if (!m_pPhysicsInterface->GetGeomManager())
	{
		return false;
	}

	// CGF is already compiled, so we just need to perform some validation and re-compiling steps.

	ReportDuplicatedMeshNodeNames(pCGF);

	m_pLODs[0] = pCGF;
	m_bOwnLod0 = false;

	const int nodeCount = pCGF->GetNodeCount();
	for (int i = 0; i < nodeCount; ++i) 
	{
		CNodeCGF* const pNode = pCGF->GetNode(i);
		m_pPhysicsInterface->RephysicalizeNode(pNode, pCGF);

		if (m_bUseOrbisVCO && pNode->pMesh && sizeof(vtx_idx)==sizeof(uint32_t))
		{
			uint32_t* const pIndices = (uint32_t*)pNode->pMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES);
			for (int j = 0; j < pNode->pMesh->GetSubSetCount(); ++j)
			{
				auto& subSet = pNode->pMesh->m_subsets[j];
				OptimizeIndexBufferForOrbis(pIndices + subSet.nFirstIndexId, subSet.nNumIndices);
			}
		}
	}

	CompileDeformablePhysData(pCGF);

	// Try to split LODs
	if (m_bSplitLODs)
	{
		const bool ok = SplitLODs(pCGF);
		if (!ok)
		{
			return false;
		}
	}

	ValidateBreakableJoints(pCGF);

	return true;
}

//////////////////////////////////////////////////////////////////////////
namespace LodHelpers
{

static bool NodeHasChildren(
	const CContentCGF* const pCGF,
	const CNodeCGF* const pNode)
{
	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		const CNodeCGF* const p = pCGF->GetNode(i);
		if (p->pParent == pNode)
		{
			return true;
		}
	}
	return false;
}

static int GetLodIndex(const char* const pName, const string& lodNamePrefix)
{
	if (!StringHelpers::StartsWithIgnoreCase(string(pName), lodNamePrefix))
	{
		return -1;
	}

	int value = 0;
	const char* p = pName + lodNamePrefix.length();
	while ((p[0] >= '0') && (p[0] <= '9'))
	{
		value = value * 10 + (p[0] - '0');
		++p;
	}

	return value;
}

static bool ValidateLodNodes(CContentCGF* const pCGF, const string& lodNamePrefix)
{
	static const char* const howToFix = " Please modify and re-export source asset file.";

	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		const CNodeCGF* const pNode = pCGF->GetNode(i);

		const string nodeName = pNode->name;
		if (!StringHelpers::StartsWithIgnoreCase(nodeName, lodNamePrefix))
		{
			continue;
		}

		if (nodeName.length() == lodNamePrefix.length())
		{
			RCLogError(
				"LOD node '%s' has no index. Valid name format is '%sNxxx', where N is LOD index 1-%i and xxx is any text. File: %s.%s",
				nodeName.c_str(), lodNamePrefix.c_str(), (int)MAX_STATOBJ_LODS_NUM - 1, pCGF->GetFilename(), howToFix);
			return false;			
		}

		const int lodIndex = GetLodIndex(nodeName.c_str(), lodNamePrefix);
		if ((lodIndex <= 0) || (lodIndex >= MAX_STATOBJ_LODS_NUM))
		{
			RCLogError(
				"LOD node '%s' has bad or missing LOD index. Valid LOD name format is '%sNxxx', where N is LOD index 1-%i and xxx is any text. File: %s.%s",
				nodeName.c_str(), lodNamePrefix.c_str(), (int)MAX_STATOBJ_LODS_NUM - 1, pCGF->GetFilename(), howToFix);
			return false;			
		}

		if (pNode->pParent == NULL)
		{
			RCLogError(
				"LOD node '%s' has no parent node. File: %s.%s",
				nodeName.c_str(), pCGF->GetFilename(), howToFix);
			return false;			
		}

		if ((pNode->pParent->type != CNodeCGF::NODE_MESH) && (pNode->pParent->type != CNodeCGF::NODE_HELPER))
		{
			RCLogError(
				"LOD0 node '%s' (parent node of LOD node '%s') is neither MESH nor HELPER. File: %s.%s",
				pNode->pParent->name, nodeName.c_str(), pCGF->GetFilename(), howToFix);
			return false;			
		}

		if (pNode->pParent->pMesh == 0)
		{
			RCLogError(
				"LOD0 node '%s' (parent node of LOD node '%s') has no mesh data. File: %s.%s",
				pNode->pParent->name, nodeName.c_str(), pCGF->GetFilename(), howToFix);
			return false;			
		}

		if ((pNode->type != CNodeCGF::NODE_MESH) && (pNode->type != CNodeCGF::NODE_HELPER))
		{
			RCLogError(
				"LOD node '%s' is neither MESH nor HELPER. File %s.%s",
				nodeName.c_str(), pCGF->GetFilename(), howToFix);
			return false;			
		}

		if (pNode->pMesh == 0)
		{
			RCLogError(
				"LOD node '%s' has no mesh data. File: %s.%s",
				nodeName.c_str(), pCGF->GetFilename(), howToFix);
			return false;			
		}

		if (NodeHasChildren(pCGF, pNode))
		{
			RCLogError(
				"LOD node '%s' has children. File: %s.%s",
				nodeName.c_str(), pCGF->GetFilename(), howToFix);
			return false;			
		}
	}

	return true;
}

static void FindLodNodes(
	std::vector<CNodeCGF*>& resultLodNodes,
	CContentCGF* const pCGF,
	bool bReturnSingleNode,
	const CNodeCGF* const pParent,
	const string& lodNamePrefix)
{
	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const pNode = pCGF->GetNode(i);

		if (pParent && (pNode->pParent != pParent))
		{
			continue;
		}

		if (StringHelpers::StartsWithIgnoreCase(string(pNode->name), lodNamePrefix))
		{
			resultLodNodes.push_back(pNode);
			if (bReturnSingleNode)
			{
				// Caller asked for *single* arbitrary LOD node 
				break;
			}
		}
	}
}

static bool ValidateMeshSharing(const CContentCGF* pCGF)
{
	assert(pCGF);

	typedef std::set<CMesh*> MeshSet;
	MeshSet meshes;

	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		const CNodeCGF* const pNode = pCGF->GetNode(i);
		assert(pNode);

		if (pNode->pMesh == 0)
		{
			if (pNode->pSharedMesh)
			{
				RCLogError(
					"Data integrity check failed on %s: node refers a shared node, but pointer to shared mesh is NULL. Contact an RC programmer.",
					pCGF->GetFilename());
				return false;			
			}
		}
		else if (pNode->pSharedMesh == 0)
		{
			const MeshSet::const_iterator it = meshes.find(pNode->pMesh);
			if (it == meshes.end())
			{
				meshes.insert(pNode->pMesh);
			}
			else
			{
				RCLogError(
					"Data integrity check failed on %s: a mesh referenced from few nodes without using sharing. Contact an RC programmer.",
					pCGF->GetFilename());
				return false;			
			}
		}
		else
		{
			if (pNode == pNode->pSharedMesh)
			{
				RCLogError(
					"Data integrity check failed on %s: a node refers itself. Contact an RC programmer.",
					pCGF->GetFilename());
				return false;			
			}
			if (pNode->pSharedMesh->pSharedMesh)
			{
				RCLogError(
					"Data integrity check failed on %s: a chain of shared nodes found. Contact an RC programmer.",
					pCGF->GetFilename());
				return false;			
			}
			if (!pNode->pSharedMesh->pMesh)
			{
				RCLogError(
					"Data integrity check failed on %s: mesh in shared node is NULL. Contact an RC programmer.",
					pCGF->GetFilename());
				return false;			
			}
			if (pNode->pSharedMesh->pMesh != pNode->pMesh)
			{
				RCLogError(
					"Data integrity check failed on %s: pointer to shared mesh does not point to mesh in shared node. Contact an RC programmer.",
					pCGF->GetFilename());
				return false;			
			}
		}
	}

	return true;
}

static bool CopyMeshData(CContentCGF* pCGF, CNodeCGF* pDstNode, CNodeCGF* pSrcNode)
{
	assert(pDstNode != pSrcNode);

	if (!ValidateMeshSharing(pCGF))
	{
		return false;
	}

	if (!pSrcNode->pMesh)
	{
		RCLogError(
			"Unexpected empty LOD mesh in %s. Contact an RC programmer.",
			pCGF->GetFilename());
		return false;			
	}

	if (!pDstNode->pMesh)
	{
		RCLogError(
			"Unexpected empty LOD 0 mesh in %s. Contact an RC programmer.",
			pCGF->GetFilename());
		return false;			
	}

	if (pSrcNode->pSharedMesh == pDstNode)
	{
		return true;
	}

	// Make destination node "meshless"
	if (pDstNode->pSharedMesh == 0)
	{
		// Find new owner for destination node's mesh.
		CNodeCGF* newOwnerOfDstMesh = 0;
		for (int i = 0; i < pCGF->GetNodeCount(); ++i)
		{
			CNodeCGF* const pNode = pCGF->GetNode(i);
			if (pNode->pSharedMesh == pDstNode)
			{
				if (newOwnerOfDstMesh == 0)
				{
					newOwnerOfDstMesh = pNode;
					pNode->pSharedMesh = 0;
				}
				else
				{
					pNode->pSharedMesh = newOwnerOfDstMesh;
				}
			}
		}
		if (newOwnerOfDstMesh == 0)
		{
			delete pDstNode->pMesh;
		}
	}
	pDstNode->pMesh = 0;
	pDstNode->pSharedMesh = 0;

	// Everyone who referred source node, now should refer destination node
	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const pNode = pCGF->GetNode(i);
		if (pNode->pSharedMesh == pSrcNode)
		{
			pNode->pSharedMesh = pDstNode;
		}
	}

	// Transfer mesh data from source to destination node
	pDstNode->pMesh = pSrcNode->pMesh;
	pDstNode->pSharedMesh = pSrcNode->pSharedMesh ?	pSrcNode->pSharedMesh : 0;
	pSrcNode->pSharedMesh = pDstNode;

	if (!ValidateMeshSharing(pCGF))
	{
		return false;
	}

	return true;
}

static bool DeleteNode(CContentCGF* pCGF, CNodeCGF* pDeleteNode)
{
	assert(pDeleteNode);

	if (!ValidateMeshSharing(pCGF))
	{
		return false;
	}

	pCGF->RemoveNode(pDeleteNode);

	if (!ValidateMeshSharing(pCGF))
	{
		return false;
	}

	return true;
}

} // namespace LodHelpers
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
bool CStaticObjectCompiler::SplitLODs(CContentCGF* pCGF)
{
	const string lodNamePrefix(CGF_NODE_NAME_LOD_PREFIX);

	if (!LodHelpers::ValidateMeshSharing(pCGF) || !LodHelpers::ValidateLodNodes(pCGF, lodNamePrefix))
	{
		return false;
	}

	// Check that meshes are not damaged
	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		const CNodeCGF* const pNode = pCGF->GetNode(i);
		if (!pNode)
		{
			RCLogError(
				"Unexpected NULL node in %s. Contact an RC programmer.",
				pCGF->GetFilename());
			return false;
		}
		const CMesh* const pMesh = pNode->pMesh;
		if (!pMesh)
		{
			continue;
		}
		const char* pError = "";
		if (!pMesh->Validate(&pError))
		{
			RCLogError(
				"Mesh in node '%s' is damaged: %s. File %s.",
				pNode->name, pError, pCGF->GetFilename());
			return false;
		}
	}

	std::vector<CNodeCGF*> lodNodes;

	LodHelpers::FindLodNodes(lodNodes, pCGF, true, NULL, lodNamePrefix);
	if (lodNodes.empty())
	{
		// We don't have any LOD nodes. Done.
		return true;
	}

	//
	// Collect all nodes which can potentially have LODs.
	//

	struct LodableNodeInfo
	{
		CNodeCGF* pNode;
		size_t maxLodFound;
		size_t lod0Index;
		LodableNodeInfo(CNodeCGF* a_pNode, size_t a_maxLodFound, size_t a_lod0Index)
			: pNode(a_pNode)
			, maxLodFound(a_maxLodFound)
			, lod0Index(a_lod0Index)
		{
			assert(a_pNode);
		}
	};
	std::vector<LodableNodeInfo> lodableNodes;	

	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		CNodeCGF* const pNode = pCGF->GetNode(i);

		// Skip nodes which cannot have LODs
		{
			if (pNode->pMesh == 0)
			{
				continue;
			}

			if ((pNode->type != CNodeCGF::NODE_MESH) && (pNode->type != CNodeCGF::NODE_HELPER))
			{
				continue;
			}

			const string nodeName = pNode->name;
			if (StringHelpers::StartsWithIgnoreCase(nodeName, lodNamePrefix))
			{
				continue;
			}
		}

		//
		// Get and analyze children LOD nodes, if any
		//

		LodableNodeInfo lodsInfo(pNode, 0, 0);

		lodNodes.clear();
		LodHelpers::FindLodNodes(lodNodes, pCGF, false, pNode, lodNamePrefix);
		if (lodNodes.empty())
		{
			lodableNodes.push_back(lodsInfo);
			continue;
		}

		CNodeCGF* arrLodNodes[MAX_STATOBJ_LODS_NUM] = { 0 };

		for (size_t i = 0; i < lodNodes.size(); ++i)
		{
			CNodeCGF* const pLodNode = lodNodes[i];
			assert(pLodNode);
			assert(pNode == pLodNode->pParent);
			const int lodIndex = LodHelpers::GetLodIndex(pLodNode->name, lodNamePrefix);
			assert((lodIndex > 0) && (lodIndex < MAX_STATOBJ_LODS_NUM));
			if (arrLodNodes[lodIndex])
			{
				RCLogError(
					"More than one node of LOD %i ('%s', '%s') attached to same parent node '%s' in file %s. Please modify and re-export source asset file.",
					lodIndex, arrLodNodes[lodIndex]->name, pLodNode->name, pNode->name, pCGF->GetFilename());
				return false;			
			}
			arrLodNodes[lodIndex] = pLodNode;

			if (lodIndex > lodsInfo.maxLodFound)
			{
				lodsInfo.maxLodFound = lodIndex;
			}
		}
		assert(arrLodNodes[0] == 0);

		//
		// Check LOD sequence for validity
		//
		{
			arrLodNodes[0] = pNode;

			// Check that we don't have gaps in the LOD sequence
			{
				int gapStart = -1;
				for (int lodIndex = 1; lodIndex < MAX_STATOBJ_LODS_NUM; ++lodIndex)
				{
					if ((gapStart < 0) && !arrLodNodes[lodIndex] && arrLodNodes[lodIndex - 1])
					{
						gapStart = lodIndex;
					}

					if ((gapStart >= 0) && arrLodNodes[lodIndex])
					{
						RCLogError(
							"Missing LOD node%s between '%s' and '%s' in file %s. Please modify and re-export source asset file.",
							((lodIndex - gapStart > 1) ? "s" : ""), arrLodNodes[gapStart - 1]->name, arrLodNodes[lodIndex]->name, pCGF->GetFilename());
						return false;								
					}
				}
			}

			// Check that geometry simplification of the LODs is good
			for (int lodIndex = 1; lodIndex < MAX_STATOBJ_LODS_NUM; ++lodIndex)
			{
				const CNodeCGF* const pLod0 = arrLodNodes[lodIndex - 1];
				const CNodeCGF* const pLod1 = arrLodNodes[lodIndex];

				if (pLod1 == 0)
				{
					break;
				}

				const int subsetCount0 = pLod0->pMesh->GetSubSetCount();
				const int subsetCount1 = pLod1->pMesh->GetSubSetCount();

				if (subsetCount1 < subsetCount0)
				{
					// Number of draw calls decreased. The LOD is good.
					continue;
				}

				if (subsetCount1 > subsetCount0)
				{
					RCLogWarning(
						"LOD node '%s' has more submaterials used than node '%s' (%d vs %d) in file %s. Please modify and re-export source asset file.",
						pLod1->name, pLod0->name, subsetCount1, subsetCount0, pCGF->GetFilename());
					continue;								
				}

				// Number of draw calls is same. Let's check that number of faces is small enough comparing to previous LOD.

				const int faceCount0 = pLod0->pMesh->GetIndexCount() / 3;
				const int faceCount1 = pLod1->pMesh->GetIndexCount() / 3;

				static const float faceCountRatio = 1.5f;
				const int maxFaceCount1 = int(faceCount0 / faceCountRatio);

				if (faceCount1 > maxFaceCount1)
				{
					RCLogWarning(
						"LOD node '%s' should have less than %2.0f%% of the faces of its parent. It has %d faces, its parent has %d. It should have less than %d.",
						pLod1->name, 100.0f/faceCountRatio, faceCount1, faceCount0, maxFaceCount1);
					continue;								
				}
			}

			arrLodNodes[0] = 0;
		}

		//
		// For consoles user can mark a LOD as being LOD0. Let's handle it.
		//
		if (m_bConsole)
		{

			int newLod0 = -1;
			for (int lodIndex = 0; lodIndex < MAX_STATOBJ_LODS_NUM; ++lodIndex)
			{
				CNodeCGF* const pLodNode = arrLodNodes[lodIndex];
				if (pLodNode && NodeHasProperty(pLodNode, s_consolesLod0MarkerStr))
				{
					newLod0 = lodIndex;
					break;
				}
			}

			if (newLod0 >= 0)
			{
				// Breakable objects expect rendering and physics geometry matching each other,
				// so we cannot change geometry (LOD0's by consoles_lod0's)

				const string filename = StringHelpers::MakeLowerCase(PathUtil::GetFile(pCGF->GetFilename()));
				if (strstr(filename.c_str(), "break"))
				{
					RCLogWarning(
						"Ignoring property '%s' in node '%s' because the mesh is Breakable. File %s.",
						s_consolesLod0MarkerStr, arrLodNodes[newLod0]->name, pCGF->GetFilename());
					newLod0 = -1;
				}
			}

			if (newLod0 >= 0)
			{
				RCLog(
					"Found property '%s' in node '%s' of file %s. This node becomes LOD 0.",
					s_consolesLod0MarkerStr, arrLodNodes[newLod0]->name, pCGF->GetFilename());
				lodsInfo.lod0Index = newLod0;
			}
		}

		lodableNodes.push_back(lodsInfo);
	}

	assert(!lodableNodes.empty());

	//
	// Process all nodes which can potentially have LODs.
	//

	size_t maxFinalLod = 0;
	for (size_t lodableIndex = 0; lodableIndex < lodableNodes.size(); ++lodableIndex)
	{
		const LodableNodeInfo& info = lodableNodes[lodableIndex];
		const size_t finalLod = info.maxLodFound - info.lod0Index;
		if (finalLod > maxFinalLod)
		{
			maxFinalLod = finalLod;
		}
	}

	int64 finalLodVertexCount[MAX_STATOBJ_LODS_NUM] = { 0 };
	int finalLodMaxAutocopyReceiver = 0;
	bool bUsedAutocopyLimit = false;

	// Two passes: first one computes resulting mesh sizes; second one
	// performs modifications to nodes and forms final lod lists.
	// Between passes we will made decision about how many lod levels
	// should be auto-copied.
	for (int pass = 0; pass < 2; ++pass)
	{
		// Find maximal LOD which should receive auto-copied LODs
		if (pass == 1)
		{
			for (int finalLodIndex = 1; finalLodIndex <= maxFinalLod; ++finalLodIndex)
			{
				const double vertexCount0 = (double)finalLodVertexCount[finalLodIndex - 1];
				const double vertexCount1 = (double)finalLodVertexCount[finalLodIndex];

				if (vertexCount1 > vertexCount0 * 0.6)
				{
					// size of this LOD is more than 60% of previous LOD, 
					// so it's better to disable auto-copying to this LOD
					break;
				}

				finalLodMaxAutocopyReceiver = finalLodIndex;
			}
		}

		for (size_t lodableIndex = 0; lodableIndex < lodableNodes.size(); ++lodableIndex)
		{
			const LodableNodeInfo& info = lodableNodes[lodableIndex];

			CNodeCGF* const pNode = info.pNode;
			assert(info.lod0Index + maxFinalLod >= info.maxLodFound);

			lodNodes.clear();
			LodHelpers::FindLodNodes(lodNodes, pCGF, false, pNode, lodNamePrefix);

			CNodeCGF* arrLodNodes[MAX_STATOBJ_LODS_NUM] = { 0 };

			for (size_t i = 0; i < lodNodes.size(); ++i)
			{
				CNodeCGF* const pLodNode = lodNodes[i];
				const int lodIndex = LodHelpers::GetLodIndex(pLodNode->name, lodNamePrefix);
				assert((lodIndex > 0) && (lodIndex < MAX_STATOBJ_LODS_NUM));
				arrLodNodes[lodIndex] = pLodNode;
			}
			arrLodNodes[0] = pNode;

			// Handle shifting LODs according to consoles_lod0
			if ((pass == 1) && (info.lod0Index > 0))
			{				
				if (!LodHelpers::CopyMeshData(pCGF, pNode, arrLodNodes[info.lod0Index]))
				{
					return false;
				}

				// Delete unused LOD nodes
				for (int lodIndex = 1; lodIndex <= info.lod0Index; ++lodIndex)
				{
					if (arrLodNodes[lodIndex])
					{
						if (!LodHelpers::DeleteNode(pCGF, arrLodNodes[lodIndex]))
						{
							return false;
						}
					}
				}

				arrLodNodes[info.lod0Index] = pNode;
			}

			// Add LOD nodes to appropriate CGF objects
			static const bool bShowAutcopyStatistics = false;
			CNodeCGF* pLastLodNode = arrLodNodes[info.lod0Index];
			for (int finalLodIndex = 0; finalLodIndex <= maxFinalLod; ++finalLodIndex)
			{
				const size_t lodIndex = info.lod0Index + finalLodIndex;

				const bool bDuplicate = ((lodIndex >= MAX_STATOBJ_LODS_NUM) || (arrLodNodes[lodIndex] == 0));
				CNodeCGF* const pLodNode = bDuplicate ? pLastLodNode : arrLodNodes[lodIndex];

				if (bDuplicate && (pNode->name[0] == '$'))
				{
					// No need to autocopy special nodes, engine doesn't handle them
					// correctly when they are stored in LODs
					continue;					
				}

				if ((pass == 0) || (finalLodIndex == 0))
				{
					if (pass == 0)
					{
						finalLodVertexCount[finalLodIndex] += pLodNode->pMesh->GetVertexCount();
					}
					continue;
				}

				if (bDuplicate && (pLodNode->pMesh->GetVertexCount() <= 0))
				{
					// No need to autocopy meshes without geometry, streaming in the
					// engine doesn't use such nodes.
					continue;
				}

				if (bDuplicate && (finalLodIndex > finalLodMaxAutocopyReceiver))
				{
					bUsedAutocopyLimit = true;
					continue;
				}

				if (bShowAutcopyStatistics && bDuplicate)
				{
					const int subsetCount = pLodNode->pMesh->GetSubSetCount();
					const int faceCount = pLodNode->pMesh->GetIndexCount() / 3;
					const int vertexCount = pLodNode->pMesh->GetVertexCount();
					RCLogWarning(
						"@,%u,%u,%u,%u,%u,%s,%s",
						(uint)finalLodIndex - 1, (uint)maxFinalLod, (uint)subsetCount, (uint)faceCount, (uint)vertexCount, pLodNode->name, pCGF->GetFilename());
				}

				if (pLodNode != pNode)
				{
					cry_strcpy(pLodNode->name, pNode->name);
					pLodNode->pParent = 0;
				}

				CContentCGF* const pLodCGF = (m_pLODs[finalLodIndex] == 0)
					? MakeLOD(finalLodIndex, pCGF)
					: m_pLODs[finalLodIndex];

				// Note: we should set lod nodes' pParent to 0 right before saving lod nodes to
				// a file. Otherwise parent nodes (LOD0 and its parents) will be exported also,
				// which is wrong for lod files.
				// Note that we cannot set pParent to 0 *here*, because in case of LODs auto-copying
				// the lod node can actually be the LOD0 node (pLodNome == pNode), so setting its
				// pParent to 0 will destroy LOD0's parenting info, which is bad, because we really
				// need to have proper hierarchy for LOD0 CGF.
				pLodCGF->AddNode(pLodNode);

				pLastLodNode = pLodNode;
			}

			// Delete LOD nodes from LOD0 CGF
			if (pass == 1)
			{
				for (int finalLodIndex = 1; finalLodIndex < MAX_STATOBJ_LODS_NUM; ++finalLodIndex)
				{
					const size_t lodIndex = info.lod0Index + finalLodIndex;
					if ((lodIndex < MAX_STATOBJ_LODS_NUM) && arrLodNodes[lodIndex])
					{
						if (!LodHelpers::DeleteNode(pCGF, arrLodNodes[lodIndex]))
						{
							return false;
						}
					}
				}
			}
		}
	}

	if (bUsedAutocopyLimit)
	{
		RCLogWarning(
			"Autocopying LODs was limited to LOD %d because vertex count difference between LODs is less than 40%% (%lli vs %lli). File %s.",
			finalLodMaxAutocopyReceiver, finalLodVertexCount[finalLodMaxAutocopyReceiver], finalLodVertexCount[finalLodMaxAutocopyReceiver + 1], pCGF->GetFilename());
	}

	if (!LodHelpers::ValidateMeshSharing(pCGF) || !LodHelpers::ValidateLodNodes(pCGF, lodNamePrefix))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* CStaticObjectCompiler::MakeLOD(int lodIndex, const CContentCGF* pCGF)
{
	assert((lodIndex >= 0) && (lodIndex < MAX_STATOBJ_LODS_NUM));

	if (m_pLODs[lodIndex])
	{
		return 0;
	}

	const string filename = pCGF->GetFilename();
	m_pLODs[lodIndex] = new CContentCGF(filename.c_str());

	CContentCGF *pCompiledCGF = m_pLODs[lodIndex];
	*pCompiledCGF->GetExportInfo() = *pCGF->GetExportInfo(); // Copy export info.
	*pCompiledCGF->GetPhysicalizeInfo() = *pCGF->GetPhysicalizeInfo();
	pCompiledCGF->GetExportInfo()->bCompiledCGF = true;
	pCompiledCGF->GetUsedMaterialIDs() = pCGF->GetUsedMaterialIDs();
	
	if (lodIndex > 0 && m_pLODs[0])
	{
		m_pLODs[0]->GetExportInfo()->bHaveAutoLods = true;
	}

	return m_pLODs[lodIndex];
}

//////////////////////////////////////////////////////////////////////////
bool CStaticObjectCompiler::MakeMergedCGF( CContentCGF *pCompiledCGF,CContentCGF *pCGF )
{
	std::vector<CNodeCGF*> merge_nodes;
	for (int i = 0; i < pCGF->GetNodeCount(); i++)
	{
		CNodeCGF *pNode = pCGF->GetNode(i);
		if (pNode->pMesh && !pNode->bPhysicsProxy && pNode->type == CNodeCGF::NODE_MESH)
		{
			merge_nodes.push_back(pNode);
		}
	}

	if (merge_nodes.empty())
	{
		RCLogError("Error merging nodes, No mergeable geometry in CGF %s",pCGF->GetFilename());
		return false;
	}

	CMesh *pMergedMesh = new CMesh;
	string errorMessage;
	if (!CGFNodeMerger::MergeNodes(pCGF, merge_nodes,errorMessage,pMergedMesh))
	{
		RCLogError("Error merging nodes: %s, in CGF %s", errorMessage.c_str(),pCGF->GetFilename());
		delete pMergedMesh;
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Add single node for merged mesh.
	CNodeCGF *pNode = new CNodeCGF;
	pNode->type = CNodeCGF::NODE_MESH;
	cry_strcpy(pNode->name, "Merged");
	pNode->bIdentityMatrix = true;
	pNode->pMesh = pMergedMesh;
	pNode->pMaterial = pCGF->GetCommonMaterial();

	CNodeCGF *pMergedNode = pNode;
	
	// Add node to CGF contents.
	pCompiledCGF->AddNode( pNode );
	//////////////////////////////////////////////////////////////////////////

	const string lodNamePrefix(CGF_NODE_NAME_LOD_PREFIX);

	for (int nLod = 1; nLod < MAX_STATOBJ_LODS_NUM; nLod++)
	{
		// Try to merge LOD nodes.
		merge_nodes.clear();
		for (int i = 0; i < pCGF->GetNodeCount(); i++)
		{
			CNodeCGF *pNode = pCGF->GetNode(i);
			if (pNode->pMesh && !pNode->bPhysicsProxy && pNode->type == CNodeCGF::NODE_HELPER)
			{
				const string nodeName(pNode->name);
				if (StringHelpers::StartsWithIgnoreCase(nodeName, lodNamePrefix))
				{
					if (nodeName.length() <= lodNamePrefix.length())
					{
						RCLogError("Error merging LOD %d nodes: LOD node name '%s' doesn't contain LOD index in CGF %s", nLod, nodeName.c_str(), pCGF->GetFilename());
						return false;
					}
					const int lodIndex = atoi(nodeName.c_str() + lodNamePrefix.length());
					if (lodIndex == nLod)
					{
						// This is a LOD helper.
						merge_nodes.push_back(pNode);
					}
				}
			}
		}
		if (!merge_nodes.empty())
		{
			CMesh *pMergedLodMesh = new CMesh;
			string errorMessage;
			if (!CGFNodeMerger::MergeNodes(pCGF, merge_nodes,errorMessage,pMergedLodMesh))
			{
				RCLogError("Error merging LOD %d nodes: %s, in CGF %s",nLod,errorMessage.c_str(),pCGF->GetFilename());
				delete pMergedLodMesh;
				return false;
			}

			//////////////////////////////////////////////////////////////////////////
			// Add LOD node
			//////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////
			// Add single node for merged mesh.
			CNodeCGF *pNode = new CNodeCGF;
			pNode->type = CNodeCGF::NODE_HELPER;
			pNode->helperType = HP_GEOMETRY;
			cry_sprintf(pNode->name, "%s%d_%s", lodNamePrefix.c_str(), nLod, pMergedNode->name);
			pNode->bIdentityMatrix = true;
			pNode->pMesh = pMergedLodMesh;
			pNode->pParent = pMergedNode;
			pNode->pMaterial = pCGF->GetCommonMaterial();

			// Add node to CGF contents.
			pCompiledCGF->AddNode( pNode );
			//////////////////////////////////////////////////////////////////////////
		}
	}

	// Add rest of the helper nodes.
	int numNodes = pCGF->GetNodeCount();
	for (int i=0; i<numNodes; i++)
	{
		CNodeCGF *pNode = pCGF->GetNode(i);

		if (pNode->type != CNodeCGF::NODE_MESH)
		{
			// Do not add LOD nodes.
			if (!StringHelpers::StartsWithIgnoreCase(string(pNode->name), lodNamePrefix))
			{
				if (pNode->pParent && pNode->pParent->type == CNodeCGF::NODE_MESH)
				{
					pNode->pParent = pMergedNode;
				}
				pCompiledCGF->AddNode(pNode);
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
int CStaticObjectCompiler::GetSubMeshCount(const CContentCGF* pCGFLod0)
{
	int subMeshCount = 0;
	if (pCGFLod0) 
	{
		const int nodeCount = pCGFLod0->GetNodeCount();
		for (int i = 0; i < nodeCount; ++i)
		{
			const CNodeCGF* const pNode = pCGFLod0->GetNode(i);
			if (pNode && pNode->type == CNodeCGF::NODE_MESH && pNode->pMesh)
			{
				++subMeshCount;
			}
		}
	}
	return subMeshCount;
}

//////////////////////////////////////////////////////////////////////////
int CStaticObjectCompiler::GetJointCount(const CContentCGF* pCGF)
{
	int jointCount = 0;
	if (pCGF)
	{
		const int nodeCount = pCGF->GetNodeCount();
		for (int i = 0; i < nodeCount; ++i)
		{
			const CNodeCGF* const pNode = pCGF->GetNode(i);
			if (pNode && pNode->type == CNodeCGF::NODE_HELPER && StringHelpers::StartsWith(pNode->name, "$joint"))
			{
				++jointCount;
			}
		}
	}
	return jointCount;
}
