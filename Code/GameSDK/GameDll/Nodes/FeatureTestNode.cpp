// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 30:03:2010   Created by Will Wilson
*************************************************************************/

#include "StdAfx.h"

#include "FeatureTestNode.h"

#include "Testing/FeatureTester.h"  // For ENABLE_FEATURE_TESTER definition(!)
#include <CryAISystem/ICommunicationManager.h>
#include <CrySystem/ICodeCheckpointMgr.h>     // For ICodeCheckpointMgr & CCodeCheckPoint
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryAISystem/IGoalPipe.h>              // For Entity debug info
#include "Player.h"

#include "CodeCheckpointDebugMgr.h"
#include "Utility/CryWatch.h"
#include "IMovementController.h"

#include <CryEntitySystem/IEntitySystem.h>

CFlowNode_FeatureTest::CFlowNode_FeatureTest(SActivationInfo* activationInformation)
	: m_actInfo()
	, m_entityStartPos(Vec3Constants<float>::fVec3_Zero)
	, m_entitySeqIndex(-1)
	, m_failureCount()
	, m_timeRunning(0.0f)
	, m_ready()
	, m_running()
	, m_startNextRun()
	, m_labelProfileData(false)
	, m_isEnabled(false)
{
#if ENABLE_FEATURE_TESTER
	// Register with FeatureTester
	CFeatureTester* pFeatureTester = CFeatureTester::GetInstance();
	if (pFeatureTester)
	{
		pFeatureTester->GetMapFeatureTestMgr().RegisterFeatureTest(this);
	}
#endif
}

CFlowNode_FeatureTest::~CFlowNode_FeatureTest()
{
#if ENABLE_FEATURE_TESTER
	// Unregister from FeatureTester
	CFeatureTester* pFeatureTester = CFeatureTester::GetInstance();
	if (pFeatureTester)
	{
		pFeatureTester->GetMapFeatureTestMgr().UnregisterFeatureTest(this);
	}
#endif
}

IFlowNodePtr CFlowNode_FeatureTest::Clone(SActivationInfo* pActInfo)
{
	return new CFlowNode_FeatureTest(pActInfo);
}

void CFlowNode_FeatureTest::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig<string>("Name",           _HELP("Name of the feature test. Should be short, unique and descriptive. Appended with entity name if test is sequential.")),
		InputPortConfig<string>("Description",    _HELP("Description of the feature test. What feature is being tested and how it's being tested.")),
		InputPortConfig<bool>("Ready",            true,                                                                                                                         _HELP("Boolean to indicate if all dependencies have been met and this test is ready to run.")),
		// TODO: Categories?
		InputPortConfig<float>("MaxTime",         30.0f,                                                                                                                        _HELP("How long (in game time) is the test is allow to run before it fails (seconds).")),
		InputPortConfig<bool>("LabelProfileData", false,                                                                                                                        _HELP("If true then frame profile capture data will be labeled with the name of this test.")),
		InputPortConfig<string>("Owners",         _HELP("A string containing semi-colon separated names of owners (by domain name) responsible for this test.")),

		InputPortConfig<bool>("Sequential",       true,                                                                                                                         _HELP("If true the entities are tested in individually (in sequence). Otherwise entities all are tested together.")),

		InputPortConfig<EntityId>("Camera",       _HELP("Optional entity used to act as the camera for the test. Does not have to be a real camera.")),

		InputPortConfig<EntityId>("Entity1",      _HELP("Test Entity 1, allows entities associated with the test to be automatically hidden and shown.")),
		InputPortConfig<EntityId>("Entity2",      _HELP("Test Entity 2, allows entities associated with the test to be automatically hidden and shown.")),
		InputPortConfig<EntityId>("Entity3",      _HELP("Test Entity 3, allows entities associated with the test to be automatically hidden and shown.")),
		InputPortConfig<EntityId>("Entity4",      _HELP("Test Entity 4, allows entities associated with the test to be automatically hidden and shown.")),
		InputPortConfig<EntityId>("Entity5",      _HELP("Test Entity 5, allows entities associated with the test to be automatically hidden and shown.")),
		InputPortConfig<EntityId>("Entity6",      _HELP("Test Entity 6, allows entities associated with the test to be automatically hidden and shown.")),

		InputPortConfig_Void("Succeeded",         _HELP("Trigger to indicated the feature test has passed. Cleanup will then be triggered.")),
		InputPortConfig_Void("Failed",            _HELP("Trigger to indicated the feature test has filed.")),
		{ 0 }
	};

	static const SOutputPortConfig outputs[] = {
		OutputPortConfig_Void("Start",               _HELP("Trigger to start running the feature test.")),
		OutputPortConfig<EntityId>("SequenceEntity", _HELP("Outputs the entity in use for this part of the test. Only used if Sequential is true and entities are set.")),
		OutputPortConfig_Void("Cleanup",             _HELP("Trigger to cleanup the feature test once it's done.")),

		OutputPortConfig<bool>("Entity1Passed",      _HELP("Outputs true when Entity1 has successfully run the test. Only used if Sequential is true and Entity1 is set.")),
		OutputPortConfig<bool>("Entity2Passed",      _HELP("Outputs true when Entity2 has successfully run the test. Only used if Sequential is true and Entity2 is set.")),
		OutputPortConfig<bool>("Entity3Passed",      _HELP("Outputs true when Entity3 has successfully run the test. Only used if Sequential is true and Entity3 is set.")),
		OutputPortConfig<bool>("Entity4Passed",      _HELP("Outputs true when Entity4 has successfully run the test. Only used if Sequential is true and Entity4 is set.")),
		OutputPortConfig<bool>("Entity5Passed",      _HELP("Outputs true when Entity5 has successfully run the test. Only used if Sequential is true and Entity5 is set.")),
		OutputPortConfig<bool>("Entity6Passed",      _HELP("Outputs true when Entity6 has successfully run the test. Only used if Sequential is true and Entity6 is set.")),

		OutputPortConfig<bool>("AllPassed",          _HELP("Outputs true when the test has been successfully run and all sequential entities passed their tests.")),
		{ 0 }
	};

	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.sDescription = _HELP("Feature Test node that controls automated feature tests");
	config.SetCategory(EFLN_DEBUG);
}

//------------------------------------------------------------------------
void CFlowNode_FeatureTest::Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
{
	ser.BeginGroup("FlowFeatureTest");

	ser.Value("IsReady", m_ready);
	ser.Value("IsRunning", m_running);
	ser.Value("StartNextRun", m_startNextRun);
	ser.Value("FailCount", m_failureCount);
	ser.Value("TimeRunning", m_timeRunning);

	ser.Value("SeqIdx", m_entitySeqIndex);

	ser.EndGroup();
}

void CFlowNode_FeatureTest::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
#if ENABLE_FEATURE_TESTER
	CFeatureTester* pFeatureTester = CFeatureTester::GetInstance();
	if (pFeatureTester)
	{
		CFeatureTestMgr& ftMgr = pFeatureTester->GetMapFeatureTestMgr();

		switch (event)
		{
		case eFE_ConnectInputPort:
			{
				int port = pActInfo->connectPort;
				if (port == eInputPorts_Ready)
				{
					m_isEnabled = true;
				}
			}
			break;

		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;

				// Use this event to reset state in CMapFeatureTestMgr if tests were in progress,
				// allowing it to cope with game/editor transitions cleanly
				if (m_running)
					ftMgr.Reset();

				// Called on level load/reset
				m_ready = false;
				m_running = false;
				m_startNextRun = false;
				m_failureCount = 0;
				m_timeRunning = 0.0f;

				// Deactivate & hide any associated entities - doesn't work... Need to explore other techniques
				// For now just use HiddenInGame for tests
				//DeactivateAllEntities();

				// Set default result outputs to false with reason "Not run"
				for (m_entitySeqIndex = 0; m_entitySeqIndex < SEQ_ENTITY_COUNT; ++m_entitySeqIndex)
					SetResult(false, "Not run");

				m_entitySeqIndex = -1;

				ActivateOutput(pActInfo, eOutputPorts_AllPassed, false);
			}
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eInputPorts_Ready))
				{
					bool readyInput = GetPortBool(pActInfo, eInputPorts_Ready);
					m_labelProfileData = GetPortBool(pActInfo, eInputPorts_LabelProfileData);
					m_owners = GetPortString(pActInfo, eInputPorts_Owners);
					if (readyInput != m_ready)
					{
						m_ready = readyInput;
						CryLogAlways("Test %s now: %s", Name(), m_ready ? "Ready to run" : "Not ready to run");
					}
				}
				else if (IsPortActive(pActInfo, eInputPorts_Succeeded))
				{
					OnTestResult(true, "FG Succeeded Triggered");
				}
				else if (IsPortActive(pActInfo, eInputPorts_Failed))
				{
					OnTestResult(false, "FG Failed Triggered");
				}
			}
			break;
		}
	}
#endif
}

void CFlowNode_FeatureTest::GetMemoryUsage(ICrySizer* sizer) const
{
	sizer->Add(*this);
}

REGISTER_FLOW_NODE("FeatureTest:FeatureTest", CFlowNode_FeatureTest);

/// Indicates all dependencies are met and this test is ready to run
bool CFlowNode_FeatureTest::ReadyToRun() const
{
	return m_ready;
}

/// Actually runs the test
bool CFlowNode_FeatureTest::Start()
{
	bool result = false;

	if (!m_ready)
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Test %s is being forced to run despite not being ready.", Name());

	if (!m_running)
	{
		if (m_labelProfileData)
		{
			string labelName = Name();
			labelName += "_start";
			if (gEnv->pStatoscope)
			{
				gEnv->pStatoscope->AddUserMarker("FeatureTest", labelName);
			}
		}

		result = StartNextTestRun();

		if (result)
		{
			// Check the max time value to ensure a timeout is set
			float maxTime = GetPortFloat(&m_actInfo, eInputPorts_MaxTime);
			if (maxTime <= 0.0f)
				CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "Test %s is been run with max time set to %f - test may never end!", Name(), maxTime);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "Test %s failed to start! Have you already run it?", Name());
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Feature test %s already running (ignoring run request).", Name());
	}

	return result;
}

/// Used to update any time dependent state (such as timeouts)
void CFlowNode_FeatureTest::Update(float deltaTime)
{
	// Has the next test run been scheduled (usually to allow a previous test run time to finish)
	if (m_startNextRun)
	{
		// If testing a sequence, this will run the next test and mark this as running again
		StartNextTestRun();

		// If no more sequential entities?
		if (!m_running)
		{
			CODECHECKPOINT(FeatureTest_OnTestResult_TestFinished);

			ActivateOutput(&m_actInfo, eOutputPorts_AllPassed, m_failureCount == 0);

#if ENABLE_FEATURE_TESTER
			CFeatureTester* pFeatureTester = CFeatureTester::GetInstance();
			if (pFeatureTester)
			{
				// Indicate test case complete
				CFeatureTestMgr& ftMgr = pFeatureTester->GetMapFeatureTestMgr();
				ftMgr.OnTestFinished(this);
			}
#endif
		}
	}

	if (m_running)
	{
		m_timeRunning += deltaTime;

#if ENABLE_FEATURE_TESTER
		CryWatch("$7[FG FeatureTest]$o Running test '%s'", Name());

		const string& description = GetPortString(&m_actInfo, eInputPorts_Description);
		if (!description.empty())
		{
			CryWatch("$7[FG FeatureTest]$o %s", description.c_str());
		}
#endif // ENABLE_FEATURE_TESTER

		const float maxTime = GetPortFloat(&m_actInfo, eInputPorts_MaxTime);

		IEntity* pFollowEntity = NULL;

		// Firstly, attempt to get the camera entity (index: -1)
		bool bHasEntry = GetEntityAtIndex(-1, pFollowEntity);

		if (!bHasEntry && !pFollowEntity)
		{
			// If there's an entity being tested, force the view camera to follow it (if no camera entity defined)
			// This needs to be implemented in a cleaner way and allow other options for non-entity based tests.
			bHasEntry = GetEntityAtIndex(m_entitySeqIndex, pFollowEntity);

			// If no sequence entity defined
			if (!bHasEntry && !pFollowEntity)
			{
				// Look for another suitable (active) entity to follow
				for (int i = 0; i < SEQ_ENTITY_COUNT; ++i)
				{
					GetEntityAtIndex(i, pFollowEntity);
					if (pFollowEntity && pFollowEntity->IsActivatedForUpdates())
					{
						break;
					}
				}
			}
		}

		if (pFollowEntity)
		{
			CCamera viewCamera = gEnv->pSystem->GetViewCamera();

			Vec3 vPos(0, 0, 0);
			Vec3 vDir(0, 0, 0);

			AABB bounds;
			pFollowEntity->GetWorldBounds(bounds);

			if (m_cameraOffset.IsZero())
			{
				m_cameraOffset.Set(0.0f, -(2.0f + bounds.GetRadius()), bounds.GetRadius());
			}

			const Vec3 vTarget = bounds.GetCenter();

			vPos = vTarget + (pFollowEntity->GetForwardDir().GetNormalizedSafe() * m_cameraOffset.y) + (Vec3Constants<float>::fVec3_OneZ * m_cameraOffset.z);
			vDir = (vTarget - vPos).GetNormalizedSafe();

			const float fRoll(0.0f);

			viewCamera.SetMatrix(CCamera::CreateOrientationYPR(CCamera::CreateAnglesYPR(vDir, fRoll)));
			viewCamera.SetPosition(vPos);
			
			gEnv->pSystem->SetViewCamera(viewCamera);
		}

		// If a valid max time has been set
		if (maxTime > 0.0f)
		{
			// If test has exceeded max time
			if (m_timeRunning >= maxTime)
			{
				OnTestResult(false, string().Format("Test failed: Test exceeded maximum time (%f).", maxTime).c_str());
			}
		}
	}
}

/// Called to cleanup test state once the test is complete
void CFlowNode_FeatureTest::Cleanup()
{
	if (m_labelProfileData)
	{
		string labelName = Name();
		labelName += "_end";
		if (gEnv->pStatoscope)
		{
			gEnv->pStatoscope->AddUserMarker("FeatureTest", labelName);

			//Tick once here to make sure end marker is written
			gEnv->pStatoscope->Tick();
		}
	}

	m_running = false;
	ActivateOutput(&m_actInfo, eOutputPorts_Cleanup, true);

	ActivateAllEntities(false);
}

/// Returns the name of the test
const char* CFlowNode_FeatureTest::Name()
{
	if (m_actInfo.pInputPorts)
		return GetPortString(&m_actInfo, eInputPorts_Name).c_str();
	else
		return "Unknown";
}

/// Returns the test description along with data about the entities attached to the node
/// Note: if the node is not sequential the xml does not contain enitities
///  Example output:
///  <test name="ExclusionMeshTest">
///		<entity name="ForbiddenBoundary_Marine" class="Human"/>
///   </test>

const XmlNodeRef CFlowNode_FeatureTest::XmlDescription()
{
	const int entityCount = GetTestEntityCount();

	//useful for debugging
	if (entityCount == 0)
	{
		CryLogAlways("Parsing feature testNode \"%s\". No entities are attached to it, returning...", Name());
	}

	XmlNodeRef testNode = GetISystem()->CreateXmlNode("test");
	stack_string name = this->Name();
	testNode->setAttr("name", name.c_str());
	testNode->setAttr("enabled", this->m_isEnabled ? "true" : "false");
	testNode->setAttr("owners", this->m_owners.c_str());

	CryLogAlways("Parsing feature testNode \"%s\". %d entities are attached to it", Name(), entityCount);

	const bool sequential = GetPortBool(&m_actInfo, eInputPorts_Sequential);

	//if sequential go through the entities attached to this node
	if (entityCount > 0 && sequential)
	{
		for (int i = 0; i < SEQ_ENTITY_COUNT; ++i)
		{
			SEntityData entityData;
			if (GetEntityDataAtIndex(i, entityData))
			{
				XmlNodeRef entityNode = GetISystem()->CreateXmlNode("entity");
				entityNode->setAttr("name", entityData.m_name);
				entityNode->setAttr("class", entityData.m_class);
				testNode->addChild(entityNode);

			}
		}
	}
	return testNode;

}

/// Attempts to start the next test. Returns true if successful.
bool CFlowNode_FeatureTest::StartNextTestRun()
{
	m_startNextRun = false;

	m_cameraOffset.zero();

	// Ensure this is not marked as running
	CRY_ASSERT(!m_running);

	CODECHECKPOINT(FeatureTest_StartNextTestRun_Start);

	const int entityCount = GetTestEntityCount();

	// Is this a sequential test?
	const bool sequential = (entityCount > 0) && GetPortBool(&m_actInfo, eInputPorts_Sequential);

	if (sequential)
	{
		// Ensure sequence index is within a valid range (-1 indicates first run)
		CRY_ASSERT(m_entitySeqIndex >= -1 && m_entitySeqIndex < SEQ_ENTITY_COUNT);

		// If first run
		if (!TestHasRun())
		{
			CryLogAlways("Running sequential test \"%s\" for %d entities...", Name(), entityCount);
		}

		bool bHasEntry = false;
		IEntity* pSeqEntity = NULL;
		for (int i = m_entitySeqIndex + 1; i < SEQ_ENTITY_COUNT; ++i)
		{
			// Prepare the entity from pool if needed
			bHasEntry = GetEntityAtIndex(i, pSeqEntity, true);

			// If there's a valid entity at this index, use it
			if (pSeqEntity)
			{
				m_entitySeqIndex = i;
				break;
			}
			else if (bHasEntry)
			{
				// Fail this test and continue on to the next one
				m_entitySeqIndex = i;
				m_running = true;
				OnTestResult(false, "Test failed: Entity could not be found. Check Entity Pools or the Flowgraph setup.");
				break;
			}
		}

		// Prepare entity ready for test run
		if (pSeqEntity)
		{
			pSeqEntity->Hide(false);
			if (IGameObject* pGameObject = gEnv->pGameFramework->GetIGameObjectSystem()->CreateGameObjectForEntity(pSeqEntity->GetId()))
			{
				pGameObject->ForceUpdate(true);
			}

			m_running = true;
			m_timeRunning = 0;

			m_entityStartPos = pSeqEntity->GetWorldPos();

			CryLogAlways("Starting test: \"%s[%s]\". Max time: %fs. Entity start pos: (%f, %f, %f)",
			             Name(),
			             pSeqEntity->GetEntityTextDescription().c_str(),
			             GetPortFloat(&m_actInfo, eInputPorts_MaxTime),
			             m_entityStartPos.x, m_entityStartPos.y, m_entityStartPos.z);

			// Output entity ID and trigger start
			ActivateOutput(&m_actInfo, eOutputPorts_SequenceEntity, pSeqEntity->GetId());
			ActivateOutput(&m_actInfo, eOutputPorts_Start, true);
		}
		else if (!bHasEntry)
		{
			// Indicate end of sequence
			m_entitySeqIndex = -1;
			CryLogAlways("Finished running sequential test \"%s\" for %d entities.", Name(), entityCount);
		}
	}
	else if (!TestHasRun()) // If test has not yet been run
	{
		// Not using sequence
		m_entitySeqIndex = -1;

		// Activate any associated entities
		ActivateAllEntities(true);

		m_running = true;
		m_timeRunning = 0;

		CryLogAlways("Starting test: \"%s\". Max time: %fs.",
		             Name(),
		             GetPortFloat(&m_actInfo, eInputPorts_MaxTime));

		// Start test
		ActivateOutput(&m_actInfo, eOutputPorts_Start, true);
	}

	CPlayer* pPlayerActor = static_cast<CPlayer*>(gEnv->pGameFramework->GetClientActor());
	if (pPlayerActor)
		pPlayerActor->SetThirdPerson(m_running);

	return m_running;
}

/// Used to return results and schedule next run in sequence
void CFlowNode_FeatureTest::OnTestResult(bool result, const char* reason)
{
	if (m_running)
	{
		CODECHECKPOINT(FeatureTest_OnTestResult_Start);

		if (!result)
			++m_failureCount;

		// Sets the result for any associated entity passed triggers and informs manager of result
		SetResult(result, reason);

		// Ensure all tests are reset and this is no longer marked as running
		Cleanup();

		m_startNextRun = true;
	}
	else
	{
		CryLogAlways("FeatureTest %s received %s signal when test was inactive (ignoring). Timeout too short?", Name(), result ? "success" : "fail");
	}
}

/// Returns the number of attached entities
int CFlowNode_FeatureTest::GetTestEntityCount()
{
	int count = 0;
	for (int i = 0; i < SEQ_ENTITY_COUNT; ++i)
	{
		IEntity* pEntity = NULL;
		if (GetEntityAtIndex(i, pEntity))
			++count;
	}
	return count;
}

/// Utility function for getting the entity at the index
/// outEntity contains the entity if one could be found at the given index, otherwise NULL
/// bPrepareFromPool is used to specify if the entity at the given index should be prepared from the pool if needed
/// NOTE: Index -1 is special case for camera entity.
/// Returns: True if there was an entityId specified at this index. Note you can still have a NULL outEntity even if true, indicating error.
bool CFlowNode_FeatureTest::GetEntityAtIndex(int index, IEntity*& outEntity, bool bPrepareFromPool)
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	CRY_ASSERT(pEntitySystem);

	outEntity = NULL;
	bool bHasEntry = false;

	if (index >= -1 && index < SEQ_ENTITY_COUNT)
	{
		EntityId id = GetPortEntityId(&m_actInfo, int(SEQ_ENTITY_FIRST_INPUT_PORT + index));

		if (id)
		{
			bHasEntry = true;
			outEntity = pEntitySystem->GetEntity(id);
		}
	}

	return bHasEntry;
}

/// This method might appear very similar to the above one
/// instead this one doesn't call ***pEntityPoolManager->PrepareFromPool***
/// which could cause undesired behavior
bool CFlowNode_FeatureTest::GetEntityDataAtIndex(int index, SEntityData& outData)
{

	IEntity* pOutEntity = NULL;

	if (index >= -1 && index < SEQ_ENTITY_COUNT)
	{
		EntityId id = GetPortEntityId(&m_actInfo, int(SEQ_ENTITY_FIRST_INPUT_PORT + index));

		if (id)
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			CRY_ASSERT(pEntitySystem);

			pOutEntity = pEntitySystem->GetEntity(id);

			if (pOutEntity)
			{
				outData.m_name = pOutEntity->GetName();
				outData.m_class = pOutEntity->GetClass()->GetName();
				return true;
			}

			// if it neither exists nor is bookmarked return false
			// should never happen, if it does something has gone wrong
			return false;
		}
	}
	return false;
}

class EntityLogger
{
public:
	EntityLogger(uint32 maxDepth)
		: m_maxDepth(maxDepth)
	{
		m_prefixBuff.reserve(maxDepth + 1);
	}

	bool TooDeep(uint32& depth)
	{
		return ++depth > m_maxDepth;
	}

	const char* Prefix(uint32 depth)
	{
		if (m_prefixBuff.size() != depth)
		{
			m_prefixBuff.resize(0);
			m_prefixBuff.resize(depth, ' ');
		}
		return m_prefixBuff.c_str();
	}

	void Log(const char* name, const char* value, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: \"%s\"", Prefix(depth), name, value);
	}

	void Log(const char* name, const bool& value, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: %s", Prefix(depth), name, value ? "true" : "false");
	}

	void Log(const char* name, const uint32& value, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: %u", Prefix(depth), name, value);
	}

	void Log(const char* name, const uint64& value, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: %" PRIu64, Prefix(depth), name, value);
	}

	void Log(const char* name, const int32& value, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: %d", Prefix(depth), name, value);
	}

	void Log(const char* name, const float& value, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: %f", Prefix(depth), name, value);
	}

	void Log(const char* name, const Vec3& value, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: Vec3(%f, %f, %f)", Prefix(depth), name, value.x, value.y, value.z);
	}

	void Log(const char* name, const AABB& value, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		const Vec3 size(value.GetSize());

		CryLogAlways("%s%s: AABB min(%f, %f, %f) max(%f, %f, %f) size(%f, %f, %f)", Prefix(depth), name,
		             value.min.x, value.min.y, value.min.z,
		             value.max.x, value.max.y, value.max.z,
		             size.x, size.y, size.z);
	}

	void Log(const char* name, IAIObject* pAI, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IAIObject", Prefix(depth), name);

		Log("AIType", (uint32)pAI->GetAIType(), depth);
		Log("GroupID", pAI->GetGroupId(), depth);
		Log("FactionID", pAI->GetFactionID(), depth);
		Log("BodyDir", pAI->GetBodyDir(), depth);
		Log("FirePos", pAI->GetFirePos(), depth);
		Log("Velocity", pAI->GetVelocity(), depth);
		Log("ViewDir", pAI->GetViewDir(), depth);
		//    uint32 visionID = pAI->GetVisionID();
		//    Log("VisionID", visionID, depth);

		if (IAIActorProxy* pProxy = pAI->GetProxy())
			Log("ActorProxy", pProxy, depth);

		if (IPuppet* pPuppet = pAI->CastToIPuppet())
			Log("Puppet", pPuppet, depth);

		if (IPipeUser* pPipeUser = pAI->CastToIPipeUser())
			Log("PipeUser", pPipeUser, depth);
	}

	void Log(const char* name, IPipeUser* pPipeUser, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IPipeUser", Prefix(depth), name);

		if (IGoalPipe* pGoalPipe = pPipeUser->GetCurrentGoalPipe())
			Log("CurrentGoalPipe", pGoalPipe->GetName(), depth);

		Log("FireMode", pPipeUser->GetFireMode(), depth);

		Log("LastUsedSmartObjectId", pPipeUser->GetLastUsedSmartObjectId(), depth);
		Log("ProbableTargetPosition", pPipeUser->GetProbableTargetPosition(), depth);
		Log("RefPoint", pPipeUser->GetRefPoint(), depth);
		Log("IsCoverCompromised", pPipeUser->IsCoverCompromised(), depth);
		Log("IsMovingToCover", pPipeUser->IsMovingToCover(), depth);
		Log("IsPaused", pPipeUser->IsPaused(), depth);

		if (IAIObject* pAttTarget = pPipeUser->GetAttentionTargetAssociation())
			Log("AttentionTarget", pAttTarget, depth);
	}

	void Log(const char* name, IPuppet* pPuppet, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IPuppet", Prefix(depth), name);

		Log("IsAlarmed", pPuppet->IsAlarmed(), depth);
		Log("IsCoverFireEnabled", pPuppet->IsCoverFireEnabled(), depth);
		Log("IsFireEnabled", pPuppet->IsFireEnabled(), depth);
	}

	void Log(const char* name, IAIActorProxy* pProxy, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IAIActorProxy", Prefix(depth), name);

		Log("ActorArmor", pProxy->GetActorArmor(), depth);
		Log("ActorHealth", pProxy->GetActorHealth(), depth);
		Log("ActorIsFallen", pProxy->GetActorIsFallen(), depth);
		Log("ActorMaxArmor", pProxy->GetActorMaxArmor(), depth);
		Log("ActorMaxHealth", pProxy->GetActorMaxHealth(), depth);
		Log("ActorTargetPhase", pProxy->GetActorTargetPhase(), depth);
		Log("AndResetShotBulletCount", pProxy->GetAndResetShotBulletCount(), depth);
		Log("CommunicationConfigName", pProxy->GetCommunicationConfigName(), depth);
		//Log("CommunicationHandler", pProxy->GetCommunicationHandler(), depth);
		Log("CurrentBehaviorName", pProxy->GetCurrentBehaviorName(), depth);
		//Log("CurrentWeaponDescriptor", pProxy->GetCurrentWeaponDescriptor()., depth);
		Log("LinkedVehicleEntityId", pProxy->GetLinkedVehicleEntityId(), depth);
		if (IEntity* pEntity = pProxy->GetGrabbedEntity())
			Log("GrabbedEntity", pEntity, depth);
		Log("PreviousBehaviorName", pProxy->GetPreviousBehaviorName(), depth);
		Log("VoiceLibraryName", pProxy->GetVoiceLibraryName(), depth);
		Log("Dead", pProxy->IsDead(), depth);
		Log("Enabled", pProxy->IsEnabled(), depth);
		Log("ForcedExecute", pProxy->IsForcedExecute(), depth);
		Log("UpdateAlways", pProxy->IsUpdateAlways(), depth);
		Log("IsUnique", pProxy->Unique(), depth);
	}

	void Log(const char* name, IEntityArchetype* pArchetype, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IEntityArchetype", Prefix(depth), name);

		Log("Name", pArchetype->GetName(), depth);
		Log("Class", pArchetype->GetClass(), depth);
	}

	void Log(const char* name, IEntityClass* pClass, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IEntityClass", Prefix(depth), name);

		Log("Name", pClass->GetName(), depth);
		Log("ScriptFile", pClass->GetScriptFile(), depth);
	}

	void Log(const char* name, ICharacterInstance* pCharacter, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: ICharacterInstance", Prefix(depth), name);

		Log("FilePath", pCharacter->GetFilePath(), depth);
		Log("PlaybackScale", pCharacter->GetPlaybackScale(), depth);

		if (ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim())
		{
			IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
			for (int layerIndex = 0; layerIndex < 15; ++layerIndex)
			{
				int numAnims = pSkeletonAnim->GetNumAnimsInFIFO(layerIndex);
				if (numAnims > 0)
				{
					CryLogAlways("%sFound %d animations in layer %d:", Prefix(depth), numAnims, layerIndex);
					for (int i = 0; i < numAnims; ++i)
					{
						const CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(layerIndex, i);

						const char* animName = (pAnimSet) ? pAnimSet->GetNameByAnimID(anim.GetAnimationId()) : "unknown";

						const float normalizedTime = pSkeletonAnim->GetAnimationNormalizedTime(&anim);

						CryLogAlways("%s CAnimation[%d]: ID[%d] \"%s\" active: %d animTime: %f duration: %f loopCount: %d repeatCount: %d", Prefix(depth), i,
						             anim.GetAnimationId(),
						             animName,
						             anim.IsActivated() ? 1 : 0,
						             normalizedTime,
						             anim.GetExpectedTotalDurationSeconds(),
						             anim.GetLoop(),
						             anim.GetRepeat() ? 1 : 0);
					}
				}
			}
		}
	}

	void Log(const char* name, IAnimatedCharacter* pAnimCharacter, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IAnimatedCharacter", Prefix(depth), name);

		Log("AnimLocation", pAnimCharacter->GetAnimLocation().t, depth);
		Log("CurrentStance", pAnimCharacter->GetCurrentStance(), depth);
		Log("PhysicalColliderMode", pAnimCharacter->GetPhysicalColliderMode(), depth);
		Log("IsAimIkAllowed", pAnimCharacter->IsAimIkAllowed(), depth);
		Log("IsLookIkAllowed", pAnimCharacter->IsLookIkAllowed(), depth);

		const SAnimatedCharacterParams& animParams = pAnimCharacter->GetParams();
	}

	void Log(const char* name, IMovementController* pMovementController, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IMovementController", Prefix(depth), name);

		if (const SExactPositioningTarget* pExactPositioningTarget = pMovementController->GetExactPositioningTarget())
		{
			CryLogAlways("%s SExactPositioningTarget", Prefix(depth));

			Log("Activated", pExactPositioningTarget->activated, depth);
			Log("IsNavigationalSO", pExactPositioningTarget->isNavigationalSO, depth);
			Log("Position", pExactPositioningTarget->location.t, depth);
			Log("Direction", pExactPositioningTarget->location.q.GetColumn1(), depth);
			Log("Preparing", pExactPositioningTarget->preparing, depth);
			Log("Action", pExactPositioningTarget->pAction.get() ? pExactPositioningTarget->pAction->GetName() : "NULL", depth);
		}
	}

	void Log(const char* name, IEntity* pEntity, uint32 depth)
	{
		if (TooDeep(depth))
			return;

		CryLogAlways("%s%s: IEntity", Prefix(depth), name);

		Log("Class", pEntity->GetClass()->GetName(), depth);  // TODO: Expand
		Log("ForwardDir", pEntity->GetForwardDir(), depth);
		Log("Guid", pEntity->GetGuid().ToDebugString(), depth);
		Log("EntityID", pEntity->GetId(), depth);
		AABB worldBounds;
		pEntity->GetWorldBounds(worldBounds);
		Log("WorldBounds", worldBounds, depth);
		Log("WorldPos", pEntity->GetWorldPos(), depth);

		//  if (IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics())

		if (IAIObject* pAI = pEntity->GetAI())
			Log("AI", pAI, depth);

		if (IEntityArchetype* pArchetype = pEntity->GetArchetype())
			Log("EntityArchetype", pArchetype, depth);

		if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
			Log("CharacterInstance[0]", pCharacter, depth);

		if (IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()))
		{
			if (IAnimatedCharacter* pAnimCharacter = pActor->GetAnimatedCharacter())
				Log("AnimatedCharacter", pAnimCharacter, depth);

			if (IMovementController* pMovementController = pActor->GetMovementController())
				Log("MovementController", pMovementController, depth);
		}
	}

private:
	string m_prefixBuff;
	uint32 m_maxDepth;
};

/// Utility function for returning a test result to the manager and updating any associated entity passed trigger
void CFlowNode_FeatureTest::SetResult(bool result, const char* reason)
{
#if ENABLE_FEATURE_TESTER
	CFeatureTester* pFeatureTester = CFeatureTester::GetInstance();
	if (pFeatureTester)
	{
		CFeatureTestMgr& ftMgr = pFeatureTester->GetMapFeatureTestMgr();

		// If this is the result for a sequence test
		if (m_entitySeqIndex >= 0 && m_entitySeqIndex < SEQ_ENTITY_COUNT)
		{
			IEntity* pEnt = NULL;
			bool hasEntity = GetEntityAtIndex(m_entitySeqIndex, pEnt);
			if (pEnt)
			{
				ActivateOutput(&m_actInfo, SEQ_ENTITY_FIRST_OUTPUT_PORT + m_entitySeqIndex, result);

				// If test has actually run
				if (m_running)
				{
					const Vec3 finalEntityPos(pEnt->GetWorldPos());
					const Vec3 entityTranslation(finalEntityPos - m_entityStartPos);
					CryLogAlways("Final position for %s: (%f, %f, %f). Translation since start: (%f, %f, %f)",
					             pEnt->GetName(),
					             finalEntityPos.x, finalEntityPos.y, finalEntityPos.z,
					             entityTranslation.x, entityTranslation.y, entityTranslation.z);

					// Log the entity state (more verbose on failure)
					EntityLogger entityLogger(result ? 4 : 6);
					entityLogger.Log("TestEntity", pEnt, 0);

					// Inform manager of results
					string testSeqName(Name());
					testSeqName.append("[");
					testSeqName.append(pEnt->GetEntityTextDescription());
					testSeqName.append("]");

					const string desc(GetPortString(&m_actInfo, eInputPorts_Description));

					//Need to ensure non-owned tests pass null pointer
					const char* owners = m_owners.empty() ? NULL : m_owners.c_str();

					ftMgr.OnTestResults(testSeqName.c_str(), desc.c_str(), (result) ? NULL : reason, m_timeRunning, owners);
				}
				else
				{
					CryLogAlways("ERROR: Received result for a sequential test that hasn't yet started: %s", Name());
				}
			}
		}
		else  // Single test (not sequence)
		{
			// If test has actually run
			if (m_running)
			{
				// Inform manager of results
				string testSeqName(Name());
				const string desc(GetPortString(&m_actInfo, eInputPorts_Description));

				//Need to ensure non-owned tests pass null pointer
				const char* owners = m_owners.empty() ? NULL : m_owners.c_str();

				ftMgr.OnTestResults(testSeqName.c_str(), desc.c_str(), (result) ? NULL : reason, m_timeRunning, owners);
			}
			else
			{
				CryLogAlways("ERROR: Received result for a non-sequential test that hasn't yet started: %s", Name());
			}
		}
	}
#endif
	// NOTE: AllPassed should be triggered by the caller if no more tests are left to run
}

/// Utility function for activating/deactivating all associated entities
void CFlowNode_FeatureTest::ActivateAllEntities(bool activate)
{
	// Activate/deactivate any associated entities
	for (int i = 0; i < SEQ_ENTITY_COUNT; ++i)
	{
		IEntity* pEnt = NULL;
		GetEntityAtIndex(i, pEnt, activate);
		if (pEnt)
		{
			pEnt->Hide(!activate);
			if (IGameObject* pGameObject = gEnv->pGameFramework->GetGameObject(pEnt->GetId()))
			{
				pGameObject->ForceUpdate(activate);
			}
		}
	}
}

// *************************************************************************************

CFlowNode_WatchCodeCheckpoint::CFlowNode_WatchCodeCheckpoint(SActivationInfo* activationInformation)
	: m_actInfo(),
	m_checkPointIdx(~0),
	m_pCheckPoint(),
	m_prevHitCount(0),
	m_watchRequested(false)
{
}

CFlowNode_WatchCodeCheckpoint::~CFlowNode_WatchCodeCheckpoint()
{
	RemoveAsWatcher();
}

IFlowNodePtr CFlowNode_WatchCodeCheckpoint::Clone(SActivationInfo* pActInfo)
{
	return new CFlowNode_WatchCodeCheckpoint(pActInfo);
}

void CFlowNode_WatchCodeCheckpoint::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig_Void("StartWatching", _HELP("Trigger to begin observing code checkpoint.")),
		InputPortConfig<string>("Name",       _HELP("Name of the checkpoint to watch.")),
		InputPortConfig_Void("StopWatching",  _HELP("Trigger to stop watching the code point and output checkpoint status.")),
		{ 0 }
	};

	static const SOutputPortConfig outputs[] = {
		OutputPortConfig<int>("Count",     _HELP("Triggered on StopWatching input. Provides the number of hits since the last StartWatching request.")),
		OutputPortConfig<bool>("Found",    _HELP("Triggered on StopWatching input. Output if this point was hit at least once since the last StartWatching request.")),
		OutputPortConfig<bool>("NotFound", _HELP("Triggered on StopWatching input. Output if this point was not hit at least once since the last StartWatching request.")),
		OutputPortConfig<int>("TotalHits", _HELP("Triggered on StopWatching input. Outputs the total number of hits for the checkpoint since program start.")),
		{ 0 }
	};

	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.sDescription = _HELP("Provides support for detecting hits to code coverage checkpoints.");
	config.SetCategory(EFLN_DEBUG);
}

void CFlowNode_WatchCodeCheckpoint::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{

	case eFE_Initialize:
		{
			m_actInfo = *pActInfo;

			RemoveAsWatcher();

			// Reset state
			m_checkPointIdx = ~0;
			m_pCheckPoint = NULL;
		}
		break;

	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, eInputPorts_StartWatching))
			{
				StartWatching(pActInfo);
			}
			else if (IsPortActive(pActInfo, eInputPorts_StopWatching))
			{
				StopWatching(pActInfo);
			}
		}
		break;
	}
}

void CFlowNode_WatchCodeCheckpoint::GetMemoryUsage(ICrySizer* sizer) const
{
	sizer->Add(*this);
}

REGISTER_FLOW_NODE("FeatureTest:WatchCodeCheckpoint", CFlowNode_WatchCodeCheckpoint);

void CFlowNode_WatchCodeCheckpoint::StartWatching(SActivationInfo* pActInfo)
{
	ResolveCheckpointStatus();

	if (m_pCheckPoint)
		m_prevHitCount = static_cast<int>(m_pCheckPoint->HitCount());

}

void CFlowNode_WatchCodeCheckpoint::StopWatching(SActivationInfo* pActInfo)
{
	ResolveCheckpointStatus();

	//Inform the code checkpoint debug manager that we want to stop observing this point
	if (m_watchRequested)
	{

		if (!m_pCheckPoint)
		{
			ActivateOutput(pActInfo, eOutputPorts_RecentHits, 0);
			ActivateOutput(pActInfo, eOutputPorts_NotFound, true);
			ActivateOutput(pActInfo, eOutputPorts_TotalHits, 0);
		}
		else
		{
			int updatedHitcount = static_cast<int>(m_pCheckPoint->HitCount());
			int difference = updatedHitcount - m_prevHitCount;

			ActivateOutput(pActInfo, eOutputPorts_RecentHits, static_cast<int>(difference));

			if (difference > 0)
				ActivateOutput(pActInfo, eOutputPorts_Found, true);
			else
				ActivateOutput(pActInfo, eOutputPorts_NotFound, true);

			ActivateOutput(pActInfo, eOutputPorts_TotalHits, updatedHitcount);

			m_prevHitCount = static_cast<int>(updatedHitcount);
		}

	}

	RemoveAsWatcher();

}

void CFlowNode_WatchCodeCheckpoint::RemoveAsWatcher()
{
	if (m_watchRequested)
	{
		CCodeCheckpointDebugMgr::RetrieveCodeCheckpointDebugMgr()->UnregisterWatchPoint(m_checkpointName);
		m_watchRequested = false;
	}
}

void CFlowNode_WatchCodeCheckpoint::ResolveCheckpointStatus()
{
	if (!m_pCheckPoint)
	{
		ICodeCheckpointMgr* pCodeCheckpointMgr = gEnv->pCodeCheckpointMgr;
		if (pCodeCheckpointMgr)
		{
			// If handle is invalid (indicates first update)
			if (m_checkPointIdx == ~0)
			{
				string name(GetPortString(&m_actInfo, eInputPorts_Name));

				// And we have a name
				if (!name.empty())
				{
					// Query the code checkpoint manager for a handle to the CCCPOINT
					m_checkPointIdx = pCodeCheckpointMgr->GetCheckpointIndex(name.c_str());
					m_checkpointName = name;
					m_pCheckPoint = pCodeCheckpointMgr->GetCheckpoint(m_checkPointIdx);
				}
			}

			// Ensure we have a valid handle (GetHandle() should always return a valid handle)
			CRY_ASSERT(m_checkPointIdx != ~0);

			// If no checkpoint instance yet resolved
			if (!m_pCheckPoint)
			{
				// Query the manager for checkpoint using the handle
				m_pCheckPoint = pCodeCheckpointMgr->GetCheckpoint(m_checkPointIdx);
			}
		}
	}

	//Inform the code checkpoint debug manager that we want to observe this point
	if (!m_watchRequested)
	{
		CCodeCheckpointDebugMgr::RetrieveCodeCheckpointDebugMgr()->RegisterWatchPoint(m_checkpointName);
		m_watchRequested = true;
	}
}

// *************************************************************************************

CFlowNode_ListenForCommunication::CFlowNode_ListenForCommunication(SActivationInfo* activationInformation)
	: m_actInfo(), m_timeListened(0.0f), m_isListening(false), m_timeout(0.0f)
{
}

CFlowNode_ListenForCommunication::~CFlowNode_ListenForCommunication()
{
}

IFlowNodePtr CFlowNode_ListenForCommunication::Clone(SActivationInfo* pActInfo)
{
	return new CFlowNode_ListenForCommunication(pActInfo);
}

void CFlowNode_ListenForCommunication::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig_Void("StartListening", _HELP("Trigger to begin observing for communication.")),
		InputPortConfig_Void("StopListening",  _HELP("Trigger to stop observing for communication and cleanup.")),
		InputPortConfig<string>("Name",        _HELP("Name of the communication to listen for."),                 _HELP("Name"),                                                    _UICONFIG("enum_global:communications")),
		InputPortConfig<float>("Timeout",      30.0f,                                                             _HELP("Length of time to listen for tracked communication.")),
		InputPortConfig<EntityId>("Entity",    0,                                                                 _HELP("Entity id for which the communication will be played.")),
		{ 0 }
	};

	static const SOutputPortConfig outputs[] = {
		OutputPortConfig<bool>("CommunicationEncountered", _HELP("Triggered and returns true if communication encountered, or returns false if timeout value reached.")),
		OutputPortConfig<bool>("Success",                  _HELP("Triggered and returns true if communication encountered.")),
		OutputPortConfig<bool>("Failure",                  _HELP("Triggered and returns true if timeout limit reached.")),
		{ 0 }
	};

	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.sDescription = _HELP("Provides support for testing communications with feature test system");
	config.SetCategory(EFLN_DEBUG);
}

void CFlowNode_ListenForCommunication::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{

	case eFE_Initialize:
		{
			m_actInfo = *pActInfo;
			m_timeListened = 0.0f;
			m_isListening = false;
			m_timeout = 0.0f;

			RemoveAsListener();

			// Request eFE_Update events
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}
		break;

	case eFE_Update:
		{
			// Check for hits
			float deltaTime = gEnv->pTimer->GetFrameTime();
			Update(deltaTime);
		}
		break;

	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, eInputPorts_StartListening))
				RegisterAsListener(pActInfo);
			if (IsPortActive(pActInfo, eInputPorts_StopListening))
				RemoveAsListener();
		}
		break;
	}
}

void CFlowNode_ListenForCommunication::GetMemoryUsage(ICrySizer* sizer) const
{
	sizer->Add(*this);
}

REGISTER_FLOW_NODE("FeatureTest:ListenForCommunication", CFlowNode_ListenForCommunication);

void CFlowNode_ListenForCommunication::RegisterAsListener(SActivationInfo* pActInfo)
{
	m_communicationName = GetPortString(pActInfo, eInputPorts_Name);
	m_timeout = GetPortFloat(pActInfo, eInputPorts_Timeout);

	ICommunicationManager* pCommunicationManager = gEnv->pAISystem->GetCommunicationManager();

	CRY_ASSERT(pCommunicationManager);

	m_commId = pCommunicationManager->GetCommunicationID(m_communicationName);

	m_timeListened = 0.0f;
	m_isListening = true;

	m_entityId = GetPortEntityId(&m_actInfo, eInputPorts_Entity);

	pCommunicationManager->RegisterListener(this, "CommunicationFeatureTest");

}

void CFlowNode_ListenForCommunication::RemoveAsListener()
{
	ICommunicationManager* pCommunicationManager = gEnv->pAISystem->GetCommunicationManager();

	CRY_ASSERT(pCommunicationManager);

	m_isListening = false;
	pCommunicationManager->UnregisterListener(this);
}

void CFlowNode_ListenForCommunication::OnCommunicationEvent(ICommunicationManager::ECommunicationEvent event, EntityId actorID, const CommID& playID)
{
	if ((m_entityId == 0 || actorID == m_entityId) && playID == m_commId)
	{
		switch (event)
		{
		case ICommunicationManager::CommunicationFinished:

			ActivateOutput(&m_actInfo, eOutputPorts_CommunicationPlayed, true);
			ActivateOutput(&m_actInfo, eOutputPorts_Success, true);
			RemoveAsListener();
			break;
		default:
			break;
		}
	}
}

void CFlowNode_ListenForCommunication::Update(float deltaTime)
{
	if (m_isListening)
	{
		m_timeListened += deltaTime;
		if (deltaTime > m_timeout)
		{
			RemoveAsListener();
			ActivateOutput(&m_actInfo, eOutputPorts_CommunicationPlayed, false);
			ActivateOutput(&m_actInfo, eOutputPorts_Failure, true);

		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CFlowNode_SimulateInput::CFlowNode_SimulateInput(SActivationInfo* pActInfo)
{

}

void CFlowNode_SimulateInput::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_ports[] =
	{
		InputPortConfig<string>("ActionInput", _HELP("Action input to trigger"), _HELP("Action"),                 _UICONFIG("enum_global:input_actions")),
		InputPortConfig_Void("Press",          _HELP("Simulate press")),
		InputPortConfig_Void("Hold",           _HELP("Simulate hold event")),
		InputPortConfig_Void("Release",        _HELP("Simulate relase")),
		InputPortConfig<float>("Value",        1.0f,                             _HELP("Value between 0 and 1")),
		{ 0 }
	};

	static const SOutputPortConfig out_ports[] =
	{
		OutputPortConfig_Void("Pressed",  _HELP("Pressed")),
		OutputPortConfig_Void("Held",     _HELP("Held")),
		OutputPortConfig_Void("Released", _HELP("Released")),
		{ 0 }
	};

	config.sDescription = _HELP("Simulate player actions input");
	config.pInputPorts = in_ports;
	config.pOutputPorts = out_ports;
	config.SetCategory(EFLN_DEBUG);
}

void CFlowNode_SimulateInput::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate)
	{
		bool isPressPort = IsPortActive(pActInfo, eInputPorts_Press);
		bool isReleasePort = IsPortActive(pActInfo, eInputPorts_Release);
		bool isHoldPort = IsPortActive(pActInfo, eInputPorts_Hold);

		bool triggerResponse = false;
		int playerInputEvent;
		int nodeOutput;
		if (isPressPort)
		{
			triggerResponse = true;
			playerInputEvent = eAAM_OnPress;
			nodeOutput = eOutputPort_Pressed;
		}
		else if (isHoldPort)
		{
			triggerResponse = true;
			playerInputEvent = eAAM_OnHold;
			nodeOutput = eOutputPort_Held;
		}
		else if (isReleasePort)
		{
			triggerResponse = true;
			playerInputEvent = eAAM_OnRelease;
			nodeOutput = eOutputPort_Released;
		}

		if (triggerResponse)
		{
			CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());

			if (pClientActor)
			{
				if (pClientActor->GetActorClass() == CPlayer::GetActorClassType())
				{
					CPlayer* pClientPlayer = static_cast<CPlayer*>(pClientActor);
					const string action = GetPortString(pActInfo, eInputPorts_Action);
					const float inputValue = GetPortFloat(pActInfo, eInputPorts_Value);

					IPlayerInput* pPlayerInput = pClientPlayer->GetPlayerInput();
					if (pPlayerInput)
					{
						CODECHECKPOINT(SimulateInput_ProcessEvent_SendAction);
						pPlayerInput->OnAction(CCryName(action), playerInputEvent, isReleasePort ? 0.0f : clamp_tpl(inputValue, 0.0f, 1.0f));
					}
				}
			}

			ActivateOutput(pActInfo, nodeOutput, true);
		}
	}
}

void CFlowNode_SimulateInput::GetMemoryUsage(ICrySizer* sizer) const
{
	sizer->Add(*this);
}

REGISTER_FLOW_NODE("FeatureTest:SimulateInput", CFlowNode_SimulateInput);
