// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Serialization;

namespace CryEngine.Serialization
{
	internal class CachedTypeInfo
	{
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

		

		
		private static readonly Type _objectType = typeof(object);
		private static readonly Type _delegateType = typeof(Delegate);
		private static readonly Type _typeType = typeof(Type);
		private static readonly Type _assemblyType = typeof(Assembly);
		private static readonly Type _memberInfoType = typeof(MemberInfo);
		private static readonly Type _stringType = typeof(string);
		private static readonly Type _intptrType = typeof(IntPtr);
		private static readonly Type _uintptrType = typeof(UIntPtr);
		private static readonly Type _decimalType = typeof(decimal);

		private static readonly Type _SerializeValueType = typeof(ISerializable);
		private static readonly Type _entityComponentType = typeof(EntityComponent);

		private static readonly Type _monoType;
		private static readonly Type _monoAssemblyType;

		private readonly Type _type;
		private readonly SerializedObjectType _serializedType;

		public static Type ObjectType { get { return _objectType; } }
		public static Type DelegateType { get { return _delegateType; } }
		public Type CachedType { get { return _type; } }
		public SerializedObjectType SerializedType { get { return _serializedType; } }

		static CachedTypeInfo()
		{
			_monoType = Type.GetType("System.MonoType");
			_monoAssemblyType = Type.GetType("System.Reflection.MonoAssembly");
		}

		public CachedTypeInfo(Type type)
		{
			_type = type;

			// Start by writing primitives, these cannot be references
			if(type == _intptrType)
			{
				_serializedType = SerializedObjectType.IntPtr;
			}
			else if(type == _uintptrType)
			{
				_serializedType = SerializedObjectType.UIntPtr;
			}
			else if(_type.IsPrimitive || _type == _decimalType)
			{
				_serializedType = (SerializedObjectType)Type.GetTypeCode(_type);
			}
			else if(_type.IsEnum)
			{
				_serializedType = SerializedObjectType.Enum;
			}
			else if(_assemblyType.IsAssignableFrom(_type) || _type == _monoAssemblyType)
			{
				_serializedType = SerializedObjectType.Assembly;
			}
			else if(_typeType.IsAssignableFrom(_type) || _type == _monoType)
			{
				_serializedType = SerializedObjectType.Type;
			}
			else if(_type == _stringType)
			{
				_serializedType = SerializedObjectType.String;
			}
			else if(_type.IsArray)
			{
				_serializedType = SerializedObjectType.Array;
			}
			else if(_memberInfoType.IsAssignableFrom(_type))
			{
				_serializedType = SerializedObjectType.MemberInfo;
			}
			else if(_SerializeValueType.IsAssignableFrom(_type) && _type.GetCustomAttributes<SerializeValueAttribute>(true) != null)
			{
				_serializedType = SerializedObjectType.ISerializable;
			}
			else if(_entityComponentType.IsAssignableFrom(type))
			{
				_serializedType = SerializedObjectType.EntityComponent;
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
			if(_objects.TryGetValue(obj, out referenceId))
			{
				return true;
			}

			referenceId = _objects.Count;
			_objects.Add(obj, referenceId);

			return false;
		}
	}
}
