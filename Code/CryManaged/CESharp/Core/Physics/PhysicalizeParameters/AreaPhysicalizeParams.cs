using System;
using CryEngine.Common;

namespace CryEngine
{
	/// <summary>
	/// Parameters for Physicalizing an Area.
	/// </summary>
	public class AreaPhysicalizeParams : EntityPhysicalizeParams
	{
		private readonly AreaParameters _gravityParameters = new AreaParameters();
		private BuoyancyParameters _buoyancyParameters = null;

		internal override pe_type Type { get { return pe_type.PE_AREA; } }

		public BuoyancyParameters BuoyancyParameters
		{
			get
			{
				if(_buoyancyParameters == null)
				{
					_buoyancyParameters = new BuoyancyParameters();
				}
				return _buoyancyParameters;
			}
		}

		/// <summary>
		/// 
		/// </summary>
		public AreaParameters GravityParameters { get { return _gravityParameters; } }

		/// <summary>
		/// The shape of the physical area.
		/// </summary>
		public AreaType AreaType { get; set; } = AreaType.Sphere;

		/// <summary>
		/// Must be set when using AreaType.Sphere or AreaType.Cylinder area type or AreaType.Spline.
		/// </summary>
		public float Radius { get; set; }

		/// <summary>
		/// Min of bounding box, must be set when using AreaType.Box area type.
		/// </summary>
		public Vector3 BoxMin { get; set; }

		/// <summary>
		/// Max of bounding box, must be set when using AreaType.Box area type.
		/// </summary>
		public Vector3 BoxMax { get; set; }

		//TODO Currently disabled because AreaDefinition.pPoints can't receive a collection of points.
		/// <summary>
		/// Must be set when using AreaType.Shape area type or AreaType.Spline.
		/// </summary>
		/*public Vector3[] Points
		{
			get { throw new NotImplementedException("Getting the value of Points is currently not available"); }
			set { throw new NotImplementedException("Setting the value of Points is currently not available"); }
		}*/

		/// <summary>
		/// Minimum height of points.
		/// </summary>
		public float MinHeight { get; set; }

		/// <summary>
		/// Maximum height of points.
		/// </summary>
		public float MaxHeight { get; set; }

		/// <summary>
		/// Center of the area.
		/// </summary>
		public Vector3 Center { get; set; }

		//TODO Figure out what Axis does.
		public Vector3 Axis { get; set; }

		internal override SEntityPhysicalizeParams ToNativeParams()
		{
			var parameters = base.ToNativeParams();
			var areaDefinition = new SEntityPhysicalizeParams.AreaDefinition();

			switch(AreaType)
			{
				case AreaType.Sphere:
					areaDefinition.areaType = SEntityPhysicalizeParams.AreaDefinition.EAreaType.AREA_SPHERE;
					break;
				case AreaType.Box:
					areaDefinition.areaType = SEntityPhysicalizeParams.AreaDefinition.EAreaType.AREA_BOX;
					break;
				case AreaType.Geometry:
					areaDefinition.areaType = SEntityPhysicalizeParams.AreaDefinition.EAreaType.AREA_GEOMETRY;
					break;
				case AreaType.Shape:
					areaDefinition.areaType = SEntityPhysicalizeParams.AreaDefinition.EAreaType.AREA_SHAPE;
					break;
				case AreaType.Cylinder:
					areaDefinition.areaType = SEntityPhysicalizeParams.AreaDefinition.EAreaType.AREA_CYLINDER;
					break;
				case AreaType.Spline:
					areaDefinition.areaType = SEntityPhysicalizeParams.AreaDefinition.EAreaType.AREA_SPLINE;
					break;
				default:
					throw new NotImplementedException(string.Format("The value {0} of AreaType cannot be converted to a valid EAreaType!", AreaType));
			}

			areaDefinition.fRadius = Radius;
			areaDefinition.boxmin = BoxMin;
			areaDefinition.boxmax = BoxMax;

			//TODO Add support for sending a collection of points instead of setting a single point.
			//areaDefinition.pPoints = Points;

			areaDefinition.zmin = MinHeight;
			areaDefinition.zmax = MaxHeight;
			areaDefinition.center = Center;
			areaDefinition.axis = Axis;

			areaDefinition.pGravityParams = _gravityParameters.ToNativeParameters();

			//Set the buoyancy only if it has been assigned a value.
			if(_buoyancyParameters != null)
			{
				parameters.pBuoyancy = _buoyancyParameters.ToNativeParameters();
			}

			parameters.pAreaDef = areaDefinition;
			return parameters;
		}
	}
}
