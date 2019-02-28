// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;

namespace CryEngine.Serialization
{
	internal class ObjectWriter
	{
		internal ObjectWriter(Stream stream)
		{
			Writer = new BinaryWriter(stream);

			Converter = new FormatterConverter();
		}

		private Dictionary<Type, CachedTypeInfo> _typeCache = new Dictionary<Type, CachedTypeInfo>();

		private BinaryWriter Writer { get; set; }

		private FormatterConverter Converter { get; set; }

		internal void Write(object obj)
		{
			WriteInstance(obj);

			Writer.Flush();
		}

		internal void WriteStatics(Assembly assembly)
		{
			if(assembly != null)
			{
				var types = assembly.GetTypes();

				Writer.Write(types.Length);

				foreach(var type in types)
				{
					if(type.IsGenericTypeDefinition || type.IsInterface || type.IsEnum)
					{
						Writer.Write(false);
						continue;
					}

					Writer.Write(true);
					WriteType(type);

					WriteObjectMembers(null, type, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly | BindingFlags.Static);
				}
			}
			else
			{
				Writer.Write(0);
			}

			Writer.Flush();
		}

		private void WriteInstance(object obj)
		{
			if(obj == null)
			{
				Writer.Write((byte)SerializedObjectType.Null);
				return;
			}

			var cachedTypeInfo = GetTypeInfo(obj.GetType());

			if(cachedTypeInfo.SerializedType == SerializedObjectType.Boolean)
			{
				Writer.Write((byte)SerializedObjectType.Boolean);
				Writer.Write((bool)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Char)
			{
				Writer.Write((byte)SerializedObjectType.Char);
				Writer.Write((char)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.SByte)
			{
				Writer.Write((byte)SerializedObjectType.SByte);
				Writer.Write((sbyte)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Byte)
			{
				Writer.Write((byte)SerializedObjectType.Byte);
				Writer.Write((byte)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Int16)
			{
				Writer.Write((byte)SerializedObjectType.Int16);
				Writer.Write((short)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.UInt16)
			{
				Writer.Write((byte)SerializedObjectType.UInt16);
				Writer.Write((ushort)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Int32)
			{
				Writer.Write((byte)SerializedObjectType.Int32);
				Writer.Write((int)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.UInt32)
			{
				Writer.Write((byte)SerializedObjectType.UInt32);
				Writer.Write((uint)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Int64)
			{
				Writer.Write((byte)SerializedObjectType.Int64);
				Writer.Write((long)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.UInt64)
			{
				Writer.Write((byte)SerializedObjectType.UInt64);
				Writer.Write((ulong)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Single)
			{
				Writer.Write((byte)SerializedObjectType.Single);
				Writer.Write((float)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Double)
			{
				Writer.Write((byte)SerializedObjectType.Double);
				Writer.Write((double)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Decimal)
			{
				Writer.Write((byte)SerializedObjectType.Decimal);
				Writer.Write((decimal)obj);
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.IntPtr)
			{
				Writer.Write((byte)SerializedObjectType.IntPtr);
				Writer.Write(((IntPtr)obj).ToInt64());
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.UIntPtr)
			{
				Writer.Write((byte)SerializedObjectType.UIntPtr);
				Writer.Write(((UIntPtr)obj).ToUInt64());
			}
			else if(cachedTypeInfo.SerializedType == SerializedObjectType.Enum)
			{
				Writer.Write((byte)SerializedObjectType.Enum);
				WriteType(cachedTypeInfo.CachedType);

				var integerType = Enum.GetUnderlyingType(cachedTypeInfo.CachedType);
				WriteInstance(Convert.ChangeType(obj, integerType));
			}
			else
			{
				// Write reference types below
				// Skip if already written

				if(cachedTypeInfo.HasWrittenReference(obj, out int referenceId))
				{
					Writer.Write((byte)SerializedObjectType.Reference);
					Writer.Write(referenceId);
					WriteType(cachedTypeInfo.CachedType);
				}
				else
				{
					Writer.Write((byte)cachedTypeInfo.SerializedType);
					Writer.Write(referenceId);

					switch(cachedTypeInfo.SerializedType)
					{
						case SerializedObjectType.Type:
						{
							WriteType((Type)obj);
							break;
						}

						case SerializedObjectType.Assembly:
						{
							WriteAssembly((Assembly)obj);
							break;
						}
						case SerializedObjectType.String:
						{
							Write((string)obj);
							break;
						}
						case SerializedObjectType.Array:
						{
							WriteArray(obj, cachedTypeInfo.CachedType);
							break;
						}
						case SerializedObjectType.MemberInfo:
						{
							WriteMemberInfo(obj);
							break;
						}
						case SerializedObjectType.ISerializable:
						{
							WriteISerializable(obj, cachedTypeInfo.CachedType);
							break;
						}
						case SerializedObjectType.EntityComponent:
						{
							WriteEntityComponent(obj, cachedTypeInfo.CachedType);
							break;
						}
						case SerializedObjectType.Object:
						{
							WriteObject(obj, cachedTypeInfo.CachedType);
							break;
						}
					}
				}
			}
		}

		private CachedTypeInfo GetTypeInfo(Type type)
		{
			if(!_typeCache.TryGetValue(type, out CachedTypeInfo cachedTypeInfo))
			{
				cachedTypeInfo = new CachedTypeInfo(type);
				_typeCache[type] = cachedTypeInfo;
			}

			return cachedTypeInfo;
		}

		private void WriteEntityComponent(object obj, Type objectType)
		{
			var componentTypeGUID = EntityComponent.GetComponentTypeGUID(objectType);
			Write(componentTypeGUID);

			WriteObjectMembers(obj, objectType);
		}

		private void WriteObject(object obj, Type objectType)
		{
			WriteType(objectType);

			WriteObjectMembers(obj, objectType);
		}

		private void WriteObjectMembers(object obj, Type objectType)
		{
			var baseType = objectType.BaseType;
			var baseTypes = new List<Type>();

			while(baseType != CachedTypeInfo.ObjectType && baseType != null)
			{
				baseTypes.Add(baseType);

				baseType = baseType.BaseType;
			}

			Writer.Write(baseTypes.Count);

			WriteObjectMembers(obj, objectType, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly | BindingFlags.Instance);

			foreach(var storedType in baseTypes)
			{
				WriteType(storedType);

				WriteObjectMembers(obj, storedType, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly | BindingFlags.Instance);
			}
		}

		private void WriteObjectMembers(object obj, Type objectType, BindingFlags flags)
		{
			var fields = new List<FieldInfo>(objectType.GetFields(flags));
			for(int i = fields.Count - 1; i > -1; --i)
			{
				var field = fields[i];
				if(field.Attributes.HasFlag(FieldAttributes.NotSerialized) || field.GetCustomAttribute<NonSerializedAttribute>(true) != null)
				{
					fields.RemoveAt(i);
				}
			}

			Writer.Write(fields.Count);
			foreach(var field in fields)
			{
				Write(field.Name);
				var fieldValue = field.GetValue(obj);

				if(fieldValue != null && CachedTypeInfo.DelegateType.IsAssignableFrom(fieldValue.GetType()))
				{
					WriteInstance(null);
				}
				else
				{
					WriteInstance(fieldValue);
				}
			}

			var events = objectType.GetEvents(flags);
			Writer.Write(events.Length);
			foreach(var eventMember in events)
			{
				Write(eventMember.Name);
				var eventField = objectType.GetField(eventMember.Name, flags);

				var value = eventField.GetValue(obj);
				if(value == null)
				{
					Writer.Write(false);
				}
				else
				{
					Writer.Write(true);

					var delegateObject = value as Delegate;
					WriteType(delegateObject.GetType());
					var invocationList = delegateObject.GetInvocationList();

					Writer.Write(invocationList.Length);

					foreach(var func in invocationList)
					{
						WriteInstance(func.Method);
						WriteInstance(func.Target);
					}
				}
			}

			// Special case, prevent deletion of SWIG pointers for non-director types
			// This is done by making sure that Dispose does not release the memory that we'll restore on deserialization
			var swigCMemOwn = objectType.GetField("swigCMemOwn", flags);
			if(swigCMemOwn != null)
			{
				swigCMemOwn.SetValue(obj, false);
			}
		}

		private void WriteArray(object obj, Type type)
		{
			var array = obj as Array;
			Writer.Write(array.Length);

			WriteType(type.GetElementType());

			foreach(var element in array)
			{
				WriteInstance(element);
			}
		}

		private void WriteMemberInfo(object obj)
		{
			var memberInfo = obj as MemberInfo;

			Write(memberInfo.Name);

			bool isStatic = false;
			var staticMembers = memberInfo.DeclaringType.GetMember(memberInfo.Name, BindingFlags.Static | BindingFlags.DeclaredOnly | BindingFlags.Public | BindingFlags.NonPublic);

			foreach(var member in staticMembers)
			{
				if(member == memberInfo)
				{
					isStatic = true;
					break;
				}
			}

			WriteType(memberInfo.DeclaringType);
			Writer.Write((byte)memberInfo.MemberType);

			Writer.Write(isStatic);

			if(memberInfo.MemberType == MemberTypes.Method)
			{
				var methodInfo = memberInfo as MethodInfo;

				var parameters = methodInfo.GetParameters();
				Writer.Write(parameters.Length);
				foreach(var parameter in parameters)
				{
					WriteType(parameter.ParameterType);
				}
			}
		}

		private void WriteISerializable(object obj, Type type)
		{
			var serializable = obj as ISerializable;

			WriteType(type);

			var serInfo = new SerializationInfo(type, Converter);
			serializable.GetObjectData(serInfo, new StreamingContext(StreamingContextStates.CrossAppDomain));

			Writer.Write(serInfo.MemberCount);

			var enumerator = serInfo.GetEnumerator();
			while(enumerator.MoveNext())
			{
				Write(enumerator.Current.Name);
				WriteType(enumerator.Current.ObjectType);
				WriteInstance(enumerator.Current.Value);
			}
		}

		private void WriteType(Type type)
		{
			Writer.Write(type.IsArray);
			if(type.IsArray)
			{
				WriteType(type.GetElementType());
			}
			else
			{
				var guid = GetGuid(type);
				Writer.Write(guid != null);
				if(guid != null)
				{
					Write(guid);
				}

				Writer.Write(type.IsGenericType);

				// Write the AssemblyQualifiedName instead of the FullName.
				// This way we don't run into problems when deserializing types from other assemblies (eg. Cryengine.Core.UI).
				if(type.IsGenericType)
				{
					Write(type.GetGenericTypeDefinition().AssemblyQualifiedName);

					var genericArgs = type.GetGenericArguments();
					Writer.Write(genericArgs.Length);
					foreach(var genericArg in genericArgs)
					{
						WriteType(genericArg);
					}
				}
				else
				{
					Write(type.AssemblyQualifiedName);
				}
			}
		}

		private void WriteAssembly(Assembly assembly)
		{
			Write(assembly.Location);
		}

		internal static string GetGuid(Type type)
		{
			var guidAttribute = type.GetCustomAttribute<GuidAttribute>(false);
			if(guidAttribute != null)
			{
				var guid = guidAttribute.Value;
				if(!string.IsNullOrWhiteSpace(guid))
				{
					return guid;
				}
			}

			var componentAttribute = type.GetCustomAttribute<EntityComponentAttribute>(false);
			if(componentAttribute != null)
			{
				var guid = componentAttribute.Guid;
				if(!string.IsNullOrWhiteSpace(guid))
				{
					return guid;
				}
			}
			return null;
		}

		private void Write(string text)
		{
			// Write strings here for easier debugging.
			Writer.Write(text);
		}
	}
}
