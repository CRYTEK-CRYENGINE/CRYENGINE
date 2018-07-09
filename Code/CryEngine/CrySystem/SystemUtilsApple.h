// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:SystemUtilsApple.h
//  Description: Utilities for iOS and Mac OS X. Needs to be separated
//    due to conflict with the system headers.
//
//	History:
//	-Apr 15,2014:Created by Leander Beernaert
//
//////////////////////////////////////////////////////////////////////

// Get the path to the user's document directory.
// Return length of the string or 0 on failure.
size_t AppleGetUserDocumentDirectory(char* buffer, const size_t bufferLen);

// Get the path to the user's library directory.
// Return length of the string or 0 on failure.
size_t AppleGetUserLibraryDirectory(char* buffer, const size_t bufferLen);

// Get the User's name
// Return length of the string or 0 on failure.
size_t AppleGetUserName(char* buffer, const size_t bufferLen);
