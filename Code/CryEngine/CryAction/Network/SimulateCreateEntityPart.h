// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: generic 'create part' events - try not to use
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __SIMULATECREATEENTITYPART_H__
#define __SIMULATECREATEENTITYPART_H__

#pragma once

// dummy struct to resolve compilation of BreakReplicator.cpp on gcc-4.2
struct SSimulateCreateEntityPartMessage
{
};

/*
   #include "ObjectSelector.h"

   struct SSimulateCreateEntityPart
   {
   bool bInvalid;
   CObjectSelector entSrc;
   CObjectSelector entNew;
   int partidSrc;
   int partidNew;
   int nTotParts;
   Vec3 breakImpulse;
   Vec3 breakAngImpulse;
   float cutRadius;
   Vec3 cutPtLoc[2];
   Vec3 cutDirLoc[2];

   bool GetPosition(Vec3& pos)
   {
    Vec3 a, b;
    if (entSrc.GetPosition(a))
    {
      if (entNew.GetPosition(b))
        pos = 0.5f * (a+b);
      else
        pos = a;
      return true;
    }
    else
      return entNew.GetPosition(pos);
   }

   void SerializeWith( TSerialize ser )
   {
    ser.Value("bInvalid", bInvalid);
    entSrc.SerializeWith(ser);
    entNew.SerializeWith(ser);
    ser.Value("partidSrc", partidSrc);
    ser.Value("partidNew", partidNew);
    ser.Value("nTotParts", nTotParts);
    ser.Value("breakImpulse", breakImpulse);
    ser.Value("breakAngImpulse", breakAngImpulse);
    ser.Value("cutRadius", cutRadius);
    ser.Value("cutPtLoc0", cutPtLoc[0]);
    ser.Value("cutPtLoc1", cutPtLoc[1]);
    ser.Value("cutDirLoc0", cutDirLoc[0]);
    ser.Value("cutDirLoc1", cutDirLoc[1]);
   }
   };

   struct SSimulateCreateEntityPartInfo : public IBreakDescriptionInfo
   {
   SSimulateCreateEntityPartInfo( int bid ) : breakId(bid) {}

   SSimulateCreateEntityPart msg;
   int breakId;
   };
 */

#endif
