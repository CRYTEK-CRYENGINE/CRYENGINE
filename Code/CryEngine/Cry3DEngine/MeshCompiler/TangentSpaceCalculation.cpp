// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TangentSpaceCalculation.h"
#include "../../SDKs/TangentBasisComputation/mikkelsen/mikktspace.h"

struct SMikkVertex
{
	Vec3 pos, norm, tang, bitang;
	Vec2 magST;
	Vec2 texc;
};

struct SMikkFace
{
	int vertexOffset;
	int nrOriginalFace;
};

struct SMikkMesh
{
	std::vector<SMikkVertex> mikkVerts;
	std::vector<SMikkFace>   mikkFaces;
	int                      mikkNumFaces;
};

CTangentSpaceCalculation::CBase33::CBase33()
{
}

CTangentSpaceCalculation::CBase33::CBase33(const Vec3& Uval, const Vec3& Vval, const Vec3& Nval)
{
	u = Uval;
	v = Vval;
	n = Nval;
}

bool CTangentSpaceCalculation::CVec3PredicateLess::operator()(const Vec3& first, const Vec3& second) const
{
	if (first.x < second.x)
		return true;

	if (first.x > second.x)
		return false;

	if (first.y < second.y)
		return true;

	if (first.y > second.y)
		return false;

	return (first.z < second.z);
}

bool CTangentSpaceCalculation::CBase33PredicateLess::operator()(const CBase33& first, const CBase33& second) const
{
	if (first.n.x < second.n.x)
		return true;

	if (first.n.x > second.n.x)
		return false;

	if (first.n.y < second.n.y)
		return true;

	if (first.n.y > second.n.y)
		return false;

	if (first.n.z < second.n.z)
		return true;

	if (first.n.z > second.n.z)
		return false;

	if (first.u.x < second.u.x)
		return true;

	if (first.u.x > second.u.x)
		return false;

	if (first.u.y < second.u.y)
		return true;

	if (first.u.y > second.u.y)
		return false;

	if (first.u.z < second.u.z)
		return true;

	if (first.u.z > second.u.z)
		return false;

	if (first.v.x < second.v.x)
		return true;

	if (first.v.x > second.v.x)
		return false;

	if (first.v.y < second.v.y)
		return true;

	if (first.v.y > second.v.y)
		return false;

	return first.v.z < second.v.z;
}

bool CTangentSpaceCalculation::CBaseIndexOrder::operator()(const CBaseIndex& a, const CBaseIndex& b) const
{
	// first sort by position
	if (a.m_posIndex < b.m_posIndex) return true;
	if (a.m_posIndex > b.m_posIndex) return false;

	// then by normal
	if (a.m_normIndex < b.m_normIndex) return true;
	if (a.m_normIndex > b.m_normIndex) return false;

	return false;
}

float CTangentSpaceCalculation::CalcAngleBetween(const Vec3& invA, const Vec3& invB)
{
	double LengthQ = sqrt(invA.len2() * invB.len2());

	// to prevent division by zero
	if (LengthQ < 0.00000001)
		LengthQ = 0.00000001;

	double f = invA.Dot(invB) / LengthQ;

	// cosf is not available on every platform
	float fRet = (float)acos_tpl(f);

	return fRet;
}

void CTangentSpaceCalculation::DebugMesh(const ITriangleInputProxy& proxy) const
{
	uint32 dwTriCount = proxy.GetTriangleCount();

	// search for polygons that use the same indices (input data problems)
	for (uint32 a = 0; a < dwTriCount; a++)
	{
		uint32 dwAPos[3], dwANorm[3], dwAUV[3];

		proxy.GetTriangleIndices(a, dwAPos, dwANorm, dwAUV);

		for (uint32 b = a + 1; b < dwTriCount; b++)
		{
			uint32 dwBPos[3], dwBNorm[3], dwBUV[3];

			proxy.GetTriangleIndices(b, dwBPos, dwBNorm, dwBUV);

			assert(!(dwAPos[0] == dwBPos[0] && dwAPos[1] == dwBPos[1] && dwAPos[2] == dwBPos[2]));
			assert(!(dwAPos[1] == dwBPos[0] && dwAPos[2] == dwBPos[1] && dwAPos[0] == dwBPos[2]));
			assert(!(dwAPos[2] == dwBPos[0] && dwAPos[0] == dwBPos[1] && dwAPos[1] == dwBPos[2]));

			assert(!(dwAPos[1] == dwBPos[0] && dwAPos[0] == dwBPos[1] && dwAPos[2] == dwBPos[2]));
			assert(!(dwAPos[2] == dwBPos[0] && dwAPos[1] == dwBPos[1] && dwAPos[0] == dwBPos[2]));
			assert(!(dwAPos[0] == dwBPos[0] && dwAPos[2] == dwBPos[1] && dwAPos[1] == dwBPos[2]));
		}
	}
}

Vec3 CTangentSpaceCalculation::Rotate(const Vec3& vFrom, const Vec3& vTo, const Vec3& vInput)
{
	// no mesh is perfect
	//		assert(IsNormalized(vFrom));
	// no mesh is perfect
	//		assert(IsNormalized(vTo));

	// rotation axis
	Vec3 vRotAxis = vFrom.cross(vTo);

	float fSin = vRotAxis.len();
	float fCos = vFrom.Dot(vTo);

	// no rotation
	if (fSin < 0.00001f)
		return vInput;

	// normalize
	vRotAxis = vRotAxis * (1.0f / fSin);

	// perpendicular to vRotAxis and vFrom90deg
	Vec3 vFrom90deg = (vRotAxis.cross(vFrom)).normalize();

	// Base is vFrom,vFrom90deg,vRotAxis
	float fXInPlane = vFrom.Dot(vInput);
	float fYInPlane = vFrom90deg.Dot(vInput);

	Vec3 a = vFrom * (fXInPlane * fCos - fYInPlane * fSin);
	Vec3 b = vFrom90deg * (fXInPlane * fSin + fYInPlane * fCos);
	Vec3 c = vRotAxis * (vRotAxis.Dot(vInput));

	return a + b + c;
}

eCalculateTangentSpaceErrorCode CTangentSpaceCalculation::CalculateTangentSpace(
  const ITriangleInputProxy& inInput,
  const bool bUseCustomNormals,
  const bool bIgnoreDegeneracies,
  string& errorMessage)
{
	if (bUseCustomNormals)
	{
		return CalculateTangentSpaceMikk(inInput, errorMessage);
	}

	uint32 dwTriCount = inInput.GetTriangleCount();

	// not a number in texture coordinates
	bool bTextureCoordinatesBroken = false;

	// clear result
	m_baseVectors.clear();
	m_trianglesBaseAssigment.clear();
	m_trianglesBaseAssigment.reserve(dwTriCount);
	assert(m_baseVectors.empty());
	assert(m_trianglesBaseAssigment.empty());

	// second=index into m_BaseVectors, generated output data
	std::multimap<CBaseIndex, uint32, CBaseIndexOrder> mBaseMap;
	// base vectors per triangle
	std::vector<CBase33> vTriangleBase;

	// calculate the base vectors per triangle -------------------------------------------
	{
		eCalculateTangentSpaceErrorCode errorCode = CALCULATE_TANGENT_SPACE_NO_ERRORS;
		for (uint32 i = 0; i < dwTriCount; i++)
		{
			// get data from caller ---------------------------
			uint32 dwPos[3], dwNorm[3], dwUV[3];

			inInput.GetTriangleIndices(i, dwPos, dwNorm, dwUV);

			Vec3 vPos[3];
			Vec2 vUV[3];

			for (int e = 0; e < 3; e++)
			{
				inInput.GetPos(dwPos[e], vPos[e]);
				inInput.GetUV(dwUV[e], vUV[e]);
			}

			// calculate tangent vectors ---------------------------

			Vec3 vA = vPos[1] - vPos[0];
			Vec3 vB = vPos[2] - vPos[0];
			Vec3 vC = vPos[2] - vPos[1];

			if (!bIgnoreDegeneracies)
			{
				if (vA.IsZero())
				{
					//vert 0 and 1 have the same coordinates
					errorMessage.Format("Vertices 0 and 1 have the same coordinate: (%f : %f : %f)\n", vPos[0].x, vPos[0].y, vPos[0].z);
					errorCode = VERTICES_SHARING_COORDINATES;
					continue;
				}

				if (vB.IsZero())
				{
					//vert 2 and 0 have the same coordinates
					errorMessage.Format("Vertices 2 and 0 have the same coordinate: (%f : %f : %f)\n", vPos[0].x, vPos[0].y, vPos[0].z);
					errorCode = VERTICES_SHARING_COORDINATES;
					continue;
				}

				if (vC.IsZero())
				{
					//vert 2 and 1 have the same coordinates
					errorMessage.Format("Vertices 2 and 1 have the same coordinate: (%f : %f : %f)\n", vPos[1].x, vPos[1].y, vPos[1].z);
					errorCode = VERTICES_SHARING_COORDINATES;
					continue;
				}
			}

			float fDeltaU1 = vUV[1].x - vUV[0].x;
			float fDeltaU2 = vUV[2].x - vUV[0].x;
			float fDeltaV1 = vUV[1].y - vUV[0].y;
			float fDeltaV2 = vUV[2].y - vUV[0].y;

			float div = (fDeltaU1 * fDeltaV2 - fDeltaU2 * fDeltaV1);

			if (_isnan(div))
			{
				errorMessage.Format("Vertices 0,1,2 have broken texture coordinates v0:(%f : %f : %f) v1:(%f : %f : %f) v2:(%f : %f : %f)\n", vPos[0].x, vPos[0].y, vPos[0].z, vPos[1].x, vPos[1].y, vPos[1].z, vPos[2].x, vPos[2].y, vPos[2].z);
				bTextureCoordinatesBroken = true;
				div = 0.0f;
			}

			Vec3 vU, vV, vN = (vA.cross(vB)).normalize();

			if (div != 0.0)
			{
				//	2D triangle area = (u1*v2-u2*v1)/2
				float a = fDeltaV2;           // /div was removed - no required because of normalize()
				float b = -fDeltaV1;
				float c = -fDeltaU2;
				float d = fDeltaU1;

				// /fAreaMul2*fAreaMul2 was optimized away -> small triangles in UV should contribute less and
				// less artifacts (no divide and multiply)
				vU = (vA * a + vB * b) * fsgnf(div);
				vV = (vA * c + vB * d) * fsgnf(div);
			}
			else
			{
				vU = Vec3(1, 0, 0);
				vV = Vec3(0, 1, 0);
			}

			vTriangleBase.push_back(CBase33(vU, vV, vN));
		}
		if (errorCode != CALCULATE_TANGENT_SPACE_NO_ERRORS)
		{
			return errorCode;
		}
	}

	// distribute the normals to the vertices
	{
		// we create a new tangent base for every vertex index that has a different normal (later we split further for mirrored use)
		// and sum the base vectors (weighted by angle and mirrored if necessary)
		for (uint32 i = 0; i < dwTriCount; i++)
		{
			uint32 e;

			// get data from caller ---------------------------
			uint32 dwPos[3], dwNorm[3], dwUV[3];

			inInput.GetTriangleIndices(i, dwPos, dwNorm, dwUV);
			CBase33 TriBase = vTriangleBase[i];
			Vec3 vPos[3];

			for (e = 0; e < 3; e++)
			{
				inInput.GetPos(dwPos[e], vPos[e]);
			}

			// for each triangle vertex
			for (e = 0; e < 3; e++)
			{
				// weight by angle to fix the L-Shape problem
				float fWeight = CalcAngleBetween(vPos[(e + 2) % 3] - vPos[e], vPos[(e + 1) % 3] - vPos[e]);

				if (fWeight <= 0.0f)
					fWeight = 0.0001f;

				AddNormal2Base(mBaseMap, dwPos[e], dwNorm[e], TriBase.n * fWeight);
			}
		}
	}

	// distribute the uv vectors to the vertices
	{
		// we create a new tangent base for every vertex index that has a different normal
		// if the base vectors does'nt fit we split as well
		for (uint32 i = 0; i < dwTriCount; i++)
		{
			uint32 e;

			// get data from caller ---------------------------
			uint32 dwPos[3], dwNorm[3], dwUV[3];

			CTriBaseIndex Indx;
			inInput.GetTriangleIndices(i, dwPos, dwNorm, dwUV);
			CBase33 TriBase = vTriangleBase[i];
			Vec3 vPos[3];

			for (e = 0; e < 3; e++)
			{
				inInput.GetPos(dwPos[e], vPos[e]);
			}

			// for each triangle vertex
			for (e = 0; e < 3; e++)
			{
				// weight by angle to fix the L-Shape problem
				float fWeight = CalcAngleBetween(vPos[(e + 2) % 3] - vPos[e], vPos[(e + 1) % 3] - vPos[e]);

				Indx.p[e] = AddUV2Base(mBaseMap, dwPos[e], dwNorm[e], TriBase.u * fWeight, TriBase.v * fWeight, TriBase.n.normalize(), bIgnoreDegeneracies);
			}

			m_trianglesBaseAssigment.push_back(Indx);
		}
	}

	// adjust the base vectors per vertex -------------------------------------------
	{
		std::vector<CBase33>::iterator it;

		for (it = m_baseVectors.begin(); it != m_baseVectors.end(); ++it)
		{
			CBase33& ref = (*it);

			// rotate u and v in n plane
			{
				Vec3 vUout, vVout, vNout;

				vNout = ref.n;
				vNout.normalize();

				// project u in n plane
				// project v in n plane
				vUout = ref.u - vNout * (vNout.Dot(ref.u));
				vVout = ref.v - vNout * (vNout.Dot(ref.v));

				ref.u = vUout;
				ref.u.normalize();

				ref.v = vVout;
				ref.v.normalize();

				ref.n = vNout;

				//assert(ref.u.x>=-1 && ref.u.x<=1);
				//assert(ref.u.y>=-1 && ref.u.y<=1);
				//assert(ref.u.z>=-1 && ref.u.z<=1);
				//assert(ref.v.x>=-1 && ref.v.x<=1);
				//assert(ref.v.y>=-1 && ref.v.y<=1);
				//assert(ref.v.z>=-1 && ref.v.z<=1);
				//assert(ref.n.x>=-1 && ref.n.x<=1);
				//assert(ref.n.y>=-1 && ref.n.y<=1);
				//assert(ref.n.z>=-1 && ref.n.z<=1);
			}
		}
	}

	return bTextureCoordinatesBroken ? BROKEN_TEXTURE_COORDINATES : CALCULATE_TANGENT_SPACE_NO_ERRORS;
}

uint32 CTangentSpaceCalculation::AddUV2Base(std::multimap<CBaseIndex, uint32, CBaseIndexOrder>& inMap,
                                            const uint32 indwPosNo, const uint32 indwNormNo, const Vec3& inU, const Vec3& inV, const Vec3& inNormN,
                                            bool bIgnoreDegeneracies)
{
	CBaseIndex Indx;

	Indx.m_posIndex = indwPosNo;
	Indx.m_normIndex = indwNormNo;

	std::multimap<CBaseIndex, uint32, CBaseIndexOrder>::iterator iFind, iFindEnd;

	iFind = inMap.lower_bound(Indx);

	assert(iFind != inMap.end());

	Vec3 vNormal = m_baseVectors[(*iFind).second].n;

	iFindEnd = inMap.upper_bound(Indx);

	uint32 dwBaseUVIndex = 0xffffffff;                          // init with not found

	bool bParity = inU.cross(inV).Dot(inNormN) > 0.0f;

	for (; iFind != iFindEnd; ++iFind)
	{
		CBase33& refFound = m_baseVectors[(*iFind).second];

		if (!refFound.u.IsZero())
		{
			bool bParityRef = refFound.u.cross(refFound.v).Dot(refFound.n) > 0.0f;
			bool bParityCheck = (bParityRef == bParity);

			if (!bParityCheck) continue;

			//		bool bHalfAngleCheck=normalize(inU+inV) * normalize(refFound.u+refFound.v) > 0.0f;
			Vec3 normRefFound = refFound.n;
			normRefFound.normalize();

			Vec3 uvRefSum = refFound.u + refFound.v;
			uvRefSum.normalize();

			Vec3 vRotHalf = Rotate(normRefFound, inNormN, uvRefSum);

			Vec3 uvInSum = inU + inV;
			uvInSum.normalize();
			bool bHalfAngleCheck = uvInSum.Dot(vRotHalf) > 0.0f;
			//		bool bHalfAngleCheck=normalize(normalize(inU)+normalize(inV)) * normalize(normalize(refFound.u)+normalize(refFound.v)) > 0.0f;

			if (!bHalfAngleCheck) continue;
		}

		dwBaseUVIndex = (*iFind).second;
		break;
	}

	// not found
	if (dwBaseUVIndex == 0xffffffff)
	{
		// otherwise create a new base
		CBase33 Base(Vec3(0, 0, 0), Vec3(0, 0, 0), vNormal);

		dwBaseUVIndex = m_baseVectors.size();

		inMap.insert(std::pair<CBaseIndex, uint32>(Indx, dwBaseUVIndex));
		m_baseVectors.push_back(Base);
	}

	CBase33& refBaseUV = m_baseVectors[dwBaseUVIndex];

	refBaseUV.u = refBaseUV.u + inU;
	refBaseUV.v = refBaseUV.v + inV;

	if (!bIgnoreDegeneracies)
	{
		//no mesh is perfect
		if (inU.x != 0.0f || inU.y != 0.0f || inU.z != 0.0f)
			assert(refBaseUV.u.x != 0.0f || refBaseUV.u.y != 0.0f || refBaseUV.u.z != 0.0f);
		// no mesh is perfect
		if (inV.x != 0.0f || inV.y != 0.0f || inV.z != 0.0f)
			assert(refBaseUV.v.x != 0.0f || refBaseUV.v.y != 0.0f || refBaseUV.v.z != 0.0f);
	}

	return dwBaseUVIndex;
}

void CTangentSpaceCalculation::AddNormal2Base(std::multimap<CBaseIndex, uint32, CBaseIndexOrder>& inMap, const uint32 indwPosNo, const uint32 indwNormNo, const Vec3& inNormal)
{
	CBaseIndex Indx;

	Indx.m_posIndex = indwPosNo;
	Indx.m_normIndex = indwNormNo;

	std::multimap<CBaseIndex, uint32, CBaseIndexOrder>::iterator iFind = inMap.find(Indx);

	uint32 dwBaseNIndex;

	if (iFind != inMap.end())
	{
		dwBaseNIndex = (*iFind).second;
	}
	else
	{
		CBase33 Base(Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(0, 0, 0));

		dwBaseNIndex = m_baseVectors.size();
		inMap.insert(std::pair<CBaseIndex, uint32>(Indx, dwBaseNIndex));
		m_baseVectors.push_back(Base);
	}

	CBase33& refBaseN = m_baseVectors[dwBaseNIndex];

	refBaseN.n = refBaseN.n + inNormal;
}

void CTangentSpaceCalculation::GetBase(const uint32 indwPos, float* outU, float* outV, float* outN)
{
	CBase33& base = m_baseVectors[indwPos];

	outU[0] = base.u.x;
	outV[0] = base.v.x;
	outN[0] = base.n.x;
	outU[1] = base.u.y;
	outV[1] = base.v.y;
	outN[1] = base.n.y;
	outU[2] = base.u.z;
	outV[2] = base.v.z;
	outN[2] = base.n.z;
}

void CTangentSpaceCalculation::GetTriangleBaseIndices(const uint32 indwTriNo, uint32 outdwBase[3])
{
	assert(indwTriNo < m_trianglesBaseAssigment.size());
	CTriBaseIndex& indx = m_trianglesBaseAssigment[indwTriNo];

	for (uint32 i = 0; i < 3; i++)
		outdwBase[i] = indx.p[i];
}

size_t CTangentSpaceCalculation::GetBaseCount()
{
	return m_baseVectors.size();
}

static int MikkGetNumFaces(const SMikkTSpaceContext* pContext)
{
	SMikkMesh* mikkMesh = (SMikkMesh*)pContext->m_pUserData;
	return mikkMesh->mikkNumFaces;
}

static int MikkGetNumVerticesOfFace(const SMikkTSpaceContext* pContext, const int iFace)
{
	return 3;
}

static void MikkGetPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
{
	SMikkMesh* mikkMesh = (SMikkMesh*)pContext->m_pUserData;
	const SMikkFace& face = mikkMesh->mikkFaces[iFace];

	const Vec3& pos = mikkMesh->mikkVerts[face.vertexOffset + iVert].pos;
	fvPosOut[0] = pos.x;
	fvPosOut[1] = pos.y;
	fvPosOut[2] = pos.z;
}

static void MikkGetNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert)
{
	SMikkMesh* mikkMesh = (SMikkMesh*)pContext->m_pUserData;
	const SMikkFace& face = mikkMesh->mikkFaces[iFace];

	const Vec3& normal = mikkMesh->mikkVerts[face.vertexOffset + iVert].norm;
	fvNormOut[0] = normal.x;
	fvNormOut[1] = normal.y;
	fvNormOut[2] = normal.z;
}

static void MikkGetTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert)
{
	SMikkMesh* mikkMesh = (SMikkMesh*)pContext->m_pUserData;
	const SMikkFace& face = mikkMesh->mikkFaces[iFace];

	const Vec2& tan = mikkMesh->mikkVerts[face.vertexOffset + iVert].texc;
	fvTexcOut[0] = tan.x;
	fvTexcOut[1] = tan.y;
}

static void MikkSetTSpace(const SMikkTSpaceContext* pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT, const tbool bIsOrientationPreserving, const int iFace, const int iVert)
{
	SMikkMesh* mikkMesh = (SMikkMesh*)pContext->m_pUserData;
	const SMikkFace& face = mikkMesh->mikkFaces[iFace];
	const int index = face.vertexOffset + iVert;

	mikkMesh->mikkVerts[index].tang = Vec3(fvTangent[0], fvTangent[1], fvTangent[2]);
	mikkMesh->mikkVerts[index].bitang = Vec3(fvBiTangent[0], fvBiTangent[1], fvBiTangent[2]);
	mikkMesh->mikkVerts[index].magST.x = fMagS;
	mikkMesh->mikkVerts[index].magST.y = fMagT;
}

eCalculateTangentSpaceErrorCode CTangentSpaceCalculation::CalculateTangentSpaceMikk(const ITriangleInputProxy& proxy, string& errorMessage)
{
	const uint32 numFaces = proxy.GetTriangleCount();

	// prepare the working mesh for mikkelsen algorithm
	// when custom normals are specified, we'll use them
	SMikkMesh mikkMesh;
	mikkMesh.mikkNumFaces = numFaces;
	mikkMesh.mikkVerts.resize(numFaces * 3);
	mikkMesh.mikkFaces.resize(numFaces);

	for (uint32 f = 0; f < numFaces; ++f)
	{
		uint32 outdwPos[3];
		uint32 outdwNorm[3];
		uint32 outdwUV[3];
		proxy.GetTriangleIndices(f, outdwPos, outdwNorm, outdwUV);

		mikkMesh.mikkFaces[f].vertexOffset = f * 3;
		mikkMesh.mikkFaces[f].nrOriginalFace = f;

		for (uint32 vId = 0; vId < 3; ++vId)
		{
			SMikkVertex& vert(mikkMesh.mikkVerts[mikkMesh.mikkFaces[f].vertexOffset + vId]);

			proxy.GetPos(outdwPos[vId], vert.pos);
			proxy.GetNorm(f, vId, vert.norm);
			proxy.GetUV(outdwUV[vId], vert.texc);

			vert.tang = Vec3(1.0f, 0.0f, 0.0f);
			vert.bitang = Vec3(0.0f, 1.0f, 0.0f);
		}
	}

	// prepare mikkelsen interface
	SMikkTSpaceInterface mikkInterface;
	memset(&mikkInterface, 0, sizeof(SMikkTSpaceInterface));

	mikkInterface.m_getNumFaces = MikkGetNumFaces;
	mikkInterface.m_getNumVerticesOfFace = MikkGetNumVerticesOfFace;
	mikkInterface.m_getPosition = MikkGetPosition;
	mikkInterface.m_getNormal = MikkGetNormal;
	mikkInterface.m_getTexCoord = MikkGetTexCoord;
	mikkInterface.m_setTSpace = MikkSetTSpace;

	SMikkTSpaceContext mikkContext;
	memset(&mikkContext, 0, sizeof(SMikkTSpaceContext));
	mikkContext.m_pUserData = &mikkMesh;
	mikkContext.m_pInterface = &mikkInterface;

	// generate tangent basis
	bool res = genTangSpaceDefault(&mikkContext) != 0;
	if (!res)
	{
		errorMessage = "Failed to allocate memory for Mikkelsen Tangent Basis algorithm.";
		return MEMORY_ALLOCATION_FAILED;
	}

	m_baseVectors.clear();
	m_trianglesBaseAssigment.clear();
	m_trianglesBaseAssigment.resize(proxy.GetTriangleCount());

	std::map<CBase33, int, CBase33PredicateLess> uniqueBaseVectors;
	std::map<CBase33, int, CBase33PredicateLess>::const_iterator it;

	// remove tangent basis duplicates and add them to the mesh
	for (int f = 0; f < mikkMesh.mikkNumFaces; ++f)
	{
		const SMikkFace& face = mikkMesh.mikkFaces[f];

		CTriBaseIndex tbi;
		for (int ii = 0; ii < 3; ++ii)
		{
			const int index = face.vertexOffset + ii;
			const SMikkVertex& vert = mikkMesh.mikkVerts[index];

			CBase33 base;
			base.u = vert.tang;
			base.v = vert.bitang;

			float fNorm[3];
			MikkGetNormal(&mikkContext, &fNorm[0], face.nrOriginalFace, ii);

			base.n.x = fNorm[0];
			base.n.y = fNorm[1];
			base.n.z = fNorm[2];

			int val;
			it = uniqueBaseVectors.find(base);
			if (it != uniqueBaseVectors.end())
				val = it->second;
			else
			{
				val = m_baseVectors.size();
				m_baseVectors.push_back(base);
				uniqueBaseVectors[base] = val;
			}
			tbi.p[ii] = val;
		}
		m_trianglesBaseAssigment[face.nrOriginalFace] = tbi;
	}

	return CALCULATE_TANGENT_SPACE_NO_ERRORS;
}
