//
// BtTest.jsx
//  This is a test script for BtRestart. Set the imageFolder path to a
//  directory with 6 or 7 jpg files and run the script from Photoshop.
//
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/XBridgeTalk.jsx"
//@include "xlib/BTRestart.jsx"

// var f = new File("/c/work/ps-scripts/BtRestart.jsx");
// // var f2 = new File("/c/Program Files/Common Files/Adobe/StartupScripts/BtRestart.jsx");

// f.copy(f2);


function getScriptFileName() {
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



alert("BtTest.jsx");

var imageFolder = "~/images";
var logFile = Folder.temp + "/files.log";

function startProcessing(script) {
  var folder = new Folder(imageFolder);
  var files = folder.getFiles("*.jpg");
  var str = files.toString().replace(/,/g, '\n');
  var file = new File(logFile);
  file.open("w");
  file.write(str);
  file.close();

  processImages(script);
};

function processImages(script) {
  var maxFiles = 2;

  var file = new File(logFile);

  if (!file.open("r")) {
    alert("File list not found.");
    return;
  }
  var str = file.read();
  file.close();
  var files = str.split('\n');
  
  var str = "Processing\r";
  for (var i = 0; i < maxFiles && files.length != 0; i++) {
    str += files.shift() + "\r";
  }
  alert(str);

  if (files.length > 0) {
    var str = files.toString().replace(/,/g, '\n');
    file.open("w");
    file.write(str);
    file.close();
    alert("Restarting Photoshop");
    BtRestart.restartPS(script);

  } else {
    file.remove();
    alert("Processing Complete.");
  }
};

var script = new FTScript(getScriptFileName(),
                          "startProcessing",
                          "processImages");

BtRestart.main(script);

"BtTest.jsx"

// EOF
