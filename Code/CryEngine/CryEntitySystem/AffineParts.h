// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

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
