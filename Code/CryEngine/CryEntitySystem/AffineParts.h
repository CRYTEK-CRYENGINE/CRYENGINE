// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   affineparts.h
//  Version:     v1.00
//  Created:     19/8/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __affineparts_h__
#define __affineparts_h__

#if _MSC_VER > 1000
	#pragma once
#endif

struct AffineParts
{
	Vec3  pos;      //!< Translation components
	Quat  rot;      //!< Essential rotation.
	Quat  rotScale; //!< Stretch rotation.
	Vec3  scale;    //!< Stretch factors.
	float fDet;     //!< Sign of determinant.

	/** Decompose matrix to its affnie parts.
	 */
	void Decompose(const Matrix44& mat);

	/** Decompose matrix to its affnie parts.
	    Assume there`s no stretch rotation.
	 */
	void SpectralDecompose(const Matrix44& mat);

	/** Decompose matrix to its affnie parts.
	   Assume there`s no stretch rotation.
	 */
	void SpectralDecompose(const Matrix34& mat);
};

#endif // __affineparts_h__
