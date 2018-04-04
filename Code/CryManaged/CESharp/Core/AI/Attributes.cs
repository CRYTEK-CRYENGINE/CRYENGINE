using System;

namespace CryEngine
{
	[AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = false)]
	public sealed class BehaviorTreeNodeAttribute : Attribute
	{
		#region Fields
		private readonly string _name;
		private readonly bool _register;
		#endregion

		#region Properties
		public string Name { get { return _name; } }
		public bool Register { get { return _register; } } 
		#endregion

		#region Constructors
		public BehaviorTreeNodeAttribute(string name, bool register = true)
		{
			_name = name;
			_register = register;
		} 
		#endregion
	}
}