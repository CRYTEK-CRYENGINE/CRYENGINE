// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//fix add alignment 16 bytes
CMatrixStack::CMatrixStack(uint32 maxDepth, uint32 dirtyFlag)
{
	uint32 i;

	//gRenDev->m_RP.m_nRendFlags = sizeof(CRenderObject);

	m_nDepth = 0;
	m_nMaxDepth = maxDepth;
	m_nDirtyFlag = dirtyFlag;
	// The stack
	m_pStack = new Matrix44A[maxDepth];
	assert(m_pStack);

	for (i = 0; i < maxDepth; i++)
	{
		m_pStack[i].SetIdentity();
		//matrix_ctr(m_pStack[i]);
		//matrix_alloc_inv(m_pStack[i]);
	}
	m_pTop = m_pStack;
}

CMatrixStack::~CMatrixStack()
{
	SAFE_DELETE_ARRAY(m_pStack);
	m_pTop = m_pStack = NULL;
}

// Pops the top of the stack, returns the current top
// *after* popping the top.
bool CMatrixStack::Pop()
{
	if (m_nDepth == 0)
	{
		assert(0);
		return false;
	}

	m_nDepth--;
	m_pTop = &m_pStack[m_nDepth];
	//todo:set dirty flag

	return true;
}

// Pushes the stack by one, duplicating the current matrix.
bool CMatrixStack::Push()
{
	if (m_nDepth + 1 >= m_nMaxDepth)
	{
		assert(0);
		return false;
	}
	cryMemcpy(&m_pStack[m_nDepth + 1], &m_pStack[m_nDepth], sizeof(Matrix44));

	m_nDepth++;
	m_pTop = &m_pStack[m_nDepth];
	//todo:set dirty flag
	return true;
}

// Loads identity in the current matrix.
bool CMatrixStack::LoadIdentity()
{
	m_pTop->SetIdentity();

	return true;
}

// Loads the given matrix into the current matrix
bool CMatrixStack::LoadMatrix(const Matrix44* pMat)
{
	assert(pMat != NULL);
	(*m_pTop) = (*pMat);

	return true;
}

// Right-Multiplies the given matrix to the current matrix.
// (transformation is about the current world origin)
bool CMatrixStack::MultMatrix(const Matrix44* pMat)
{
	(*m_pTop) = (*m_pTop) * (*pMat);
	return true;
}

// Left-Multiplies the given matrix to the current matrix
// (transformation is about the local origin of the object)
bool CMatrixStack::MultMatrixLocal(const Matrix44* pMat)
{
	assert(pMat != NULL);

	(*m_pTop) = (*pMat) * (*m_pTop);

	return true;
}

// Right multiply the current matrix with the computed rotation
// matrix, counterclockwise about the given axis with the given angle.
// (rotation is about the current world origin)
bool CMatrixStack::RotateAxis(const Vec3* pVec, f32 Angle)
{
	assert(pVec != NULL);

	Matrix44A tmp = Matrix44A(Matrix34::CreateRotationAA(Angle, *pVec));
	tmp.Transpose();

	(*m_pTop) = (*m_pTop) * tmp;

	return true;
}

// Left multiply the current matrix with the computed rotation
// matrix, counterclockwise about the given axis with the given angle.
// (rotation is about the local origin of the object)
bool CMatrixStack::RotateAxisLocal(const Vec3* pVec, f32 Angle)
{
	assert(pVec != NULL);

	Matrix44A tmp = Matrix44A(Matrix34::CreateRotationAA(Angle, *pVec));
	tmp.Transpose();

	(*m_pTop) = tmp * (*m_pTop);

	return true;
}

// Right multiply the current matrix with the computed rotation
// matrix. All angles are counterclockwise. (rotation is about the
// current world origin)

// Right multiply the current matrix with the computed scale
// matrix. (transformation is about the current world origin)
bool CMatrixStack::Scale(f32 x, f32 y, f32 z)
{
	Matrix44A tmp;

	tmp = Matrix44A(Matrix33::CreateScale(Vec3(x, y, z))).GetTransposed();
	(*m_pTop) = (*m_pTop) * tmp;

	return true;
}

// Left multiply the current matrix with the computed scale
// matrix. (transformation is about the local origin of the object)
bool CMatrixStack::ScaleLocal(f32 x, f32 y, f32 z)
{
	Matrix44A tmp;

	tmp = Matrix44A(Matrix33::CreateScale(Vec3(x, y, z))).GetTransposed();
	(*m_pTop) = tmp * (*m_pTop);

	return true;
}

// Right multiply the current matrix with the computed translation
// matrix. (transformation is about the current world origin)
bool CMatrixStack::Translate(f32 x, f32 y, f32 z)
{
	Matrix44A tmp;
	tmp.SetIdentity();
	tmp.SetTranslation(Vec3(x, y, z));
	tmp.Transpose();

	(*m_pTop) = (*m_pTop) * tmp;
	return true;
}

// Left multiply the current matrix with the computed translation
// matrix. (transformation is about the local origin of the object)
bool CMatrixStack::TranslateLocal(f32 x, f32 y, f32 z)
{
	Matrix44A tmp;
	tmp.SetIdentity();
	tmp.SetTranslation(Vec3(x, y, z));
	tmp.Transpose();

	(*m_pTop) = tmp * (*m_pTop);

	return true;
}
