// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Animations;

namespace CryEngine.Game
{
	public class PlayerAnimations
	{
		private const string RotationTagName = "rotate";
		private const string WalkTagName = "walk";

		private readonly float _minWalkSpeed = 0.2f;
		private readonly Player _player;
		private readonly CharacterAnimator _animator;
		private AnimationTag _rotationTag;
		private AnimationTag _walkTag;

		public PlayerAnimations(Player player, CharacterAnimator animator)
		{
			_player = player;
			_animator = animator;

			_rotationTag = animator.FindTag(RotationTagName);
			_walkTag = animator.FindTag(WalkTagName);
		}

		public void UpdateAnimationState(float frameTime, Quaternion cameraRotation)
		{
			// Start updating the motion parameters used for blend spaces

			var animator = _animator;
			var entity = _player.Entity;
			var physEntity = entity.Physics;

			if(physEntity == null)
			{
				Log.Warning<PlayerAnimations>("Entity is missing Physics!");
				return;
			}

			// Update entity rotation as the player turns
			// Start with getting the look orientation's yaw, pitch and roll
			var flatOrientation = cameraRotation;
			var ypr = flatOrientation.YawPitchRoll;

			// We only want to affect Z-axis rotation, zero pitch and roll
			ypr.Y = 0;
			ypr.Z = 0;

			// Re-calculate the quaternion based on the corrected look orientation
			flatOrientation.YawPitchRoll = ypr;

			// Get the player's velocity from physics
			var dynamicsStatus = physEntity.GetStatus<DynamicsStatus>();
			if(dynamicsStatus == null)
			{
				Log.Warning<PlayerAnimations>("Unable to retrieve DynamicsStatus from Entity!");
				return;
			}

			var forward = entity.Forward;
			var flatCameraForward = flatOrientation.Forward;

			// Set turn rate as the difference between previous and new entity rotation
			var turnAngle = Vector3.FlatAngle(forward, flatCameraForward);
			var travelAngle = Vector3.FlatAngle(forward, dynamicsStatus.Velocity);
			var travelSpeed = dynamicsStatus.Velocity.Length2D;

			// The calculated turnAngle is the angle difference of one frame, but we need it as an angle per second value.
			// But if the frameTime is 0 (because the game is paused for example) we don't want to divide by zero.
			turnAngle = frameTime > 0.0f ? turnAngle / frameTime : 0.0f;

			// Set the travel speed based on the physics velocity magnitude
			// Keep in mind that the maximum number for motion parameters is 10.
			// If your velocity can reach a magnitude higher than this, divide by the maximum theoretical account and work with a 0 - 1 ratio.
			animator.SetMotionParameter(MotionParameterId.TravelSpeed, travelSpeed);

			// Update the turn speed in CryAnimation, note that the maximum motion parameter (10) applies here too.
			animator.SetMotionParameter(MotionParameterId.TurnAngle, turnAngle);
			animator.SetMotionParameter(MotionParameterId.TravelAngle, travelAngle);

			var livingStatus = physEntity.GetStatus<LivingStatus>();
			if(!livingStatus.IsFlying)
			{
				// Calculate slope value
				var groundNormal = livingStatus.GroundSlope * flatOrientation;

				groundNormal.X = 0.0f;
				var cosine = Vector3.Dot(Vector3.Up, groundNormal);
				var sine = Vector3.Cross(Vector3.Up, groundNormal);

				var travelSlope = (float)Math.Atan2(Math.Sign(sine.X) * sine.Magnitude, cosine);

				animator.SetMotionParameter(MotionParameterId.TravelSlope, travelSlope);
			}

			var isRotating = Math.Abs(turnAngle) > 0.0f;
			var isWalking = travelSpeed > _minWalkSpeed && !livingStatus.IsFlying;

			// Update the Animator tags
			animator.SetTagValue(_rotationTag, isRotating);
			animator.SetTagValue(_walkTag, isWalking);

			// Send updated rotation to the entity.
			entity.Rotation = flatOrientation;
		}
	}
}

