// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ShadowUtils.h"
#include "DeferredRenderUtils.h"

#pragma warning(push)
#pragma warning(disable: 4244)

void CDeferredRenderUtils::CreateUnitFrustumMesh(int tessx, int tessy, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
	//////////////////////////////////////////////////////////////////////////
	// Geometry generating
	//////////////////////////////////////////////////////////////////////////

	int32 nBaseVertexIndex = 0;

	indBuff.clear();
	indBuff.reserve(indBuff.size() + (tessx * tessy - 1) * 6); //TOFIX: correct number of indices
	vertBuff.clear();
	vertBuff.reserve(nBaseVertexIndex + tessx * tessy);

	float pViewport[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	float fViewportMinZ = 0.0f;
	float fViewportMaxZ = 1.0f;

	float szx = 1.0f;
	float szy = 1.0f;

	float hsizex = szx / 2;
	float hsizey = szy / 2;
	float deltax = szx / (tessx - 1.0f);
	float deltay = szy / (tessy - 1.0f);

	SVF_P3F_C4B_T2F vert;
	Vec3 tri_vert;
	Vec3 a;
	Vec3 vPos;

	//generate tessellation for far plane
	a.z = 1.0f;

	for (int i = 0; i < tessy; i++)
	{
		for (int j = 0; j < tessx; j++)
		{
			a.x = j * deltax;
			a.y = i * deltay;

			//pre-transform viewport transform vertices in static mesh
			vPos.x = (a.x - pViewport[0]) * 2.0f / pViewport[2] - 1.0f;
			vPos.y = 1.0f - ((a.y - pViewport[1]) * 2.0f / pViewport[3]);   //flip coords for y axis
			vPos.z = (a.z - fViewportMinZ) / (fViewportMaxZ - fViewportMinZ);

			vert.xyz = vPos;
			vert.st = Vec2(1.0f, 1.0f); //valid extraction
			vertBuff.push_back(vert);

		}
	}

	//push light origin
	vert.xyz = Vec3(0, 0, 0);
	vert.st = Vec2(0.0f, 0.0f); //do not extract
	vertBuff.push_back(vert);

	//init indices for triangles drawing
	for (int i = 0; i < tessy - 1; i++)
	{
		for (int j = 0; j < tessx - 1; j++)
		{
			indBuff.push_back((uint16)(i * tessx + j + 1 + nBaseVertexIndex));
			indBuff.push_back((uint16)(i * tessx + j + nBaseVertexIndex));
			indBuff.push_back((uint16)((i + 1) * tessx + (j + 1) + nBaseVertexIndex));

			indBuff.push_back((uint16)((i + 1) * tessx + j + nBaseVertexIndex));
			indBuff.push_back((uint16)((i + 1) * tessx + (j + 1) + nBaseVertexIndex));
			indBuff.push_back((uint16)(i * tessx + j + nBaseVertexIndex));
		}
	}

	//Additional faces
	for (int j = 0; j < tessx - 1; j++)
	{
		indBuff.push_back((uint16)((tessy - 1) * tessx + j + 1 + nBaseVertexIndex));
		indBuff.push_back((uint16)((tessy - 1) * tessx + j + nBaseVertexIndex));
		indBuff.push_back((uint16)(tessy * tessx + nBaseVertexIndex));            //light origin - last vertex

		indBuff.push_back((uint16)(tessy * tessx + nBaseVertexIndex));   //light origin - last vertex
		indBuff.push_back((uint16)(j + nBaseVertexIndex));
		indBuff.push_back((uint16)(j + 1 + nBaseVertexIndex));

	}

	for (int i = 0; i < tessy - 1; i++)
	{

		indBuff.push_back((uint16)((i + 1) * tessx + nBaseVertexIndex));
		indBuff.push_back((uint16)(i * tessx + nBaseVertexIndex));
		indBuff.push_back((uint16)(tessy * tessx + nBaseVertexIndex));  //light origin - last vertex

		indBuff.push_back((uint16)(tessy * tessx + nBaseVertexIndex));              //light origin - last vertex
		indBuff.push_back((uint16)(i * tessx + (tessx - 1) + nBaseVertexIndex));
		indBuff.push_back((uint16)((i + 1) * tessx + (tessx - 1) + nBaseVertexIndex));

	}

}

//push rectangle mesh
void CDeferredRenderUtils::CreateUnitFrustumMeshTransformed(SRenderLight* pLight, ShadowMapFrustum* pFrustum, int nAxis, int tessx, int tessy, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{

	//assert(pFrustum!=NULL);
	assert(pLight != NULL);

	Vec3& vLightPos = pLight->m_Origin;
	f32 fLightRadius = pLight->m_fRadius;

	int32 pViewport[4] = { 0, 0, 1, 1 };

	Matrix44A mProjection;
	Matrix44A mView;

	if (pFrustum == NULL)
	{
		//for light source
		CShadowUtils::GetCubemapFrustumForLight(pLight, nAxis, g_fOmniLightFov /*pLight->m_fLightFrustumAngle*/ + 3.0f, &mProjection, &mView, false); // 3.0f -  offset to make sure that frustums are intersected
	}
	else
	{
		if (!pFrustum->bOmniDirectionalShadow)
		{
			//temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
			//pmProjection = &(pFrustum->mLightProjMatrix);
			mProjection = gRenDev->m_IdentityMatrix;
			mView = pFrustum->mLightViewMatrix;
		}
		else
		{
			//calc one of cubemap's frustums
			Matrix33 mRot = (Matrix33(pLight->m_ObjMatrix));
			//rotation for shadow frustums is disabled
			CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, pFrustum, nAxis, &mProjection, &mView /*, &mRot*/);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Geometry generating
	//////////////////////////////////////////////////////////////////////////

	//add geometry to the existing one
	int32 nBaseVertexIndex = vertBuff.size();

	indBuff.clear();
	indBuff.reserve(indBuff.size() + (tessx * tessy - 1) * 6); //TOFIX: correct number of indices
	vertBuff.clear();
	vertBuff.reserve(nBaseVertexIndex + tessx * tessy);

	float szx = 1.0f;
	float szy = 1.0f;

	float hsizex = szx / 2;
	float hsizey = szy / 2;
	float deltax = szx / (tessx - 1);
	float deltay = szy / (tessy - 1);

	SVF_P3F_C4B_T2F vert;
	Vec3 tri_vert;
	Vec3 a;

	//generate tessellation for far plane
	a.z = 1.0f;

	for (int i = 0; i < tessy; i++)
	{
		for (int j = 0; j < tessx; j++)
		{
			a.x = j * deltax;
			a.y = i * deltay;

			// A
			mathVec3UnProject(&tri_vert, &a, pViewport, &mProjection, &mView, &gRenDev->m_IdentityMatrix);

			//calc vertex expansion in world space coords
			Vec3 vLightDir = tri_vert - vLightPos;
			vLightDir.Normalize();
			vLightDir.SetLength(fLightRadius * 1.05f);

			vert.xyz = vLightPos + vLightDir;
			vert.st = Vec2(0.0f, 0.0f);
			vertBuff.push_back(vert);
		}
	}

	//push light origin
	vert.xyz = vLightPos;
	vert.st = Vec2(0.0f, 0.0f);
	vertBuff.push_back(vert);

	//init indices for triangles drawing
	for (int i = 0; i < tessy - 1; i++)
	{
		for (int j = 0; j < tessx - 1; j++)
		{
			indBuff.push_back((uint16)(i * tessx + j + 1 + nBaseVertexIndex));
			indBuff.push_back((uint16)(i * tessx + j + nBaseVertexIndex));
			indBuff.push_back((uint16)((i + 1) * tessx + (j + 1) + nBaseVertexIndex));

			indBuff.push_back((uint16)((i + 1) * tessx + j + nBaseVertexIndex));
			indBuff.push_back((uint16)((i + 1) * tessx + (j + 1) + nBaseVertexIndex));
			indBuff.push_back((uint16)(i * tessx + j + nBaseVertexIndex));
		}
	}

	//Additional faces
	for (int j = 0; j < tessx - 1; j++)
	{
		indBuff.push_back((uint16)((tessy - 1) * tessx + j + 1 + nBaseVertexIndex));
		indBuff.push_back((uint16)((tessy - 1) * tessx + j + nBaseVertexIndex));
		indBuff.push_back((uint16)(tessy * tessx + nBaseVertexIndex));            //light origin - last vertex

		indBuff.push_back((uint16)(tessy * tessx + nBaseVertexIndex));   //light origin - last vertex
		indBuff.push_back((uint16)(j + nBaseVertexIndex));
		indBuff.push_back((uint16)(j + 1 + nBaseVertexIndex));

	}

	for (int i = 0; i < tessy - 1; i++)
	{

		indBuff.push_back((uint16)((i + 1) * tessx + nBaseVertexIndex));
		indBuff.push_back((uint16)(i * tessx + nBaseVertexIndex));
		indBuff.push_back((uint16)(tessy * tessx + nBaseVertexIndex));  //light origin - last vertex

		indBuff.push_back((uint16)(tessy * tessx + nBaseVertexIndex));              //light origin - last vertex
		indBuff.push_back((uint16)(i * tessx + (tessx - 1) + nBaseVertexIndex));
		indBuff.push_back((uint16)((i + 1) * tessx + (tessx - 1) + nBaseVertexIndex));

	}

}

//////////////////////////////////////////////////////////////////////////
// Approximate with 8 vertices
//////////////////////////////////////////////////////////////////////////
void CreateSimpleLightFrustumMeshTransformed(ShadowMapFrustum* pFrustum, int nFrustNum, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{

	//SVF_P3F_T2F_T3F
	SVF_P3F_C4B_T2F vert;
	Vec3 vNDC;

	assert(pFrustum != NULL);

	indBuff.clear();
	indBuff.reserve(36);

	vertBuff.clear();
	vertBuff.reserve(8);

	//first vertex for cone
	//vert.xyz = Vec3(0.0f, 0.0f, 0.0f);
	//vert.st = Vec2(0.0f, 0.0f);
	//vertBuff.push_back(vert);

	int32 pViewport[4] = { 0, 0, 1, 1 };

	Matrix44A* pmProjection;
	Matrix44A* pmView;

	Matrix44A mProjectionCM;
	Matrix44A mViewCM;

	if (!pFrustum->bOmniDirectionalShadow)
	{
		//temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
		//pmProjection = &(pFrustum->mLightProjMatrix);
		pmProjection = &gRenDev->m_IdentityMatrix;
		pmView = &(pFrustum->mLightViewMatrix);
	}
	else
	{
		//calc one of cubemap's frustums
		CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, pFrustum, nFrustNum, &mProjectionCM, &mViewCM);

		pmProjection = &mProjectionCM;
		pmView = &mViewCM;
	}

	//Create frustum
	for (int i = 0; i < 8; i++)
	{
		//Generate screen space frustum (CCW faces)
		vNDC = Vec3((i == 0 || i == 3 || i == 4 || i == 7) ? 0.0f : 1.0f,
		            (i == 0 || i == 1 || i == 4 || i == 5) ? 0.0f : 1.0f,
		            (i == 0 || i == 1 || i == 2 || i == 3) ? 0.0f : 1.0f
		            );
		//TD: convert math functions to column ordered matrices
		Vec3 pvObj(vert.xyz);
		mathVec3UnProject(&pvObj, &vNDC, pViewport, pmProjection, pmView, &gRenDev->m_IdentityMatrix);
		vert.st = Vec2(0.0f, 0.0f);
		vertBuff.push_back(vert);
	}

	//CCW faces
	static uint16 nFaces[6][4] = {
		{ 0, 1, 2, 3 },
		{ 4, 7, 6, 5 },
		{ 0, 3, 7, 4 },
		{ 1, 5, 6, 2 },
		{ 0, 4, 5, 1 },
		{ 3, 2, 6, 7 }
	};

	//init indices for triangles drawing
	for (int i = 0; i < 6; i++)
	{
		indBuff.push_back((uint16)  nFaces[i][0]);
		indBuff.push_back((uint16)  nFaces[i][1]);
		indBuff.push_back((uint16)  nFaces[i][2]);

		indBuff.push_back((uint16)  nFaces[i][0]);
		indBuff.push_back((uint16)  nFaces[i][2]);
		indBuff.push_back((uint16)  nFaces[i][3]);
	}

}

//////////////////////////////////////////////////////////////////////////
//Sphere light volumes
//////////////////////////////////////////////////////////////////////////
void CDeferredRenderUtils::SphereTessR(Vec3& v0, Vec3& v1, Vec3& v2, int depth, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
	if (depth == 0)
	{
		SVF_P3F_C4B_T2F vert;

		int nVertCount = vertBuff.size();
		vert.st = Vec2(0.0f, 0.0f);

		vert.xyz = v0;
		vertBuff.push_back(vert);
		indBuff.push_back(nVertCount++);

		vert.xyz = v1;
		vertBuff.push_back(vert);
		indBuff.push_back(nVertCount++);

		vert.xyz = v2;
		vertBuff.push_back(vert);
		indBuff.push_back(nVertCount++);

	}
	else
	{
		Vec3 v01, v12, v02;

		v01 = (v0 + v1).GetNormalized();
		v12 = (v1 + v2).GetNormalized();
		v02 = (v0 + v2).GetNormalized();

		SphereTessR(v0, v01, v02, depth - 1, indBuff, vertBuff);
		SphereTessR(v01, v1, v12, depth - 1, indBuff, vertBuff);
		SphereTessR(v12, v02, v01, depth - 1, indBuff, vertBuff);
		SphereTessR(v12, v2, v02, depth - 1, indBuff, vertBuff);
	}
}

void CDeferredRenderUtils::SphereTess(Vec3& v0, Vec3& v1, Vec3& v2, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
	int depth;
	Vec3 w0, w1, w2;
	int i, j, k;

	SVF_P3F_C4B_T2F vert;

	vert.st = Vec2(0.0f, 0.0f);
	int nVertCount = vertBuff.size();

	depth = 2;
	for (i = 0; i < depth; i++)
	{
		for (j = 0; i + j < depth; j++)
		{
			k = depth - i - j;

			{
				w0 = (i * v0 + j * v1 + k * v2) / depth;
				w1 = ((i + 1) * v0 + j * v1 + (k - 1) * v2)
				     / depth;
				w2 = (i * v0 + (j + 1) * v1 + (k - 1) * v2)
				     / depth;
			}

			w0.Normalize();
			w1.Normalize();
			w2.Normalize();

			vert.xyz = w1;
			vertBuff.push_back(vert);
			indBuff.push_back(nVertCount++);

			vert.xyz = w0;
			vertBuff.push_back(vert);
			indBuff.push_back(nVertCount++);

			vert.xyz = w2;
			vertBuff.push_back(vert);
			indBuff.push_back(nVertCount++);

		}
	}
}

#define X .525731112119133606f
#define Z .850650808352039932f

void CDeferredRenderUtils::CreateUnitSphere(int rec, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{

	static Vec3 verts[12] =
	{
		Vec3(-X, 0,  Z),
		Vec3(X,  0,  Z),
		Vec3(-X, 0,  -Z),
		Vec3(X,  0,  -Z),
		Vec3(0,  Z,  X),
		Vec3(0,  Z,  -X),
		Vec3(0,  -Z, X),
		Vec3(0,  -Z, -X),
		Vec3(Z,  X,  0),
		Vec3(-Z, X,  0),
		Vec3(Z,  -X, 0),
		Vec3(-Z, -X, 0)
	};

	static int indices[20][3] =
	{
		{ 0,  4,  1  },
		{ 0,  9,  4  },
		{ 9,  5,  4  },
		{ 4,  5,  8  },
		{ 4,  8,  1  },
		{ 8,  10, 1  },
		{ 8,  3,  10 },
		{ 5,  3,  8  },
		{ 5,  2,  3  },
		{ 2,  7,  3  },
		{ 7,  10, 3  },
		{ 7,  6,  10 },
		{ 7,  11, 6  },
		{ 11, 0,  6  },
		{ 0,  1,  6  },
		{ 6,  1,  10 },
		{ 9,  0,  11 },
		{ 9,  11, 2  },
		{ 9,  2,  5  },
		{ 7,  2,  11 },
	};

	indBuff.clear();
	vertBuff.clear();

	SVF_P3F_C4B_T2F vert;

	vert.st = Vec2(0.0f, 0.0f);

	//debug
	/*for(int i=0; i<12; i++)
	   {
	   vert.xyz = verts[i];
	   vertBuff.push_back(vert);
	   }

	   for (int i = 19; i >= 0; i--)
	   {
	   indBuff.push_back( (uint16)indices[i][2] );
	   indBuff.push_back( (uint16)indices[i][1] );
	   indBuff.push_back( (uint16)indices[i][0] );
	   }*/

	for (int i = 19; i >= 0; i--)
	{
		Vec3& v0 = verts[indices[i][2]];
		Vec3& v1 = verts[indices[i][1]];
		Vec3& v2 = verts[indices[i][0]];
		//SphereTess(v0, v1, v2, indBuff, vertBuff);
		SphereTessR(v0, v1, v2, rec, indBuff, vertBuff);
	}

}
#undef X
#undef Z

void CDeferredRenderUtils::CreateUnitBox(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
	SVF_P3F_C4B_T2F vert;
	Vec3 vNDC;

	indBuff.clear();
	indBuff.reserve(36);

	vertBuff.clear();
	vertBuff.reserve(8);

	//Create unit box
	for (int i = 0; i < 8; i++)
	{
		//Generate screen space frustum (CCW faces)
		vNDC = Vec3((i == 0 || i == 1 || i == 4 || i == 5) ? 0.0f : 1.0f,
		            (i == 0 || i == 3 || i == 4 || i == 7) ? 0.0f : 1.0f,
		            (i == 0 || i == 1 || i == 2 || i == 3) ? 0.0f : 1.0f
		            );
		vert.xyz = vNDC;
		vert.st = Vec2(0.0f, 0.0f);
		vert.color.dcolor = -1;
		vertBuff.push_back(vert);
	}

	//CCW faces
	uint16 nFaces[6][4] = {
		{ 0, 1, 2, 3 },
		{ 4, 7, 6, 5 },
		{ 0, 3, 7, 4 },
		{ 1, 5, 6, 2 },
		{ 0, 4, 5, 1 },
		{ 3, 2, 6, 7 }
	};

	//init indices for triangles drawing
	for (int i = 0; i < 6; i++)
	{
		indBuff.push_back((uint16)  nFaces[i][0]);
		indBuff.push_back((uint16)  nFaces[i][1]);
		indBuff.push_back((uint16)  nFaces[i][2]);

		indBuff.push_back((uint16)  nFaces[i][0]);
		indBuff.push_back((uint16)  nFaces[i][2]);
		indBuff.push_back((uint16)  nFaces[i][3]);
	}
}

//////////////////////////////////////////////////////////////////////////
// Approximate with 8 vertices
//////////////////////////////////////////////////////////////////////////
void CDeferredRenderUtils::CreateSimpleLightFrustumMesh(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{

	//SVF_P3F_T2F_T3F
	SVF_P3F_C4B_T2F vert;
	Vec3 vNDC;

	indBuff.clear();
	indBuff.reserve(36);

	vertBuff.clear();
	vertBuff.reserve(8);

	int32 pViewport[4] = { 0, 0, 1, 1 };
	float fViewportMinZ = 0.0f, fViewportMaxZ = 1.0f;

	Matrix44A mProjectionCM;
	Matrix44A mViewCM;

	Vec3 vPos;

	//Create frustum
	for (int i = 0; i < 8; i++)
	{
		//Generate screen space frustum (CCW faces)
		//vNDC = Vec3((i==0 || i==3 || i==4 || i==7) ? 0.0f : 1.0f,
		//  (i==0 || i==1 || i==4 || i==5) ? 0.0f : 1.0f,
		//  (i==0 || i==1 || i==2 || i==3) ? 0.0f : 1.0f
		//  );

		vNDC = Vec3((i == 0 || i == 1 || i == 4 || i == 5) ? 0.0f : 1.0f,
		            (i == 0 || i == 3 || i == 4 || i == 7) ? 0.0f : 1.0f,
		            (i == 0 || i == 1 || i == 2 || i == 3) ? 0.0f : 1.0f);

		//pre-transform viewport transform vertices in static mesh
		vPos.x = (vNDC.x - pViewport[0]) * 2.0f / pViewport[2] - 1.0f;
		vPos.y = 1.0f - ((vNDC.y - pViewport[1]) * 2.0f / pViewport[3]);   //flip coords for y axis
		vPos.z = (vNDC.z - fViewportMinZ) / (fViewportMaxZ - fViewportMinZ);

		vert.xyz = vPos;
		vert.st = Vec2(1.0f, 1.0f); //valid extraction
		vertBuff.push_back(vert);
	}

	//CCW faces
	static uint16 nFaces[6][4] = {
		{ 0, 1, 2, 3 },
		{ 4, 7, 6, 5 },
		{ 0, 3, 7, 4 },
		{ 1, 5, 6, 2 },
		{ 0, 4, 5, 1 },
		{ 3, 2, 6, 7 }
	};

	//init indices for triangles drawing
	for (int i = 0; i < 6; i++)
	{
		indBuff.push_back((uint16)  nFaces[i][0]);
		indBuff.push_back((uint16)  nFaces[i][1]);
		indBuff.push_back((uint16)  nFaces[i][2]);

		indBuff.push_back((uint16)  nFaces[i][0]);
		indBuff.push_back((uint16)  nFaces[i][2]);
		indBuff.push_back((uint16)  nFaces[i][3]);
	}

}

//////////////////////////////////////////////////////////////////////////
//FS quad
//////////////////////////////////////////////////////////////////////////

void CDeferredRenderUtils::CreateQuad(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
	SVF_P3F_C4B_T2F pScreenQuad[] =
	{
		{ Vec3(0, 0, 0), {
						{ 0 }
			    }, Vec2(0, 0) },
		{ Vec3(0, 1, 0), {
						{ 0 }
			    }, Vec2(0, 1) },
		{ Vec3(1, 0, 0), {
						{ 0 }
			    }, Vec2(1, 0) },
		{ Vec3(1, 1, 0), {
						{ 0 }
			    }, Vec2(1, 1) },
	};

	vertBuff.clear();
	vertBuff.reserve(4);

	vertBuff.push_back(pScreenQuad[0]);
	vertBuff.push_back(pScreenQuad[1]);
	vertBuff.push_back(pScreenQuad[2]);
	vertBuff.push_back(pScreenQuad[3]);

	indBuff.clear();
}

#pragma warning(pop)
