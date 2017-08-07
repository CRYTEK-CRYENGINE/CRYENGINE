// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Game.Weapons;

namespace CryEngine.Game
{
	public class Player : EntityComponent
	{
		public enum GeometrySlots
		{
			ThirdPerson = 0
		}

		private const string PlayerGeometryUrl = "Objects/Characters/SampleCharacter/thirdperson.cdf";
		private const string InputActionMapUrl = "Libs/config/defaultprofile.xml";
		private const string InputActionMapName = "player";

		private float _mass = 90.0f;
		private float _eyeHeight = 0.935f;
		private float _airResistance = 0.0f;

		public BaseWeapon Weapon { get; private set; }
		public bool IsAiming { get; private set; }

		/// <summary>
		/// Mass of the player entity in kg.
		/// </summary>
		[EntityProperty(EntityPropertyType.Primitive, "Mass of the player entity in kg.")]
		public float Mass
		{
			get
			{
				return _mass;
			}
			set
			{
				_mass = value;
				Physicalize();
			}
		}

		/// <summary>
		/// The air-resistance of the player. Higher air-resistance will make the player float when falling down.
		/// </summary>
		[EntityProperty(EntityPropertyType.Primitive, "Air resistance of the player entity. Higher air-resistance will make the player float when falling down.")]
		public float AirResistance
		{
			get
			{
				return _airResistance;
			}
			set
			{
				_airResistance = value;
				Physicalize();
			}
		}

		/// <summary>
		/// The eye-height of the player.
		/// </summary>
		[EntityProperty(EntityPropertyType.Primitive, "The eye-height of the player.")]
		public float EyeHeight
		{
			get
			{
				return _eyeHeight;
			}
			set
			{
				_eyeHeight = value;
				Physicalize();
			}
		}

		/// <summary>
		/// Strength of the per-frame impulse when holding inputs
		/// </summary>
		/// <value>The move impulse strength.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Speed of the player in meters per second.")]
		public float MoveSpeed { get; set; } = 20.5f;

		/// <summary>
		/// Speed at which the player rotates entity yaw
		/// </summary>
		/// <value>The rotation speed yaw.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Speed at which the player rotates entity yaw")]
		public float RotationSpeedYaw { get; set; } = 0.002f;

		/// <summary>
		/// Speed at which the player rotates entity pitch
		/// </summary>
		/// <value>The rotation speed pitch.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Speed at which the player rotates entity pitch")]
		public float RotationSpeedPitch { get; set; } = 0.002f;

		/// <summary>
		/// Minimum entity pitch limit
		/// </summary>
		/// <value>The rotation limits minimum pitch.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Minimum entity pitch limit")]
		public float RotationLimitsMinPitch { get; set; } = -0.84f;

		/// <summary>
		/// Maximum entity pitch limit
		/// </summary>
		/// <value>The rotation limits max pitch.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Maximum entity pitch limit")]
		public float RotationLimitsMaxPitch { get; set; } = 1.5f;

		/// <summary>
		/// Determines the distance between player and camera.
		/// </summary>
		/// <value>The view distance.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Determines the distance between player and camera.")]
		public float CameraDistanceOffset { get; set; } = 1.5f;

		/// <summary>
		/// Determines the height offset of the camera from the player's pivot position.
		/// </summary>
		/// <value>The view distance.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Determines the height offset of the camera from the player's pivot position.")]
		public float CameraHeightOffset { get; set; } = 2f;

		/// <summary>
		/// Determines the distance between player and camera while aiming the weapon.
		/// </summary>
		/// <value>The view distance while aiming.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Determines the distance between player and camera while aiming the weapon.")]
		public float AimingCameraDistanceOffset { get; set; } = 0.5f;

		/// <summary>
		/// Determines the distance between player and camera while aiming the weapon.
		/// </summary>
		/// <value>The view distance while aiming.</value>
		[EntityProperty(EntityPropertyType.Primitive, "Determines the distance between player and camera while aiming the weapon.")]
		public float AimingCameraHorizontalOffset { get; set; } = 0.5f;

		/// <summary>
		/// The speed at which the player can switch to aiming the weapon in seconds.
		/// </summary>
		/// <value>The aim speed.</value>
		[EntityProperty(EntityPropertyType.Primitive, "The speed at which the player can switch to aiming the weapon in seconds.")]
		public float AimSpeed { get; set; } = 0.15f;

		/// <summary>
		/// Inverses the vertical rotation of the camera.
		/// </summary>
		[EntityProperty(EntityPropertyType.Primitive, "Inverses the vertical rotation of the camera.")]
		public bool InverseVerticalRotation { get; set; } = false;

		private PlayerView _playerView;
		private PlayerAnimations _animations;

		private ActionHandler _actionHandler;
		private Vector2 _movement = Vector2.Zero;
		private Vector2 _rotationMovement = Vector2.Zero;
		private CharacterAnimator _animator;

		protected override void OnInitialize()
		{
			base.OnInitialize();

			//By setting the model and physics here already, we can get a visual representation of the player in the sandbox outside of game-mode.
			SetPlayerModel();
			PrepareRigidbody();
		}

		protected override void OnGameplayStart()
		{
			base.OnGameplayStart();

			//Add the required components first.
			_animator = Entity.GetOrCreateComponent<CharacterAnimator>();

			//Prepare the visuals of the player
			SetPlayerModel();

			//Set up the player animations, but only if the game is being played.
			_animations = new PlayerAnimations(this, _animator);

			// Now create the physical representation of the entity
			PrepareRigidbody();

			_playerView = new PlayerView(this);

			InitializeInput();

			PrepareWeapon();
		}

		protected override void OnEditorGameModeChange(bool enterGame)
		{
			base.OnEditorGameModeChange(enterGame);

			if(!enterGame)
			{
				_playerView?.Deinitialize();
				_playerView = null;
			}
		}

		private void InitializeInput()
		{
			_actionHandler?.Dispose();
			_actionHandler = new ActionHandler(InputActionMapUrl, InputActionMapName);

			//Movement
			_actionHandler.AddHandler("moveforward", OnMoveForward);
			_actionHandler.AddHandler("moveback", OnMoveBack);
			_actionHandler.AddHandler("moveright", OnMoveRight);
			_actionHandler.AddHandler("moveleft", OnMoveLeft);

			//Mouse movement
			_actionHandler.AddHandler("mouse_rotateyaw", OnMoveMouseX);
			_actionHandler.AddHandler("mouse_rotatepitch", OnMoveMouseY);

			//Actions
			_actionHandler.AddHandler("shoot", OnFireWeaponPressed);
			_actionHandler.AddHandler("aim", OnAimWeaponPressed);
		}

		private void OnMoveForward(string name, InputState state, float value)
		{
			if(state == InputState.Pressed)
			{
				_movement.Y = 1.0f;
			}
			else if(state == InputState.Released)
			{
				//The movement only needs to be stopped when the player is still moving forward.
				if(_movement.Y > 0.0f)
				{
					_movement.Y = 0.0f;
				}
			}
		}

		private void OnMoveBack(string name, InputState state, float value)
		{
			if(state == InputState.Pressed)
			{
				_movement.Y = -1.0f;
			}
			else if(state == InputState.Released)
			{
				//The movement only needs to be stopped when the player is still moving back.
				if(_movement.Y < 0.0f)
				{
					_movement.Y = 0.0f;
				}
			}
		}

		private void OnMoveRight(string name, InputState state, float value)
		{
			if(state == InputState.Pressed)
			{
				_movement.X = 1.0f;
			}
			else if(state == InputState.Released)
			{
				//The movement only needs to be stopped when the player is still moving right.
				if(_movement.X > 0.0f)
				{
					_movement.X = 0.0f;
				}
			}
		}

		private void OnMoveLeft(string name, InputState state, float value)
		{
			if(state == InputState.Pressed)
			{
				_movement.X = -1.0f;
			}
			else if(state == InputState.Released)
			{
				//The movement only needs to be stopped when the player is still moving left.
				if(_movement.X < 0.0f)
				{
					_movement.X = 0.0f;
				}
			}
		}

		private void OnMoveMouseX(string name, InputState state, float value)
		{
			//If for some reason another state than Changed is received, it will be ignored.
			if(state != InputState.Changed)
			{
				return;
			}

			_rotationMovement.X += value;
		}

		private void OnMoveMouseY(string name, InputState state, float value)
		{
			//If for some reason another state than Changed is received, it will be ignored.
			if(state != InputState.Changed)
			{
				return;
			}

			if(InverseVerticalRotation)
			{
				value = -value;
			}

			_rotationMovement.Y += value;
		}

		private void OnFireWeaponPressed(string name, InputState state, float value)
		{
			if(state != InputState.Pressed)
			{
				return;
			}

			var character = Entity.GetCharacter((int)GeometrySlots.ThirdPerson);
			var weapon = Weapon;

			if(weapon == null || character == null)
			{
				return;
			}

			var barrelOutAttachment = character.GetAttachment(weapon.OutAttachementName);

			if(barrelOutAttachment == null)
			{
				return;
			}


			Vector3 position = barrelOutAttachment.WorldPosition;
			Quaternion rotation = barrelOutAttachment.WorldRotation;
			Weapon?.RequestFire(position, rotation);
		}

		private void OnAimWeaponPressed(string name, InputState state, float value)
		{
			switch(state)
			{
			case InputState.Down:
			case InputState.Pressed:
				IsAiming = true;
				break;
			case InputState.Released:
				IsAiming = false;
				break;
			default:
				return;
			}
		}

		protected override void OnUpdate(float frameTime)
		{
			base.OnUpdate(frameTime);

			var cameraRotation = _playerView == null ? Camera.Rotation : _playerView.UpdateView(frameTime, _rotationMovement);
			_rotationMovement = Vector2.Zero;
			_animations?.UpdateAnimationState(frameTime, cameraRotation);

			UpdateMovement(frameTime);
		}

		private void UpdateMovement(float frameTime)
		{
			var entity = Entity;
			var physicalEntity = entity.Physics;
			if(physicalEntity == null)
			{
				return;
			}

			var movement = new Vector3(_movement);

			//Transform the movement to camera-space
			movement = Camera.TransformDirection(movement);

			//Transforming it could have caused the movement to point up or down, so we flatten the z-axis to remove the height.
			movement.Z = 0.0f;
			movement = movement.Normalized;

			// Only dispatch the impulse to physics if one was provided
			if(movement.LengthSquared > 0.0f)
			{
				var status = physicalEntity.GetStatus<LivingStatus>();
				if(status.IsFlying)
				{
					//If we're not touching the ground we're not going to send any more move actions.
					return;
				}

				// Multiply by frame time to keep consistent across machines
				movement *= MoveSpeed * frameTime;

				physicalEntity.Move(movement);
			}
		}

		private void SetPlayerModel()
		{
			var entity = Entity;

			// Load the third person model
			entity.LoadCharacter((int)GeometrySlots.ThirdPerson, PlayerGeometryUrl);
		}

		private void PrepareWeapon()
		{
			Weapon = new Rifle();
		}

		private void PrepareRigidbody()
		{
			var physicsEntity = Entity.Physics;
			if(physicsEntity == null)
			{
				return;
			}

			// Create the physical representation of the entity
			Physicalize();
		}

		private void Physicalize()
		{
			// Physicalize the player as type Living.
			// This physical entity type is specifically implemented for players
			var parameters = new LivingPhysicalizeParams();

			//The player will have settings for the player dimensions and dynamics.
			var playerDimensions = parameters.PlayerDimensions;
			var playerDynamics = parameters.PlayerDynamics;

			parameters.Mass = Mass;

			// Prefer usage of a capsule instead of a cylinder
			playerDimensions.UseCapsule = true;

			// Specify the size of our capsule
			playerDimensions.ColliderSize = new Vector3(0.45f, 0.45f, EyeHeight * 0.25f);

			// Keep pivot at the player's feet (defined in player geometry) 
			playerDimensions.PivotHeight = 0.0f;

			// Offset collider upwards
			playerDimensions.ColliderHeight = 1.0f;
			playerDimensions.GroundContactEpsilon = 0.004f;

			playerDynamics.AirControlCoefficient = 0.0f;
			playerDynamics.AirResistance = AirResistance;
			playerDynamics.Mass = Mass;

			Entity.Physics.Physicalize(parameters);
		}
	}
}

