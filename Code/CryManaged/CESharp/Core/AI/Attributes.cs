using System;

namespace CryEngine
{
	/// <summary>
	/// Attribute for Behavior Tree nodes.
	/// </summary>
	[AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = false)]
	public sealed class BehaviorTreeNodeAttribute : Attribute
	{
		#region Fields
		private readonly string _name;
		private readonly bool _register;
		#endregion

		#region Properties
		/// <summary>
		/// Name of this node.
		/// </summary>
		public string Name { get { return _name; } }
		/// <summary>
		/// Whether this node should be registered.
		/// </summary>
		public bool Register { get { return _register; } }
		#endregion

		#region Constructors
		/// <summary>
		/// Constructor for a new <c>BehaviorTreeNodeAttribute</c>.
		/// </summary>
		/// <param name="name"></param>
		/// <param name="register"></param>
		public BehaviorTreeNodeAttribute(string name, bool register = true)
		{
			_name = name;
			_register = register;
		} 
		#endregion
	}
}