// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __APPKITWRAPPER_H__
#define __APPKITWRAPPER_H__

// Initialise the AppKit application framework
void InitAppKit();

// Shutdown the AppKit application framework
void ShutdownAppKit();

// Get the resource path in the Bundle
bool GetBundleResourcePath(char *ptr, const size_t len);

#endif
