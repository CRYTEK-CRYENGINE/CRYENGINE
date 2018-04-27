// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// CDeferredEvaluator_TestRaycast
		//
		// - Tests a raycast between 2 given positions.
		// - Whether success or failure of the raycast counts as overall success or failure of the evalutor can be specified by a parameter.
		//
		// - NOTICE: The underlying raycaster uses global timer to limit the number of raycasts per second!
		//
		//===================================================================================

		class CDeferredEvaluator_TestRaycast : public Client::IDeferredEvaluator
		{
		public:
			struct SParams
			{
				Pos3                   from;
				Pos3                   to;
				bool                   raycastShallSucceed;

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("from", from, "FROM", "Start position of the raycast");
					UQS_EXPOSE_PARAM("to", to, "TO  ", "End position of the raycast");
					UQS_EXPOSE_PARAM("raycastShallSucceed", raycastShallSucceed, "SUCC", "Whether the raycast shall succeed or fail in order for the whole evaluator to accept or discard the item");
				UQS_EXPOSE_PARAMS_END
			};

		private:
			class CRaycastRegulator
			{
			public:
				explicit               CRaycastRegulator(int maxRequestsPerSecond);
				bool                   RequestRaycast();

			private:
				const int              m_maxRequestsPerSecond;
				float                  m_timeLastFiredRaycast;
			};

		private:
			const SParams              m_params;
			static CRaycastRegulator   s_regulator;

		public:
			explicit                   CDeferredEvaluator_TestRaycast(const SParams& params);
			virtual EUpdateStatus      Update(const SUpdateContext& updateContext) override;
		};

	}
}
