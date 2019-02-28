// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Operator functions for common variable types 

-------------------------------------------------------------------------
History:
- 26:08:2011   10:15 : Created by Claire Allan

*************************************************************************/

#ifndef ITEM_PARAMS_REGISTRATION_OPERATORS
#define ITEM_PARAMS_REGISTRATION_OPERATORS


static bool operator < (const Vec3& a, const Vec3& b) 
{ 
	if(a.x != b.x) 
	{
		return (a.x < b.x);
	}
	
	if(a.y != b.y)
	{
		return (a.y < b.y);
	}
	
	return (a.z < b.z); 
}

static bool operator < (const Vec2& a, const Vec2& b) 
{ 
	if(a.x != b.x)
	{
		return (a.x < b.x);
	}
	
	return (a.y < b.y); 
}

static bool operator < (const Ang3& a, const Ang3& b) 
{ 
	if(a.x != b.x)
	{
		return (a.x < b.x);
	}
	
	if(a.y != b.y)
	{ 
		return (a.y < b.y);
	}
	
	return (a.z < b.z); 

}

static bool operator != (const Ang3& a, const Ang3& b) 
{ 
	return (a.x != b.x && a.y != b.y && a.z != b.z); 
}


#endif