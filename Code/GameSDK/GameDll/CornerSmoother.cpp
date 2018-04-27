// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/
#include "StdAfx.h"
#include "CornerSmoother.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "Utility/Hermite.h"
#include <CryAISystem/IPathfinder.h> // for pathfollower
#include "GameCVars.h"

namespace CornerSmoothing
{
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
	#define TWEAKABLE static
#else
	#define TWEAKABLE static const
#endif

	// See MaximumSpeedForTurnRadius() for more details about these magic numbers & formulae:
	TWEAKABLE float MAXIMUM_TURNRADIUS = 50.0f; // the turnradius we assume is a 'straight line'
	TWEAKABLE float MAXIMUM_SPEED = 1.f/198.f*(168.f+sqrtf(-173769.f+330000.f*MAXIMUM_TURNRADIUS));
	//TWEAKABLE float MINIMUM_TURN_RADIUS = 173769.f/330000.f; // about 0.5m

	TWEAKABLE float MIN_SPEED_FOR_PLANNING = 0.1f;
	TWEAKABLE float MIN_DESIRED_SPEED_FOR_PLANNING = 0.1f;

	TWEAKABLE float TAKECORNER_MAX_DIVERGENCE_FROM_PATH = 0.1f;
	TWEAKABLE float TAKECORNER_MAX_DIVERGENCE_FROM_PATH_SQ = TAKECORNER_MAX_DIVERGENCE_FROM_PATH * TAKECORNER_MAX_DIVERGENCE_FROM_PATH;
	TWEAKABLE float TAKECORNER_MAX_MOVEMENTDIRECTION_ANGLE_DIVERGENCE = DEG2RAD(5.0f);
	TWEAKABLE float TAKECORNER_COS_MAX_MOVEMENTDIRECTION_ANGLE_DIVERGENCE = cosf(TAKECORNER_MAX_MOVEMENTDIRECTION_ANGLE_DIVERGENCE);
	TWEAKABLE float TAKECORNER_MAX_SPEED_DIVERGENCE = 0.5f;
	static const int TAKECORNER_MAX_SHOOTOUT_SPLINES = 3;

	TWEAKABLE float SHOOTOUT_PREDICTION_STEP_DISTANCE_EPSILON = 0.01f; // m
	TWEAKABLE float SHOOTOUT_PREDICTION_NUM_STEPS_PER_HERMITE_METERS = 0.2f; // TODO: use higher number on PC
	TWEAKABLE int   SHOOTOUT_PREDICTION_MIN_NUM_STEPS = 3; // TODO: use higher number on PC
	TWEAKABLE float SHOOTOUT_PREDICTION_MIN_STEP_DISTANCE_ALONG_CURVE = 0.1f;
	TWEAKABLE float SHOOTOUT_PREDICTION_MIN_PATH_DISTANCE_BEHIND_CORNER = 0.1f; // m - minimum distance behind corner before we take the corner into account
	static const int SHOOTOUT_PREDICTION_MAX_SAMPLES = 25; // with the above settings, should be enough for 50m long distance to movetarget -- TODO: Use higher number on PC
	TWEAKABLE float SHOOTOUT_PREDICTION_SIMPLIFICATION_THRESHOLD_DISTANCE = 0.25f;
	TWEAKABLE float SHOOTOUT_PREDICTION_SIMPLIFICATION_THRESHOLD_DISTANCE_SQ = SHOOTOUT_PREDICTION_SIMPLIFICATION_THRESHOLD_DISTANCE*SHOOTOUT_PREDICTION_SIMPLIFICATION_THRESHOLD_DISTANCE;

	TWEAKABLE ColorF SHOOTOUT_PREDICTION_FAIL_COLOR = ColorF(1.0f, 0.4f, 0.1f, 1.0f);
	TWEAKABLE ColorF SHOOTOUT_PREDICTION_SUCCESS_COLOR = ColorF(0.0f, 0.9f, 0.4f, 1.0f);

	TWEAKABLE float SHOOTOUT_MAX_TARGET_DIFFERENCE = 0.1f; // m
	TWEAKABLE float SHOOTOUT_MAX_TARGET_DIFFERENCE_SQ = SHOOTOUT_MAX_TARGET_DIFFERENCE * SHOOTOUT_MAX_TARGET_DIFFERENCE;
	TWEAKABLE float SHOOTOUT_MAX_INFLECTIONPOINT_DIFFERENCE = 0.1f; // m
	TWEAKABLE float SHOOTOUT_MAX_INFLECTIONPOINT_DIFFERENCE_SQ = SHOOTOUT_MAX_INFLECTIONPOINT_DIFFERENCE * SHOOTOUT_MAX_INFLECTIONPOINT_DIFFERENCE;

	// ===========================================================================

	struct SSimpleSample
	{
		Vec2 pos;
	};

	// ===========================================================================

	struct SSplineSample
	{
		Vec2 pos;
		float alpha;
		float distance;
		float maximumSpeedSq;
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		float turnRadius;
#endif
	};

	// ===========================================================================

	// Predicted 2D trajectory
	//
	// When numSamples==0 the prediction is empty (invalid)
	//
	template <class SampleType>
	struct SPrediction_tpl
	{
		SPrediction_tpl() : numSamples(0) {}
		void Reset() { numSamples = 0; }

#if !defined(_RELEASE)
		bool IsValid() const
		{
			return (numSamples >= 0);
		}
#endif
		int numSamples;
		SampleType samples[SHOOTOUT_PREDICTION_MAX_SAMPLES];
	};

	typedef SPrediction_tpl<SSimpleSample> SSimplifiedPrediction;
	typedef SPrediction_tpl<SSplineSample> SSplinePrediction;

	// ===========================================================================

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
	namespace Debug
	{
		// TODO: Turn these into static const

		static int START_Y = 195; // TODO: Turn into 190?
		static int START_X = 10;
		static int LOG_Y_INCREMENT = 9;
		static int MAX_LOG_ENTRIES = 55; // TODO: Turn into 60??
		static float FONT_SIZE = 1.2f;

		static float RECENT_LOG_ENTRY_COLOR[4] = {0.6f,0.77f,0.47f,1};
		static float OLD_LOG_ENTRY_COLOR[4] = {0.4f,0.44f,0.3f,1};

		static int s_currentY = START_Y;
		static int s_frameID = -1;

		struct SLogEntry
		{
			int frameID;
			string text;
		};

		typedef std::deque<SLogEntry> TLogEntries;
		static TLogEntries s_logEntries;
		static bool s_debugEnabledDuringThisUpdate = false;

		static char s_buffer[256];

		void RegisterCVars()
		{

		}


		void UnregisterCVars()
		{

		}


		void AddLogEntry(const char *_pszFormat,...)
		{
			va_list args;
			va_start(args, _pszFormat);
			cry_vsprintf(s_buffer, _pszFormat, args);
			va_end(args);

			if (s_logEntries.size() == MAX_LOG_ENTRIES)
			{
				s_logEntries.pop_front();
			}

			SLogEntry entry;
			entry.frameID = gEnv->pRenderer->GetFrameID(false);
			entry.text = string(s_buffer);

			s_logEntries.push_back(entry);

			static bool logToLog = false;
			if (logToLog)
			{
				CryLog("%s", s_buffer);
			}
		}


		void Begin(const string& name)
		{
			if (s_frameID != gEnv->pRenderer->GetFrameID(false))
			{
				s_frameID = gEnv->pRenderer->GetFrameID(false);
				s_currentY = START_Y;
			}

			IRenderAuxText::Draw2dLabel( (float)START_X, (float)s_currentY, FONT_SIZE, RECENT_LOG_ENTRY_COLOR, false,
				"Cornersmoother '%s':",
				name.c_str()
				);

			s_currentY += 2*LOG_Y_INCREMENT;
		}


		void OnLevelUnload()
		{
			stl::free_container(s_logEntries);
		}


		void DrawState(const SState& state, const bool hasOutput, const SOutput& output)
		{
			// TODO
		}


		void DrawLog()
		{
			//return;

			const TLogEntries::const_iterator iEnd = s_logEntries.end();
			for(TLogEntries::const_iterator i = s_logEntries.begin(); i != iEnd; ++i)
			{
				const SLogEntry& entry = *i;

				const int age = s_frameID - entry.frameID;

				float f = FONT_SIZE;
				float* color = (age == 0) ? RECENT_LOG_ENTRY_COLOR : OLD_LOG_ENTRY_COLOR;
				IRenderAuxText::Draw2dLabel( (float)START_X, (float)s_currentY, FONT_SIZE, color, false,
					"age:%5d  %s",
					age,
					entry.text.c_str()
					);

				s_currentY += LOG_Y_INCREMENT;
			}
		}
	}
#endif


#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
#define CORNERSMOOTHER_LOG(format, ...) { if (Debug::s_debugEnabledDuringThisUpdate) Debug::AddLogEntry(format, ##__VA_ARGS__); }
#else
#define CORNERSMOOTHER_LOG(format, ...)
#endif

	// ===========================================================================

	struct SHermiteSpline
	{
		Vec2 source2D;
		Vec2 sourceTangent2D;
		Vec2 target2D;
		Vec2 targetTangent2D;

#if !defined(_RELEASE)
		bool IsValid() const
		{
			return
				source2D.IsValid() &&
				target2D.IsValid() &&
				sourceTangent2D.IsValid() &&
				targetTangent2D.IsValid();
		}
#endif
	};

	// ===========================================================================

	static ILINE void EvaluateSplineAt(const SHermiteSpline& data, const float alpha, Vec2& output)
	{
		output = ::HermiteInterpolate(data.source2D, data.sourceTangent2D, data.target2D, data.targetTangent2D, alpha);
	}


	enum EFindPointAlongSplineResult
	{
		eFPAS_Success, // newAlpha and newPos are filled in and can be trusted
		eFPAS_BeyondEndOfSpline, // newAlpha and newPos are filled in: they are 1.0f and the final position of the spline
		eFPAS_AtEndOfSpline, // newAlpha and newPos are filled in: they are 1.0f and the final position of the spline

		eFPAS_NotEnoughPrecision, // Failure: newAlpha and newPos are undefined
		eFPAS_TooManySteps, // Failure: newAlpha and newPos are undefined
	};


	// A binary search assumes the spline is very simple
	static inline EFindPointAlongSplineResult FindPointAlongSplineUsingBinarySearch(const SHermiteSpline& data, const float curAlpha, const Vec2& curPos, const float minDistanceSq, const float maxDistanceSq, float& newAlpha, Vec2& newPos)
	{
		TWEAKABLE float MIN_DIFFERENCE_BETWEEN_MIN_AND_MAX_ALPHA = FLT_EPSILON;
		TWEAKABLE float MAX_ITERATIONS = 20;

		CRY_ASSERT(minDistanceSq < maxDistanceSq);

#ifndef NDEBUG
		{
			// curPos should be the spline position at alpha
			Vec2 testPos;
			EvaluateSplineAt(data, curAlpha, testPos);
			CRY_ASSERT((testPos - curPos).GetLength2() < square(0.001f));
		}
#endif

		// Check for overshooting or reaching the end
		{
			const float distanceToEndSq = (data.target2D - curPos).GetLength2();
			if (distanceToEndSq > maxDistanceSq)
			{
				// normal case, end is out of range
			}
			else if (distanceToEndSq < minDistanceSq)
			{
				newAlpha = 1.0f;
				newPos = data.target2D;
				return eFPAS_BeyondEndOfSpline;
			}
			else
			{
				newAlpha = 1.0f;
				newPos = data.target2D;
				return eFPAS_AtEndOfSpline;
			}
		}

		float minAlpha = curAlpha;
		float maxAlpha = 1.0f;
		int numIterations = 0;
		do
		{
			const float tryAlpha = (minAlpha + maxAlpha) * 0.5f;

			CRY_ASSERT(tryAlpha >= minAlpha);
			CRY_ASSERT(tryAlpha <= maxAlpha);

			Vec2 tryPos;
			EvaluateSplineAt(data, tryAlpha, tryPos);

			const float tryDistanceSq = (tryPos - curPos).GetLength2();
			if (tryDistanceSq > maxDistanceSq)
			{
				maxAlpha = tryAlpha;
			}
			else if (tryDistanceSq < minDistanceSq)
			{
				minAlpha = tryAlpha;
			}
			else
			{
				// success
				newPos = tryPos;
				newAlpha = tryAlpha;
				return eFPAS_Success;
			}
		}
		while((++numIterations < MAX_ITERATIONS) && ((maxAlpha - minAlpha) >= MIN_DIFFERENCE_BETWEEN_MIN_AND_MAX_ALPHA));

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		static bool logIterations = true;
		if (logIterations)
		{
			if (numIterations == MAX_ITERATIONS)
			{
				CORNERSMOOTHER_LOG("BinarySearch: Failed to find point within distance in maximum iterations (%d)", numIterations);
			}
			else
			{
				CORNERSMOOTHER_LOG("BinarySearch: Failed to find point within distance in %d iterations, minAlpha & maxAlpha too close (minAlpha = %f, maxAlpha = %f)", numIterations, minAlpha, maxAlpha);
			}
		}
#endif

		return (numIterations == MAX_ITERATIONS) ? eFPAS_TooManySteps : eFPAS_NotEnoughPrecision;
	}


	static inline void SampleSplinePrediction(const SSplineSample*const beginSample, const SSplineSample*const endSample /* one beyond the last */, const float alpha, SSplineSample& output)
	{
		CRY_ASSERT(beginSample != NULL);
		CRY_ASSERT(endSample != NULL);
		CRY_ASSERT(endSample > beginSample);

		for(const SSplineSample* currentSample = beginSample; currentSample != endSample; ++currentSample)
		{
			if (currentSample->alpha >= alpha)
			{
				if (currentSample == beginSample)
				{
					output = *beginSample;
					return;
				}

				const SSplineSample*const prevSample = currentSample - 1;
				output.alpha = alpha;
				output.pos = LERP(prevSample->pos, currentSample->pos, alpha);
				output.distance = LERP(prevSample->distance, currentSample->distance, alpha);
				output.maximumSpeedSq = square(LERP(sqrtf(prevSample->maximumSpeedSq), sqrtf(currentSample->maximumSpeedSq), alpha)); // TODO: Optimize?
				CRY_ASSERT(output.maximumSpeedSq >= 0.0f);
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
				const float halfwayIntervalAlpha = (currentSample->alpha - prevSample->alpha) * 0.5f;
				if (alpha - prevSample->alpha < halfwayIntervalAlpha)
				{
					output.turnRadius = prevSample->turnRadius;
				}
				else
				{
					output.turnRadius = currentSample->turnRadius;
				}
#endif
				return;
			}
		}

		output = *(endSample - 1);
	}


	static ILINE void SampleSplinePrediction(const SSplinePrediction& splinePrediction, const float alpha, SSplineSample& output)
	{
		return SampleSplinePrediction(splinePrediction.samples, splinePrediction.samples + splinePrediction.numSamples, alpha, output);
	}


	/*static ILINE float GetLength(const SSplinePrediction& splinePrediction)
	{
		if (splinePrediction.numSamples > 0)
			return splinePrediction.samples[splinePrediction.numSamples - 1].distance;
		else
			return 0.0f;
	}


	enum EWalkSplineResult
	{
		eWSR_Success, // newAlpha and newPos are filled in and can be trusted

		eWSR_BeyondEndOfSpline, // newAlpha and newPos are filled in: they are 1.0f and the final position of the spline

		eWSR_OverShot, // Failure: newAlpha and newPos are undefined
		eWSR_TooManySteps, // Failure: newAlpha and newPos are undefined
	};


	ILINE EWalkSplineResult WalkDistanceAlongSpline(const SHermiteSpline& spline, const float curAlpha, const Vec2& curPos, const float minDistance, const float maxDistance, float& newAlpha, Vec2& newPos)
	{
		CRY_ASSERT(minDistance < maxDistance);

		static const float MAX_STEP_SIZE = 1.0f/8.0f;
		static const float MIN_STEP_SIZE = MAX_STEP_SIZE/(8.0f*8.0f); // this might be too high, 0.00001f seemed to work though
		static const float MAX_NUM_STEPS = 100;

#ifndef NDEBUG
		{
			// curPos should be the spline position at alpha
			Vec2 testPos;
			EvaluateSplineAt(spline, curAlpha, testPos);
			CRY_ASSERT((testPos - curPos).GetLength2() < square(0.001f));
		}
#endif

		float curStepSize = MAX_STEP_SIZE;
		int numStepsInTotal = 0;
		do
		{
			float tryAlpha = curAlpha + curStepSize;
			float totalDistance = 0.0f;
			bool atEnd = false;
			Vec2 oldPos = curPos;
			Vec2 tryPos;
			do
			{
				if (tryAlpha >= 1.0f)
				{
					if (atEnd)
					{
						newPos = tryPos;
						newAlpha = 1.0f;
						//CORNERSMOOTHER_LOG("WalkDistanceAlongSpline: Stepped beyond the end");
						return eWSR_BeyondEndOfSpline;
					}

					// Try one time at the very end, just in case
					tryAlpha = 1.0f;
					atEnd = true;
				}

				EvaluateSplineAt(spline, tryAlpha, tryPos);

				const float tryDistanceSq = (tryPos - oldPos).GetLength2();
				if(tryDistanceSq > square(0.1f))
				{
					// steps are too big to be safe
					break; // try with lower stepsize
				}

				totalDistance += sqrtf(tryDistanceSq);

				if (totalDistance > maxDistance)
				{
					// we overshot (hopefully not by much)
					break; // try with lower stepsize
				}
				else if (totalDistance >= minDistance)
				{
					// success
					newPos = tryPos;
					newAlpha = tryAlpha;
					return eWSR_Success;
				}

				if (++numStepsInTotal == MAX_NUM_STEPS)
				{
					CORNERSMOOTHER_LOG("WalkDistanceAlongSpline: Failed to find point within distance in maximum steps: %d", numStepsInTotal);
					return eWSR_TooManySteps;
				}

				oldPos = tryPos;
				tryAlpha += curStepSize;
			}
			while(true);

			curStepSize *= 0.5f;
		}
		while(curStepSize >= MIN_STEP_SIZE);

		CORNERSMOOTHER_LOG("WalkDistanceAlongSpline: Stepsize (%f) is not small enough to find a point along the spline within the range", curStepSize);
		return eWSR_OverShot;
	}
*/

// ===========================================================================

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
	namespace Debug
	{
		void DrawSpline(const SHermiteSpline& spline, const ColorF& lineColor, const ColorF& tangentColor, const Vec2& extraPos1, const Vec2& extraPos2, const float sphereRadius = 0.0f, const ColorF& sphereColor = ColorF(1.0f,1.0f,1.0f, 1.0f))
		{
			float lineThickness = 1.5f;

			const int flags3D = e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn;
			const int flags2D = e_Mode2D | e_AlphaBlended;
			IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom();

			pAux->SetRenderFlags(flags2D);

			int numSamples = 50;

			AABB b;
			b.Reset();
			for(int i=0; i<=numSamples; ++i)
			{
				float alpha = float(i)/float(numSamples);

				Vec2 pos;
				EvaluateSplineAt(spline, alpha, pos);
				b.Add(Vec3(pos));
			}

			b.Expand(Vec3(1,1,1)*b.GetRadius()*0.2f);

			const float maxSide = std::max(b.GetSize().x, b.GetSize().y);
			if (maxSide < FLT_EPSILON)
			{
				return;
			}

			Vec3 size = b.GetSize();
			float scale;
			if (size.x <= size.y)
			{
				scale = 1.0f/size.y;
			}
			else
			{
				scale = 1.0f/size.x;
			}

			if (scale < FLT_EPSILON)
			{
				return;
			}
		
			pAux->DrawLine(Vec3((spline.source2D-b.min)*scale), tangentColor, Vec3((spline.source2D+spline.sourceTangent2D-b.min)*scale), tangentColor, lineThickness);
			pAux->DrawLine(Vec3((spline.target2D-b.min)*scale), tangentColor, Vec3((spline.target2D+spline.targetTangent2D-b.min)*scale), tangentColor, lineThickness);

			Vec2 prevPos;
			EvaluateSplineAt(spline, 0.0f, prevPos);
			for(int i=1; i<numSamples; ++i) // start from 1!
			{
				float alpha = float(i)/float(numSamples);

				Vec2 pos;
				EvaluateSplineAt(spline, alpha, pos);

				pAux->DrawLine(Vec3((prevPos-b.min)*scale), lineColor, Vec3((pos-b.min)*scale), lineColor, lineThickness);
				prevPos = pos;
			}

			if (sphereRadius > 0.0f)
			{
				pAux->DrawSphere(Vec3(extraPos1-b.min)*scale, sphereRadius, sphereColor);
				pAux->DrawSphere(Vec3(extraPos2-b.min)*scale, sphereRadius, sphereColor);
			}

			pAux->SetRenderFlags(flags3D);
		}


		const ColorF GetSegmentColor(const SSimpleSample& sample, const ColorF& goodColor, const ColorF& badColor)
		{
			return goodColor;
		}


		const ColorF GetSegmentColor(const SSplineSample& sample, const ColorF& goodColor, const ColorF& badColor)
		{
			return LERP(badColor, goodColor, clamp_tpl(sqrtf(sample.maximumSpeedSq)/6.0f, 0.0f, 1.0f));
		}


		template<class Sample>
		void DrawPrediction(const Vec3& startPosition, const SPrediction_tpl<Sample>& prediction, const ColorF& lineColor, const float sphereRadius = 0.0f, const ColorF& sphereColor = ColorF(1.0f,1.0f,1.0f, 1.0f))
		{
			if (prediction.numSamples < 1)
				return;

			IRenderAuxGeom* pAuxRender = gEnv->pRenderer->GetIRenderAuxGeom();

			const float DEBUGDRAW_PREDICTION_PUSH_UP = 0.1f;
			float z = startPosition.z + DEBUGDRAW_PREDICTION_PUSH_UP;

			Vec3 from = Vec3(prediction.samples[0].pos);

			from.z = z;

			for(int i = 1; i < prediction.numSamples; ++i)
			{
				const ColorF& segmentColor = GetSegmentColor(prediction.samples[i], lineColor, lineColor*ColorF(1.0f,0.2f,0.2f, 1.0f));

				Vec3 to = Vec3(prediction.samples[i].pos);
				to.z = z;
				pAuxRender->DrawLine(from, segmentColor, to, segmentColor);

				if (sphereRadius > 0.0f)
				{
					pAuxRender->DrawSphere(from, sphereRadius, sphereColor, true);
				}

				from = to;
			}
		}

	} // namespace Debug
#endif // INCLUDE_CORNERSMOOTHING_DEBUGGING()

	// ===========================================================================

	// TURNRADIUS = SPEED / TURNSPEED
	// SPEED      = TURNRADIUS * TURNSPEED
	// TURNSPEED  = SPEED / TURNRADIUS

	/*
	// Data from our human motion capture set is approximately linear
	static float SPEED0 = 1.0f;
	static float TURNSPEED0 = 3.0f; 
	static float SPEED1 = 8.0f;
	static float TURNSPEED1 = 1.1f;
	static float MINIMUM_TURNRADIUS = SPEED0/TURNSPEED0;

	static float MaximumTurnSpeedForSpeed(const float speed)
	{
		CRY_ASSERT(SPEED1 > SPEED0);
		CRY_ASSERT(TURNSPEED1 < TURNSPEED0);

		const float alpha = std::max(0.0f, (speed - SPEED0)/(SPEED1 - SPEED0));
		return LERP(TURNSPEED0, TURNSPEED1, alpha);
	}*/

	/*static ILINE float MinimumTurnRadiusForSpeed(const float speed)
	{
		if (speed < 1.0f)
			return (0.1188f*1.0f - 0.2016f)*1.0f + 0.6121f;
		else
			return (0.1188f*speed - 0.2016f)*speed + 0.6121f;
	}*/

	static ILINE float MaximumSpeedForTurnRadius(const float turnRadius)
	{
		// Analysis of our motion capture data revealed a more or less quadratic
		// relationship between speed and minimum turnradius: R = 0.1188*S^2 - 0.2016*S + 0.6121
		// The inverse relationship is: S = 1/198*(168+sqrt(-173769+330000*R))

		//as a result the minimum turn radius = 173769/330000

		if (turnRadius < MAXIMUM_TURNRADIUS)
		{
			const float clampedDeterminant = std::max(0.f, -173769.f + 330000.f * turnRadius);
			return (1.f/198) * (168.f + sqrtf(clampedDeterminant));
		}
		else
		{
			return MAXIMUM_SPEED;
		}
	}


	// What is the maximum speed we can have 'distance' ago so we can still slow down to endSpeed using the maximum deceleration?
	static ILINE float CalculateMaximumSpeedAtDistanceBeforeSq(const float maximumDeceleration, const float endSpeedSq, const float distance)
	{
		// Quadratic equation, solve for t first
		//
		// dist = startspeed*t - maxdecel*t*t/2
		//
		// a = -maxdecel/2
		// b = startspeed
		// c = -dist
		//
		// ==> t = (-b + sqrt(b*b-4*a*c))/2*a
		//     t = (startspeed - sqrt(startspeed^2 - 2*maxdecel*dist))/maxdecel
		//
		// Speed at that time is
		//
		// endspeed = startspeed - maxdecel*t
		// endspeed = startspeed - (startspeed - sqrt(startspeed^2 - 2*maxdecel*dist))
		// endspeed = sqrt(startspeed^2 - 2*maxdecel*dist)
		// endspeed^2 = startspeed^2 - 2*maxdecel*dist
		// endspeed^2 + 2*decel*dist = startspeed^2
		// startspeed = sqrt(endspeed^2 + 2*maxdecel*dist)

		const float startSpeedSq = endSpeedSq + 2.0f * maximumDeceleration * distance;
		CRY_ASSERT(startSpeedSq >= 0.0f);
		return startSpeedSq;
	}


	/*
	// Could we in theory decelerate enough? (assuming we decelerate linearly over the distance)
	static bool ILINE CanWeSlowDownInDistance(const SMovementLimits& limits, const float startSpeed, const float maxDistance, const float targetSpeed, float& minimumStartSpeed)
	{
		// Quadratic equation, solve for t first
		//
		// dist = speed*t - decel*t*t/2
		//
		// a = -decel/2
		// b = speed
		// c = -dist
		//
		// ==> t = (-b + sqrt(b*b-4*a*c))/2*a
		//     t = (speed - sqrt(speed^2 - 2*decel*dist))/decel
		//
		// Speed at that time is
		//
		//     v = speed - decel*t
		//     v = speed - (speed - sqrt(speed^2 - 2*decel*dist))
		//     v = sqrt(speed^2 - 2*decel*dist)

		CRY_ASSERT(limits.speedRange.min > 0.0f);
		const float minimumSpeedWithThisGaitSq = square(limits.speedRange.min);
		const float unclampedSpeedSq = square(startSpeed) - 2.0f * limits.maximumDeceleration * maxDistance;

		const float clampedSpeedAtDistSq = std::max(minimumSpeedWithThisGaitSq, unclampedSpeedSq);
		if (clampedSpeedAtDistSq <= square(targetSpeed))
		{
			// we can make it by slowing down
			CRY_ASSERT(clampedSpeedAtDistSq >= 0.0f);
			minimumStartSpeed = sqrtf(clampedSpeedAtDistSq);
			return true;
		}
		else
		{
			return false;
		}
	}
	*/

	// ===========================================================================

	static ILINE Vec2 MirrorVectorAroundUnitVector(const Vec2& v, const Vec2 unitVector) // TODO: Write CreateReflection() for Vec2
	{
		CRY_ASSERT(unitVector.GetLength() > 0.999f); // TODO: Write IsUnit() for Vec2
		return (2.0f*(v.Dot(unitVector)))*unitVector - v;
	}

	// ===========================================================================

	// Safe distance is defined as 0.1m or 4 frames away from the target
	static float MIN_END_OF_PATH_DISTANCE = 0.1f;
	static int END_OF_PATH_FRAMES = 4;
	static ILINE float CalculateSafeDistanceToEnd(float desiredSpeed, float dt)
	{
		return std::max(MIN_END_OF_PATH_DISTANCE, END_OF_PATH_FRAMES*desiredSpeed*dt);
	}

	// ===========================================================================

	enum EPlanExecutionResult
	{
		EPER_Failed,    // Failed execution. Output is undefined.
		EPER_Continue,  // Normal execution
		EPER_Finished,  // Normal execution and final step in the plan
	};

	struct IExecutingPlan
	{
		virtual ~IExecutingPlan() {}

		virtual bool IsValidFor(const SState& state) const = 0;
		virtual EPlanExecutionResult Execute(const SState& state, SOutput& output) = 0; // return true when done

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		virtual void DebugDraw() const = 0;
#endif
	};

	// ===========================================================================

	class CStopPlan : public IExecutingPlan
	{
	public:
		CStopPlan() {}

		bool IsValidFor(const SState& state) const override { return true; }
	
		EPlanExecutionResult Execute(const SState& state, SOutput& output) override
		{
			output.desiredMovementDirection2D = state.desiredMovementDirection2D;
			output.desiredSpeed = 0.0f;

			return EPER_Finished;
		}

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		void DebugDraw() const override
		{
		}
#endif
	};

	// ===========================================================================

	struct SShootOutData
	{
		Vec3 moveTarget;
		bool hasInflectionPoint;
		Vec3 inflectionPoint;

		SHermiteSpline spline;

		// The following is calculated from the spline and the beginstate
		float minimumTurnRadius;
		SSplinePrediction prediction; // Sampled version of the spline (does contain samples for both begin & end of spline)
		SSimplifiedPrediction simplifiedPrediction; // Simplified version of the sampled spline (contains sample for end of spline but NOT the beginning to match the input of CheckWalkability)

		ILINE float GetMaximumEntrySpeedSq() const
		{
			return prediction.samples[0].maximumSpeedSq;
		}

		ILINE float GetMaximumExitSpeedSq() const
		{
			return prediction.samples[prediction.numSamples - 1].maximumSpeedSq;
		}

		ILINE void SetMaximumExitSpeedSq(const float maximumSpeedSq)
		{
			prediction.samples[prediction.numSamples - 1].maximumSpeedSq = maximumSpeedSq;
		}

		ILINE float GetLength() const
		{
			return prediction.numSamples > 0 ? prediction.samples[prediction.numSamples - 1].distance : 0.0f;
		}

#if !defined(_RELEASE)
		bool IsValid() const
		{
			return
				moveTarget.IsValid() &&
				inflectionPoint.IsValid() &&
				spline.IsValid() &&
				::NumberValid(minimumTurnRadius) &&
				(minimumTurnRadius >= 0.0f) &&
				prediction.IsValid() &&
				simplifiedPrediction.IsValid() &&
				(simplifiedPrediction.numSamples <= prediction.numSamples);
		}
#endif
	};

	// ===========================================================================

	class CTakeCornerPlan : public IExecutingPlan
	{
	public:
		CTakeCornerPlan(const SState& state, const SShootOutData& data)
			: m_data(data)
			, m_startPosition(state.motion.position3D)
			, m_lastDistToTargetSq((state.motion.position2D - Vec2(m_data.moveTarget)).GetLength2())
			, m_lastAlpha(0.0f)
			, m_lastPositionOnSpline2D(m_data.spline.source2D)
		{
			m_lastOutput.desiredMovementDirection2D.zero();
			m_lastOutput.desiredSpeed = -1.0f;
		}


		bool IsValidFor(const SState& state) const override
		{
			CRY_ASSERT(state.hasMoveTarget);

			const float targetDifferenceSq = m_data.moveTarget.GetSquaredDistance(state.moveTarget);
			if (targetDifferenceSq > SHOOTOUT_MAX_TARGET_DIFFERENCE_SQ)
			{
				CORNERSMOOTHER_LOG("Plan Invalid: MoveTarget changed too much (%f > %f)", sqrtf(max(0.0f, targetDifferenceSq)), sqrtf(max(0.0f, SHOOTOUT_MAX_TARGET_DIFFERENCE_SQ)));
				return false;
			}

			if (state.hasInflectionPoint != m_data.hasInflectionPoint)
			{
				CORNERSMOOTHER_LOG("Plan Invalid: Inflection point status changed (%d != %d)", state.hasInflectionPoint, m_data.hasInflectionPoint);
				return false;
			}

			if (state.hasInflectionPoint)
			{
				const float inflectionPointDifferenceSq = m_data.inflectionPoint.GetSquaredDistance(state.inflectionPoint);
				if (inflectionPointDifferenceSq > SHOOTOUT_MAX_INFLECTIONPOINT_DIFFERENCE_SQ)
				{
					CORNERSMOOTHER_LOG("Plan Invalid: InflectionPoint changed too much (%f > %f)", sqrtf(max(0.0f, inflectionPointDifferenceSq)), sqrtf(max(0.0f, SHOOTOUT_MAX_INFLECTIONPOINT_DIFFERENCE_SQ)));
					return false;
				}
			}

			const float divergenceFromPath = (state.motion.position2D - m_lastPositionOnSpline2D).GetLength2();
			if (divergenceFromPath > TAKECORNER_MAX_DIVERGENCE_FROM_PATH_SQ)
			{
				CORNERSMOOTHER_LOG("Plan Invalid: Too far from path (%f > %f)", sqrtf(max(0.0f, divergenceFromPath)), TAKECORNER_MAX_DIVERGENCE_FROM_PATH);
				return false;
			}

			if (m_lastAlpha > 0.0f)
			{
				CRY_ASSERT(m_lastOutput.desiredSpeed >= 0.0f);
				const float cosAngle = m_lastOutput.desiredMovementDirection2D .Dot(state.motion.movementDirection2D);
				if (cosAngle < TAKECORNER_COS_MAX_MOVEMENTDIRECTION_ANGLE_DIVERGENCE)
				{
					CORNERSMOOTHER_LOG("Plan Invalid: Movement direction too different(%f > %f)", RAD2DEG(acosf(clamp_tpl(cosAngle, -1.0f, 1.0f))), RAD2DEG(TAKECORNER_MAX_MOVEMENTDIRECTION_ANGLE_DIVERGENCE));
					return false;
				}

				const float unsignedSpeedDifference = fabsf(m_lastOutput.desiredSpeed - state.motion.speed);
				if (unsignedSpeedDifference > TAKECORNER_MAX_SPEED_DIVERGENCE)
				{
					CORNERSMOOTHER_LOG("Plan Invalid: Speed too different(|%f - %f| = %f > %f)", m_lastOutput.desiredSpeed, state.motion.speed, unsignedSpeedDifference, TAKECORNER_MAX_SPEED_DIVERGENCE);
					return false;
				}
			}

			return true;
		}


		EPlanExecutionResult Execute(const SState& state, SOutput& output) override
		{
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
			//Debug::DrawSpline(m_data.spline, ColorF(0.0f,0.0f,1.0f, 1.0f), ColorF(0.0f,0.8f,1.0f, 1.0f), m_lastPositionOnSpline2D, m_lastPositionOnSpline2D, 0.002f, ColorF(0.9f,0.9f,0.2f, 1.0f));
#endif

			// Take a step along the spline

			float newAlpha;
			Vec2 newPositionOnSpline2D;
			bool reachedEnd = false;
			{
				const float distancePerFrame = max(state.motion.speed, 0.1f) * state.frameTime; // TODO: remove this max as speed should not be zero here

				EFindPointAlongSplineResult result = FindPointAlongSplineUsingBinarySearch(m_data.spline, m_lastAlpha, m_lastPositionOnSpline2D, square(distancePerFrame), square(distancePerFrame*1.02f), newAlpha, newPositionOnSpline2D);
				IF_UNLIKELY(result != eFPAS_Success)
				{
					IF_UNLIKELY(result == eFPAS_NotEnoughPrecision)
					{
						CORNERSMOOTHER_LOG("TakeCorner Execute Fail: Not enough precision for stepping along spline");
						return EPER_Failed;
					}

					IF_UNLIKELY(result == eFPAS_TooManySteps)
					{
						CORNERSMOOTHER_LOG("TakeCorner Execute Fail: Too many iterations needed for stepping along spline");
						return EPER_Failed;
					}

					CRY_ASSERT((result == eFPAS_AtEndOfSpline) || (result == eFPAS_BeyondEndOfSpline));
					reachedEnd = true;
				}
			}

			// Move towards the new position

			const Vec2 toTarget2D = newPositionOnSpline2D - state.motion.position2D; // TODO: rename target here and below
			const float distanceToTarget2DSq = toTarget2D.GetLength2();

			const bool wasMoving = state.motion.speed >= 0.1f;
			if (wasMoving)
			{
				if (distanceToTarget2DSq <= FLT_EPSILON)
				{
					// Rare special case: Somehow we moved about twice as much as we wanted to and
					// we already are where we want to be at the beginning of the next frame

					CRY_ASSERT(!reachedEnd); // if we reachedEnd already this should have been caught before. CCornerSmoother2::IsPlanNeeded() should have returned false as we are already at the target.

					// We could now try to update the current position on the spline and take a step from that position.
					// but that has its own gotchas - as this is very unlikely to happen we just Fail (which will be handled higher up)

					CORNERSMOOTHER_LOG("TakeCorner Execute Fail: We are already where we want to be next frame");
					return EPER_Failed;
				}

				const float distanceToTarget2D = sqrtf(distanceToTarget2DSq);
				const float distanceToTarget2DInv = 1.0f/distanceToTarget2D;
				const Vec2 directionToTarget2D = toTarget2D * distanceToTarget2DInv;

				//const float angleDifference = Ang3::CreateRadZ(state.motion.movementDirection2D, directionToTarget2D);
				//const float turnSpeed = angleDifference/state.frameTime; // epsilon check has been done in the CCornerSmoother2 update loop
				//const float unsignedTurnSpeed = fabsf(turnSpeed);
				//const float turnRadius = (unsignedTurnSpeed > FLT_EPSILON) ? state.motion.speed/unsignedTurnSpeed : FLT_MAX;

				const Vec2 newDesiredMovementDirection2D = directionToTarget2D;

				SSplineSample currentSample;
				SampleSplinePrediction(m_data.prediction, newAlpha, currentSample);
					
				const float maximumDesiredSpeed = state.desiredSpeed;
				const float maximumPhysicalSpeed = state.limits.speedRange.max;
				const float maximumReachableSpeed= state.motion.speed + state.frameTime * state.limits.maximumAcceleration;
				const float maximumSpeedToWalkSpline = sqrtf(currentSample.maximumSpeedSq);

				const float maximumSpeed = std::min(maximumDesiredSpeed, std::min(maximumPhysicalSpeed, std::min(maximumReachableSpeed, maximumSpeedToWalkSpline)));

				const float minimumDesiredSpeed = 0.0f; // TODO: Take the limits.speedRange.min when not stopping?
				const float minimumReachableSpeed = state.motion.speed - state.frameTime * state.limits.maximumDeceleration;

				const float minimumSpeed = std::min(std::max(minimumDesiredSpeed, minimumReachableSpeed), maximumSpeed);

				const float newDesiredSpeed = clamp_tpl(state.desiredSpeed, minimumSpeed, maximumSpeed);

				// --

				output.desiredSpeed = newDesiredSpeed; // TODO: smooth?
				output.desiredMovementDirection2D = newDesiredMovementDirection2D;

				CORNERSMOOTHER_LOG("TakeCorner Execute Success, speed %f adjusted to speed %f (min=%f, max=%f, splinemax=%f)", state.motion.speed, newDesiredSpeed, minimumSpeed, maximumSpeed, maximumSpeedToWalkSpline);
			}
			else
			{
				output.desiredSpeed = std::min(state.desiredSpeed, state.limits.speedRange.min); // TODO: smooth? take a larger speed when using MT?
				output.desiredMovementDirection2D = state.desiredMovementDirection2D;

				CORNERSMOOTHER_LOG("TakeCorner Execute Success - we are not moving, go to clamped desiredSpeed %f", output.desiredSpeed);
			}

			// Check whether it was a good step

			//const float distToTargetSq = (state.motion.position2D - Vec2(m_data.moveTarget)).GetLength2();
			//if (distToTargetSq >= m_lastDistToTargetSq)
			//{
			//	CORNERSMOOTHER_LOG("TakeCorner Execute Fail: Somehow we didn't get closer to our moveTarget (%f >= %f)", sqrtf(max(0.0f, distToTargetSq)), sqrtf(max(0.0f, m_lastDistToTargetSq)));
			////	return EPER_Failed;

			m_lastDistToTargetSq = distanceToTarget2DSq;
			m_lastAlpha = newAlpha;
			CRY_ASSERT(m_lastAlpha > 0.0f); // if this fails I need another variable to keep track of 'am I started yet'
			m_lastPositionOnSpline2D = newPositionOnSpline2D;
			m_lastOutput = output;

			return reachedEnd ? EPER_Finished : EPER_Continue;
		}


#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		void DebugDraw() const override
		{
			Debug::DrawPrediction(m_startPosition, m_data.prediction, ColorF(0.6f,0.6f,1,1));
			Debug::DrawPrediction(m_startPosition, m_data.simplifiedPrediction, ColorF(0.9f,0.9f,1,1), 0.15f, ColorF(0.9f, 0.0f, 0.0f, 1));
		}
#endif

	private:

		const SShootOutData m_data;
		const Vec3 m_startPosition;

		float m_lastDistToTargetSq;
		float m_lastAlpha;
		Vec2 m_lastPositionOnSpline2D;
		SOutput m_lastOutput; // only set when alpha>0 (assuming alpha always is >0 after the first step)
	};


	static void SimplifyShootOutPrediction(const Vec3& playerPos, const SSplinePrediction& input, SSimplifiedPrediction*const pOutput)
	{
		CRY_ASSERT(input.numSamples > 1);

		// cut corners using a simple forward-walking corner-cutting algorithm
		Vec2 lastIncludedPos = Vec2(playerPos);
		pOutput->numSamples = 0;
		int lastIncludedIndex = 0; // we begin at the playerPos, which is not included in the list
		while(lastIncludedIndex < input.numSamples)
		{
			int nextIncludedIndex = lastIncludedIndex + 1;
			if (nextIncludedIndex < input.numSamples)
			{
				while(nextIncludedIndex < input.numSamples - 1)
				{
					// try to advance to the next
					const int proposedNextIncludedIndex = nextIncludedIndex + 1;
					const Vec2 comparePos = input.samples[(lastIncludedIndex + proposedNextIncludedIndex)/2].pos; // instead of checking all points, as an approximation I just check the half-way point (which works for 'corner' curves)
					const Vec2 newPos = input.samples[proposedNextIncludedIndex].pos;
					const Vec2 delta = newPos - lastIncludedPos;
					const Lineseg ls = Lineseg(Vec3(lastIncludedPos), Vec3(newPos));
					float lambda;
					const float distSq = Distance::Point_Lineseg2DSq(Vec3(comparePos), ls, lambda);
					if (distSq > SHOOTOUT_PREDICTION_SIMPLIFICATION_THRESHOLD_DISTANCE_SQ)
						break;

					nextIncludedIndex = proposedNextIncludedIndex;
				}
			}

			if (nextIncludedIndex < input.numSamples)
			{
				pOutput->samples[pOutput->numSamples].pos = input.samples[nextIncludedIndex].pos;
				++pOutput->numSamples;
				lastIncludedPos = input.samples[nextIncludedIndex].pos;
			}
			lastIncludedIndex = nextIncludedIndex;
		}

		CRY_ASSERT(pOutput->numSamples > 0);
		CRY_ASSERT(pOutput->samples[pOutput->numSamples-1].pos == input.samples[input.numSamples-1].pos);
	}


	void AdjustMaximumSpeedBasedOnDeceleration(const float maximumDeceleration, SSplinePrediction& prediction)
	{
		CRY_ASSERT(prediction.IsValid());
		CRY_ASSERT(prediction.numSamples >= 2);
		CRY_ASSERT(maximumDeceleration >= 0.0f);

		// Go back to front and adjust maximumspeed downwards where needed

		// TODO: Simplify this logic, we can probably replace the nested for loops by 1 for loop

		for(int iLimitingSample = prediction.numSamples - 1; iLimitingSample >= 1; --iLimitingSample)
		{
			const SSplineSample& limitingSample = prediction.samples[iLimitingSample];

			// Adjust the speeds before the 'limiting sample' so we can decelerate to the iLimitingSample
			for(int i = iLimitingSample - 1; i >= 0; --i)
			{
				SSplineSample& sample = prediction.samples[i];

				const float distance = (limitingSample.distance - sample.distance);
				CRY_ASSERT(distance >= 0.0f);

				const float newMaxSpeedSq = CalculateMaximumSpeedAtDistanceBeforeSq(maximumDeceleration, limitingSample.maximumSpeedSq, distance);
				if (newMaxSpeedSq < sample.maximumSpeedSq)
				{
					CRY_ASSERT(newMaxSpeedSq >= 0.0f);
					sample.maximumSpeedSq = newMaxSpeedSq;
					--iLimitingSample; // this becomes the new limiting sample
				}
				else
					break; // as we are walking backwards through the samples in the outer loop, the outer loop will hit this sample and slow down to it for us
			}
		}
	}

	// Predict a shootout from the current position
	//
	// return true when it is within tolerances (prediction in data is filled in)
	// return false when it is invalid (contents of data is undefined)
	bool PredictShootOut(const SMotionState& beginMotion, const SMovementLimits& limits, SShootOutData& data)
	{
		CRY_ASSERT(SHOOTOUT_PREDICTION_MAX_SAMPLES >= 1);

		//CORNERSMOOTHER_LOG("PredictShootOut (target=[%f %f %f])", data.moveTarget.x, data.moveTarget.y, data.moveTarget.z);

		// some heuristic conservative measurement of the 'size' of the hermite
		// = 1/3rd of the tangents + the length from source-to-target
		// (based on the observation that the hermite tangents are 3x the side of a bezier control box
		// and a bezier control box encloses the spline)
		const float hermiteSize = (data.spline.sourceTangent2D.GetLength() + data.spline.targetTangent2D.GetLength()) * (1.f/3.f) + (data.spline.target2D - data.spline.source2D).GetLength();
		const int unclampedNumSteps = SHOOTOUT_PREDICTION_MIN_NUM_STEPS + int(SHOOTOUT_PREDICTION_NUM_STEPS_PER_HERMITE_METERS * hermiteSize);
		const int clampedNumSteps = std::min(SHOOTOUT_PREDICTION_MAX_SAMPLES-1, unclampedNumSteps);
		const float minStepSize = 1.0f/float(clampedNumSteps);

		// Fill in first Sample
		// (the maximumSpeed will be filled in during the first iteration)
		data.prediction.samples[0].alpha = 0.0f;
		data.prediction.samples[0].pos = beginMotion.position2D;
		data.prediction.samples[0].distance = 0.0f;

		bool fail = false;
		//float previousAlpha = 0.0f;
		Vec2 previousPosition2D = beginMotion.position2D;
		Vec2 previousMovementDirection2D = beginMotion.movementDirection2D;
		float previousDistanceToTarget2DSq = (Vec2(data.moveTarget) - beginMotion.position2D).GetLength2();
		float totalDistance = 0.0f;
		float currentAlpha = 0.0f;
		float minimumTurnRadius = FLT_MAX;
		bool atEnd = false;

		int i = 1;
		do
		{
			// ----------------------------------------------------------------------
			// Take a step along the spline of a certain minimum distance

			float stepDistanceAlongCurve = 0.0f;
			Vec2 newPosition2D;
			{
				Vec2 oldPosition2D = previousPosition2D;
				do
				{
					currentAlpha += minStepSize;
					if (currentAlpha >= 1.0f)
					{
						currentAlpha = 1.0f;
						atEnd = true;
					}

					EvaluateSplineAt(data.spline, currentAlpha, newPosition2D);

					stepDistanceAlongCurve += (newPosition2D - oldPosition2D).GetLength();
					oldPosition2D = newPosition2D;
				}
				while(!atEnd && (stepDistanceAlongCurve < SHOOTOUT_PREDICTION_MIN_STEP_DISTANCE_ALONG_CURVE));
			}

			// ----------------------------------------------------------------------
			// Stop when not getting closer to the target

			const Vec2 toTarget2D = Vec2(data.moveTarget) - newPosition2D;
			const float distanceToTarget2DSq = toTarget2D.GetLength2();
			if (!fail && !atEnd && (distanceToTarget2DSq >= previousDistanceToTarget2DSq)) // TODO: Do we really need the atEnd check?
			{
//#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
//				data.prediction.numSamples = i;
//				Debug::DrawPrediction(beginState.motion.position3D, data.prediction, ColorF(0.9f, 0.1f, 0.1f, 1.0f));
//#endif
				CORNERSMOOTHER_LOG("   Test Fail: not getting closer to the target (%2.2f >= %2.2f) after %d steps (atEnd=%d)", sqrtf(distanceToTarget2DSq), sqrtf(previousDistanceToTarget2DSq), i + 1, atEnd);
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
				fail = true; // TODO: Restore early-out
#else
				return false;
#endif
			}

			const Vec2 newMovement2D = newPosition2D - previousPosition2D;
			const float distance = newMovement2D.GetLength();
			const Vec2 newMovementDirection2D = newMovement2D.GetNormalized();
			const float unsignedAngleDifference = fabsf(Ang3::CreateRadZ(previousMovementDirection2D, newMovementDirection2D));

			// Ignore the last step if it's a small one, even if it has large turnradius
			const float turnRadius = (unsignedAngleDifference > FLT_EPSILON) ? stepDistanceAlongCurve/unsignedAngleDifference : FLT_MAX; // by using the distance along the curve we overestimate a bit, which helps near the beginning of the curve 

			minimumTurnRadius = std::min(turnRadius, minimumTurnRadius);

			// TODO: Early out when turnradius way too small?

			// Increase totalDistance after using it to find maximum speed
			totalDistance += distance;

			// Store Sample
			data.prediction.samples[i].alpha = currentAlpha;
			data.prediction.samples[i].pos = newPosition2D;
			data.prediction.samples[i].distance = totalDistance;
			data.prediction.samples[i-1].maximumSpeedSq = square(MaximumSpeedForTurnRadius(turnRadius));
			CRY_ASSERT(data.prediction.samples[i-1].maximumSpeedSq >= 0.0f);
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
			data.prediction.samples[i-1].turnRadius = turnRadius;
#endif

			previousPosition2D = newPosition2D;
			previousMovementDirection2D = newMovementDirection2D;
			previousDistanceToTarget2DSq = distanceToTarget2DSq;
			++i;

			if (atEnd)
			{
#if !INCLUDE_CORNERSMOOTHER_DEBUGGING()
				if (!fail)
#endif
				{
					// At the end, take another step of the same length as the last one in the 
					// direction of the tangent to figure out the turnradius & maximum speed
					CRY_ASSERT(i >= 2);

					const float finalSignedAngleDifference = Ang3::CreateRadZ(newMovementDirection2D, data.spline.targetTangent2D);
					const float finalUnsignedAngleDifference = fabsf(finalSignedAngleDifference);
					const float finalTurnRadius = (finalUnsignedAngleDifference > FLT_EPSILON) ? stepDistanceAlongCurve/finalUnsignedAngleDifference : FLT_MAX;

					data.prediction.samples[i-1].maximumSpeedSq = square(MaximumSpeedForTurnRadius(finalTurnRadius));
					CRY_ASSERT(data.prediction.samples[i-1].maximumSpeedSq >= 0.0f);
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
					data.prediction.samples[i-1].turnRadius = finalTurnRadius;
#endif
				}
			}
		}
		while(!atEnd && (i < SHOOTOUT_PREDICTION_MAX_SAMPLES));

		if (!atEnd)
		{
			CORNERSMOOTHER_LOG("   Test Fail: need to make too many steps (%d >= %d)", clampedNumSteps, SHOOTOUT_PREDICTION_MAX_SAMPLES);
			return false;
		}

		// ----------------------------------------------------------------------

		data.prediction.numSamples = i;
		data.minimumTurnRadius = minimumTurnRadius;

		if (!fail)
		{
			AdjustMaximumSpeedBasedOnDeceleration(limits.maximumDeceleration, data.prediction);
		}

		CRY_ASSERT(data.prediction.samples[i-1].alpha == 1.0f);
		CRY_ASSERT(data.prediction.samples[i-1].pos == data.spline.target2D);
		CRY_ASSERT(data.prediction.numSamples > 0);

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		if (Debug::s_debugEnabledDuringThisUpdate)
		{
			Debug::DrawPrediction(beginMotion.position3D, data.prediction, fail ? SHOOTOUT_PREDICTION_FAIL_COLOR : SHOOTOUT_PREDICTION_SUCCESS_COLOR);
		}
#endif

		return !fail;//TODO: true;
	}


	//////////////////////////////////////////////////////////////////////////
	static bool CheckWalkability(const IPathFollower* pPathFollower, const SSimplifiedPrediction& prediction)
	{
		if (prediction.numSamples == 1)
			return true; // if the path to the target is approximately a line we don't need to check walkability (supposing pathfollower did that for us)

		IF_UNLIKELY(!pPathFollower)
			return true;

		CRY_ASSERT(sizeof(prediction.samples[0]) == sizeof(Vec2));
		CRY_ASSERT((char*)&prediction.samples[1] - (char*)&prediction.samples[0] == sizeof(Vec2)); // check alignment

		return pPathFollower->CheckWalkability(reinterpret_cast<const Vec2*>(prediction.samples), prediction.numSamples);

		return false;
	}


	// 
	bool IsBetterShootOut(const SShootOutData& a, const SShootOutData& b) // a > b ?
	{
		if (a.GetMaximumEntrySpeedSq() > b.GetMaximumEntrySpeedSq())
			return true;

		if (a.GetMaximumEntrySpeedSq() < b.GetMaximumEntrySpeedSq())
			return false;

		return a.minimumTurnRadius > b.minimumTurnRadius;

		// TODO: what about the total length of the curve?
		//     maybe return a.GetLength() < b.GetLength();
		// TODO: what about the /changes/ in turnradius?
	}


	// Plan a shootout from the current position to the moveTarget
	//
	// Take into account the inflectionpoint to curve into the corner if possible
	// (this will force slowing down)
	bool PlanShootOutToMoveTarget(const SState& state, SShootOutData& outputShootOut)
	{
		CRY_ASSERT(state.IsValid());
		CRY_ASSERT(state.hasMoveTarget);

		SShootOutData bestShootOuts[TAKECORNER_MAX_SHOOTOUT_SPLINES];
		int numBestShootOuts = 0;

		const Vec2 toMoveTarget2D = Vec2(state.moveTarget) - state.motion.position2D;
		const Vec2 toMoveTargetDirection2D = toMoveTarget2D.GetNormalizedSafe(state.motion.movementDirection2D);
		const float distToMoveTarget = toMoveTarget2D.GetLength();

		const Vec2 mirroredArrivalDirection2D = MirrorVectorAroundUnitVector(state.motion.movementDirection2D, toMoveTargetDirection2D);

		bool isCorner = false;
		bool isEndOfPath = false;

		// only filled in when isCorner == true:
		Vec2 moveTargetToInflectionPoint2D;
		Vec2 moveTargetToInflectionPointDirection2D;

		if (state.hasInflectionPoint)
		{
			moveTargetToInflectionPoint2D = Vec2(state.inflectionPoint) - Vec2(state.moveTarget);
			isCorner = !moveTargetToInflectionPoint2D.IsZeroFast(); // smartpathfollower sets both MT and IP to the same value in the last section of the path
			isEndOfPath = !isCorner; // this is (should be) true in C2 & C3
		}

		if (isCorner)
		{
			moveTargetToInflectionPointDirection2D = moveTargetToInflectionPoint2D.GetNormalizedSafe(toMoveTargetDirection2D);
		}
		
		const float maxTangentLength = 3.0f * (distToMoveTarget * 0.5f + 0.1f);
		const float tangentMultiplierStep = maxTangentLength/float(TAKECORNER_MAX_SHOOTOUT_SPLINES);
		float tangentMultiplier = tangentMultiplierStep; // start with tight curves, then increase tangent sizes
		int numShootOuts = 0;
		do
		{
			SShootOutData& testShootOut = bestShootOuts[numBestShootOuts];

			testShootOut.moveTarget = state.moveTarget;
			testShootOut.hasInflectionPoint = state.hasInflectionPoint;
			if (state.hasInflectionPoint)
				testShootOut.inflectionPoint = state.inflectionPoint;
			else
				testShootOut.inflectionPoint.zero();

			testShootOut.spline.source2D = state.motion.position2D;
			testShootOut.spline.target2D = Vec2(state.moveTarget);

			testShootOut.spline.sourceTangent2D = state.motion.movementDirection2D * tangentMultiplier;

			if (isCorner)
			{
				testShootOut.spline.targetTangent2D = (toMoveTargetDirection2D + moveTargetToInflectionPointDirection2D * distToMoveTarget).GetNormalizedSafe(mirroredArrivalDirection2D) * tangentMultiplier;
			}
			else
			{
				testShootOut.spline.targetTangent2D = (mirroredArrivalDirection2D + toMoveTargetDirection2D).GetNormalized(); // TODO: test whether this should also have tangentMultiplier
			}

			//CORNERSMOOTHER_LOG("PredictShootOut (target=[%f %f %f])", data.target.x, data.target.y, data.target.z);
			const bool predictionSucceeded = PredictShootOut(state.motion, state.limits, testShootOut);
			if (predictionSucceeded)
			{
				CRY_ASSERT(testShootOut.IsValid());

				if ((numBestShootOuts == 0) || (IsBetterShootOut(testShootOut, bestShootOuts[numBestShootOuts - 1])))
				{
					++numBestShootOuts;

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
					CORNERSMOOTHER_LOG("Chose Spline: %2.2f,%2.2f (t=%2.2f,%2.2f) -> %2.2f,%2.2f (t=%2.2f,%2.2f)", testShootOut.spline.source2D.x, testShootOut.spline.source2D.y, testShootOut.spline.sourceTangent2D.x, testShootOut.spline.sourceTangent2D.y, testShootOut.spline.target2D.x, testShootOut.spline.target2D.y, testShootOut.spline.targetTangent2D.x, testShootOut.spline.targetTangent2D.y);
					if (Debug::s_debugEnabledDuringThisUpdate)
					{
						if (g_pGameCVars->ctrlr_corner_smoother_debug & 4)
						{
							Debug::DrawSpline(testShootOut.spline, ColorF(0.0f,0.0f,1.0f, 1.0f), ColorF(0.0f,0.8f,1.0f, 1.0f), Vec2(0,0), Vec2(0,0), 0.0f, ColorF(0.9f,0.9f,0.2f, 1.0f));
						}
					}
#endif
				}
			}

			tangentMultiplier += tangentMultiplierStep;
			++numShootOuts;
		}
		while(numShootOuts < TAKECORNER_MAX_SHOOTOUT_SPLINES);

		// Walk backwards through the array of 'best' shootouts and select the first one
		// that succeeds a walkability check

		bool weFoundBest = false;

		for(int i = numBestShootOuts - 1; i >= 0; --i)
		{
			SShootOutData& bestShootOut = bestShootOuts[i];

			SimplifyShootOutPrediction(state.motion.position2D, bestShootOut.prediction, &bestShootOut.simplifiedPrediction);

			const bool bPathIsValid = CheckWalkability(state.pPathFollower, bestShootOut.simplifiedPrediction);
			if (!bPathIsValid)
			{
				CORNERSMOOTHER_LOG("   CheckWalkability Fails");
			}
			else
			{
				CORNERSMOOTHER_LOG("   CheckWalkability Succeeds");
				outputShootOut = bestShootOut;
				weFoundBest = true;
				break;
			}
		}

		if (!weFoundBest)
		{
			// Fallback plan: construct a simple spline straight to the movetarget
			SShootOutData& testShootOut = bestShootOuts[0];

			testShootOut.spline.sourceTangent2D = toMoveTarget2D*0.5f;
			testShootOut.spline.targetTangent2D = toMoveTarget2D*0.5f;

			const bool success = PredictShootOut(state.motion, state.limits, testShootOut);
			if (success)
			{
				// No need to calculate a simple prediction, but at least we need to reset it so the debugging info will be accurate:
				testShootOut.simplifiedPrediction.Reset();

				outputShootOut = testShootOut;
				weFoundBest = true;
				CORNERSMOOTHER_LOG("Chose Fallback: Straight line to the MoveTarget");
			}
			else
			{
				CRY_ASSERT(false);
				CORNERSMOOTHER_LOG("Even the fallback failed, this should not happen");
			}
		}

		if (weFoundBest)
		{
			// Slow down at end of path
			if (isEndOfPath)
			{
				CRY_ASSERT(state.hasInflectionPoint);
				outputShootOut.SetMaximumExitSpeedSq(0.0f);
				AdjustMaximumSpeedBasedOnDeceleration(state.limits.maximumDeceleration, outputShootOut.prediction);
			}

			// Slow down before corner
			if (isCorner)
			{
				CRY_ASSERT(state.hasInflectionPoint);
				CRY_ASSERT(!(state.inflectionPoint - state.moveTarget).IsZero());

				const float distanceBehindMoveTarget = std::max(0.0f, state.distToPathEnd - distToMoveTarget);

				if (distanceBehindMoveTarget < SHOOTOUT_PREDICTION_MIN_PATH_DISTANCE_BEHIND_CORNER)
				{
					// TODO: Slow down because the end of path is right behind the corner!
					CORNERSMOOTHER_LOG("  Ignoring corner: distance beyond the corner is too small (%f)", distanceBehindMoveTarget);
				}
				else
				{
					SState cornerState = state;
					cornerState.motion.position3D = state.moveTarget;
					CRY_ASSERT(outputShootOut.spline.target2D == Vec2(state.moveTarget));
					cornerState.motion.position2D = outputShootOut.spline.target2D;
					cornerState.motion.speed = state.limits.speedRange.min;
					cornerState.motion.movementDirection2D = outputShootOut.spline.targetTangent2D.GetNormalized(); // the ideal angle
					//cornerState.desiredSpeed?
					cornerState.desiredMovementDirection2D = moveTargetToInflectionPointDirection2D;
					CRY_ASSERT(cornerState.hasMoveTarget == true);
					cornerState.moveTarget = state.inflectionPoint; 
					cornerState.hasInflectionPoint = false; // to make sure we do not recurse further
					cornerState.inflectionPoint.zero();
					cornerState.distToPathEnd = distanceBehindMoveTarget;

					CORNERSMOOTHER_LOG("Planning beyond the corner:");
					SShootOutData cornerOutput;
					const bool successAfterCorner = PlanShootOutToMoveTarget(cornerState, cornerOutput);
					if (!successAfterCorner)
					{
						// TODO: Something really bad happened here - maybe slow down?
						CORNERSMOOTHER_LOG("  Result: Fail: Found no way to take the corner");
					}
					else
					{
						CRY_ASSERT(cornerOutput.IsValid());

						if (cornerOutput.GetMaximumEntrySpeedSq() < outputShootOut.GetMaximumExitSpeedSq())
						{
							CRY_ASSERT(cornerOutput.GetMaximumEntrySpeedSq() >= 0.0f);

							// We need to slow down to enter this corner
							outputShootOut.SetMaximumExitSpeedSq(cornerOutput.GetMaximumEntrySpeedSq());
							AdjustMaximumSpeedBasedOnDeceleration(state.limits.maximumDeceleration, outputShootOut.prediction);
						}
					}
				}
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	// ==========================================================================

	static inline bool IsPlanNeeded(const SState& state)
	{
		if ((state.motion.speed <= MIN_SPEED_FOR_PLANNING) && (state.desiredSpeed <= MIN_DESIRED_SPEED_FOR_PLANNING))
		{
			CORNERSMOOTHER_LOG("No plan needed: not moving (speed = %f <= %f) and don't want to move (desire = %f <= %f)", state.motion.speed, MIN_SPEED_FOR_PLANNING, state.desiredSpeed, MIN_DESIRED_SPEED_FOR_PLANNING);
			return false;
		}

		if (!state.hasMoveTarget)
		{
			CORNERSMOOTHER_LOG("No plan needed: no movetarget");
			return false;
		}

		if (!state.hasInflectionPoint)
		{
			CORNERSMOOTHER_LOG("No plan needed: no inflectionpoint, probably ORCA");
			return false;
		}

		// Stop planning when within a couple of frames from the end of the path,
		// (this prevents overshooting at low frame rates)
		const float maxSpeedWeCanReachAtEndOfPath = state.limits.speedRange.max; // TODO: use acceleration to find correct value (if that's not too slow)
		const float safeDistanceToEnd = CalculateSafeDistanceToEnd(maxSpeedWeCanReachAtEndOfPath, state.frameTime);
		const bool outsideSafeDistance = state.distToPathEnd > safeDistanceToEnd;

		if (!outsideSafeDistance)
		{
			CORNERSMOOTHER_LOG("No plan needed: too close to end of path");
			return false;
		}

		return true;
	}


	static CTakeCornerPlan* BuildTakeCornerPlan(CTakeCornerPlan** ppBuffer, const SState& state, const SShootOutData& data)
	{
		if (!*ppBuffer)
		{
			*ppBuffer = new CTakeCornerPlan(state, data);
		}
		else
		{
			(*ppBuffer)->~CTakeCornerPlan();
			new (*ppBuffer) CTakeCornerPlan(state, data);
		}

		return *ppBuffer;
	}


	static CStopPlan* BuildStopPlan(CStopPlan** ppBuffer)
	{
		if (!*ppBuffer)
		{
			*ppBuffer = new CStopPlan();
		}
		else
		{
			(*ppBuffer)->~CStopPlan();
			new (*ppBuffer) CStopPlan();
		}

		return *ppBuffer;
	}


	// ==========================================================================

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
#define CORNERSMOOTHER_DEBUGDRAW(state, retval, outputShootOut) { if (Debug::s_debugEnabledDuringThisUpdate) DebugDraw(state, retval, outputShootOut); }
#else
#define CORNERSMOOTHER_DEBUGDRAW(state, retval, outputShootOut)
#endif


	CCornerSmoother2::CCornerSmoother2()
		: m_pCurrentPlan(NULL), m_pTakeCornerPlan(NULL), m_pStopPlan(NULL)
	{
	}


	CCornerSmoother2::~CCornerSmoother2()
	{
		SAFE_DELETE(m_pTakeCornerPlan);
		SAFE_DELETE(m_pStopPlan);
	}


	bool CCornerSmoother2::Update(const SState& state, SOutput& output)
	{
		CRY_ASSERT(state.IsValid());

#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		if (g_pGameCVars->ctrlr_corner_smoother_debug > 0)
		{
			const char* filter = g_pGameCVars->pl_debug_filter->GetString();
			const char* name = state.pActor->GetEntity()->GetName();
			Debug::s_debugEnabledDuringThisUpdate = !filter || !filter[0] || (strcmp(filter, "0") == 0) || (strcmp(filter, name) == 0);
		}
		else
		{
			Debug::s_debugEnabledDuringThisUpdate = false;
		}
#endif

		if (state.frameTime <= FLT_EPSILON)
		{
			CORNERSMOOTHER_DEBUGDRAW(state, false, output);
			return false;
		}

		if (!IsPlanNeeded(state))
		{
			CORNERSMOOTHER_LOG("Cleared plan: no plan is needed");
			m_pCurrentPlan = NULL;
			CORNERSMOOTHER_DEBUGDRAW(state, false, output);
			return false;
		}

		int numberOfPlansBuilt = 0;
		EPlanExecutionResult result;
		do 
		{
			const bool weHaveAValidPlan = (m_pCurrentPlan && m_pCurrentPlan->IsValidFor(state));
			if (!weHaveAValidPlan)
			{
				CORNERSMOOTHER_LOG("Creating new plan");
				CRY_ASSERT(state.hasMoveTarget);

				SShootOutData data;
				const bool shootOutSuccess = PlanShootOutToMoveTarget(state, data);
				if (shootOutSuccess)
				{
					m_pCurrentPlan = BuildTakeCornerPlan(&m_pTakeCornerPlan, state, data);
				}
				else
				{
					m_pCurrentPlan = BuildStopPlan(&m_pStopPlan);
				}

				numberOfPlansBuilt++;
				CRY_ASSERT(m_pCurrentPlan);
			}

			result = m_pCurrentPlan->Execute(state, output);
			switch(result)
			{
			case EPER_Failed:
				{
					CORNERSMOOTHER_LOG("Cleared plan: plan failed");
					m_pCurrentPlan = NULL;
					break;
				}
			case EPER_Continue:
				{
					break;
				}
			case EPER_Finished:
				{
					CORNERSMOOTHER_LOG("Cleared plan: plan is finished");
					m_pCurrentPlan = NULL;
					break;
				}
			default:
				CRY_ASSERT(false);
				return false;
			}
		}
		while((numberOfPlansBuilt == 0) && (result == EPER_Failed));

		if (result == EPER_Failed)
		{
			CORNERSMOOTHER_LOG("Gave up: All plans failed");
			m_pCurrentPlan = NULL;
			CORNERSMOOTHER_DEBUGDRAW(state, false, output);
			return false;
		}
		else
		{
			CORNERSMOOTHER_DEBUGDRAW(state, true, output);
			return true;
		}
	}


#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
	void CCornerSmoother2::DebugDraw(const SState& state, const bool hasOutput, const SOutput& calculatedOutput) const
	{
		CRY_ASSERT(Debug::s_debugEnabledDuringThisUpdate);

		const string name = state.pActor->GetEntity()->GetName();
		Debug::Begin(name);

		Debug::DrawState(state, hasOutput, calculatedOutput);

		if (g_pGameCVars->ctrlr_corner_smoother_debug & 2)
		{
			Debug::DrawLog();
		}

		if (m_pCurrentPlan)
		{
			m_pCurrentPlan->DebugDraw();
		}
	}
#endif

	// ==========================================================================

	void OnLevelUnload()
	{
#if INCLUDE_CORNERSMOOTHER_DEBUGGING()
		Debug::OnLevelUnload();
#endif
	}


	void SetLimitsFromMovementAbility(const struct AgentMovementAbility& movementAbility, const EStance stance, const float pseudoSpeed, struct SMovementLimits& output)
	{
		output.maximumAcceleration = movementAbility.maxAccel;
		output.maximumDeceleration = movementAbility.maxDecel;

		AgentMovementSpeeds::EAgentMovementStance agentStance;
		switch (stance)
		{
		case STANCE_STEALTH: agentStance = AgentMovementSpeeds::AMS_STEALTH; break;
		case STANCE_CROUCH: agentStance = AgentMovementSpeeds::AMS_CROUCH; break;
		case STANCE_PRONE: agentStance = AgentMovementSpeeds::AMS_PRONE; break;
		case STANCE_SWIM: agentStance = AgentMovementSpeeds::AMS_SWIM; break;
		case STANCE_RELAXED: agentStance = AgentMovementSpeeds::AMS_RELAXED; break;
		case STANCE_ALERTED: agentStance =  AgentMovementSpeeds::AMS_ALERTED; break;
		case STANCE_LOW_COVER: agentStance = AgentMovementSpeeds::AMS_LOW_COVER; break;
		case STANCE_HIGH_COVER: agentStance = AgentMovementSpeeds::AMS_HIGH_COVER; break;
		default: agentStance = AgentMovementSpeeds::AMS_COMBAT; break;
		};

		// TODO: Add support for slow? (not needed for C3)
		AgentMovementSpeeds::EAgentMovementUrgency urgency = AgentMovementSpeeds::AMU_WALK;
		if (pseudoSpeed >= 0.9f)
		{
			if (pseudoSpeed < 1.1f)
				urgency = AgentMovementSpeeds::AMU_RUN;
			else
				urgency = AgentMovementSpeeds::AMU_SPRINT;
		}

		output.speedRange = movementAbility.movementSpeeds.GetRange(agentStance, urgency);
	}
} // namespace CornerSmoothing
