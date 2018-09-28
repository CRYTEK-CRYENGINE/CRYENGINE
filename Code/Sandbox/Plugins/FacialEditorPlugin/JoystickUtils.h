// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __JOYSTICKUTILS_H__
#define __JOYSTICKUTILS_H__

class IJoystick;
class IJoystickChannel;

namespace JoystickUtils
{
float Evaluate(IJoystickChannel* pChannel, const CTimeValue&  time);
void  SetKey(IJoystickChannel* pChannel, const CTimeValue& time, float value, bool createIfMissing = true);
void  Serialize(IJoystickChannel* pChannel, XmlNodeRef node, bool bLoading);
bool  HasKey(IJoystickChannel* pChannel, const CTimeValue& time);
void  RemoveKey(IJoystickChannel* pChannel, const CTimeValue& time);
void  RemoveKeysInRange(IJoystickChannel* pChannel, const CTimeValue& startTime, const CTimeValue& endTime);
void  PlaceKey(IJoystickChannel* pChannel, const CTimeValue& time);
}

#endif //__JOYSTICKUTILS_H__

