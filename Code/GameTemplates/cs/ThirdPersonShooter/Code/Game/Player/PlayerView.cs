// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;

namespace CryEngine.Game
{
	public class PlayerView
	{
		private readonly Player _player;
		private readonly Entity _cameraPivot;
		private float _aimProgress = 0;
		private ViewCamera _camera;

		public PlayerView(Player player)
		{
			if(player == null)
			{
				throw new ArgumentNullException(nameof(player));
			}
			_player = player;
			_cameraPivot = Entity.Spawn("CameraPivot", Vector3.Zero, Quaternion.Identity, Vector3.One);
			_camera = Entity.SpawnWithComponent<ViewCamera>("Player Camera", Vector3.Zero, Quaternion.Identity, Vector3.One);
		}

		public Quaternion UpdateView(float frameTime, Vector2 rotationDelta)
		{
			_cameraPivot.Position = _player.Entity.Position;
			Quaternion rotation = _cameraPivot.Rotation;
			float yawSpeed = _player.RotationSpeedYaw;
			float pitchSpeed = _player.RotationSpeedPitch;
			float pitchMin = _player.RotationLimitsMinPitch;
			float pitchMax = _player.RotationLimitsMaxPitch;
			float eyeHeight = _player.EyeHeight;

			//Invert the rotation to have proper third-person camera-control.
			rotationDelta = -rotationDelta;

			var ypr = rotation.YawPitchRoll;

			ypr.X += rotationDelta.X * yawSpeed;

			float pitchDelta = rotationDelta.Y * pitchSpeed;
			ypr.Y = MathHelpers.Clamp(ypr.Y + pitchDelta, pitchMin, pitchMax);

			ypr.Z = 0;

			rotation.YawPitchRoll = ypr;
			_cameraPivot.Rotation = rotation;

			Vector3 aimOffset;
			float viewDistance;
			UpdateAimingPosition(frameTime, out aimOffset, out viewDistance);

			var entity = _cameraPivot;
			var forward = entity.Forward;

			//Tranform the offset to the players orientation.
			aimOffset = entity.WorldRotation * aimOffset;

			//By default the camera will look at the player's feet, so we set the focus point a bit higher.
			var position = entity.WorldPosition + aimOffset + Vector3.Up * eyeHeight;
			position -= forward * viewDistance;

			_camera.Entity.Position = position;
			_camera.Entity.Rotation = rotation;
			return rotation;
		}

		private void UpdateAimingPosition(float frameTime, out Vector3 aimOffset, out float viewDistance)
		{
			aimOffset = Vector3.Zero;
			viewDistance = _player.CameraDistanceOffset;

			var aimSpeed = _player.AimSpeed;
			if(aimSpeed > 0.0f)
			{
				aimSpeed = 1.0f / aimSpeed;
			}
			else
			{
				//This makes the aim instant.
				aimSpeed = 100000f;
			}

			if(_player.IsAiming)
			{
				_aimProgress += aimSpeed * frameTime;
				_aimProgress = MathHelpers.Clamp01(_aimProgress);
			}
			else
			{
				_aimProgress -= aimSpeed * frameTime;
				_aimProgress = MathHelpers.Clamp01(_aimProgress);
			}

			aimOffset += Vector3.Lerp(Vector3.Zero, Vector3.Right * _player.AimingCameraHorizontalOffset, _aimProgress);
			viewDistance = MathHelpers.Lerp(_player.CameraDistanceOffset, _player.AimingCameraDistanceOffset, _aimProgress);
		}

		public void Deinitialize()
		{
			if(_cameraPivot != null)
			{
				_cameraPivot.Remove();
			}
		}
	}
}

