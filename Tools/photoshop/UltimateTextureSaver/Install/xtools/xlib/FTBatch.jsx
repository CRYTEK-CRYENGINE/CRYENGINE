//
// FTBatch.jsx
//
// $Id: FTBatch.jsx,v 1.9 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//include "xlib/FTScript.jsx"
//

//$.level = 0;

FTBatch = function() {
  var self = this;
};

_getPreferencesFolder = function() {
  var appData = Folder.userData;

  var folder = new Folder(appData + "/xtools");

  if (!folder.exists) {
    folder.create();
  }

  return folder;
};

FTBatch.DEFAULT_BATCH_SIZE = 10;
FTBatch.CFG_FILE = _getPreferencesFolder + "/ftbatch.cfg";

FTBatch.prototype.batchSize = FTBatch.DEFAULT_BATCH_SIZE;
FTBatch.prototype.cfgFile   = FTBatch.CFG_FILE;

FTBatch.prototype.processBatch = function(files) {
  var self = this;
  var rc = true;

  for (var i = 0; i < files.length; i++) {
    var file = new File(files[i]);
    if (!self.processFile(file)) {
      rc = false;
      break;
    }
  }

  return rc;
};
FTBatch.prototype.processFile = function(file) {
  alert(decodeURI(file));
};

FTBatch.prototype.main = function() {
  var self = this;

  if (!FTScript.isBridgeRunning()) {
    alert("Bridge must be running before using this script.");
    return ;
  }
  var mode = getRunMode();

  if (mode == 'once-only' || mode == 'start' || mode == 'restart') {
    self.process(mode);

  } else {
    alert("Internal error: bad run mode.");
  }
};

//
// processNext
//   This function is called when another batch of documents
//   can be processed.
//
FTBatch.prototype.processNext = function() {
  FTScript.restart();
};

//
// process
//   This calls start/continue processing depending on the value
//   of the 'mode' parameter
//
FTBatch.prototype.process = function(mode) {
  var self = this;

  if (mode == 'start' || mode == 'once-only') {
    var f = FTBatch.getScriptFileName();
    var opts = {
      script: decodeURI(f),
      start: Date().toString()
    };
    FTScript.setOptions(opts);   // this must be called with the
                                 // script property set to this script

    var rc = self.startProcessing();  // initialize the processor

    if (rc && self.continueProcessing()) { // process the first batch
      if (mode == 'start') {
        self.processNext();           // process the next batch as needed.
      }
    }
  } else if (mode == 'restart') {
    if (self.continueProcessing()) {
      self.processNext();             // process the next batch as needed.
    }
  }
};

//
// startProcessing
//   This function is responsible getting everything initialized.
//   In the case of this test script, it store a list of files
//   into another file, on line per document.
//
FTBatch.prototype.startProcessing = function() {
  var self = this;
  var files = self.init();
  var rc = false;

  if (files.length) {
    var str = files.join('\n');
    FTBatch.writeToFile(self.cfgFile, str);
    self.files = files;
    rc = true;
  }

  return rc;  // return 'true' to continue, 'false' if we're done
};

//
// continueProcessing
//   This function does the actual work.
//   It should return 'true' if there are more documents
//   to process or 'false' if processing has been completed
//
FTBatch.prototype.continueProcessing = function() {
  var self = this;
  //alert('continueProcessing');
  var maxFiles = 10;

  var str = FTBatch.readFromFile(self.cfgFile);

  var files = str.split(/[\r\n]+/);

  var batch = [];
  for (var i = 0; i < self.batchSize && files.length != 0; i++) {
    batch.push(files.shift());
  }

  var rc  = self.processBatch(batch);

  if (files.length > 0) {
    // if we have more files to process
    var str = files.join('\n');
    FTBatch.writeToFile(self.cfgFile, str);

  } else {
    // Completed
    new File(self.cfgFile).remove();
    rc = false;
  }

  return rc;
};


//
// Return a File or Folder object given one of:
//    A File or Folder Object
//    A string literal or a String object that refers to either
//    a File or Folder
//
FTBatch.convertFptr = function(fptr) {
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

FTBatch.readFromFile = function(fptr, encoding) {
  var file = FTBatch.convertFptr(fptr);
  file.open("r") || FTBatch.throwFileError("Unable to open input file ");
  if (encoding) {
    file.encoding = encoding;
  }
  var str = file.read();
  file.close();
  return str;
};
FTBatch.writeToFile = function(fptr, str, encoding) {
  var file = FTBatch.convertFptr(fptr);

  file.open("w") || FTBatch.throwFileError(file,
                                           "Unable to open output file ");
  if (encoding) {
    file.encoding = encoding;
  }

  file.write(str);
  file.close();
};
FTBatch.filename = undefined;
FTBatch.getScriptFileName = function() {
  var dbLevel = $.level;
  $.level = 0;
  var path = FTBatch.filename;

  if (!FTBatch.filename) {
    try {
      some_undefined_variable;
    } catch (e) {
      path = e.fileName;
    }
  }
  $.level = dbLevel;

  return path;
};
FTBatch.throwFileError = function(f, msg) {
  if (msg == undefined) msg = '';
  throw IOError(msg + '\"' + f + "\": " + f.error + '.');
};


"FTBatch.jsx";
// EOF
