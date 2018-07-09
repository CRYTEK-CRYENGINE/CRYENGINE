// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   IRangeSignaling.h
   $Id$
   $DateTime$
   Description: Signal entities based on ranges from other entities
   ---------------------------------------------------------------------
   History:
   - 12:03:2009 : Interface created by Kevin Kirst

 *********************************************************************/

#ifndef __IRANGESIGNALING_H__
#define __IRANGESIGNALING_H__

struct IAISignalExtraData;

struct IRangeSignaling
{
	virtual ~IRangeSignaling() {}

	virtual bool AddRangeSignal(EntityId IdEntity, float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL) = 0;
	virtual bool AddTargetRangeSignal(EntityId IdEntity, EntityId IdTarget, float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL) = 0;
	virtual bool AddAngleSignal(EntityId IdEntity, float fAngle, float fBoundary, const char* sSignal, IAISignalExtraData* pData = NULL) = 0;
	virtual bool DestroyPersonalRangeSignaling(EntityId IdEntity) = 0;
	virtual void ResetPersonalRangeSignaling(EntityId IdEntity) = 0;
	virtual void EnablePersonalRangeSignaling(EntityId IdEntity, bool bEnable) = 0;
};

#endif //__IRANGESIGNALING_H__
