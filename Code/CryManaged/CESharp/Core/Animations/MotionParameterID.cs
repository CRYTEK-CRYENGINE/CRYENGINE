// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace CryEngine.Animations
{
	//Based on the EMotionParamID from c++.

	/// <summary>
	/// ID-enum for motion parameters.
	/// </summary>
	public enum MotionParameterId
	{
		/// <summary>
		/// The speed at which the animated-entity is traveling.
		/// </summary>
		TravelSpeed,
		/// <summary>
		/// The speed at which the animated-entity is turning.
		/// </summary>
		TurnSpeed,
		/// <summary>
		/// The angle of the traveling-direction of the animated-entity. Used to calculate forward, backwards or sidestepping movement.
		/// </summary>
		TravelAngle,
		/// <summary>
		/// The vertical angle of the direction the animated-entity is traveling in.
		/// </summary>
		TravelSlope,
		/// <summary>
		/// Rotation angle for Idle2Move and Idle animations.
		/// </summary>
		TurnAngle,
		/// <summary>
		/// Idle-steps.
		/// </summary>
		TravelDist,
		/// <summary>
		/// Move2Idle.
		/// </summary>
		StopLeg,
		/// <summary>
		/// Custom blend weight 1
		/// </summary>
		BlendWeight1,
		/// <summary>
		/// Custom blend weight 2
		/// </summary>
		BlendWeight2,
		/// <summary>
		/// Custom blend weight 3
		/// </summary>
		BlendWeight3,
		/// <summary>
		/// Custom blend weight 4
		/// </summary>
		BlendWeight4,
		/// <summary>
		/// Custom blend weight 5
		/// </summary>
		BlendWeight5,
		/// <summary>
		/// Custom blend weight 6
		/// </summary>
		BlendWeight6,
		/// <summary>
		/// Custom blend weight 7
		/// </summary>
		BlendWeight7,
	}
}
