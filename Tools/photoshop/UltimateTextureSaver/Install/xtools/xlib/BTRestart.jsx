//
// BTRestart.jsx
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/XBridgeTalk.jsx"


$.level = 0;

cTID = function(s) { return app.charIDToTypeID(s); };
sTID = function(s) { return app.stringIDToTypeID(s); };

BTRestart = {};

BTRestart.ONCE_ONLY_MODE = 'once-only';
BTRestart.RESTART_MODE = 'restart';
BTRestart.START_MODE = 'start';

throwFileError = function(f, msg) {
  if (msg == undefined) msg = '';
  throw IOError(msg + '\"' + f + "\": " + f.error + '.');
};

BTRestart.readFromFile = function(file) {
  file.open("r") || throwFileError("Unable to open input file ");
  var str = file.read();
  file.close();
  return str;
};


BTRestart.getScriptFileName = function() {
  var dbLevel = $.level;
  $.level = 0;
  var path = undefined;

  try {
    some_undefined_variable;
  } catch (e) {
    path = e.fileName;
  }

  $.level = dbLevel;

  return path;
};

BTRestart.run = function(mode, scriptFile, delay) {
  $.writeln(BridgeTalk.appName + ": BTRestart.run(\"" + mode +
            "\", \"" + script + "\", " + delay + ");")
  try {
    if (BridgeTalk.appName == "bridge")  {
      if (mode.constructor != String) {
        mode = BTRestart.START_MODE;
      }
      if (delay == undefined) {
        delay = 0;
      }

      $.writeln("sleeping " + delay + "...");
      $.sleep(delay * 1000);

      var psSpec = XBridgeTalk.getAppSpecifier("photoshop");
//       if (!XBridgeTalk.startApplication(psSpec)) {
//         alert("Unable to launch " + psSpec);
//         return;
//       }
      BridgeTalk.bringToFront(psSpec);

      var script = ("BTRestart.run('" + mode + "', \'" +
                    encodeURI(scriptFile) + "\');");
      $.writeln(script);
    
      var btMessage = new BridgeTalk;
      btMessage.target = psSpec;
      btMessage.body = script;
      btMessage.onError = function(r)  {
        alert(TranslateErrorCodes.getMessage(r));
      }
      btMessage.send();
      $.writeln("Message sent from Bridge to Photoshop");

    } else if (BridgeTalk.appName == "photoshop")  {

      BTRestart.runMode = mode;
      var script = BTRestart.readFromFile(new File(decodeURI(scriptFile)));
      eval(script);
    }
  } catch (e) {
    alert("BridgeError " + e);
  }

  $.writeln(BridgeTalk.appName + ": exit BTRestart.run();");
};

BTRestart.restartDelay = 1; // seconds of delay before restarting Photoshop

// This function does not return 
BTRestart.restartPS = function(script) {
  var msg = ("BTRestart.restartPS");
  $.writeln(msg);

  var br = XBridgeTalk.getAppSpecifier("bridge");
  if (!XBridgeTalk.startApplication(br)) {
    alert("unable to start " + br);
    return;
  }
  try {
    var btMessage = new BridgeTalk;
    btMessage.target = "bridge";
    btMessage.body = (BTRestart.getClassDef() +
                      "try {\n" +
                      "BTRestart.run(\"" + BTRestart.RESTART_MODE +
                      "\", \"" +  script + "\", " +
                      BTRestart.restartDelay + ");\n" +
                      "} catch (e) { alert(e); }\n"
                      );
    //btMessage.body = "alert('In Bridge');";
    $.writeln(btMessage.body);
    btMessage.send();

    //executeAction(cTID('quit'), new ActionDescriptor(), DialogModes.NO);
  } catch (e) {
    alert(e);
  }
};


BTRestart.getRunMode = function() {
  var rc = BTRestart.ONCE_ONLY_MODE;
  try {
    if (BTRestart.runMode != undefined) {
      rc = BTRestart.runMode;
    }
  } catch (e) {
  }

  return rc;
};


BTRestart.init = function() {
  if (BridgeTalk.appName == "photoshop") {
    $.writeln("Photoshop-BTRestart Loaded " + Date());
  }
  if (BridgeTalk.appName == "bridge") {
    $.writeln("Bridge-BTRestart Loaded " + Date());
  }
};

BTRestart.main = function(script) {
  var runMode = BTRestart.getRunMode();

  $.writeln("In main: " + runMode);

  if (runMode == BTRestart.ONCE_ONLY_MODE ||
      runMode == BTRestart.START_MODE) {

    // run the script at least once
    eval(script.runFtn + "(" + script.script + ");");

  } else if (runMode == BTRestart.RESTART_MODE) {
    eval(script.continueFtn + "();");

  } else {
    alert("Bridge-Photoshop configuration error.");
  }
};

BTRestart.partsList = ["ONCE_ONLY_MODE",
                       "RESTART_MODE",
                       "START_MODE",
                       "run"
                       ];

BTRestart.getClassDef = function() {
  var str = "BTRestart = {};\n";
  var parts = BTRestart.partsList;

  for (var i = 0; i < parts.length; i++) {
    var part = parts[i];
    str += "BTRestart." + part + ";\n";
  }
  return str;
};
try {
  BTRestart.init();

} catch (e) {
  alert(e);
}

"BTRestart.jsx";
// EOF

