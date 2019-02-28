// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Designer
{
typedef std::vector<Vertex> Convex;

class Convexes : public _i_reference_target_t
{
public:

	void    AddConvex(const Convex& convex) { m_Convexes.push_back(convex); }
	int     GetConvexCount() const          { return m_Convexes.size(); }
	Convex& GetConvex(int nIndex)           { return m_Convexes[nIndex]; }

private:

	mutable std::vector<Convex> m_Convexes;
};
}

