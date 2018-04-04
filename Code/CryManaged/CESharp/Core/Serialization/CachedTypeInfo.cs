using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Serialization;

namespace CryEngine.Serialization
{
	internal class CachedTypeInfo
	{
		public Type _type;
		public SerializedObjectType _serializedType;

		public class IdentityEqualityComparer<T> : IEqualityComparer<T> where T : class
		{
			public int GetHashCode(T value)
			{
				return RuntimeHelpers.GetHashCode(value);
			}

			public bool Equals(T left, T right)
			{
				return left == right; // Reference identity comparison
			}
		}

		private readonly Dictionary<object, int> _objects = new Dictionary<object, int>(new IdentityEqualityComparer<object>());

		public static Type _objectType = typeof(object);
		public static Type _delegateType = typeof(Delegate);
		static Type _typeType = typeof(Type);
        static Type _assemblyType = typeof(Assembly);
		static Type _memberInfoType = typeof(MemberInfo);
		static Type _stringType = typeof(string);
		static Type _intptrType = typeof(IntPtr);
		static Type _uintptrType = typeof(UIntPtr);
		static Type _decimalType = typeof(decimal);

		static Type _serializableType = typeof(ISerializable);

		static Type _monoType;
        static Type _monoAssemblyType;

		static CachedTypeInfo()
		{
            _monoType = Type.GetType("System.MonoType");
            _monoAssemblyType = Type.GetType("System.Reflection.MonoAssembly");
        }

		public CachedTypeInfo(Type type)
		{
			_type = type;

			// Start by writing primitives, these cannot be references
			if (type == _intptrType)
			{
				_serializedType = SerializedObjectType.IntPtr;
			}
			else if (type == _uintptrType)
			{
				_serializedType = SerializedObjectType.UIntPtr;
			}
			else if (_type.IsPrimitive || _type == _decimalType)
			{
				_serializedType = (SerializedObjectType)Type.GetTypeCode(_type);
			}
			else if (_type.IsEnum)
			{
				_serializedType = SerializedObjectType.Enum;
			}
            else if (_assemblyType.IsAssignableFrom(_type) || _type == _monoAssemblyType)
            {
                _serializedType = SerializedObjectType.Assembly;
            }
            else if (_typeType.IsAssignableFrom(_type) || _type == _monoType)
            {
                _serializedType = SerializedObjectType.Type;
            }
            else if (_type == _stringType)
			{
				_serializedType = SerializedObjectType.String;
			}
			else if (_type.IsArray)
			{
				_serializedType = SerializedObjectType.Array;
			}
            else if (_memberInfoType.IsAssignableFrom(_type))
            {
                _serializedType = SerializedObjectType.MemberInfo;
            }
            else if (_serializableType.IsAssignableFrom(_type) && _type.GetCustomAttributes(typeof(SerializableAttribute), true).Length > 0)
			{
				_serializedType = SerializedObjectType.ISerializable;
			}
			else
			{
				_serializedType = SerializedObjectType.Object;
			}
		}

		/// <summary>
		/// Gets the reference id of an object if it has already been written
		/// </summary>
		/// <returns><c>true</c>, if the reference was found, <c>false</c> otherwise.</returns>
		/// <param name="obj">Key to the reference value.</param>
		/// <param name="referenceId">ID of the reference.</param>
		public bool HasWrittenReference(object obj, out int referenceId)
		{
			if (_objects.TryGetValue(obj, out referenceId))
			{
				return true;
			}

			referenceId = _objects.Count;
			_objects.Add(obj, referenceId);

			return false;
		}
	}
}
