// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __JOYSTICKUTILS_H__
#define __JOYSTICKUTILS_H__

class IJoystick;
class IJoystickChannel;

namespace JoystickUtils
{
float Evaluate(IJoystickChannel* pChannel, float time);
void  SetKey(IJoystickChannel* pChannel, float time, float value, bool createIfMissing = true);
void  Serialize(IJoystickChannel* pChannel, XmlNodeRef node, bool bLoading);
bool  HasKey(IJoystickChannel* pChannel, float time);
void  RemoveKey(IJoystickChannel* pChannel, float time);
void  RemoveKeysInRange(IJoystickChannel* pChannel, float startTime, float endTime);
void  PlaceKey(IJoystickChannel* pChannel, float time);
}

#endif //__JOYSTICKUTILS_H__

