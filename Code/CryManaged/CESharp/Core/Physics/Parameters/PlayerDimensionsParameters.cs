using CryEngine.Common;

namespace CryEngine
{
	public class PlayerDimensionsParameters : BasePhysicsParameters<pe_player_dimensions>
	{
		private readonly pe_player_dimensions _parameters = new pe_player_dimensions();

		/// <summary>
		/// Offset from central ground position that is considered entity center
		/// </summary>
		public float PivotHeight
		{
			get { return _parameters.heightPivot; }
			set { _parameters.heightPivot = value; }
		}

		/// <summary>
		/// Vertical offset of camera
		/// </summary>
		public float EyeHeight
		{
			get { return _parameters.heightEye; }
			set { _parameters.heightEye = value; }
		}

		/// <summary>
		/// Collision cylinder dimensions. X is radius, Z is half-height, Y is unused.
		/// </summary>
		public Vector3 ColliderSize
		{
			get { return _parameters.sizeCollider; }
			set { _parameters.sizeCollider = value; }
		}

		/// <summary>
		/// Vertical offset of collision geometry center
		/// </summary>
		public float ColliderHeight
		{
			get { return _parameters.heightCollider; }
			set { _parameters.heightCollider = value; }
		}

		/// <summary>
		/// Radius of the 'head' geometry (used for camera offset)
		/// </summary>
		public float HeadRadius
		{
			get { return _parameters.headRadius; }
			set { _parameters.headRadius = value; }
		}

		/// <summary>
		/// Center.z of the head geometry
		/// </summary>
		public float HeadHeight
		{
			get { return _parameters.heightHead; }
			set { _parameters.heightHead = value; }
		}

		/// <summary>
		/// Unprojection direction to test in case the new position overlaps with the environment (can be 0 for 'auto')
		/// </summary>
		public Vector3 UnprojectionDirection
		{
			get { return _parameters.dirUnproj; }
			set { _parameters.dirUnproj = value; }
		}

		/// <summary>
		/// Maximum allowed unprojection
		/// </summary>
		public float MaxUnprojection
		{
			get { return _parameters.maxUnproj; }
			set { _parameters.maxUnproj = value; }
		}

		/// <summary>
		/// Switches between capsule and cylinder collider geometry
		/// </summary>
		public bool UseCapsule
		{
			get { return _parameters.bUseCapsule == 1; }
			set { _parameters.bUseCapsule = value ? 1 : 0; }
		}

		/// <summary>
		/// The amount that the living-entity needs to move upwards before ground contact is lost. 
		/// Defaults to which ever is greater 0.004, or 0.01 * GeometryHeight
		/// </summary>
		public float GroundContactEpsilon
		{
			get { return _parameters.groundContactEps; }
			set { _parameters.groundContactEps = value; }
		}

		internal override pe_player_dimensions ToNativeParameters()
		{
			return _parameters;
		}
	}
}
