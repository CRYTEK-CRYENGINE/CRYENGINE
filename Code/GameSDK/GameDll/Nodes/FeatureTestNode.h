// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 30:03:2010   Created by Will Wilson
*************************************************************************/

#pragma once

#include "Testing/FeatureTestMgr.h"   // For IFeatureTest

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryAISystem/ICommunicationManager.h>

//helper struct to store data about the entities attached to the feature test node
struct SEntityData
{
	string m_name;
	string m_class;
};

class CFlowNode_FeatureTest : public CFlowBaseNode<eNCT_Instanced>, public IFeatureTest
{
public:
	CFlowNode_FeatureTest(SActivationInfo* pActInfo);
	virtual ~CFlowNode_FeatureTest();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override;

	virtual void         GetConfiguration(SFlowNodeConfig& config) override;
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;
	virtual void         GetMemoryUsage(ICrySizer* sizer) const override;

	enum EInputPorts
	{
		eInputPorts_Name,
		eInputPorts_Description,
		eInputPorts_Ready,
		eInputPorts_MaxTime,
		eInputPorts_LabelProfileData,
		eInputPorts_Owners,
		eInputPorts_Sequential,
		eInputPorts_Camera,
		eInputPorts_Entity1,
		eInputPorts_Entity2,
		eInputPorts_Entity3,
		eInputPorts_Entity4,
		eInputPorts_Entity5,
		eInputPorts_Entity6,
		eInputPorts_Succeeded,
		eInputPorts_Failed,
		eInputPorts_Count,
	};

	enum EOutputPorts
	{
		eOutputPorts_Start,
		eOutputPorts_SequenceEntity,
		eOutputPorts_Cleanup,
		eInputPorts_Entity1Passed,
		eInputPorts_Entity2Passed,
		eInputPorts_Entity3Passed,
		eInputPorts_Entity4Passed,
		eInputPorts_Entity5Passed,
		eInputPorts_Entity6Passed,
		eOutputPorts_AllPassed,
		eOutputPorts_Count,
	};

	static const int SEQ_ENTITY_COUNT = 6;
	static const int SEQ_ENTITY_FIRST_INPUT_PORT = eInputPorts_Entity1;
	static const int SEQ_ENTITY_FIRST_OUTPUT_PORT = eInputPorts_Entity1Passed;

	// Implements IFeatureTest

	/// Indicates all dependencies are met and this test is ready to run
	virtual bool        ReadyToRun() const override;
	/// Runs the test
	virtual bool        Start() override;
	/// Used to update any time dependent state (such as timeouts)
	virtual void        Update(float deltaTime) override;
	/// Called to cleanup test state once the test is complete
	virtual void        Cleanup() override;
	/// Returns the name of the test
	virtual const char* Name() override;
	///Returns the xml description of the node
	const XmlNodeRef    XmlDescription() override;

	// Serialize call for internal state
	virtual void Serialize(SActivationInfo* pActivationInfo, TSerialize ser) override;

protected:
	/// Attempts to start the next test. Returns true if successful.
	bool StartNextTestRun();

	/// Used to return results and schedule next run in sequence
	void OnTestResult(bool result, const char* reason);

	/// Returns the number of attached entities
	int GetTestEntityCount();

	/// Utility function for getting the entity at the index
	/// outEntity contains the entity if one could be found at the given index, otherwise NULL
	/// bPrepareFromPool is used to specify if the entity at the given index should be prepared from the pool if needed
	/// Returns: True if there was an entityId specified at this index. Note you can still have a NULL outEntity even if true, indicating error.
	bool GetEntityAtIndex(int index, IEntity*& outEntity, bool bPrepareFromPool = false);

	bool GetEntityDataAtIndex(int index, SEntityData& data);

	/// Utility function for returning a test result to the manager and updating any associated entity passed trigger
	void SetResult(bool result, const char* reason);

	/// Utility function for activating/deactivating all associated entities
	void ActivateAllEntities(bool activate);

	/// True when test has not run (only time start time can be exactly 0.0f)
	bool TestHasRun() const { return m_timeRunning > 0.0f; }

private:
	SActivationInfo m_actInfo;

	Vec3            m_cameraOffset;
	Vec3            m_entityStartPos;               // Records the initial position of a sequence entity to aid debugging. Undefined for non-sequential.
	int             m_entitySeqIndex;               // The current (virtual - not port) entity index (or -1)
	uint32          m_failureCount;                 // How many failures during a sequence run (used for AllPassed output)
	float           m_timeRunning;                  // Start time for the current test case (or current sequence if sequential test)
	bool            m_ready;                        // True when Ready input state
	bool            m_running;                      // True when Run has been called.
	bool            m_startNextRun;                 // Used to schedule the next test to run on next update (thus avoiding late false succeed/fail inputs)
	bool            m_labelProfileData;             // Switch to enable labeling profile data for test start and stop points
	string          m_owners;                       // A string containing semi-colon separated list of owners (by domain name) responsible for associated test.
	bool            m_isEnabled;
};

// Forward declaration
class CCodeCheckpoint;

class CFlowNode_WatchCodeCheckpoint : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_WatchCodeCheckpoint(SActivationInfo* pActInfo);
	virtual ~CFlowNode_WatchCodeCheckpoint();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override;

	virtual void         GetConfiguration(SFlowNodeConfig& config) override;
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;
	virtual void         GetMemoryUsage(ICrySizer* sizer) const override;

	enum EInputPorts
	{
		eInputPorts_StartWatching,
		eInputPorts_Name,
		eInputPorts_StopWatching,
	};

	enum EOutputPorts
	{
		eOutputPorts_RecentHits,
		eOutputPorts_Found,
		eOutputPorts_NotFound,
		eOutputPorts_TotalHits,
	};

private:
	void StartWatching(SActivationInfo* pActInfo);
	void StopWatching(SActivationInfo* pActInfo);

	void ResolveCheckpointStatus();
	void RemoveAsWatcher();

private:
	SActivationInfo        m_actInfo;

	size_t                 m_checkPointIdx;  /// The index for the checkpoint (~0 for invalid)
	string                 m_checkpointName; /// The checkpoint name
	const CCodeCheckpoint* m_pCheckPoint;    /// The checkpoint in use (if it has been registered)
	int                    m_prevHitCount;   /// The hit count of the associated CCCPOINT at the last sync

	bool                   m_watchRequested;
};

///This class will listen for communication events within a timeout window
class CFlowNode_ListenForCommunication : public CFlowBaseNode<eNCT_Instanced>, public ICommunicationManager::ICommGlobalListener
{
public:
	CFlowNode_ListenForCommunication(SActivationInfo* pActInfo);
	virtual ~CFlowNode_ListenForCommunication();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override;

	virtual void         GetConfiguration(SFlowNodeConfig& config) override;
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;
	virtual void         GetMemoryUsage(ICrySizer* sizer) const override;

	virtual void         OnCommunicationEvent(ICommunicationManager::ECommunicationEvent event, EntityId actorID, const CommID& playID) override;

	enum EInputPorts
	{
		eInputPorts_StartListening,
		eInputPorts_StopListening,
		eInputPorts_Name,
		eInputPorts_Timeout,
		eInputPorts_Entity
	};

	enum EOutputPorts
	{
		eOutputPorts_CommunicationPlayed,
		eOutputPorts_Success,
		eOutputPorts_Failure,
	};

private:
	void RegisterAsListener(SActivationInfo* pActInfo);
	void RemoveAsListener();
	void Update(float deltaTime);

private:

	SActivationInfo m_actInfo;

	string          m_communicationName; /// The communication name
	CommID          m_commId;            /// The id corresponding to the communication name
	float           m_timeout;           /// The length of time to list for this communication

	EntityId        m_entityId;     /// The ID corresponding to the tracked entity

	float           m_timeListened; /// Time since request to listen for comm signal
	bool            m_isListening;  /// State variable to indicate whether listening for a comm

};

//Simulate player input/actions
class CFlowNode_SimulateInput : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		eInputPorts_Action,
		eInputPorts_Press,
		eInputPorts_Hold,
		eInputPorts_Release,
		eInputPorts_Value
	};

	enum EOutputPorts
	{
		eOutputPort_Pressed,
		eOutputPort_Held,
		eOutputPort_Released,
	};

public:
	CFlowNode_SimulateInput(SActivationInfo* pActInfo);

	virtual void GetConfiguration(SFlowNodeConfig& config) override;
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;
	virtual void GetMemoryUsage(ICrySizer* sizer) const override;

};
