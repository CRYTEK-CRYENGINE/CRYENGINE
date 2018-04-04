namespace CryEngine.Game
{
	public class Player : EntityComponent
	{
		public enum GeometrySlots
		{
			ThirdPerson = 0
		}

		private const string PlayerGeometryUrl = "Objects/Default/primitive_sphere.cgf";
		private const string PlayerMaterialUrl = "EngineAssets/TextureMsg/DefaultSolids";

		private float _mass = 90.0f;

		/// <summary>
		/// Mass of the player entity in kg.
		/// </summary>
		/// <value>The mass.</value>
		[EntityProperty(0, "Mass of the player entity in kg.")]
		public float Mass
		{
			get
			{
				return _mass;
			}
			set
			{
				_mass = value;
				PrepareRigidbody();
			}
		}

		/// <summary>
		/// Strength of the per-frame impulse when holding inputs
		/// </summary>
		/// <value>The move impulse strength.</value>
		[EntityProperty(0, "Strength of the per-frame impulse when holding inputs")]
		public float MoveImpulseStrength{ get; set; } = 800.0f;

		/// <summary>
		/// Speed at which the player rotates entity yaw
		/// </summary>
		/// <value>The rotation speed yaw.</value>
		[EntityProperty(0, "Speed at which the player rotates entity yaw")]
		public float RotationSpeedYaw{ get; set; } = 0.002f;

		/// <summary>
		/// Speed at which the player rotates entity pitch
		/// </summary>
		/// <value>The rotation speed pitch.</value>
		[EntityProperty(0, "Speed at which the player rotates entity pitch")]
		public float RotationSpeedPitch{ get; set; } = 0.002f;

		/// <summary>
		/// Minimum entity pitch limit
		/// </summary>
		/// <value>The rotation limits minimum pitch.</value>
		[EntityProperty(0, "Minimum entity pitch limit")]
		public float RotationLimitsMinPitch{ get; set; } = -0.84f;

		/// <summary>
		/// Maximum entity pitch limit
		/// </summary>
		/// <value>The rotation limits max pitch.</value>
		[EntityProperty(0, "Maximum entity pitch limit")]
		public float RotationLimitsMaxPitch{ get; set; } = 1.5f;

		/// <summary>
		/// Determines the distance between player and camera
		/// </summary>
		/// <value>The view distance.</value>
		[EntityProperty(0, "Determines the distance between player and camera")]
		public float ViewDistance{ get; set; } = 10.0f;

		private PlayerView _playerView;

		protected override void OnInitialize()
		{
			base.OnInitialize();

			Initialize();
		}

		protected override void OnGameplayStart()
		{
			base.OnGameplayStart();

			// It is possible that there were changes made during the last time the gamemode was started and now.
			// To prevent any weird behaviour we initialize again just in case.
			Initialize();
		}

		private void Initialize()
		{
			// Prepare the visuals of the player
			SetPlayerModel();

			// Now create the physical representation of the entity
			PrepareRigidbody();

			_playerView = new PlayerView(this);
		}

		protected override void OnUpdate(float frameTime)
		{
			base.OnUpdate(frameTime);

			UpdateMovement(frameTime);

			_playerView.UpdateView(frameTime);
		}

		private void UpdateMovement(float frameTime)
		{
			var entity = Entity;
			var physicalEntity = entity.Physics;
			if(physicalEntity == null)
			{
				return;
			}
				
			var moveForward = Input.KeyDown(KeyId.W);
			var moveBack = Input.KeyDown(KeyId.S);
			var moveLeft = Input.KeyDown(KeyId.A);
			var moveRight = Input.KeyDown(KeyId.D);

			//We need to transform the input-direction to camera-space, so we use the forward of the camera to calculate the proper direction.
			var forward = Camera.ForwardDirection;

			//Cross the forward of the camera with Vector3.up, so we get the right-direction of the camera.
			//The forward is flatten on the z-axis, so we don't have to worry about the sphere being pushed up or down.
			var flatForward = forward;
			flatForward.Z = 0.0f;
			flatForward = flatForward.Normalized;
			var right = flatForward.Cross(Vector3.Up);

			Vector3 impulse = Vector3.Zero;

			// Go through the inputs and apply an impulse corresponding to view rotation
			if(moveForward)
			{
				impulse += flatForward * MoveImpulseStrength;
			}
			if(moveBack)
			{
				impulse -= flatForward * MoveImpulseStrength;
			}
			if(moveLeft)
			{
				impulse -= right * MoveImpulseStrength;
			}
			if(moveRight)
			{
				impulse += right * MoveImpulseStrength;
			}

			// Only dispatch the impulse to physics if one was provided
			if(impulse.LengthSquared > 0.0f)
			{
				// Multiply by frame time to keep consistent across machines
				impulse *= frameTime;

				physicalEntity.AddImpulse(impulse);
			}
		}

		private void SetPlayerModel()
		{
			var entity = Entity;

			// Load the third person model
			entity.LoadGeometry((int)GeometrySlots.ThirdPerson, PlayerGeometryUrl);

			// Override material so that we can see the ball rolling more easily
			entity.LoadMaterial(PlayerMaterialUrl);
		}

		private void PrepareRigidbody()
		{
			var physicsEntity = Entity.Physics;
			if(physicsEntity == null)
			{
				return;
			}

			// Physicalize the player as type Living.
			// This physical entity type is specifically implemented for players
			physicsEntity.Physicalize(Mass, PhysicalizationType.Rigid);
		}
	}
}

