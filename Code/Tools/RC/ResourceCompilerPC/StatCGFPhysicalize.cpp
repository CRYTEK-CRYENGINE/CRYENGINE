// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   StatCGFPhysicalize.cpp
//  Version:     v1.00
//  Created:     8/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "StatCGFPhysicalize.h"
#include "../../CryEngine/Cry3DEngine/MeshCompiler/MeshCompiler.h"
#include "Util.h"

#define SMALL_MESH_NUM_INDEX 30	

//////////////////////////////////////////////////////////////////////////
CPhysicsInterface::CPhysicsInterface()
{
	IPhysicalWorld* const p = m_physLoader.GetWorldPtr();
	m_pGeomManager = p ? p->GetGeomManager() : 0;
}

//////////////////////////////////////////////////////////////////////////
CPhysicsInterface::~CPhysicsInterface()
{
}

//////////////////////////////////////////////////////////////////////////
CPhysicsInterface::EPhysicalizeResult CPhysicsInterface::Physicalize( CNodeCGF *pNodeCGF,CContentCGF *pCGF )
{
	if (!pNodeCGF->pMesh)
	{
		return ePR_Empty;
	}

	for (size_t i = 0; i < CRY_ARRAY_COUNT(pNodeCGF->physicalGeomData); ++i)
	{
		pNodeCGF->physicalGeomData[i].clear();
	}

	CPhysicsInterface::EPhysicalizeResult res;

	res = PhysicalizeGeomType(PHYS_GEOM_TYPE_DEFAULT, pNodeCGF, pCGF);
	if (res == ePR_Fail)
	{
		return ePR_Fail;
	}

	res = PhysicalizeGeomType(PHYS_GEOM_TYPE_NO_COLLIDE, pNodeCGF, pCGF);
	if (res == ePR_Fail)
	{
		return ePR_Fail;
	}

	res = PhysicalizeGeomType(PHYS_GEOM_TYPE_OBSTRUCT, pNodeCGF, pCGF);
	if (res == ePR_Fail)
	{
		return ePR_Fail;
	}

	return ePR_Ok;
}


int CPhysicsInterface::CheckNodeBreakable(CNodeCGF *pNode, IGeometry *pGeom)
{
	if (!pNode->pMesh || !GetGeomManager())
	{
		return 0;
	}

	assert(pNode->pMesh->m_pPositionsF16 == 0);
	phys_geometry *pPhysGeom = 0;
	int res = 0;
	if (!pGeom)
	{
		if (pNode->physicalGeomData[0].empty())
			return 0;
		CMemStream stmIn( &pNode->physicalGeomData[0].front(), pNode->physicalGeomData[0].size(), true );
		pPhysGeom = GetGeomManager()->LoadPhysGeometry(stmIn, pNode->pMesh->m_pPositions, pNode->pMesh->m_pIndices, 0);
		if (!pPhysGeom || !(pGeom=pPhysGeom->pGeom))
			return 0;
	}
	if ((pGeom->GetType()==GEOM_TRIMESH) && !pNode->pMesh->m_subsets.empty())
	{
		mesh_data *pmd = (mesh_data*)pGeom->GetData();
		SMeshSubset *subsets = &pNode->pMesh->m_subsets[0];
		int i; res=1;
		for(i=pNode->pMesh->GetSubSetCount()-1; i>0 && subsets[i].nPhysicalizeType!=PHYS_GEOM_TYPE_DEFAULT; i--);
		if (i!=0 || subsets[0].nPhysicalizeType!=PHYS_GEOM_TYPE_DEFAULT || !pmd->pForeignIdx || pmd->nTris*3!=subsets[0].nNumIndices || !pmd->pVtxMap)
		{
			res = 0;
		}
		else
		{
			for(i=0; i<pmd->nTris && pmd->pForeignIdx[i]*3>=subsets[0].nFirstIndexId && 
					pmd->pForeignIdx[i]*3<subsets[0].nFirstIndexId+subsets[0].nNumIndices; i++);
			if (i<pmd->nTris)
				res = 0;
		}
	}
	if (pPhysGeom)
		GetGeomManager()->UnregisterGeometry(pPhysGeom);
	return res;
}


//////////////////////////////////////////////////////////////////////////
CPhysicsInterface::EPhysicalizeResult CPhysicsInterface::PhysicalizeGeomType( int nGeomType,CNodeCGF *pNodeCGF,CContentCGF *pCGF )
{
	if (!GetGeomManager())
	{
		return ePR_Fail;
	}

	CMesh &mesh = *pNodeCGF->pMesh;
	assert(mesh.m_pPositionsF16 == 0);

	std::vector<index_t> indices;
	std::vector<char> faceMaterials;
	std::vector<int> originalFaces;
	indices.reserve( mesh.GetIndexCount() );
	originalFaces.reserve( mesh.GetIndexCount()/3 );

	if (pNodeCGF->bPhysicsProxy && pNodeCGF->type != CNodeCGF::NODE_HELPER)
	{
		RCLogError(
			"Error in calculating physics mesh: Node '%s' in file '%s' is marked as 'physics proxy' but is not marked as 'helper'.",
			pNodeCGF->name, pCGF->GetFilename());
		return ePR_Fail;
	}

	bool bHasRenderPhysicsGeometry = false;
	bool bHasPurePhysicsGeometry = false;

	for (int m = 0; m < mesh.GetSubSetCount(); m++)
	{
		SMeshSubset &subset = mesh.m_subsets[m];
		
		if (subset.nPhysicalizeType == PHYS_GEOM_TYPE_NONE)
			continue;
		if ((subset.nPhysicalizeType != nGeomType) && !(subset.nPhysicalizeType==PHYS_GEOM_TYPE_DEFAULT_PROXY && nGeomType==PHYS_GEOM_TYPE_DEFAULT))
			continue;
		if (subset.nNumIndices <= 0 || subset.nNumVerts <= 0)
			continue;

		// Everything not PHYS_GEOM_TYPE_DEFAULT is a physical proxy and don't need foreign ids.
		if (subset.nPhysicalizeType == PHYS_GEOM_TYPE_DEFAULT && !pNodeCGF->bPhysicsProxy && pNodeCGF->type != CNodeCGF::NODE_HELPER)
		{
			bHasRenderPhysicsGeometry = true;
		}
		else
		{
			bHasPurePhysicsGeometry = true;
		}

		for (int idx = subset.nFirstIndexId; idx < subset.nFirstIndexId+subset.nNumIndices; idx += 3)
		{
			indices.push_back((index_t)mesh.m_pIndices[idx]);
			indices.push_back((index_t)mesh.m_pIndices[idx + 1]);
			indices.push_back((index_t)mesh.m_pIndices[idx + 2]);
			faceMaterials.push_back( subset.nMatID );
			const int nOriginalFace = idx / 3;
			originalFaces.push_back( nOriginalFace );
		}
	}

	if (indices.empty())
	{
		return ePR_Empty;
	}
	assert(!originalFaces.empty());

	if (bHasRenderPhysicsGeometry && bHasPurePhysicsGeometry)
	{
		RCLogError(
			"Error in Calculating Physics Mesh, Node '%s' in file '%s' contains both physicalized render geometry and a physics proxy.\n"
			"This will cause corruption to the physics mesh in game. To fix this you can do either of the following:\n"
			"\t1) Create a physics proxy for all of your mesh and do not have physicalized render geometry.\n"
			"\t2) Remove your physics proxies and only have physicalized render geometry.\n"
			"\t3) Have the physicalized render geometry and the physics proxies in separate nodes and do not merge them on export.",
			pNodeCGF->name, pCGF->GetFilename());
	}

	Vec3* pVertices = mesh.m_pPositions;
	const int nVerts = mesh.GetVertexCount();

	if (nVerts > (std::numeric_limits<index_t>::max)())
	{
		RCLogError(
			"Physicalizer cannot be started: mesh of the Node '%s' in file '%s' contains oo many vertices: %d (max for the physicalizer is %" PRISIZE_T ").",
			pNodeCGF->name, pCGF->GetFilename(), nVerts, (size_t)(std::numeric_limits<index_t>::max)());
		return ePR_Fail;
	}

	// We have to check if the input data are ok (3D MAX produces "not a number" coordinates sometimes)
	for (int i = 0; i < nVerts; i++)
	{
		if (!mesh.m_pPositions[i].IsValid())
		{
			RCLogError(
				"Mesh of the Node '%s' in file '%s' contains invalid vertex position at index %d",
				pNodeCGF->name, pCGF->GetFilename(), i);
			return ePR_Fail;
		}
	}	

	//////////////////////////////////////////////////////////////////////////
	if (pNodeCGF->type == CNodeCGF::NODE_HELPER && pCGF->GetExportInfo()->bMergeAllNodes)
	{
		// Transform physics proxy nodes into the world space when everything must be merged.
		pVertices = new Vec3[nVerts];
		for (int i = 0; i < nVerts; i++)
		{
			pVertices[i] = pNodeCGF->worldTM.TransformPoint(mesh.m_pPositions[i]);
		}
	}
	else if (pNodeCGF->type == CNodeCGF::NODE_HELPER && !pCGF->GetExportInfo()->bMergeAllNodes)
	{
		// Transform physics proxy nodes into the local space for not merged objects.
		pVertices = new Vec3[nVerts];
		for (int i = 0; i < nVerts; i++)
		{
			pVertices[i] = pNodeCGF->localTM.TransformPoint(mesh.m_pPositions[i]);
		}
	}

	if (nVerts > 2)
	{
		IGeomManager *pGeoman = GetGeomManager();
		Vec3 ptmin = mesh.m_bbox.min;
		Vec3 ptmax = mesh.m_bbox.max;

		int flags = mesh_multicontact0;
		float tol = 0.05f;
		flags |= indices.size() <= SMALL_MESH_NUM_INDEX ? mesh_SingleBB : mesh_OBB|mesh_AABB|mesh_AABB_rotated;
		flags |= mesh_approx_box | mesh_approx_sphere | mesh_approx_cylinder | mesh_approx_capsule;

		int* const pForeignIndices = bHasRenderPhysicsGeometry ? &originalFaces[0] : nullptr;

		// When no foreign indices there is no need to save vtxmap.
		if (pForeignIndices)
		{
			flags |= mesh_keep_vtxmap_for_saving;
		}

		flags |= mesh_full_serialization;

		//////////////////////////////////////////////////////////////////////////
		// Get flags from properties string.
		//////////////////////////////////////////////////////////////////////////
		bool bForceBox = false;
		string props = pNodeCGF->properties;
		if (!props.empty())
		{
			int curPos = 0;
			string token = props.Tokenize("\n",curPos);
			while (token != "")
			{
				token = token.Trim();

				if (strstr(token,"wheel") || strstr(token,"cylinder"))
				{
					flags &= ~(mesh_approx_box | mesh_approx_sphere | mesh_approx_capsule);
					flags |= mesh_approx_cylinder;
					tol = 1.0f;
				}
				else if (strstr(token,"explosion") || strstr(token,"crack")) // temporary solution
				{
					flags = flags & ~mesh_multicontact1 | mesh_multicontact2;	// temporary solution
					flags &= ~(mesh_approx_box | mesh_approx_sphere | mesh_approx_cylinder);
				}
				else if (strstr(token,"box_forced"))
				{
					bForceBox = true;
				}
				else if (strstr(token, "box"))
				{
					flags &= ~(mesh_approx_cylinder | mesh_approx_sphere | mesh_approx_capsule);
					flags |= mesh_approx_box;
					tol = 1.0f;
				}
				else if (strstr(token,"sphere"))
				{
					flags &= ~(mesh_approx_cylinder | mesh_approx_box | mesh_approx_capsule);
					flags |= mesh_approx_sphere;
					tol = 1.0f;	
				}
				else if (strstr(token,"capsule"))
				{
					flags &= ~(mesh_approx_cylinder | mesh_approx_box | mesh_approx_sphere);
					flags |= mesh_approx_capsule;
					tol = 1.0f;	
				}
				else if (strstr(token,"notaprim"))
				{
					flags &= ~(mesh_approx_capsule | mesh_approx_cylinder | mesh_approx_box | mesh_approx_sphere);
					tol = 0.0f;
					break;
				}

				token = props.Tokenize("\n",curPos);
			}
		}

		if (!bForceBox)
		{
			// Physics doesn't support more than 64K vertices (yet).
			// Note that console-related code in physics doesn't support more
			// than 32K vertices - Anton should write a proper check below.
			if (nVerts >= 0xffff)  // '>=': we reserve index 0xffff for ourselves
			{
				RCLogWarning(
					"Mesh physicalizer cannot be started: mesh of the Node '%s' in file '%s' contains too many vertices: %d (max for the physicalizer is %d). Using Box Physicalizer instead.",
					pNodeCGF->name, pCGF->GetFilename(), nVerts, 0xffff);
				bForceBox = true;
			}
		}

		if (bForceBox)
		{
			flags = mesh_full_serialization | mesh_SingleBB | mesh_shared_idx;
		}

		int nMinTrisPerNode = 2;
		int nMaxTrisPerNode = 4;
		Vec3 size = ptmax - ptmin;
		if (indices.size() < 600 && Util::getMax(size.x,size.y,size.z) > 6) // make more dense OBBs for large (wrt terrain grid) objects
			nMinTrisPerNode = nMaxTrisPerNode = 1;

		//////////////////////////////////////////////////////////////////////////
		// Create physical mesh.
		//////////////////////////////////////////////////////////////////////////
		IGeometry *pGeom = pGeoman->CreateMesh(
			pVertices,
			&indices[0],
			&faceMaterials[0],
			pForeignIndices,
			indices.size() / 3,
			flags,
			bForceBox ? 0.0f : tol, 
			nMinTrisPerNode,
			nMaxTrisPerNode,
			2.5f);

		if(pGeom)
		{
			if (bForceBox)
			{
				primitives::box bbox;
				pGeom->GetBBox(&bbox);
				pGeom->Release();
				pGeom = pGeoman->CreatePrimitive(primitives::box::type, &bbox);
			}
			else
			{
				if (pGeom->GetErrorCount())
				{
					if (bHasRenderPhysicsGeometry)
					{
						RCLog("%d artifacts in physicalized render geometry (%s), one or more meshes are not manifold, "
							"try converting the physicalized render geometry to a manifold physics proxy if you experience problems in game.", pGeom->GetErrorCount(), pNodeCGF->name);
					}
					else
					{
						RCLog("%d error(s) in physics proxy (%s), one or more physics proxies are not manifold.", pGeom->GetErrorCount(), pNodeCGF->name);
					}
				}
			}

			phys_geometry *pPhysGeom = pGeoman->RegisterGeometry(pGeom, faceMaterials[0]);

			CMemStream stm(false);

			pGeoman->SavePhysGeometry( stm,pPhysGeom );

			// Add physicalized data to the node.
			pNodeCGF->physicalGeomData[nGeomType-PHYS_GEOM_TYPE_DEFAULT].resize(stm.GetUsedSize());
			memcpy( &pNodeCGF->physicalGeomData[nGeomType-PHYS_GEOM_TYPE_DEFAULT][0],stm.GetBuf(),stm.GetUsedSize() );

			if (flags & mesh_full_serialization)
			{
				// We saved complete physics mesh.
				pNodeCGF->nPhysicalizeFlags |= CNodeCGF::ePhysicsalizeFlag_MeshNotNeeded;
			}

			if (nGeomType==PHYS_GEOM_TYPE_DEFAULT && !CheckNodeBreakable(pNodeCGF, pGeom))
				pNodeCGF->nPhysicalizeFlags |= CNodeCGF::ePhysicsalizeFlag_NoBreaking;

			pGeoman->UnregisterGeometry(pPhysGeom);
			pGeom->Release();
		}
	}

	// Free temporary verts array.
	if (pVertices != mesh.m_pPositions)
		delete [] pVertices;

	return ePR_Ok;
}

namespace {

	int SaveStlSurface(const char *fname, Vec3 *pt,vtx_idx *pidx,int nTris)
	{
		int i,j;
		Vec3 n;
		FILE *f = fopen(fname,"wt");
		if (!f)
			return 0;

		fprintf(f, "solid tmpobj\n");
		for(i=0;i<nTris;i++) {
			n = (pt[pidx[i*3+1]]-pt[pidx[i*3]] ^ pt[pidx[i*3+2]]-pt[pidx[i*3]]).normalized();
			fprintf(f, "  facet normal %.8g %.8g %.8g\n", n.x,n.y,n.z);
			fprintf(f, "    outer loop\n");
			for(j=0;j<3;j++)
				fprintf(f, "      vertex %.8g %.8g %.8g\n", pt[pidx[i*3+j]].x,pt[pidx[i*3+j]].y,pt[pidx[i*3+j]].z);
			fprintf(f, "    endloop\n");
			fprintf(f, "  endfacet\n");
		}
		fprintf(f, "endsolid tmpobj\n");
		fclose(f);

		return 1;
	}

	int LoadNetgenTetrahedrization(const char *fname, Vec3 *&pVtx,int &nVtx, int *&pTets,int &nTets)
	{
		int i,j,nj;
		FILE *f = fopen(fname,"rt");
		char buf[65536],*pbuf=buf,*str;

		pVtx=0; pTets=0;
		while ((!pVtx || !pTets) && (str=fgets(pbuf,65536, f))) {
			if (!strncmp(str,"volumeelements",14)) {
				fscanf(f, "%d",&nTets); pTets = new int[nTets*4];
				for(i=0;i<nTets;i++) {
					fscanf(f, " %d %d %d %d %d %d", &j,&nj, pTets+i*4,pTets+i*4+1,pTets+i*4+2,pTets+i*4+3);
					for(j=0;j<nj;j++) pTets[i*4+j]--;
				}
			}     else if (!strncmp(str,"points",6)) {
				fscanf(f, "%d",&nVtx); pVtx = new Vec3[nVtx];
				for(i=0;i<nVtx;i++) 
					fscanf(f, " %f %f %f", &pVtx[i].x,&pVtx[i].y,&pVtx[i].z);
			}
		}
		
		fclose(f);
		
		return pVtx && pTets;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPhysicsInterface::ProcessBreakablePhysics( CContentCGF *pCompiledCGF,CContentCGF *pSrcCGF )
{
	if(!pSrcCGF)
		return;
	
	const CPhysicalizeInfoCGF* const pPi = pSrcCGF->GetPhysicalizeInfo();

	if(pPi->nMode==-1 || pPi->nGranularity==-1)
		return;

	char path[1024];
	GetModuleFileName(GetModuleHandle(NULL),path, 1024);
	char * ch = strrchr(path, '\\');
	if(!ch)
		return;
	strcpy(ch, "\\netgen\\");

	char filepath[4096];
	cry_sprintf(filepath, "%sng.exe", path);

	char stlFile[1024];
	cry_sprintf(stlFile, "%stempin.stl", path);

	char volFile[1024];
	cry_sprintf(volFile, "%stempout.vol", path);

	{
		// Remove read only attribute of ng.ini
		char ngIni[1024];
		cry_sprintf(ngIni, "%sng.ini", path);
		SetFileAttributes( ngIni,FILE_ATTRIBUTE_ARCHIVE );
	}

	CNodeCGF * pNode = 0;
	int i;
	for(i=0; i<pSrcCGF->GetNodeCount(); i++)
	{
		pNode = pSrcCGF->GetNode(i);
		if(!strcmp(pNode->name, "$tet"))
			break;
		pNode = 0;
	}
	if(!pNode)
		return;
	CMesh * pMesh = pNode->pMesh;
	if(!pMesh)
		return;
	assert(pMesh->m_pPositionsF16 == 0);

	const int vertexCount = pMesh->GetVertexCount();
	if (vertexCount > 0)
	{
		std::vector<Vec3> vtx;
		vtx.resize(vertexCount);
		for(i=0; i<vertexCount; i++)
			vtx[i] = pNode->worldTM*pMesh->m_pPositions[i];

		if(!pMesh->m_pIndices)
		{
			const int faceCount = pMesh->GetFaceCount();

			if (vertexCount >= (1 << 16))   // ">=": we reserve index 0xffff for ourselves
			{
				RCLogError("Breakable physics cannot be processed: too many vertices (%i, max allowed is %i)", vertexCount, (1 << 16));
				return;
			}

			std::vector<vtx_idx> inds;
			inds.resize(faceCount * 3);

			for(i=0; i<faceCount; i++)
			{
				for (int j = 0; j < 3; ++j)
				{
					const int vIdx = pMesh->m_pFaces[i].v[j];
					if (vIdx < 0 || vIdx >= vertexCount)
					{
						RCLogError("Breakable physics cannot be processed: bad vertex index found (%i, # of vertices is %i)", vIdx, vertexCount);
						return;
					}
					inds[i*3+j] = vIdx;
				}
			}

			if (faceCount > 0)
			{
				SaveStlSurface(stlFile, &vtx[0], &inds[0], faceCount);
			}
		}
		else
		{
			SaveStlSurface(stlFile, &vtx[0], pMesh->m_pIndices, pMesh->GetIndexCount()/3);
		}
	}

	const char * pGr = "";

	switch(pPi->nGranularity)
	{
	case 0:
		pGr = "-verycoarse";
		break;
	case 1:
		pGr = "-coarse";
		break;
	case 2:
		pGr = "-moderate";
		break;
	case 3:
		pGr = "-fine";
		break;
	case 4:
		pGr = "-veryfine";
		break;
	}

	char cmd[4096];
	cry_sprintf(cmd, "%s -geofile=tempin.stl -meshfile=tempout.vol -batchmode %s", filepath, pGr);

	SetEnvironmentVariable("TCL_LIBRARY", ".\\tcl8.3");
	SetEnvironmentVariable("TIX_LIBRARY", ".\\tix8.2");

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	memset( &si,0,sizeof(si) );
	si.cb = sizeof(si);
	if (!CreateProcess( NULL,cmd,NULL,NULL,FALSE,0,NULL,path,&si,&pi ))
	{
		RCLogError("Error executing netgen. To solve this you can Either:\n\n\tensure that the NETGEN executables are present in the RC folder.\n\tRemove the asset\n\tRemove the breakable flag from the asset.", pNode->name);
		return;
	}
	// Wait until child process exits.
	WaitForSingleObject( pi.hProcess, INFINITE );

	DWORD lpExitCode = 0;
	GetExitCodeProcess( pi.hProcess,&lpExitCode );
	// Close process and thread handles.
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	if (lpExitCode != EXIT_SUCCESS)
		return;

	CPhysicalizeInfoCGF* const pConPi = pCompiledCGF->GetPhysicalizeInfo();
	LoadNetgenTetrahedrization(volFile, pConPi->pRetVtx, pConPi->nRetVtx, pConPi->pRetTets, pConPi->nRetTets);

	remove(stlFile);
	remove(volFile);
}

//////////////////////////////////////////////////////////////////////////
bool CPhysicsInterface::DeletePhysicalProxySubsets(CNodeCGF* pNodeCGF, bool const bCga)
{
	CMesh& mesh = *pNodeCGF->pMesh;

	if (mesh.GetFaceCount() > 0)
	{
		RCLogError("Uncompiled mesh passed to %s. Contact an RC programmer.", __FUNCTION__);
		return false;		
	}

	const bool bPreValidation = mesh.Validate(0);

	// Sometimes subsets are empty or broken (due to errors in the past 
	// versions of the RC), so we delete them before further processing.
	for (size_t i = 0; i < mesh.m_subsets.size(); ++i)
	{
		const SMeshSubset& subset = mesh.m_subsets[i];

		const bool bEmpty = ((subset.nNumIndices <= 0) && (subset.nNumVerts <= 0));
		const bool bBroken = ((subset.nNumIndices <= 0) != (subset.nNumVerts <= 0));

		if (bEmpty || bBroken)
		{
			mesh.m_subsets.erase(i);
			--i;
		}
	}

	// Check if we still have physical geometry to delete.
	{
		bool bHasPhysicalGeometry = false;
		
		for (size_t i = 0; i < mesh.m_subsets.size(); ++i)
		{
			const SMeshSubset& subset = mesh.m_subsets[i];

			if (subset.nPhysicalizeType != PHYS_GEOM_TYPE_NONE && 
				subset.nPhysicalizeType != PHYS_GEOM_TYPE_DEFAULT)
			{
				bHasPhysicalGeometry = true;
				break;
			}
		}

		if (!bHasPhysicalGeometry)
		{
			return true;
		}
	}

	// Remove physical geometry
	for (int i = (int)mesh.m_subsets.size()-1; i >= 0; --i)
	{
		SMeshSubset& subset = mesh.m_subsets[i];

		if (subset.nPhysicalizeType == PHYS_GEOM_TYPE_NONE ||
			subset.nPhysicalizeType == PHYS_GEOM_TYPE_DEFAULT)
		{
			continue;
		}

		const int nFirstIndexId = subset.nFirstIndexId;
		const int nNumIndices = subset.nNumIndices;
		const int nFirstVertexId = subset.nFirstVertId;
		const int nNumVertices = subset.nNumVerts;

		// Collapse indices.
		mesh.RemoveRangeFromStream(CMesh::INDICES, nFirstIndexId, nNumIndices );

		// Collapse vertices.
		mesh.RemoveRangeFromStream(CMesh::POSITIONS,   nFirstVertexId, nNumVertices);
		mesh.RemoveRangeFromStream(CMesh::NORMALS,     nFirstVertexId, nNumVertices);
		mesh.RemoveRangeFromStream(CMesh::TEXCOORDS,   nFirstVertexId, nNumVertices);
		mesh.RemoveRangeFromStream(CMesh::COLORS_0,    nFirstVertexId, nNumVertices);
		mesh.RemoveRangeFromStream(CMesh::COLORS_1,    nFirstVertexId, nNumVertices);
		mesh.RemoveRangeFromStream(CMesh::TANGENTS,    nFirstVertexId, nNumVertices);
		mesh.RemoveRangeFromStream(CMesh::BONEMAPPING, nFirstVertexId, nNumVertices);
		mesh.RemoveRangeFromStream(CMesh::VERT_MATS,   nFirstVertexId, nNumVertices);
		mesh.RemoveRangeFromStream(CMesh::QTANGENTS,   nFirstVertexId, nNumVertices);

		// Erase subset.
		mesh.m_subsets.erase(i);
		--i;

		// Fix references to vertices and indices in remaining subsets.
		for (size_t j = 0; j < mesh.m_subsets.size(); ++j)
		{
			SMeshSubset& subset = mesh.m_subsets[j];

			if (subset.nFirstVertId >= nFirstVertexId)
			{
				subset.nFirstVertId -= nNumVertices;
			}

			if (subset.nFirstIndexId >= nFirstIndexId)
			{
				subset.nFirstIndexId -= nNumIndices;
			}
		}

		// Fix indices.
		const int nNumTotalIndices = mesh.GetIndexCount();
		for (int j = 0; j < nNumTotalIndices; ++j)
		{
			if (mesh.m_pIndices[j] >= nFirstVertexId)
			{
				mesh.m_pIndices[j] -= nNumVertices;					
			}
		}		
	}

	{
		const char* err = "unknown problem";
		const bool bPostValidation = mesh.Validate(&err);

		if (bPreValidation && !bPostValidation)
		{
			RCLogError("Mesh is damaged by %s: '%s'. Contact an RC programmer.",  __FUNCTION__, err);
			return false;
		}
	}

	// Check if mesh is now empty so the whole node is just a physics proxy.
	if (!bCga && !strstr(pNodeCGF->properties,"other_rendermesh") && !strstr(pNodeCGF->properties,"notaproxy"))
	{
		bool bEmptyMesh = true;
		for (size_t i = 0; i < mesh.m_subsets.size(); ++i)
		{
			const SMeshSubset& subset = mesh.m_subsets[i];
			if (subset.nNumIndices > 0 && subset.nNumVerts > 0)
			{
				bEmptyMesh = false;
			}
		}

		if (bEmptyMesh)
		{
			// Force this node to be physics proxy.
			pNodeCGF->bPhysicsProxy = true;
			pNodeCGF->type = CNodeCGF::NODE_HELPER;
			pNodeCGF->helperType = HP_GEOMETRY;
			
			const string newName = string(CGF_NODE_NAME_PHYSICS_PROXY2 "_") + pNodeCGF->name;
			cry_strcpy(pNodeCGF->name, newName.c_str());
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CPhysicsInterface::RephysicalizeNode( CNodeCGF *pNode,CContentCGF *pCGF )
{
	if (!GetGeomManager())
	{
		return;
	}

	//if (pNode->nPhysicalizeFlags & CNodeCGF::ePhysicsalizeFlag_MeshNotNeeded)
	{
		//if (!CheckNodeBreakable(pNode))
		//	pNode->nPhysicalizeFlags |= CNodeCGF::ePhysicsalizeFlag_NoBreaking;
		// Not needed, this is a fully physicalized mesh.
		//return;
	}

	{
		bool bHavePhysicsData = false;
		for (int nPhysGeomType = 0; nPhysGeomType < CRY_ARRAY_COUNT(pNode->physicalGeomData); ++nPhysGeomType)
		{
			if (!pNode->physicalGeomData[nPhysGeomType].empty())
			{
				bHavePhysicsData = true;
				break;
			}
		}
		if (!bHavePhysicsData)
		{
			return;
		}
	}

	std::vector<char> faceMaterials;
	if (pNode->pMesh)
	{
		const CMesh& mesh = *pNode->pMesh;
		// Fill faces material array.
		faceMaterials.resize(mesh.GetIndexCount() / 3);
		if (!faceMaterials.empty())
		{
			for (int m = 0; m < mesh.GetSubSetCount(); m++)
			{
				char* const pFaceMats = &faceMaterials[0];
				const SMeshSubset& subset = mesh.m_subsets[m];
				for (int nFaceIndex = subset.nFirstIndexId / 3, last = subset.nFirstIndexId / 3 + subset.nNumIndices / 3; nFaceIndex < last; ++nFaceIndex)
				{
					pFaceMats[nFaceIndex] = subset.nMatID;
				}
			}
		}
	}

	for (int nPhysGeomType = 0; nPhysGeomType < CRY_ARRAY_COUNT(pNode->physicalGeomData); ++nPhysGeomType)
	{
		DynArray<char> &physData = pNode->physicalGeomData[nPhysGeomType];
		if (physData.empty())
		{
			continue;
		}

		char *pFaceMats = 0;
		if (!faceMaterials.empty())
		{
			pFaceMats = &faceMaterials[0];
		}

		phys_geometry *pPhysGeom = 0;

		{
			// We have physical geometry for this node.
			CMemStream stmIn( &physData.front(), physData.size(), true );
			if (pNode->pMesh)
			{
				CMesh &mesh = *pNode->pMesh;
				assert(mesh.m_pPositionsF16 == 0);
				// Load using mesh information.
				pPhysGeom = GetGeomManager()->LoadPhysGeometry(stmIn,mesh.m_pPositions,mesh.m_pIndices,pFaceMats);
			}
			else
			{
				// Load only using physics saved data.
				pPhysGeom = GetGeomManager()->LoadPhysGeometry(stmIn,0,0,0);
			}
		}

		if (!pPhysGeom)
		{
			RCLogError( "Bad physical data in CGF node, Physicalization Failed, (%s)" );
			return;
		}

		if (nPhysGeomType==0 && !CheckNodeBreakable(pNode,pPhysGeom->pGeom))
		{
			pNode->nPhysicalizeFlags |= CNodeCGF::ePhysicsalizeFlag_NoBreaking;
		}

		if (pPhysGeom->pGeom->GetType() == GEOM_TRIMESH)
		{
			mesh_data *pmd = (mesh_data*)pPhysGeom->pGeom->GetData();

			if (pmd->flags & mesh_full_serialization)
			{
				// Already fully serialized.
				GetGeomManager()->UnregisterGeometry(pPhysGeom);
				return;
			}

			pmd->flags |= mesh_full_serialization;

			// Save it again, back to the node.
			CMemStream stm(false);
			GetGeomManager()->SavePhysGeometry( stm,pPhysGeom );

			// Add physicalized data to the node.
			physData.resize(stm.GetUsedSize());
			memcpy( &(physData[0]),stm.GetBuf(),stm.GetUsedSize() );
			
			// We saved complete physics mesh.
			pNode->nPhysicalizeFlags |= CNodeCGF::ePhysicsalizeFlag_MeshNotNeeded;
		}

		GetGeomManager()->UnregisterGeometry(pPhysGeom);		
	}
}
