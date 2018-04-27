using System.Collections.Generic;
using CryEngine.Common;
using CryEngine.Common.BehaviorTree;

namespace CryEngine
{
	/// <summary>
	/// Context of a behavior tree.
	/// </summary>
	public class BehaviorTreeContext
	{
		#region Fields
		private uint _nodeId;
		private uint _entityId;
		private readonly UpdateContext _updateContext;
		#endregion

		#region Properties
		/// <summary>
		/// ID of the node.
		/// </summary>
		public uint NodeId { get { return _nodeId; } }
		/// <summary>
		/// ID of the entity.
		/// </summary>
		public uint EntityId { get { return _entityId; } }
		#endregion

		#region Constructors
		internal BehaviorTreeContext(uint nodeId, UpdateContext updateContext)
		{
			_nodeId = nodeId;
			_updateContext = updateContext;
			_entityId = updateContext.entityId;
		}

		internal BehaviorTreeContext(uint nodeId, EventContext eventContext)
		{
			_nodeId = nodeId;
			_entityId = eventContext.entityId;
		}
		#endregion

		#region Methods
		/// <summary>
		/// Adds the message to the behavior log.
		/// </summary>
		/// <param name="message"></param>
		public void Log(string message)
		{
#if(!RELEASE && (WIN32 || WIN64))
			if(_updateContext != null)
			{
				_updateContext.behaviorLog.AddMessage(message);
			}
#endif
		}
		#endregion
	}

	/// <summary>
	/// Generic context of a behavior tree.
	/// </summary>
	/// <typeparam name="TRuntimeData"></typeparam>
	public class BehaviorTreeContext<TRuntimeData> : BehaviorTreeContext
	{
		#region Fields
		private TRuntimeData _runtimeData;
		#endregion

		#region Properties
		/// <summary>
		/// The generic runtime data of the context.
		/// </summary>
		public TRuntimeData RuntimeData { get { return _runtimeData; } }
		#endregion

		#region Constructors
		internal BehaviorTreeContext(uint nodeId, TRuntimeData runtimeData, UpdateContext updateContext) : base(nodeId, updateContext)
		{
			_runtimeData = runtimeData;
		}

		internal BehaviorTreeContext(uint nodeId, TRuntimeData runtimeData, EventContext eventContext) : base(nodeId, eventContext)
		{
			_runtimeData = runtimeData;
		}
		#endregion
	}

	/// <summary>
	/// Internal base class of a behavior tree node.
	/// </summary>
	public abstract class BehaviorTreeNodeBase : NodeProxy
	{
		internal BehaviorTreeNodeBase() { } // Make sure only types inside the Core assembly can inherit from it 
	}

	/// <summary>
	/// Base class for a behavior tree node.
	/// </summary>
	public abstract class BehaviorTreeNode : BehaviorTreeNodeBase
	{
		#region Static fields
		private static uint _nodeIdCount = 0;
		#endregion

		#region Fields
		private HashSet<ulong> _initialized = new HashSet<ulong>();
		private uint _nodeId;
		private string _startLog;
		private string _successLog;
		private string _failureLog;
		private string _interruptLog;
		#endregion

		#region Properties
		/// <summary>
		/// ID of this node.
		/// </summary>
		public uint NodeId { get { return _nodeId; } }
		#endregion

		#region Constructors
		/// <summary>
		/// Constructor for a base behavior tree node.
		/// </summary>
		protected BehaviorTreeNode()
		{
			_nodeId = _nodeIdCount++;
		}
		#endregion

		#region Methods
		/// <summary>
		/// Updates this node one tick.
		/// </summary>
		/// <param name="unmodifiedContext"></param>
		/// <returns></returns>
		public sealed override Status Tick(UpdateContext unmodifiedContext)
		{
			Status returnStatus = Status.Running;

			var runtimeDataId = MakeRuntimeDataId(unmodifiedContext.entityId, _nodeId);
			var updateContext = new BehaviorTreeContext(_nodeId, unmodifiedContext);
			if(!_initialized.Contains(runtimeDataId))
			{
				if(!string.IsNullOrEmpty(_startLog))
				{
					updateContext.Log(_startLog);
				}
				returnStatus = OnInitialize(updateContext) ? Status.Running : Status.Failure;
			}

			if(returnStatus == Status.Running)
			{
				returnStatus = OnUpdate(updateContext);
			}

			if(returnStatus != Status.Running)
			{
				OnTerminate(updateContext);
				_initialized.Remove(runtimeDataId);
			}

			if(returnStatus == Status.Success && !string.IsNullOrEmpty(_successLog))
			{
				updateContext.Log(_successLog);
			}
			else if(returnStatus == Status.Failure && !string.IsNullOrEmpty(_failureLog))
			{
				updateContext.Log(_failureLog);
			}

			return returnStatus;
		}

		/// <summary>
		/// Terminate this node.
		/// </summary>
		/// <param name="unmodifiedContext"></param>
		public sealed override void Terminate(UpdateContext unmodifiedContext)
		{
			var runtimeDataId = MakeRuntimeDataId(unmodifiedContext.entityId, _nodeId);
			if(_initialized.Contains(runtimeDataId))
			{
				var updateContext = new BehaviorTreeContext(_nodeId, unmodifiedContext);
				OnTerminate(updateContext, true);
				_initialized.Remove(runtimeDataId);

				if(!string.IsNullOrEmpty(_interruptLog))
				{
					updateContext.Log(_interruptLog);
				}
			}
		}

		/// <summary>
		/// Handles events for this node.
		/// </summary>
		/// <param name="context"></param>
		/// <param name="arg1"></param>
		public sealed override void HandleEvent(EventContext context, Event arg1)
		{
			var runtimeDataId = MakeRuntimeDataId(context.entityId, _nodeId);
			if(_initialized.Contains(runtimeDataId))
			{
				var updateContext = new BehaviorTreeContext(_nodeId, context);
				HandleEvent(updateContext, arg1);
			}
		}

		/// <summary>
		/// Loads data from an XML node.
		/// </summary>
		/// <param name="xml"></param>
		/// <param name="context"></param>
		/// <returns></returns>
		public sealed override LoadResult LoadFromXml(XmlNodeRef xml, LoadContext context)
		{
			_startLog = xml.getAttr("_startLog");
			_successLog = xml.getAttr("_successLog");
			_failureLog = xml.getAttr("_failureLog");
			_interruptLog = xml.getAttr("_interruptLog");

			return OnLoadFromXml(xml, context);
		}
		
		private static ulong MakeRuntimeDataId(uint entityId, uint nodeId)
		{
			return entityId | ((ulong)nodeId) << 32;
		}

		/// <summary>
		/// Called when this node is initialized.
		/// </summary>
		/// <param name="updateContext"></param>
		/// <returns></returns>
		protected virtual bool OnInitialize(BehaviorTreeContext updateContext) { return true; }

		/// <summary>
		/// Called when this node is being updated.
		/// </summary>
		/// <param name="updateContext"></param>
		/// <returns></returns>
		protected virtual Status OnUpdate(BehaviorTreeContext updateContext) { return Status.Success; }

		/// <summary>
		/// Called when this node is being terminated.
		/// </summary>
		/// <param name="updateContext"></param>
		/// <param name="interrupted"></param>
		protected virtual void OnTerminate(BehaviorTreeContext updateContext, bool interrupted = false) { }

		/// <summary>
		/// Called when this node is loading from an XML node.
		/// </summary>
		/// <param name="xml"></param>
		/// <param name="context"></param>
		/// <returns></returns>
		protected virtual LoadResult OnLoadFromXml(XmlNodeRef xml, LoadContext context) { return LoadResult.LoadSuccess; }
		
		/// <summary>
		/// Called when this node needs to handle an event.
		/// </summary>
		/// <param name="updateContext"></param>
		/// <param name="e"></param>
		protected virtual void HandleEvent(BehaviorTreeContext updateContext, Event e) { }
		#endregion
	}

	/// <summary>
	/// Generic base for a behavior tree node.
	/// </summary>
	/// <typeparam name="TRuntimeData"></typeparam>
	public abstract class BehaviorTreeNode<TRuntimeData> : BehaviorTreeNodeBase where TRuntimeData : class, new()
	{
		#region Static fields
		private static Dictionary<ulong, TRuntimeData> _runtimeData = new Dictionary<ulong, TRuntimeData>();
		// FIXME Is this static field in generic type intended? Add pragma if it's intended, otherwise save the _nodeIdCount in BahviorTreeNodeBase.
		private static uint _nodeIdCount = 0;
		#endregion

		#region Fields
		private uint _nodeId;
		private string _startLog;
		private string _successLog;
		private string _failureLog;
		private string _interruptLog;
		#endregion

		#region Properties
		/// <summary>
		/// ID of this node.
		/// </summary>
		public uint NodeId { get { return _nodeId; } }
		#endregion

		#region Constructors
		/// <summary>
		/// Constructor for a generic behavior tree node.
		/// </summary>
		protected BehaviorTreeNode()
		{
			_nodeId = _nodeIdCount++;
		}
		#endregion

		#region Methods
		/// <summary>
		/// Updates this node one tick.
		/// </summary>
		/// <param name="unmodifiedContext"></param>
		/// <returns></returns>
		public sealed override Status Tick(UpdateContext unmodifiedContext)
		{
			Status returnStatus = Status.Running;

#if(!RELEASE && (WIN32 || WIN64))
			unmodifiedContext.debugTree.Push(this);
#endif

			var runtimeDataId = MakeRuntimeDataId(unmodifiedContext.entityId, _nodeId);
			bool needsToBeInitialized = false;
			var runtimeData = GetRuntimeData(runtimeDataId, out needsToBeInitialized);
			var updateContext = new BehaviorTreeContext<TRuntimeData>(_nodeId, runtimeData, unmodifiedContext);
			if(needsToBeInitialized)
			{
				if(!string.IsNullOrEmpty(_startLog))
				{
					updateContext.Log(_startLog);
				}
				returnStatus = OnInitialize(updateContext) ? Status.Running : Status.Failure;
			}

			if(returnStatus == Status.Running)
			{
				returnStatus = OnUpdate(updateContext);
			}

			if(returnStatus != Status.Running)
			{
				OnTerminate(updateContext);
				_runtimeData.Remove(runtimeDataId);
			}

			if(returnStatus == Status.Success && !string.IsNullOrEmpty(_successLog))
			{
				updateContext.Log(_successLog);
			}
			else if(returnStatus == Status.Failure && !string.IsNullOrEmpty(_failureLog))
			{
				updateContext.Log(_failureLog);
			}

#if(!RELEASE && (WIN32 || WIN64))
			unmodifiedContext.debugTree.Pop(returnStatus);
#endif


			return returnStatus;
		}

		/// <summary>
		/// Terminates this node.
		/// </summary>
		/// <param name="unmodifiedContext"></param>
		public sealed override void Terminate(UpdateContext unmodifiedContext)
		{
			var runtimeDataId = MakeRuntimeDataId(unmodifiedContext.entityId, _nodeId);
			if(_runtimeData.ContainsKey(runtimeDataId))
			{
				TRuntimeData runtimeData = _runtimeData[runtimeDataId];
				var updateContext = new BehaviorTreeContext<TRuntimeData>(_nodeId, runtimeData, unmodifiedContext);
				OnTerminate(updateContext, true);
				_runtimeData.Remove(runtimeDataId);

				if(!string.IsNullOrEmpty(_interruptLog))
				{
					updateContext.Log(_interruptLog);
				}
			}
		}

		/// <summary>
		/// Handles events for this node.
		/// </summary>
		/// <param name="context"></param>
		/// <param name="arg1"></param>
		public sealed override void HandleEvent(EventContext context, Event arg1)
		{
			var runtimeDataId = MakeRuntimeDataId(context.entityId, _nodeId);
			if(_runtimeData.ContainsKey(runtimeDataId))
			{
				TRuntimeData runtimeData = _runtimeData[runtimeDataId];
				var updateContext = new BehaviorTreeContext<TRuntimeData>(_nodeId, runtimeData, context);
				HandleEvent(updateContext, arg1);
			}
		}

		/// <summary>
		/// Loads data from an XML node.
		/// </summary>
		/// <param name="xml"></param>
		/// <param name="context"></param>
		/// <returns></returns>
		public sealed override LoadResult LoadFromXml(XmlNodeRef xml, LoadContext context)
		{
			_startLog = xml.getAttr("_startLog");
			_successLog = xml.getAttr("_successLog");
			_failureLog = xml.getAttr("_failureLog");
			_interruptLog = xml.getAttr("_interruptLog");

			return OnLoadFromXml(xml, context);
		}

		private static ulong MakeRuntimeDataId(uint entityId, uint nodeId)
		{
			return entityId | ((ulong)nodeId) << 32;
		}

		private static TRuntimeData GetRuntimeData(ulong runtimeDataId, out bool justCreated)
		{
			justCreated = false;
			if(!_runtimeData.ContainsKey(runtimeDataId))
			{
				_runtimeData.Add(runtimeDataId, new TRuntimeData());
				justCreated = true;
			}
			return _runtimeData[runtimeDataId];
		}

		/// <summary>
		/// Called when this node is initialized.
		/// </summary>
		/// <param name="updateContext"></param>
		/// <returns></returns>
		protected virtual bool OnInitialize(BehaviorTreeContext<TRuntimeData> updateContext) { return true; }
		
		/// <summary>
		/// Called when this node is being updated.
		/// </summary>
		/// <param name="updateContext"></param>
		/// <returns></returns>
		protected virtual Status OnUpdate(BehaviorTreeContext<TRuntimeData> updateContext) { return Status.Success; }
		
		/// <summary>
		/// Called when this node is being terminated.
		/// </summary>
		/// <param name="updateContext"></param>
		/// <param name="interrupted"></param>
		protected virtual void OnTerminate(BehaviorTreeContext<TRuntimeData> updateContext, bool interrupted = false) { }

		/// <summary>
		/// Called when this node is loading from an XML node.
		/// </summary>
		/// <param name="xml"></param>
		/// <param name="context"></param>
		/// <returns></returns>
		protected virtual LoadResult OnLoadFromXml(XmlNodeRef xml, LoadContext context) { return LoadResult.LoadSuccess; }

		/// <summary>
		/// Called when this node needs to handle an event.
		/// </summary>
		/// <param name="updateContext"></param>
		/// <param name="e"></param>
		protected virtual void HandleEvent(BehaviorTreeContext<TRuntimeData> updateContext, Event e) { }
		#endregion
	}
}