using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;

using System.Runtime.Serialization;

namespace CryEngine.Serialization
{
	public class ObjectWriter
	{
		public ObjectWriter(Stream stream)
		{
			Writer = new BinaryWriter(stream);

			Converter = new FormatterConverter();
		}

		Dictionary<Type, CachedTypeInfo> _typeCache = new Dictionary<Type, CachedTypeInfo>();

		BinaryWriter Writer { get; set; }

		FormatterConverter Converter { get; set; }

		public void Write(object obj)
		{
			WriteInstance(obj);

			Writer.Flush();
		}

		public void WriteStatics(Assembly assembly)
		{
			var types = assembly.GetTypes();

			Writer.Write(types.Length);

			foreach (var type in types)
			{
				if (type.IsGenericTypeDefinition || type.IsInterface || type.IsEnum)
				{
					Writer.Write(false);
					continue;
				}

				Writer.Write(true);
				WriteType(type);

				WriteObjectMembers(null, type, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly | BindingFlags.Static);
			}

			Writer.Flush();
		}

		void WriteInstance(object obj)
		{
			if (obj == null)
			{
				Writer.Write((byte)SerializedObjectType.Null);
				return;
			}

			var cachedTypeInfo = GetTypeInfo(obj.GetType());

			if (cachedTypeInfo._serializedType == SerializedObjectType.Boolean)
			{
				Writer.Write((byte)SerializedObjectType.Boolean);
				Writer.Write((bool)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Char)
			{
				Writer.Write((byte)SerializedObjectType.Char);
				Writer.Write((char)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.SByte)
			{
				Writer.Write((byte)SerializedObjectType.SByte);
				Writer.Write((sbyte)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Byte)
			{
				Writer.Write((byte)SerializedObjectType.Byte);
				Writer.Write((byte)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Int16)
			{
				Writer.Write((byte)SerializedObjectType.Int16);
				Writer.Write((Int16)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.UInt16)
			{
				Writer.Write((byte)SerializedObjectType.UInt16);
				Writer.Write((UInt16)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Int32)
			{
				Writer.Write((byte)SerializedObjectType.Int32);
				Writer.Write((Int32)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.UInt32)
			{
				Writer.Write((byte)SerializedObjectType.UInt32);
				Writer.Write((UInt32)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Int64)
			{
				Writer.Write((byte)SerializedObjectType.Int64);
				Writer.Write((Int64)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.UInt64)
			{
				Writer.Write((byte)SerializedObjectType.UInt64);
				Writer.Write((UInt64)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Single)
			{
				Writer.Write((byte)SerializedObjectType.Single);
				Writer.Write((Single)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Double)
			{
				Writer.Write((byte)SerializedObjectType.Double);
				Writer.Write((Double)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Decimal)
			{
				Writer.Write((byte)SerializedObjectType.Decimal);
				Writer.Write((Decimal)obj);
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.IntPtr)
			{
				Writer.Write((byte)SerializedObjectType.IntPtr);
				Writer.Write(((IntPtr)obj).ToInt64());
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.UIntPtr)
			{
				Writer.Write((byte)SerializedObjectType.UIntPtr);
				Writer.Write(((UIntPtr)obj).ToUInt64());
			}
			else if (cachedTypeInfo._serializedType == SerializedObjectType.Enum)
			{
				Writer.Write((byte)SerializedObjectType.Enum);
				WriteType(cachedTypeInfo._type);

				var integerType = Enum.GetUnderlyingType(cachedTypeInfo._type);
				WriteInstance(Convert.ChangeType(obj, integerType));
			}
			else
			{
				// Write reference types below
				// Skip if already written
				int referenceId;

				if (cachedTypeInfo.HasWrittenReference(obj, out referenceId))
				{
					Writer.Write((byte)SerializedObjectType.Reference);
					Writer.Write(referenceId);
					WriteType(cachedTypeInfo._type);
				}
				else
				{
					Writer.Write((byte)cachedTypeInfo._serializedType);
					Writer.Write(referenceId);

					switch (cachedTypeInfo._serializedType)
					{
						case SerializedObjectType.Type:
							{
								WriteType((Type)obj);
							}
							break;
						case SerializedObjectType.String:
							{
								Writer.Write((string)obj);
							}
							break;
						case SerializedObjectType.Array:
							{
								WriteArray(obj, cachedTypeInfo._type);
							}
							break;
						case SerializedObjectType.MemberInfo:
							{
								WriteMemberInfo(obj, cachedTypeInfo._type);
							}
							break;
						case SerializedObjectType.ISerializable:
							{
								WriteISerializable(obj, cachedTypeInfo._type);
							}
							break;
						case SerializedObjectType.Object:
							{
								WriteObject(obj, cachedTypeInfo._type);
							}
							break;
					}
				}
			}
		}

		CachedTypeInfo GetTypeInfo(Type type)
		{
			CachedTypeInfo cachedTypeInfo;
			if (!_typeCache.TryGetValue(type, out cachedTypeInfo))
			{
				cachedTypeInfo = new CachedTypeInfo(type);
				_typeCache[type] = cachedTypeInfo;
			}

			return cachedTypeInfo;
		}

		void WriteObject(object obj, Type objectType)
		{
			var types = new List<Type>();

			while (objectType != CachedTypeInfo._objectType)
			{
				types.Add(objectType);

				objectType = objectType.BaseType;
			}

			Writer.Write(types.Count);

			foreach (var storedType in types)
			{
				WriteType(storedType);

				WriteObjectMembers(obj, storedType, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly | BindingFlags.Instance);
			}
		}

		void WriteObjectMembers(object obj, Type objectType, BindingFlags flags)
		{
			var fields = objectType.GetFields(flags);
			Writer.Write(fields.Length);
			foreach (var field in fields)
			{
				Writer.Write(field.Name);
				var fieldValue = field.GetValue(obj);

				if (fieldValue != null && CachedTypeInfo._delegateType.IsAssignableFrom(fieldValue.GetType()))
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
			foreach (var eventMember in events)
			{
				Writer.Write(eventMember.Name);
				var eventField = objectType.GetField(eventMember.Name, flags);

				var value = eventField.GetValue(obj);
				if (value == null)
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

					foreach (var func in invocationList)
					{
						WriteInstance(func.Method);
						WriteInstance(func.Target);
					}
				}
			}

			// Special case, prevent deletion of SWIG pointers for non-director types
			// This is done by making sure that Dispose does not release the memory that we'll restore on deserialization
			var swigCMemOwn = objectType.GetField("swigCMemOwn", flags);
			if (swigCMemOwn != null)
			{
				swigCMemOwn.SetValue(obj, false);
			}
		}

		void WriteArray(object obj, Type type)
		{
			var array = obj as Array;
			Writer.Write(array.Length);

			WriteType(type.GetElementType());

			foreach (var element in array)
			{
				WriteInstance(element);
			}
		}

		void WriteMemberInfo(object obj, Type type)
		{
			var memberInfo = obj as MemberInfo;

			Writer.Write(memberInfo.Name);

			bool isStatic = false;
			var staticMembers = memberInfo.DeclaringType.GetMember(memberInfo.Name, BindingFlags.Static | BindingFlags.DeclaredOnly | BindingFlags.Public | BindingFlags.NonPublic);

			foreach (var member in staticMembers)
			{
				if (member == memberInfo)
				{
					isStatic = true;
					break;
				}
			}

			WriteType(memberInfo.DeclaringType);
			Writer.Write((byte)memberInfo.MemberType);

			Writer.Write(isStatic);

			if (memberInfo.MemberType == MemberTypes.Method)
			{
				var methodInfo = memberInfo as MethodInfo;

				var parameters = methodInfo.GetParameters();
				Writer.Write(parameters.Length);
				foreach (var parameter in parameters)
				{
					WriteType(parameter.ParameterType);
				}
			}
		}

		void WriteISerializable(object obj, Type type)
		{
			var serializable = obj as ISerializable;

			WriteType(type);

			var serInfo = new SerializationInfo(type, Converter);
			serializable.GetObjectData(serInfo, new StreamingContext(StreamingContextStates.CrossAppDomain));

			Writer.Write(serInfo.MemberCount);

			var enumerator = serInfo.GetEnumerator();
			while (enumerator.MoveNext())
			{
				Writer.Write(enumerator.Current.Name);
				WriteType(enumerator.Current.ObjectType);
				WriteInstance(enumerator.Current.Value);
			}
		}

		void WriteType(Type type)
		{
			Writer.Write(type.IsArray);
			if (type.IsArray)
			{
				WriteType(type.GetElementType());
			}
			else
			{
				Writer.Write(type.IsGenericType);

				if (type.IsGenericType)
				{
					Writer.Write(type.GetGenericTypeDefinition().FullName);

					var genericArgs = type.GetGenericArguments();
					Writer.Write(genericArgs.Length);
					foreach (var genericArg in genericArgs)
						WriteType(genericArg);
				}
				else
				{
					Writer.Write(type.FullName);
				}
			}
		}
	}
}
