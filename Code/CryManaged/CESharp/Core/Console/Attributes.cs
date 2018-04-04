// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.	

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Text;
using CryEngine.Common;

namespace CryEngine.Attributes
{
	[AttributeUsage(AttributeTargets.Method, AllowMultiple = false, Inherited = false)]
	public sealed class ConsoleCommandAttribute : Attribute
	{
		public ConsoleCommandAttribute(string commandName, uint commandFlag, string commandHelp)
		{
			Name = commandName;
			Flags = commandFlag;
			Comments = commandHelp;
		}

		public string Name
		{
			get;
			private set;
		}

		public string Comments
		{
			get;
			private set;
		}

		public uint Flags
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// Thrown when Console Command Attributes are applied wrongly
	/// </summary>
	public sealed class ConsoleCommandConfigurationException : Exception
	{
		public ConsoleCommandConfigurationException(string msg) : base(msg)
		{

		}
	}

	public static class ConsoleCommandAttributeManager
	{
		[Conditional("DEBUG")]
		private static void ValidateConsoleCommandRegisterAttributes(ref List<MethodInfo> processedlist, Assembly targetAssembly)
		{
			// detect attributes from all methods
			var totalAttributes = targetAssembly.GetTypes().SelectMany(x => x.GetMethods(BindingFlags.Static | BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public))
				.Where(y => y.GetCustomAttributes().OfType<ConsoleCommandAttribute>().Any())
				.ToList();

			//filter attributes that are detected but not processed, these are configured wrongly !
			var filteredAttributes = totalAttributes.Except(processedlist).ToList();
			if(filteredAttributes.Count > 0)
			{
				var msg = new StringBuilder();
				foreach(var filteredAttribute in filteredAttributes)
				{
					msg.Append("Attribute on method ").Append(filteredAttribute.Name).Append(" is not processed").AppendLine();
				}
				throw new ConsoleCommandConfigurationException(msg.ToString());
			}
		}

		/// <summary>
		/// Parse the specified assembly for all public, static methods marked with attribute "ConsoleCommandRegisterAttribute". Ignores all methods otherwise.
		/// </summary>
		/// <param name="targetAssembly"></param>
		public static void RegisterAttribute(Assembly targetAssembly)
		{
			//do reflection to invoke the attribute for console command function
			Assembly assembly = targetAssembly ?? Assembly.GetExecutingAssembly();

			var processedList = assembly.GetTypes().SelectMany(x => x.GetMethods(BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public))
				.Where(y => y.GetCustomAttributes().OfType<ConsoleCommandAttribute>().Any())
				.ToList();

			var managedConsoleType = typeof(ConsoleCommand.ManagedConsoleCommandFunctionDelegate);
			foreach(MethodInfo methodInfo in processedList)
			{
				var managedConsoleCmdDelegate = Delegate.CreateDelegate(managedConsoleType, methodInfo) as ConsoleCommand.ManagedConsoleCommandFunctionDelegate;
				if(managedConsoleCmdDelegate != null)
				{
					var consoleAttribute = methodInfo.GetCustomAttribute<ConsoleCommandAttribute>() as ConsoleCommandAttribute;
					if(consoleAttribute != null)
					{
						ConsoleCommand.RegisterManagedConsoleCommandFunction(consoleAttribute.Name, consoleAttribute.Flags, consoleAttribute.Comments, managedConsoleCmdDelegate);
					}
				}
			}
			ValidateConsoleCommandRegisterAttributes(ref processedList, assembly);
		}
	}

	/// <summary>
	/// Responsible to registration, processing and validation of console variable attributes. Currently there are 4 types of console variable attributes.
	/// 1. String - Can be read and modified after declaration
	/// 2. Integer (32-bit) - Can be read modified after declaration
	/// 3. Integer (64-bit) - Can be read and modified after declaration
	/// 4. Float - Can be read and modified after declaration
	/// </summary>
	public static class ConsoleVariableAttributeManager
	{
		public static void RegisterAttribute(Assembly targetAssembly)
		{
			RegisterSubAttribute<ConsoleVariableStringAttribute, ConsoleVariableAttributeStringProperty>(targetAssembly);
			RegisterSubAttribute<ConsoleVariableIntegerAttribute, ConsoleVariableAttributeIntegerProperty>(targetAssembly);
			RegisterSubAttribute<ConsoleVariableInteger64Attribute, ConsoleVariableAttributeInteger64Property>(targetAssembly);
			RegisterSubAttribute<ConsoleVariableFloatAttribute, ConsoleVariableAttributeFloatProperty>(targetAssembly);
		}
		private static void RegisterSubAttribute<TAttribute, TAttributeProperty>(Assembly targetAssembly)
			where TAttribute : ConsoleVariableAttribute
			where TAttributeProperty : class, new()
		{
			Assembly assembly = targetAssembly ?? Assembly.GetExecutingAssembly();
			var processedTypes = assembly.GetTypes().SelectMany(x => x.GetFields(BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public))
				.Where(y => y.GetCustomAttributes().OfType<TAttribute>().Any())
				.ToList();
			foreach(FieldInfo fieldInfo in processedTypes)
			{
				Type fieldType = fieldInfo.FieldType;

				var attributeProperty = fieldInfo.GetValue(null) as TAttributeProperty;
				var consoleVariableAttribute = fieldInfo.GetCustomAttribute<ConsoleVariableAttribute>() as ConsoleVariableAttribute;
				if(fieldType == typeof(TAttributeProperty))
				{
					if(attributeProperty != null)
					{
						ICVar newVar = null;
						ProcessConsoleVariableProperty(consoleVariableAttribute, attributeProperty, out newVar);
					}
					else
					{
						throw new ConsoleVariableConfigurationException("Console Variable Attribute " + consoleVariableAttribute.Name + " cannot be applied to static variable " + fieldType.FullName + " which is null!");
					}
				}
				else // attribute declared does not match property value 
				{
					throw new ConsoleVariableConfigurationException("Attribute [" + consoleVariableAttribute.Name + "] type does not match Property type " + fieldType);
				}
			}
		}

		private static void ProcessConsoleVariableProperty(ConsoleVariableAttribute consoleVariableAttribute, object property, out ICVar icvar)
		{
			icvar = null;
			var stringProperty = property as ConsoleVariableAttributeStringProperty;
			if(stringProperty != null)
			{
				var stringAttribute = consoleVariableAttribute as ConsoleVariableStringAttribute;
				string value = stringProperty.Content ?? stringAttribute.Content;
				var success = ConsoleVariable.Register(consoleVariableAttribute.Name, value, consoleVariableAttribute.Flags, consoleVariableAttribute.Comments, stringProperty.OnValueChanged, out icvar);
				if(success)
				{
					stringProperty.SetInternalContent(value, icvar);
				}
				return;
			}

			var integerProperty = property as ConsoleVariableAttributeIntegerProperty;
			if(integerProperty != null)
			{
				var integerAttribute = consoleVariableAttribute as ConsoleVariableIntegerAttribute;
				int value = integerAttribute.Content.HasValue ? integerAttribute.Content.Value : integerProperty.Content;
				var success = ConsoleVariable.Register(consoleVariableAttribute.Name, value, consoleVariableAttribute.Flags, consoleVariableAttribute.Comments, integerProperty.OnValueChanged, out icvar);
				if(success)
				{
					integerProperty.SetInternalContent(value, icvar);
				}
				return;
			}

			var floatProperty = property as ConsoleVariableAttributeFloatProperty;
			if(floatProperty != null)
			{
				var floatAttribute = consoleVariableAttribute as ConsoleVariableFloatAttribute;
				float value = floatAttribute.Content.HasValue ? floatAttribute.Content.Value : floatProperty.Content;
				var success = ConsoleVariable.Register(consoleVariableAttribute.Name, value, consoleVariableAttribute.Flags, consoleVariableAttribute.Comments, floatProperty.OnValueChanged, out icvar);
				if(success)
				{
					floatProperty.SetInternalContent(value, icvar);
				}
				return;
			}

			var integer64Property = property as ConsoleVariableAttributeInteger64Property;
			if(integer64Property != null)
			{
				var integer64Attribute = consoleVariableAttribute as ConsoleVariableInteger64Attribute;
				long value = integer64Attribute.Content.HasValue ? integer64Attribute.Content.Value : integer64Property.Content;
				var success = ConsoleVariable.Register(consoleVariableAttribute.Name, value, consoleVariableAttribute.Flags, consoleVariableAttribute.Comments, integer64Property.OnValueChanged, out icvar);
				if(success)
				{
					integer64Property.SetInternalContent(value, icvar);
				}
			}
		}

		public static ConsoleVariableAttributeIntegerProperty CreateConsoleVariableIntegerProperty(string name, int value, string comments, uint flags)
		{
			return new ConsoleVariableAttributeIntegerProperty(name, value, comments, flags);
		}

		public static ConsoleVariableAttributeInteger64Property CreateConsoleVariableInteger64Property(string name, long value, string comments, uint flags)
		{
			return new ConsoleVariableAttributeInteger64Property(name, value, comments, flags);
		}

		public static ConsoleVariableAttributeStringProperty CreateConsoleVariableStringProperty(string name, string value, string comments, uint flags)
		{
			return new ConsoleVariableAttributeStringProperty(name, value, comments, flags);
		}

		public static ConsoleVariableAttributeFloatProperty CreateConsoleVariableFloatProperty(string name, float value, string comments, uint flags)
		{
			return new ConsoleVariableAttributeFloatProperty(name, value, comments, flags);
		}
	}

	/// <summary>
	/// 
	/// </summary>
	public class ConsoleVariableFloatAttribute : ConsoleVariableAttribute
	{
		public ConsoleVariableFloatAttribute(string commandName, float value, uint commandFlag, string commandHelp) : base(commandName, commandFlag, commandHelp)
		{
			Content = value;
		}

		public float? Content
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// 
	/// </summary>
	public class ConsoleVariableInteger64Attribute : ConsoleVariableAttribute
	{
		public ConsoleVariableInteger64Attribute(string commandName, long value, uint commandFlag, string commandHelp) : base(commandName, commandFlag, commandHelp)
		{
			Content = value;
		}

		public long? Content
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// 
	/// </summary>
	public class ConsoleVariableIntegerAttribute : ConsoleVariableAttribute
	{
		public ConsoleVariableIntegerAttribute(string commandName, int value, uint commandFlag, string commandHelp) : base(commandName, commandFlag, commandHelp)
		{
			Content = value;
		}

		public int? Content
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// 
	/// </summary>
	public class ConsoleVariableStringAttribute : ConsoleVariableAttribute
	{
		public ConsoleVariableStringAttribute(string commandName, string value, uint commandFlag, string commandHelp) : base(commandName, commandFlag, commandHelp)
		{
			Content = value;
		}

		public string Content
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// The base class for Console Variable attributes. Cannot be used by user
	/// </summary>
	[AttributeUsage(AttributeTargets.Field, AllowMultiple = false, Inherited = true)]
	public abstract class ConsoleVariableAttribute : Attribute
	{
		protected ConsoleVariableAttribute(string commandName, uint commandFlag, string commandHelp)
		{
			Name = commandName;
			Flags = commandFlag;
			Comments = commandHelp;
		}

		public string Name
		{
			get;
			private set;
		}

		public string Comments
		{
			get;
			private set;
		}

		public uint Flags
		{
			get;
			private set;
		}
	}

	/// <summary>
	/// 
	/// </summary>
	public class ConsoleVariableAttributeFloatProperty : IDisposable
	{
		public ConsoleVariableAttributeFloatProperty()
		{
		}

		internal ConsoleVariableAttributeFloatProperty(string name, float value, string comments, uint flags)
		{
			ICVar icVar = null;
			var registered = ConsoleVariable.Register(name, value, flags, comments, OnValueChanged, out icVar);
			if(!registered)
			{
				throw new ConsoleVariableConfigurationException(GetType() + " cannot be created for attribute name " + name);
			}
			SetInternalContent(value, icVar);
		}

		~ConsoleVariableAttributeFloatProperty()
		{
			Dispose(false);
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		public float Content
		{
			get
			{
				return m_value;
			}
			set
			{
				NotifyOnChanged(value);
			}
		}

		protected virtual void Dispose(bool isDisposing)
		{
			if(m_disposed) return;
			//unregister from c++ 
			if(m_icVar != null)
			{
				ConsoleVariable.UnRegister(ref m_icVar);
			}
			m_disposed = true;
		}

		internal void OnValueChanged(ICVar var)
		{
			m_value = var.GetFVal();
		}

		internal void NotifyOnChanged(float newValue)
		{
			if(m_icVar == null) return;
			m_icVar.Set(newValue);
		}

		public override string ToString()
		{
			return "ConsoleVariableAttributeFloatProperty[" + Content + "]";
		}

		// does not activate managed console variable listener. Called when console variable has been successfully registered natively
		internal void SetInternalContent(float newValue, ICVar icVar)
		{
			m_value = newValue;
			m_icVar = icVar;
		}

		private float m_value;
		private ICVar m_icVar;
		private bool m_disposed;
	}


	/// <summary>
	/// 
	/// </summary>
	public class ConsoleVariableAttributeInteger64Property : IDisposable
	{
		public ConsoleVariableAttributeInteger64Property()
		{
			m_valueInText = new StringBuilder(20); //reserve capacity to hold 64-bit integer
		}

		internal ConsoleVariableAttributeInteger64Property(string name, long value, string comments, uint flags) : this()
		{
			ICVar icVar = null;
			var registered = ConsoleVariable.Register(name, value, flags, comments, OnValueChanged, out icVar);
			if(!registered)
			{
				throw new ConsoleVariableConfigurationException(GetType() + " cannot be created for attribute name " + name);
			}
			SetInternalContent(value, icVar);
		}

		~ConsoleVariableAttributeInteger64Property()
		{
			Dispose(false);
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		public long Content
		{
			get
			{
				return m_value;
			}
			set
			{
				NotifyOnChanged(value);
			}
		}

		protected virtual void Dispose(bool isDisposing)
		{
			if(m_disposed) return;
			if(m_icVar != null)
			{
				ConsoleVariable.UnRegister(ref m_icVar);
			}
			if(isDisposing)
			{
				m_valueInText.Clear();
				m_valueInText = null;
			}
			m_disposed = true;
		}

		internal void OnValueChanged(ICVar var)
		{
			m_value = var.GetI64Val();
		}

		internal void NotifyOnChanged(long newValue)
		{
			if(m_icVar == null) return;
			m_valueInText.Clear();
			m_valueInText.Append(newValue);
			m_icVar.Set(m_valueInText.ToString());
		}

		public override string ToString()
		{
			return "ConsoleVariableAttributeInteger64Property[" + Content + "]";
		}

		// does not activate managed console variable listener. Called when console variable has been successfully registered natively
		internal void SetInternalContent(long newValue, ICVar icVar)
		{
			m_value = newValue;
			m_icVar = icVar;
		}

		private long m_value;
		private ICVar m_icVar;
		private StringBuilder m_valueInText;
		private bool m_disposed;
	}

	/// <summary>
	/// 
	/// </summary>
	public class ConsoleVariableAttributeIntegerProperty : IDisposable
	{
		public ConsoleVariableAttributeIntegerProperty()
		{
		}

		internal ConsoleVariableAttributeIntegerProperty(string name, int value, string comments, uint flags)
		{
			ICVar icVar = null;
			var registered = ConsoleVariable.Register(name, value, flags, comments, OnValueChanged, out icVar);
			if(!registered)
			{
				throw new ConsoleVariableConfigurationException(GetType() + " cannot be created for attribute name " + name);
			}
			SetInternalContent(value, icVar);
		}


		~ConsoleVariableAttributeIntegerProperty()
		{
			Dispose(false);
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		public int Content
		{
			get
			{
				return m_value;
			}
			set
			{
				NotifyOnChanged(value);
			}
		}

		protected virtual void Dispose(bool isDisposing)
		{
			if(m_disposed) return;
			if(m_icVar != null)
			{
				ConsoleVariable.UnRegister(ref m_icVar);
			}
			m_disposed = true;
		}

		internal void OnValueChanged(ICVar var)
		{
			m_value = var.GetIVal();
		}

		internal void NotifyOnChanged(int newValue)
		{
			if(m_icVar == null) return;
			m_icVar.Set(newValue);
		}

		public override string ToString()
		{
			return "ConsoleVariableAttributeIntegerProperty[" + Content + "]";
		}

		// does not activate managed console variable listener. Called when console variable has been successfully registered natively
		internal void SetInternalContent(int newValue, ICVar icVar)
		{
			m_value = newValue;
			m_icVar = icVar;
		}

		private int m_value;
		private ICVar m_icVar;
		private bool m_disposed;
	}

	/// <summary>
	/// 
	/// </summary>
	public class ConsoleVariableAttributeStringProperty : IDisposable
	{
		public ConsoleVariableAttributeStringProperty()
		{
		}

		internal ConsoleVariableAttributeStringProperty(string name, string value, string comments, uint flags)
		{
			ICVar icVar = null;
			var registered = ConsoleVariable.Register(name, value, flags, comments, OnValueChanged, out icVar);
			if(!registered)
			{
				throw new ConsoleVariableConfigurationException(GetType() + " cannot be created for attribute name " + name);
			}
			SetInternalContent(value, icVar);
		}

		~ConsoleVariableAttributeStringProperty()
		{
			Dispose(false);
		}

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		public string Content
		{
			get
			{
				return m_value;
			}
			set
			{
				NotifyOnChanged(value);
			}
		}

		protected virtual void Dispose(bool isDisposing)
		{
			if(m_disposed) return;
			if(m_icVar != null)
			{
				ConsoleVariable.UnRegister(ref m_icVar);
			}
			if(isDisposing)
			{
				m_value = null;
			}
			m_disposed = true;
		}

		internal void OnValueChanged(ICVar var)
		{
			m_value = var.GetString();
		}

		internal void NotifyOnChanged(string newValue)
		{
			if(m_icVar == null) return;
			m_icVar.Set(newValue);
		}

		public override string ToString()
		{
			return "ConsoleVariableAttributeStringProperty[" + Content + "]";
		}

		// does not activate managed console variable listener. Called when console variable has been successfully registered natively
		internal void SetInternalContent(string newValue, ICVar icVar)
		{
			m_value = newValue;
			m_icVar = icVar;
		}

		private string m_value;
		private ICVar m_icVar;
		private bool m_disposed;
	}

	/// <summary>
	/// Thrown when Console Variable Attributes are configured wrongly
	/// </summary>
	public sealed class ConsoleVariableConfigurationException : Exception
	{
		public ConsoleVariableConfigurationException(string msg) : base(msg)
		{
		}
	}
}