// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   ScriptBind_AI.h
   Version:     v1.00
   Description: Core scriptbinds for the AI system

   -------------------------------------------------------------------------
   History:
   - 3:11:2004   - Created by Kirill Bulatsev
   -13:02:2008   - Moved into AI system by Matthew

 *********************************************************************/

#ifndef __ScriptBind_AI_H__
#define __ScriptBind_AI_H__

#pragma once

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>

// These numerical values are deprecated; use the strings instead
enum EAIPathType
{
	AIPATH_DEFAULT,
	AIPATH_HUMAN,
	AIPATH_HUMAN_COVER,
	AIPATH_CAR,
	AIPATH_TANK,
	AIPATH_BOAT,
	AIPATH_HELI,
	AIPATH_3D,
	AIPATH_SCOUT,
	AIPATH_TROOPER,
	AIPATH_HUNTER,
};

struct IAISystem;
struct ISystem;
struct AgentMovementAbility;

class CScriptBind_AI :
	public CScriptableBase
{
public:

	CScriptBind_AI();
	virtual ~CScriptBind_AI();

	enum EEncloseDistanceCheckType
	{
		CHECKTYPE_MIN_DISTANCE,
		CHECKTYPE_MIN_ROOMSIZE
	};

	void Release() { delete this; };

	bool RunStartupScript(bool reload);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Log functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.LogProgress( szMessage )</code>
	//! <description>Used to indicate "progress markers"">e.g. during loading.</description>
	//!		<param name="szMessage">message line to be displayed in log</param>
	int LogProgress(IFunctionHandler* pH);

	//! <code>AI.LogEvent( szMessage )</code>
	//! <description>
	//!		Used to indicate event-driven info that would be useful for debugging
	//!		(may occur on a per-frame or even per-AI-update basis).
	//!	</description>
	//!		<param name="szMessage">message line to be displayed in log</param>
	int LogEvent(IFunctionHandler* pH);

	//! <code>AI.LogComment( szMessage )</code>
	//! <description>Used to indicate info that would be useful for debugging,
	//!				but there's too much of it for it to be enabled all the time.</description>
	//!		<param name="szMessage">message line to be displayed in log</param>
	int LogComment(IFunctionHandler* pH);

	//! <code>AI.Warning( szMessage )</code>
	//! <description>Used for warnings about data/script errors.</description>
	//!		<param name="szMessage">message line to be displayed in log</param>
	int Warning(IFunctionHandler* pH);

	//! <code>AI.Error( szMessage )</code>
	//! <description>
	//!		Used when we really can't handle some data/situation.
	//!		The code following this should struggle on so that the original cause (e.g. data)
	//!		of the problem can be fixed in the editor, but in game this would halt execution.
	//! </description>
	//!		<param name="szMessage">message line to be displayed in log</param>
	int Error(IFunctionHandler* pH);

	//! <code>AI.RecComment( szMessage )</code>
	//! <description>Record comment with AIRecorder.</description>
	//!		<param name="szMessage">message line to be displayed in recorder view</param>
	int RecComment(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// General AI System functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.ResetParameters( entityId, bProcessMovement, PropertiesTable, PropertiesInstanceTable )</code>
	//! <description>Resets all the AI parameters.</description>
	//!		<param name="entityId">entity id</param>
	//!		<param name="bProcessMovement">reset movement data as well</param>
	//!		<param name="PropertiesTable">Lua table containing the entity properties like groupid, sightrange, sound range etc (editable in editor, usually defined as "Properties")</param>
	//!		<param name="PropertiesInstanceTable">another properties table, same as PropertiesTable (editable in editor, usually defined as "PropertiesInstance")</param>
	int ResetParameters(IFunctionHandler* pH);

	//! <code>AI.ChangeParameter( entityId, paramEnum, paramValue )</code>
	//! <description>Changes an enumerated AI parameter.</description>
	//!		<param name="entityId - entity id</param>
	//!		<param name="paramEnum - the parameter to change, should be one of the following:
	//!			AIPARAM_SIGHTRANGE - sight range in (meters).
	//!			AIPARAM_ATTACKRANGE	- attack range in (meters).
	//!			AIPARAM_ACCURACY - firing accuracy (real [0..1]).
	//!			AIPARAM_AGGRESION - aggression (real [0..1]).
	//!			AIPARAM_GROUPID	- group id (integer).
	//!			AIPARAM_FOV_PRIMARY - primary field of vision (degrees).
	//!			AIPARAM_FOV_SECONDARY - pheripheral field of vision (degrees).
	//!			AIPARAM_COMMRANGE - communication range (meters).
	//!			AIPARAM_FWDSPEED - forward speed (vehicles only).
	//!			AIPARAM_RESPONSIVENESS - responsiveness (real).
	//!			AIPARAM_SPECIES - entity species (integer).
	//!			AIPARAM_TRACKPATTERN - track pattern name (string).
	//!			AIPARAM_TRACKPATTERN_ADVANCE - track pattern advancing (0 = stop, 1 = advance).</param>
	//!		<param name="paramValue">new parameter value, see above for type and meaning.</param>
	int ChangeParameter(IFunctionHandler* pH);

	//! <code>AI.IsEnabled( entityId )</code>
	//! <description>Returns true if entity's AI is enabled.</description>
	//!		<param name="entityId">entity id</param>
	int IsEnabled(IFunctionHandler* pH);

	//! <code>AI.GetTypeOf( entityId )</code>
	//! <description>returns the given entity's type.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>type of given entity (as defined in IAgent.h)</returns>
	int GetTypeOf(IFunctionHandler* pH);

	//! <code>AI.GetSubTypeOf( entityId )</code>
	//! <description>returns the given entity's sub type.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>sub type of given entity (as defined in IAgent.h).</returns>
	// TODO : only returns subtypes below 26/06/2006
	// AIOBJECT_CAR
	// AIOBJECT_BOAT
	// AIOBJECT_HELICOPTER
	int GetSubTypeOf(IFunctionHandler* pH);

	//! <code>AI.GetParameter( entityId, paramEnum )</code>
	//! <description>Changes an enumerated AI parameter.</description>
	//!		<param name="entityId">entity id</param>
	//!		<param name="paramEnum">index of the parameter to get, see AI.ChangeParameter() for complete list</param>
	//! <returns>parameter value</returns>
	// INTEGRATION : (MATT) GetAIParameter will appear from main branch and is duplicate {2007/08/03:11:00:39}
	// Embracing and extending their code whilst keeping the old name - wish to remain consistent with ChangeParameter above
	int GetParameter(IFunctionHandler* pH);

	//! <code>AI.ChangeMovementAbility( entityId, paramEnum, paramValue )</code>
	//! <description>Changes an enumerated AI movement ability parameter.</description>
	//!		<param name="entityId">entity id</param>
	//!		<param name="paramEnum">the parameter to change, should be one of the following:
	//!			AIMOVEABILITY_OPTIMALFLIGHTHEIGHT - optimal flight height while finding path (meters).
	//!			AIMOVEABILITY_MINFLIGHTHEIGHT - minimum flight height while finding path (meters).
	//!			AIMOVEABILITY_MAXFLIGHTHEIGHT - maximum flight height while finding path (meters).</param>
	//!		<param name="paramValue">new parameter value, see above for type and meaning.</param>
	int ChangeMovementAbility(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Smart Object functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	   //! <code>AI.AddSmartObjectCondition( smartObjectTable )</code>
	   //! <description>Registers the given smartObjectTable to AI System.</description>
	   //!		<param name="smartObjectTable">a table defining conditions on which specified Smart Object could be used
	   int AddSmartObjectCondition(IFunctionHandler *pH);
	 */

	//! <code>AI.ExecuteAction( action, participant1 [, participant2 [, ... , participantN ] ] )</code>
	//! <description>Executes an Action on a set of Participants.</description>
	//!		<param name="action">Smart Object Action name or id</param>
	//!		<param name="participant1">entity id of the first Participant in the Action</param>
	//!		<param name="(optional) participant2..N">entity ids of other participants</param>
	int ExecuteAction(IFunctionHandler* pH);

	//! <code>AI.AbortAction( userId [, actionId ] )</code>
	//! <description>Aborts execution of an action if actionId is specified, or aborts execution of all actions if actionId is nil or 0</description>
	//!		<param name="userId">entity id of the user which AI action is aborted</param>
	//!		<param name="actionId (optional)">id of action to be aborted or 0 (or nil) to abort all actions on specified entity</param>
	int AbortAction(IFunctionHandler* pH);

	//! <code>AI.SetSmartObjectState( entityId, stateName )</code>
	//! <description>Sets only one single smart object state replacing all other states.</description>
	//!		<param name="entityId">entity id</param>
	//!		<param name="stateName">name of the new state (i.e. "Idle")</param>
	int SetSmartObjectState(IFunctionHandler* pH);

	//! <code>AI.ModifySmartObjectStates( entityId, listStates )</code>
	//! <description>Adds/Removes smart object states of a given entity.</description>
	//!		<param name="entityId">entity id</param>
	//!		<param name="listStates">names of the states to be added and removed (i.e. "Closed, Locked">Open, Unlocked, Busy")</param>
	int ModifySmartObjectStates(IFunctionHandler* pH);

	//! <code>AI.SmartObjectEvent( actionName, userEntityId, objectEntityId [, vRefPoint] )</code>
	//! <description>Executes a smart action.</description>
	//!		<param name="actionName">smart action name</param>
	//!		<param name="usedEntityId">entity id of the user who needs to execute the smart action or a nil if user is unknown</param>
	//!		<param name="objectEntityId">entity id of the object on which the smart action needs to be executed or a nil if objects is unknown</param>
	//!		<param name="vRefPoint">(optional) point to be used instead of user's attention target position</param>
	//! <returns>0 if smart object rule wasn't found or a non-zero unique id of the goal pipe inserted to execute the action.</returns>
	int SmartObjectEvent(IFunctionHandler* pH);

	//! <code>AI.GetLastUsedSmartObject( userEntityId )</code>
	//! <description>Returns the last used smart object.</description>
	//!		<param name="usedEntityId">entity id of the user for which its last used smart object is needed</param>
	//! <returns>
	//!		nil if there's no last used smart object (or if an error has occurred)
	//!		or the script table of the entity which is the last used smart object
	//! </returns>
	int GetLastUsedSmartObject(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// GoalPipe functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.CreateGoalPipe( szPipeName )</code>
	//! <description>Used for warnings about data/script errors.</description>
	//!		<param name="szPipeName">goal pipe name</param>
	int CreateGoalPipe(IFunctionHandler* pH);

	//! <code>AI.BeginGoalPipe( szPipeName )</code>
	//! <description>Creates a goal pipe and allows to start filling it.</description>
	//!		<param name="szPipeName">goal pipe name</param>
	int BeginGoalPipe(IFunctionHandler* pH);

	//! <code>AI.EndGoalPipe()</code>
	//! <description>Ends creating a goal pipe.</description>
	int EndGoalPipe(IFunctionHandler* pH);

	//! <code>AI.BeginGroup()</code>
	//! <description>to define group of goals.</description>
	int BeginGroup(IFunctionHandler* pH);

	//! <code>AI.EndGroup()</code>
	//! <description>Defines the end of a group of goals.</description>
	int EndGroup(IFunctionHandler* pH);

	//! <code>AI.PushGoal( szPipeName, goalName, blocking [,{params}] )</code>
	//! <description>Used for warnings about data/script errors.</description>
	//!		<param name="szPipeName">goal pipe name</param>
	//!		<param name="szGoalName">goal name - see AI Manual for a complete list of goals</param>
	//!		<param name="blocking">used to mark the goal as blocking (goal pipe execution will stop here</param>
	//!		<param name="until the goal has finished) 0: not blocking, 1: blocking</param>
	//!		<param name="(optional) params">set of parameters depending on the goal selected; see the AI Manual for a complete list of the parameters for each goal</param>
	int PushGoal(IFunctionHandler* pH);

	//! <code>AI.PushLabel( szPipeName, szLabelName )</code>
	//! <description>Used in combination with "branch" goal operation to identify jump destination.</description>
	//!		<param name="szPipeName">goalpipe name</param>
	//!		<param name="szLabelName">label name</param>
	int PushLabel(IFunctionHandler* pH);

	//! <code>AI.IsGoalPipe( szPipeName )</code>
	//! <description>Checks is a goalpipe of certain name exists already, returns true if pipe exists.</description>
	//!		<param name="szPipeName">goalpipe name</param>
	int IsGoalPipe(IFunctionHandler* pH);

	//! <code>AI.ClearForReload( szPipeName )</code>
	//! <description>Clears all goalpipes from the system.</description>
	int ClearForReload(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Signal functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.Signal(signalFilter, signalType, SignalText, senderId [, signalExtraData] )</code>
	//! <description>Sends a signal to an entity or a group of entities.</description>
	//!		<param name="sender Id: sender's entity id; despite the name, sender may not be necessarily the actual AI.Signal function caller
	//!		<param name="signalFilter - filter which defines the subset of AIobjects which will receive the signal:
	//!			SIGNALFILTER_SENDER: signal is sent to the sender only (may be not the caller itself in this case)
	//!			SIGNALFILTER_GROUPONLY: signal is sent to all the AIObjects in the same sender's group, within its communication range
	//!			SIGNALFILTER_GROUPONLY_EXCEPT: signal is sent to all the AIObjects in the same sender's group, within its communication range, except the sender itself
	//!			SIGNALFILTER_SUPERGROUP: signal is sent signal is sent to all the AIObjects in the same sender's group
	//!			SIGNALFILTER_SPECIESONLY: signal is sent to all the AIObjects in the same sender's species, in his comm range
	//!			SIGNALFILTER_SUPERSPECIES: signal is sent to all AIObjects in the same sender's species
	//!			SIGNALFILTER_ANYONEINCOMM: signal is sent to all the AIObjects in sender's communication range
	//!			SIGNALFILTER_NEARESTGROUP: signal is sent to the nearest AIObject in the sender's group
	//!			SIGNALFILTER_NEARESTINCOMM: signal is sent to the nearest AIObject, in the sender's group, within sender's communication range
	//!			SIGNALFILTER_NEARESTINCOMM_SPECIES: signal is sent to the nearest AIObject, in the sender's species, within sender's communication range
	//!			SIGNALFILTER_HALFOFGROUP: signal is sent to the first half of the AI Objects in the sender's group
	//!			SIGNALFILTER_LEADER: signal is sent to the sender's group CLeader Object
	//!			SIGNALFILTER_LEADERENTITY: signal is sent to the sender's group leader AIObject (which has the CLeader associated)</param>
	//!		<param name="signalType -
	//!			-1: signal is processed by all entities, even if they're disabled/sleeping
	//!			 0: signal is processed by all entities which are enabled
	//!			 1: signal is processed by all entities which are enabled and not sleeping</param>
	//!			10: signal is added in the sender's signal queue even if another signal with same text is present (normally it's not)
	//!		<param name="signalText: signal text which will be processed by the receivers (in a Lua callback with the same name as the text, or directly by the CAIObject)</param>
	//!		<param name="(optional) signalExtraData: lua table which can be used to send extra data. It can contains the following values:
	//!			point: a vector in format {x,y,z}
	//!			point2: a vector in format {x,y,z}
	//!			ObjectName: a string
	//!			id: an entity id
	//!			fValue: a float value
	//!			iValue: an integer value
	//!			iValue2: a second integer value</param>
	int Signal(IFunctionHandler* pH);

	int NotifyGroup(IFunctionHandler* pH, int groupID, ScriptHandle sender, const char* notification);

	//! <code>AI.FreeSignal( signalType, SignalText, position, radius [, entityID [,signalExtraData]] )</code>
	//! <description>Sends a signal to anyone in a given radius around a position.</description>
	//!		<param name="signalType">see AI.Signal</param>
	//!		<param name="signalText">See AI.Signal</param>
	//!		<param name="position">center position {x,y,z}around which the signal is sent</param>
	//!		<param name="radius">radius inside which the signal is sent</param>
	//!		<param name="(optional) entityID">signal is NOT sent to entities which groupID is same as the entity's one</param>
	//!		<param name="(optional) signalExtraData">See AI.Signal</param>
	int FreeSignal(IFunctionHandler* pH);

	//! <code>AI.SetIgnorant( entityId, ignorant )</code>
	//! <description>makes an AI ignore system signals, visual stimuli and sound stimuli.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="ignorant">0: not ignorant, 1: ignorant</param>
	int SetIgnorant(IFunctionHandler* pH);

	int BreakEvent(IFunctionHandler* pH, ScriptHandle entityID, Vec3 pos, float radius);
	int AddCoverEntity(IFunctionHandler* pH, ScriptHandle entityID);
	int RemoveCoverEntity(IFunctionHandler* pH, ScriptHandle entityID);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	//// Group-Species functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.GetGroupCount( entityId, flags, type )</code>
	//! <description>returns the given entity's group members count.</description>
	//!		<param name="entityId">AI's entity id or groupId - AI's group id</param>
	//!		<param name="flags">combination of following flags:
	//!			GROUP_ALL - Returns all agents in the group (default).
	//!			GROUP_ENABLED - Returns only the count of enabled agents (exclusive with all).
	//!			GROUP_MAX - Returns the maximum number of agents during the game (can be combined with all or enabled).</param>
	//!		<param name="type">allows to filter to return only specific AI objects by type (cannot be used in together with GROUP_MAX flag).</param>
	//! <returns>group members count</returns>
	int GetGroupCount(IFunctionHandler* pH);

	//! <code>AI.GetGroupMember( entityId|groupId, idx, flags, type)</code>
	//! <description>returns the idx-th entity in the given group.</description>
	//!		<param name="entityId|groupId">AI's entity id \ group id</param>
	//!		<param name="idx">index  (1..N)</param>
	//!		<param name="flags">combination of following flags:
	//!			GROUP_ALL - Returns all agents in the group (default).
	//!			GROUP_ENABLED - Returns only the count of enabled agents (exclusive with all).</param>
	//!		<param name="type">allows to filter to return only specific AI objects by type (cannot be used in together with GROUP_MAX flag).</param>
	//! <returns>script handler of idx-th entity (null if idx is out of range)</returns>
	int GetGroupMember(IFunctionHandler* pH);

	//! <code>AI.GetGroupOf( entityId )</code>
	//! <description>returns the given entity's group id.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>group id of given entity</returns>
	int GetGroupOf(IFunctionHandler* pH);

	//! <code>AI.GetFactionOf( entityID )</code>
	//! <returns>The faction of the specified entity.</returns>
	int GetFactionOf(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.SetFactionOf( entityID, factionName )</code>
	//! <description>Make the specified entity a member of the named faction.</description>
	int SetFactionOf(IFunctionHandler* pH, ScriptHandle entityID, const char* factionName);

	int GetReactionOf(IFunctionHandler* pH);
	int SetReactionOf(IFunctionHandler* pH, const char* factionOne, const char* factionTwo, int reaction);

	//! <code>AI.GetUnitInRank( groupID [,rank] )</code>
	//! <description>Returns the entity in the group  id in the given rank position, or the highest if rank == nil or rank <=0
	//!	the rank is specified in entity.Properties.nRank
	//! </description>
	//!		<param name="rank">rank position (the highest (1) if rank == nil or rank <=0)</param>
	//! <returns>entity script table of the ranked unit</returns>
	int GetUnitInRank(IFunctionHandler* pH);

	//! <code>AI.SetUnitProperties( entityId, unitProperties )</code>
	//! <description>Sets the leader knowledge about the units combat capabilities.
	//!		The leader will be found based on the group id of the entity.
	//! </description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="unitProperties">binary mask of unit properties type for which the attack is requested
	//!			(in the form of UPR_* + UPR* i.e. UPR_COMBAT_GROUND + UPR_COMBAT_FLIGHT)
	//!			see IAgent.h for definition of unit properties UPR_*</param>
	int SetUnitProperties(IFunctionHandler* pH);

	//! <code>AI.GetUnitCount( entityId, unitProperties )</code>
	//! <description>Gets the number of units the leader knows about.
	//!     The leader will be found based on the group id of the entity.
	//! </description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="unitProperties">binary mask of the returned unit properties type
	//!			(in the form of UPR_* + UPR* i.e. UPR_COMBAT_GROUND + UPR_COMBAT_FLIGHT)
	//!			see IAgent.h for definition of unit properties UPR_*</param>
	int GetUnitCount(IFunctionHandler* pH);

	//! <code>AI.Hostile( entityId, entity2Id|AIObjectName )</code>
	//! <description>returns true if the two entities are hostile.</description>
	//!		<param name="entityId">1st AI's entity id</param>
	//!		<param name="entity2Id | AIObjectName">2nd AI's entity id | 2nd AIobject's name</param>
	//! <returns>true if the entities are hostile</returns>
	int Hostile(IFunctionHandler* pH);

	//! <code>AI.GetLeader( groupID | entityID )</code>
	//! <returns>the leader's name of the given groupID / entity</returns>
	//!		<param name="groupID">group id</param>
	//!		<param name="entityID">entity (id) of which we want to find the leader</param>
	int GetLeader(IFunctionHandler* pH);

	//! <code>AI.SetLeader( entityID )</code>
	//! <description>
	//!		Set the given entity as Leader (associating a CLeader object to it and creating it if it doesn't exist)
	//!		Only one leader can be set per group.
	//! </description>
	//!		<param name="entityID">entity (id) of which to which we want to associate a leader class</param>
	//! <returns>true if the leader creation/association has been successful</returns>
	int SetLeader(IFunctionHandler* pH);

	//! <code>AI.GetBeaconPosition( entityId | AIObjectName, returnPos )</code>
	//! <description>get the beacon's position for the given entity/object's group.</description>
	//!		<param name="entityId | AIObjectName">AI's entity id | AI object name</param>
	//!		<param name="returnPos">vector {x,y,z} where the beacon position will be set</param>
	//! <returns>true if the beacon has been found and the position set</returns>
	int GetBeaconPosition(IFunctionHandler* pH);

	//! <code>AI.SetBeaconPosition( entityId | AIObjectName, pos )</code>
	//! <description>Set the beacon's position for the given entity/object's group.</description>
	//!		<param name="entityId | AIObjectName">AI's entity id | AI object name</param>
	//!		<param name="pos">beacon position vector {x,y,z}</param>
	int SetBeaconPosition(IFunctionHandler* pH);

	//! <code>AI.RequestAttack( entityId, unitProperties, attackTypeList [,duration] )</code>
	//! <description>in a group with leader, the entity requests for a group attack behavior against the enemy
	//!		The Cleader later will possibly create an attack leader action (CLeaderAction_Attack_*)
	//! </description>
	//!		<param name="entityId">AI's entity id which group/leader is determined</param>
	//!		<param name="unitProperties">binary mask of unit properties type for which the attack is requested
	//!			(in the form of UPR_* + UPR* i.e. UPR_COMBAT_GROUND + UPR_COMBAT_FLIGHT)
	//!			see IAgent.h for definition of unit properties UPR_*</param>
	//!		<param name="attackTypeList">a lua table which contains the list of preferred attack strategies (Leader action subtypes),
	//!			sorted by priority (most preferred first)
	//!			the list must be in the format {LAS_*, LAS_*,..} i.e. (LAS_ATTACK_ROW,LAS_ATTACK_FLANK} means that first an Attack_row
	//!			action will be tried, then an attack_flank if the first ends/fails.
	//!			see IAgent.h for definition of LeaderActionSubtype (LAS_*) action types</param>
	//!		<param name="duration">(optional) max duration in sec (default = 0)</param>
	int RequestAttack(IFunctionHandler* pH);

	//! <code>AI.GetGroupTarget( entityId [,bHostileOnly [,bLiveOnly]] )</code>
	//! <description>Returns the most threatening attention target amongst the agents in the given entity's group. (see IAgent.h for definition of alert status)</description>
	//!		<param name="entityId">AI's entity id which group is determined</param>
	//!		<param name="bHostileOnly (optional)">filter only hostile targets in group</param>
	//!		<param name="bLiveOnly (optional)">filter only live targets in group (entities)</param>
	int GetGroupTarget(IFunctionHandler* pH);

	//! <code>AI.GetGroupTargetType( groupID )</code>
	int GetGroupTargetType(IFunctionHandler* pH, int groupID);

	//! <code>AI.GetGroupTargetThreat( groupID )</code>
	int GetGroupTargetThreat(IFunctionHandler* pH, int groupID);

	//! <code>AI.GetGroupTargetEntity( groupID )</code>
	int GetGroupTargetEntity(IFunctionHandler* pH, int groupID);

	//! <code>AI.GetGroupScriptTable( groupID )</code>
	int GetGroupScriptTable(IFunctionHandler* pH, int groupID);

	//! <code>AI.GetGroupTargetCount( entityId [,bHostileOnly [,bLiveOnly]] )</code>
	//! <description>Returns the number of attention targets amongst the agents in the given entity's group.</description>
	//!		<param name="entityId">AI's entity id which group is determined</param>
	//!		<param name="bHostileOnly (optional)">filter only hostile targets in group</param>
	//!		<param name="bLiveOnly (optional)">filter only live targets in group (entities)</param>
	int GetGroupTargetCount(IFunctionHandler* pH);

	//! <code>AI.GetGroupAveragePosition( entityId, properties, returnPos )</code>
	//! <description>gets the average position of the (leader's) group members</description>
	//!		<param name="entityId">AI's entity id which group/leader is determined</param>
	//!		<param name="unitProperties">binary mask of unit properties type for which the attack is requested
	//!			(in the form of UPR_* + UPR* i.e. UPR_COMBAT_GROUND + UPR_COMBAT_FLIGHT)
	//!			see IAgent.h for definition of unit properties UPR_*</param>
	//! <returns>Pos returned average position</returns>
	int GetGroupAveragePosition(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Object finding
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.FindObjectOfType( entityId, radius, AIObjectType, flags [,returnPosition [,returnDirection]] )
	//!		  AI.FindObjectOfType( position, radius, AIObjectType, [,returnPosition [,returnDirection]] )
	//! </code>
	//! <description>returns the closest AIObject of a given type around a given entity/position;
	//!				once an AIObject is found, it's devalued and can't be found again for some seconds (unless specified in flags).
	//! </description>
	//!		<param name="entityId">AI's entity id as a center position of the search (if 1st parameter is not a vector)</param>
	//!		<param name="position">center position of the search (if 1st parameter is not an entity id)</param>
	//!		<param name="radius">radius inside which the object must be found</param>
	//!		<param name="AIObjectType">AIObject type; see ScriptBindAI.cpp and Scripts/AIAnchor.lua for a complete list of AIObject types available</param>
	//!		<param name="flags">filter for AI Objects to search, which can be the sum of one or more of the following values:
	//!			AIFAF_VISIBLE_FROM_REQUESTER: Requires whoever is requesting the anchor to also have a line of sight to it
	//!     AIFAF_VISIBLE_TARGET: Requires that there is a line of sight between target and anchor
	//!     AIFAF_INCLUDE_DEVALUED: don't care if the object is devalued
	//!			AIFAF_INCLUDE_DISABLED: don't care if the object is disabled</param>
	//!		<param name="(optional) returnPosition">position of the found object</param>
	//!		<param name="(optional) returnDirection">direction of the found object</param>
	//! <returns>the found AIObject's name (nil if not found)</returns>
	int FindObjectOfType(IFunctionHandler* pH);

	//! <code>AI.GetAnchor( entityId, radius, AIAnchorType, searchType [,returnPosition [,returnDirection]] )</code>
	//! <description>returns the closest Anchor of a given type around a given entity;
	//!				once an Anchor is found, it's devalued and can't be found again for some seconds (unless specified in flags).
	//! </description>
	//!		<param name="entityId">AI's entity id as a center position of the search</param>
	//!		<param name="radius">radius inside which the object must be found. Alternatively a search range can be specified {min=minRad,max=maxRad}.</param>
	//!		<param name="AIAnchorType">Anchor type; see Scripts/AIAnchor.lua for a complete list of Anchor types available</param>
	//!		<param name="searchType">search type filter, which can be one of the following values:
	//!			AIANCHOR_NEAREST: (default) the nearest anchor of the given type
	//!			AIANCHOR_NEAREST_IN_FRONT: the nearest anchor inside the front cone of entity
	//!			AIANCHOR_NEAREST_FACING_AT: the nearest anchor of given type which is oriented towards entity's attention target (...Dejan? )
	//!			AIANCHOR_RANDOM_IN_RANGE: a random anchor of the given type
	//!			AIANCHOR_NEAREST_TO_REFPOINT: the nearest anchor of given type to the entity's reference point</param>
	//!		<param name="(optional) returnPosition">position of the found object</param>
	//!		<param name="(optional) returnDirection">direction of the found object</param>
	//! <returns>the found Anchor's name (nil if not found)</returns>
	int GetAnchor(IFunctionHandler* pH);

	//! <code>AI.GetNearestEntitiesOfType( entityId|objectname|position, AIObjectType, maxObjects, returnList [,objectFilter [,radius]] )</code>
	//! <description>returns a list of the closest N entities of a given AIObjkect type associated the found objects are then devalued.</description>
	//!		<param name="entityId | objectname |position">AI's entity id | name| position as a center position of the search</param>
	//!		<param name="radius">radius inside which the entities must be found</param>
	//!		<param name="AIObjectType">AIObject type; see ScriptBindAI.cpp and Scripts/AIAnchor.lua for a complete list of AIObject types available</param>
	//!		<param name="maxObjects">maximum number of objects to find</param>
	//!		<param name="return list">Lua table which will be filled up with the list of the found entities (Lua handlers)</param>
	//!		<param name="(optional) objectFilter">search type filter, which can be the sum of one or more of the following values:
	//!			AIOBJECTFILTER_SAMEFACTION: AI objects of the same species of the querying object
	//!			AIOBJECTFILTER_SAMEGROUP: AI objects of the same group of the querying object or with no group
	//!			AIOBJECTFILTER_NOGROUP :AI objects with Group ID ==AI_NOGROUP
	//!			AIOBJECTFILTER_INCLUDEINACTIVE :Includes objects which are inactivated.</param>
	//!		<param name="(optional) radius - serch range( default 30m )</param>
	//! <returns>the number of found entities</returns>
	int GetNearestEntitiesOfType(IFunctionHandler* pH);

	//! <code>AI.GetAIObjectPosition( entityId | AIObjectName )</code>
	//! <description>get the given AIObject's position.</description>
	//!		<param name="entityId | AIObjectName">AI's entity id | AI object name</param>
	//! <returns>AI Object position vector {x,y,z}</returns>
	int GetAIObjectPosition(IFunctionHandler* pH);

	//! <code>AI.IgnoreCurrentHideObject( entityId )</code>
	//! <description>Marks the current hideobject unreachable (will be omitted from future hidespot selections).</description>
	//!		<param name="entityId">AI's entity id</param>
	int IgnoreCurrentHideObject(IFunctionHandler* pH);

	//! <code>AI.GetCurrentHideAnchor( entityId )</code>
	//! <description>Returns the name of the current anchor the entity is using for cover.</description>
	//!		<param name="entityId">AI's entity id</param>
	int GetCurrentHideAnchor(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Attention Target / perception related functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	int GetForwardDir(IFunctionHandler* pH);

	//! <code>AI.GetAttentionTargetOf( entityId )</code>
	//! <description>returns the given entity's attention target.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>attention target's name (nil if no target)</returns>
	int GetAttentionTargetOf(IFunctionHandler* pH);

	//! <code>AI.GetAttentionTargetAIType( entityId )</code>
	//! <description>returns the given entity's attention target AI type (AIOBJECT_*).</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>attention target's type (AIOBJECT_NONE if no target)</returns>
	int GetAttentionTargetAIType(IFunctionHandler* pH);

	//! <code>AI.GetAttentionTargetType( entityId )</code>
	//! <description>returns the given entity's attention target type (AITARGET_*).</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>attention target's type (AITARGET_NONE if no target)</returns>
	int GetAttentionTargetType(IFunctionHandler* pH);

	//! <code>AI.GetAttentionTargetEntity( entityId )</code>
	//! <description>returns the given entity's attention target entity (if it is an entity) or
	//!   the owner entity of the attention target if it is a dummy object (if there is an owner entity).
	//! </description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>attention target's entity</returns>
	int GetAttentionTargetEntity(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.GetAttentionTargetPosition( entityId, returnPos )</code>
	//! <description>returns the given entity's attention target's position.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>Pos - vector {x,y,z} passed as a return value (attention target 's position)</returns>
	int GetAttentionTargetPosition(IFunctionHandler* pH);

	//! <code>AI.GetAttentionTargetDirection( entityId, returnDir )</code>
	//! <description>returns the given entity's attention target's direction.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>Dir - vector {x,y,z} passed as a return value (attention target 's direction)</returns>
	int GetAttentionTargetDirection(IFunctionHandler* pH);

	//! <code>AI.GetAttentionTargetViewDirection( entityId, returnDir )</code>
	//! <description>returns the given entity's attention target's view direction.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>Dir - vector {x,y,z} passed as a return value (attention target 's direction)</returns>
	int GetAttentionTargetViewDirection(IFunctionHandler* pH);

	//! <code>AI.GetAttentionTargetDistance( entityId )</code>
	//! <description>returns the given entity's attention target's distance to the entity.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>distance to the attention target (nil if no target)</returns>
	int GetAttentionTargetDistance(IFunctionHandler* pH);

	//! <code>AI.GetAttentionTargetThreat( entityID )</code>
	int GetAttentionTargetThreat(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.UpTargetPriority( entityId, targetId, increment )</code>
	//! <description>modifies the current entity's target priority for the given target if the given target is among the entity's target list.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="targetId">target's entity id</param>
	//!		<param name="increment">value to modify the target's priority</param>
	int UpTargetPriority(IFunctionHandler* pH);

	//! <code>AI.DropTarget( entityId, targetId )</code>
	//! <description>Clears the target from the AI's perception handler.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="targetId">target's entity id</param>
	int DropTarget(IFunctionHandler* pH);

	//! <code>AI.ClearPotentialTargets( entityId )</code>
	//! <description>Clears all the potential targets from the AI's perception handler.</description>
	//!		<param name="entityId">AI's entity id</param>
	int ClearPotentialTargets(IFunctionHandler* pH);

	//! <code>AI.SetTempTargetPriority( entityId, priority )</code>
	//! <description>Set the selection priority of the temp target over other potential targets.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name=" priority">New priority value</param>
	//! <returns>true if updated</returns>
	int SetTempTargetPriority(IFunctionHandler* pH);

	//! <code>AI.AddAggressiveTarget( entityId, targetId )</code>
	//! <description>Add the target Id as an aggressive potential target to the entity's list.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="targetId">Target to add to the list</param>
	//! <returns>true if updated</returns>
	int AddAggressiveTarget(IFunctionHandler* pH);

	//! <code>AI.UpdateTempTarget( entityId, vPos )</code>
	//! <description>Updates the entity's temporary potential target to the given position.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="vPos">New position of the temporary target</param>
	//! <returns>true if updated</returns>
	int UpdateTempTarget(IFunctionHandler* pH);

	//! <code>AI.ClearTempTarget( entityId )</code>
	//! <description>Removes the entity's temporary potential target, so it is no longer considered for target selection.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>true if cleared</returns>
	int ClearTempTarget(IFunctionHandler* pH);

	//! <code>AI.SetExtraPriority( enemyEntityId, increment )</code>
	//! <description>Set a extra priority value to the entity which is given by enemyEntityId.</description>
	//!		<param name="enemyEntityId">AI's entity id</param>
	//!		<param name="float increment">value to add to the target's priority</param>
	int SetExtraPriority(IFunctionHandler* pH);

	//! <code>AI.GetExtraPriority( enemyEntityId )</code>
	//! <description>get an extra priority value to the entity which is given by enemyEntityId.</description>
	//!		<param name="enemyEntityId">AI's entity id</param>
	int GetExtraPriority(IFunctionHandler* pH);

	//! <code>AI.GetTargetType(entityId)</code>
	//! <description>Returns the type of current entity's attention target (memory, human, none etc).</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>Target type value (AITARGET_NONE,AITARGET_MEMORY,AITARGET_BEACON,AITARGET_ENEMY etc) -
	//!		see IAgent.h for all definitions of target types.
	//! </returns>
	int GetTargetType(IFunctionHandler* pH);

	//! <code>AI.GetTargetSubType(entityId)</code>
	//! <description>Returns the subtype of current entity's attention target.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>Target subtype value - see IAgent.h for all definitions of target types</returns>
	int GetTargetSubType(IFunctionHandler* pH);

	//! <code>AI.RegisterTargetTrack(entityId, configuration, targetLimit, classThreat)</code>
	//! <description>Registers the AI object to use the given target track configuration for target selection.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="configuration">Target stimulus configuration to use</param>
	//!		<param name="targetLimit">Number of agents who can target the AI at any given time (0 for infinite)</param>
	//!		<param name="classThreat">(Optional) Initial class threat value for the user</param>
	//! <returns>True if registration was successful. Note that this does nothing if 'ai_TargetTracking' is not set to '2'</returns>
	int RegisterTargetTrack(IFunctionHandler* pH);

	//! <code>AI.UnregisterTargetTrack(entityId)</code>
	//! <description>Unregisters the AI object with the target track manager.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>True if unregistration was successful. Note that this does nothing if 'ai_TargetTracking' is not set to '2'.</returns>
	int UnregisterTargetTrack(IFunctionHandler* pH);

	//! <code>AI.SetTargetTrackClassThreat(entityId, classThreat)</code>
	//! <description>Sets the class threat for the entity for target track usage.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="classThreat">Class threat value for the user</param>
	int SetTargetTrackClassThreat(IFunctionHandler* pH);

	int TriggerCurrentTargetTrackPulse(IFunctionHandler* pH, ScriptHandle entityID, const char* stimulusName,
	                                   const char* pulseName);

	//! <code>AI.CreateStimulusEvent(entityId, targetId, stimulusName, pData)</code>
	//! <description>Creates a target track stimulus event for the entity using the specified data.</description>
	//!		<param name="ownerId">Owner's entity id (who owns and is receiving the event)</param>
	//!		<param name="targetId">Target's entity id (who sent the event, and becomes owner's perceived target)</param>
	//!		<param name="stimulusName">Name of the stimulus event</param>
	//!		<param name="pData">Data about the event (see TargetTrackHelpers::SStimulusEvent)</param>
	int CreateStimulusEvent(IFunctionHandler* pH, ScriptHandle ownerId, ScriptHandle targetId, const char* stimulusName, SmartScriptTable pData);
	int CreateStimulusEventInRange(IFunctionHandler* pH, ScriptHandle targetId, const char* stimulusName, SmartScriptTable dataScriptTable);

	//! <code>AI.SoundEvent(entityId, position, radius, type, sourceId)</code>
	//! <description>Generates a sound event in the AI system with the given parameters.</description>
	//!     <param name="entityId">who receives the sound event</param>
	//!		<param name="position">of the origin of the sound event</param>
	//!		<param name="radius">in which this sound event should be heard</param>
	//!		<param name="type">type of the sound event</param>
	//!		<param name="targetId">who the receiver is hearing</param>
	int SoundEvent(IFunctionHandler* pH);

	//! <code>AI.VisualEvent(entityId, sourceId)</code>
	//! <description>Generates a visual event in the AI system with the given parameters.</description>
	//!		<param name="entityId">who receives the visual event</param>
	//!		<param name="sourceId">who the receiver is seeing</param>
	int VisualEvent(IFunctionHandler* pH);

	//! <code>AI.GetSoundPerceptionDescriptor(entityId, soundType, descriptorTable)</code>
	//! <description>Fills descriptorTable with info about how perception works for the entity dealing with soundType.</description>
	//!		<param name="entityId">who to get the info from</param>
	//!		<param name="soundType">what type of sound stimulus to get the info for</param>
	//!		<param name="descriptorTable">where to store the info once retrieved</param>
	//! <returns>True if info was returned</returns>
	int GetSoundPerceptionDescriptor(IFunctionHandler* pH);

	//! <code>AI.SetSoundPerceptionDescriptor(entityId, soundType, descriptorTable)</code>
	//! <description>Sets data on how perception works for the entity dealing with soundType.</description>
	//!		<param name="entityId">who to set the info for</param>
	//!		<param name="soundType">what type of sound stimulus to set the info for</param>
	//!		<param name="descriptorTable">info to be set</param>
	//! <returns>True if info was saved</returns>
	int SetSoundPerceptionDescriptor(IFunctionHandler* pH);

	//! <code>AI.SetAssesmentMultiplier(AIObjectType, multiplier)</code>
	//! <description>set the assesment multiplier factor for the given AIObject type.</description>
	//!		<param name="AIObjectType">AIObject type; see ScriptBindAI.cpp for a complete list of AIObject types available</param>
	//!		<param name="multiplier">assesment multiplication factor</param>
	int SetAssesmentMultiplier(IFunctionHandler* pH);

	//! <code>AI.SetFactionThreatMultiplier(nSpecies, multiplier)</code>
	//! <description>set the threat multiplier factor for the given species (if 0, species is not hostile to any other).</description>
	//!		<param name="factionID: factionID</param>
	//!		<param name="multiplier">Threat multiplication factor</param>
	int SetFactionThreatMultiplier(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Reference point script methods
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.GetRefPointPosition(entityId)</code>
	//! <description>Get the entity's reference point World position.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>(script)vector (x,y,z) reference point position</returns>
	int GetRefPointPosition(IFunctionHandler* pH);

	//! <code>AI.GetRefPointDirection(entityId)</code>
	//! <description>Get the entity's reference point direction.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>(script)vector (x,y,z) reference point direction</returns>
	int GetRefPointDirection(IFunctionHandler* pH);

	//! <code>AI.SetRefPointPosition(entityId, vRefPointPos)</code>
	//! <description>Sets the reference point's World position of an entity</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="vRefPointPos">(script)vector (x,y,z) value</param>
	int SetRefPointPosition(IFunctionHandler* pH);

	//! <code>AI.SetRefPointDirection( vRefPointDir )</code>
	//! <description>Sets the reference point's World position of an entity</description>
	//!		<param name="vRefPointDir">(script)vector (x,y,z) value</param>
	int SetRefPointDirection(IFunctionHandler* pH);

	//! <code>AI.SetRefPointRadius(entityId, radius)</code>
	//! <description>Sets the reference point's radius.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="radius">the radius to set.</param>
	int SetRefPointRadius(IFunctionHandler* pH);

	//! <code>AI.SetRefShapeName(entityId, name)</code>
	//! <description>Sets the reference shape name.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="name">the name of the reference shape.</param>
	int SetRefShapeName(IFunctionHandler* pH);

	//! <code>AI.GetRefShapeName(entityId)</code>
	//! <returns>reference shape name.</returns>
	//!		<param name="entityId">AI's entity id</param>
	int GetRefShapeName(IFunctionHandler* pH);

	int SetVehicleStickTarget(IFunctionHandler* pH);

	//! <code>AI.SetTerritoryShapeName(entityId, shapeName)</code>
	//! <description>Sets the territory of the AI Actor.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="shapeName">name of the shape to set</param>
	int SetTerritoryShapeName(IFunctionHandler* pH);

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	//! <code>AI.SetCharacter(entityId, newCharater)</code>
	//! <description>Sets the AI character of the entity.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="newCharacter">name of the new character to set.</param>
	int SetCharacter(IFunctionHandler* pH);
#endif

	//! <code>AI.SetRefPointAtDefensePos(entityId, point2defend, distance)</code>
	//! <description>Set the entity refpoint position in an intermediate distance between the entity's att target and the given point.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="point2defend">point to defend</param>
	//!		<param name="distance">max distance to keep from the point</param>
	int SetRefPointAtDefensePos(IFunctionHandler* pH);

	//! <code>AI.SetPathToFollow(entityId, pathName)</code>
	//! <description>Set the name of the path to be used in 'followpath' goal operation.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pathName">(string) name of the path to set to be followed.</param>
	int SetPathToFollow(IFunctionHandler* pH);

	//! <code>AI.SetPathAttributeToFollow(entityId, flag)</code>
	//! <description>Set the attribute of the path to be used in 'followpath' goal operation.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="flag">.</param>
	int SetPathAttributeToFollow(IFunctionHandler* pH);

	//! <code>AI.GetNearestPathOfTypeInRange(entityId, pos, range, type [, devalue, useStartNode])</code>
	//! <description>
	//!		Queries a nearest path of specified type. The type uses same types as anchors and is specified in the path properties.
	//!		The function will only return paths that match the requesters (entityId) navigation caps. The nav type is also
	//!		specified in the path properties.
	//!	</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pos">a vector specifying to the point of interest. Path nearest to this position is returned.</param>
	//!		<param name="range">search range. If useStartNode=1, paths whose start point are within this range are returned or
	//!         if useStartNode=0 nearest distance to the path is calculated and compared against the range.</param>
	//!		<param name="type">type of path to return.</param>
	//!		<param name="devalue">(optional) specifies the time the returned path is marked as occupied.</param>
	//!		<param name="useStartNode">(optional) if set to 1 the range check will use distance to the start node on the path,
	//!             else nearest distance to the path is used.</param>
	int GetNearestPathOfTypeInRange(IFunctionHandler* pH);

	//! <code>AI.GetTotalLengthOfPath( entityId, pathname )</code>
	//! <returns>a total length of the path</returns>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pathname">designers path name</param>
	int GetTotalLengthOfPath(IFunctionHandler* pH);

	//! <code>AI.GetNearestPointOnPath( entityId, pathname, vPos )</code>
	//! <returns> a nearest point on the path from vPos</returns>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pathname">designers path name</param>
	//!		<param name="vPos">.</param>
	int GetNearestPointOnPath(IFunctionHandler* pH);

	//! <code>AI.GetPathSegNoOnPath( entityId, pathname, vPos )</code>
	//! <returns>segment ratio ( 0.0 start point 100.0 end point )</returns>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pathname">designer's path name</param>
	//!		<param name="vPos">.</param>
	int GetPathSegNoOnPath(IFunctionHandler* pH);

	//! <code>AI.GetPathSegNoOnPath( entityId, pathname, segNo )</code>
	//! <returns>returns point by segment ratio ( 0.0 start point 100.0 end point )</returns>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pathname">designer's path name</param>
	//!		<param name="segNo">segment ratio</param>
	int GetPointOnPathBySegNo(IFunctionHandler* pH);

	//! <code>AI.GetPathSegNoOnPath( entityId, pathname )</code>
	//! <returns>returns true if the path is looped</returns>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pathname">designer's path name</param>
	int GetPathLoop(IFunctionHandler* pH);

	//! <code>AI.GetPredictedPosAlongPath( entityId, time, retPos )</code>
	//! <description>Gets the agent predicted position along his path at a given time</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="time">prediction time (sec)</param>
	//!		<param name="retPos">return point value</param>
	//! <returns>true if successful</returns>
	int GetPredictedPosAlongPath(IFunctionHandler* pH);

	//! <code>AI.SetPointListToFollow(entityId, pointlist, howmanypoints, bspline [, navtype])</code>
	//! <description>Set a point list for followpath goal op</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pointList">should be like below
	//!					local	vectors = {
	//!						{ x = 0.0, y = 0.0, z = 0.0 }, -- vectors[1].(x,y,z)
	//!						{ x = 0.0, y = 0.0, z = 0.0 }, -- vectors[2].(x,y,z)
	//!						{ x = 0.0, y = 0.0, z = 0.0 }, -- vectors[3].(x,y,z)
	//!						{ x = 0.0, y = 0.0, z = 0.0 }, -- vectors[4].(x,y,z)
	//!						{ x = 0.0, y = 0.0, z = 0.0 }, -- vectors[5].(x,y,z)
	//!						{ x = 0.0, y = 0.0, z = 0.0 }, -- vectors[6].(x,y,z)
	//!						{ x = 0.0, y = 0.0, z = 0.0 }, -- vectors[7].(x,y,z)
	//!						{ x = 0.0, y = 0.0, z = 0.0 }, -- vectors[8].(x,y,z)
	//!					}</param>
	//!		<param name="howmanypoints">.</param>
	//!		<param name="bspline">if true, the line is recalcurated by spline interpolation.</param>
	//!		<param name="navtype">(optional) specify a navigation type ( default = IAISystem::NAV_FLIGHT )</param>
	int SetPointListToFollow(IFunctionHandler* pH);

	//! <code>AI.GetEnclosingGenericShapeOfType(position, type[, checkHeight])</code>
	//! <returns>the name of the first shape that is enclosing the specified point and is of specified type</returns>
	//!		<param name="position">the position to check</param>
	//!		<param name="type">the type of the shapes to check against (uses anchor types).</param>
	//!		<param name="checkHeight">(optional) Default=false, if the flag is set the height of the shape is tested too.
	//!			The test will check space between the shape.aabb.min.z and shape.aabb.min.z+shape.height.</param>
	int GetEnclosingGenericShapeOfType(IFunctionHandler* pH);

	//! <code>AI.IsPointInsideGenericShape(position, shapeName[, checkHeight])</code>
	//! <returns>true if the point is inside the specified shape.</returns>
	//!		<param name="position">the position to check</param>
	//!		<param name="shapeName">the name of the shape to test (returned by AI.GetEnclosingGenericShapeOfType)</param>
	//!		<param name="checkHeight">(optional) Default=false, if the flag is set the height of the shape is tested too.
	//!			The test will check space between the shape.aabb.min.z and shape.aabb.min.z+shape.height.</param>
	int IsPointInsideGenericShape(IFunctionHandler* pH);

	//! <code>AI.ConstrainPointInsideGenericShape(position, shapeName[, checkHeight])</code>
	//! <returns>the nearest point inside specified shape.</returns>
	//!		<param name="position">the position to check</param>
	//!		<param name="shapeName">the name of the shape to use as constraint.</param>
	//!		<param name="checkHeight">(optional) Default=false, if the flag is set the height should be constrained too.</param>
	int ConstrainPointInsideGenericShape(IFunctionHandler* pH);

	//! <code>AI.DistanceToGenericShape(position, shapeName[, checkHeight])</code>
	//! <returns>true if the point is inside the specified shape.</returns>
	//!		<param name="position">the position to check</param>
	//!		<param name="shapeName">the name of the shape to test (returned by AI.GetEnclosingGenericShapeOfType)</param>
	//!		<param name="checkHeight">(optional) if the flag is set the height of the shape is tested too.
	//!			The test will check space between the shape.aabb.min.z and shape.aabb.min.z+shape.height.</param>
	int DistanceToGenericShape(IFunctionHandler* pH);

	//! <code>AI.CreateTempGenericShapeBox(center, radius, height, type)</code>
	//! <description>Creates a temporary box shaped generic shape (will be destroyed upon AIsystem reset).</description>
	//! <returns>the name of the shape.</returns>
	//!		<param name="center">the center of the box</param>
	//!		<param name="radius">the extend of the box in x and y directions.</param>
	//!		<param name="height">the height of the box.</param>
	//!		<param name="type">the AIanchor type of the shape.</param>
	int CreateTempGenericShapeBox(IFunctionHandler* pH);

	//! <code>AI.GetObjectRadius(entityId)</code>
	//! <returns>the radius of specified AI object.</returns>
	//!		<param name="entityId">AI's entity id</param>
	int GetObjectRadius(IFunctionHandler* pH);

	//! <code>AI.GetProbableTargetPosition(entityId)</code>
	//! <returns>the probable target position of the AI.</returns>
	//!		<param name="entityId">AI's entity id</param>
	int GetProbableTargetPosition(IFunctionHandler* pH);

	int NotifyReinfDone(IFunctionHandler* pH);

	int BehaviorEvent(IFunctionHandler* pH);

	int NotifySurpriseEntityAction(IFunctionHandler* pH);

	int InvalidateHidespot(IFunctionHandler* pH);
	int EvalHidespot(IFunctionHandler* pH);

	//! <code>AI.EvalPeek(entityId [, bGetOptimalSide])</code>
	//! <description>Evaluates if an AI object can peek from his current position.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="bGetOptimalSide (optional)">If TRUE, and AI object can peek from both sides, will return the
	//!			side that best fits where the attention target currently is</param>
	//! <returns>
	//!		-1 - don't need to peek
	//!		0 - cannot peek
	//!		1 - can peek from left
	//!		2 - can peek from right
	//!		3 - can peek from left & right
	//! </returns>
	int EvalPeek(IFunctionHandler* pH);

	//! <code>AI.AddPersonallyHostile(entityID, hostileID)</code>
	int AddPersonallyHostile(IFunctionHandler* pH, ScriptHandle entityID, ScriptHandle hostileID);

	//! <code>AI.RemovePersonallyHostile(entityID, hostileID)</code>
	int RemovePersonallyHostile(IFunctionHandler* pH, ScriptHandle entityID, ScriptHandle hostileID);

	//! <code>AI.ResetPersonallyHostiles(entityID, hostileID)</code>
	int ResetPersonallyHostiles(IFunctionHandler* pH, ScriptHandle entityID, ScriptHandle hostileID);

	//! <code>AI.IsPersonallyHostile(entityID, hostileID)</code>
	int IsPersonallyHostile(IFunctionHandler* pH, ScriptHandle entityID, ScriptHandle hostileID);

	//! <code>AI.GetDirectAttackPos(entityId, searchRange, minAttackRange)</code>
	//! <description>Useful for choosing attack position for RPGs and such.</description>
	//! <returns>a cover point which can be used to directly attack the attention target, nil if no attack point is available.</returns>
	//! <remarks>Calling the function is quite heavy since it does raycasting.</remarks>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="AIAnchorType">Anchor type; see Scripts/AIAnchor.lua for a complete list of Anchor types available</param>
	//!		<param name="maxDist">search range</param>
	int GetDirectAnchorPos(IFunctionHandler* pH);

	int GetGroupAlertness(IFunctionHandler* pH);

	int GetAlertness(IFunctionHandler* pH);

	//! <returns>the estimated surrounding navigable space in meters.</returns>
	int GetEnclosingSpace(IFunctionHandler* pH);

	int Event(IFunctionHandler* pH);

	//! <code>AI.GetPotentialTargetCountFromFaction( entityID, name )</code>
	//! <description>Retrieves the number of potential targets for an entity from a specified factions.</description>
	//!		<param name="entityId">id handle of entity to match against.</param>
	//!		<param name="name">name of specified faction.</param>
	int GetPotentialTargetCountFromFaction(IFunctionHandler* pH, ScriptHandle entityID, const char* factionName);

	//! <code>AI.GetPotentialTargetCount( entityID )</code>
	//! <description>Retrieves the number of potential targets for an entity from a specified factions.</description>
	//!		<param name="entityId">id handle of entity to match against.</param>
	int GetPotentialTargetCount(IFunctionHandler* pH, ScriptHandle entityID);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Formation related functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.CreateFormation( name )</code>
	//! <description>Creates a formation descriptor and adds a fixed node at 0,0,0 (owner's node).</description>
	//!		<param name="name">name of the new formation descriptor.</param>
	int CreateFormation(IFunctionHandler* pH);

	//! <code>AI.AddFormationPointFixed(name, sightangle, x, y, z [,unit_class])</code>
	//! <description>Adds a node with a fixed offset to a formation descriptor.</description>
	//!		<param name="name">name of the formation descriptor</param>
	//!		<param name="sightangle">angle of sight of the node (-180,180; 0 = the guy looks forward)</param>
	//!		<param name="x, y, z">offset from formation owner</param>
	//!		<param name="unit_class">class of soldier (see eSoldierClass definition in IAgent.h)</param>
	int AddFormationPointFixed(IFunctionHandler* pH);

	//! <code>AI.AddFormationPoint(name, sightangle, distance, offset, [unit_class [,distanceAlt, offsetAlt]] )</code>
	//! <description>Adds a follow-type node to a formation descriptor.</description>
	//!		<param name="name">name of the formation descriptor</param>
	//!		<param name="sightangle">angle of sight of the node (-180,180; 0 = the guy looks forward)</param>
	//!		<param name="distance">distance from the formation's owner</param>
	//!		<param name="offset">X offset along the following line (negative = left, positive = right)</param>
	//!		<param name="unit_class">class of soldier (see eSoldierClass definition in IAgent.h)</param>
	//!		<param name="distanceAlt (optional)- alternative distance from the formation owner (if 0, distanceAlt and offsetAlt will be set respectively to distance and offset)</param>
	//!		<param name="offsetAlt (optional)">alternative X offset</param>
	int AddFormationPoint(IFunctionHandler* pH);

	//! <code>AI.AddFormationPoint(name, position )</code>
	//! <description>Adds a follow-type node to a formation descriptor.</description>
	//!		<param name="name">name of the formation descriptor</param>
	//!		<param name="position">point index in the formation (1..N)</param>
	//! <returns>class of specified formation point (-1 if not found)</returns>
	int GetFormationPointClass(IFunctionHandler* pH);

	//! <code>AI.GetFormationPointPosition(entityId, pos)</code>
	//! <description>Gets the AI's formation point position.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="pos">return value- position of entity AI's current formation point if there is</param>
	//! <returns>true if the formation point has been found</returns>
	int GetFormationPointPosition(IFunctionHandler* pH);

	//! <code>AI.BeginTrackPattern( patternName, flags, validationRadius, [stateTresholdMin],</code>
	//		[stateTresholdMax], [globalDeformTreshold], [localDeformTreshold], [exposureMod], [randomRotAng] )
	//! <example>AI.BeginTrackPattern( "mypattern", AITRACKPAT_VALIDATE_SWEPTSPHERE, 1.0 )</example>
	//! <description>
	//!		Begins definition of a new track pattern descriptor. The pattern is created bu calling
	//!		the AI.AddPatternPoint() and AI.AddPatternBranch() functions, and finalised by calling
	//!		the AI.EndTrackPattern().
	//! </description>
	//!		<param name="patternName">name of the new track pattern descriptor</param>
	//!		<param name="flags">The functionality flags of the track pattern.
	//!			Validation:
	//!				The validation method describes how the pattern is validated to fit the physical world.
	//!				Should be one of:
	//!				AITRACKPAT_VALIDATE_NONE - no validation at all.
	//!				AITRACKPAT_VALIDATE_SWEPTSPHERE - validate using swept sphere tests, the spehre radius is validation
	//!					radius plus the entity pass radius.
	//!				AITRACKPAT_VALIDATE_RAYCAST - validate using raycasting, the hit position is pulled back by the amount
	//!					of validation radius plus the entity pass radius.
	//!			Aligning:
	//!				When the pattern is selected to be used the alignment of the patter ncan be changed.
	//!				The alignment can be combination of the following. The descriptions are in order they are evaluated.
	//!				AITRACKPAT_ALIGN_TO_TARGET - Align the pattern so that the y-axis will
	//!					point towards the target each time it is set. If the agent does not have valid attention target
	//!					at the time the pattern is set the pattern will be aligned to the world.
	//!				AITRACKPAT_ALIGN_RANDOM - Align the pattern randonly each time it is set.
	//!					The rotation ranges are set using SetRandomRotation().
	//!		</param>
	//!		<param name="validationRadius">the validation radius is added to the entity pass radius when validating
	//!				the pattern along the offsets.</param>
	//!		<param name="stateTresholdMin (optional)">If the state of the pattern is 'enclosed' (high deformation) and
	//!				the global deformation < stateTresholdMin, the state becomes exposed. Default 0.35.</param>
	//!		<param name="stateTresholdMax (optional)">If the state of the pattern is 'exposed' (low deformation) and
	//!				the global deformation > stateTresholdMax, the state becomes enclosed. Default 0.4.</param>
	//!		<param name="globalDeformTreshold (optional)">the deformation of the whole pattern is tracked in range [0..1].
	//!				This treshold value can be used to clamp the bottom range, so that values in range [trhd..1] becomes [0..1], default 0.0.</param>
	//!		<param name="localDeformTreshold (optional)">the deformation of the each node is tracked in range [0..1].
	//!				This treshold value can be used to clamp the bottom range, so that values in range [trhd..1] becomes [0..1], default 0.0.</param>
	//!		<param name="exposureMod (optional)">the exposure modifier allows to take the node exposure (how much it is seen by
	//!				the tracked target) into account when branching. The modifier should be in range [-1..1], -1 means to
	//!				favor unseen nodes, and 1 means to favor seen, exposed node. Default 0 (no effect).</param>
	//!		<param name="randomRotAng (optional)">each time the pattern is set, it can be optionally rotated randomly.
	//!				This parameter allows to define angles (in degrees) around each axis. The rotation is performed in XYZ order.</param>
	int BeginTrackPattern(IFunctionHandler* pH);

	//! <code>AI.AddPatternNode( nodeName, offsetx, offsety, offsetz, flags, [parent], [signalValue] )</code>
	//! <example>AI.AddPatternNode( "point1", 1.0, 0, 0, AITRACKPAT_NODE_START+AITRACKPAT_NODE_SIGNAL, "root" )</example>
	//! <description>
	//!		Adds point to the track pattern. When validating the points test is made from the start position to the end position.
	//!		Start position is either the pattern origin or in case the parent is provided, the parent position. The end position
	//!		is either relative offset from the start position or offset from the pattern origin, this is chosen based on the node flag.
	//!		The offset is clamped to the physical world based on the test method.
	//!		The points will be evaluated in the same oder they are added to the descriptor, and hte system does not try to correct the
	//!		evaluation order. In case hierarchies are used (parent name is defined) it is up to the pattern creator to make sure the
	//!		nodes are created in such order that the parent is added before it is referenced.
	//! </description>
	//!		<param name="nodeName">name of the new point, the point names are local to the pattern that is currently being specified.</param>
	//!		<param name="offsetx, offsety, offsetz">The offset from the start position or from the pattern center, see AITRACKPAT_NODE_ABSOLUTE.</param>
	//!				If zero offset is used, the node will become an alias, that is it will not be validated and the parent position and deformation value is used directly.</param>
	//!		<param name="flags">Defines the node evaluation flags, the flags are as follows and can be combined:
	//!				AITRACKPAT_NODE_START - If this flag is set, this node can be used as the first node in the pattern. There can be multiple start nodes. In that case the closest one is chosen.
	//!				AITRACKPAT_NODE_ABSOLUTE - If this flag is set, the offset is interpret as an offset from the pattern center, otherwise the offset is offset from the start position.
	//!				AITRACKPAT_NODE_SIGNAL - If this flag is set, a signal "OnReachedTrackPatternNode" will be send when the node is reached.
	//!				AITRACKPAT_NODE_STOP - If this flag is set, the advancing will be stopped, it can be continue by calling entity:ChangeAIParameter( AIPARAM_TRACKPATTERN_ADVANCE, 1 ).
	//!				AITRACKPAT_NODE_DIRBRANCH - The default direction at each pattern node is direction from the node position to the center of the pattern
	//!											If this flag is set, the direction will be average direction to the branch nodes.</param>
	//!		<param name="parent (optional)">If this parameter is set, the start position is considered to be the parent node position instead of the pattern center.</param>
	//!		<param name="signalValue (optional)">If the signal flag is set, this value will be passed as signal parameter, it is accessible from the signal handler in data.iValue.</param>
	int AddPatternNode(IFunctionHandler* pH);

	//! <code>AI.AddPatternBranch( nodeName, method, branchNode1, branchNode2, ..., branchNodeN  )</code>
	//!	<example>AI.AddPatternBranch( "point1", AITRACKPAT_CHOOSE_ALWAYS, "point2" )</example>
	//! <description>
	//!		Creates a branch pattern at the specified node. When the entity has approached the specified node (nodeName),
	//!		and it is time to choose a new point, the rules defined by this function will be used to select the new point.
	//!		This function allows to associate multiple target points and an evaluation rule.
	//! </description>
	//!		<param name="nodeName">name of the node to add the branches.</param>
	//!		<param name="method">The method to choose the next node when the node is reached. Should be one of the following:
	//!				AITRACKPAT_CHOOSE_ALWAYS - Chooses on node from the list in linear sequence.
	//!				AITRACKPAT_CHOOSE_LESS_DEFORMED - Chooses the least deformed point in the list. Each node is associated with a deformation value
	//!					(percentage) which describes how much it was required to move in order to stay within the physical world. These deformation values
	//!					are summed down to the parent nodes so that deformation at the end of the hierarchy will be caught down the hierarchy.
	//!				AITRACKPAT_CHOOSE_RANDOM - Chooses one point in the list randomly.</param>
	int AddPatternBranch(IFunctionHandler* pH);

	//! <code>AI.EndTrackPattern()</code>
	//! <description>
	//!		Finalizes the track pattern definition. This function should always called to finalize the pattern.
	//!		Failing to do so will cause erratic behavior.
	//! </description>
	int EndTrackPattern(IFunctionHandler* pH);

	//! <code>AI.ChangeFormation(entityId, name [,scale] )</code>
	//! <description>Changes the formation descriptor for the current formation of given entity's group (if there is a formation).</description>
	//!		<param name="entityId">entity id of which group the formation is changed</param>
	//!		<param name="name">name of the formation descriptor</param>
	//!		<param name="scale (optional)">scale factor (1 = default)</param>
	//! <returns>true if the formation change was successful</returns>
	int ChangeFormation(IFunctionHandler* pH);

	//! <code>AI.ScaleFormation(entityId, scale)</code>
	//! <description>changes the scale factor of the given entity's formation (if there is).</description>
	//!		<param name="entityId">entity id of which group the formation is scaled</param>
	//!		<param name="scale">scale factor</param>
	//! <returns>true if the formation scaling was successful</returns>
	int ScaleFormation(IFunctionHandler* pH);

	//! <code>AI.SetFormationUpdate(entityId, update)</code>
	//! <description>
	//!		Changes the update flag of the given entity's formation (if there is) -
	//!		the formation is no more updated if the flag is false.
	//! </description>
	//!		<param name="entityId">entity id of which group the formation is scaled</param>
	//!		<param name="update">update flag (true/false)</param>
	//! <returns>true if the formation update setting was successful</returns>
	int SetFormationUpdate(IFunctionHandler* pH);

	//! <code>AI.SetFormationUpdateSight(entityId, range, minTime, maxTime )</code>
	//! <description>Sets a random angle rotation for the given entity's formation sight directions.</description>
	//!		<param name="entityId">entity id owner of the formation </param>
	//!		<param name="range">angle (0,360) of rotation around the default sight direction</param>
	//!		<param name="minTime (optional)">minimum timespan for changing the direction (default = 2)</param>
	//!		<param name="maxTime (optional)">minimum timespan for changing the direction (default = minTime)</param>
	int SetFormationUpdateSight(IFunctionHandler* pH);

	////////////////////////////////////////////////////////////////////
	/// Navigation/pathfind related functions
	////////////////////////////////////////////////////////////////////

	//! <code>AI.GetNavigationType(entityId)</code>
	//! <description>returns the navigation type value at the specified entity's position, given the entity navigation properties.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>
	//!		navigation type at the entity's position (NAV_TRIANGULAR,NAV_WAYPOINT_HUMAN,NAV_ROAD,NAV_VOLUME,NAV_WAYPOINT_3DSURFACE,
	//!		NAV_FLIGHT,NAV_SMARTOBJECT) see IAISystem::ENavigationType definition
	//! </returns>
	int GetNavigationType(IFunctionHandler* pH);

	//! <code>AI.GetDistanceAlongPath(entityId1, entityid2)</code>
	//! <description>returns the distance between entity1 and entity2, along entity1's path.</description>
	//!		<param name="entityId1">AI's entity1 id</param>
	//!		<param name="entityId2">AI's entity2 id</param>
	//! <returns>distance along entity1 path; distance value would be negative if the entity2 is ahead along the path</returns>
	int GetDistanceAlongPath(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// tank/warrior related functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.IsPointInFlightRegion(start, end)</code>
	//! <description>check if the line is in a Forbidden Region.</description>
	//!		<param name="start">a vector in format {x,y,z}</param>
	//!		<param name="end">a vector in format {x,y,z}</param>
	//! <returns>intersected position or end (if there is no intersection)</returns>
	int IntersectsForbidden(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Helicopter/VTOL related functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//Helicopter combat, should be merged with GetAlienApproachParams

	int GetHeliAdvancePoint(IFunctionHandler* pH);

	int GetFlyingVehicleFlockingPos(IFunctionHandler* pH);

	int CheckVehicleColision(IFunctionHandler* pH);

	int SetForcedNavigation(IFunctionHandler* pH);

	int SetAdjustPath(IFunctionHandler* pH);

	//! <code>AI.IsPointInFlightRegion(point)</code>
	//! <description>Check if the point is in the Flight Region.</description>
	//!		<param name="point: a vector in format {x,y,z}
	//! <returns>true if the point is in the Flight Region</returns>
	int IsPointInFlightRegion(IFunctionHandler* pH);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Boat related functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! <code>AI.IsPointInFlightRegion(point)</code>
	//! <description>Check if the point is in the Flight Region.</description>
	//!		<param name="point: a vector in format {x,y,z}
	//! <returns>water level - ground level, values greater than 0 mean there is water.</returns>
	int IsPointInWaterRegion(IFunctionHandler* pH);

	////////////////////////////////////////////////////////////////////
	/// Miscellaneous
	////////////////////////////////////////////////////////////////////

	//! <code>AI.SetFireMode(entityId, mode)</code>
	//! <description>Immediately sets firemode.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="firemode">new firemode</param>
	int SetFireMode(IFunctionHandler* pH);

	//! <code>AI.SetMemoryFireType(entityId, type)</code>
	//! <description>Sets how the puppet handles firing at its memory target.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="type">Memory fire type (see EMemoryFireType)</param>
	int SetMemoryFireType(IFunctionHandler* pH);

	//! <code>AI.GetMemoryFireType(entityId)</code>
	//! <returns>How the puppet handles firing at its memory target.</returns>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>Memory fire type (see EMemoryFireType)</returns>
	int GetMemoryFireType(IFunctionHandler* pH);

	//! <code>AI.ThrowGrenade(entityId, grenadeType, regTargetType)</code>
	//! <description>Throws a specified grenade at target type without interrupting fire mode.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="grenadeType">Requested grenade type (see ERequestedGrenadeType)</param>
	//!		<param name="regTargetType">Who to throw at (see AI_REG_*)</param>
	int ThrowGrenade(IFunctionHandler* pH);

	//! <code>AI.EnableCoverFire(entityId, enable)</code>
	//! <description>Enables/disables fire when the FIREMODE_COVER is selected.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="enable">boolean</param>
	int EnableCoverFire(IFunctionHandler* pH);

	//! <code>AI.EnableFire(entityId, enable)</code>
	//! <description>Enables/disables fire.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="enable">boolean</param>
	int EnableFire(IFunctionHandler* pH);

	//! <code>AI.IsFireEnabled(entityId)</code>
	//! <description>Checks if AI is allowed to fire or not.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>True if AI is enabled to fire</returns>
	int IsFireEnabled(IFunctionHandler* pH);

	//! <code>AI.CanFireInStance(entityId, stance)</code>
	//! <returns>true if AI can fire at his target in the given stance at his current position</returns>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="stance">Stance Id (STANCE_*)</param>
	int CanFireInStance(IFunctionHandler* pH);

	//! <code>AI.SetUseSecondaryVehicleWeapon(entityId, bUseSecondary)</code>
	//! <description>Sets if the AI should use the secondary weapon when firing from a vehicle gunner seat if possible.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="bUseSecondary">TRUE to use the secondary weapon</param>
	int SetUseSecondaryVehicleWeapon(IFunctionHandler* pH);

	//! <description>Assign AgentPathfindingProperties to the given path type.</description>
	int AssignPFPropertiesToPathType(IFunctionHandler* pH);

	int AssignPathTypeToSOUser(IFunctionHandler* pH);

	//! <description>Set agent's pathfinder properties, (normal, road, cover, ....).</description>
	int SetPFProperties(IFunctionHandler* pH);

	int SetRefPointToGrenadeAvoidTarget(IFunctionHandler* pH);

	//! <code>AI.IsAgentInTargetFOV(entityId, fov)</code>
	//! <description>Checks if the entity is in the FOV of the attention target.</description>
	//!		<param name="entityId">AI's entity id.</param>
	//!		<param name="fov">FOV of the enemy in degrees.</param>
	//! <returns>true if in the FOV of the attention target else false.</returns>
	int IsAgentInTargetFOV(IFunctionHandler* pH);

	//! <code>AI.AgentLookAtPos(entityId, pos)</code>
	//! <description>Makes the entityId look at certain position.</description>
	//!		<param name="entityId">AI's entity id.</param>
	//!		<param name="fov">vec3 to look at</param>
	int AgentLookAtPos(IFunctionHandler* pH);

	//! <code>AI.ResetAgentLookAtPos(entityId)</code>
	//! <description>Makes the entityId resets a previous call to AgentLookAtPos().</description>
	//!		<param name="entityId">AI's entity id.</param>
	int ResetAgentLookAtPos(IFunctionHandler* pH);

	//! <code>AI.IsAgentInAgentFOV(entityId, entityId2)</code>
	//! <description>Check if the entity2 is within the entity FOV.</description>
	//!		<param name="entityId">AI's entity who's FOV is checked</param>
	//!		<param name="entityId2">that's entity is looking for ;)</param>
	//! <returns>
	//!		1st value - TRUE if the agent is within the entity FOV
	//!		2nd value - TRUE if the agent is within the entity's primary FOV or FALSE if within secondary FOV
	//! </returns>
	int IsAgentInAgentFOV(IFunctionHandler* pH);

	//! <code>AI.CreateGroupFormation(entityId, leaderId)</code>
	//! <description>Creates a group formation with leader (or updates leader).</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="leaderId">New leader</param>
	int CreateGroupFormation(IFunctionHandler* pH);

	//! <code>AI.SetFormationPosition(entityId, v2RelativePosition )</code>
	//! <description>Sets the Relative position inside the formation.</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="v2RelativePosition">Table with format {x,y} storing the new relative position</param>
	int SetFormationPosition(IFunctionHandler* pH);

	//! <code>AI.SetFormationLookingPoint(entityId, v3RelativePosition )</code>
	//! <description>Sets the Relative looking point position inside the formation.</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="v3RelativePosition">Table with format {x,y,z} storing the new relative looking point</param>
	int SetFormationLookingPoint(IFunctionHandler* pH);

	//! <code>AI.SetFormationAngleThreshold(entityId, fAngleThreshold )</code>
	//! <description>Sets the Relative position inside the formation.</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="fAngleThreshold">New Leader orientation angle threshold in degrees to recal position</param>
	int SetFormationAngleThreshold(IFunctionHandler* pH);

	//! <code>AI.GetFormationPosition(entityId)</code>
	//! <description>Gets the Relative position inside the formation.</description>
	//!		<param name="entityId">AI's entity</param>
	//! <returns>v3 - Table with format {x,y,z} storing the relative position</returns>
	int GetFormationPosition(IFunctionHandler* pH);

	//! <code>AI.GetFormationLookingPoint(entityId)</code>
	//! <description>Gets the looking point position inside the formation.</description>
	//!		<param name="entityId">AI's entity</param>
	//! <returns>v3 - Table with format {x,y,z} storing the looking point position</returns>
	int GetFormationLookingPoint(IFunctionHandler* pH);

	int AutoDisable(IFunctionHandler* pH);

	//! <description>Creates new combat class.</description>
	int AddCombatClass(IFunctionHandler* pH);

	int Animation(IFunctionHandler* pH);

	//! <code>AI.SetAnimationTag(entityId, tagName)</code>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="tagName">.</param>
	int SetAnimationTag(IFunctionHandler* pH, ScriptHandle entityID, const char* tagName);

	//! <code>AI.ClearAnimationTag(entityId, tagName)</code>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="tagName">.</param>
	int ClearAnimationTag(IFunctionHandler* pH, ScriptHandle entityID, const char* tagName);

	//int GetClosestPointToOBB(IFunctionHandler *pH);

	//! <code>AI.GetStance(entityId)</code>
	//! <description>Get the given entity's stance.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>entity stance (STANCE_*)</returns>
	int GetStance(IFunctionHandler* pH);

	//! <code>AI.SetStance(entityId, stance)</code>
	//! <description>Set the given entity's stance.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="stance">stance value (STANCE_*)</param>
	int SetStance(IFunctionHandler* pH);

	//! <code>AI.SetMovementContext(entityId, context)</code>
	//! <description>Set the given entity's movement context.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="context">context value </param>
	int SetMovementContext(IFunctionHandler* pH, ScriptHandle entityId, int context);

	//! <code>AI.ResetMovementContext(entityId)</code>
	//! <description>Reset the given entity's movement context.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="context">context value</param>
	int ClearMovementContext(IFunctionHandler* pH, ScriptHandle entityId, int context);

	//! <code>AI.SetPostures(entityId, postures)</code>
	//! <description>Set the given entity's postures.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="postures">.</param>
	int SetPostures(IFunctionHandler* pH, ScriptHandle entityId, SmartScriptTable postures);

	//! <code>AI.SetPosturePriority(entityId, postureName, priority)</code>
	//! <description>Set the given entity's posture priority.</description>
	int SetPosturePriority(IFunctionHandler* pH, ScriptHandle entityId, const char* postureName, float priority);

	//! <code>AI.GetPosturePriority(entityId, postureName)</code>
	//! <description>Set the given entity's posture priority.</description>
	int GetPosturePriority(IFunctionHandler* pH, ScriptHandle entityId, const char* postureName);

	//! <code>AI.IsMoving(entityId)</code>
	//! <description>Returns true if the agent desires to move.</description>
	//!		<param name="entityId">AI's entity id</param>
	int IsMoving(IFunctionHandler* pH);

	//! <code>AI.RegisterDamageRegion(entityId, radius)</code>
	//! <description>
	//!		Register a spherical region that causes damage (so should be avoided in pathfinding).
	//!		Owner entity position is used as region center. Can be called multiple times, will just move
	//!		update region position
	//! </description>
	//!		<param name="entityId">owner entity id.</param>
	//!		<param name="radius">If radius <= 0 then the region is disabled</param>
	int RegisterDamageRegion(IFunctionHandler* pH);

	//! <code>AI.GetBiasedDirection(entityId)</code>
	//! <description>Get biased direction of certain point.</description>
	//!		<param name="entityId">AI's entity
	int GetBiasedDirection(IFunctionHandler* pH);

	//! <code>AI.SetAttentiontarget(entityId, targetId)</code>
	//! <description>Set a new attention target.</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="targetId">target's id</param>
	int SetAttentiontarget(IFunctionHandler* pH);

	//! <code>AI.FindStandbySpotInShape(centerPos, targetPos, anchorType)</code>
	int FindStandbySpotInShape(IFunctionHandler* pH);

	//! <code>AI.FindStandbySpotInSphere(centerPos, targetPos, anchorType)</code>
	int FindStandbySpotInSphere(IFunctionHandler* pH);

	//! <code>AI.CanMelee(entityId)</code>
	//! <description>returns 1 if the AI is able to do melee attack.</description>
	//!		<param name="entityId">AI's entity id</param>
	int CanMelee(IFunctionHandler* pH);

	//! <code>AI.CheckMeleeDamage(entityId, targetId, radius, minheight, maxheight, angle)</code>
	//! <description>returns 1 if the AI performing melee is actually hitting target.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="targetId">AI's target entity id</param>
	//!		<param name="radius">max distance in 2d to target</param>
	//!		<param name="minheight">min distance in height</param>
	//!		<param name="maxheight">max distance in height</param>
	//!		<param name="angle">FOV to include target</param>
	//! <returns>(distance,angle) pair between entity and target (degrees) if melee is possible, nil otherwise</returns>
	int CheckMeleeDamage(IFunctionHandler* pH);

	//! <code>AI.GetDirLabelToPoint(entityId, point)</code>
	//! <description>Returns a direction label (front=0, back=1, left=2, right_3, above=4, -1=invalid) to the specified point.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="point">point to evaluate.</param>
	int GetDirLabelToPoint(IFunctionHandler* pH);

	//! <code>AI.DebugReportHitDamage(pVictimEntity, pShooterEntity)</code>
	//!		<param name="pVictimEntity">Victim ID.</param>
	//!		<param name="pShooterEntity">Shooter ID.</param>
	//! <description>Creates a debug report for the hit damage.</description>
	int DebugReportHitDamage(IFunctionHandler* pH);

	//! <code>AI.ProcessBalancedDamage(pShooterEntity, pTargetEntity, damage, damageType)</code>
	//!		<param name="pShooterEntity">Shooter ID.</param>
	//!		<param name="pTargetEntity">Target ID.</param>
	//!		<param name="damage">Hit damage.</param>
	//!		<param name="damageType">Hit damage type.</param>
	//! <description>Processes balanced damage.</description>
	int ProcessBalancedDamage(IFunctionHandler* pH);

	//! <code>AI.SetRefpointToAnchor(entityId, rangeMin, rangeMax, findType, findMethod)</code>
	//!		<param name="entityId">AI's entity ID.</param>
	//!		<param name="rangeMin">Minimum range.</param>
	//!		<param name="rangeMax">Maximum range.</param>
	//!		<param name="findType">Finding type.</param>
	//!		<param name="findMethod">Finding method.</param>
	//! <description>Sets a reference point to an anchor.</description>
	int SetRefpointToAnchor(IFunctionHandler* pH);

	//! <code>AI.SetRefpointToPunchableObject(entityId, range)</code>
	//!		<param name="entityId">AI's entity ID.</param>
	//!		<param name="range">Range for the punchable object.</param>
	//! <description>Sets the reference point to the punchable object.</description>
	int SetRefpointToPunchableObject(IFunctionHandler* pH);

	//! <code>AI.MeleePunchableObject(entityId, objectId, origPos)</code>
	//!		<param name="entityId">AI's entity ID.</param>
	//!		<param name="objectId">Object ID.</param>
	//!		<param name="origPos	">Position of the melee punchable object</param>
	int MeleePunchableObject(IFunctionHandler* pH);

	//! <code>AI.IsPunchableObjectValid(userId, objectId, origPos)</code>
	//!		<param name="userId">User ID.</param>
	//!		<param name="objectId">Object ID.</param>
	//!		<param name="origPos">Object position in the world.</param>
	//! <description>Checks if a punchable object is valid.</description>
	int IsPunchableObjectValid(IFunctionHandler* pH);

	//! <code>AI.PlayReadabilitySound(entityId, soundName)</code>
	//! <description>Plays readability sound on the AI agent. This call does not do any filtering like playing readability using signals.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="soundName">the name of the readability sound signal to play</param>
	//!		<param name="stopPreviousSounds (Optional)">TRUE if any currently playing readability should be stopped in favor of this one</param>
	//!		<param name="responseDelayMin (Optional)">Minimum (or exact, if no maximum) delay for the Response readability to play</param>
	//!		<param name="repsonseDelayMax (Optional)">Maximum delay for the Response readability to play</param>
	int PlayReadabilitySound(IFunctionHandler* pH);

	//! <code>AI.PlayCommunication(entityId, commName, channelName[, targetId] [, targetPos])</code>
	//! <description>Plays communication on the AI agent.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="commName">The name of the communication to play</param>
	//!		<param name="channelName">The name of the channel where the communication will play</param>
	int PlayCommunication(IFunctionHandler* pH, ScriptHandle entityId, const char* commName, const char* channelName,
	                      float contextExpiry);

	//! <code>AI.StopCommunication(playID)</code>
	//! <description>Stops given communication.</description>
	//!		<param name="playID">The id of the communication to stop.</param>
	int StopCommunication(IFunctionHandler* pH, ScriptHandle playID);

	//! <code>AI.EnableWeaponAccessory(entityId, accessory, state)</code>
	//! <description>Enables or disables certain weapon accessory usage.</description>
	//!		<param name="entityId">AI's entity id</param>
	//!		<param name="accessory">enum of the accessory to enable (see EAIWeaponAccessories)</param>
	//!		<param name="state">true/false to enable/disable</param>
	int EnableWeaponAccessory(IFunctionHandler* pH);

	// AIHandler calls this function to replace the OnPlayerSeen signal
	/*	static const char* GetCustomOnSeenSignal( int iCombatClass )
	   {
	    if ( iCombatClass < 0 || iCombatClass >= m_CustomOnSeenSignals.size() )
	      return "OnPlayerSeen";
	    const char* result = m_CustomOnSeenSignals[ iCombatClass ];
	    return *result ? result : "OnPlayerSeen";
	   }*/

	//! <code>AI.RegisterInterestingEntity(entityId, baseInterest, category, aiAction)</code>
	//! <description>Registers the entity with the interest system. Any errors go to the error log.</description>
	//!		<param name="entityId">AI's entity id</param>
	//! <returns>
	//!		true - if a valid update was performed
	//!		nil - if not (Interest system is disabled, parameters not valid, etc)
	//! </returns>
	int RegisterInterestingEntity(IFunctionHandler* pH, ScriptHandle entityId, float radius, float baseInterest, const char* actionName, Vec3 offset, float pause, int shared);

	//! <code>AI.UnregisterInterestingEntity(entityId)</code>
	//! <description>Unregisters the entity with the interest system. Any errors go to the error log.</description>
	//!		<param name="entityId">AI's entity</param>
	int UnregisterInterestingEntity(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>AI.RegisterInterestedActor(entityId, fInterestFilter, fAngleInDegrees)</code>
	//! <description>Registers the interested actor with the interest system. Any errors go to the error log.</description>
	//!		<param name="entityId">AI's entity</param>
	//! <returns>
	//!		true - if a valid update was performed
	//!		nil - if not (Interest system is disabled, parameters not valid, etc)
	//! </returns>
	int RegisterInterestedActor(IFunctionHandler* pH, ScriptHandle entityId, float fInterestFilter, float fAngleInDegrees);

	//! <code>AI.UnregisterInterestedActor(entityId)</code>
	//! <description>Unregisters the entity with the interest system. Any errors go to the error log.</description>
	//!		<param name="entityId">AI's entity id</param>
	int UnregisterInterestedActor(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>AI.IsCoverCompromised(entityId)</code>
	//!		<param name="entityId">AI's entity</param>
	//! <returns>
	//!		true - Cover is not good anymore
	//!		nil - if not
	//! </returns>
	int IsCoverCompromised(IFunctionHandler* pH);

	//! <code>AI.SetCoverCompromised(entityId)</code>
	//!		<param name="entityId">AI's entity</param>
	int SetCoverCompromised(IFunctionHandler* pH);

	//! <code>AI.IsTakingCover(entityId, [distanceThreshold])</code>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="distanceThreshold">(optional) distance over which if the agent is running to cover, he won't be considered as taking cover</param>
	//! <returns>
	//!		true - Agent is either in cover or running to cover
	//!		nil - if not
	//! </returns>
	int IsTakingCover(IFunctionHandler* pH);

	//! <code>AI.IsMovingInCover(entityId)</code>
	//!		<param name="entityId">AI's entity</param>
	//! <returns>
	//!		true - Agent is moving in cover
	//!		nil - if not
	//! </returns>
	int IsMovingInCover(IFunctionHandler* pH);

	//! <code>AI.IsMovingToCover(entityId)</code>
	//!		<param name="entityId">AI's entity</param>
	//! <returns></returns>
	//!		true - Agent is running to cover
	//!		nil - if not
	//! </returns>
	int IsMovingToCover(IFunctionHandler* pH);

	//! <code>AI.IsInCover(entityId)</code>
	//! <returns></returns>
	//!		true - if AI is using cover
	//!		nil - if not
	//! </returns>
	int IsInCover(IFunctionHandler* pH);

	//! <code>AI.SetInCover(entityId, inCover)</code>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="inCover">if the AI should be set to be in cover or not</param>
	int SetInCover(IFunctionHandler* pH);

	//! <code>AI.IsOutOfAmmo(entityId)</code>
	//!		<param name="entityId">AI's entity</param>
	//! <returns>
	//!		true - Entity is out of ammo
	//!		nil - if not
	//! </returns>
	int IsOutOfAmmo(IFunctionHandler* pH);

	//! <code>AI.IsLowOnAmmo(entityId)</code>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="threshold">the ammo percentage threshold</param>
	int IsLowOnAmmo(IFunctionHandler* pH);

	//! <code>AI.ResetAgentState(entityId,stateLabel)</code>
	//! <description>Resets a particular aspect of the agent's state, such as "lean". Intended to keep these hacky concepts together.</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="stateLabel">string describing the state that must be reset to default</param>
	//! <returns>nil</returns>
	int ResetAgentState(IFunctionHandler* pH, ScriptHandle entityId, const char* stateLabel);

	//! <code>AI.RegisterTacticalPointQuery( querySpecTable )</code>
	//! <description>Get a query ID for the given tactical point query.</description>
	//!		<param name="querySpecTable">table specifying the query (a mini-language - see Tactical Point System docs elsewhere)</param>
	//! <returns>
	//!		> 0 - If the query was parsed successfully
	//!		0 - Otherwise
	//! </returns>
	int RegisterTacticalPointQuery(IFunctionHandler* pH);

	//! <code>AI.GetTacticalPoints( entityId, tacPointSpec, point )</code>
	//! <description>Get a point matching a description, related to an entity. Format of a point is: { x,y,z }.</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="tacPointSpec">table specifying the points required</param>
	//!		<param name="point">a table put coordinates of point found</param>
	//! <returns>
	//!		true - If a valid point was found
	//!		false - Otherwise
	//! </returns>
	int GetTacticalPoints(IFunctionHandler* pH);

	//! <code>AI2.DestroyAllTPSQueries()</code>
	//! <description>Destroys all the tactical point system queries.</description>
	int DestroyAllTPSQueries(IFunctionHandler* pH);

	//! <code>AI.GetObjectBlackBoard( entity )</code>
	//! <description>retrieves given object's black board (a Lua table).</description>
	//!		<param name="entityId or entityName">some kind of AI entity's identifier</param>
	//! <returns>
	//!		black board - if there was one
	//!		nil - Otherwise
	//! </returns>
	int GetObjectBlackBoard(IFunctionHandler* pH);

	//! <code>AI.GetBehaviorBlackBoard( entity )</code>
	//! <description>retrieves given AIActor current behaviour's black board (a Lua table).</description>
	//!		<param name="entityId or entityName">some kind of AIActors's identifier</param>
	//! <returns>
	//!		black board - if there was one
	//!		nil - Otherwise
	//! </returns>
	int GetBehaviorBlackBoard(IFunctionHandler* pH);

	//! <code>AI.SetBehaviorVariable( entity, variableName, value )</code>
	//! <description>Sets a behaviour variable for the specified actor.</description>
	int SetBehaviorVariable(IFunctionHandler* pH, ScriptHandle entityId, const char* variableName, bool value);

	//! <code>AI.GetBehaviorVariable( entity, variableName, value )</code>
	//! <description>Returns a behavior variable for the specified actor.</description>
	int GetBehaviorVariable(IFunctionHandler* pH, ScriptHandle entityId, const char* variableName);

	//! <code>AI.ParseTables( firstTable,parseMovementAbility,pH,aiParams,updateAlways )</code>
	//!		<param name="firstTable">Properties table.</param>
	//!		<param name="parseMovementAbility">True to parse movement ability, false otherwise.</param>
	//!		<param name="aiParams">AI parameters.</param>
	//!		<param name="updateAlways">.</param>
	bool ParseTables(int firstTable, bool parseMovementAbility, IFunctionHandler* pH, AIObjectParams& aiParams, bool& updateAlways);

	//! <code>AI.GoTo(entityId, vDestination)</code>
	//! <description>This function is intended to allow AI Actor (the entity) go to the specified destination.</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="vDestination">.</param>
	int GoTo(IFunctionHandler* pH);

	//! <code>AI.SetSpeed(entityId, urgency)</code>
	//! <description>This function allows the user to override the entity's current speed (urgency).</description>
	//!		<param name="entityId">AI's entity</param>
	//!		<param name="urgency">float value specifying the movement urgency (see AgentMovementSpeeds::EAgentMovementUrgency)</param>
	int SetSpeed(IFunctionHandler* pH);

	//! <code>AI.SetEntitySpeedRange( userEntityId, urgency, defaultSpeed, minSpeed, maxSpeed, stance = all)</code>
	//! <description>This function allows the user to override the entity's speed range for the given urgency.</description>
	//!		<param name="usedEntityId">entity id of the user for which its last used smart object is needed</param>
	//!		<param name="urgency">integer value specifying the movement urgency (see AgentMovementSpeeds::EAgentMovementUrgency)</param>
	//!		<param name="defaultSpeed">floating point value specifying the default speed</param>
	//!		<param name="minSpeed">floating point value specifying the min speed</param>
	//!		<param name="maxSpeedfloating point value specifying the max speed</param>
	//!		<param name="stance">optional parameter specifying the stance for which the range is set. default is all (see AgentMovementSpeeds::EAgentMovementStance)</param>
	//! <returns>true if the operation was successful and false otherwise</returns>
	int SetEntitySpeedRange(IFunctionHandler* pH);

	//! <code>AI.SetAlarmed( entityId )</code>
	//! <description>This function sets the entity to be "perception alarmed".</description>
	int SetAlarmed(IFunctionHandler* pH);

	//! <code>AI.LoadBehaviors( folderName, extension, globalEnv)</code>
	int LoadBehaviors(IFunctionHandler* pH, const char* folderName, const char* extension);

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	//! <code>AI.LoadCharacters(folderName, tbl)</code>
	int LoadCharacters(IFunctionHandler* pH, const char* folderName, SmartScriptTable tbl);
#endif

	//! <code>AI.IsLowHealthPauseActive( entityID )</code>
	int IsLowHealthPauseActive(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.GetPreviousBehaviorName( entityID )</code>
	int GetPreviousBehaviorName(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.SetContinuousMotion( entityID, continuousMotion )</code>
	int SetContinuousMotion(IFunctionHandler* pH, ScriptHandle entityID, bool continuousMotion);

	//! <code>AI.GetPeakThreatLevel( entityID )</code>
	int GetPeakThreatLevel(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.GetPeakThreatType( entityID )</code>
	int GetPeakThreatType(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.GetPreviousPeakThreatLevel( entityID )</code>
	int GetPreviousPeakThreatLevel(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.GetPreviousPeakThreatType( entityID )</code>
	int GetPreviousPeakThreatType(IFunctionHandler* pH, ScriptHandle entityID);

	//! <code>AI.CheckForFriendlyAgentsAroundPoint( entityID, point, radius)</code>
	int CheckForFriendlyAgentsAroundPoint(IFunctionHandler* pH, ScriptHandle entityID, Vec3 point, float radius);

	//! <code>AI.EnableUpdateLookTarget(entityID, bEnable)</code>
	int EnableUpdateLookTarget(IFunctionHandler* pH, ScriptHandle entityID, bool bEnable);

	//! <code>AI.UpdateGlobalPerceptionScale(visualScale, audioScale, [filterType], [faction])</code>
	int UpdateGlobalPerceptionScale(IFunctionHandler* pH, float visualScale, float audioScale);

	//! <code>AI.QueueBubbleMessage(entityID, message, flags)</code>
	int QueueBubbleMessage(IFunctionHandler* pH, ScriptHandle entityID, const char* message);

	//! <code>AI.SequenceBehaviorReady(entityId)</code>
	int SequenceBehaviorReady(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>AI.SequenceInterruptibleBehaviorLeft(entityId)</code>
	int SequenceInterruptibleBehaviorLeft(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>AI.SequenceNonInterruptibleBehaviorLeft(entityId)</code>
	int SequenceNonInterruptibleBehaviorLeft(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>AI.SetCollisionAvoidanceRadiusIncrement(entityId, radius)</code>
	int SetCollisionAvoidanceRadiusIncrement(IFunctionHandler* pH, ScriptHandle entityId, float radius);

	//! <code>AI.RequestToStopMovement(entityId)</code>
	int RequestToStopMovement(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>AI.GetDistanceToClosestGroupMember(entityId)</code>
	int GetDistanceToClosestGroupMember(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>AI.IsAimReady(entityIdHandle)</code>
	int IsAimReady(IFunctionHandler* pH, ScriptHandle entityIdHandle);

	//! <code>AI.LockBodyTurn(entityID, bAllowLowerBodyToTurn)</code>
	//!		<param name="entityId">entity id of the agent you want to set the look style to</param>
	//!		<param name="bAllowLowerBodyToTurn">true if you want to allow the turning movement of the body, false otherwise</param>
	int AllowLowerBodyToTurn(IFunctionHandler* pH, ScriptHandle entityID, bool bAllowLowerBodyToTurn);

	//! <code>AI.GetGroupScopeUserCount(entityID, bAllowLowerBodyToTurn)</code>
	//!		<param name="entityId">entity id of the agent you want to access the group scope for.</param>
	//!		<param name="groupScopeName">the group scope name.</param>
	//! <returns>The amount of actors inside the group scope (>= 0) or nil if an error occurred.</returns>
	int GetGroupScopeUserCount(IFunctionHandler* pH, ScriptHandle entityIdHandle, const char* groupScopeName);

	int StartModularBehaviorTree(IFunctionHandler* pH, ScriptHandle entityIdHandle, const char* treeName);
	int StopModularBehaviorTree(IFunctionHandler* pH, ScriptHandle entityIdHandle);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// XML support for goal pipes
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	int LoadGoalPipes(IFunctionHandler* pH);

private:

	static const char* GetPathTypeName(EAIPathType pathType);

protected:

	//====================================================================
	// Fetch entity ID from script parameter
	//====================================================================
	EntityId GetEntityIdFromParam(IFunctionHandler* pH, int i);

	//====================================================================
	// Fetch entity pointer from script parameter
	//====================================================================
	IEntity* GetEntityFromParam(IFunctionHandler* pH, int i);

	void     AssignPFPropertiesToPathType(const string& sPathType, AgentPathfindingProperties& properties);

	void     SetPFProperties(AgentMovementAbility& moveAbility, EAIPathType nPathType) const;
	void     SetPFProperties(AgentMovementAbility& moveAbility, const string& sPathType) const;

	bool     GetSignalExtraData(IFunctionHandler* pH, int iParam, IAISignalExtraData*& pEData);

	int      RayWorldIntersectionWrapper(Vec3 org, Vec3 dir, int objtypes, unsigned int flags, ray_hit* hits, int nMaxHits,
	                                     IPhysicalEntity** pSkipEnts = 0, int nSkipEnts = 0, void* pForeignData = 0, int iForeignData = 0);

	int CreateQueryFromTacticalSpec(SmartScriptTable specTable);

	typedef std::map<int, int>                              VTypeChart_t;
	typedef std::multimap<int, int, std::greater<int> /**/> VTypeChartSorted_t;

	bool GetGroupSpatialProperties(IAIObject* pRequester, float& offset, Vec3& avgGroupPos, Vec3& targetPos, Vec3& dirToTarget, Vec3& normToTarget);

	int  SetLastOpResult(IFunctionHandler* pH, ScriptHandle entityIdHandle, ScriptHandle targetEntityIdHandle);

	std::vector<Vec3> m_lstPointsInFOVHistory;

	IGoalPipe*        m_pCurrentGoalPipe;
	bool              m_IsGroupOpen;
};

#endif // __ScriptBind_AI_H__
