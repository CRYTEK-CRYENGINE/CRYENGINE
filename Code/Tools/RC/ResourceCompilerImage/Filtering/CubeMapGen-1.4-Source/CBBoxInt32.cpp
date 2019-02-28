// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//CBBoxInt32
// 3D bounding box with int32 coordinates
//
//=============================================================================
// (C) 2005 ATI Research, Inc., All rights reserved.
//=============================================================================

#include "CBBoxInt32.h"

#define CP_MIN_INT32 0x80000000
#define CP_MAX_INT32 0x7fffffff
#define CP_MIN(a, b)  (((a) < (b)) ? (a) : (b)) 
#define CP_MAX(a, b)  (((a) > (b)) ? (a) : (b)) 


//--------------------------------------------------------------------------------------
// CBBoxInt32
//--------------------------------------------------------------------------------------
CBBoxInt32::CBBoxInt32(void)
{ 
   Clear();
}


//--------------------------------------------------------------------------------------
// Text to see if CBBoxInt32 is empty or not
//--------------------------------------------------------------------------------------
bool8 CBBoxInt32::Empty(void)
{
   if( (m_minCoord[0] > m_maxCoord[0]) ||
       (m_minCoord[1] > m_maxCoord[1]) ||
       (m_minCoord[2] > m_maxCoord[2]) )
   {
      return TRUE;
   }
   else
   {
      return FALSE;    
   }
}


//--------------------------------------------------------------------------------------
// Clear bounding box extents
//--------------------------------------------------------------------------------------
void CBBoxInt32::Clear(void)
{ 
   m_minCoord[0] = CP_MAX_INT32;
   m_minCoord[1] = CP_MAX_INT32;
   m_minCoord[2] = CP_MAX_INT32;
   m_maxCoord[0] = CP_MIN_INT32;
   m_maxCoord[1] = CP_MIN_INT32;
   m_maxCoord[2] = CP_MIN_INT32;
}


//--------------------------------------------------------------------------------------
// Augment bounding box extents by specifying point to include in bounding box
//--------------------------------------------------------------------------------------
void CBBoxInt32::Augment(int32 aX, int32 aY, int32 aZ)
{ 
   m_minCoord[0] = CP_MIN( m_minCoord[0], aX );
   m_minCoord[1] = CP_MIN( m_minCoord[1], aY );
   m_minCoord[2] = CP_MIN( m_minCoord[2], aZ );
   m_maxCoord[0] = CP_MAX( m_maxCoord[0], aX );
   m_maxCoord[1] = CP_MAX( m_maxCoord[1], aY );
   m_maxCoord[2] = CP_MAX( m_maxCoord[2], aZ );
}


//--------------------------------------------------------------------------------------
// Augment bounding box extents by specifying x coordinate to include in bounding box
//--------------------------------------------------------------------------------------
void CBBoxInt32::AugmentX(int32 aX)
{ 
   m_minCoord[0] = CP_MIN( m_minCoord[0], aX );
   m_maxCoord[0] = CP_MAX( m_maxCoord[0], aX );
}


//--------------------------------------------------------------------------------------
// Augment bounding box extents by specifying x coordinate to include in bounding box
//--------------------------------------------------------------------------------------
void CBBoxInt32::AugmentY(int32 aY)
{ 
   m_minCoord[1] = CP_MIN( m_minCoord[1], aY );
   m_maxCoord[1] = CP_MAX( m_maxCoord[1], aY );
}


//--------------------------------------------------------------------------------------
// Augment bounding box extents by specifying x coordinate to include in bounding box
//--------------------------------------------------------------------------------------
void CBBoxInt32::AugmentZ(int32 aZ)
{ 
   m_minCoord[2] = CP_MIN( m_minCoord[2], aZ );
   m_maxCoord[2] = CP_MAX( m_maxCoord[2], aZ );
}


//--------------------------------------------------------------------------------------
// Clamp minimum values in bbox to be no larger than aX, aY, aZ
//--------------------------------------------------------------------------------------
void CBBoxInt32::ClampMin(int32 aX, int32 aY, int32 aZ)
{ 
   m_minCoord[0] = CP_MAX( m_minCoord[0], aX );
   m_minCoord[1] = CP_MAX( m_minCoord[1], aY );
   m_minCoord[2] = CP_MAX( m_minCoord[2], aZ );
}


//--------------------------------------------------------------------------------------
// Clamp maximum values in bbox to be no larger than aX, aY, aZ
//--------------------------------------------------------------------------------------
void CBBoxInt32::ClampMax(int32 aX, int32 aY, int32 aZ)
{
   m_maxCoord[0] = CP_MIN( m_maxCoord[0], aX );
   m_maxCoord[1] = CP_MIN( m_maxCoord[1], aY );
   m_maxCoord[2] = CP_MIN( m_maxCoord[2], aZ );
}





