//
// FolderMonitor.jsx
//
//
// $Id: FolderMonitor.jsx,v 1.6 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2006, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//-include "xlib/stdlib.js"
//

FolderMonitor = function(obj) {
  var self = this;

  if (obj) {
    for (var idx in obj) {
      self[idx] = obj[idx];
    }
  }
};

FolderMonitor.INI_FILE = "~/folderMonitor.ini";
FolderMonitor.LOG_FILE = "~/folderMonitor.log";

FolderMonitor.prototype.iniFile = FolderMonitor.INI_FILE;
FolderMonitor.prototype.logFile = FolderMonitor.LOG_FILE;
FolderMonitor.prototype.logEnabled = true;
FolderMonitor.prototype.hotFolder = undefined;
FolderMonitor.prototype.destFolder = undefined;
FolderMonitor.prototype.name = "FolderMonitor";
FolderMonitor.prototype.getImageFilesRecursive = false;
FolderMonitor.prototype.getImageFilesComplete = true;

FolderMonitor.prototype.check = function() {
  var self = this;

  if (!self.hotFolder || !self.hotFolder.exists) {
    Stdlib.log("Hot folder \"" + self.hotFolder.toUIString() +
               "\" does not exist.");
    return false;
  }

  return true;
};

FolderMonitor.prototype.copyFiles = function(files, folder, remove) {
  var self = this;
  var rc = true;

  folder = Stdlib.convertFptr(folder);

  for (var i = 0; i < files.length; i++) {
    var file = files[i];
    var opFile = new File(folder + '/' + file.name);
    if (!file.copy(opFile)) {
      self.log("Copy of " + file.name + " failed.\n" + file.error);
      return false;
    }
    if (remove == true) {
      file.remove();
    }
  }
  return true;
};
FolderMonitor.prototype.processFolder = function(folder) {
  var self = this;
  self.log("Processing folder " + folder.toUIString());

  var files = Stdlib.getImageFiles(folder, self.getImageFilesRecursive,
                                   self.getImageFilesComplete);
  var rc = false;

  try {
    rc = self.processFiles(files);

  } catch (e) {
    self.log("Error processing folder: " + e + '@' + e.line);
  }

  return rc;
};
FolderMonitor.prototype.processFiles = function(files) {
  var self = this;

  if (self.destFolder) {
    if (!self.copyFiles(files, self.destFolder, true)) {
      return false;
    }
  }

  for (var i = 0; i < files.length; i++) {
    var file = files[i];
    var rc;

    if (self.destFolder) {
      var opFile = new File(self.destFolder + '/' + file.name);
      rc = self.processFile(opFile);

    } else {
      rc = self.processFile(file);
    }
    if (!rc) {
      return rc;
    }
  }
  return true;
};
FolderMonitor.prototype.processFile = function(file) {
  var self = this;

  var rc = false;

  if (file && file.exists) {
    self.log("Processing file " + file.toUIString());

    try {
      var doc = app.open(file);

    } catch (e) {
      self.log("Failed to open document: " + file.name + ".\r" + e);
    }

    if (doc) {
      try {
        rc = self.processDocument(doc);

      } catch (e) {
        self.log("Failure processing document: " + file.name + ".\r" + e);
      }

      try {
        // if the document is still open after processing, close it.
        doc.close(SaveOptions.DONOTSAVECHANGES);
      } catch (e) {
      }
    }
  }
  return rc;
};

FolderMonitor.prototype.processDocument = function(doc) {
  // do nothing...
  return true;
};

FolderMonitor.prototype.log = function(str) {
  Stdlib.log(str);
};

FolderMonitor.prototype._init = function() {
  var self = this;

  if (!self.iniFile) {
    alert("No INI file specified.");
    return false;
  }

  var opts = Stdlib.readIniFile(self.iniFile);

  //$.level = 1; debugger;
  Stdlib.copyFromTo(opts, self);

  if (!self.hotFolder) {
    alert("No hotfolder specified.");
    return false;
  }
  self.hotFolder = Stdlib.convertFptr(self.hotFolder);
  if (self.destFolder) {
    self.destFolder = Stdlib.convertFptr(self.destFolder);
  }

  if (self.sleep) {
    var n = toNumber(self.sleep);
    if (isNaN(n)) {
      alert("Bad sleep value specified");
      return false;
    }
    self.sleep = n * 1000;

  } else {
    self.sleep = 0;
  }

  if (self.logFile) {
    Stdlib.log.setFile(self.logFile);
    Stdlib.log.enabled = self.logEnabled;
    Stdlib.log.append = true;
  }

  if (self.init) {
    return self.init();
  }
  return true;
};
FolderMonitor.prototype.exec = function() {
  var self = this;

  if (!self._init()) {
    return;
  }

  self.log("Start Monitor " + self.name);
  Stdlib.log(listProps(self));

  if (self.sleep == 0) {
    if (self.check()) {
      self.processFolder(self.hotFolder);
    }

  } else {
    var file = self.getLockFile();
    Stdlib.writeToFile(file,
                       "delete this file to terminate the hotfolder script");
    while (file.exists) {
      if (self.check()) {
        self.processFolder(self.hotFolder);
      }
      $.sleep(self.sleep);
    }
  }
  self.log("Stop  Monitor " + self.name);
};

FolderMonitor.prototype.getLockFile = function() {
  var self = this;
  if (!self.hotFolder) {
    if (!self._init()) {
      return undefined;
    }
  }
  return new File(self.hotFolder + '/' + self.hotFolder.name + ".lck");
};

FolderMonitor.test = function() {
  var testIni = {
    hotFolder: "/c/tmp/monitor/hot",
    destFolder: "/c/tmp/monitor/dest",
    logFile: "~/folderMonitor.log",
    logEnabled: true,
    name: "TestMonitor"
  };

  Stdlib.writeIniFile(FolderMonitor.INI_FILE, testIni);

  var monitor = new FolderMonitor();
  monitor.exec();
};

// FolderMonitor.test();

"FolderMonitor.jsx";
// EOF
