// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef LINE_SEGMENT_2D_H
#define LINE_SEGMENT_2D_H

#include "Utils.h"

#include <vector>

/**
 * @brief A simple oriented 2D line segment implementation.
 */
template<typename VectorT>
class LineSeg
{
	VectorT m_start;
	VectorT m_end;
public:
	LineSeg(const VectorT& start, const VectorT& end);
	const VectorT& Start() const;
	const VectorT& End() const;
	/// Inverts this line segment's direction.
	void           Invert();
	VectorT        Dir() const;
	/// Takes parameter and returns point corresponding to that parameter.
	VectorT        Pt(float t) const;
	float          GetParam(const VectorT& pt) const;
	/// Returns (unnormalized) normal vector to this lineseg.
	VectorT        Normal() const;
};

template<typename VectorT>
inline LineSeg<VectorT>::LineSeg(const VectorT& start, const VectorT& end) :
	m_start(start), m_end(end)
{
}

template<typename VectorT>
inline const VectorT& LineSeg<VectorT >::Start() const
{
	return m_start;
}

template<typename VectorT>
inline const VectorT& LineSeg<VectorT >::End() const
{
	return m_end;
}

template<typename VectorT>
inline void LineSeg<VectorT >::Invert()
{
	std::swap(m_start, m_end);
}

template<typename VectorT>
inline VectorT LineSeg<VectorT >::Dir() const
{
	return m_end - m_start;
}

template<typename VectorT>
inline VectorT LineSeg<VectorT >::Pt(float t) const
{
	return m_start + Dir() * t;
}

template<typename VectorT>
inline float LineSeg<VectorT >::GetParam(const VectorT& pt) const
{
	const VectorT toPt = pt - m_start;
	const VectorT thisDir = Dir();
	const int sign = toPt.Dot(thisDir) > 0.0f ? 1 : -1;
	return sign * toPt.GetLength() / thisDir.GetLength();
}

template<typename VectorT>
inline VectorT LineSeg<VectorT >::Normal() const
{
	return Dir().Perp();
}

typedef LineSeg<Vector2d>      LineSeg2d;
typedef std::vector<LineSeg2d> LineSegVec;

#endif
