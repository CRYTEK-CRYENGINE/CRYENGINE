// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Globalization;
using System.Text;
using CryEngine.Common;

namespace CryEngine.EntitySystem
{
	/// <summary>
	/// Interface for entity property conversion. Necessary to expose a certain property type to the Sandbox.
	/// </summary>
	public interface IEntityPropertyConverter
	{
		#region Properties
		/// <summary>
		/// Managed property type.
		/// </summary>
		Type PropertyType { get; }
		/// <summary>
		/// CRYENGINE type equivalent.
		/// </summary>
		IEntityPropertyHandler.EPropertyType EngineType { get; }
		EEditTypes DefaultEditType { get; }
		#endregion

		#region Methods
		/// <summary>
		/// Convert the given input string to the property type.
		/// </summary>
		/// <returns>Converted value of 'PropertyType', casted to object.</returns>
		/// <param name="strValue">String representing the value.</param>
		object SetValue(string strValue);
		/// <summary>
		/// Convert the given input object of type 'PropertyType' casted to object.
		/// </summary>
		/// <returns>Value of type 'PropertyType' casted to object.</returns>
		/// <param name="objValue">String representing the value.</param>
		string GetValue(object objValue);
		#endregion
	}

	/// <summary>
	/// Generic implementation of the property converter interface
	/// </summary>
	public abstract class EntityPropertyConverter<T> : IEntityPropertyConverter
	{
		#region Properties
		public Type PropertyType { get { return typeof(T); } }
		public abstract IEntityPropertyHandler.EPropertyType EngineType { get; }
		public abstract EEditTypes DefaultEditType { get; }
		#endregion

		#region Methods
		public object SetValue(string strValue)
		{
			return (object)Set (strValue);
		}

		public string GetValue(object objValue)
		{
			return Get ((T)objValue);
		}

		protected abstract T Set(string strValue);
		protected abstract string Get(T objValue);
		#endregion
	}

	public class StringPropertyConverter : EntityPropertyConverter<string>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.String; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.String; } }
		#endregion

		#region Methods
		protected override string Get (string objValue)
		{
			return objValue;
		}

		protected override string Set (string strValue)
		{
			return strValue;
		}
		#endregion
	}

	public class Int32PropertyConverter : EntityPropertyConverter<Int32>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Int; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Number; } }
		#endregion

		#region Methods
		protected override string Get (Int32 objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override Int32 Set (string strValue)
		{
			return Convert.ToInt32 (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class UInt32PropertyConverter : EntityPropertyConverter<UInt32>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Int; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Integer; } }
		#endregion

		#region Methods
		protected override string Get (UInt32 objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override UInt32 Set (string strValue)
		{
			return Convert.ToUInt32 (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class Int64PropertyConverter : EntityPropertyConverter<Int64>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Int; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Integer; } }
		#endregion

		#region Methods
		protected override string Get (Int64 objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override Int64 Set (string strValue)
		{
			return Convert.ToInt64 (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class UInt64PropertyConverter : EntityPropertyConverter<UInt64>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Int; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Integer; } }
		#endregion

		#region Methods
		protected override string Get (UInt64 objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override UInt64 Set (string strValue)
		{
			return Convert.ToUInt64 (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class FloatPropertyConverter : EntityPropertyConverter<float>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Float; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Float; } }
		#endregion

		#region Methods
		protected override string Get (float objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override float Set (string strValue)
		{
			return Convert.ToSingle (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class DoublePropertyConverter : EntityPropertyConverter<double>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Float; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Float; } }
		#endregion

		#region Methods
		protected override string Get (double objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override double Set (string strValue)
		{
			return Convert.ToDouble (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class DecimalPropertyConverter : EntityPropertyConverter<decimal>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Float; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Float; } }
		#endregion

		#region Methods
		protected override string Get (decimal objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override decimal Set (string strValue)
		{
			return Convert.ToDecimal (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class CharPropertyConverter : EntityPropertyConverter<char>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.String; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.String; } }
		#endregion

		#region Methods
		protected override string Get (char objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override char Set (string strValue)
		{
			return Convert.ToChar (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class BytePropertyConverter : EntityPropertyConverter<byte>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Int; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Integer; } }
		#endregion

		#region Methods
		protected override string Get (byte objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override byte Set (string strValue)
		{
			return Convert.ToByte (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class BoolPropertyConverter : EntityPropertyConverter<bool>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Bool; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Bool; } }
		#endregion

		#region Methods
		protected override string Get (bool objValue)
		{
			return Convert.ToString (objValue, CultureInfo.InvariantCulture);
		}

		protected override bool Set (string strValue)
		{
			return Convert.ToBoolean (strValue, CultureInfo.InvariantCulture);
		}
		#endregion
	}

	public class Vec3PropertyConverter : EntityPropertyConverter<Vec3>
	{
		#region Properties
		public override IEntityPropertyHandler.EPropertyType EngineType { get { return IEntityPropertyHandler.EPropertyType.Vector; } }
		public override EEditTypes DefaultEditType { get { return EEditTypes.Vector; } }
		#endregion

		#region Methods
		protected override string Get (Vec3 objValue)
		{
			if (objValue == null)
				return String.Empty;
			
			StringBuilder sb = new StringBuilder ();
			sb.Append (objValue.x.ToString (CultureInfo.InvariantCulture));
			sb.Append (',');
			sb.Append (objValue.x.ToString (CultureInfo.InvariantCulture));
			sb.Append (',');
			sb.Append (objValue.x.ToString (CultureInfo.InvariantCulture));
			return sb.ToString ();
		}

		protected override Vec3 Set (string strValue)
		{
			if (String.IsNullOrEmpty (strValue))
				return new Vec3 ();

			string[] parts = strValue.Split (',');
			if (parts == null || parts.Length < 3)
				return new Vec3 ();

			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			if (!float.TryParse (parts [0], NumberStyles.Any, CultureInfo.InvariantCulture, out x) || 
				!float.TryParse (parts [1], NumberStyles.Any, CultureInfo.InvariantCulture, out y) ||
				!float.TryParse (parts [2], NumberStyles.Any, CultureInfo.InvariantCulture, out z))
				return new Vec3 ();

			return new Vec3 (x, y, z);
		}
		#endregion
	}
}

