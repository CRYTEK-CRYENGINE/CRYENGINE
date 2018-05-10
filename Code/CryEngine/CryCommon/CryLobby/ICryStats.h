// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "CommonICryLobby.h"

#define CRYLOBBY_USER_NAME_LENGTH        32
#define CRYLOBBY_USER_GUID_STRING_LENGTH 40

struct SCryUserID : public CMultiThreadRefCount
{
	virtual bool                                              operator==(const SCryUserID& other) const = 0;
	virtual bool                                              operator<(const SCryUserID& other) const = 0;

	virtual CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> GetGUIDAsString() const
	{
		return CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH>("");
	}
};

struct CryUserID
{
	CryUserID() : userID()
	{}

	CryUserID(SCryUserID* ptr) : userID(ptr)
	{}

	const SCryUserID* get() const
	{
		return userID.get();
	}

	bool operator!=(const CryUserID& other) const
	{
		return !(*this == other);
	}

	bool operator==(const CryUserID& other) const
	{
		if (other.IsValid() && IsValid())
		{
			return ((*other.userID) == (*userID));
		}
		if ((!other.IsValid()) && (!IsValid()))
		{
			return true;
		}
		return false;
	}

	bool operator<(const CryUserID& other) const
	{
		// In the case where one is invalid, the invalid one is considered less than the valid one
		if (other.IsValid())
		{
			if (IsValid())
			{
				return (*userID) < (*other.userID);
			}
			else
			{
				return true;
			}
		}
		return false;
	}

	bool IsValid() const
	{
		return (userID.get() != NULL);
	}

	_smart_ptr<SCryUserID> userID;
};

const CryUserID CryUserInvalidID = NULL;

class CLobbyString : public string
{
public:
	CLobbyString() : string() {}
	CLobbyString(uint32 n) : string() { Format("%u", n); }
	CLobbyString(string s) : string(s) {}
	CLobbyString(uint16 n) : string() { Format("%u", n); }
	CLobbyString(int n) : string() { Format("%i", n); }
	bool operator==(const CLobbyString& other) { return compare(other) == 0; }
	bool operator==(CLobbyString& other) { return compare(other) == 0; }
	CLobbyString(const CLobbyString& s) : string(s) {}
private:
	CLobbyString(const char* p) {}
};
typedef CLobbyString CryLobbyUserDataID;

enum ECryLobbyUserDataType
{
	eCLUDT_Int64,
	eCLUDT_Int32,
	eCLUDT_Int16,
	eCLUDT_Int8,
	eCLUDT_Float64,
	eCLUDT_Float32,
	eCLUDT_Int64NoEndianSwap
};

struct SCryLobbyUserData
{
	CryLobbyUserDataID    m_id;
	ECryLobbyUserDataType m_type;

	SCryLobbyUserData()
	{
		m_id = CryLobbyUserDataID();
		m_type = eCLUDT_Int64;
		m_int64 = 0;
	}

	union
	{
		int64 m_int64;
		f64   m_f64;
		int32 m_int32;
		f32   m_f32;
		int16 m_int16;
		int8  m_int8;
	};

	const SCryLobbyUserData& operator=(const SCryLobbyUserData& src)
	{
		m_id = src.m_id;
		m_type = src.m_type;

		switch (m_type)
		{
		case eCLUDT_Int64:
			m_int64 = src.m_int64;
			break;
		case eCLUDT_Int32:
			m_int32 = src.m_int32;
			break;
		case eCLUDT_Int16:
			m_int16 = src.m_int16;
			break;
		case eCLUDT_Int8:
			m_int8 = src.m_int8;
			break;
		case eCLUDT_Float64:
			m_f64 = src.m_f64;
			break;
		case eCLUDT_Float32:
			m_f32 = src.m_f32;
			break;
		case eCLUDT_Int64NoEndianSwap:
			m_int64 = src.m_int64;
			break;
		default:
			CryLog("Unhandled ECryLobbyUserDataType %d", m_type);
			break;
		}

		return *this;
	};

	bool operator==(const SCryLobbyUserData& other)
	{
		if ((m_id == other.m_id) && (m_type == other.m_type))
		{
			switch (m_type)
			{
			case eCLUDT_Int64:
				return m_int64 == other.m_int64;
			case eCLUDT_Int32:
				return m_int32 == other.m_int32;
			case eCLUDT_Int16:
				return m_int16 == other.m_int16;
			case eCLUDT_Int8:
				return m_int8 == other.m_int8;
			case eCLUDT_Float64:
				return m_f64 == other.m_f64;
			case eCLUDT_Float32:
				return m_f32 == other.m_f32;
			case eCLUDT_Int64NoEndianSwap:
				return m_int64 == other.m_int64;
			default:
				CryLog("Unhandled ECryLobbyUserDataType %d", m_type);
				return false;
			}
		}

		return false;
	}

	bool operator!=(const SCryLobbyUserData& other)
	{
		if ((m_id == other.m_id) && (m_type == other.m_type))
		{
			switch (m_type)
			{
			case eCLUDT_Int64:
				return m_int64 != other.m_int64;
			case eCLUDT_Int32:
				return m_int32 != other.m_int32;
			case eCLUDT_Int16:
				return m_int16 != other.m_int16;
			case eCLUDT_Int8:
				return m_int8 != other.m_int8;
			case eCLUDT_Float64:
				return m_f64 != other.m_f64;
			case eCLUDT_Float32:
				return m_f32 != other.m_f32;
			case eCLUDT_Int64NoEndianSwap:
				return m_int64 != other.m_int64;
			default:
				CryLog("Unhandled ECryLobbyUserDataType %d", m_type);
				return true;
			}
		}

		return true;
	}
};


typedef uint32 CryStatsLeaderBoardID;

//! The score column for a leader board entry.
//! In Live the score id is the property id from the xlast program given to the score column when defining a leaderboard.
struct SCryStatsLeaderBoardScore
{
	SCryLobbyUserData  score;
	CryLobbyUserDataID id;
};

//! The user defined columns for a leaderboard.
//! In Live a leaderboard can have up to 64 user defined columns.
//! The CryLobbyUserDataID inside data is the property id from the xlast program given to the column when defining a leaderboard
//! columnID is the id output by the xlast program when defining a leaderboard.
struct SCryStatsLeaderBoardUserColumn
{
	SCryLobbyUserData  data;
	CryLobbyUserDataID columnID;
};

//! A leaderboard row contains a score column and 0 or more custom columns.
struct SCryStatsLeaderBoardData
{
	SCryStatsLeaderBoardScore       score;
	SCryStatsLeaderBoardUserColumn* pColumns;
	uint32                          numColumns;
};

//! In Live the leaderboard id is output by the xlast program when defining a leaderboard.
struct SCryStatsLeaderBoardWrite
{
	SCryStatsLeaderBoardData data;
	CryStatsLeaderBoardID    id;
};

struct SCryStatsLeaderBoardReadRow
{
	SCryStatsLeaderBoardData data;
	uint32                   rank;
	CryUserID                userID;
	char                     name[CRYLOBBY_USER_NAME_LENGTH];
};

struct SCryStatsLeaderBoardReadResult
{
	CryStatsLeaderBoardID        id;
	SCryStatsLeaderBoardReadRow* pRows;
	uint32                       numRows;
	uint32                       totalNumBoardRows;
};

//! \param taskID	   Task ID allocated when the function was executed
//! \param error		 Error code    eCLE_Success if the function succeeded or an error that occurred while processing the function
//! \param pArg			 Pointer to application-specified data
typedef void(*CryStatsCallback)(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);

//! \param taskID	   Task ID allocated when the function was executed
//! \param error		 Error code    eCLE_Success if the function succeeded or an error that occurred while processing the function
//! \param pResult	 If error is eCLE_Success a pointer to a SCryStatsLeaderBoardReadResult which contains the information read from the leaderboard.
//! \param pArg			 Pointer to application-specified data
typedef void(*CryStatsReadLeaderBoardCallback)(CryLobbyTaskID taskID, ECryLobbyError error, SCryStatsLeaderBoardReadResult* pResult, void* pArg);

//! \param taskID	   Task ID allocated when the function was executed
//! \param error		 Error code    eCLE_Success if the function succeeded or an error that occurred while processing the function
//! \param pData		 Pointer to an array of SCryLobbyUserData that will match the data registered and contain the last data written.
//! \param numData	 The number of SCryLobbyUserData returned.
//! \param pArg			 Pointer to application-specified data
typedef void(*CryStatsReadUserDataCallback)(CryLobbyTaskID taskID, ECryLobbyError error, SCryLobbyUserData* pData, uint32 numData, void* pArg);

enum ECryLobbyLeaderboardType
{
	eCLLT_P2P,
	eCLLT_Dedicated,
	eCLLT_Num
};

struct ICryStats
{
	// <interfuscator:shuffle>
	virtual ~ICryStats() {}

	//! This function must be called before any other leaderboard functions.
	//! It defines the applications custom data used for it's leaderboards.
	//! \param pBoards		   Pointer to an array of SCryStatsLeaderBoardWrite that defines the applications leaderboards
	//! \param numBoards	   Number of leaderboards to register
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback
	//! \param cb					   Callback function that is called when function completes
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start
	virtual ECryLobbyError StatsRegisterLeaderBoards(SCryStatsLeaderBoardWrite* pBoards, uint32 numBoards, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg) = 0;

	//! Write one or more leaderboard entries for the given user.
	//! In Live this call must be made between SessionStart and SessionEnd.
	//! \param session		   The session the user is in and the stats are for.
	//! \param user				   The pad number of the local user the stats are being written for.
	//! \param pBoards		   Pointer to an array of leaderboard entires to be written.
	//! \param numBoards	   Number of leaderboard entries to be written.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsWriteLeaderBoards(CrySessionHandle session, uint32 user, SCryStatsLeaderBoardWrite* pBoards, uint32 numBoards, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg) = 0;

	//! Write one or more leaderboard entries for one or more users.
	//! In Live this call must be made between SessionStart and SessionEnd.
	//! \param session		   The session the users are in and the stats are for.
	//! \param pUserIDs		   Pointer to an array of CryUserID of the users the stats are for.
	//! \param ppBoards		   Pointer to an array of arrays of leaderboard entries to be written. One array to be written for each user.
	//! \param pNumBoards	   The number of leaderboard entries to be written for each user.
	//! \param numUserIDs	   The number of users being written.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsWriteLeaderBoards(CrySessionHandle session, CryUserID* pUserIDs, SCryStatsLeaderBoardWrite** ppBoards, uint32* pNumBoards, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg) = 0;

	//! Retrieves a list of entires in the order of their ranking within a leaderboard, starting with a specified rank value.
	//! \param board			   The leaderboard to read from.
	//! \param startRank	   The rank to start retrieving from.
	//! \param num				   The number of entires to retrieve.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsReadLeaderBoardByRankForRange(CryStatsLeaderBoardID board, uint32 startRank, uint32 num, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg) = 0;

	//! Retrieves a list of entires in the order of their ranking within a leaderboard, with the given local user appearing in the middle.
	//! \param board			   The leaderboard to read from.
	//! \param user				   The pad number of the local user who will appear in the middle of the list.
	//! \param num				   The number of entires to retrieve.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsReadLeaderBoardByRankForUser(CryStatsLeaderBoardID board, uint32 user, uint32 num, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg) = 0;

	//! Retrieves a list of entires for a given list of users in the order of their ranking within a leaderboard.
	//! \param board			   The leaderboard to read from.
	//! \param pUserIDs		   Pointer to an array of CryUserID for the users to read.
	//! \param numUserIDs	   Number of users to read.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsReadLeaderBoardByUserID(CryStatsLeaderBoardID board, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg) = 0;

	//! This function must be called before any other user data functions.
	//! It defines the applications custom data used for it's users.
	//! In Live a maximum of 3000 bytes of custom user data can be stored.
	//! \param pData			   Pointer to an array of SCryLobbyUserData that defines the user data the application wants to store for each user.
	//! \param numData		   Number of items to store.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsRegisterUserData(SCryLobbyUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg) = 0;

	//! Write the user data for a local user.
	//! \param user				   The pad number of the local user to have their data written.
	//! \param pData			   Pointer to an array of SCryLobbyUserData that defines the user data to write.
	//! \param numData		   Number of data items to write.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsWriteUserData(uint32 user, SCryLobbyUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg) = 0;

	//! Write the user data for an array of users.
	//! \param pUserIDs		   The user IDs to have their data written.
	//! \param ppData			   Ragged 2D array of SCryLobbyUserData that defines the user data to write.
	//! \param pNumData		   Array of lengths of rows in ppData.
	//! \param numUserIDs	   The number of user IDs.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsWriteUserData(CryUserID* pUserIDs, SCryLobbyUserData** ppData, uint32* pNumData, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg) = 0;

	//! Read the user data for a local user.
	//! \param user				   The pad number of the local user to read data for.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsReadUserData(uint32 user, CryLobbyTaskID* pTaskID, CryStatsReadUserDataCallback cb, void* pCbArg) = 0;

	//! Read the user data for a given CryUserID.
	//! \param user				   The pad number of the local user who is doing the read.
	//! \param userID			   The CryUserID of the user to read data for.
	//! \param pTaskID		   Pointer to buffer to store the task ID to identify this call in the callback.
	//! \param cb					   Callback function that is called when function completes.
	//! \param pCbArg			   Pointer to application-specified data that is passed to the callback.
	//! \return			   eCLE_Success if function successfully started or an error code if function failed to start.
	virtual ECryLobbyError StatsReadUserData(uint32 user, CryUserID userID, CryLobbyTaskID* pTaskID, CryStatsReadUserDataCallback cb, void* pCbArg) = 0;

	//! Cancel the given task. The task will still be running in the background but the callback will not be called when it finishes.
	//! \param taskID			   The task to cancel
	virtual void CancelTask(CryLobbyTaskID taskID) = 0;

	//! Set the leaderboard type.
	//! \param leaderboardType	   The leaderboard type
	virtual void SetLeaderboardType(ECryLobbyLeaderboardType leaderboardType) = 0;

	//! Associates a name with an ID for API's that handle leaderboard by string.
	//! \param name    leaderboard name API requires
	//! \param id    the ID passed to CryStats when reading/writing
	virtual void RegisterLeaderboardNameIdPair(string name, uint32 id) = 0;

	//! Get the leaderboard type.
	virtual ECryLobbyLeaderboardType GetLeaderboardType() = 0;

	// </interfuscator:shuffle>
};
