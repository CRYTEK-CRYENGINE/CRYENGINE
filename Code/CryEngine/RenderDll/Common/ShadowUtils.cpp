// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShadowUtils.cpp :

   Revision history:
* Created by Nick Kasyan
   =============================================================================*/
#include "StdAfx.h"
#include "ShadowUtils.h"

#include "Common/RenderView.h"
#include "DriverD3D.h"
#include "GraphicsPipeline/Common/FullscreenPass.h"
#include "GraphicsPipeline/Common/ComputeRenderPass.h"

std::vector<CPoissonDiskGen> CPoissonDiskGen::s_kernelSizeGens;

void CShadowUtils::CalcScreenToWorldExpansionBasis(const CCamera& cam, float fViewWidth, float fViewHeight, Vec3& vWBasisX, Vec3& vWBasisY, Vec3& vWBasisZ, bool bWPos)
{

	const Matrix34& camMatrix = cam.GetMatrix();

	Vec3 vNearEdge = cam.GetEdgeN();

	//all values are in camera space
	float fFar = cam.GetFarPlane();
	float fNear = abs(vNearEdge.y);
	float fWorldWidthDiv2 = abs(vNearEdge.x);
	float fWorldHeightDiv2 = abs(vNearEdge.z);

	float k = fFar / fNear;

	Vec3 vStereoShift = camMatrix.GetColumn0().GetNormalized() * cam.GetAsymL() + camMatrix.GetColumn2() * cam.GetAsymB();

	Vec3 vZ = (camMatrix.GetColumn1().GetNormalized() * fNear + vStereoShift) * k; // size of vZ is the distance from camera pos to near plane
	Vec3 vX = camMatrix.GetColumn0().GetNormalized() * (fWorldWidthDiv2 * k);
	Vec3 vY = camMatrix.GetColumn2().GetNormalized() * (fWorldHeightDiv2 * k);

	//Add view position to constant vector to avoid additional ADD in the shader
	//vZ = vZ + cam.GetPosition();

	//WPos basis adjustments
	if (bWPos)
	{
		//float fViewWidth = cam.GetViewSurfaceX();
		//float fViewHeight = cam.GetViewSurfaceZ();

		vZ = vZ - vX;
		vX *= (2.0f / fViewWidth);
		//vX *= 2.0f; //uncomment for ScreenTC

		vZ = vZ + vY;
		vY *= -(2.0f / fViewHeight);
		//vY *= -2.0f; //uncomment for ScreenTC
	}

	vWBasisX = vX;
	vWBasisY = vY;
	vWBasisZ = vZ;

}

void CShadowUtils::ProjectScreenToWorldExpansionBasis(const Matrix44r& mShadowTexGen, const CCamera& cam, const Vec2& vJitter, float fViewWidth, float fViewHeight, Vec4r& vWBasisX, Vec4r& vWBasisY, Vec4r& vWBasisZ, Vec4r& vCamPos, bool bWPos)
{
	const Matrix34& camMatrix = cam.GetMatrix();

	//all values are in camera space
	const float fFar = cam.GetFarPlane();
	const float fNear = abs(cam.GetEdgeN().y);

	const float k = fFar / fNear;

	//simple general non-hack to shift stereo with off center projection
	float l, r, b, t;
	cam.GetAsymmetricFrustumParams(l, r, b, t);
	Vec3 vStereoShift = 
		camMatrix.GetColumn0().GetNormalized() * (r+l) * .5f +
		camMatrix.GetColumn2().GetNormalized() * (t+b) * .5f;
	const float fWorldWidthDiv2 = (r-l) * .5f;
	const float fWorldHeightDiv2 = (t-b) * .5f;

	const Vec3 vNearX = camMatrix.GetColumn0().GetNormalized() * fWorldWidthDiv2;
	const Vec3 vNearY = camMatrix.GetColumn2().GetNormalized() * fWorldHeightDiv2;
	const Vec3 vNearZ = camMatrix.GetColumn1().GetNormalized() * fNear;

	const Vec3 vJitterShiftX = vNearX * vJitter.x;
	const Vec3 vJitterShiftY = vNearY * vJitter.y;

	Vec3 vZ = (vNearZ + vJitterShiftX + vJitterShiftY + vStereoShift) * k; // size of vZ is the distance from camera pos to near plane

	Vec3 vX = camMatrix.GetColumn0().GetNormalized() * fWorldWidthDiv2 * k;
	Vec3 vY = camMatrix.GetColumn2().GetNormalized() * fWorldHeightDiv2 * k;

	//Add view position to constant vector to avoid additional ADD in the shader
	//vZ = vZ + cam.GetPosition();

	//TFIX calc how k-scale does work with this projection
	//WPos basis adjustments
	if (bWPos)
	{

		//float fViewWidth = cam.GetViewSurfaceX();
		//float fViewHeight = cam.GetViewSurfaceZ();
		vZ = vZ - vX;
		vX *= (2.0f / fViewWidth);
		//vX *= 2.0f; //uncomment for ScreenTC

		vZ = vZ + vY;
		vY *= -(2.0f / fViewHeight);
		//vY *= -2.0f; //uncomment for ScreenTC
	}

	//TOFIX: create PB components for these params
	//creating common projection matrix for depth reconstruction
	vWBasisX = mShadowTexGen * Vec4r(vX, 0.0f);
	vWBasisY = mShadowTexGen * Vec4r(vY, 0.0f);
	vWBasisZ = mShadowTexGen * Vec4r(vZ, 0.0f);
	vCamPos = mShadowTexGen * Vec4r(cam.GetPosition(), 1.0f);

}

void CShadowUtils::GetProjectiveTexGen(const SRenderLight* pLight, int nFace, Matrix44A* mTexGen)
{
	assert(pLight != NULL);
	float fOffsetX = 0.5f;
	float fOffsetY = 0.5f;
	Matrix44A mTexScaleBiasMat = Matrix44A(0.5f, 0.0f, 0.0f, 0.0f,
	                                       0.0f, -0.5f, 0.0f, 0.0f,
	                                       0.0f, 0.0f, 1.0f, 0.0f,
	                                       fOffsetX, fOffsetY, 0.0f, 1.0f);

	Matrix44A mLightProj, mLightView;
	CShadowUtils::GetCubemapFrustumForLight(pLight, nFace, pLight->m_fLightFrustumAngle * 2.f, &mLightProj, &mLightView, true);

	Matrix44 mLightViewProj = mLightView * mLightProj;

	*mTexGen = mLightViewProj * mTexScaleBiasMat;
}

//TF: make support for cubemap faces
void CShadowUtils::GetCubemapFrustumForLight(const SRenderLight* pLight, int nS, float fFov, Matrix44A* pmProj, Matrix44A* pmView, bool bProjLight)
{
	Vec3 zaxis, yaxis, xaxis;

	//new cubemap calculation
	float sCubeVector[6][7] =
	{
		{ 1,  0,  0,  0,  0,  -1, -90 }, //posx
		{ -1, 0,  0,  0,  0,  1,  90  }, //negx
		{ 0,  1,  0,  -1, 0,  0,  0   }, //posy
		{ 0,  -1, 0,  1,  0,  0,  0   }, //negy
		{ 0,  0,  1,  0,  -1, 0,  0   }, //posz
		{ 0,  0,  -1, 0,  1,  0,  0   }, //negz
	};

	/*const float sCubeVector[6][7] =
	   {
	   {1,0,0,  0,0,1, -90}, //posx
	   {-1,0,0, 0,0,1,  90}, //negx
	   {0,1,0,  0,0,-1, 0},  //posy
	   {0,-1,0, 0,0,1,  0},  //negy
	   {0,0,1,  0,1,0,  0},  //posz
	   {0,0,-1, 0,1,0,  0},  //negz
	   };*/

	Vec3 vForward = Vec3(sCubeVector[nS][0], sCubeVector[nS][1], sCubeVector[nS][2]);
	Vec3 vUp = Vec3(sCubeVector[nS][3], sCubeVector[nS][4], sCubeVector[nS][5]);
	Vec3 vEyePt = pLight->m_Origin;
	vForward = vForward + vEyePt;

	//mathMatrixLookAt( pmView, vEyePt, vForward, vUp );

	//adjust light rotation
	Matrix34 mLightRot = pLight->m_ObjMatrix;

	//coord systems conversion(from orientation to shader matrix)
	if (bProjLight) //legacy orientation
	{
		zaxis = mLightRot.GetColumn1().GetNormalized();
		yaxis = -mLightRot.GetColumn0().GetNormalized();
		xaxis = -mLightRot.GetColumn2().GetNormalized();

	}
	else
	{
		zaxis = mLightRot.GetColumn2().GetNormalized();
		yaxis = -mLightRot.GetColumn0().GetNormalized();
		xaxis = -mLightRot.GetColumn1().GetNormalized();
	}

	//Vec3 vLightDir = xaxis;

	//RH
	(*pmView)(0, 0) = xaxis.x;
	(*pmView)(0, 1) = zaxis.x;
	(*pmView)(0, 2) = yaxis.x;
	(*pmView)(0, 3) = 0;
	(*pmView)(1, 0) = xaxis.y;
	(*pmView)(1, 1) = zaxis.y;
	(*pmView)(1, 2) = yaxis.y;
	(*pmView)(1, 3) = 0;
	(*pmView)(2, 0) = xaxis.z;
	(*pmView)(2, 1) = zaxis.z;
	(*pmView)(2, 2) = yaxis.z;
	(*pmView)(2, 3) = 0;
	(*pmView)(3, 0) = -xaxis.Dot(vEyePt);
	(*pmView)(3, 1) = -zaxis.Dot(vEyePt);
	(*pmView)(3, 2) = -yaxis.Dot(vEyePt);
	(*pmView)(3, 3) = 1;

	float zn = max(pLight->m_fProjectorNearPlane, 0.01f);
	float zf = max(pLight->m_fRadius, zn + 0.01f);
	mathMatrixPerspectiveFov(pmProj, (float)DEG2RAD_R(fFov), 1.f, zn, zf);

	/*if (bShadowGen)
	   {
	   mathMatrixPerspectiveFov( pmProj, DEG2RAD_R(fFov), 1.f, max(pLight->m_fProjectorNearPlane,0.01f), pLight->m_fRadius);
	   }
	   else
	   {
	   mathMatrixPerspectiveFov( pmProj, DEG2RAD_R(fFov*1.001), 1.f, max(pLight->m_fProjectorNearPlane,0.01f)+0.01f, pLight->m_fRadius+3.0f);
	   }*/
}

void CShadowUtils::GetCubemapFrustum(EFrustum_Type eFrustumType, const ShadowMapFrustum* pFrust, int nS, Matrix44A* pmProj, Matrix44A* pmView, Matrix33* pmLightRot)
{
	//new cubemap calculation

	float sCubeVector[6][7] =
	{
		{ 1,  0,  0,  0,  0,  -1, -90 }, //posx
		{ -1, 0,  0,  0,  0,  1,  90  }, //negx
		{ 0,  1,  0,  -1, 0,  0,  0   }, //posy
		{ 0,  -1, 0,  1,  0,  0,  0   }, //negy
		{ 0,  0,  1,  0,  -1, 0,  0   }, //posz
		{ 0,  0,  -1, 0,  1,  0,  0   }, //negz
	};

	/*const float sCubeVector[6][7] =
	   {
	   {1,0,0,  0,0,1, -90}, //posx
	   {-1,0,0, 0,0,1,  90}, //negx
	   {0,1,0,  0,0,-1, 0},  //posy
	   {0,-1,0, 0,0,1,  0},  //negy
	   {0,0,1,  0,1,0,  0},  //posz
	   {0,0,-1, 0,1,0,  0},  //negz
	   };*/

	Vec3 vForward = Vec3(sCubeVector[nS][0], sCubeVector[nS][1], sCubeVector[nS][2]);
	Vec3 vUp = Vec3(sCubeVector[nS][3], sCubeVector[nS][4], sCubeVector[nS][5]);
	float fMinDist = max(pFrust->fNearDist, 0.03f);
	float fMaxDist = pFrust->fFarDist;

	Vec3 vEyePt = Vec3(
	  pFrust->vLightSrcRelPos.x + pFrust->vProjTranslation.x,
	  pFrust->vLightSrcRelPos.y + pFrust->vProjTranslation.y,
	  pFrust->vLightSrcRelPos.z + pFrust->vProjTranslation.z
	  );
	Vec3 At = vEyePt;

	if (eFrustumType == FTYP_OMNILIGHTVOLUME)
	{
		vEyePt -= (2.0f * fMinDist) * vForward.GetNormalized();
	}

	vForward = vForward + At;

	//mathMatrixTranslation(&mLightTranslate, -vPos.x, -vPos.y, -vPos.z);2
	mathMatrixLookAt(pmView, vEyePt, vForward, vUp);

	//adjust light rotation
	if (pmLightRot)
	{
		(*pmView) = (*pmView) * (*pmLightRot);
	}
	if (eFrustumType == FTYP_SHADOWOMNIPROJECTION)
	{
		//near plane does have big influence on the precision (depth distribution) for non linear frustums
		mathMatrixPerspectiveFov(pmProj, (float)DEG2RAD_R(g_fOmniShadowFov), 1.f, fMinDist, fMaxDist);
	}
	else if (eFrustumType == FTYP_OMNILIGHTVOLUME)
	{
		//near plane should be extremely small in order to avoid  seams on between frustums
		mathMatrixPerspectiveFov(pmProj, (float)DEG2RAD_R(g_fOmniLightFov), 1.f, fMinDist, fMaxDist);
	}

}

Matrix34 CShadowUtils::GetAreaLightMatrix(const SRenderLight* pLight, Vec3 vScale)
{
	// Box needs to be scaled by 2x for correct radius.
	vScale *= 2.0f;

	// Add width and height to scale.
	float fFOVScale = 2.16f * (max(0.001f, pLight->m_fLightFrustumAngle * 2.0f) / 180.0f);
	vScale.y += pLight->m_fAreaWidth * fFOVScale;
	vScale.z += pLight->m_fAreaHeight * fFOVScale;

	Matrix34 mAreaMatr;
	mAreaMatr.SetIdentity();
	mAreaMatr.SetScale(vScale, pLight->m_Origin);

	// Apply rotation.
	mAreaMatr = pLight->m_ObjMatrix * mAreaMatr;

	// Move box center to light center and pull it back slightly.
	Vec3 vOffsetDir = vScale.y * 0.5f * pLight->m_ObjMatrix.GetColumn1().GetNormalized() +
	                  vScale.z * 0.5f * pLight->m_ObjMatrix.GetColumn2().GetNormalized() +
	                  0.1f * pLight->m_ObjMatrix.GetColumn0().GetNormalized();

	mAreaMatr.SetTranslation(pLight->m_Origin - vOffsetDir);

	return mAreaMatr;
}

static inline Vec3 frac(Vec3 in)
{
	Vec3 out;
	out.x = in.x - (float)(int)in.x;
	out.y = in.y - (float)(int)in.y;
	out.z = in.z - (float)(int)in.z;

	return out;
}

float snap_frac2(float fVal, float fSnap)
{
	float fValSnapped = fSnap * int64(fVal / fSnap);
	return fValSnapped;
}

//Right Handed
void CShadowUtils::mathMatrixLookAtSnap(Matrix44A* pMatr, const Vec3& Eye, const Vec3& At, ShadowMapFrustum* pFrust)
{
	const Vec3 vZ(0.f, 0.f, 1.f);
	const Vec3 vY(0.f, 1.f, 0.f);

	Vec3 vUp;
	Vec3 vEye = Eye;
	Vec3 vAt = At;

	Vec3 vLightDir = (vEye - vAt);
	vLightDir.Normalize();

	if (fabsf(vLightDir.Dot(vZ)) > 0.9995f)
		vUp = vY;
	else
		vUp = vZ;

	float fKatetSize = 1000000.0f * tan_tpl(DEG2RAD(pFrust->fFOV) * 0.5f);

	assert(pFrust->nTexSize > 0);
	float fSnapXY = fKatetSize * 2.f / pFrust->nTexSize; //texture size should be valid already

	//TD - add ratios to the frustum
	fSnapXY *= 2.0f;

	float fZSnap = 192.0f * 2.0f / 16777216.f /*1024.0f*/;
	//fZSnap *= 32.0f;

	Vec3 zaxis = vLightDir.GetNormalized();
	Vec3 xaxis = (vUp.Cross(zaxis)).GetNormalized();
	Vec3 yaxis = zaxis.Cross(xaxis);

	//vAt -= xaxis * snap_frac(vAt.Dot(xaxis), fSnapXY);
	//vAt -= yaxis  * snap_frac(vAt.Dot(yaxis), fSnapXY);

	(*pMatr)(0, 0) = xaxis.x;
	(*pMatr)(0, 1) = yaxis.x;
	(*pMatr)(0, 2) = zaxis.x;
	(*pMatr)(0, 3) = 0;
	(*pMatr)(1, 0) = xaxis.y;
	(*pMatr)(1, 1) = yaxis.y;
	(*pMatr)(1, 2) = zaxis.y;
	(*pMatr)(1, 3) = 0;
	(*pMatr)(2, 0) = xaxis.z;
	(*pMatr)(2, 1) = yaxis.z;
	(*pMatr)(2, 2) = zaxis.z;
	(*pMatr)(2, 3) = 0;
	(*pMatr)(3, 0) = -xaxis.Dot(vEye);
	(*pMatr)(3, 1) = -yaxis.Dot(vEye);
	(*pMatr)(3, 2) = -zaxis.Dot(vEye);
	(*pMatr)(3, 3) = 1;

	float fTranslX = (*pMatr)(3, 0);
	float fTranslY = (*pMatr)(3, 1);
	float fTranslZ = (*pMatr)(3, 2);

	(*pMatr)(3, 0) = snap_frac2(fTranslX, fSnapXY);
	(*pMatr)(3, 1) = snap_frac2(fTranslY, fSnapXY);
	//(*pMatr)(3,2) = snap_frac2(fTranslZ, fZSnap);

}

//todo move frustum computations to the 3d engine
void CShadowUtils::GetShadowMatrixOrtho(Matrix44A& mLightProj, Matrix44A& mLightView, const Matrix44A& mViewMatrix, ShadowMapFrustum* lof, bool bViewDependent)
{
	if (CRenderer::CV_r_ShadowGenDepthClip == 0)
	{
		mathMatrixPerspectiveFov(&mLightProj, (float)DEG2RAD_R(max(lof->fFOV, 0.0000001f)), max(lof->fProjRatio, 0.0001f), lof->fRendNear, lof->fFarDist); //lof->fRendFar
	}
	else
	{
		mathMatrixPerspectiveFov(&mLightProj, (float)DEG2RAD_R(max(lof->fFOV, 0.0000001f)), max(lof->fProjRatio, 0.0001f), lof->fNearDist, lof->fFarDist);
	}

	//linearized depth
	//mLightProj.m22/= lof->fFarDist;
	//mLightProj.m32/= lof->fFarDist;

	const Vec3 zAxis(0.f, 0.f, 1.f);
	const Vec3 yAxis(0.f, 1.f, 0.f);
	Vec3 Up;
	Vec3 Eye = Vec3(
	  lof->vLightSrcRelPos.x + lof->vProjTranslation.x,
	  lof->vLightSrcRelPos.y + lof->vProjTranslation.y,
	  lof->vLightSrcRelPos.z + lof->vProjTranslation.z);
	Vec3 At = Vec3(lof->vProjTranslation.x, lof->vProjTranslation.y, lof->vProjTranslation.z);

	Vec3 vLightDir = At - Eye;
	vLightDir.Normalize();

	if (bViewDependent)
	{
		//we should have LightDir vector transformed to the view space

		Eye = mViewMatrix.GetTransposed().TransformPoint(Eye);
		At = (mViewMatrix.GetTransposed()).TransformPoint(At);

		vLightDir = (mViewMatrix.GetTransposed()).TransformVector(vLightDir);
		//vLightDir.Normalize();

	}

	//get look-at matrix
	if (CRenderer::CV_r_ShadowsGridAligned && lof->m_Flags & DLF_DIRECTIONAL)
	{
		mathMatrixLookAtSnap(&mLightView, Eye, At, lof);
	}
	else
	{
		if (fabsf(vLightDir.Dot(zAxis)) > 0.9995f)
			Up = yAxis;
		else
			Up = zAxis;

		mathMatrixLookAt(&mLightView, Eye, At, Up);
	}

	//we should transform coords to the view space, so shadows are oriented according to camera always
	if (bViewDependent)
	{
		mLightView = mViewMatrix * mLightView;
	}
}

//todo move frustum computations to the 3d engine
void CShadowUtils::GetShadowMatrixForObject(Matrix44A& mLightProj, Matrix44A& mLightView, Vec4& vFrustumInfo, Vec3 vLightSrcRelPos, const AABB& aabb)
{
	if (aabb.GetRadius() < 0.001f)
	{
		mLightProj.SetIdentity();
		mLightView.SetIdentity();
		vFrustumInfo.x = 0.1;
		vFrustumInfo.y = 100.0f;
		vFrustumInfo.w = 0.00001f;  //ObjDepthTestBias;
		vFrustumInfo.z = 0.0f;
		return;
	}

	//Ortho
	f32 yScale = aabb.GetRadius() * 1.11f;///1.1f; //2.0f*
	f32 xScale = yScale;
	f32 fNear = vLightSrcRelPos.GetLength();
	f32 fFar = fNear;
	fNear -= aabb.GetRadius();
	fFar += aabb.GetRadius();

	mathMatrixOrtho(&mLightProj, yScale, xScale, fNear, fFar);

	const Vec3 zAxis(0.f, 0.f, 1.f);
	const Vec3 yAxis(0.f, 1.f, 0.f);
	Vec3 Up;
	Vec3 At = aabb.GetCenter();
	Vec3 vLightDir = -vLightSrcRelPos;
	vLightDir.Normalize();

	Vec3 Eye = At - vLightSrcRelPos.len() * vLightDir;

	if (fabsf(vLightDir.Dot(zAxis)) > 0.9995f)
		Up = yAxis;
	else
		Up = zAxis;

	mathMatrixLookAt(&mLightView, Eye, At, Up);

	vFrustumInfo.x = fNear;
	vFrustumInfo.y = fFar;
	vFrustumInfo.w = 0.00001f;//ObjDepthTestBias;
	vFrustumInfo.z = 0.0f;
}

AABB CShadowUtils::GetShadowMatrixForCasterBox(Matrix44A& mLightProj, Matrix44A& mLightView, ShadowMapFrustum* lof, float fFarPlaneOffset)
{
	GetShadowMatrixForObject(mLightProj, mLightView, lof->vFrustInfo, lof->vLightSrcRelPos, lof->aabbCasters);

	AABB lightSpaceBounds = AABB::CreateTransformedAABB(Matrix34(mLightView.GetTransposed()), lof->aabbCasters);
	Vec3 lightSpaceRange = lightSpaceBounds.GetSize();

	const float fNear = -lightSpaceBounds.max.z;
	const float fFar = -lightSpaceBounds.min.z + fFarPlaneOffset;

	const float yfov = atan_tpl(lightSpaceRange.y * 0.5f / fNear) * 2.f;
	const float aspect = lightSpaceRange.x / lightSpaceRange.y;

	mathMatrixPerspectiveFov(&mLightProj, yfov, aspect, fNear, fFar);

	return lightSpaceBounds;
}

bool CShadowUtils::GetSubfrustumMatrix(Matrix44A& result, const ShadowMapFrustum* pFullFrustum, const ShadowMapFrustum* pSubFrustum)
{
	// get crop rectangle for projection
	Matrix44r mReproj = Matrix44r(pSubFrustum->mLightViewMatrix).GetInverted() * Matrix44r(pFullFrustum->mLightViewMatrix);
	Vec4r srcClipPosTL = Vec4r(-1, -1, 0, 1) * mReproj;
	srcClipPosTL /= srcClipPosTL.w;

	const float fSnap = 2.0f / pFullFrustum->pDepthTex->GetWidth();
	Vec4 crop = Vec4(
	  crop.x = fSnap * int(srcClipPosTL.x / fSnap),
	  crop.y = fSnap * int(srcClipPosTL.y / fSnap),
	  crop.z = 2.0f * pSubFrustum->nTextureWidth / float(pFullFrustum->nTextureWidth),
	  crop.w = 2.0f * pSubFrustum->nTextureHeight / float(pFullFrustum->nTextureHeight)
	);

	Matrix44 cropMatrix(IDENTITY);
	cropMatrix.m00 = 2.f / crop.z;
	cropMatrix.m11 = 2.f / crop.w;
	cropMatrix.m30 = -(1.0f + cropMatrix.m00 * crop.x);
	cropMatrix.m31 = -(1.0f + cropMatrix.m11 * crop.y);

	result = pFullFrustum->mLightViewMatrix * cropMatrix;
	return !(crop.x < -1.0f || crop.z > 1.0f || crop.y < -1.0f || crop.w > 1.0f);
}

bool CShadowUtils::SetupShadowsForFog(SShadowCascades& shadowCascades, const CRenderView* pRenderView)
{
	CRY_ASSERT(pRenderView != nullptr);
	return GetShadowCascades(shadowCascades, pRenderView);	 
}

template<class RenderPassType>
void CShadowUtils::SetShadowSamplingContextToRenderPass(
  RenderPassType& pass,
  int32 linearClampComparisonSamplerSlot,
  int32 pointWrapSamplerSlot,
  int32 pointClampSamplerSlot,
  int32 bilinearWrapSamplerSlot,
  int32 shadowNoiseTextureSlot)
{
	if (pointWrapSamplerSlot >= 0)
	{
		pass.SetSampler(pointWrapSamplerSlot, EDefaultSamplerStates::PointWrap);
	}
	if (pointClampSamplerSlot >= 0)
	{
		pass.SetSampler(pointClampSamplerSlot, EDefaultSamplerStates::PointClamp);
	}
	if (linearClampComparisonSamplerSlot >= 0)
	{
		pass.SetSampler(linearClampComparisonSamplerSlot, EDefaultSamplerStates::LinearCompare);
	}
	if (bilinearWrapSamplerSlot >= 0)
	{
		pass.SetSampler(bilinearWrapSamplerSlot, EDefaultSamplerStates::BilinearWrap);
	}

	if (shadowNoiseTextureSlot >= 0)
	{
		pass.SetTexture(shadowNoiseTextureSlot, CRendererResources::s_ptexShadowJitterMap);
	}
}

// explicit instantiation
template void CShadowUtils::SetShadowSamplingContextToRenderPass(
  CFullscreenPass& pass,
  int32 linearClampComparisonSamplerSlot,
  int32 pointWrapSamplerSlot,
  int32 pointClampSamplerSlot,
  int32 bilinearWrapSamplerSlot,
  int32 shadowNoiseTextureSlot);
template void CShadowUtils::SetShadowSamplingContextToRenderPass(
  CComputeRenderPass& pass,
  int32 linearClampComparisonSamplerSlot,
  int32 pointWrapSamplerSlot,
  int32 pointClampSamplerSlot,
  int32 bilinearWrapSamplerSlot,
  int32 shadowNoiseTextureSlot);

template<class RenderPassType>
void CShadowUtils::SetShadowCascadesToRenderPass(
  RenderPassType& pass,
  int32 startShadowMapsTexSlot,
  int32 cloudShadowTexSlot,
  const SShadowCascades& shadowCascades)
{
	if (startShadowMapsTexSlot >= 0)
	{
		for (auto tex : shadowCascades.pShadowMap)
		{
			if (tex)
			{
				pass.SetTexture(startShadowMapsTexSlot++, tex);
			}
		}
	}
	if (cloudShadowTexSlot >= 0 && shadowCascades.pCloudShadowMap)
	{
		pass.SetTexture(cloudShadowTexSlot, shadowCascades.pCloudShadowMap);
	}
}

// explicit instantiation
template void CShadowUtils::SetShadowCascadesToRenderPass(
  CFullscreenPass& pass,
  int32 startShadowMapsTexSlot,
  int32 cloudShadowTexSlot,
  const SShadowCascades& shadowCascades);
template void CShadowUtils::SetShadowCascadesToRenderPass(
  CComputeRenderPass& pass,
  int32 startShadowMapsTexSlot,
  int32 cloudShadowTexSlot,
  const SShadowCascades& shadowCascades);

bool CShadowUtils::GetShadowCascades(SShadowCascades& shadowCascades, const CRenderView* pRenderView)
{
	CRY_ASSERT(pRenderView != nullptr);

	auto cloudShadowTexId = gcpRendD3D->GetCloudShadowTextureId();

	const auto& shadowFrustumArray = pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunDynamic);

	// fill shadow cascades with default shadow map texture.
	std::fill(std::begin(shadowCascades.pShadowMap), std::end(shadowCascades.pShadowMap), CRendererResources::s_ptexFarPlane);

	// check all shadow map textures are valid and set them to array.
	bool valid = true;
	const int32 size = static_cast<int32>(shadowFrustumArray.size());
	const int32 count = (size < MaxCascadesNum) ? size : MaxCascadesNum;
	for (int32 i = 0; i < count; ++i)
	{
		const auto& pFrustm = shadowFrustumArray[i];
		if (pFrustm && pFrustm->pFrustum && pFrustm->pFrustum->pDepthTex)
		{
			if (pFrustm->pFrustum->pDepthTex == CRendererResources::s_ptexFarPlane)
			{
				valid = false;
			}

			shadowCascades.pShadowMap[i] = pFrustm->pFrustum->pDepthTex;
		}
	}

	// cloud shadow map
	shadowCascades.pCloudShadowMap =
		(cloudShadowTexId > 0)
		? CTexture::GetByID(cloudShadowTexId)
		: CRendererResources::s_ptexWhite;

	return valid;
}

bool CShadowUtils::GetShadowCascadesSamplingInfo(SShadowCascadesSamplingInfo& samplingInfo, const CRenderView* pRenderView)
{
	CRY_ASSERT(pRenderView != nullptr);

	EShaderQuality shaderQuality = gcpRendD3D->m_cEF.m_ShaderProfiles[eST_Shadow].GetShaderQuality();
	const auto& PF = pRenderView->GetShaderConstants();
	const bool bCloudShadow = gcpRendD3D->m_bCloudShadowsEnabled && (gcpRendD3D->GetCloudShadowTextureId() > 0);

	const auto& shadowFrustumArray = pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunDynamic);

	memset(&samplingInfo, 0, sizeof(samplingInfo));

	// gather sun shadow sampling info.
	SShadowSamplingInfo info;
	Matrix44 lightViewProj;
	uint32 nCascadeMask = 0;
	bool valid = true;
	const auto size = static_cast<int32>(shadowFrustumArray.size());
	const auto count = (size < MaxCascadesNum) ? size : MaxCascadesNum;
	for (int32 i = 0; i < count; ++i)
	{
		const auto& pFrustm = shadowFrustumArray[i];
		if (pFrustm && pFrustm->pFrustum)
		{
			nCascadeMask |= 0x1 << i;

			GetForwardShadowSamplingInfo(info, lightViewProj, pFrustm->pFrustum, 0);

			samplingInfo.shadowTexGen[i] = info.shadowTexGen;
			samplingInfo.invShadowMapSize[i] = info.invShadowMapSize;
			samplingInfo.depthTestBias[i] = info.depthTestBias;
			samplingInfo.oneDivFarDist[i] = info.oneDivFarDist;
			if (i == 0)
			{
				samplingInfo.kernelRadius.x = info.kernelRadius;
				samplingInfo.kernelRadius.y = info.kernelRadius;

				// only do full pcf filtering on nearest shadow cascade
				if(pFrustm->pFrustum->nShadowMapLod != 0)
				{
					nCascadeMask |= eForwardShadowFlags_Cascade0_SingleTap;
				}
			}
		}
		else
		{
			valid = false;
		}
	}

	// cloud shadow map parameters.
	samplingInfo.cloudShadowParams = PF.pCloudShadowParams;
	samplingInfo.cloudShadowAnimParams = PF.pCloudShadowAnimParams;
	if (bCloudShadow)
	{
		nCascadeMask |= eForwardShadowFlags_CloudsShadows;
	}

	memcpy(samplingInfo.irregKernel2d, PF.irregularFilterKernel, sizeof(samplingInfo.irregKernel2d));
	static_assert(sizeof(samplingInfo.irregKernel2d) == sizeof(PF.irregularFilterKernel), "Both sizes must be same.");

	// store cascade mask and misc bit mask.
	samplingInfo.kernelRadius.z = alias_cast<float>(nCascadeMask);

	return valid;
}

Matrix44 CShadowUtils::GetClipToTexSpaceMatrix(const ShadowMapFrustum* pFrustum, int nSide)
{
	//TODO: check if half texel offset is still needed
	float fOffsetX = 0.5f;
	float fOffsetY = 0.5f;

	Matrix44 mClipToTexSpace(
		0.5f,     0.0f,     0.0f,     0.0f,
		0.0f,    -0.5f,     0.0f,     0.0f,
		0.0f,     0.0f,     1.0f,     0.0f,
		fOffsetX, fOffsetY, 0.0f,     1.0f);

	if (pFrustum->bUseShadowsPool)
	{
		float arrOffs[2];
		float arrScale[2];
		pFrustum->GetTexOffset(nSide, arrOffs, arrScale);

		//calculate crop matrix for  frustum
		//TD: investigate proper half-texel offset with mCropView
		Matrix44 mCropView(
			arrScale[0],      0.0f,             0.0f,       0.0f,
			0.0f,             arrScale[1],      0.0f,       0.0f,
			0.0f,             0.0f,             1.0f,       0.0f,
			arrOffs[0],       arrOffs[1],       0.0f,       1.0f);

		mClipToTexSpace = mClipToTexSpace * mCropView;
	}
	else if(pFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance)
	{
		const int texWidth = max(pFrustum->pDepthTex->GetWidth(), 1);
		const int texHeight = max(pFrustum->pDepthTex->GetHeight(), 1);

		Matrix44 mCropView(IDENTITY);
		mCropView.m00 = pFrustum->shadowPoolPack[0].GetDim().x / float(texWidth);
		mCropView.m11 = pFrustum->shadowPoolPack[0].GetDim().y / float(texHeight);
		mCropView.m30 = pFrustum->shadowPoolPack[0].Min.x      / float(texWidth);
		mCropView.m31 = pFrustum->shadowPoolPack[0].Min.y      / float(texHeight);

		mClipToTexSpace = mClipToTexSpace * mCropView;
	}

	return mClipToTexSpace;
}


CShadowUtils::SShadowSamplingInfo CShadowUtils::GetDeferredShadowSamplingInfo(ShadowMapFrustum* pFr, int nSide, const CCamera& cam, const SRenderViewport& viewport, const Vec2& subpixelOffset)
{
	CShadowUtils::SShadowSamplingInfo result;
	Matrix44 lightViewProj;
	GetForwardShadowSamplingInfo(result, lightViewProj, pFr, nSide);

	// screenToWorldBasis: screen -> world space
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPosShadowSpace, vWBasisMagnitues;
	CShadowUtils::ProjectScreenToWorldExpansionBasis(result.shadowTexGen, cam, subpixelOffset, float(viewport.width), float(viewport.height), vWBasisX, vWBasisY, vWBasisZ, vCamPosShadowSpace, true);
	Vec4r vWBasisMagnitudes = Vec4r(vWBasisX.GetLength(), vWBasisY.GetLength(), vWBasisZ.GetLength(), 1.0f);

	// noise projection
	Matrix33 mRotMatrix(lightViewProj);
	mRotMatrix.orthonormalizeFastLH();
	Matrix44 noiseProjection = Matrix44r(mRotMatrix).GetTransposed() * Matrix44r(result.shadowTexGen).GetInverted();

	// cascade blending related
	Matrix44 blendTexGen(ZERO);
	Vec4 blendInfo(ZERO);
	Vec4 blendTcNormalize(1.f, 1.f, 0.f, 0.f);

	if (pFr->bBlendFrustum)
	{
		const float fBlendVal = pFr->fBlendVal;
		blendInfo.x = fBlendVal;
		blendInfo.y = 1.0f / (1.0f - fBlendVal);

		if (pFr->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance && 
			pFr->shadowPoolPack[0].GetDim().x > 0 && pFr->shadowPoolPack[0].GetDim().y > 0)
		{
			blendTcNormalize.x =  pFr->pDepthTex->GetWidth()                       / float(pFr->shadowPoolPack[0].GetDim().x);
			blendTcNormalize.y =  pFr->pDepthTex->GetHeight()                      / float(pFr->shadowPoolPack[0].GetDim().y);
			blendTcNormalize.z = -static_cast<float>(pFr->shadowPoolPack[0].Min.x) / float(pFr->shadowPoolPack[0].GetDim().x);
			blendTcNormalize.w = -static_cast<float>(pFr->shadowPoolPack[0].Min.y) / float(pFr->shadowPoolPack[0].GetDim().y);
		}

		if (const ShadowMapFrustum* pPrevFr = pFr->pPrevFrustum)
		{
			//TODO: check if half texel offset is still needed
			float fOffsetX = 0.5f;
			float fOffsetY = 0.5f;

			Matrix44 clipToTexSpace(
				0.5f,     0.0f,     0.0f,     0.0f,
				0.0f,    -0.5f,     0.0f,     0.0f,
				0.0f,     0.0f,     1.0f,     0.0f,
				fOffsetX, fOffsetY, 0.0f,     1.0f);

			Matrix44A shadowMatPrev = pPrevFr->mLightViewMatrix * clipToTexSpace;  // NOTE: no sub-rect here as blending code assumes full [0-1] UV range;
			Vec4r vWBasisPrevX, vWBasisPrevY, vWBasisPrevZ, vCamPosShadowSpacePrev;
			CShadowUtils::ProjectScreenToWorldExpansionBasis(shadowMatPrev.GetTransposed(), cam, subpixelOffset, float(viewport.width), float(viewport.height), vWBasisPrevX, vWBasisPrevY, vWBasisPrevZ, vCamPosShadowSpacePrev, true);

			blendTexGen.SetRow4(0, vWBasisPrevX);
			blendTexGen.SetRow4(1, vWBasisPrevY);
			blendTexGen.SetRow4(2, vWBasisPrevZ);
			blendTexGen.SetRow4(3, vCamPosShadowSpacePrev);

			float fBlendValPrev = pPrevFr->fBlendVal;
			blendInfo.z = fBlendValPrev;
			blendInfo.w = 1.0f / (1.0f - fBlendValPrev);
		}
	}

	result.screenToShadowBasis.SetRow4(0, vWBasisX / vWBasisMagnitudes.x);
	result.screenToShadowBasis.SetRow4(1, vWBasisY / vWBasisMagnitudes.y);
	result.screenToShadowBasis.SetRow4(2, vWBasisZ / vWBasisMagnitudes.z);
	result.screenToShadowBasis.SetRow4(3, vWBasisMagnitudes);
	result.noiseProjection = noiseProjection;
	result.blendTexGen = blendTexGen;
	result.camPosShadowSpace = vCamPosShadowSpace;
	result.blendInfo = blendInfo;
	result.blendTcNormalize = blendTcNormalize;
	result.oneDivFarDistBlend = pFr->pPrevFrustum ? (1.0f / pFr->pPrevFrustum->fFarDist) : (1.0f / pFr->fFarDist);

	return result;
}

void CShadowUtils::GetForwardShadowSamplingInfo(
	SShadowSamplingInfo& samplingInfo,
	Matrix44& lightViewProj,
	const ShadowMapFrustum* pFr,
	int nSide)
{
	// shadowTexGen: world -> shadow tex space
	lightViewProj = pFr->mLightViewMatrix;
	if (pFr->bOmniDirectionalShadow)
	{
		Matrix44 lightView, lightProj;
		CShadowUtils::GetCubemapFrustum(FTYP_SHADOWOMNIPROJECTION, pFr, nSide, &lightProj, &lightView);

		lightViewProj = lightView * lightProj;
	}

	Matrix44 clipToTex = CShadowUtils::GetClipToTexSpaceMatrix(pFr, nSide);
	Matrix44 shadowTexGen = (lightViewProj * clipToTex).GetTransposed();

	// filter kernel size
	float kernelSize;
	if (pFr->m_Flags & DLF_DIRECTIONAL)
	{
		float fFilteredArea = gcpRendD3D->GetShadowJittering() * (pFr->fWidthS + pFr->fBlurS);
		if (pFr->m_eFrustumType == ShadowMapFrustum::e_Nearest)
			fFilteredArea *= 0.1;

		kernelSize = fFilteredArea;
	}
	else
	{
		kernelSize = 2.0f; //constant penumbra for now
		if (pFr->bOmniDirectionalShadow)
			kernelSize *= 1.0f / 3.0f;
	}

	samplingInfo.shadowTexGen = shadowTexGen;
	samplingInfo.oneDivFarDist = 1.0f / pFr->fFarDist;
	samplingInfo.depthTestBias = pFr->fDepthConstBias * ((pFr->m_eFrustumType == ShadowMapFrustum::e_Nearest) ? 3.0f : 1.0f);
	samplingInfo.kernelRadius = kernelSize;
	samplingInfo.invShadowMapSize = 1.0f / pFr->nTexSize;
	samplingInfo.shadowFadingDist = pFr->fShadowFadingDist;
}

CShadowUtils::CShadowUtils()
{
}

CShadowUtils::~CShadowUtils()
{
}

Vec2& CPoissonDiskGen::GetSample(int ind)
{
	assert(ind < m_vSamples.size() && ind >= 0);
	return m_vSamples[ind];
}

CPoissonDiskGen& CPoissonDiskGen::GetGenForKernelSize(int size)
{
	if ((int)s_kernelSizeGens.size() <= size)
		s_kernelSizeGens.resize(size + 1);

	CPoissonDiskGen& pdg = s_kernelSizeGens[size];

	if (pdg.m_vSamples.size() == 0)
	{
		pdg.m_vSamples.resize(size);
		pdg.InitSamples();
	}

	return pdg;
}

void CPoissonDiskGen::FreeMemory()
{
	stl::free_container(s_kernelSizeGens);
}

void CPoissonDiskGen::RandomPoint(Vec2& p)
{
	//generate random point inside circle
	do
	{
		p.x = cry_random(-0.5f, 0.5f);
		p.y = cry_random(-0.5f, 0.5f);
	}
	while (p.x * p.x + p.y * p.y > 0.25f);

	return;
}

//samples distance-based sorting
struct SamplesDistaceSort
{
	bool operator()(const Vec2& samplA, const Vec2& samplB) const
	{
		float R2sampleA = samplA.x * samplA.x + samplA.y * samplA.y;
		float R2sampleB = samplB.x * samplB.x + samplB.y * samplB.y;

		return
		  (R2sampleA < R2sampleB);
	}
};

void CPoissonDiskGen::InitSamples()
{
	const int nQ = 1000;

	RandomPoint(m_vSamples[0]);

	for (int i = 1; i < (int)m_vSamples.size(); i++)
	{
		float dmax = -1.0;

		for (int c = 0; c < i * nQ; c++)
		{
			Vec2 curSample;
			RandomPoint(curSample);

			float dc = 2.0;
			for (int j = 0; j < i; j++)
			{
				float dj =
				  (m_vSamples[j].x - curSample.x) * (m_vSamples[j].x - curSample.x) +
				  (m_vSamples[j].y - curSample.y) * (m_vSamples[j].y - curSample.y);
				if (dc > dj)
					dc = dj;
			}

			if (dc > dmax)
			{
				m_vSamples[i] = curSample;
				dmax = dc;
			}
		}
	}

	for (int i = 0; i < (int)m_vSamples.size(); i++)
	{
		m_vSamples[i] *= 2.0f;
		//CryLogAlways("Sample %i: (%.6f, %.6f)\n", i, m_vSamples[i].x, m_vSamples[i].y);
	}

	//samples sorting
	std::stable_sort(m_vSamples.begin(), m_vSamples.end(), SamplesDistaceSort());

	return;
}

void CShadowUtils::GetIrregKernel(float sData[][4], int nSamplesNum)
{
#define PACKED_SAMPLES 1
	//samples for cubemaps
	/*const Vec4 irreg_kernel[8]=
	   {
	   Vec4(0.527837f, -0.085868f, 0.527837f, 0),
	   Vec4(-0.040088f, 0.536087f, -0.040088f, 0),
	   Vec4(-0.670445f, -0.179949f, -0.670445f, 0),
	   Vec4(-0.419418f, -0.616039f, -0.419418f, 0),
	   Vec4(0.440453f, -0.639399f, 0.440453f, 0),
	   Vec4(-0.757088f, 0.349334f, -0.757088f, 0),
	   Vec4(0.574619f, 0.685879f, 0.574619f, 0),
	   Vec4(0.03851f, -0.939059f, 0.03851f, 0)
	   };
	   //take only first cubemap
	   f32 fFrustumScale = // SShadowsSetupInfo::Filter.x; 
	   for (int i=0; i<8; i++)
	   {
	   sData[i].f[0] = irreg_kernel[i][0] * (1.0f/fFrustumScale);
	   sData[i].f[1] = irreg_kernel[i][1] * (1.0f/fFrustumScale);
	   sData[i].f[2] = irreg_kernel[i][2] * (1.0f/fFrustumScale);
	   sData[i].f[3] = 0;
	   }*/


#ifdef PACKED_SAMPLES
	CPoissonDiskGen& pdg = CPoissonDiskGen::GetGenForKernelSize((nSamplesNum + 1) & ~0x1);

	for (int i = 0, nIdx = 0; i < nSamplesNum; i += 2, nIdx++)
	{
		Vec2 vSample = pdg.GetSample(i);
		sData[nIdx][0] = vSample.x;
		sData[nIdx][1] = vSample.y;
		vSample = pdg.GetSample(i + 1);
		sData[nIdx][2] = vSample.x;
		sData[nIdx][3] = vSample.y;
	}
#else
	CPoissonDiskGen& pdg = CPoissonDiskGen::GetGenForKernelSize(nSamplesNum);

	for (int i = 0, nIdx = 0; i < nSamplesNum; i++, nIdx++)
	{
		Vec2 vSample = pdg.GetSample(i);
		sData[nIdx][0] = vSample.x;
		sData[nIdx][1] = vSample.y;
		sData[nIdx][2] = 0.0f;
		sData[nIdx][3] = 0.0f;
	}
#endif

#undef PACKED_SAMPLES

}

void ShadowMapFrustum::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(pFrustumOwner);
	pSizer->AddObject(pDepthTex);
}
