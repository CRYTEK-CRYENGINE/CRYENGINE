using System;
using System.Collections.Generic;
using System.Linq;
using CryEngine.Common;
using CryEngine.Common.BehaviorTree;

namespace CryEngine
{
	public class GenericNodeCreator : IManagedNodeCreator
	{
		#region Fields
		private static readonly List<Node> _nodes = new List<Node>();
		private readonly Type _nodeType;
		#endregion

		#region Properties
		public Type NodeType { get { return _nodeType; } }
		#endregion

		#region Constructors
		public GenericNodeCreator(Type nodeType)
		{
			_nodeType = nodeType;
		}
		#endregion

		#region Methods
		public override Node Create()
		{
			var node = (Node)Activator.CreateInstance(_nodeType);
			_nodes.Add(node); // keep a reference to the node to prevent it from being garbage collected.
			return node;
		}


		#endregion
	}

	public static class BehaviorTreeNodeFactory
	{
		private static readonly Dictionary<string, GenericNodeCreator> _registry = new Dictionary<string, GenericNodeCreator>();

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