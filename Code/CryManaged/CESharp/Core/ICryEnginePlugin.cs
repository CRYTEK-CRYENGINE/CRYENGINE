using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace CryEngine
{
    /// <summary>
    /// Represents the entry point of a C# CRYENGINE plug-in
    /// Only one type per assembly may implement this
    /// An instance will be created shortly after the assembly has been loaded.
    /// </summary>
    public abstract class ICryEnginePlugin
    {
        /// <summary>
        /// Called immediately after the plug-in was loaded to initialize it
        /// </summary>
        public virtual void Initialize() { }
        /// <summary>
        /// Called when plug-in is being unloaded, usually at shutdown
        /// </summary>
        public virtual void Shutdown() { }

        /// <summary>
        /// Called after a level has been loaded
        /// </summary>
        public virtual void OnLevelLoaded() { }

        /// <summary>
        /// Called after the level has been loaded and gameplay can start
        /// </summary>
        public virtual void OnGameStart() { }

        /// <summary>
        /// Called on level unload or when gameplay stops
        /// </summary>
        public virtual void OnGameStop() { }

        /// <summary>
        /// Sent when a new client connects to the server
        /// This can be used to create entities for each player, or to keep references of them in game code.
        /// </summary>
        /// <param name="channelId">Identifier for the network channel between the server and this client</param>
        public virtual void OnClientConnectionReceived(int channelId) { }

        /// <summary>
        /// Sent to the server after a successful client connection when they are ready for gameplay to start.
        /// </summary>
        /// <param name="channelId"></param>
        public virtual void OnClientReadyForGameplay(int channelId) { }

        /// <summary>
        /// Sent when a client disconnects from the server
        /// </summary>
        /// <param name="channelId"></param>
        public virtual void OnClientDisconnected(int channelId) { }
    }
}
