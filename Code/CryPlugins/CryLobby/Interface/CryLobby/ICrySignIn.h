// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryCore/Containers/CryArray.h>
#include <CryString/CryString.h>
#include <CryLobby/ICryLobby.h> // <> required for Interfuscator

//! User credential identifiers used by GetUserCredentials task.
enum ECryLobbyUserCredential
{
	eCLUC_Email,
	eCLUC_Password,
	eCLUC_UserName,
	eCLUC_Invalid
};

struct SUserCredential
{
	SUserCredential()
		: type(eCLUC_Invalid),
		value()
	{}

	SUserCredential(const SUserCredential& src)
		: type(src.type),
		value(src.value)
	{}

	SUserCredential(ECryLobbyUserCredential srcType, const string& srcValue)
		: type(srcType),
		value(srcValue)
	{}

	~SUserCredential()
	{}

	const SUserCredential& operator=(const SUserCredential& src)
	{
		type = src.type;
		value = src.value;

		return *this;
	}

	ECryLobbyUserCredential type;
	string                  value;
};

typedef DynArray<SUserCredential> TUserCredentials;

//! \param taskID				  Task ID allocated when the function was executed.
//! \param error				  Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param user           local user index
//! \param pArg					  Pointer to application-specified data.
typedef void (* CrySignInCallback)(CryLobbyTaskID taskID, ECryLobbyError error, uint32 user, void* pCbArg);

//! \param taskID			    Task ID allocated when the function was executed
//! \param error			    Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function
//! \param user           local user index
//! \param pName          If email and password are correct for existing account, the user name for that account
//! \param pArg				    Pointer to application-specified data
typedef void (* CrySignInCreateUserCallback)(CryLobbyTaskID taskID, ECryLobbyError error, uint32 user, const char* pName, void* pArg);

//! \param taskID			    Task ID allocated when the function was executed
//! \param error			    Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function
//! \param user           local user index
//! \param pCredentials   Pointer to array of user credentials
//! \param pArg				    Pointer to application-specified data
typedef void (* CrySignInGetUserCredentialsCallback)(CryLobbyTaskID taskID, ECryLobbyError error, uint32 user, DynArray<SUserCredential>* pCredentials, void* pArg);

struct ICrySignIn
{
	// <interfuscator:shuffle>
	virtual ~ICrySignIn() {}

	//! Sign a user in.
	//! \param user				    local user index
	//! \param credentials    credentials of user
	//! \param pTaskID		    lobby task ID
	//! \param pCB				    callback
	//! \param pCBArg			    callback argument
	//! \return			    eCLE_Success if task started successfully, otherwise a failure code
	virtual ECryLobbyError SignInUser(uint32 user, const TUserCredentials& credentials, CryLobbyTaskID* pTaskID, CrySignInCallback pCb, void* pCbArg) = 0;

	//! Sign a user out.
	//! \param user				    local user index
	//! \param pTaskID		    lobby task ID
	//! \param pCB				    callback
	//! \param pCBArg			    callback argument
	//! \return			    eCLE_Success if task started successfully, otherwise a failure code
	virtual ECryLobbyError SignOutUser(uint32 user, CryLobbyTaskID* pTaskID, CrySignInCallback pCb, void* pCbArg) = 0;

	//! Create a new user.
	//! \param user				    local user index
	//! \param credentials    credentials of new user
	//! \param pTaskID		    lobby task ID
	//! \param pCB				    callback
	//! \param pCBArg			    callback argument
	//! \return			    eCLE_Success if task started successfully, otherwise a failure code
	virtual ECryLobbyError CreateUser(uint32 user, const TUserCredentials& credentials, CryLobbyTaskID* pTaskID, CrySignInCreateUserCallback pCb, void* pCbArg) = 0;

	//! Create and sign in a new user.
	//! \param user				    local user index
	//! \param credentials    credentials of new user
	//! \param pTaskID		    lobby task ID
	//! \param pCB				    callback
	//! \param pCBArg			    callback argument
	//! \return			    eCLE_Success if task started successfully, otherwise a failure code
	virtual ECryLobbyError CreateAndSignInUser(uint32 user, const TUserCredentials& credentials, CryLobbyTaskID* pTaskID, CrySignInCreateUserCallback pCb, void* pCbArg) = 0;

	//! Get the given user's credentials.
	//! \param user				    local user index
	//! \param pTaskID		    lobby task ID
	//! \param pCB				    callback
	//! \param pCBArg			    callback argument
	//! \return			    eCLE_Success if task started successfully, otherwise a failure code
	virtual ECryLobbyError GetUserCredentials(uint32 user, CryLobbyTaskID* pTaskID, CrySignInGetUserCredentialsCallback pCB, void* pCBArg) = 0;

	//! Cancel a task
	//! \param lTaskID		     lobby task ID
	virtual void CancelTask(CryLobbyTaskID lTaskID) = 0;
	// </interfuscator:shuffle>
};
