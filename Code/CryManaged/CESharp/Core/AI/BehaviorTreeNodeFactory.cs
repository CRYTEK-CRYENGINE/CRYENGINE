using System;
using System.Collections.Generic;
using System.Linq;
using CryEngine.Common;
using CryEngine.Common.BehaviorTree;

namespace CryEngine
{
	/// <summary>
	/// Creator for a specific node type.
	/// </summary>
	public class GenericNodeCreator : IManagedNodeCreator
	{
		#region Fields
		private static readonly List<Node> _nodes = new List<Node>();
		private readonly Type _nodeType;
		#endregion

		#region Properties
		/// <summary>
		/// The type of node that this creater handles.
		/// </summary>
		public Type NodeType { get { return _nodeType; } }
		#endregion

		#region Constructors
		/// <summary>
		/// Constructor for a node creator.
		/// </summary>
		/// <param name="nodeType"></param>
		public GenericNodeCreator(Type nodeType)
		{
			_nodeType = nodeType;
		}
		#endregion

		#region Methods
		/// <summary>
		/// Creates a new instance of this creator's node type.
		/// </summary>
		/// <returns></returns>
		public override Node Create()
		{
			var node = (Node)Activator.CreateInstance(_nodeType);
			_nodes.Add(node); // keep a reference to the node to prevent it from being garbage collected.
			return node;
		}


		#endregion
	}

	/// <summary>
	/// Factory for behavior tree nodes.
	/// </summary>
	public static class BehaviorTreeNodeFactory
	{
		private static readonly Dictionary<string, GenericNodeCreator> _registry = new Dictionary<string, GenericNodeCreator>();

		/// <summary>
		/// Registers a node of the specified type.
		/// </summary>
		/// <param name="type"></param>
		public static void TryRegister(Type type)
		{
			string name = type.Name;
			var behaviorNodeAttribute = (BehaviorTreeNodeAttribute)type.GetCustomAttributes(typeof(BehaviorTreeNodeAttribute), true).FirstOrDefault();
			if(behaviorNodeAttribute != null)
			{
				if(!behaviorNodeAttribute.Register)
				{
					return;
				}

				if(!string.IsNullOrEmpty(behaviorNodeAttribute.Name))
				{
					name = behaviorNodeAttribute.Name;
				}
			}

			if(_registry.ContainsKey(name))
			{
				return;
			}
			if(_registry.Values.Any(x => x.NodeType == type))
			{
				return;
			}

			var creator = new GenericNodeCreator(type);
			_registry.Add(name, creator);
			Global.gEnv.pMonoRuntime.RegisterManagedNodeCreator(name, creator);
		}
	}
}