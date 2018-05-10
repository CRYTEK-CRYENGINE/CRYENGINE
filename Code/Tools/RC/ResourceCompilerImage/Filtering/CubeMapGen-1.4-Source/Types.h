// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Types.h -- Variable prototypes
//
//========================================================================================================================================================
// (C) 2004-2005 ATI Research, Inc., All rights reserved.
//========================================================================================================================================================

#ifndef TYPES_H
#define TYPES_H

static char sgCopyrightString[] = "\r\n\r\n(C) 2004 ATI Research, Inc.\r\n\r\n";

// DEFINES ===============================================================================================================================================
#ifdef TRUE
 #undef TRUE
#endif
#define TRUE  1

#ifdef FALSE
 #undef FALSE
#endif
#define FALSE 0

   //=========//
   // Windows //
   //=========//
   //Signed
   typedef char bool8;
   typedef char char8;

/*
   typedef char    int8;
   typedef short   int16;
   typedef int     int32;
   typedef __int64 int64;
*/

   typedef float       float32;
   typedef double      float64;
   //typedef long double float80; //Windows treats this the same as a double

   //Unsigned
/*
   typedef unsigned char    uint8;
   typedef unsigned short   uint16;
   typedef unsigned int     uint32;
   typedef unsigned __int64 uint64;
*/   
   //String
/*
   typedef const char cstr;
*/   

#include <CryCore/BaseTypes.h>

#endif
