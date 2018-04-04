//
// FTScript.jsx
//
// $Id: FTScript.jsx,v 1.12 2011/05/11 02:46:10 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

FTScript = function() {};
FTScript.INI_FILE = "~/ftscript.ini";

//================================= Bridge  ==================================

if (BridgeTalk.appName == "bridge") {

FTScript.DELAY = 2000;    // amount of time to wait to relaunch PS after
                          // it has been shutdown

//
// FTScript.runBridge
//   This function is called as part of the restart processing. When this
//   function is called, Photoshop has just been shut down. After a brief
//   pause, this function retarts Photoshop and send the FTScript.runPS
//   call to Photoshop to cause the next process block to happen.
//
FTScript.runBridge = function(mode) {
  try {
    //alert("FTScript.runBridge");
    $.sleep(FTScript.DELAY);
    if (mode.constructor != String) {
      mode = 'start';
    }
    var psSpec = FTScript.getAppSpecifier("photoshop");

    if (!BridgeTalk.isRunning(psSpec)) {
      if (BridgeTalk.launch) {
        BridgeTalk.launch(psSpec);

      } else if (!startTargetApplication(psSpec)) {
        alert("Failed to launch " + psSpec);
        return;
      }
    }
    //alert("Photoshop started");

    var script = ("try { FTScript.runPS('" + mode + "'); }\n" +
                  "catch (e) { alert(e); };\n");

    var btMessage = new BridgeTalk;
    btMessage.target = "photoshop";
    btMessage.body = script;
    btMessage.onError = function(r)  {
      alert(TranslateErrorCodes.getMessage(r));
    };
    btMessage.send();

  } catch (e) {
    alert(e);
  }
};

} // "bridge"

//================================= Photoshop =================================
if (BridgeTalk.appName == "photoshop") {

cTID = function(s) { return app.charIDToTypeID(s); };

//
// FTScript.runPS
//   This function is called as a result of a message from FTScript.runBridge.
//   The name of the script to execute is read from the options stored in the
//    INI file (ftscript.ini).
// FTScriptRunMode is set to 'restart' before the app script is loaded and
//   executed
//
FTScript.runPS = function(mode) {
  try {
    //alert(app.name + "::FTScript.runPS");
    var opts = FTScript.getOptions();
    if (!opts.script) {
      throw "Unable to load script:" + opts.script;
    }

    FTScriptRunMode = 'restart';
    //alert("mode set to restart before eval");

    var f = new File(opts.script);

    if (!f.exists) {
      alert("Internal error: unable to locate FT script: " + opts.script);
      return;
    }

    if (FTScript.isCS3() || FTScript.isCS4() || FTScript.isCS5()) {
      $.evalFile(f);

    } else {
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

    var btMessage = new BridgeTalk;
    btMessage.target = brSpec;
    //btMessage.body = "alert('restart');";

    btMessage.body = ("try { FTScript.runBridge('restart'); }" +
                      "catch (e) { alert(e); }\n");
    btMessage.onError = function(r)  {
      alert(TranslateErrorCodes.getMessage(r));
    };
    btMessage.send();
    //alert("photoshop - quit");
    executeAction(cTID('quit'), new ActionDescriptor(), DialogModes.NO);

  } catch (e) {
    alert(e);
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
  if (opts.script.charAt(0) == '#') {
    return;
    //$.level = 1; debugger;
  }
  var f = new File(FTScript.INI_FILE);
  f.open("w");
  for (var idx in opts) {
    f.writeln(idx + '\t' + opts[idx]);
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
  var f = new File(FTScript.INI_FILE);
  f.open("r");
  var str = f.read();
  f.close();
  var obj = {};
  var lines = str.split('\n');
  for (var i = 0; i < lines.length; i++) {
    var sp = lines[i].split('\t');
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
} // "photoshop"

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
  if (appName == 'golive') {
    return version.match(/^8\./) != null;
  }
  if (appName == 'acrobat') {
    return version.match(/^7\./) != null;
  }
  if (appName == 'helpcenter') {
    return version.match(/^1\./) != null;
  }

  return false;
};
FTScript.isCS3 = function() {
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
  if (appName == 'devicecentral') {
    return version.match(/^1\./) != null;
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
  if (appName == 'devicecentral') {
    return version.match(/^2\./) != null;
  }
  return false;
};
FTScript.isCS5 = function() {
  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^12\./) != null;
  }
  if (appName == 'bridge') {
    return version.match(/^4\./) != null;
  }
  if (appName == 'estoolkit') {
    return version.match(/^4\./) != null;
  }
};

FTScript.getAppSpecifier = function(appName) {
  var prefix = '';

  if (FTScript.isCS2()) {
    if (appName == 'photoshop') {
      prefix = appName + '-9';
    }
    if (appName == 'bridge') {
      prefix = appName + '-1';
    }
    if (appName == 'estoolkit') {
      prefix = appName + '-1';
    }
    if (appName == 'golive') {
      prefix = appName + '-8';
    }
    if (appName == 'acrobat') {
      prefix = appName + '-7';
    }
    if (appName == 'helpcenter') {
      prefix = appName + '-1';
    }
    // add other apps here
  }

  if (FTScript.isCS3()) {
    if (appName == 'photoshop') {
      prefix = appName + '-10';
    }
    if (appName == 'bridge') {
      prefix = appName + '-2';
    }
    if (appName == 'estoolkit') {
      prefix = appName + '-2';
    }
    if (appName == 'devicecentral') {
      prefix = appName + '-1';
    }
    // add other apps here
  }

  if (FTScript.isCS4()) {
    if (appName == 'photoshop') {
      prefix = appName + '-11';
    }
    if (appName == 'bridge') {
      prefix = appName + '-3';
    }
    if (appName == 'estoolkit') {
      prefix = appName + '-3';
    }
    if (appName == 'devicecentral') {
      prefix = appName + '-2';
    }
    // add other apps here
  }

  if (!prefix) {
    return undefined;
  }

  var targets = BridgeTalk.getTargets(null);

  var appSpec = undefined;
  var rex = new RegExp('^' + prefix);

  // find the most recent minor version
  for (var i = 0; i < targets.length; i++) {
    var spec = targets[i];
    if (spec.match(rex)) {
      appSpec = spec;
    }
  }
  return appSpec;
};

"FTScript.jsx";
// EOF
