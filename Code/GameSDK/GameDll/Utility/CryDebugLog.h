// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
CryDebugLog.h

Description: 
- debug logging for users and arbitrary groups
- logs are compiled out if you don't request logging for the relevant user or group
- by default ALL the logs are all compiled out for release
- to reduce pollution of the namespace all relevant user defines have a CRY_DEBUG_LOG_USER prefix

Usage:

If you want to use these logs you just need to 

- add a section for your username to handle the user defines and group defines
  you are interested in

If you want to add a new group you simply need to 

- add a define for the new group
	- set the define to 1 for the users who want to receive the logs
	- add handling of setting the define to 0 if its not defined

Examples:

// For logs for individual users or groups you can just use CRY_DEBUG_LOG() with your user or groupname

CRY_DEBUG_LOG(jim, "jim is saying testing testing 123 (%d %d %d)", a,b,c);
CRY_DEBUG_LOG(alexwe, "alexwe is saying testing testing 123 (%d %d %d)", a,b,c);
CRY_DEBUG_LOG(SPAWNING, "SPAWNING is saying testing 123 (%d %d %d)", a, b, c);

// For logs suitable for multiple people, or groups you have to use CRY_DEBUG_LOG_MULTI() with your users 
// or groupnames inside CDLU() calls to expand up to the full CRY_DEBUG_LOG_USER_jim define

CRY_DEBUG_LOG_MULTI(CDLU(jim)||CDLU(alexwe), "jim || alexwe is saying testing 123 (%d %d %d)", a,b,c);
CRY_DEBUG_LOG_MULTI(CDLU(alexwe)||CDLU(jim), "alexwe || jim is saying testing 123 (%d %d %d)", a,b,c);

-------------------------------------------------------------------------
History:
-	[07/05/2010] : Created by James Bamford

*************************************************************************/

#ifndef __CRYDEBUGLOG_H__
#define __CRYDEBUGLOG_H__

// doesn't work CRY_DEBUG_LOG_ENABLED comes out as 0 even when ENABLE_PROFILING_CODE was defined
//#define CRY_DEBUG_LOG_ENABLED  (1 && defined(ENABLE_PROFILING_CODE))

#if defined(ENABLE_PROFILING_CODE)
#define CRY_DEBUG_LOG_ENABLED 1
#endif

//
// Available users
//

// nb. both case usernames have been added as windows will take whatever the case you've logged
// in with and set this as your %USERNAME% system variable. First letter captilisation seems
// a common variation and was breaking when we were only checking the lower case username


//
// Available groups 
//

#ifndef CRY_DEBUG_LOG_USER_SPAWNING
	#define CRY_DEBUG_LOG_USER_SPAWNING		0
#endif

#ifndef CRY_DEBUG_LOG_USER_GAMEMODE_EXTRACTION
	#define CRY_DEBUG_LOG_USER_GAMEMODE_EXTRACTION	0
#endif

#ifndef CRY_DEBUG_LOG_USER_GAMEMODE_ALLORNOTHING
	#define CRY_DEBUG_LOG_USER_GAMEMODE_ALLORNOTHING 0
#endif

#ifndef CRY_DEBUG_LOG_USER_GAMEMODE_POWERSTRUGGLE
	#define CRY_DEBUG_LOG_USER_GAMEMODE_POWERSTRUGGLE 0
#endif

#ifndef CRY_DEBUG_LOG_USER_GAMEMODE_GLADIATOR
	#define CRY_DEBUG_LOG_USER_GAMEMODE_GLADIATOR 0
#endif

#ifndef CRY_DEBUG_LOG_USER_AFTER_MATCH_AWARDS
	#define CRY_DEBUG_LOG_USER_AFTER_MATCH_AWARDS	0
#endif

#ifndef CRY_DEBUG_LOG_USER_GAME_FRIENDS_MANAGER
#define CRY_DEBUG_LOG_USER_GAME_FRIENDS_MANAGER	0
#endif

#ifndef CRY_DEBUG_LOG_USER_NY_FEED_MANAGER
#define CRY_DEBUG_LOG_USER_NY_FEED_MANAGER 0
#endif

#ifndef CRY_DEBUG_LOG_USER_BATTLECHATTER
	#define CRY_DEBUG_LOG_USER_BATTLECHATTER 0
#endif

#define CDLU(user)						CRY_DEBUG_LOG_USER_##user

#if CRY_DEBUG_LOG_ENABLED
	#define CRY_DEBUG_LOG(user, ...)                            do { if (CDLU(user)) { CryLog(__VA_ARGS__); } } while(0)
	#define CRY_DEBUG_LOG_ALWAYS(user, ...)                     do { if (CDLU(user)) { CryLogAlways(__VA_ARGS__); } } while (0)
	#define CRY_DEBUG_LOG_MULTI(user, ...)                      do { if (user) { CryLog(__VA_ARGS__); } } while(0)
	#define CRY_DEBUG_LOG_MULTI_ALWAYS(user, ...)               do { if (user) { CryLogAlways(__VA_ARGS__); } } while (0) 
	#define CRY_DEBUG_LOG_COND(user, cond, ...)                 if(cond) { CRY_DEBUG_LOG( user, __VA_ARGS__ ); }
	#define CRY_DEBUG_LOG_ALWAYS_COND(user, cond, ...)          if(cond) { CRY_DEBUG_LOG_ALWAYS( user, __VA_ARGS__ ); }
	#define CRY_DEBUG_LOG_MULTI_COND(user, cond, ...)           if(cond) { CRY_DEBUG_LOG_MULTI( user, __VA_ARGS__ ); }
	#define CRY_DEBUG_LOG_MULTI_ALWAYS_COND(user, cond, ...)    if(cond) { CRY_DEBUG_LOG_MULTI_ALWAYS( user, __VA_ARGS__ ); }
#else
	#define CRY_DEBUG_LOG(user, ...) {}
	#define CRY_DEBUG_LOG_ALWAYS(user, ...) {}
	#define CRY_DEBUG_LOG_MULTI(user, ...) {}
	#define CRY_DEBUG_LOG_MULTI_ALWAYS(user, ...) {}
	#define CRY_DEBUG_LOG_COND(user, cond, ...) {}
	#define CRY_DEBUG_LOG_ALWAYS_COND(user, cond, ...) {}
	#define CRY_DEBUG_LOG_MULTI_COND(user, cond, ...) {}
	#define CRY_DEBUG_LOG_MULTI_ALWAYS_COND(user, cond, ...) {}
#endif


#endif // __CRYDEBUGLOG_H__
