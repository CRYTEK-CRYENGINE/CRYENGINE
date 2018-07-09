// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INTERSECTIONASSISTANCEUNIT_H
#define __INTERSECTIONASSISTANCEUNIT_H

#define NUM_TEST_OFFSETS 8

class CIntersectionAssistanceUnit
{
public:

	CIntersectionAssistanceUnit();
	~CIntersectionAssistanceUnit();

	enum eResolutionPolicy
	{
		eRP_None = 0,
		eRP_SleepObject,
		eRP_DeleteObject,
	};

	// Embed PREVENTION - Run tests at a series of given Rotations from the focal entity (center?), for the OOBB of the subject entity to try and determine 'safe' positions that do not embed this object
	// Also remembers last known good positions (within a given tolerance)
	void SetFocalEntityId(EntityId id); 
	void UpdateEmbeddingPrevention();
	
	// Returns true if object requires adjust, false if current orientation already acceptable
	bool GetSafeLocationForSubjectEntity(QuatT& safePos);
	// ~Embed PREVENTION


	// Embed RESOLUTION - Begin Evaluation of subject entity over several seconds to attempt to determine if the entity has become embedded. If so, react as specified by current eResolutionPolicy (e.g.sleep or delete)
	void Init(EntityId subjectEntityId, eResolutionPolicy resolutionPolicy);
	ILINE void SetClientHasAuthority(const bool bHasAuthority) {m_bClientHasAuthority = bHasAuthority;};

	void BeginCheckingForObjectEmbed(); 
	void UpdateCheckingForObjectEmbed(const float dt);
	void AbortCheckingForObjectEmbed(); 
	// ~Embed RESOLUTION

	// GENERAL
	enum eTestMethod
	{
		eTM_Immediate = 0,
		eTM_Deferred
	};
	bool TestForIntersectionAtLocation(const eTestMethod testMethod, const Matrix34& wMat, EntityId testEntityId, EntityId ignoreEnt, QuatT& outAdjustedResult, const bool bCentreOnFocalEnt = false, bool bRenderOnFail = true, const int index = -1); 
	bool IsPositionWithinAcceptedLimits(const Vec3& vTestPos, const Vec3& vOrigin, const float fTolerance) const;
	ILINE bool Enabled() const  {return m_bEnabled;} 

	void Update(const float dt);

	// public Callbacks required by internal logic
	void SetEmbedPreventionResult(uint8 index, const QuatT& result); 
	void OnCollision(const EventPhysCollision& collision);

#ifndef _RELEASE
	void DebugUpdate() const; 
#endif // #ifndef _RELEASE

private:

	// Handle deferred primitive tests pushed onto physics thread
	class CPrimitiveIntersectionTester
	{
	public:
		CPrimitiveIntersectionTester();
		~CPrimitiveIntersectionTester();

		void Init(CIntersectionAssistanceUnit* pResultsOwner);
		void DoCheck( uint8 index, const primitives::box& testBox, QuatT& resultIfSuccess, IPhysicalEntity* pIgnoreEntity = NULL );
		void IntersectionTestComplete(const QueuedIntersectionID& intersectionID, const IntersectionTestResult& result);
		void SetNumTestOffsets(uint8 numTestOffsets); 
		void Reset();

		bool IsObstructed(int index);

	private:
		struct STest
		{
			STest()
			{
				Clear(); 
			}

			void Clear()
			{
				m_queuedPrimitiveId			= 0;
				m_resultIfSuccess = QuatT(IDENTITY);
			}

			QueuedIntersectionID	m_queuedPrimitiveId;
			QuatT									m_resultIfSuccess; 
		};

		typedef std::vector<STest> TTestContainer;
		TTestContainer m_testContainer;
		CIntersectionAssistanceUnit* m_pResultsOwner;
	};

	void OnDetectObjectEmbedded(); 
	bool IsClientResponsibleForSubjectEntity() const; 
	bool TestForIntersection(const eTestMethod testMethod, const QuatT& qWOrient, const Quat& qRotOffset, const bool bCentreOnFocalEnt, int index);
	Vec3 CalculateTargetAdjustPoint(const IEntity* pEntity, const Matrix34 &wMat, const Vec3& vStartingPos) const;
	bool GetHighestScoringLastKnownGoodPosition(const QuatT& baseOrientation, QuatT& outQuat) const; 

	enum eEmbedState
	{
		eES_None = 0,
		eES_Evaluating,
		eES_ReEvaluating,
		eES_Embedded,
		eES_NotEmbedded
	};
	eEmbedState			  m_embedState; 
	eResolutionPolicy m_resolutionPolicy;

	CPrimitiveIntersectionTester m_intersectionTester;

	typedef CryFixedArray<Quat,NUM_TEST_OFFSETS> TRotationContainer; 
	typedef std::vector<QuatT> TLastKnownGoodContainer;

	TLastKnownGoodContainer m_lastKnownGoodPositions;
	static TRotationContainer s_embedPreventRotations;// Test at these rotations for safe orientations.

	Vec3				m_entityStartingWPos; 
	float				m_embedTimer; 
	EntityId		m_subjectEntityId;			// the entity we are performing intersection tests for (e.g. environmental weapon)
	EntityId    m_focalEntityId;				// the entity we want to center the intersection tests around (e.g. player)
	uint32			m_collisionCount;
	bool				m_bClientHasAuthority;
	bool				m_bEnabled;
	uint8				m_currentTestIndex;

#ifndef _RELEASE
	mutable int	m_currentBestIndex; // *cough* mutable... 
#endif // #ifndef _RELEASE
};

#endif
