// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PostureManager.h"
#include "Cover/CoverSystem.h"

PostureManager::PostureManager()
	: m_queryGenID(0)
	, m_queueTail(0)
	, m_queueSize(0)
{
	m_queue.resize(2);
}

PostureManager::~PostureManager()
{
	QueuedPostureQueries::iterator it = m_queue.begin();
	QueuedPostureQueries::iterator end = m_queue.end();

	for (; it != end; ++it)
	{
		QueuedQuery& query = *it;

		CancelRays(query);
	}
}

void PostureManager::CancelRays(QueuedQuery& query)
{
	QueuedPostureChecks& postureChecks = query.postureChecks;
	QueuedPostureChecks::iterator pit = postureChecks.begin();
	QueuedPostureChecks::iterator pend = postureChecks.end();

	for (; pit != pend; ++pit)
	{
		QueuedPostureCheck& cancellingCheck = *pit;

		if (cancellingCheck.leanabilityRayID)
			gAIEnv.pRayCaster->Cancel(cancellingCheck.leanabilityRayID);

		if (cancellingCheck.aimabilityRayID)
			gAIEnv.pRayCaster->Cancel(cancellingCheck.aimabilityRayID);

		if (cancellingCheck.visibilityRayID)
			gAIEnv.pRayCaster->Cancel(cancellingCheck.visibilityRayID);
	}

	query.postureChecks.clear();
}

void PostureManager::ResetPostures()
{
	QueuedPostureQueries::iterator it = m_queue.begin();
	QueuedPostureQueries::iterator end = m_queue.end();

	for (; it != end; ++it)
	{
		QueuedQuery& query = *it;

		CancelRays(query);
	}

	m_postureInfos.clear();
}

void PostureManager::AddDefaultPostures(PostureType type)
{
	switch (type)
	{
	case HidePosture:
		{
			AddPosture(PostureInfo(HidePosture, "HideProne", 0.0f, 0.0f, STANCE_PRONE, 5.0f));
			AddPosture(PostureInfo(HidePosture, "HideCrouch", 0.0f, 0.0f, STANCE_CROUCH, 3.0f));
			AddPosture(PostureInfo(HidePosture, "HideStealth", 0.0f, 0.0f, STANCE_STEALTH, 2.0f));
			AddPosture(PostureInfo(HidePosture, "HideStand", 0.0f, 0.0f, STANCE_STAND, 1.0f));
		}
		break;
	case AimPosture:
		{
			AddPosture(PostureInfo(AimPosture, "AimProne", 0.0f, 0.0f, STANCE_PRONE, 5.0f));
			const PostureID crouchID = AddPosture(PostureInfo(AimPosture, "AimCrouch", 0.0f, 0.0f, STANCE_CROUCH, 4.0f));

			AddPosture(PostureInfo(AimPosture, "AimCrouchRightLean", 1.0f, 0.0f, STANCE_CROUCH, 3.0f, "peekRight", crouchID));
			AddPosture(PostureInfo(AimPosture, "AimCrouchLeftLean", -1.0f, 0.0f, STANCE_CROUCH, 3.0f, "peekLeft", crouchID));

			const PostureID standID = AddPosture(PostureInfo(AimPosture, "AimStand", 0.0f, 0.0f, STANCE_STAND, 2.0f));
			AddPosture(PostureInfo(AimPosture, "AimStandRightLean", 1.0f, 0.0f, STANCE_STAND, 1.0f, "peekRight", standID));
			AddPosture(PostureInfo(AimPosture, "AimStandLeftLean", -1.0f, 0.0f, STANCE_STAND, 1.0f, "peekLeft", standID));
		}
		break;
	default:
		assert(0);
		break;
	}
}

PostureManager::PostureID PostureManager::AddPosture(const PostureInfo& posture)
{
	m_postureInfos.push_back(posture);

	return m_postureInfos.size() - 1;
}

void PostureManager::SetPosture(PostureID postureID, const PostureInfo& posture)
{
	assert(postureID >= 0 && postureID < (PostureID)m_postureInfos.size());
	m_postureInfos[postureID] = posture;
}

bool PostureManager::GetPosture(PostureID postureID, PostureInfo* posture) const
{
	if ((postureID < 0) || (postureID >= (int)m_postureInfos.size()))
		return false;

	if (posture)
		*posture = m_postureInfos[postureID];

	return true;
}

PostureManager::PostureID PostureManager::GetPostureID(const char* postureName) const
{
	for (uint32 i = 0; i < m_postureInfos.size(); ++i)
	{
		if (m_postureInfos[i].name == postureName)
			return i;
	}

	return -1;
}

bool PostureManager::GetPostureByName(const char* postureName, PostureInfo* posture) const
{
	for (uint32 i = 0; i < m_postureInfos.size(); ++i)
	{
		const PostureInfo& info = m_postureInfos[i];

		if (info.name == postureName)
		{
			if (posture)
				*posture = info;

			return true;
		}
	}

	return false;
}

void PostureManager::SetPosturePriority(PostureID postureID, float priority)
{
	if ((postureID < 0) || (postureID >= (int)m_postureInfos.size()))
		return;

	m_postureInfos[postureID].priority = priority;
}

float PostureManager::GetPosturePriority(PostureID postureID) const
{
	if ((postureID < 0) || (postureID >= (int)m_postureInfos.size()))
		return 0.0f;

	const PostureInfo& postureInfo = m_postureInfos[postureID];

	return GetPosturePriority(postureInfo.parentID) + postureInfo.priority;
}

namespace
{
struct Mate
{
	Mate()
		: physicsBoundingBox(AABB::RESET)
		, headPosition(ZERO)
		, footPosition(ZERO)
		, actor(NULL)
	{
	}

	AABB      physicsBoundingBox;
	Vec3      headPosition;
	Vec3      footPosition;
	CAIActor* actor;
};

typedef std::vector<Mate> Mates;
}

PostureManager::PostureQueryID PostureManager::QueryPosture(const PostureQuery& postureQuery)
{
	assert(postureQuery.actor);

	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;

	if (postureQuery.checks & CheckFriendlyFire)
		lookUp.Prepare(ActorLookUp::Position);

	uint32 capacity = m_queue.size();

	if (++m_queueSize > capacity)
	{
		m_queue.insert(m_queue.begin() + m_queueTail, QueuedQuery());
		m_queueTail = (m_queueTail + 1) % ++capacity;
	}

	assert(m_queueTail < capacity);
	assert(capacity >= m_queueSize);
	assert(capacity <= 4);

	QueuedQuery& newQuery = m_queue[m_queueTail];
	m_queueTail = (m_queueTail + 1) % capacity;

	newQuery.queryID = ++m_queryGenID;
	newQuery.postureChecks.clear();
	newQuery.postureChecks.reserve(16);

	uint32 runningPostureCount = 0;
	RunningPosture runningPostures[96];

	for (size_t i = 0; i < m_postureInfos.size(); ++i)
	{
		const PostureInfo& postureInfo = m_postureInfos[i];

		if ((postureInfo.type == postureQuery.type) &&
		    (postureQuery.allowLean || (fabs_tpl(postureInfo.lean) <= 0.001f)) &&
		    (postureQuery.allowProne || (postureInfo.stance != STANCE_PRONE)))
		{
			new(&runningPostures[runningPostureCount++])RunningPosture(static_cast<int16>(i));
		}
	}

	CAIActor* actorPerformingQuery = postureQuery.actor;

	if (postureQuery.stickyStance && actorPerformingQuery)
	{
		std::sort(&runningPostures[0], &runningPostures[0] + runningPostureCount,
		          StickyStancePostureSorter(*this, m_postureInfos, (EStance)postureQuery.actor->GetState().bodystate,
		                                    postureQuery.hintPostureID));
	}
	else
	{
		std::sort(&runningPostures[0], &runningPostures[0] + runningPostureCount,
		          PostureSorter(*this, postureQuery.hintPostureID));
	}

	PhysSkipList skipList;
	if (actorPerformingQuery)
		actorPerformingQuery->GetPhysicalSkipEntities(skipList);

	IPhysicalEntity** skipListArray = &skipList[0];
	uint32 skipListSize = skipList.size();

	IAIActorProxy* proxy = actorPerformingQuery ? actorPerformingQuery->GetProxy() : NULL;

	float distanceSq = Distance::Point_PointSq(postureQuery.target, postureQuery.position);
	SAIBodyInfo bodyInfo;

	// Gather information about mates
	Mates mates;
	if (postureQuery.checks & CheckFriendlyFire)
	{
		pe_status_pos ppos;

		const size_t actorCount = lookUp.GetActiveCount();
		for (size_t actorIndex = 0; actorIndex < actorCount; ++actorIndex)
		{
			if (CAIActor* mateActor = lookUp.GetActor<CAIActor>(actorIndex))
			{
				if (actorPerformingQuery && mateActor != actorPerformingQuery)
				{
					if (!actorPerformingQuery->IsHostile(mateActor))
					{
						if (IPhysicalEntity* physicalEntity = mateActor->GetPhysics())
						{
							if (physicalEntity->GetStatus(&ppos))
							{
								mates.push_back(Mate());
								Mate& mate = mates.back();
								mate.physicsBoundingBox = AABB(ppos.pos - ppos.BBox[0], ppos.pos + ppos.BBox[1]);
								mate.headPosition = mateActor->GetPos();
								mate.footPosition = mateActor->GetPhysicsPos();
								mate.actor = mateActor;
							}
						}
					}
				}
			}
		}
	}

	// Update the postures.
	for (uint32 i = 0; i < runningPostureCount; ++i)
	{
		RunningPosture& posture = runningPostures[i];
		const PostureInfo& info = m_postureInfos[posture.postureID];

		if (!info.enabled)
			continue;

		if (distanceSq < sqr(info.minDistanceToTarget))
			continue;

		if (distanceSq > sqr(info.maxDistanceToTarget))
			continue;

		uint8 positiveResultCount = 0;
		uint8 awaitingResultCount = 0;

		if (!posture.processed)
		{
			if (!proxy->QueryBodyInfo(SAIBodyInfoQuery(
			                            postureQuery.position, postureQuery.target, info.stance, info.lean, info.peekOver, false), bodyInfo))
				continue;

			posture.eye = bodyInfo.vEyePos;
			posture.weapon = bodyInfo.vFirePos;
			posture.processed = true;
		}

		Vec3 weaponDir = postureQuery.target - posture.weapon;

		if (postureQuery.checks & CheckFriendlyFire)
		{
			bool valid = true;
			Ray fireRay(posture.weapon, weaponDir);

			Mates::iterator mateIt = mates.begin();
			Mates::iterator mateEnd = mates.end();

			for (; mateIt != mateEnd; ++mateIt)
			{
				const Mate& mate = *mateIt;

				const Vec3 headPosition = mate.headPosition;
				const Vec3 footPosition = mate.footPosition;
				const AABB physicsBoundingBox = mate.physicsBoundingBox;

				Vec3 dummyPoint;
				IF_UNLIKELY (Intersect::Ray_AABB(fireRay, physicsBoundingBox, dummyPoint))
				{
					valid = false;
					break;
				}

				// Additional check for the head, in case of leaning.
				if (fabsf(actorPerformingQuery->GetState().lean) >= 0.001f)
				{
					// The reason using the weapon position here is that it is close to the body and
					// better represents the leaned out upper body.
					Vec3 toeToHead = headPosition - footPosition;
					float len = toeToHead.GetLength();
					if (len > 0.0001f)
						toeToHead *= (len - 0.3f) / len;

					Vec3 dummy;
					IF_UNLIKELY (Intersect::Ray_Sphere(fireRay, Sphere(footPosition + toeToHead, 0.4f + 0.05f), dummy, dummy))
					{
						valid = false;
						break;
					}
				}
			}

			if (!valid)
				continue;
		}

		if (postureQuery.checks & CheckVisibility)
		{
			if (postureQuery.coverID)
			{
				const CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(postureQuery.coverID);

				if (surface.IsPointInCover(postureQuery.target, posture.eye))
					continue;
			}
		}

		if (postureQuery.checks & CheckAimability)
		{
			if (postureQuery.coverID)
			{
				const CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(postureQuery.coverID);

				if (surface.IsPointInCover(postureQuery.target, posture.weapon))
					continue;
			}
		}

		// deferred checks follow
		QueuedRayID leanabilityRayID = 0;
		QueuedRayID visibilityRayID = 0;
		QueuedRayID aimabilityRayID = 0;

		if (postureQuery.checks & CheckVisibility)
		{
			Vec3 eyeDir = postureQuery.target - posture.eye;

			visibilityRayID = gAIEnv.pRayCaster->Queue(RayCastRequest::HighPriority,
			                                           RayCastRequest(posture.eye, eyeDir * postureQuery.distancePercent, COVER_OBJECT_TYPES,
			                                                          AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER, skipListArray, skipListSize),
			                                           functor(*this, &PostureManager::RayComplete));
			++awaitingResultCount;
		}
		else
			++positiveResultCount;

		if (postureQuery.checks & CheckAimability)
		{
			aimabilityRayID = gAIEnv.pRayCaster->Queue(RayCastRequest::HighPriority,
			                                           RayCastRequest(posture.weapon, weaponDir * postureQuery.distancePercent, COVER_OBJECT_TYPES,
			                                                          AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER, skipListArray, skipListSize),
			                                           functor(*this, &PostureManager::RayComplete));
			++awaitingResultCount;
		}
		else
			++positiveResultCount;

		++positiveResultCount;
		if (postureQuery.checks & CheckLeanability)
		{
			// Check if the path from the non-lean state to the lean state is clear.
			if (info.parentID != -1)
			{
				for (uint32 p = 0; p < runningPostureCount; ++p)
				{
					RunningPosture& parentPosture = runningPostures[p];

					if (parentPosture.postureID == info.parentID)
					{
						const PostureInfo& parentInfo = m_postureInfos[parentPosture.postureID];

						if (!parentPosture.processed)
						{
							SAIBodyInfo parentBodyInfo;

							if (proxy->QueryBodyInfo(
							      SAIBodyInfoQuery(postureQuery.position, postureQuery.target, parentInfo.stance, parentInfo.lean,
							                       parentInfo.peekOver, false), parentBodyInfo))
							{
								parentPosture.eye = parentBodyInfo.vEyePos;
								parentPosture.weapon = parentBodyInfo.vFirePos;
								parentPosture.processed = true;
							}
						}

						const Vec3 eyeToParentEye = parentPosture.eye - posture.eye;
						if (!eyeToParentEye.IsZero())
						{
							leanabilityRayID = gAIEnv.pRayCaster->Queue(RayCastRequest::HighPriority,
							                                            RayCastRequest(parentPosture.eye, eyeToParentEye, COVER_OBJECT_TYPES,
							                                                           AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER, skipListArray, skipListSize),
							                                            functor(*this, &PostureManager::RayComplete));

							--positiveResultCount;
							++awaitingResultCount;
						}
					}
				}
			}
		}

		if ((positiveResultCount == TotalCheckCount) && newQuery.postureChecks.empty())
		{
			newQuery.status = AsyncComplete;
			newQuery.result = posture.postureID;

			return m_queryGenID;
		}
		else
		{
			newQuery.postureChecks.resize(newQuery.postureChecks.size() + 1);

			QueuedPostureCheck& newCheck = newQuery.postureChecks.back();
			newCheck.leanabilityRayID = leanabilityRayID;
			newCheck.aimabilityRayID = aimabilityRayID;
			newCheck.visibilityRayID = visibilityRayID;
			newCheck.awaitingResultCount = awaitingResultCount;
			newCheck.positiveResultCount = positiveResultCount;
			newCheck.postureID = posture.postureID;
		}
	}

	newQuery.status = AsyncInProgress;

	if (newQuery.postureChecks.empty())
	{
		--m_queueSize;

		newQuery.queryID = 0;

		return 0;
	}

	return m_queryGenID;
}

void PostureManager::CancelPostureQuery(PostureQueryID queryID)
{
	QueuedPostureQueries::iterator it = m_queue.begin();
	QueuedPostureQueries::iterator end = m_queue.end();

	for (; it != end; ++it)
	{
		QueuedQuery& query = *it;

		if (query.queryID == queryID)
		{
			CancelRays(query);
			--m_queueSize;

			query.queryID = 0;

			return;
		}
	}
}

AsyncState PostureManager::GetPostureQueryResult(PostureQueryID queryID, PostureID* postureID, PostureInfo** postureInfo)
{
	QueuedPostureQueries::iterator it = m_queue.begin();
	QueuedPostureQueries::iterator end = m_queue.end();

	for (; it != end; ++it)
	{
		QueuedQuery& query = *it;

		if (query.queryID == queryID)
		{
			if (query.status != AsyncInProgress)
			{
				--m_queueSize;

				if (query.status == AsyncComplete)
				{
					*postureID = query.result;
					*postureInfo = &m_postureInfos[query.result];
				}

				query.queryID = 0;
			}

			AsyncState status = query.status;

			return status;
		}
	}

	return AsyncFailed;
}

void PostureManager::RayComplete(const QueuedRayID& rayID, const RayCastResult& result)
{
	QueuedPostureQueries::iterator it = m_queue.begin();
	QueuedPostureQueries::iterator end = m_queue.end();

	for (; it != end; ++it)
	{
		QueuedQuery& query = *it;

		QueuedPostureChecks& postureChecks = query.postureChecks;
		QueuedPostureChecks::iterator pit = postureChecks.begin();
		QueuedPostureChecks::iterator pend = postureChecks.end();

		for (; pit != pend; )
		{
			QueuedPostureCheck& check = *pit;

			if (check.leanabilityRayID == rayID)
				check.leanabilityRayID = 0;
			else if (check.aimabilityRayID == rayID)
				check.aimabilityRayID = 0;
			else if (check.visibilityRayID == rayID)
				check.visibilityRayID = 0;
			else
			{
				++pit;
				continue;
			}

			check.positiveResultCount += result ? 0 : 1;
			--check.awaitingResultCount;

			if (check.awaitingResultCount == 0)
			{
				if (check.positiveResultCount != TotalCheckCount)
					postureChecks.erase(pit);

				if (!query.postureChecks.empty())
				{
					QueuedPostureCheck& front = query.postureChecks.front();
					if (front.positiveResultCount == TotalCheckCount)
					{
						CancelRays(query);

						query.status = AsyncComplete;
						query.result = front.postureID;
						query.postureChecks.clear();
					}
				}
				else
					query.status = AsyncFailed;
			}

			return;
		}
	}
}
