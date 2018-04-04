//
// FTScript.jsx
//
// $Id: FTScriptCS3.jsx,v 1.8 2011/05/11 02:46:10 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

FTScript = function() {};

//================================= Bridge  ==================================

FTScript.DELAY = 2000;    // amount of time to wait to relaunch PS after
                          // it has been shutdown
FTScript.TIMEOUT = 1000 * 120;

FTScript.INI_FILE = "~/ftscript.ini"

//
// FTScript.runBridge
//   This function is called as part of the restart processing. When this
//   function is called, Photoshop has just been shut down. After a brief
//   pause, this function retarts Photoshop and send the FTScript.runPS
//   call to Photoshop to cause the next process block to happen.
//
FTScript.runBridge = function(mode) {
  function writeln(m) {
    // $.writeln(new Date() + ": " + m);
  }
  try {
    //var psSpec = FTScript.getAppSpecifier("photoshop");
    var psSpec = "photoshop";

    writeln("FTScript.runBridge");

    if (FTScript.isCS3() || FTScript.isCS4()) {
      // photoshop10.quit();
    }

    var timer = FTScript.DELAY;

    writeln("shutting down PS");
    writeln("Photoshop is " +
            (BridgeTalk.isRunning(psSpec) ? "" : "not ")
            + "running");

    while (BridgeTalk.isRunning(psSpec)) {
      writeln("waiting...");
      $.sleep(FTScript.DELAY);

      if (timer > FTScript.TIMEOUT) {
        var msg = "Photoshop failed to shutdown";
        writeln(msg);
        throw msg;
      }
    }

    if (mode.constructor != String) {
      mode = 'start';
    }

    if (!BridgeTalk.isRunning(psSpec)) {
      writeln("Photoshop is not running");

      if (BridgeTalk.launch) {
        writeln("doing launch");
        var i = 5;
        while (!BridgeTalk.isRunning(psSpec)) {
          writeln("launch...");
          BridgeTalk.launch(psSpec);
          $.sleep(FTScript.DELAY);
          if (!(i--)) {
            alert("Failed to launch " + psSpec);
          }
        }
        if (!BridgeTalk.isRunning(psSpec)) {
          alert("Photoshop is not running");
          return false;
        }
      } else if (!startTargetApplication(psSpec)) {
        alert("Failed to launch " + psSpec);
        return false;
      }

      var btMessage = new BridgeTalk();
      btMessage.target = "photoshop";
      btMessage.body = "'started';";
      btMessage.send();
    }
    writeln("Photoshop started");

    var brCode = FTScript._implementationString() + " ";

    brCode += "FTScript.runPS('" + mode + "'); ";

    var btMessage = new BridgeTalk;
    btMessage.target = "photoshop";
    btMessage.body = ("try { " + brCode + " } catch (e) { " +
                      "BridgeTalk.bringToFront(BridgeTalk.appName); " +
                      "alert('Bridge Error:' + e); }");
    btMessage.onError = function(r)  {
      alert(TranslateErrorCodes.getMessage(r));
    }
    btMessage.send();

  } catch (e) {
    alert(Stdlib.exceptionMessage(e));
  }
};

//================================= Photoshop =================================

var FTScriptRunMode;

//
// FTScript.runPS
//   This function is called as a result of a message from FTScript.runBridge.
//   The name of the script to execute is read from the options stored in the
//    INI file (ftscript.ini).
// FTScriptRunMode is set to 'restart' before the app script is loaded and
//   executed
//
FTScript.runPS = function(mode) {
  function listProps(obj) {
    var tab = String.fromCharCode(9);
    var nl = String.fromCharCode(10);
    var s = '';
    var sep = nl;
    for (var x in obj) {
      s += x + ":" + tab;
      try {
        var o = obj[x];

        s += (typeof o == "function") ? "[function]" : o;
      } catch (e) {
      }
      s += sep;
    }
    return s;
  };

  try {
    // alert(app.name + "::FTScript.runPS(" + mode + ")");
    var opts = FTScript.getOptions();
    // alert(listProps(opts));

    if (!opts.script) {
      Error.runtimeError(9007, "Script missing from ftscript.ini");
    }

    opts.runMode = 'restart';

    FTScriptRunMode = 'restart';

    //alert("mode set to restart before eval");

    var f = new File(opts.script);

    if (!f.exists) {
      alert("Internal error: unable to locate FT script: " + opts.script);
      return;
    }

    if (FTScript.isCS3() || FTScript.isCS4()) {
      // alert("evalFile " + f);
      var rc = $.evalFile(decodeURI(f.fsName));

    } else {
      // alert(app.name + "::FTScript.runPS - 7");
      f.open("r");
      var str = f.read();
      f.close();

      var rc;
      eval(str);
    }
      //alert("after eval");

  } catch (e) {
    alert("FTScript.runPS: " + e);
  }
};

FTScript.isBridgeRunning = function() {
  var brSpec = FTScript.getAppSpecifier("bridge");

  return BridgeTalk.isRunning(brSpec);
};

//
//  FTScript.restart
//    Send a message to Bridge to restart this script.
//    Shutdown Photoshop
//  This function is called by the app script to move on
//  to the next block of processing.
//
FTScript.restart = function() {
  try {
    var brSpec = FTScript.getAppSpecifier("bridge");

    if (!BridgeTalk.isRunning(brSpec)) {
      //XXX add auto-start code
      alert("Bridge must be running before using this script.");
      return;
    }

    var opts = FTScript.getOptions();
    opts.mode = 'restart';
    FTScript.setOptions(opts);

    var btMessage = new BridgeTalk;
    btMessage.target = brSpec;
    //btMessage.body = "alert('restart');";

    var brCode = FTScript._implementationString() + "  ";

    brCode += "FTScript.runBridge('restart');  ";

    // brCode += " alert('Parse OK');";

    var code = ("try { " + brCode + " } catch (e) { " +
                "BridgeTalk.bringToFront(BridgeTalk.appName); " +
                "alert('Bridge Error:' + e); }");

    // Stdlib.writeToFile("~/Desktop/brCode.jsx", code);

    btMessage.body = code;

    btMessage.onError = function(r)  {
      alert(TranslateErrorCodes.getMessage(r));
    }
    var rc = btMessage.send();

    executeAction(app.charIDToTypeID('quit'),
                  new ActionDescriptor(), DialogModes.NO);

  } catch (e) {
    alert(Stdlib.exceptionMessage(e));
  }
};

//
// FTScript.setOptions
//   This script is called once by the application script. The 'opts'
//   object must have at least one property called 'script' which is
//   the full name of the script file that should be invoked when the
//   Bridge relaunches Photoshop
//
FTScript.setOptions = function(opts) {
  FTScript.opts = opts;

  if (!opts.script) {
    Error.runtimeError(9007, "Internal Error: script not specified in opts");
  }

  if (opts.script.charAt(0) == '#') {
    return;
    //$.level = 1; debugger;
  }
  var tab = String.fromCharCode(9);
  var f = new File(FTScript.INI_FILE);
  f.lineFeed = 'Unix';
  f.encoding = 'BINARY';
  f.open("w");
  for (var idx in opts) {
    if (!idx) {
      continue;
    }
    if (opts[idx] == undefined) {
      continue;
    }
    f.writeln(idx + tab + opts[idx]);
  }
  f.close();
};

//
// FTScript.getOptions
//   This function is called by Photoshop in response to a call from
//   FTScript.runBridge. The options contain at least the name of the
//   script that should be invoked to process the next block.
//
FTScript.getOptions = function() {
  var tab = String.fromCharCode(9);
  var nl = String.fromCharCode(10);

  var f = new File(FTScript.INI_FILE);
  f.lineFeed = 'Unix';
  f.encoding = 'BINARY';
  f.open("r");
  var str = f.read();
  f.close();

  var obj = {};
  var lines = str.split(nl);

  for (var i = 0; i < lines.length; i++) {
    if (lines[i].length == 0) {
      continue;
    }
    var sp = lines[i].split(tab);
    obj[sp[0]] = sp[1];
  }
  return obj;
};

//
// getRunMode
//   Returns the current runMode. Defaults to 'start'
//
getRunMode = function() {
  var rc = 'start';
  var lvl = $.level;
  $.level = 0;
  try {
    if (FTScriptRunMode != undefined) {
      rc = FTScriptRunMode;
    }
  } catch (e) {
  }
  $.level = lvl;

  return rc;
};

//================================= Common  ==================================

FTScript.isCS2 = function() {
  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^9\./) != null;
  }
  if (appName == 'bridge') {
    return version.match(/^1\./) != null;
  }
  if (appName == 'estoolkit') {
    return version.match(/^1\./) != null;
  }

  return false;
};
FTScript.isCS3 = function() {
  return true;

  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^10\./) != null;
  }
  if (appName == 'bridge') {
    return version.match(/^2\./) != null;
  }
  if (appName == 'estoolkit') {
    return version.match(/^2\./) != null;
  }
  return false;
};

FTScript.isCS4 = function() {
  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^11\./) != null;
  }
  if (appName == 'bridge') {
    return version.match(/^3\./) != null;
  }
  if (appName == 'estoolkit') {
    return version.match(/^3\./) != null;
  }
  return false;
};

FTScript.getAppSpecifier = function(appName) {
  var prefix = '';

  return appName;

  // if (FTScript.isCS2()) {
  //   if (appName == 'photoshop') {
  //     prefix = appName + '-9';
  //   }
  //   if (appName == 'bridge') {
  //     prefix = appName + '-1';
  //   }
  //   if (appName == 'estoolkit') {
  //     prefix = appName + '-1';
  //   }
  //   if (appName == 'golive') {
  //     prefix = appName + '-8';
  //   }
  //   if (appName == 'acrobat') {
  //     prefix = appName + '-7';
  //   }
  //   if (appName == 'helpcenter') {
  //     prefix = appName + '-1';
  //   }
  //   // add other apps here
  // }

  // if (FTScript.isCS3()) {
  //   if (appName == 'photoshop') {
  //     prefix = appName + '-10';
  //   }
  //   if (appName == 'bridge') {
  //     prefix = appName + '-2';
  //   }
  //   if (appName == 'estoolkit') {
  //     prefix = appName + '-2';
  //   }
  //   if (appName == 'devicecentral') {
  //     prefix = appName + '-1';
  //   }
  //   // add other apps here
  // }

  // if (!prefix) {
  //   return undefined;
  // }

  // var targets = BridgeTalk.getTargets(null);

  // var appSpec = undefined;
  // var rex = new RegExp('^' + prefix);

  // // find the most recent minor version
  // for (var i = 0; i < targets.length; i++) {
  //   var spec = targets[i];
  //   if (spec.match(rex)) {
  //     appSpec = spec;
  //   }
  // }
  // return appSpec;
};


FTScript._implementationString = function() {
  var str = "FTScript = function(){};  ";

  var ftns = ["getRunMode"];
  var clsProperties = ["DELAY", "TIMEOUT", "INI_FILE"];
  var clsMethods = ["runBridge", "runPS", "isBridgeRunning", "restart",
    "setOptions", "getOptions", "isCS2", "isCS3", "isCS4",
    "getAppSpecifier", "_implementationString"];

  for (var i = 0; i < clsMethods.length; i++) {
    var meth = clsMethods[i];
    str += ("FTScript." + meth + " = " + FTScript[meth] + ";  ");
  }

  for (var i = 0; i < clsProperties.length; i++) {
    var prop = clsProperties[i];
    if (prop == "INI_FILE") {
      str += ("FTScript." + prop + " = " + FTScript[prop].toSource() + ";  ");
    } else {
      str += ("FTScript." + prop + " = " + FTScript[prop] + ";  ");
    }
  }

  str += ("getRunMode = " + getRunMode + "; ");

  return str;
};

"FTScript.jsx";
// EOF
