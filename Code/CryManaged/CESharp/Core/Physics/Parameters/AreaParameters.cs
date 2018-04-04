using CryEngine.Common;

namespace CryEngine
{
	public class AreaParameters : BasePhysicsParameters<pe_params_area>
	{
		public class WaveSimulationParameters
		{
			private readonly params_wavesim _parameters = new params_wavesim();

			/// <summary>
			/// Fixed timestep used for the simulation
			/// </summary>
			public float TimeStep
			{
				get { return _parameters.timeStep; }
				set { _parameters.timeStep = value; }
			}

			/// <summary>
			/// Wave propagation speed
			/// </summary>
			public float WaveSpeed
			{
				get { return _parameters.waveSpeed; }
				set { _parameters.waveSpeed = value; }
			}

			/// <summary>
			/// Assumed height of moving water layer (relative to cell size)
			/// </summary>
			public float SimulationDepth
			{
				get { return _parameters.simDepth; }
				set { _parameters.simDepth = value; }
			}

			/// <summary>
			/// Hard limit on height changes (relative to cell size)
			/// </summary>
			public float HeightLimit
			{
				get { return _parameters.heightLimit; }
				set { _parameters.heightLimit = value; }
			}

			/// <summary>
			/// Rate of velocity transfer from floating objects
			/// </summary>
			public float Resistance
			{
				get { return _parameters.resistance; }
				set { _parameters.resistance = value; }
			}

			/// <summary>
			/// Damping in the central tile
			/// </summary>
			public float DampingCenter
			{
				get { return _parameters.dampingCenter; }
				set { _parameters.dampingCenter = value; }
			}

			/// <summary>
			/// Damping in the outer tiles
			/// </summary>
			public float DampingRim
			{
				get { return _parameters.dampingRim; }
				set { _parameters.dampingRim = value; }
			}

			/// <summary>
			/// Minimum height perturbation that activates a neighboring tile
			/// </summary>
			public float MinHeightSpread
			{
				get { return _parameters.minhSpread; }
				set { _parameters.minhSpread = value; }
			}

			/// <summary>
			/// Sleep speed threshold
			/// </summary>
			public float MinVelocity
			{
				get { return _parameters.minVel; }
				set { _parameters.minVel = value; }
			}

			internal params_wavesim ToNativeParameters()
			{
				return _parameters;
			}
		}

		private readonly pe_params_area _parameters = new pe_params_area();
		private WaveSimulationParameters _waveSimulation = null;

		/// <summary>
		/// Wave simulation parameters
		/// </summary>
		public WaveSimulationParameters WaveSimulation
		{
			get
			{
				if(_waveSimulation == null)
				{
					_waveSimulation = new WaveSimulationParameters();
				}
				return _waveSimulation;
			}
		}

		/// <summary>
		/// Gravity in this area. Depending on the value of Uniform the gravity will have the same direction in every point, or always points to the center.
		/// </summary>
		public Vector3 Gravity
		{
			get { return _parameters.gravity; }
			set { _parameters.gravity = value; }
		}

		/// <summary>
		/// Parametric distance (0..1) where falloff starts
		/// </summary>
		public float Falloff
		{
			get { return _parameters.falloff0; }
			set { _parameters.falloff0 = value; }
		}

		/// <summary>
		/// Gravity has same direction in every point or always points to the center
		/// </summary>
		public bool Uniform
		{
			get { return _parameters.bUniform == 1; }
			set { _parameters.bUniform = value ? 1 : 0; }
		}

		/// <summary>
		/// Will generate immediate EventPhysArea when needs to apply params to an entity
		/// </summary>
		public bool UseCallback
		{
			get { return _parameters.bUseCallback == 1; }
			set { _parameters.bUseCallback = value ? 1 : 0; }
		}

		/// <summary>
		/// Uniform damping
		/// </summary>
		public float Damping
		{
			get { return _parameters.damping; }
			set { _parameters.damping = value; }
		}

		/// <summary>
		/// Physical geometry used in the area
		/// </summary>
		public IGeometry Geometry
		{
			get { return _parameters.pGeom; }
			set { _parameters.pGeom = value; }
		}

		/// <summary>
		/// The area will try to maintain this volume by adjusting water level
		/// </summary>
		public float Volume
		{
			get { return _parameters.volume; }
			set { _parameters.volume = value; }
		}

		/// <summary>
		/// Accuracy of level computation based on volume (in fractions of the volume)
		/// </summary>
		public float VolumeAccuracy
		{
			get { return _parameters.volumeAccuracy; }
			set { _parameters.volumeAccuracy = value; }
		}

		/// <summary>
		/// After adjusting level, expand the border by this distance
		/// </summary>
		public float BorderPadding
		{
			get { return _parameters.borderPad; }
			set { _parameters.borderPad = value; }
		}

		/// <summary>
		/// Forces convex border after water level adjustments
		/// </summary>
		public bool ConvexBorder
		{
			get { return _parameters.bConvexBorder == 1; }
			set { _parameters.bConvexBorder = value ? 1 : 0; }
		}

		/// <summary>
		/// Only consider entities larger than this for level adjustment (set in fractions of area volume)
		/// </summary>
		public float ObjectVolumeThreshold
		{
			get { return _parameters.objectVolumeThreshold; }
			set { _parameters.objectVolumeThreshold = value; }
		}

		/// <summary>
		/// Cell size for wave simulation
		/// </summary>
		public float CellSize
		{
			get { return _parameters.cellSize; }
			set { _parameters.cellSize = value; }
		}

		/// <summary>
		/// Assume this area increase during level adjustment (only used for wave simulation)
		/// </summary>
		public float GrowthReserve
		{
			get { return _parameters.growthReserve; }
			set { _parameters.growthReserve = value; }
		}

		internal override pe_params_area ToNativeParameters()
		{
			if(_waveSimulation != null)
			{
				_parameters.waveSim = _waveSimulation.ToNativeParameters();
			}
			return _parameters;
		}
	}
}
