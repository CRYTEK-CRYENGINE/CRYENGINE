// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WIGGLE__H__
#define __WIGGLE__H__


class CWiggle
{
public:
	CWiggle();

	void SetParams(float frequency);
	float Update(float deltaTime);

public:
	float m_points[4];
	float m_frequency;
	float m_time;
};



class CWiggleVec3
{
public:

	void SetParams(float frequency);
	Vec3 Update(float deltaTime);

private:
	CWiggle m_x, m_y, m_z;
};


#endif
