// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_DYNAMICARRAY_H__3B36BCC2_AB88_11D1_8F4C_D0B67CC10B05__INCLUDED_)
#define AFX_DYNAMICARRAY_H__3B36BCC2_AB88_11D1_8F4C_D0B67CC10B05__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

class CDynamicArray2D
{
public:
	// constructor
	CDynamicArray2D(unsigned int iDimension1, unsigned int iDimension2);
	// destructor
	virtual ~CDynamicArray2D();
	//
	void ScaleImage(CDynamicArray2D* pDestination);
	//
	void GetMemoryUsage(ICrySizer* pSizer);

	float** m_Array;                    //

private:

	unsigned int m_Dimension1;          //
	unsigned int m_Dimension2;          //
};

#endif // !defined(AFX_DYNAMICARRAY_H__3B36BCC2_AB88_11D1_8F4C_D0B67CC10B05__INCLUDED_)

