//
// FTScriptTest.jsx
//
// $Id: FTScriptTest.jsx,v 1.6 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/FTScriptCS3.jsx"
//

function throwFileError(f, msg) {
  if (msg == undefined) msg = '';
  throw IOError(msg + '\"' + f + "\": " + f.error + '.');
};

//
// Return a File or Folder object given one of:
//    A File or Folder Object
//    A string literal or a String object that refers to either
//    a File or Folder
//
function convertFptr(fptr) {
  var f;
  if (fptr.constructor == String) {
    f = File(fptr);
  } else if (fptr instanceof File || fptr instanceof Folder) {
    f = fptr;
  } else {
    throw IOError("Bad file \"" + fptr + "\" specified.");
  }
  return f;
};

function readFromFile(fptr, encoding) {
  var file = convertFptr(fptr);
  file.open("r") || throwFileError("Unable to open input file ");
  if (encoding) {
    file.encoding = encoding;
  }
  var str = file.read();
  file.close();
  return str;
};
function writeToFile(fptr, str, encoding) {
  var file = convertFptr(fptr);

  file.open("w") || throwFileError(file, "Unable to open output file ");
  if (encoding) {
    file.encoding = encoding;
  }

  file.write(str);
  file.close();
};

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

getPreferencesFolder = function() {
  var appData = Folder.userData;

  var folder = new Folder(appData + "/xtools");

  if (!folder.exists) {
    folder.create();
  }

  return folder;
};

FTest = function() {};
FTest.imageFolder = "/h/Bianca/misc";
FTest.logFile = getPreferencesFolder() + "/ftTest.log";

//
// startProcessing
//   This function is responsible getting everything initialized.
//   In the case of this test script, it store a list of files
//   into another file, on line per document.
//
function startProcessing() {
  //alert('startProcessing');

  var folder = new Folder(FTest.imageFolder);
  var files = folder.getFiles("*.jpg");
  var str = files.toString().replace(/,/g, '\n');

  writeToFile(FTest.logFile, str);

  return true;  // return to continue
};


BATCH_SIZE = 5;

//
// continueProcessing
//   This function does the actual work.
//   It should return 'true' if there are more documents
//   to process or 'false' if processing has been completed
//
function continueProcessing() {
  //alert('continueProcessing');
  var maxFiles = BATCH_SIZE;

  var str = readFromFile(FTest.logFile);

  var files = str.split('\n');
  
  var str = "Processing\r";
  for (var i = 0; i < maxFiles && files.length != 0; i++) {
    str += files.shift() + "\r";
  }
  
  // alert(str + "\n" + files.length + " remaining");
  $.writeln(str + "\n" + files.length + " remaining");

  var rc;
  if (files.length > 0) {
    // if we have more files to process
    var str = files.toString().replace(/,/g, '\n');
    writeToFile(FTest.logFile, str);
    rc = true;

  } else {
    // Completed
    new File(FTest.logFile).remove();
    rc = false;
  }

  //alert("continueProcessing - " + rc);

  return rc;
};

//
// processNext
//   This function is called when another batch of documents
//   can be processed.
//
function processNext() {
  FTScript.restart();
};

//
// process
//   This calls start/continue processing depending on the value
//   of the 'mode' parameter
//
function process(mode) {
  // put common stuff here...

  if (mode == 'start' || mode == 'once-only') {
    var f = getScriptFileName();
    var opts = {
      script: decodeURI(f),
      start: Date().toString()
    };
    FTScript.setOptions(opts);   // this must be called with the
                                 // script property set to this script

    var rc = startProcessing();  // initialize the processor

    if (rc && continueProcessing()) { // process the first batch
      if (mode == 'start') {
        processNext();           // process the next batch as needed.
      }
    }
  } else if (mode == 'restart') {
    if (continueProcessing()) {
      processNext();             // process the next batch as needed.
    }
  }
};

function main() {
  if (!FTScript.isBridgeRunning()) {
    alert("Bridge must be running before using this script.");
    return ;
  }
  var mode = getRunMode();
  //alert("FTScriptTest.main(" + mode + ")");

  if (mode == 'once-only' || mode == 'start' || mode == 'restart') {
    process(mode);

  } else {
    alert("Internal error: bad run mode.");
  }
};

main();

"FTScriptTest.jsx";
// EOF
