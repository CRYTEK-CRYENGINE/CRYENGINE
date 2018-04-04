/* AIRAliases.js - Revision: 2.0 */

/*
ADOBE SYSTEMS INCORPORATED
Copyright 2007-2009 Adobe Systems Incorporated. All Rights Reserved.
 
NOTICE:   Adobe permits you to modify and distribute this file only in accordance with
the terms of Adobe AIR SDK license agreement.  You may have received this file from a
source other than Adobe.  Nonetheless, you may modify or
distribute this file only in accordance with such agreement. 
*/


var air;
if (window.runtime)
{
    if (!air) air = {};
    // functions
    air.trace = window.runtime.trace;
    air.navigateToURL = window.runtime.flash.net.navigateToURL;
    air.sendToURL = window.runtime.flash.net.sendToURL;


    // file
    air.File = window.runtime.flash.filesystem.File;
    air.FileStream = window.runtime.flash.filesystem.FileStream;
    air.FileMode = window.runtime.flash.filesystem.FileMode;

    air.StorageVolumeInfo = window.runtime.flash.filesystem.StorageVolumeInfo;

    // events
    air.ActivityEvent = window.runtime.flash.events.ActivityEvent;
    air.AsyncErrorEvent = window.runtime.flash.events.AsyncErrorEvent;
    air.BrowserInvokeEvent = window.runtime.flash.events.BrowserInvokeEvent;
    air.DataEvent = window.runtime.flash.events.DataEvent;
    air.DRMAuthenticateEvent = window.runtime.flash.events.DRMAuthenticateEvent;
    air.DRMStatusEvent = window.runtime.flash.events.DRMStatusEvent;
    air.ErrorEvent = window.runtime.flash.events.ErrorEvent;
    air.Event = window.runtime.flash.events.Event;
    air.EventDispatcher = window.runtime.flash.events.EventDispatcher;
    air.FileListEvent = window.runtime.flash.events.FileListEvent;
    air.HTTPStatusEvent = window.runtime.flash.events.HTTPStatusEvent;
    air.IOErrorEvent = window.runtime.flash.events.IOErrorEvent;
    air.InvokeEvent = window.runtime.flash.events.InvokeEvent;
    air.InvokeEventReason = window.runtime.flash.desktop.InvokeEventReason;
    air.NetStatusEvent = window.runtime.flash.events.NetStatusEvent;
    air.OutputProgressEvent = window.runtime.flash.events.OutputProgressEvent;
    air.ProgressEvent = window.runtime.flash.events.ProgressEvent;
    air.SecurityErrorEvent = window.runtime.flash.events.SecurityErrorEvent;
    air.StatusEvent = window.runtime.flash.events.StatusEvent;
    air.TimerEvent = window.runtime.flash.events.TimerEvent;
    air.SampleDataEvent = window.runtime.flash.events.SampleDataEvent;
    air.DatagramSocketDataEvent = window.runtime.flash.events.DatagramSocketDataEvent;
    air.DNSResolverEvent = window.runtime.flash.events.DNSResolverEvent;
    air.ServerSocketConnectEvent = window.runtime.flash.events.ServerSocketConnectEvent;
    air.StorageVolumeChangeEvent = window.runtime.flash.events.StorageVolumeChangeEvent;
    air.NativeProcessExitEvent = window.runtime.flash.events.NativeProcessExitEvent;
    air.UncaughtErrorEvent = window.runtime.flash.events.UncaughtErrorEvent;
    air.MouseEvent = window.runtime.flash.events.MouseEvent;

    // native window
    air.NativeWindow = window.runtime.flash.display.NativeWindow;
    air.NativeWindowDisplayState = window.runtime.flash.display.NativeWindowDisplayState;
    air.NativeWindowInitOptions = window.runtime.flash.display.NativeWindowInitOptions;
    air.NativeWindowSystemChrome = window.runtime.flash.display.NativeWindowSystemChrome;
    air.NativeWindowResize = window.runtime.flash.display.NativeWindowResize;
    air.NativeWindowType = window.runtime.flash.display.NativeWindowType;

    air.NativeWindowBoundsEvent = window.runtime.flash.events.NativeWindowBoundsEvent;
    air.NativeWindowDisplayStateEvent = window.runtime.flash.events.NativeWindowDisplayStateEvent;

    // geom
    air.Point = window.runtime.flash.geom.Point;
    air.Rectangle = window.runtime.flash.geom.Rectangle;
    air.Matrix = window.runtime.flash.geom.Matrix;

    // 3D
    air.Matrix3D  = window.runtime.flash.geom.Matrix3D;
    air.Vector3D  = window.runtime.flash.geom.Vector3D;
    air.Orientation3D  = window.runtime.flash.geom.Orientation3D;
    air.Utils3D  = window.runtime.flash.geom.Utils3D;
    
    // Shader    
    air.Shader = window.runtime.flash.display.Shader;
    air.ShaderFilter = window.runtime.flash.filters.ShaderFilter;
    air.ShaderPrecision = window.runtime.flash.display.ShaderPrecision;
    
    // net
    air.FileFilter = window.runtime.flash.net.FileFilter;
    
    air.LocalConnection = window.runtime.flash.net.LocalConnection;
    air.NetConnection = window.runtime.flash.net.NetConnection;

    air.URLLoader = window.runtime.flash.net.URLLoader;
    air.URLLoaderDataFormat = window.runtime.flash.net.URLLoaderDataFormat;
    air.URLRequest = window.runtime.flash.net.URLRequest;
    air.URLRequestDefaults = window.runtime.flash.net.URLRequestDefaults;
    air.URLRequestHeader = window.runtime.flash.net.URLRequestHeader;
    air.URLRequestMethod = window.runtime.flash.net.URLRequestMethod;
    air.URLStream = window.runtime.flash.net.URLStream;
    air.URLVariables = window.runtime.flash.net.URLVariables;
    air.Socket = window.runtime.flash.net.Socket;
    air.XMLSocket = window.runtime.flash.net.XMLSocket;

	air.SecureSocket = window.runtime.flash.net.SecureSocket;
	air.CertificateStatus = window.runtime.flash.security.CertificateStatus;

    air.Responder = window.runtime.flash.net.Responder;
    air.ObjectEncoding = window.runtime.flash.net.ObjectEncoding;

    air.NetStream = window.runtime.flash.net.NetStream;
    air.NetStreamInfo = window.runtime.flash.net.NetStreamInfo;
    air.NetStreamPlayOptions = window.runtime.flash.net.NetStreamPlayOptions;
    air.NetStreamPlayTransitions = window.runtime.flash.net.NetStreamPlayTransitions;
    air.SharedObject = window.runtime.flash.net.SharedObject;
    air.SharedObjectFlushStatus = window.runtime.flash.net.SharedObjectFlushStatus;

    air.DatagramSocket = window.runtime.flash.net.DatagramSocket;
    air.NetworkInfo = window.runtime.flash.net.NetworkInfo;
    air.ServerSocket = window.runtime.flash.net.ServerSocket;
    air.IPVersion = window.runtime.flash.net.IPVersion;

    air.DNSResolver = window.runtime.flash.net.dns.DNSResolver;
    air.ARecord = window.runtime.flash.net.dns.ARecord;
    air.AAAARecord = window.runtime.flash.net.dns.AAAARecord;
    air.MXRecord = window.runtime.flash.net.dns.MXRecord;
    air.PTRRecord = window.runtime.flash.net.dns.PTRRecord;
    air.SRVRecord = window.runtime.flash.net.dns.SRVRecord;

    // system
    air.Capabilities = window.runtime.flash.system.Capabilities;
    air.System = window.runtime.flash.system.System;
    air.Security = window.runtime.flash.system.Security;
    air.Updater = window.runtime.flash.desktop.Updater;

    // desktop
    air.Clipboard = window.runtime.flash.desktop.Clipboard;
    air.ClipboardFormats = window.runtime.flash.desktop.ClipboardFormats;
    air.ClipboardTransferMode = window.runtime.flash.desktop.ClipboardTransferMode;

    air.NativeDragManager = window.runtime.flash.desktop.NativeDragManager;
    air.NativeDragOptions = window.runtime.flash.desktop.NativeDragOptions;
    air.NativeDragActions = window.runtime.flash.desktop.NativeDragActions;

    air.Icon = window.runtime.flash.desktop.Icon;
    air.DockIcon = window.runtime.flash.desktop.DockIcon;
    air.InteractiveIcon = window.runtime.flash.desktop.InteractiveIcon;
    air.NotificationType = window.runtime.flash.desktop.NotificationType;
    air.SystemTrayIcon = window.runtime.flash.desktop.SystemTrayIcon;

    air.NativeApplication = window.runtime.flash.desktop.NativeApplication;

    air.NativeProcess = window.runtime.flash.desktop.NativeProcess;
    air.NativeProcessStartupInfo = window.runtime.flash.desktop.NativeProcessStartupInfo;
   
    // display
    air.NativeMenu = window.runtime.flash.display.NativeMenu;
    air.NativeMenuItem = window.runtime.flash.display.NativeMenuItem;
    air.Screen = window.runtime.flash.display.Screen;
    
    air.Loader  = window.runtime.flash.display.Loader;
    air.Bitmap = window.runtime.flash.display.Bitmap;
    air.BitmapData = window.runtime.flash.display.BitmapData;

    // ui
    air.Keyboard = window.runtime.flash.ui.Keyboard;
    air.KeyLocation = window.runtime.flash.ui.KeyLocation;
    air.Mouse = window.runtime.flash.ui.Mouse;


    //security
    air.ReferencesValidationSetting = window.runtime.flash.security.ReferencesValidationSetting;
    air.RevocationCheckSettings = window.runtime.flash.security.RevocationCheckSettings;
    air.SignatureStatus = window.runtime.flash.security.SignatureStatus;
    air.SignerTrustSettings = window.runtime.flash.security.SignerTrustSettings;
    air.XMLSignatureValidator = window.runtime.flash.security.XMLSignatureValidator;
        

    // utils
    air.ByteArray = window.runtime.flash.utils.ByteArray;
    air.CompressionAlgorithm = window.runtime.flash.utils.CompressionAlgorithm;
    air.Endian = window.runtime.flash.utils.Endian;
    air.Timer = window.runtime.flash.utils.Timer;

    air.HTMLLoader = window.runtime.flash.html.HTMLLoader;
    air.HTMLPDFCapability = window.runtime.flash.html.HTMLPDFCapability;    

    air.Vector = window.runtime.Vector;    

    // media
    air.ID3Info = window.runtime.flash.media.ID3Info;
    air.Sound = window.runtime.flash.media.Sound;
    air.SoundChannel = window.runtime.flash.media.SoundChannel;
    air.SoundLoaderContext = window.runtime.flash.media.SoundLoaderContext;
    air.SoundMixer = window.runtime.flash.media.SoundMixer;
    air.SoundTransform = window.runtime.flash.media.SoundTransform;
    air.Microphone = window.runtime.flash.media.Microphone;
    air.Video = window.runtime.flash.media.Video;
    air.Camera = window.runtime.flash.media.Camera;
    air.SoundCodec = window.runtime.flash.media.SoundCodec;

    // data
    air.EncryptedLocalStore = window.runtime.flash.data.EncryptedLocalStore;
    air.SQLCollationType = window.runtime.flash.data.SQLCollationType;
    air.SQLColumnNameStyle = window.runtime.flash.data.SQLColumnNameStyle;
    air.SQLColumnSchema = window.runtime.flash.data.SQLColumnSchema;
    air.SQLConnection = window.runtime.flash.data.SQLConnection;
    air.SQLError = window.runtime.flash.errors.SQLError;
    air.SQLErrorEvent = window.runtime.flash.events.SQLErrorEvent;
    air.SQLErrorOperation = window.runtime.flash.errors.SQLErrorOperation;
    air.SQLEvent = window.runtime.flash.events.SQLEvent;
    air.SQLIndexSchema = window.runtime.flash.data.SQLIndexSchema;
    air.SQLMode = window.runtime.flash.data.SQLMode;
    air.SQLResult = window.runtime.flash.data.SQLResult;
    air.SQLSchema = window.runtime.flash.data.SQLSchema;
    air.SQLSchemaResult = window.runtime.flash.data.SQLSchemaResult;
    air.SQLStatement = window.runtime.flash.data.SQLStatement;
    air.SQLTableSchema = window.runtime.flash.data.SQLTableSchema;
    air.SQLTransactionLockType = window.runtime.flash.data.SQLTransactionLockType;
    air.SQLTriggerSchema = window.runtime.flash.data.SQLTriggerSchema;
    air.SQLUpdateEvent = window.runtime.flash.events.SQLUpdateEvent;
    air.SQLViewSchema = window.runtime.flash.data.SQLViewSchema;

    // core framework
    air.__defineGetter__("ServiceMonitor", function() { return window.runtime.air.net.ServiceMonitor; });
    air.__defineGetter__("SocketMonitor", function() { return window.runtime.air.net.SocketMonitor; });
    air.__defineGetter__("URLMonitor", function() { return window.runtime.air.net.URLMonitor; });
    air.__defineGetter__("SecureSocketMonitor", function() { return window.runtime.air.net.SecureSocketMonitor; });
    air.__defineGetter__("URLFilePromise", function() { return window.runtime.air.desktop.URLFilePromise; });

    // update framework
    air.__defineGetter__("ApplicationUpdater", function() { return window.runtime.air.update.ApplicationUpdater; });
    air.__defineGetter__("ApplicationUpdaterUI", function() { return window.runtime.air.update.ApplicationUpdaterUI; });
    air.__defineGetter__("UpdateEvent", function() { return window.runtime.air.update.events.UpdateEvent; });
    air.__defineGetter__("StatusUpdateEvent", function() { return window.runtime.air.update.events.StatusUpdateEvent; });
    air.__defineGetter__("StatusUpdateErrorEvent", function() { return window.runtime.air.update.events.StatusUpdateErrorEvent; });
    air.__defineGetter__("DownloadErrorEvent", function() { return window.runtime.air.update.events.DownloadErrorEvent; });
    air.__defineGetter__("StatusFileUpdateEvent", function() { return window.runtime.air.update.events.StatusFileUpdateEvent; });
    air.__defineGetter__("StatusFileUpdateErrorEvent", function() { return window.runtime.air.update.events.StatusFileUpdateErrorEvent; });
}
