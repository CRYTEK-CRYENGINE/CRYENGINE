// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SCRIPTBIND_ACTION__
#define __SCRIPTBIND_ACTION__

#pragma once

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>
#include <IViewSystem.h>

// FIXME: Cell SDK GCC bug workaround.
#ifndef __IGAMEOBJECTSYSTEM_H__
	#include "IGameObjectSystem.h"
#endif

class CCryAction;

class CScriptBind_Action :
	public CScriptableBase
{
public:
	CScriptBind_Action(CCryAction* pCryAction);
	virtual ~CScriptBind_Action();

	void         Release() { delete this; };

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>Action.LoadXML(definitionFile, dataFile)</code>
	//!		<param name="definitionFile">.</param>
	//!		<param name="dataFile">XML-lua data file name.</param>
	int LoadXML(IFunctionHandler* pH, const char* definitionFile, const char* dataFile);

	//! <code>Action.SaveXML(definitionFile, dataFile, dataTable)</code>
	//!		<param name="definitionFile">.</param>
	//!		<param name="dataFile">XML-lua data file name.</param>
	//!		<param name="dataTable">.</param>
	int SaveXML(IFunctionHandler* pH, const char* definitionFile, const char* dataFile, SmartScriptTable dataTable);

	//! <code>Action.IsServer()</code>
	//! <returns>true if the current script runs on a server.</returns>
	int IsServer(IFunctionHandler* pH);

	//! <code>Action.IsClient()</code>
	//! <returns>true if the current script runs on a client.</returns>
	int IsClient(IFunctionHandler* pH);

	//! <code>Action.IsGameStarted()</code>
	//! <description>true if the game has started.</description>
	int IsGameStarted(IFunctionHandler* pH);

	//! <code>Action.IsRMIServer()</code>
	//! <description>true if the current script is running on an RMI (Remote Method Invocation) server.</description>
	int IsRMIServer(IFunctionHandler* pH);

	//! <code>Action.GetPlayerList()</code>
	//! <description>Checks the current players list.</description>
	int GetPlayerList(IFunctionHandler* pH);

	//! <code>Action.IsGameObjectProbablyVisible( gameObject )</code>
	//!		<param name="gameObject">Object that we want to check.</param>
	//! <returns>true if an object is probably visible.</returns>
	int IsGameObjectProbablyVisible(IFunctionHandler* pH, ScriptHandle gameObject);

	//! <code>Action.ActivateEffect( name )</code>
	//!		<param name="name">Name of the effect.</param>
	//! <description>Activates the specified effect.</description>
	int ActivateEffect(IFunctionHandler* pH, const char* name);

	//! <code>Action.GetWaterInfo( pos )</code>
	//!		<param name="pos">Position to be checked.</param>
	//! <description>Gets information about the water at the position pos.</description>
	int GetWaterInfo(IFunctionHandler* pH, Vec3 pos);

	//! <code>Action.SetViewCamera()</code>
	//! <description>Saves the previous valid view and override it with the current camera settings.</description>
	int SetViewCamera(IFunctionHandler* pH);

	//! <code>Action.ResetToNormalCamera()</code>
	//! <description>Resets the camera to the last valid view stored.</description>
	int ResetToNormalCamera(IFunctionHandler* pH);

	//! <code>Action.GetServer( number )</code>
	//! <description>Gets the server.</description>
	int GetServer(IFunctionHandler* pFH, int number);

	//! <code>Action.RefreshPings()</code>
	//! <description>Refreshes pings for all the servers listed.</description>
	int RefreshPings(IFunctionHandler* pFH);

	//! <code>Action.ConnectToServer( server )</code>
	//!		<param name="server">String that specifies the server to be used for the connection.</param>
	//! <description>Connects to the specified server.</description>
	int ConnectToServer(IFunctionHandler* pFH, char* server);

	//! <code>Action.GetServerTime()</code>
	//! <description>Gets the current time on the server.</description>
	int GetServerTime(IFunctionHandler* pFH);

	//! <code>Action.PauseGame( pause )</code>
	//!		<param name="pause">True to set the game into the pause mode, false to resume the game.</param>
	//! <description>Puts the game into pause mode.</description>
	int PauseGame(IFunctionHandler* pFH, bool pause);

	//! <code>Action.IsImmersivenessEnabled()</code>
	//! <returns>true if immersive multiplayer is enabled.</returns>
	int IsImmersivenessEnabled(IFunctionHandler* pH);

	//! <code>Action.IsChannelSpecial()</code>
	//! <returns>true if the channel is special.</returns>
	int IsChannelSpecial(IFunctionHandler* pH);

	//! <code>Action.ForceGameObjectUpdate( entityId, force )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="force">True to force the update, false otherwise.</param>
	//! <description>Forces the game object to be updated.</description>
	int ForceGameObjectUpdate(IFunctionHandler* pH, ScriptHandle entityId, bool force);

	//! <code>Action.CreateGameObjectForEntity( entityId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//! <description>Creates a game object for the specified entity.</description>
	int CreateGameObjectForEntity(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.BindGameObjectToNetwork( entityId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//! <description>Binds game object to the network.</description>
	int BindGameObjectToNetwork(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.ActivateExtensionForGameObject( entityId, extension, activate )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="extension">Extension name.</param>
	//!		<param name="activate">True to activate the extension, false to deactivate it.</param>
	//! <description>Activates a specified extension for a game object.</description>
	int ActivateExtensionForGameObject(IFunctionHandler* pH, ScriptHandle entityId, const char* extension, bool activate);

	//! <code>Action.SetNetworkParent( entityId, parentId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="parentID">Identifier for the parent network.</param>
	//! <description>Sets the network parent.</description>
	int SetNetworkParent(IFunctionHandler* pH, ScriptHandle entityId, ScriptHandle parentId);

	//! <code>Action.IsChannelOnHold( channelId )</code>
	//!		<param name="channelId">Identifier for the channel.
	//! <description>Checks if the specified channel is on hold.</description>
	int IsChannelOnHold(IFunctionHandler* pH, int channelId);

	//! <code>Action.BanPlayer( entityId, message )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="message">Message for the ban.</param>
	//! <description>Bans a specified player.</description>
	int BanPlayer(IFunctionHandler* pH, ScriptHandle entityId, const char* message);

	//! <code>Action.PersistantSphere( pos, radius, color, name, timeout )</code>
	//!		<param name="pos">Position of the sphere.</param>
	//!		<param name="radius">Radius of the sphere.</param>
	//!		<param name="color">Color of the sphere.</param>
	//!		<param name="name">Name assigned to the sphere.</param>
	//!		<param name="timeout">Timeout for the sphere.</param>
	//! <description>Adds a persistent sphere to the world.</description>
	int PersistantSphere(IFunctionHandler* pH, Vec3 pos, float radius, Vec3 color, const char* name, float timeout);

	//! <code>Action.PersistantLine( start, end, color, name, timeout )	</code>
	//!		<param name="start">Starting position of the line.</param>
	//!		<param name="end">Ending position of the line.</param>
	//!		<param name="color">Color of the line.</param>
	//!		<param name="name">Name assigned to the line.</param>
	//!		<param name="timeout">Timeout for the line.</param>
	//! <description>Adds a persistent line to the world.</description>
	int PersistantLine(IFunctionHandler* pH, Vec3 start, Vec3 end, Vec3 color, const char* name, float timeout);

	//! <code>Action.PersistantArrow( pos, radius, dir, color, name, timeout )</code>
	//!		<param name="pos">Position of the arrow.</param>
	//!		<param name="radius">Radius of the arrow.</param>
	//!		<param name="dir">Direction of the arrow.</param>
	//!		<param name="color">Color of the arrow.</param>
	//!		<param name="name">Name assigned to the arrow.</param>
	//!		<param name="timeout">Timeout for the arrow.</param>
	//! <description>Adds a persistent arrow to the world.</description>
	int PersistantArrow(IFunctionHandler* pH, Vec3 pos, float radius, Vec3 dir, Vec3 color, const char* name, float timeout);

	//! <code>Action.Persistant2DText( text, size, color, name, timeout )</code>
	//!		<param name="text">Text that has to be displayed.</param>
	//!		<param name="size">Size of the 2D text.</param>
	//!		<param name="color">Color of the 2D text.</param>
	//!		<param name="name">Name assigned to the 2D text.</param>
	//!		<param name="timeout">Timeout for the 2D text.</param>
	//! <description>Adds a persistent 2D text.</description>
	int Persistant2DText(IFunctionHandler* pH, const char* text, float size, Vec3 color, const char* name, float timeout);

	//! <code>Action.PersistantEntityTag( entityId, text )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="text">Text for the entity tag.</param>
	//! <description>Adds a persistent entity tag.</description>
	int PersistantEntityTag(IFunctionHandler* pH, ScriptHandle entityId, const char* text);

	//! <code>Action.ClearEntityTags( entityId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//! <description>Clears the tag for the specified entity.</description>
	int ClearEntityTags(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.ClearStaticTag( entityId, staticId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="staticId">Identifier for the static tag.</param>
	//! <description>Clears the specified static tag for the specified entity.</description>
	int ClearStaticTag(IFunctionHandler* pH, ScriptHandle entityId, const char* staticId);

	//! <code>Action.SendGameplayEvent( entityId, event )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="event">Integer for the event.</param>
	//! <description>Sends an event for the gameplay.</description>
	int SendGameplayEvent(IFunctionHandler* pH, ScriptHandle entityId, int event);

	//! <code>Action.CacheItemSound( itemName )</code>
	//!		<param name="itemName">Item name string.</param>
	//! <description>Caches an item sound.</description>
	int CacheItemSound(IFunctionHandler* pH, const char* itemName);

	//! <code>Action.CacheItemGeometry( itemName )</code>
	//!		<param name="itemName">Item name string.</param>
	//! <description>Caches an item geometry.</description>
	int CacheItemGeometry(IFunctionHandler* pH, const char* itemName);

	//! <code>Action.DontSyncPhysics( entityId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//! <description>Doesn't sync physics for the specified entity.</description>
	int DontSyncPhysics(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.EnableSignalTimer( entityId, sText )</code>
	//!		<param name="entityId">Identifier for the entity.
	//!		<param name="sText">Text for the signal.</param>
	//! <description>Enables the signal timer.</description>
	int EnableSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText);

	//! <code>Action.DisableSignalTimer( entityId, sText )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="sText">Text for the signal.</param>
	//! <description>Disables the signal timer.</description>
	int DisableSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText);

	//! <code>Action.SetSignalTimerRate( entityId, sText, fRateMin, fRateMax )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="sText">Text for the signal.</param>
	//!		<param name="fRateMin">Minimum rate for the signal timer.</param>
	//!		<param name="fRateMax">Maximum rate for the signal timer.</param>
	//! <description>Sets the rate for the signal timer.</description>
	int SetSignalTimerRate(IFunctionHandler* pH, ScriptHandle entityId, const char* sText, float fRateMin, float fRateMax);

	//! <code>Action.ResetSignalTimer( entityId, sText )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="sText">Text for the signal.</param>
	//! <description>Resets the rate for the signal timer.</description>
	int ResetSignalTimer(IFunctionHandler* pH, ScriptHandle entityId, const char* sText);

	//! <code>Action.EnableRangeSignaling( entityId, bEnable )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="bEnable">Enable/Disable range signalling.</param>
	//! <description>Enable/Disable range signalling for the specified entity.</description>
	int EnableRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId, bool bEnable);

	//! <code>Action.DestroyRangeSignaling( entityId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	int DestroyRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.ResetRangeSignaling( entityId )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	int ResetRangeSignaling(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.AddRangeSignal( entityId, fRadius, fFlexibleBoundary, sSignal )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="fRadius">Radius of the range area.</param>
	//!		<param name="fFlexibleBoundary">Flexible boundary size.</param>
	//!		<param name="sSignal">String for signal.</param>
	//! <description>Adds a range for the signal.</description>
	int AddRangeSignal(IFunctionHandler* pH, ScriptHandle entityId, float fRadius, float fFlexibleBoundary, const char* sSignal);

	//! <code>Action.AddTargetRangeSignal( entityId, targetId, fRadius, fFlexibleBoundary, sSignal )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="targetId">Identifier for the target.</param>
	//!		<param name="fRadius">Radius of the range area.</param>
	//!		<param name="fFlexibleBoundary">Flexible boundary size.</param>
	//!		<param name="sSignal">String for signal.</param>
	int AddTargetRangeSignal(IFunctionHandler* pH, ScriptHandle entityId, ScriptHandle targetId, float fRadius, float fFlexibleBoundary, const char* sSignal);

	//! <code>Action.AddRangeSignal( entityId, fAngle, fFlexibleBoundary, sSignal )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="fAngle">Angle value.</param>
	//!		<param name="fFlexibleBoundary">Flexible boundary size.</param>
	//!		<param name="sSignal">String for signal.</param>
	//! <description>Adds an angle for the signal.</description>
	int AddAngleSignal(IFunctionHandler* pH, ScriptHandle entityId, float fAngle, float fFlexibleBoundary, const char* sSignal);

	//! <code>Action.RegisterWithAI()</code>
	//! <description>Registers the entity to AI System, creating an AI object associated to it.</description>
	int RegisterWithAI(IFunctionHandler* pH);

	//! <code>Action.HasAI( entityId )</code>
	//! <returns>true if the entity has an AI object associated to it, meaning it has been registered with the AI System</returns>
	int HasAI(IFunctionHandler* pH, ScriptHandle entityId);

	//! <code>Action.GetClassName( classId )</code>
	//! <returns>the matching class name if available for specified classId.</returns>
	int GetClassName(IFunctionHandler* pH, int classId);

	//! <code>Action.SetAimQueryMode( entityId, mode )</code>
	//!		<param name="entityId">Identifier for the entity.</param>
	//!		<param name="mode">QueryAimFromMovementController or OverriddenAndAiming or OverriddenAndNotAiming</param>
	//! <description>
	//!		Set the aim query mode for the ai proxy. Normally the ai proxy
	//!		asks the movement controller if the character is aiming.
	//!		You can override that and set your own 'isAiming' state.
	//! </description>
	int SetAimQueryMode(IFunctionHandler* pH, ScriptHandle entityId, int mode);

	//! <code>Action.PreLoadADB( adbFileName )</code>
	//!		<param name="adbFileName">The path and filename of the animation ADB file which is to be pre-loaded.</param>
	//! <description>Use this function to pre-cache ADB files.</description>
	int PreLoadADB(IFunctionHandler* pH, const char* adbFileName);

private:
	void RegisterGlobals();
	void RegisterMethods();

	CCryAction* m_pCryAction;
	IView*      m_pPreviousView;
};

#endif
