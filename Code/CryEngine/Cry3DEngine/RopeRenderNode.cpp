// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RopeRenderNode.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "MatMan.h"
#include <CryAudio/IObject.h>

#include <CryEntitySystem/IEntity.h>

#pragma warning(push)
#pragma warning(disable: 4244)

class TubeSurface : public _i_reference_target_t
{

public:
	Matrix34 m_worldTM;

	// If rkUpVector is not the zero vector, it will be used as 'up' in the frame calculations.  If it is the zero
	// vector, the Frenet frame will be used.  If bWantColors is 'true',
	// the vertex colors are allocated and set to black.  The application
	// needs to assign colors as needed.  If either of pkTextureMin or
	// pkTextureMax is not null, both must be not null.  In this case,
	// texture coordinates are generated for the surface.
	void GenerateSurface(spline::CatmullRomSpline<Vec3>* pSpline, float fRadius, bool bClosed,
	                     const Vec3& rkUpVector, int iMedialSamples, int iSliceSamples,
	                     bool bWantNormals, bool bWantColors, bool bSampleByArcLength,
	                     bool bInsideView, const Vec2* pkTextureMin, const Vec2* pkTextureMax);

	TubeSurface();
	~TubeSurface();

	// member access
	Vec3&       UpVector();
	const Vec3& GetUpVector() const;
	int         GetSliceSamples() const;

	// Generate vertices for the end slices.  These are useful when you build
	// an open tube and want to attach meshes at the ends to close the tube.
	// The input array must have size (at least) S+1 where S is the number
	// returned by GetSliceSamples.  Function GetTMinSlice is used to access
	// the slice corresponding to the medial curve evaluated at its domain
	// minimum, tmin.  Function GetTMaxSlice accesses the slice for the
	// domain maximum, tmax.  If the curve is closed, the slices are the same.
	void GetTMinSlice(Vec3* akSlice);
	void GetTMaxSlice(Vec3* akSlice);

	// If the medial curve is modified, for example if it is control point
	// based and the control points are modified, then you should call this
	// update function to recompute the tube surface geometry.
	void UpdateSurface();

	//////////////////////////////////////////////////////////////////////////
	// tessellation support
	int  Index(int iS, int iM) { return iS + (m_iSliceSamples + 1) * iM; };
	void ComputeSinCos();
	void ComputeVertices(Vec3* akVertex);
	void ComputeNormals();
	void ComputeTextures(const Vec2& rkTextureMin,
	                     const Vec2& rkTextureMax, Vec2* akTexture);
	void ComputeConnectivity(int& riTQuantity, vtx_idx*& raiConnect, bool bInsideView);

	float                           m_fRadius;
	int                             m_iMedialSamples, m_iSliceSamples;
	Vec3                            m_kUpVector;
	float*                          m_afSin;
	float*                          m_afCos;
	bool                            m_bClosed, m_bSampleByArcLength;
	bool                            m_bCap;

	int                             nAllocVerts;
	int                             nAllocIndices;
	int                             nAllocSinCos;

	int                             iVQuantity;
	int                             iNumIndices;

	int                             iTQuantity; // Num triangles (use iNumIndices instead).

	Vec3*                           m_akTangents;
	Vec3*                           m_akBitangents;

	Vec3*                           m_akVertex;
	Vec3*                           m_akNormals;
	Vec2*                           m_akTexture;
	vtx_idx*                        m_pIndices;
	spline::CatmullRomSpline<Vec3>* m_pSpline;
};

//////////////////////////////////////////////////////////////////////////
TubeSurface::TubeSurface()
{
	m_afSin = NULL;
	m_afCos = NULL;

	m_bCap = false;
	m_bClosed = false;
	m_bSampleByArcLength = false;
	m_fRadius = 0;
	m_iMedialSamples = 0;
	m_iSliceSamples = 0;

	nAllocVerts = 0;
	nAllocIndices = 0;
	nAllocSinCos = 0;

	iVQuantity = 0;
	iTQuantity = 0;
	iNumIndices = 0;

	m_pSpline = NULL;
	m_akTangents = NULL;
	m_akBitangents = NULL;

	m_akVertex = NULL;
	m_akNormals = NULL;
	m_akTexture = NULL;
	m_pIndices = NULL;
}

//----------------------------------------------------------------------------
void TubeSurface::GenerateSurface(spline::CatmullRomSpline<Vec3>* pSpline, float fRadius,
                                  bool bClosed, const Vec3& rkUpVector, int iMedialSamples,
                                  int iSliceSamples, bool bWantNormals, bool bWantColors,
                                  bool bSampleByArcLength, bool bInsideView, const Vec2* pkTextureMin, const Vec2* pkTextureMax)
{
	assert((pkTextureMin && pkTextureMax) || (!pkTextureMin && !pkTextureMax));

	m_pSpline = pSpline;
	m_fRadius = fRadius;
	m_kUpVector = rkUpVector;
	m_iMedialSamples = iMedialSamples;
	m_iSliceSamples = iSliceSamples;
	m_bClosed = bClosed;
	m_bSampleByArcLength = bSampleByArcLength;

	m_bCap = !m_bClosed;
	// compute the surface vertices
	if (m_bClosed)
		iVQuantity = (m_iSliceSamples + 1) * (m_iMedialSamples + 1);
	else
		iVQuantity = (m_iSliceSamples + 1) * m_iMedialSamples;

	bool bAllocVerts = false;
	if (iVQuantity > nAllocVerts || !m_akVertex)
	{
		bAllocVerts = true;
		nAllocVerts = iVQuantity;

		delete[] m_akVertex;
		m_akVertex = new Vec3[nAllocVerts];
	}

	ComputeSinCos();
	ComputeVertices(m_akVertex);

	// compute the surface normals
	if (bWantNormals)
	{
		if (bAllocVerts)
		{
			delete[] m_akNormals;
			delete[] m_akTangents;
			delete[] m_akBitangents;

			m_akNormals = new Vec3[iVQuantity];
			m_akTangents = new Vec3[iVQuantity];
			m_akBitangents = new Vec3[iVQuantity];
		}
		ComputeNormals();
	}

	// compute the surface textures coordinates
	if (pkTextureMin && pkTextureMax)
	{
		if (bAllocVerts)
		{
			delete[] m_akTexture;
			m_akTexture = new Vec2[iVQuantity];
		}

		ComputeTextures(*pkTextureMin, *pkTextureMax, m_akTexture);
	}

	// compute the surface triangle connectivity
	ComputeConnectivity(iTQuantity, m_pIndices, bInsideView);

	// create the triangle mesh for the tube surface
	//Reconstruct(iVQuantity,akVertex,akNormal,akColor,akTexture,iTQuantity,aiConnect);
}

//----------------------------------------------------------------------------
TubeSurface::~TubeSurface()
{
	delete[] m_pIndices;
	delete[] m_akTexture;
	delete[] m_akBitangents;
	delete[] m_akTangents;
	delete[] m_akNormals;
	delete[] m_akVertex;
	delete[] m_afSin;
	delete[] m_afCos;
}
//----------------------------------------------------------------------------
void TubeSurface::ComputeSinCos()
{
	// Compute slice vertex coefficients.  The first and last coefficients
	// are duplicated to allow a closed cross section that has two different
	// pairs of texture coordinates at the shared vertex.

	if (m_iSliceSamples + 1 != nAllocSinCos)
	{
		nAllocSinCos = m_iSliceSamples + 1;
		delete[]m_afSin;
		delete[]m_afCos;
		m_afSin = new float[nAllocSinCos];
		m_afCos = new float[nAllocSinCos];
	}

	PREFAST_ASSUME(m_iSliceSamples > 0 && m_iSliceSamples == nAllocSinCos - 1);

	float fInvSliceSamples = 1.0f / (float)m_iSliceSamples;
	for (int i = 0; i < m_iSliceSamples; i++)
	{
		float fAngle = gf_PI2 * fInvSliceSamples * i;
		m_afCos[i] = cosf(fAngle);
		m_afSin[i] = sinf(fAngle);
	}
	m_afSin[m_iSliceSamples] = m_afSin[0];
	m_afCos[m_iSliceSamples] = m_afCos[0];
}
//----------------------------------------------------------------------------
void TubeSurface::ComputeVertices(Vec3* akVertex)
{
	float fTMin = m_pSpline->GetRangeStart();
	float fTRange = m_pSpline->GetRangeEnd() - fTMin;

	// sampling by arc length requires the total length of the curve
	/*	float fTotalLength;
	   if ( m_bSampleByArcLength )
	    //fTotalLength = m_pkMedial->GetTotalLength();
	    fTotalLength = 0.0f;
	   else
	    fTotalLength = 0.0f;
	 */
	if (Cry3DEngineBase::GetCVars()->e_Ropes == 2)
	{
		for (int i = 0, npoints = m_pSpline->num_keys() - 1; i < npoints; i++)
		{
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(m_worldTM * m_pSpline->value(i), ColorB(0, 255, 0, 255), m_worldTM * m_pSpline->value(i + 1), ColorB(0, 255, 0, 255), 6);
		}
	}

	// vertex construction requires a normalized time (uses a division)
	float fDenom;
	if (m_bClosed)
		fDenom = 1.0f / (float)m_iMedialSamples;
	else
		fDenom = 1.0f / (float)(m_iMedialSamples - 1);

	Vec3 kT_Prev = Vec3(1.0f, 0, 0), kB = m_kUpVector;
	for (int iM = 0, iV = 0; iM < m_iMedialSamples; iM++)
	{
		float fT = fTMin + iM * fTRange * fDenom;

		float fRadius = m_fRadius;

		// compute frame (position P, tangent T, normal N, bitangent B)
		Vec3 kP, kP1, kT, kN;

		{
			// Always use 'up' vector N rather than curve normal.  You must
			// constrain the curve so that T and N are never parallel.  To
			// build the frame from this, let
			//     B = Cross(T,N)/Length(Cross(T,N))
			// and replace
			//     N = Cross(B,T)/Length(Cross(B,T)).

			//kP = m_pkMedial->GetPosition(fT);
			//kT = m_pkMedial->GetTangent(fT);
			if (iM < m_iMedialSamples - 1)
			{
				m_pSpline->interpolate(fT, kP);
				m_pSpline->interpolate(fT + 0.01f, kP1);
				kT = (kP1 - kP);
			}
			else
			{
				m_pSpline->interpolate(fT - 0.01f, kP1);
				m_pSpline->interpolate(fT, kP);
				kT = (kP - kP1);
			}

			if (!kT.IsZero())
				kT.NormalizeFast();
			else
			{
				// Take direction of last points.
				kT = kT_Prev;
			}
			kT_Prev = kT;
			//kT = m_pkMedial->GetTangent(fT);

			kB -= kT * (kB * kT);
			if (kB.len2() < sqr(0.001f))
				kB = kT.GetOrthogonal();
			//kB = kT.Cross(m_kUpVector);
			//if (kB.IsZero())
			//	kB = Vec3(1.0f,0,0);
			kB.NormalizeFast();
			kN = kB.Cross(kT);
			kN.NormalizeFast();
		}

		// compute slice vertices, duplication at end point as noted earlier
		int iSave = iV;
		for (int i = 0; i < m_iSliceSamples; i++)
		{
			akVertex[iV] = kP + fRadius * (m_afCos[i] * kN + m_afSin[i] * kB);

			if (Cry3DEngineBase::GetCVars()->e_Ropes == 2)
			{
				ColorB col = ColorB(255, 0, 0, 255);
				if (i == 0)
					col = ColorB(255, 0, 0, 255);
				else if (i == 1)
					col = ColorB(0, 255, 0, 255);
				else
					col = ColorB(0, 0, 255, 255);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawPoint(m_worldTM * akVertex[iV], col, 8);
			}

			iV++;
		}
		akVertex[iV] = akVertex[iSave];
		iV++;
	}

	if (m_bClosed)
	{
		for (int i = 0; i <= m_iSliceSamples; i++)
		{
			int i1 = Index(i, m_iMedialSamples);
			int i0 = Index(i, 0);
			akVertex[i1] = akVertex[i0];
		}
	}
}
//----------------------------------------------------------------------------
void TubeSurface::ComputeNormals()
{
	int iS, iSm, iSp, iM, iMm, iMp;
	Vec3 kDir0, kDir1;

	// interior normals (central differences)
	for (iM = 1; iM <= m_iMedialSamples - 2; iM++)
	{
		for (iS = 0; iS < m_iSliceSamples; iS++)
		{
			iSm = (iS > 0 ? iS - 1 : m_iSliceSamples - 1);
			iSp = iS + 1;
			iMm = iM - 1;
			iMp = iM + 1;

			kDir0 = m_akVertex[Index(iSm, iM)] - m_akVertex[Index(iSp, iM)];
			kDir1 = m_akVertex[Index(iS, iMm)] - m_akVertex[Index(iS, iMp)];

			if (kDir0.IsZero(1e-10f))
				kDir0 = Vec3(1, 0, 0);
			if (kDir1.IsZero(1e-10f))
				kDir1 = Vec3(0, 1, 0);

			Vec3 kN = kDir0.Cross(kDir1); // Normal
			Vec3 kT = kDir0;              // Tangent
			Vec3 kB = kDir1;              // Bitangent

			if (kN.IsZero(1e-10f))
				kN = Vec3(0, 0, 1);

			kN.NormalizeFast();
			kT.NormalizeFast();
			kB.NormalizeFast();

			int nIndex = Index(iS, iM);
			m_akNormals[nIndex] = kN;
			m_akTangents[nIndex] = kT;
			m_akBitangents[nIndex] = kB;
		}

		m_akNormals[Index(m_iSliceSamples, iM)] = m_akNormals[Index(0, iM)];
		m_akTangents[Index(m_iSliceSamples, iM)] = m_akTangents[Index(0, iM)];
		m_akBitangents[Index(m_iSliceSamples, iM)] = m_akBitangents[Index(0, iM)];
	}

	// boundary normals
	if (m_bClosed)
	{
		// central differences
		for (iS = 0; iS < m_iSliceSamples; iS++)
		{
			iSm = (iS > 0 ? iS - 1 : m_iSliceSamples - 1);
			iSp = iS + 1;

			// m = 0
			kDir0 = m_akVertex[Index(iSm, 0)] - m_akVertex[Index(iSp, 0)];
			kDir1 = m_akVertex[Index(iS, m_iMedialSamples - 1)] - m_akVertex[Index(iS, 1)];
			m_akNormals[iS] = kDir0.Cross(kDir1).GetNormalized();

			// m = max
			m_akNormals[Index(iS, m_iMedialSamples)] = m_akNormals[Index(iS, 0)];
		}
		m_akNormals[Index(m_iSliceSamples, 0)] = m_akNormals[Index(0, 0)];
		m_akNormals[Index(m_iSliceSamples, m_iMedialSamples)] = m_akNormals[Index(0, m_iMedialSamples)];
	}
	else
	{
		// one-sided finite differences

		// m = 0
		for (iS = 0; iS < m_iSliceSamples; iS++)
		{
			iSm = (iS > 0 ? iS - 1 : m_iSliceSamples - 1);
			iSp = iS + 1;

			kDir0 = m_akVertex[Index(iSm, 0)] - m_akVertex[Index(iSp, 0)];
			kDir1 = m_akVertex[Index(iS, 0)] - m_akVertex[Index(iS, 1)];
			//m_akNormals[Index(iS,0)] = kDir0.Cross(kDir1).GetNormalized();

			if (kDir0.IsZero())
				kDir0 = Vec3(1, 0, 0);
			if (kDir1.IsZero())
				kDir1 = Vec3(0, 1, 0);

			Vec3 kN = kDir0.Cross(kDir1); // Normal
			Vec3 kT = kDir0;              // Tangent
			Vec3 kB = kDir1;              // Bitangent

			if (kN.IsZero(1e-10f))
				kN = Vec3(0, 0, 1);

			kN.NormalizeFast();
			kT.NormalizeFast();
			kB.NormalizeFast();

			int nIndex = iS;
			m_akNormals[nIndex] = kN;
			m_akTangents[nIndex] = kT;
			m_akBitangents[nIndex] = kB;
		}
		m_akNormals[Index(m_iSliceSamples, 0)] = m_akNormals[Index(0, 0)];
		m_akTangents[Index(m_iSliceSamples, 0)] = m_akTangents[Index(0, 0)];
		m_akBitangents[Index(m_iSliceSamples, 0)] = m_akBitangents[Index(0, 0)];

		// m = max-1
		for (iS = 0; iS < m_iSliceSamples; iS++)
		{
			iSm = (iS > 0 ? iS - 1 : m_iSliceSamples - 1);
			iSp = iS + 1;
			kDir0 = m_akVertex[Index(iSm, m_iMedialSamples - 1)] - m_akVertex[Index(iSp, m_iMedialSamples - 1)];
			kDir1 = m_akVertex[Index(iS, m_iMedialSamples - 2)] - m_akVertex[Index(iS, m_iMedialSamples - 1)];
			//m_akNormals[iS] = kDir0.Cross(kDir1).GetNormalized();

			if (kDir0.IsZero())
				kDir0 = Vec3(1, 0, 0);
			if (kDir1.IsZero())
				kDir1 = Vec3(0, 1, 0);

			Vec3 kN = kDir0.Cross(kDir1); // Normal
			Vec3 kT = kDir0;              // Tangent
			Vec3 kB = kDir1;              // Bitangent

			if (kN.IsZero(1e-10f))
				kN = Vec3(0, 0, 1);

			kN.NormalizeFast();
			kT.NormalizeFast();
			kB.NormalizeFast();

			int nIndex = Index(iS, m_iMedialSamples - 1);
			m_akNormals[nIndex] = kN;
			m_akTangents[nIndex] = kT;
			m_akBitangents[nIndex] = kB;
		}
		m_akNormals[Index(m_iSliceSamples, m_iMedialSamples - 1)] = m_akNormals[Index(0, m_iMedialSamples - 1)];
		m_akTangents[Index(m_iSliceSamples, m_iMedialSamples - 1)] = m_akTangents[Index(0, m_iMedialSamples - 1)];
		m_akBitangents[Index(m_iSliceSamples, m_iMedialSamples - 1)] = m_akBitangents[Index(0, m_iMedialSamples - 1)];
	}
}

//----------------------------------------------------------------------------
void TubeSurface::ComputeTextures(const Vec2& rkTextureMin,
                                  const Vec2& rkTextureMax, Vec2* akTexture)
{
	Vec2 kTextureRange = rkTextureMax - rkTextureMin;
	int iMMax = (m_bClosed ? m_iMedialSamples : m_iMedialSamples - 1);
	for (int iM = 0, iV = 0; iM <= iMMax; iM++)
	{
		float fMRatio = ((float)iM) / ((float)iMMax);
		float fMValue = rkTextureMin.y + fMRatio * kTextureRange.y;
		for (int iS = 0; iS <= m_iSliceSamples; iS++)
		{
			float fSRatio = ((float)iS) / ((float)m_iSliceSamples);
			float fSValue = rkTextureMin.x + fSRatio * kTextureRange.x;
			akTexture[iV].x = fSValue;
			akTexture[iV].y = fMValue;
			iV++;
		}
	}
}
//----------------------------------------------------------------------------
void TubeSurface::ComputeConnectivity(int& riTQuantity, vtx_idx*& raiConnect, bool bInsideView)
{
	if (m_bClosed)
		riTQuantity = 2 * m_iSliceSamples * m_iMedialSamples;
	else
		riTQuantity = 2 * m_iSliceSamples * (m_iMedialSamples - 1);

	iNumIndices = riTQuantity * 3;

	if (m_bCap)
	{
		riTQuantity += ((m_iSliceSamples - 2) * 3) * 2;
		iNumIndices += ((m_iSliceSamples - 2) * 3) * 2;
	}

	if (nAllocIndices != iNumIndices)
	{
		if (raiConnect)
			delete[]raiConnect;
		nAllocIndices = iNumIndices;
		raiConnect = new vtx_idx[nAllocIndices];
	}

	int iM, iMStart, i0, i1, i2, i3, i;
	vtx_idx* aiConnect = raiConnect;
	for (iM = 0, iMStart = 0; iM < m_iMedialSamples - 1; iM++)
	{
		i0 = iMStart;
		i1 = i0 + 1;
		iMStart += m_iSliceSamples + 1;
		i2 = iMStart;
		i3 = i2 + 1;
		for (i = 0; i < m_iSliceSamples; i++, aiConnect += 6)
		{
			if (bInsideView)
			{
				aiConnect[0] = i0++;
				aiConnect[1] = i2;
				aiConnect[2] = i1;
				aiConnect[3] = i1++;
				aiConnect[4] = i2++;
				aiConnect[5] = i3++;
			}
			else  // outside view
			{
				aiConnect[0] = i0++;
				aiConnect[1] = i1;
				aiConnect[2] = i2;
				aiConnect[3] = i1++;
				aiConnect[4] = i3++;
				aiConnect[5] = i2++;

				if (Cry3DEngineBase::GetCVars()->e_Ropes == 2)
				{
					ColorB col = ColorB(100, 100, 0, 50);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(m_worldTM * m_akVertex[aiConnect[0]], col, m_worldTM * m_akVertex[aiConnect[1]], col, m_worldTM * m_akVertex[aiConnect[2]], col);
					col = ColorB(100, 0, 100, 50);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(m_worldTM * m_akVertex[aiConnect[3]], col, m_worldTM * m_akVertex[aiConnect[4]], col, m_worldTM * m_akVertex[aiConnect[5]], col);
				}
			}
		}
	}

	if (m_bClosed)
	{
		i0 = iMStart;
		i1 = i0 + 1;
		i2 = 0;
		i3 = i2 + 1;
		for (i = 0; i < m_iSliceSamples; i++, aiConnect += 6)
		{
			if (bInsideView)
			{
				aiConnect[0] = i0++;
				aiConnect[1] = i2;
				aiConnect[2] = i1;
				aiConnect[3] = i1++;
				aiConnect[4] = i2++;
				aiConnect[5] = i3++;
			}
			else  // outside view
			{
				aiConnect[0] = i0++;
				aiConnect[1] = i1;
				aiConnect[2] = i2;
				aiConnect[3] = i1++;
				aiConnect[4] = i3++;
				aiConnect[5] = i2++;
			}
		}
	}
	else if (m_bCap)
	{
		i0 = 0;
		i1 = 1;
		i2 = 2;
		// Begining Cap.
		for (i = 0; i < m_iSliceSamples - 2; i++, aiConnect += 3)
		{
			aiConnect[0] = i0;
			aiConnect[1] = i2++;
			aiConnect[2] = i1++;
		}
		i0 = iMStart;
		i1 = i0 + 1;
		i2 = i0 + 2;
		// Ending Cap.
		for (i = 0; i < m_iSliceSamples - 2; i++, aiConnect += 3)
		{
			aiConnect[0] = i0;
			aiConnect[1] = i1++;
			aiConnect[2] = i2++;
		}
	}
}

/*
   //----------------------------------------------------------------------------
   void TubeSurface::GetTMinSlice (Vec3* akSlice)
   {
   for (int i = 0; i <= m_iSliceSamples; i++)
    akSlice[i] = m_akVertex[i];
   }
   //----------------------------------------------------------------------------
   void TubeSurface::GetTMaxSlice (Vec3* akSlice)
   {
   int j = m_iVertexQuantity - m_iSliceSamples - 1;
   for (int i = 0; i <= m_iSliceSamples; i++, j++)
    akSlice[i] = m_akVertex[j];
   }
   //----------------------------------------------------------------------------
   void TubeSurface::UpdateSurface ()
   {
   ComputeVertices(m_akVertex);
   if ( m_akNormal )
    ComputeNormals(m_akVertex,m_akNormal);
   }
 */
//----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// Every Rope will keep a pointer to this and increase reference count.
//////////////////////////////////////////////////////////////////////////
class CRopeSurfaceCache : public _i_reference_target_t
{
public:
	static CRopeSurfaceCache* GetSurfaceCache();
	static void               ReleaseSurfaceCache(CRopeSurfaceCache* cache);

	static void               CleanupCaches();

public:
	struct GetLocalCache
	{
		GetLocalCache()
			: pCache(CRopeSurfaceCache::GetSurfaceCache())
		{}

		~GetLocalCache()
		{
			CRopeSurfaceCache::ReleaseSurfaceCache(pCache);
		}

		CRopeSurfaceCache* pCache;

	private:
		GetLocalCache(const GetLocalCache&);
		GetLocalCache& operator=(const GetLocalCache&);
	};

public:
	TubeSurface tubeSurface;

	CRopeSurfaceCache() {};
	~CRopeSurfaceCache() {}

private:
	static CryCriticalSectionNonRecursive  g_CacheLock;
	static std::vector<CRopeSurfaceCache*> g_CachePool;
};

CryCriticalSectionNonRecursive CRopeSurfaceCache::g_CacheLock;
std::vector<CRopeSurfaceCache*> CRopeSurfaceCache::g_CachePool;

CRopeSurfaceCache* CRopeSurfaceCache::GetSurfaceCache()
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(g_CacheLock);

	CRopeSurfaceCache* ret = NULL;

	if (g_CachePool.empty())
	{
		ret = new CRopeSurfaceCache;
	}
	else
	{
		ret = g_CachePool.back();
		g_CachePool.pop_back();
	}

	return ret;
}

void CRopeSurfaceCache::ReleaseSurfaceCache(CRopeSurfaceCache* cache)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(g_CacheLock);
	g_CachePool.push_back(cache);
}

void CRopeSurfaceCache::CleanupCaches()
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(g_CacheLock);

	for (std::vector<CRopeSurfaceCache*>::iterator it = g_CachePool.begin(), itEnd = g_CachePool.end(); it != itEnd; ++it)
		delete *it;

	stl::free_container(g_CachePool);
}

//////////////////////////////////////////////////////////////////////////
CRopeRenderNode::CRopeRenderNode()
	: m_pos(0, 0, 0)
	, m_localBounds(Vec3(-1, -1, -1), Vec3(1, 1, 1))
	, m_pRenderMesh(0)
	, m_pMaterial(0)
	, m_pPhysicalEntity(0)
	, m_nLinkedEndsMask(0)
	, m_pIAudioObject(nullptr)
{
	GetInstCount(GetRenderNodeType())++;

	m_pMaterial = GetMatMan()->GetDefaultMaterial();

	m_sName.Format("Rope_%p", this);

	memset(&m_params, 0, sizeof(m_params));
	m_params.nMaxIters = 650;
	m_params.maxTimeStep = 0.02f;
	m_params.stiffness = 10.0f;
	m_params.damping = 0.2f;
	m_params.sleepSpeed = 0.04f;

	m_bModified = true;
	m_bRopeCreatedInsideVisArea = false;

	m_worldTM.SetIdentity();
	m_InvWorldTM.SetIdentity();
	m_WSBBox.min = Vec3(0, 0, 0);
	m_WSBBox.max = Vec3(1, 1, 1);
	m_bNeedToReRegister = true;
	m_bStaticPhysics = false;

	gEnv->pPhysicalWorld->AddEventClient(EventPhysStateChange::id, &CRopeRenderNode::OnPhysStateChange, 1);
}

//////////////////////////////////////////////////////////////////////////
CRopeRenderNode::~CRopeRenderNode()
{
	GetInstCount(GetRenderNodeType())--;
	Dephysicalize();
	Get3DEngine()->FreeRenderNodeState(this);
	m_pRenderMesh = NULL;
	gEnv->pPhysicalWorld->RemoveEventClient(EventPhysStateChange::id, &CRopeRenderNode::OnPhysStateChange, 1);
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::StaticReset()
{
	CRopeSurfaceCache::CleanupCaches();
}

//////////////////////////////////////////////////////////////////////////
int CRopeRenderNode::OnPhysStateChange(EventPhys const* pEvent)
{
	EventPhysStateChange const* const pStateChangeEvent = static_cast<EventPhysStateChange const*>(pEvent);

	if (pStateChangeEvent->iSimClass[0] == 4 && pStateChangeEvent->iSimClass[1] == 4)
	{
		if (pStateChangeEvent->iForeignData == PHYS_FOREIGN_ID_ROPE)
		{
			CRopeRenderNode* const pRopeRenderNode = static_cast<CRopeRenderNode*>(pStateChangeEvent->pForeignData);

			if (pRopeRenderNode != nullptr)
			{
				pe_status_awake status;

				if (pStateChangeEvent->pEntity->GetStatus(&status) == 0)
				{
					// Rope stopped moving.
					pRopeRenderNode->DisableAudio();
				}
			}
		}
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::GetLocalBounds(AABB& bbox)
{
	bbox = m_localBounds;
};

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::SetMatrix(const Matrix34& mat)
{
	if (m_worldTM == mat)
		return;

	m_worldTM = mat;
	m_InvWorldTM = m_worldTM.GetInverted();
	m_pos = mat.GetTranslation();

	m_WSBBox.SetTransformedAABB(mat, m_localBounds);

	Get3DEngine()->RegisterEntity(this);
	m_bNeedToReRegister = false;

	if (!m_pPhysicalEntity)
		Physicalize();
	else
	{
		// Just move physics.
		pe_params_pos par_pos;
		par_pos.pMtx3x4 = &m_worldTM;
		m_pPhysicalEntity->SetParams(&par_pos);

		IVisArea* pVisArea = GetEntityVisArea();
		if ((pVisArea != 0) != m_bRopeCreatedInsideVisArea)
		{
			// Rope moved between vis area and outside.
			m_bRopeCreatedInsideVisArea = pVisArea != 0;

			pe_params_flags par_flags;
			if (m_params.nFlags & eRope_CheckCollisinos)
			{
				// If we are inside vis/area disable terrain collisions.
				if (!m_bRopeCreatedInsideVisArea)
					par_flags.flagsOR = rope_collides_with_terrain;
				else
					par_flags.flagsAND = ~rope_collides_with_terrain;
			}
			m_pPhysicalEntity->SetParams(&par_flags);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const char* CRopeRenderNode::GetEntityClassName() const
{
	return "RopeEntity";
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::SetName(const char* sName)
{
	m_sName = sName;
}

//////////////////////////////////////////////////////////////////////////
const char* CRopeRenderNode::GetName() const
{
	return m_sName;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::Render(const SRendParams& rParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (GetCVars()->e_Ropes == 0 || (m_dwRndFlags & ERF_HIDDEN) || m_pMaterial == NULL)
		return; // false;

	if (GetCVars()->e_Ropes == 2)
	{
		UpdateRenderMesh();
		return; // true;
	}

	if (m_bModified)
		UpdateRenderMesh();

	if (!m_pRenderMesh || m_pRenderMesh->GetVerticesCount() <= 3)
		return; // false;

	IRenderer* pRend = GetRenderer();

	CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	if (!pObj)
		return; // false;

	pObj->m_pRenderNode = this;
	pObj->m_DissolveRef = rParams.nDissolveRef;
	pObj->m_ObjFlags |= FOB_TRANS_MASK | rParams.dwFObjFlags;
	pObj->m_fAlpha = rParams.fAlpha;
	pObj->SetAmbientColor(rParams.AmbientColor, passInfo);
	pObj->SetMatrix(m_worldTM, passInfo);

	pObj->m_ObjFlags |= FOB_INSHADOW;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Set render quality
	////////////////////////////////////////////////////////////////////////////////////////////////////
	pObj->m_fDistance = rParams.fDistance;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Add render elements
	////////////////////////////////////////////////////////////////////////////////////////////////////

	if (rParams.pMaterial)
		pObj->m_pCurrMaterial = rParams.pMaterial;
	else
		pObj->m_pCurrMaterial = m_pMaterial;

	pObj->m_nMaterialLayers = m_nMaterialLayers;

	//////////////////////////////////////////////////////////////////////////
	if (GetCVars()->e_DebugDraw && pObj->m_fDistance <= GetCVars()->e_DebugDrawMaxDistance)
	{
		RenderDebugInfo(rParams, passInfo);
	}
	//////////////////////////////////////////////////////////////////////////

	m_pRenderMesh->Render(pObj, passInfo);
}

//////////////////////////////////////////////////////////////////////////
bool CRopeRenderNode::RenderDebugInfo(const SRendParams& rParams, const SRenderingPassInfo& passInfo)
{
	if (passInfo.IsShadowPass())
		return false;

	//	bool bVerbose = GetCVars()->e_DebugDraw > 1;
	bool bOnlyBoxes = GetCVars()->e_DebugDraw < 0;

	if (bOnlyBoxes)
		return true;

	Vec3 pos = m_worldTM.GetTranslation();

	int nTris = m_pRenderMesh->GetIndicesCount() / 3;
	// text
	float color[4] = { 0, 1, 1, 1 };

	if (GetCVars()->e_DebugDraw == 2)          // color coded polygon count
		IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "%d", nTris);
	else if (GetCVars()->e_DebugDraw == 5)  // number of render materials (color coded)
		IRenderAuxText::DrawLabelEx(pos, 1.3f, color, true, true, "1");

	return true;
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CRopeRenderNode::GetPhysics() const
{
	return m_pPhysicalEntity;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::SetPhysics(IPhysicalEntity* pPhysicalEntity)
{
	m_pPhysicalEntity = pPhysicalEntity;
	m_bStaticPhysics = pPhysicalEntity->GetType() != PE_ROPE;
	pe_params_foreign_data pfd;
	pfd.pForeignData = (IRenderNode*)this;
	pfd.iForeignData = PHYS_FOREIGN_ID_ROPE;
	pPhysicalEntity->SetParams(&pfd, 1);

	pe_params_rope pr;
	pPhysicalEntity->GetParams(&pr);
	m_points.resize(2);
	m_points[0] = pr.pPoints[0];
	m_points[1] = pr.pPoints[pr.nSegments];
	SyncWithPhysicalRope(true);
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::Physicalize(bool bInstant)
{
	if (m_params.mass <= 0)
		m_bStaticPhysics = true;
	else
		m_bStaticPhysics = false;

	if (!m_pPhysicalEntity || (m_pPhysicalEntity->GetType() == PE_ROPE) != (m_params.mass > 0))
	{
		if (m_pPhysicalEntity)
			gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pPhysicalEntity);
		m_pPhysicalEntity = gEnv->pPhysicalWorld->CreatePhysicalEntity((m_bStaticPhysics) ? PE_STATIC : PE_ROPE,
		                                                               NULL, (IRenderNode*)this, PHYS_FOREIGN_ID_ROPE, GetOwnerEntity() ? GetOwnerEntity()->GetId() : -1);
		if (!m_pPhysicalEntity)
			return;
	}

	if (m_points.size() < 2 || m_params.nPhysSegments < 1)
		return;

	pe_params_pos par_pos;
	par_pos.pMtx3x4 = &m_worldTM;
	m_pPhysicalEntity->SetParams(&par_pos);

	int surfaceTypesId[MAX_SUB_MATERIALS];
	memset(surfaceTypesId, 0, sizeof(surfaceTypesId));
	surfaceTypesId[0] = 0;
	if (m_pMaterial)
		m_pMaterial->FillSurfaceTypeIds(surfaceTypesId);

	Vec3* pSrcPoints = &m_points[0];
	int nSrcPoints = (int)m_points.size();
	int nSrcSegments = nSrcPoints - 1;

	m_physicsPoints.resize(m_params.nPhysSegments + 1);
	int nPoints = (int)m_physicsPoints.size();
	int nTargetSegments = nPoints - 1;
	int i;

	//////////////////////////////////////////////////////////////////////////
	// Set physical ropes params.
	pe_params_rope pr;

	pr.nSegments = nTargetSegments;
	pr.pPoints = strided_pointer<Vec3>(&m_physicsPoints[0]);
	pr.length = 0;

	pr.pEntTiedTo[0] = 0;
	pr.pEntTiedTo[1] = 0;

	// Calc original length.
	for (i = 0; i < nSrcPoints - 1; i++)
	{
		pr.length += (pSrcPoints[i + 1] - pSrcPoints[i]).GetLength();
	}

	// Make rope segments of equal length.
	if (nSrcPoints == 2 && m_params.tension < 0)
	{
		int idir, ibound[2] = { 0, 128 };
		double h, s, L, a, k, ra, seglen, rh, abound[2], x;
		Vec3 origin, axisx;
		static double g_atab[128];
		static int g_bTabFilled = 0;

		h = fabs_tpl(pSrcPoints[0].z - pSrcPoints[1].z);
		s = (Vec2(pSrcPoints[0]) - Vec2(pSrcPoints[1])).GetLength();
		L = (pSrcPoints[0] - pSrcPoints[1]).GetLength() * (1 - m_params.tension);
		if (h < s * 0.0001)
		{
			// will solve for 2*a*sinh(s/(2*a)) == L
			rh = L;
			k = 0;
		}
		else
		{
			// will solve for 2*a*sinh(L/(2*a)) == h/sinh(k)
			k = h / L;
			k = log_tpl((1 + k) / (1 - k)) * 0.5; // k = atanh(k)
			rh = h / sinh(k);
		}
		if (!g_bTabFilled)
			for (i = 0, a = 0.125, g_bTabFilled = 1; i < 128; i++, a += (8.0 / 128))
				g_atab[i] = 2 * a * sinh(1 / (2 * a));
		ibound[0] = 0;
		ibound[1] = 127;
		do
		{
			i = (ibound[0] + ibound[1]) >> 1;
			ibound[isneg(g_atab[i] * s - L)] = i;
		}
		while (ibound[1] > ibound[0] + 1);
		abound[1] = (abound[0] = (ibound[0] * (8.0 / 128) + 0.125) * s) * 2;
		i = 0;
		do
		{
			a = (abound[0] + abound[1]) * 0.5;
			x = 2 * a * sinh(s / (2 * a)) - rh;
			abound[isneg(x)] = a;
		}
		while (++i < 16);
		ra = 1 / (a = (abound[0] + abound[1]) * 0.5);

		idir = isneg(pSrcPoints[0].z - pSrcPoints[1].z);
		pr.pPoints[0] = pSrcPoints[0];
		pr.pPoints[nTargetSegments] = pSrcPoints[1];
		(axisx = (Vec2(pSrcPoints[idir ^ 1]) - Vec2(pSrcPoints[idir])) / s).z = 0;
		origin = (pSrcPoints[0] + pSrcPoints[1]) * 0.5;
		origin.z = pSrcPoints[idir].z;
		origin += axisx * (a * k);
		x = -s * 0.5 - a * k;
		origin.z -= a * cosh(x * ra);
		seglen = L / nTargetSegments;
		for (i = 1; i < nTargetSegments; i++)
		{
			x = seglen * ra + sinh(x * ra);
			x = log(x + sqrt(x * x + 1)) * a; // x = asinh(x)*a;
			pr.pPoints[(nTargetSegments & - idir) + i - (i & - idir) * 2] = origin + axisx * x + Vec3(0, 0, 1) * a * cosh(x * ra);
		}
	}
	else
	{
		int iter, ivtx;
		float ka, kb, kc, kd;
		float rnSegs = 1.0f / nTargetSegments;
		float len = pr.length;

		Vec3 dir, v0;

		iter = 3;
		do
		{
			pr.pPoints[0] = pSrcPoints[0];
			for (i = ivtx = 0; i < nTargetSegments && ivtx < nSrcSegments; )
			{
				dir = pSrcPoints[ivtx + 1] - pSrcPoints[ivtx];
				v0 = pSrcPoints[ivtx] - pr.pPoints[i];
				ka = dir.GetLengthSquared();
				kb = v0 * dir;
				kc = v0.GetLengthSquared() - sqr(len * rnSegs);
				kd = sqrt_tpl(std::max(0.0f, kb * kb - ka * kc));
				if (kd - kb < ka)
					pr.pPoints[++i] = pSrcPoints[ivtx] + dir * ((kd - kb) / ka);
				else
					++ivtx;
			}
			len *= (1.0f - (nTargetSegments - i) * rnSegs);
			len += (pSrcPoints[nSrcSegments] - pr.pPoints[i]).GetLength();
		}
		while (--iter);
		pr.pPoints[nTargetSegments] = pSrcPoints[nSrcSegments]; // copy last point
		dir = pr.pPoints[nTargetSegments] - pr.pPoints[i];
		if (i + 1 < nTargetSegments)
		{
			for (ivtx = i + 1, rnSegs = 1.0f / (nTargetSegments - i); ivtx < nTargetSegments; ivtx++)
			{
				pr.pPoints[ivtx] = pr.pPoints[i] + dir * ((ivtx - i) * rnSegs);
			}
		}
		// Update length.
		pr.length = 0;
		for (i = 0; i < nPoints - 1; i++)
		{
			pr.length += (pr.pPoints[i + 1] - pr.pPoints[i]).GetLength();
		}
	}

	// Transform to world space.
	for (i = 0; i < nPoints; i++)
	{
		pr.pPoints[i] = m_worldTM.TransformPoint(pr.pPoints[i]);
	}

	if (!m_bStaticPhysics)
	{
		//////////////////////////////////////////////////////////////////////////
		pe_params_flags par_flags;
		par_flags.flags = pef_log_state_changes | pef_log_poststep;
		if (m_params.nFlags & eRope_Subdivide)
			par_flags.flags |= rope_subdivide_segs;
		if (m_params.nFlags & eRope_CheckCollisinos)
		{
			par_flags.flags |= rope_collides;
			if (m_params.nFlags & eRope_NoAttachmentCollisions)
				par_flags.flags |= rope_ignore_attachments;
			// If we are inside vis/area disable terrain collisions.
			if (!m_bRopeCreatedInsideVisArea)
			{
				par_flags.flags |= rope_collides_with_terrain;
			}
		}
		par_flags.flags |= rope_traceable;
		if (m_params.mass <= 0)
			par_flags.flags |= pef_disabled;
		par_flags.flags |= rope_no_tears & - isneg(m_params.maxForce);
		m_pPhysicalEntity->SetParams(&par_flags);

		pr.mass = m_params.mass;
		pr.airResistance = m_params.airResistance;
		pr.collDist = m_params.fThickness;
		pr.sensorRadius = m_params.fAnchorRadius;
		pr.maxForce = fabsf(m_params.maxForce);
		pr.jointLimit = m_params.jointLimit;
		pr.friction = m_params.friction;
		pr.frictionPull = m_params.frictionPull;
		pr.wind = m_params.wind;
		pr.windVariance = m_params.windVariance;
		pr.nMaxSubVtx = m_params.nMaxSubVtx;
		pr.surface_idx = surfaceTypesId[0];
		pr.maxIters = m_params.nMaxIters;
		pr.stiffness = m_params.stiffness;
		pr.penaltyScale = m_params.hardness;

		AnchorEndPoints(pr);

		if (m_params.tension != 0)
			pr.length *= 1 - m_params.tension;
		else if (nSrcPoints == 2)
			pr.length *= 0.999f;

		m_pPhysicalEntity->SetParams(&pr);
		//////////////////////////////////////////////////////////////////////////

		// After creation rope is put to sleep, will be awaked on first render after modify.
		pe_action_awake pa;
		pa.bAwake = m_params.nFlags & eRope_Awake ? 1 : 0;
		m_pPhysicalEntity->Action(&pa);

		pe_simulation_params simparams;
		simparams.maxTimeStep = m_params.maxTimeStep;
		simparams.damping = m_params.damping;
		simparams.minEnergy = sqr(m_params.sleepSpeed);
		m_pPhysicalEntity->SetParams(&simparams);

		m_bRopeCreatedInsideVisArea = GetEntityVisArea() != 0;

		if (m_params.nFlags & eRope_NoPlayerCollisions)
		{
			pe_params_collision_class pcs;
			pcs.collisionClassOR.ignore = collision_class_living;
			m_pPhysicalEntity->SetParams(&pcs);
		}
	}
	else
	{
		primitives::capsule caps;
		pe_params_pos pp;
		IGeomManager* pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
		phys_geometry* pgeom;
		pe_geomparams gp;
		pp.pos = pr.pPoints[0];
		m_pPhysicalEntity->SetParams(&pp);
		caps.r = m_params.fThickness;

		/*
		   m_pPhysicalEntity->RemoveGeometry()
		   for(i=0; i<nPoints-1; i++)
		   {
		   caps.axis = pr.pPoints[i+1]-pr.pPoints[i];
		   caps.axis /= (caps.hh = caps.axis.len());	caps.hh *= 0.5f;
		   caps.center = (pr.pPoints[i+1]+pr.pPoints[i])*0.5f-pr.pPoints[0];
		   pgeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::capsule::type, &caps), surfaceTypesId[0]);
		   pgeom->nRefCount = 0;	// we want it to be deleted together with the physical entity
		   m_pPhysicalEntity->AddGeometry(pgeom, &gp);
		   }
		 */
		pe_action_remove_all_parts removeall;
		m_pPhysicalEntity->Action(&removeall);

		int numSplinePoints = m_points.size();
		m_spline.resize(numSplinePoints);
		for (int pnt = 0; pnt < numSplinePoints; pnt++)
		{
			m_spline.key(pnt).flags = 0;
			m_spline.time(pnt) = (float)pnt / (numSplinePoints - 1);
			m_spline.value(pnt) = m_points[pnt];
		}

		int nLengthSamples = m_params.nNumSegments + 1;
		if (!(m_params.nFlags & IRopeRenderNode::eRope_Smooth))
			nLengthSamples = m_params.nPhysSegments + 1;

		Vec3 p0 = m_points[0];
		Vec3 p1;

		for (i = 1; i < nLengthSamples; i++)
		{
			m_spline.interpolate((float)i / (nLengthSamples - 1), p1);
			caps.axis = p1 - p0;
			caps.axis /= (caps.hh = caps.axis.len());
			caps.hh *= 0.5f;
			caps.center = (p1 + p0) * 0.5f - m_points[0];
			IGeometry* pGeom = pGeoman->CreatePrimitive(primitives::capsule::type, &caps);
			pgeom = pGeoman->RegisterGeometry(pGeom, surfaceTypesId[0]);
			pGeom->Release();
			pgeom->nRefCount = 0; // we want it to be deleted together with the physical entity
			m_pPhysicalEntity->AddGeometry(pgeom, &gp);
			p0 = p1;
		}
	}

	m_WSBBox.Reset();
	for (uint i = 0; i < m_points.size(); i++)
		m_WSBBox.Add(m_worldTM.TransformPoint(m_points[i]));
	m_WSBBox.Expand(Vec3(m_params.fThickness));

	m_bModified = true;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::LinkEndPoints()
{
	if (m_pPhysicalEntity)
	{
		if (m_params.mass > 0)
		{
			pe_params_flags par_flags;
			par_flags.flagsOR = pef_disabled;
			m_pPhysicalEntity->SetParams(&par_flags); // To prevent rope from awakining objects on linking.
		}

		pe_params_rope pr, pr1;
		Vec3 ptend[2] = { m_worldTM* m_points[0], m_worldTM * m_points[m_points.size() - 1] };
		pr1.nSegments = 1;
		pr1.pPoints.data = ptend;
		if (m_pPhysicalEntity->GetParams(&pr1) && pr1.nSegments)
		{
			pr.nSegments = pr1.nSegments;
			pr.pPoints = pr1.pPoints;
		}

		pr.pEntTiedTo[0] = 0;
		pr.pEntTiedTo[1] = 0;
		AnchorEndPoints(pr);
		MARK_UNUSED pr.nSegments, pr.pPoints;
		m_pPhysicalEntity->SetParams(&pr);

		if (m_params.mass > 0 && !(m_params.nFlags & eRope_Disabled))
		{
			pe_params_flags par_flags;
			par_flags.flagsAND = ~pef_disabled;
			m_pPhysicalEntity->SetParams(&par_flags);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::LinkEndEntities(IPhysicalEntity* pStartEntity, IPhysicalEntity* pEndEntity)
{
	m_nLinkedEndsMask = 0;

	if (!m_pPhysicalEntity || m_params.mass <= 0)
		return;

	if (m_params.mass > 0)
	{
		pe_params_flags par_flags;
		par_flags.flagsOR = pef_disabled;
		m_pPhysicalEntity->SetParams(&par_flags); // To prevent rope from awakining objects on linking.
	}

	pe_params_rope pr;

	if (pStartEntity)
	{
		pr.pEntTiedTo[0] = pStartEntity;
		pr.idPartTiedTo[0] = 0;
		m_nLinkedEndsMask |= 0x01;
	}

	if (pEndEntity)
	{
		pr.pEntTiedTo[1] = pEndEntity;
		pr.idPartTiedTo[1] = 0;
		m_nLinkedEndsMask |= 0x02;
	}

	m_pPhysicalEntity->SetParams(&pr);

	if (m_params.mass > 0 && !(m_params.nFlags & eRope_Disabled))
	{
		pe_params_flags par_flags;
		par_flags.flagsAND = ~pef_disabled;
		m_pPhysicalEntity->SetParams(&par_flags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::GetEndPointLinks(SEndPointLink* links)
{
	links[0].pPhysicalEntity = 0;
	links[0].offset.Set(0, 0, 0);
	links[0].nPartId = 0;
	links[1].pPhysicalEntity = 0;
	links[1].offset.Set(0, 0, 0);
	links[1].nPartId = 0;

	pe_params_rope pr;
	if (m_pPhysicalEntity)
	{
		m_pPhysicalEntity->GetParams(&pr);
		for (int i = 0; i < 2; i++)
		{
			if (!is_unused(pr.pEntTiedTo[i]) && pr.pEntTiedTo[i])
			{
				if (pr.pEntTiedTo[i] == WORLD_ENTITY)
					pr.pEntTiedTo[i] = gEnv->pPhysicalWorld->GetPhysicalEntityById(gEnv->pPhysicalWorld->GetPhysicalEntityId(WORLD_ENTITY));
				links[i].pPhysicalEntity = pr.pEntTiedTo[i];
				links[i].nPartId = pr.idPartTiedTo[i];

				Matrix34 tm;
				pe_status_pos ppos;
				ppos.pMtx3x4 = &tm;
				pr.pEntTiedTo[i]->GetStatus(&ppos);
				tm.Invert();
				links[i].offset = tm.TransformPoint(pr.ptTiedTo[i]); // To local space.
			}
			else
				links[i].offset = Vec3(0, 0, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::AnchorEndPoints(pe_params_rope& pr)
{
	m_nLinkedEndsMask = 0;
	//m_params.fAnchorRadius
	if (!m_pPhysicalEntity || m_params.mass <= 0)
		return;

	if (m_points.size() < 2)
		return;

	//pr.pEntTiedTo[0] = WORLD_ENTITY;
	//pr.pEntTiedTo[1] = WORLD_ENTITY;
	bool bEndsAdjusted = false, bAdjustEnds = m_params.fAnchorRadius > 0.05001f;
	Vec3 ptend[2] = { pr.pPoints[0], pr.pPoints[pr.nSegments] };

	primitives::sphere sphPrim;

	geom_contact* pContacts = 0;
	sphPrim.center = ptend[0];
	sphPrim.r = m_params.fAnchorRadius;

	int collisionEntityTypes = ent_static | ent_sleeping_rigid | ent_rigid | ent_ignore_noncolliding;
	float d = 0;
	if (m_params.nFlags & eRope_StaticAttachStart)
		pr.pEntTiedTo[0] = WORLD_ENTITY;
	else
	{
		d = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphPrim.type, &sphPrim, Vec3(0, 0, 0), collisionEntityTypes, &pContacts, 0, geom_colltype0, 0);
		if (d > 0 && pContacts)
		{
			IPhysicalEntity* pBindToEntity = gEnv->pPhysicalWorld->GetPhysicalEntityById(pContacts[0].iPrim[0]);
			if (pBindToEntity)
			{
				pr.pEntTiedTo[0] = pBindToEntity;
				pr.idPartTiedTo[0] = pContacts[0].iPrim[1];
				m_nLinkedEndsMask |= 0x01;
				if (bAdjustEnds && inrange((pContacts->center - sphPrim.center).len2(), sqr(0.01f), sqr(m_params.fAnchorRadius)))
					ptend[0] = pr.ptTiedTo[0] = pContacts->t > 0 ? pContacts->pt : pContacts->center, bEndsAdjusted = true;
			}
		}
	}

	if (m_params.nFlags & eRope_StaticAttachEnd)
		pr.pEntTiedTo[1] = WORLD_ENTITY;
	else
	{
		sphPrim.center = ptend[1];
		d = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphPrim.type, &sphPrim, Vec3(0, 0, 0), collisionEntityTypes, &pContacts, 0, geom_colltype0, 0);
		if (d > 0 && pContacts)
		{
			IPhysicalEntity* pBindToEntity = gEnv->pPhysicalWorld->GetPhysicalEntityById(pContacts[0].iPrim[0]);
			if (pBindToEntity)
			{
				pr.pEntTiedTo[1] = pBindToEntity;
				pr.idPartTiedTo[1] = pContacts[0].iPrim[1];
				m_nLinkedEndsMask |= 0x02;
				if (bAdjustEnds && inrange((pContacts->center - sphPrim.center).len2(), sqr(0.01f), sqr(m_params.fAnchorRadius)))
					ptend[1] = pr.ptTiedTo[1] = pContacts->t > 0 ? pContacts->pt : pContacts->center, bEndsAdjusted = true;
			}
		}
	}

	if (bEndsAdjusted && !is_unused(pr.length) &&
	    inrange((pr.pPoints[0] - pr.pPoints[pr.nSegments]).len2(), sqr(pr.length * 0.999f), sqr(pr.length * 1.001f)))
		pr.length = (ptend[0] - ptend[1]).len();
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::Dephysicalize(bool bKeepIfReferenced)
{
	// delete old physics
	if (m_pPhysicalEntity)
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pPhysicalEntity);
	m_pPhysicalEntity = 0;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::SetMaterial(IMaterial* pMat)
{
	if (!pMat)
		pMat = m_pMaterial = GetMatMan()->GetDefaultMaterial();
	m_pMaterial = pMat;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::Precache()
{

}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "RopeRenderNode");
	pSizer->AddObject(this, sizeof(*this));
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::SyncWithPhysicalRope(bool bForce)
{
	if (m_bStaticPhysics)
	{
		m_physicsPoints = m_points;
		m_WSBBox.Reset();
		int numPoints = m_points.size();
		for (int i = 0; i < numPoints; i++)
		{
			Vec3 point = m_points[i];
			m_localBounds.Add(point);
			point = m_worldTM.TransformPoint(point);
			m_WSBBox.Add(point);
		}
		float r = m_params.fThickness;
		m_localBounds.min -= Vec3(r, r, r);
		m_localBounds.max += Vec3(r, r, r);
		m_WSBBox.min -= Vec3(r, r, r);
		m_WSBBox.max += Vec3(r, r, r);
		m_bNeedToReRegister = true; // Bounding box was recalculated, rope needs to be registered again 3d engine.
		m_bModified = false;
		return;
	}
	//////////////////////////////////////////////////////////////////////////
	if (m_pPhysicalEntity)
	{
		pe_status_rope sr;
		sr.lock = 1;
		sr.pGridRefEnt = WORLD_ENTITY;
		if (!m_pPhysicalEntity->GetStatus(&sr))
			return;
		sr.lock = -1;
		int numPoints = sr.nSegments + 1;
		if (m_params.nFlags & eRope_Subdivide && sr.nVtx != 0)
			numPoints = sr.nVtx;
		if (numPoints < 2)
		{
			m_pPhysicalEntity->GetStatus(&sr); // just to unlock if it was locked
			return;
		}
		m_physicsPoints.resize(numPoints);
		m_WSBBox.Reset();
		m_localBounds.Reset();
		if (m_params.nFlags & eRope_Subdivide && sr.nVtx != 0)
			sr.pVtx = &m_physicsPoints[0];
		else
			sr.pPoints = &m_physicsPoints[0];
		m_pPhysicalEntity->GetStatus(&sr);
		for (int i = 0; i < numPoints; i++)
		{
			Vec3 point = m_physicsPoints[i];
			m_WSBBox.Add(point);
			point = m_InvWorldTM.TransformPoint(point);
			m_physicsPoints[i] = point;
			m_localBounds.Add(point);
		}
		float r = m_params.fThickness;
		m_localBounds.min -= Vec3(r, r, r);
		m_localBounds.max += Vec3(r, r, r);
		m_WSBBox.min -= Vec3(r, r, r);
		m_WSBBox.max += Vec3(r, r, r);
		m_bNeedToReRegister = true; // Bounding box was recalculated, rope needs to be registered again 3d engine.
	}
	//////////////////////////////////////////////////////////////////////////
	m_bModified = false;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::CreateRenderMesh()
{
	// make new RenderMesh
	//////////////////////////////////////////////////////////////////////////
	m_pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
	  NULL, 3, EDefaultInputLayouts::P3F_C4B_T2F,
	  NULL, 3, prtTriangleList,
	  "Rope", GetName(),
	  eRMT_Dynamic, 1, 0, NULL, NULL, false, false);

	CRenderChunk chunk;
	memset(&chunk, 0, sizeof(chunk));
	if (m_pMaterial)
		chunk.m_nMatFlags = m_pMaterial->GetFlags();
	chunk.nFirstIndexId = 0;
	chunk.nFirstVertId = 0;
	chunk.nNumIndices = 3;
	chunk.nNumVerts = 3;
	chunk.m_texelAreaDensity = 1.0f;
	m_pRenderMesh->SetChunk(0, chunk);
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::UpdateRenderMesh()
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_pRenderMesh)
	{
		CreateRenderMesh();
		if (!m_pRenderMesh)
			return;
	}

	if (m_physicsPoints.size() < 2 && !m_bStaticPhysics)
		return;
	if (m_points.size() < 2)
		return;

	SyncWithPhysicalRope(false);

	//////////////////////////////////////////////////////////////////////////
	// Tesselate.
	//////////////////////////////////////////////////////////////////////////
	int numSplinePoints = m_physicsPoints.size();
	m_spline.resize(numSplinePoints);
	for (int pnt = 0; pnt < numSplinePoints; pnt++)
	{
		m_spline.key(pnt).flags = 0;
		m_spline.time(pnt) = pnt;
		m_spline.value(pnt) = m_physicsPoints[pnt];
	}
	m_spline.SetRange(0, m_physicsPoints.size() - 1);
	m_spline.SetModified(true);

	int nLengthSamples = m_params.nNumSegments + 1;
	if (!(m_params.nFlags & IRopeRenderNode::eRope_Smooth))
		nLengthSamples = m_params.nPhysSegments + 1;

	Vec2 vTexMin(0, 0);
	Vec2 vTexMax(m_params.fTextureTileU, m_params.fTextureTileV);

	CRopeSurfaceCache::GetLocalCache getCache;
	TubeSurface& tubeSurf = getCache.pCache->tubeSurface;

	tubeSurf.m_worldTM = m_worldTM;
	//tubeSurf.m_worldTM.SetIdentity();

	tubeSurf.GenerateSurface(&m_spline, m_params.fThickness, false, (m_physicsPoints[numSplinePoints - 1] - m_physicsPoints[0]).GetOrthogonal().GetNormalized(),
	                         nLengthSamples, m_params.nNumSides, true, false, false, false, &vTexMin, &vTexMax);

	int nNewVertexCount = tubeSurf.iVQuantity;

	// Resize vertex buffer.
	m_pRenderMesh->LockForThreadAccess();
	m_pRenderMesh->UpdateVertices(NULL, nNewVertexCount, 0, VSF_GENERAL, 0u);

	int nPosStride = 0;
	int nUVStride = 0;
	int nTangsStride = 0;

	byte* pVertPos = m_pRenderMesh->GetPosPtr(nPosStride, FSL_VIDEO_CREATE);
	if (!pVertPos)
		return;
	byte* pVertTexUV = m_pRenderMesh->GetUVPtr(nUVStride, FSL_VIDEO_CREATE);

	byte* pTangents = m_pRenderMesh->GetTangentPtr(nTangsStride, FSL_VIDEO_CREATE);

	for (int i = 0; i < nNewVertexCount; i++)
	{
		Vec3 vP = Vec3(tubeSurf.m_akVertex[i].x, tubeSurf.m_akVertex[i].y, tubeSurf.m_akVertex[i].z);
		Vec2 vT = Vec2(tubeSurf.m_akTexture[i].x, tubeSurf.m_akTexture[i].y);

		*((Vec3*)pVertPos) = vP;
		*((Vec2*)pVertTexUV) = vT;

		*(SPipTangents*)pTangents = SPipTangents(
		  tubeSurf.m_akTangents[i],
		  tubeSurf.m_akBitangents[i], 1);

		pVertPos += nPosStride;
		pVertTexUV += nUVStride;
		pTangents += nTangsStride;
	}

	m_pRenderMesh->UnlockStream(VSF_GENERAL);
	m_pRenderMesh->UnlockStream(VSF_TANGENTS);
	m_pRenderMesh->UpdateIndices(tubeSurf.m_pIndices, tubeSurf.iNumIndices, 0, 0u);

	// Update chunk params.
	CRenderChunk* pChunk = &m_pRenderMesh->GetChunks()[0];
	if (m_pMaterial)
		pChunk->m_nMatFlags = m_pMaterial->GetFlags();
	pChunk->nNumIndices = tubeSurf.iNumIndices;
	pChunk->nNumVerts = tubeSurf.iVQuantity;
	m_pRenderMesh->SetChunk(0, *pChunk);

	m_pRenderMesh->UnLockForThreadAccess();
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::SetParams(const SRopeParams& params)
{
	m_params = params;
	(m_dwRndFlags &= ~(ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS)) |= -(params.nFlags & eRope_CastShadows) >> 31 & (ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS);
}

//////////////////////////////////////////////////////////////////////////
const CRopeRenderNode::SRopeParams& CRopeRenderNode::GetParams() const
{
	return m_params;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::SetPoints(const Vec3* pPoints, int nCount)
{
	m_points.resize(nCount);
	m_physicsPoints.resize(nCount);
	if (nCount > 0)
	{
		m_localBounds.Reset();
		for (int i = 0; i < nCount; i++)
		{
			m_points[i] = pPoints[i];
			m_physicsPoints[i] = pPoints[i];
			m_localBounds.Add(pPoints[i]);
		}
	}
	float r = m_params.fThickness;
	m_localBounds.min -= Vec3(r, r, r);
	m_localBounds.max += Vec3(r, r, r);
	m_WSBBox.SetTransformedAABB(m_worldTM, m_localBounds);

	Get3DEngine()->RegisterEntity(this);
	m_bNeedToReRegister = false;

	Physicalize();
	m_bModified = true;
}

//////////////////////////////////////////////////////////////////////////
int CRopeRenderNode::GetPointsCount() const
{
	return (int)m_points.size();
}

//////////////////////////////////////////////////////////////////////////
const Vec3* CRopeRenderNode::GetPoints() const
{
	if (!m_points.empty())
		return &m_points[0];
	else
		return 0;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::ResetPoints()
{
	m_physicsPoints = m_points;

	Physicalize();
	m_bModified = true;
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::OnPhysicsPostStep()
{
	// Re-register entity.
	if (m_bNeedToReRegister)
	{
		pe_status_pos sp;
		sp.pGridRefEnt = WORLD_ENTITY;
		m_pPhysicalEntity->GetStatus(&sp);
		m_WSBBox = AABB(sp.pos+sp.BBox[0], sp.pos+sp.BBox[1]);
		Get3DEngine()->RegisterEntity(this);
	}
	m_bNeedToReRegister = false;
	m_bModified = true;

	if (m_pIAudioObject != nullptr)
	{
		UpdateAudio();
	}
	else
	{
		pe_status_awake status;

		if (m_pPhysicalEntity->GetStatus(&status) != 0)
		{
			CryAudio::SCreateObjectData const objectData("RopeyMcRopeFace", m_audioParams.occlusionType);
			m_pIAudioObject = gEnv->pAudioSystem->CreateObject(objectData);
			m_pIAudioObject->SetSwitchState(CryAudio::AbsoluteVelocityTrackingSwitchId, CryAudio::OnStateId);
			UpdateAudio();
			m_pIAudioObject->ExecuteTrigger(m_audioParams.startTrigger);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::UpdateAudio()
{
	pe_params_rope ropeParams;
	m_pPhysicalEntity->GetParams(&ropeParams);

	int idx = m_audioParams.segementToAttachTo - 1;
	Vec3 pos = ropeParams.pPoints[idx];

	// Calculate an offset if set.
	if (m_audioParams.offset > 0.0f)
	{
		if (m_audioParams.offset == 1.0f)
		{
			idx = m_audioParams.segementToAttachTo;
			pos = ropeParams.pPoints[idx];
		}
		else
		{
			if (m_audioParams.offset > 0.5f)
			{
				idx = m_audioParams.segementToAttachTo;
			}

			pos += (ropeParams.pPoints[m_audioParams.segementToAttachTo] - ropeParams.pPoints[m_audioParams.segementToAttachTo - 1]) * m_audioParams.offset;
		}
	}

	// Calculate the angle between the rope segments that we're attached to.
	if (m_audioParams.angleParameter != CryAudio::InvalidControlId)
	{
		float angle = 180.0f;

		if (idx > 0 && idx < ropeParams.nSegments)
		{
			// Update the angle parameter (min 0 max 180 in degree).
			// Do this only if we're not attached to either end.
			Vec3 segment1 = ropeParams.pPoints[idx] - ropeParams.pPoints[idx - 1];
			Vec3 segment2 = ropeParams.pPoints[idx] - ropeParams.pPoints[idx + 1];
			segment1.Normalize();
			segment2.Normalize();
			angle = RAD2DEG(acosf(std::max(-1.0f, segment1.Dot(segment2))));
		}

		m_pIAudioObject->SetParameter(m_audioParams.angleParameter, angle);
	}

	m_pIAudioObject->SetTransformation(pos);
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::DisableAudio()
{
	if (m_pIAudioObject != nullptr)
	{
		if (m_audioParams.stopTrigger != CryAudio::InvalidControlId)
		{
			m_pIAudioObject->ExecuteTrigger(m_audioParams.stopTrigger);
		}
		else
		{
			m_pIAudioObject->StopTrigger(m_audioParams.startTrigger);
		}

		gEnv->pAudioSystem->ReleaseObject(m_pIAudioObject);
		m_pIAudioObject = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::SetAudioParams(SRopeAudioParams const& audioParams)
{
	m_audioParams.angleParameter = audioParams.angleParameter;
	m_audioParams.occlusionType = audioParams.occlusionType;
	m_audioParams.offset = audioParams.offset;
	m_audioParams.segementToAttachTo = audioParams.segementToAttachTo;

	if (m_pIAudioObject != nullptr)
	{
		if (m_audioParams.startTrigger != audioParams.startTrigger)
		{
			if (m_audioParams.startTrigger != CryAudio::InvalidControlId)
			{
				m_pIAudioObject->StopTrigger(m_audioParams.startTrigger);
			}
		}

		m_pIAudioObject->SetOcclusionType(m_audioParams.occlusionType);
	}

	m_audioParams.startTrigger = audioParams.startTrigger;
	m_audioParams.stopTrigger = audioParams.stopTrigger;
}

///////////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_pos += delta;
	m_worldTM.SetTranslation(m_pos);
	m_InvWorldTM = m_worldTM.GetInverted();
	m_WSBBox.Move(delta);

	if (m_pPhysicalEntity)
	{
		pe_status_pos status_pos;
		m_pPhysicalEntity->GetStatus(&status_pos);

		pe_params_pos par_pos;
		par_pos.pos = status_pos.pos + delta;
		par_pos.bRecalcBounds = 3;
		m_pPhysicalEntity->SetParams(&par_pos);
	}
}

///////////////////////////////////////////////////////////////////////////////
void CRopeRenderNode::FillBBox(AABB& aabb)
{
	aabb = CRopeRenderNode::GetBBox();
}

///////////////////////////////////////////////////////////////////////////////
EERType CRopeRenderNode::GetRenderNodeType()
{
	return eERType_Rope;
}

///////////////////////////////////////////////////////////////////////////////
float CRopeRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return std::max(GetCVars()->e_ViewDistMin, CRopeRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return(std::max(GetCVars()->e_ViewDistMin, CRopeRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized()));
}

///////////////////////////////////////////////////////////////////////////////
Vec3 CRopeRenderNode::GetPos(bool bWorldOnly) const
{
	return m_pos;
}

///////////////////////////////////////////////////////////////////////////////
IMaterial* CRopeRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}

#pragma warning(pop)
