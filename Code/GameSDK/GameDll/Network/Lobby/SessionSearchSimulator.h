// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Class for returning a fake session list to session searches.
Aim is to use this to test match making performance.
The fake session lists are loaded from XML files.

-------------------------------------------------------------------------
History:
- 20:07:2011 : Created By Andrew Blackwell

*************************************************************************/

//////////////////////////////////////////////////////////////////////////
//Header Guard
#ifndef __SESSIONSEARCHSIMULATOR_H__
#define __SESSIONSEARCHSIMULATOR_H__

//For SCrySessionID and CRYSESSIONID_STRINGLEN
#include <CryLobby/ICryLobby.h>
//For CryMatchmakingSessionSearchCallback
#include <CryLobby/ICryMatchMaking.h>


//////////////////////////////////////////////////////////////////////////
//The session search simulator class
class CSessionSearchSimulator
{
public:
	CSessionSearchSimulator();
	~CSessionSearchSimulator();

	bool OpenSessionListXML( const char* filepath );
	bool OutputSessionListBlock( CryLobbyTaskID& taskID, CryMatchmakingSessionSearchCallback cb, void* cbArg );

	//Inlines
	const char* GetCurrentFilepath() { return m_currentFilepath.c_str(); }

private:
	CryFixedStringT< ICryPak::g_nMaxPath >	m_currentFilepath;

	XmlNodeRef	m_sessionListXML;
	int	m_currentNode;
};

//////////////////////////////////////////////////////////////////////////
// Fake Session IDs used by Session Search Simulator.
// They are only required to display session IDs read in as strings
struct SCryFakeSessionID : public SCrySessionID
{
	char m_idStr[CRYSESSIONID_STRINGLEN];
	
	bool operator == (const SCrySessionID &other)
	{
		char otherIdStr[CRYSESSIONID_STRINGLEN];
		other.AsCStr( otherIdStr, CRYSESSIONID_STRINGLEN );
		return ( strcmpi( m_idStr, otherIdStr ) == 0 );
	}
	
	bool operator < (const SCrySessionID &other)
	{
		char otherIdStr[CRYSESSIONID_STRINGLEN];
		other.AsCStr( otherIdStr, CRYSESSIONID_STRINGLEN );
		return ( strcmpi( m_idStr, otherIdStr ) < 0 );
	}
	
	bool IsFromInvite() const
	{
		return false;
	}

	void AsCStr( char* pOutString, int inBufferSize ) const
	{
		if (pOutString && inBufferSize > 0)
		{
			size_t len = inBufferSize > sizeof(m_idStr) ? sizeof(m_idStr) : inBufferSize;
			memcpy( pOutString, m_idStr, len);
			pOutString[len-1]=0;
		}
	}
};


#endif	//__SESSIONSEARCHSIMULATOR_H__

