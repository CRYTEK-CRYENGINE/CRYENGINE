//
// FTP.jsx
//    This file contains additional code to simplify the use of the
//    FtpConnection class in Bridge.
//
// Thanks go out to Chanandler_Bong@adobeforums.com for helping me get
// the tasking correct between the progress window and ftp transfer.
//
// This code is implemented in such a way that you do not have to have FTP.jsx
// installed under Startup Scripts CS3. When RemoteFtpConnection makes its
// request to Bridge over BridgeTalk, it also sends along the entire set of
// FtpConnection extensions.
//
// $Id: FTP.jsx,v 1.27 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//
//app;
//
// This include file is required by RemoteFtpConnection
//-include "xlib/XBridgeTalk.jsx"
//

FTPConfig = function() {
  var self = this;

  self.binary     = true;
  self.passive    = false;
  self.sourceDir  = "~";
  self.targetDir  = "tmp";
  self.mode       = "sftp";              // ftp,sftp
  self.username   = "xbytor";
  self.password   = "password";
  self.hostname   = "192.168.1.114";
  self.logFile    = "~/ftp.log";
  self.logEnabled = true;
  self.timeout    = 20;
  self.refresh    = false;  // for putDirTree

  self.verbose    = false;
  self.trace      = false;
};
FTPConfig._implementationStr = function() {
  return "FTPConfig = " + FTPConfig + ";\n\n\n";
};

if (BridgeTalk.appName == 'bridge') {
  if (!BridgeTalk.appVersion.match(/^2\./)) {
    throw "This script requires Bridge CS3";
  }

  //
  // This code only works on Bridge CS3
  //

 if (ExternalObject.webaccesslib == undefined)  {
    if (Folder.fs == "Windows" ) {
      var _pathToLib = Folder.startup.fsName + "/webaccesslib.dll";
    } else {
      var _pathToLib = Folder.startup.fsName + "/webaccesslib.bundle";
    }

    var _libfile = new File(_pathToLib);
    ExternalObject.webaccesslib = new ExternalObject("lib:" + _pathToLib);
  }


//   // Load the webaccess library
//  if(ExternalObject.webaccesslib == undefined )  {
//     if( Folder.fs == "Windows" ) {
//       var pathToLib = Folder.startup.fsName + "/webaccesslib.dll";
//     } else {
//       var pathToLib = Folder.startup.fsName + "/webaccesslib.bundle";
//       // verify that the path is valid
//     }
//     var libfile = new File( pathToLib );
//     ExternalObject.webaccesslib = new ExternalObject("lib:" + pathToLib );
//   }

  // at this point, Bridge CS3 now has the FtpConnection class available

} else {
  // We are not in Bridge CS3, so we have to define a stub FtpConnection
  // class to hang our extensions on.

  FtpConnection = function() {};
}

FtpConnection._toBoolean = function(s) {
  if (s == undefined) { return false; }
  if (s.constructor == Boolean) { return s.valueOf(); }
  if (s.constructor == String)  { return s.toLowerCase() == "true"; }
  return Boolean(s);
};
FtpConnection._listProps = function(obj) {
  var s = '';
  for (var x in obj) {
    try {
      var o = obj[x];
      if (typeof(o) != "function") {
        s += (x + ":\t" + o + "\r\n");
      }
    } catch (e) {
    }
  }
  return s;
};


//========================= createFrom =================================
//
// createFrom will automatically open (to verify) a connection to an
// ftp server. If there was a problem, an exception is thrown
//
FtpConnection.createFrom = function(ftpConfig) {
  function toNumber(s, def) {
    if (s == undefined) { return NaN; }
    if (s.constructor == String && s.length == 0) { return NaN; }
    if (s.constructor == Number) { return s.valueOf(); }
    return Number(s.toString());
  };

  // setup a mechanism for logging info
  if (ftpConfig.logFile && FtpConnection._toBoolean(ftpConfig.logEnabled)) {
    ftpConfig._log = new File(ftpConfig.logFile);
    if (FTPConfig._logFile != ftpConfig.logFile) {
      FTPConfig._logFile = ftpConfig.logFile;
      ftpConfig._log.length = 0;
    }
    if (!ftpConfig._log.open("e")) {
      throw ("Unable to open ftp log file: " +
             decodeURI(ftpConfig._log.fsName) +
             ": " + ftpConfig._log.error);
    }
    ftpConfig._log.close();
    ftpConfig.log = function(str) {
      ftpConfig._log.open("e");
      if (File.fs == "Macintosh") {
        ftpConfig.lineFeed = "Unix";
        ftpConfig.encoding = 'BINARY';
      }
      ftpConfig._log.seek(0, 2);
      ftpConfig._log.writeln(str);
      ftpConfig._log.close();
    }
  } else {
    ftpConfig.log = function() {}
  }


  var mode = ftpConfig.mode || "sftp";

  ftpConfig.log("Creating FTP connection:\n" +
                FtpConnection._listProps(ftpConfig));

  var ftp = new FtpConnection(mode + "://" + ftpConfig.hostname);
  ftp._config  = ftpConfig;
  ftp.binary   = ftpConfig.binary;
  ftp.username = ftpConfig.username;
  ftp.password = ftpConfig.password;
  ftp.passive  = FtpConnection._toBoolean(ftpConfig.passive);

  if (ftpConfig.timeout) {
    ftp.timeout = toNumber(ftpConfig.timeout);
  }

  ftp.log = function(msg) {
    this._config.log(msg);
    if (this.verbose) {
      $.writeln(msg);
    }
  }

  // controls estk logging
  ftp.verbose = FtpConnection._toBoolean(ftpConfig.verbose);
  // generates detailed ftp logs
  ftp.trace = FtpConnection._toBoolean(ftpConfig.trace);

  ftp._ftpConfig = ftpConfig;

  ftp.log("Opening " + ftp.url + "...");

  // override for now...
//   ftp.url = "ftp://" + ftpConfig.hostname;

  if (ftp.open()) {
    ftpConfig.mode = mode;

  } else {
    mode = (mode == "sftp" ? "ftp" : "sftp");

    ftp.url = mode + "://" + ftpConfig.hostname;
    if (!ftp.open()) {
      ftp.log("Open Error: " + ftp.errorString);
      throw ftp.errorString;
    }
    ftpConfig.mode = mode;
  }

  ftp.log("Opened " + ftp.url);

  return ftp;
};


//========================= forceMkdir =================================
//
// Create a given path on an ftp server, if necessary.
// Parent folders are created as needed.
//

FtpConnection.prototype.forceMkdir = function(path) {
  var self = this;

  if (!self.isOpen) {
    self.deferredError = FtpConnection.errorChannelClosed;
    self.deferredErrorString = "channel closed";
    return false;
  }
  if (!path) {
    self.deferredError = FtpConnection.errorNotDirectory;
    self.deferredErrorString = "bad pathname";
    return false;
  }

  // do something reasonable with win-style path separators
  var vpath = path.replace(/\\/g, '/');

  if (!self.isDir(vpath)) {
    // split the path in to component parts
    var parts = vpath.split('/');

    // if there's not part[0], then we have an absolute path
    // and need to prepend a '/' onto the first actual part
    if (!parts[0]) {
      parts.shift();
      parts[0] = '/' + parts[0];
    }

    // loop through all of the path components from the base on
    // up, creating if needed as we go along
    var current = parts.shift();
    while (true) {
      if (!self.isDir(current)) {
        if (!self.mkdir(current)) {
          self.deferredError = self.error;
          self.deferredErrorString = self.errorString;
          return false;
        }
      }
      if (parts.length == 0) {
        break;
      }

      // next part...
      current += '/' + parts.shift();
    }
  }

  // we either created or found a valid folder
  return true;
};

//========================= putDirTree =================================
//
// Put an entire directory tree on an ftp server in the designated
// folder. An exception will be thrown if anything is wrong with the
// request.
//
FtpConnection.prototype.putDirTree = function(folderS, targetS, refresh) {
  var self = this;
  var ftpConfig = self._config;
  var folder;

  if (!Array.prototype.indexOf) {
    Array.prototype.indexOf = function(el) {
      for (var i = 0; i < this.length; i++) {
        if (this[i] == el) {
          return i;
        }
      }
      return -1;
    };
  }

  if (!Array.prototype.contains) {
    Array.prototype.contains = function(el) {
      for (var i = 0; i < this.length; i++) {
        if (this[i] == el) {
          return true;
        }
      }
      return false;
    };
  }

  refresh = !!refresh;

  // we only do async
  self.async = true;

  self._ctxt = {}; // context data for this call
  var ctxt = self._ctxt;

  ctxt.ftn = "FtpConnection.putDirTree";
  ctxt.args = { folderS: folderS, targetS: targetS, refresh: refresh };
  ctxt.startTime = new Date().getTime();
  ctxt.refresh = refresh;
  ctxt.byteCount = 0;
  ctxt.fileCount = 0;
  ctxt.errorCount = 0;
  ctxt.ftpErrors = 0;

  if (!folderS) {
    var msg = "Source folder not specified";
    self.log(msg);
    throw msg;
  }

  if (folderS.constructor == String) {
    folder = new Folder(folderS);

  } else if (folderS instanceof Folder) {
    folder = folderS;

  } else {
    var msg = "Bad folder specified: " + folderS.toString();
    self.log(msg);
    throw msg;
  }

  if (!targetS) {
    targetS = ftpConfig.targetDir;
  }

  // make sure we have a root directory to put the files in
  if (!self.isDir(targetS)) {
    if (!self.forceMkdir(targetS)) {
      var msg = ("Unable to create directory " + targetS +
                 " on ftp server: " + self.deferredErrorString);
      self.log("mkdir Error: " + msg);
      throw msg;
    }
  }

  // this is where we actually do the 'put'
  self._putFile = function(fobj) {
    var self = this;
    var ctxt = self._ctxt;

    var targetF = fobj.targetDir + '/' + fobj.file.name;
    fobj.target = targetF;
    fobj.attempts++;

    self.log(ctxt.fileCount + " put(" + decodeURI(fobj.file.absoluteURI) +
             ", " + targetF + ")");
    ctxt.currentFile = new File(fobj.file.absoluteURI);
    ctxt.currentFile.fobj = fobj;

    if (!self.forceMkdir(fobj.targetDir)) {
      self._msg = ("Failed to create folder on ftp server: " + fobj.targetDir +
                   ". " + self.deferredErrorString);
      return false;
    }

    if (!self.put(ctxt.currentFile, targetF)) {
      self._msg = self.errorString;
      return false;
    }

    return true;
  }

  self._determineNextFile = function() {
    var self = this;
    var ctxt = self._ctxt;
    var fobj = undefined;

    self.progressBar.show();

    if (true) {
      var cd = self.cd;
      try {
        var files = undefined;
        for (var idx in ctxt.targetDirs) {
          files = ctxt.targetDirs[idx];
          delete ctxt.targetDirs[idx];
          break;
        }
        if (!files) {
          return 0;
        }

        ctxt.fileCount += files.length;
        var targetDir = files[0].targetDir;

        self.log("Checking " + targetDir);

        if (self.isDir(targetDir)) {
          if (!self.cwd(targetDir)) {
            self.log("CWD failed for " + targetDir);
            return 1;
          }
          if (!self.ls()) {
            self.log("LS failed for " + targetDir);
            return 1;
          }

          // ls apparently has some async component. This bit of code tries
          // to wait out that component.
          var timeout = 50;
          var ftpFiles = undefined;
          var ftpSizes = undefined;
          while (timeout) {
            $.sleep(100); // XXX need this or self.files will fail
            try {
              if (!ftpFiles) {
                ftpFiles = self.files.slice(0);
              }
              if (!ftpSizes) {
                ftpSizes = self.sizes.slice(0);
              }
              break;
            } catch (e) {
            }
            timeout--;
          }
          if (!timeout) {
            self.log("LS timed out");
          }

          // loop through files and compare to self.files,self.size
          for (var i = 0; i < files.length; i++) {
            var fobj = files[i];
            var file = fobj.file;
            var fname = file.name;

            if (ftpFiles && ftpSizes) {
              var idx = ftpFiles.indexOf(fname);
              if (idx == -1) {
                idx = ftpFiles.indexOf(decodeURI(fname));
              }
              if (idx >= 0) {
                if (ftpSizes[idx] == file.length) {
                  continue;
                }
              }
            }

            self.log("Enqueueing " + decodeURI(fobj.file.name));
            ctxt.refreshList.push(fobj);
          }
        } else {
          for (var i = 0; i < files.length; i++) {
            var fobj = files[i];
            self.log("Enqueueing " + decodeURI(fobj.file.name));
            ctxt.refreshList.push(fobj);
          }
        }
      } catch (e) {
        var msg = "Error: " + e;
        if (e.line) {
          msg += ' @' + e.line;
        }
        self.log(msg);

        for (var i = 0; i < files.length; i++) {
          var fobj = files[i];
          self.log("Enqueueing " + decodeURI(fobj.file.name));
          ctxt.refreshList.push(fobj);
        }

      } finally {
        self.cd = cd;
      }
    }

    return 1;

    while (true) {
      // this doesn't really loop...
      if (ctxt.filesList.length == 0) {
        break;
      }
      fobj = ctxt.filesList.shift();
      if (!fobj) {
        break;
      }
//       if (!ctxt.refresh) {
//         break;
//       }

      var targetF = fobj.targetDir + '/' + fobj.file.name;

      if (!self.exists(targetF)) {
        break;
      }

      try {
        var size = self.size(targetF);
        if (size <= 0) {
          break;
        }
        if (size != fobj.file.length) {
          break;
        }

        fobj = undefined;

      } catch (e) {
        break;
      }

      break;
    }

    if (fobj) {
      self.log("Enqueueing " + decodeURI(fobj.file.name));
      ctxt.refreshList.push(fobj);
    }

    return ctxt.filesList.length;
  };

  self._nextFile = function() {
    var self = this;
    var ctxt = self._ctxt;
    var rc = true;

    self.progressBar.show();

    if (ctxt.filesList.length != 0) {
      fobj = ctxt.filesList.shift();
      if (fobj) {

        var rc = self._putFile(fobj);
        if (!rc) {
          self._stop(self._msg, true);
        }

      } else {
        self._stop("Transfer completed.  " + self._stats(), false);
      }
    } else {
      self._stop("Transfer completed.  " + self._stats(), false);
    }

    return rc;
  };

  self._stats = function() {
    var self = this;
    var ctxt = self._ctxt;
    var stopTime = new Date().getTime();
    var elapsed = (stopTime - ctxt.startTime)/1000.0;
    var rate = ctxt.byteCount/elapsed;
    var str = (ctxt.fileCount + " files transfered in " +
               elapsed.toFixed(2) + " seconds.  " +
               "(" + rate.toFixed(2) + " bytes per second)");
    return str;
  };

  self._stop = function(msg, retry) {
    var self = this;
    app.cancelTask(self.task.id);
    self.cancel();
    self.progressBar.close();
    FtpConnection.backgroundTask = undefined;
    var lmsg = "FTP session stopped: " + msg;
    self.log(lmsg);
    BridgeTalk.bringToFront(BridgeTalk.appName);

    if (!retry) {
      alert(msg);

    } else {
      var ftpConf = self._config;
      try {
        var ftp = FtpConnection.createFrom(ftpConf);
        ftp.putDirTree(ftpConf.sourceDir, ftpConf.targetDir, true);

      } catch (e) {
        throw "Fatal Error: " + e;
      }
    }
  };


  var rc = true;

  // everything is now ready for the copy

  self.log('putDirTree("' + decodeURI(folder.fsName) + '", "' +
           targetS + '", ' + refresh + ')');

  try {
    // get the list of files to transfer
    ctxt.filesList = [];
    ctxt.fileCount = 0;
    ctxt.targetDirs = {};
    ctxt.fileTotal = FtpConnection._inventoryFiles(folder, targetS, ctxt);
    self.log("Putting " + ctxt.fileTotal + " files.");

    var title = "FTP Upload: " + self.url;
    // create a progress bar window
    self.progressBar = FtpConnection.createProgressPalette(title,
                                                           0, ctxt.fileTotal,
                                                           undefined, true);
    self.progressBar.ftpServer = self;
    self.progressBar.onCancel = function() {
      var ftpServer = this.ftpServer;
      ftpServer._stop("Transfer terminated by user.", false);
    }

    // Setup the task to keep the progress bar updated
    var task = {};
    task.id = 0;
    task.ftpServer = self;
    self.task = task;

    task.updateProgressBar = function() {
      var self = this;
      var ftpServer = self.ftpServer;
      var ctxt = ftpServer._ctxt;

      ftpServer.pump();
      var msg = ("Transferring " + ctxt.fileCount + " of " +
                 ctxt.fileTotal + " files...");
      ftpServer.progressBar.updateProgress(ctxt.fileCount, msg);
      return true;
    }

    task.updateProgressBarRefresh = function() {
      // This callback is used during the refresh phase

      var self = this;
      var ftpServer = self.ftpServer;
      var ctxt = ftpServer._ctxt;

      try {
        if (ftpServer._determineNextFile()) {
          // if we still have more files to check
          ftpServer.onFileXfer(undefined);

          // update the progress bar
          var msg = ("Checking " + ctxt.fileCount + " of " +
                     ctxt.fileTotal + " files...");
          ftpServer.progressBar.updateProgress(ctxt.fileCount, msg);

        } else {
          ftpServer.log("Switching to upload mode");
          // no more files to check
          // so swap out the callback
          self.updateProgressBarRefresh = self.updateProgressBar;

          // (re)set the context settings
          ctxt.filesList = ctxt.refreshList || [];
          ctxt.fileCount = 0;
          ctxt.fileTotal = ctxt.filesList.length;
          ctxt.byteCount = 0;

          // set the max value on the progress bar
          ftpServer.progressBar.setMaxvalue(ctxt.fileTotal);

          // and request the first file
          ftpServer._nextFile();
        }

      } catch (e) {
        // we had an unexpected problem, so we log and bail
        var msg = e;
        if (e.line) {
          msg += ' @' + e.line;
        }
        ftpServer._stop(msg, true);
        return false;
      }
      return true;
    }

    if (ctxt.refresh) {
      ctxt.refreshList = [];
      FtpConnection.backgroundTask = task;
      task.id = app.scheduleTask("FtpConnection.backgroundTask." +
                                 "updateProgressBarRefresh()",
                                 100, true);
    } else {
      FtpConnection.backgroundTask = task;
      task.id = app.scheduleTask("FtpConnection.backgroundTask." +
                                 "updateProgressBar()",
                                 100, true);
    }

  } catch (e) {
    self._result = "Error in ftp transfer setup: " + e;
    if (e.line) {
      self._result += '@' + e.line;
    }
    rc = false;
    self.close();
  }

  if (!rc) {
    return rc;
  }

  self.onCallback = function(reason, p_log, total) {
    try {
      var self = this;
      var ctxt = self._ctxt;
      var currentFile = ctxt.currentFile;

      if (self.trace) {
        var type = '';
        switch (reason) {
        case FtpConnection.reasonStart:     type = "reasonStart"; break;
        case FtpConnection.reasonProgress:  type = "reasonProgress"; break;
        case FtpConnection.reasonLog:       type = "reasonLog"; break;
        case FtpConnection.reasonComplete:  type = "reasonComplete"; break;
        case FtpConnection.reasonFailed:    type = "reasonFailed"; break;
        default: type = 'reasonUnknown';
        }
        var msg = type;
        if (currentFile) {
          type += ': ' + currentFile.name;
        }
        if (p_log) {
          type += ' : ' + p_log;
        }
        self.log(type);
      }

      var fobj = currentFile.fobj;

      // Complete
      if (reason == FtpConnection.reasonComplete) {
        //debugger;

        if (self.size(fobj.target) == fobj.file.length) {
          if (self.verbose) {
            $.writeln("AsyncFTP: Upload finished");
          }

          // we're good, so log a message and update the stats
          rc = self.onFileXfer(currentFile);
          currentFile.close();
          ctxt.currentFile = undefined;

          // one down, send the next
          self._nextFile();

        } else {
          ctxt.errorCount++;

          // we try 3 times to upload a file
          if (fobj.attempts < 3) {
            self.log("Local and remote sizes do not match for " +
                     fobj.file.name + ". Retrying...");

            var rc = self._putFile(fobj);
            if (!rc) {
              self._stop(self._msg, true);
            }

            return rc;

          } else {
            self.log("Failed to upload " + fobj.file.name + ".");

            // one failed, send the next
            self._nextFile();
          }
        }
      }

      // Failed
      if (reason == FtpConnection.reasonFailed) {
        if (self.verbose) {
          $.writeln("AsyncFTP: Upload failed");
        }

        ctxt.errorCount++;
        ctxt.currentFile.close();
        ctxt.currentFile = undefined;

        if (self.error == FtpConnection.errorTimedOut ||
            self.error == FtpConnection.errorProtocolError) {
          // We may also want to retry on other errors
          ctxt.ftpErrors++;

          if (ctxt.ftpErrors < 20) {
            self.log("Transfer for " + fobj.file.name +
                     " failed (" + ctxt.ftpErrors + "). Retrying...");

            var rc = self.close();
            if (!rc) {
              self.log("Error closing connection: " + self.errorString);
            }

            $.gc();

            self.url = self._ftpConfig.mode + "://" + self._ftpConfig.hostname;

            if (!self.open()) {
              var msg = ("Re-open failure: " + self.errorString);
              self._stop(msg, true);
              rc = false

            } else {
              var rc = self._putFile(fobj);
              if (!rc) {
                self._stop(self._msg, true);
              }
            }

            return rc;
          }
        }

        self._stop("Transfer terminated. FTP Error: " + self.errorString,
                   true);
      }

    } catch (e) {
      var msg = ("Error in ftp callback: " + e + " :: "  + self.errorString);
      if (e.line) {
        msg += (" @" + e.line);
      }
      self._stop(msg, true);
    }
  } // self.onCallback()

  if (!ctxt.refresh) {
  // submit the first file
    return self._nextFile();
  }
};

FtpConnection._inventoryFiles = function(folder, targetS, ctxt) {
  var cnt = 0;
  var files = folder.getFiles();

  for (var i = 0; i < files.length; i++) {
    var file = files[i];

    if (file instanceof Folder) {
      var subTarget = targetS + "/" + file.name;
      cnt += FtpConnection._inventoryFiles(file, subTarget, ctxt);

    } else {
      var fobj = {
        attempts: 0,
        file: file,
        targetDir: targetS,
        toString: function() { return (this.file.absoluteURI + '::' +
                                       this.targetDir); }
      }
      ctxt.filesList.push(fobj);

      if (!ctxt.targetDirs[targetS]) {
        ctxt.targetDirs[targetS] = [];
      }
      ctxt.targetDirs[targetS].push(fobj);
      cnt++;
    }
  }
  return cnt;
};

FtpConnection.prototype.onFileXfer = function(file) {
  var self = this;
  var ctxt = self._ctxt;

  if (self.progressBar) {
    if (self.progressBar.isDone) {
      return false;
    }
  }

  if (file) {
    self.log(ctxt.fileCount + " done.");
    ctxt.byteCount += file.length;
    ctxt.fileCount++;
  }

  return true;
};

//
// createProgressPalette
//   title     the window title
//   min       the minimum value for the progress bar
//   max       the maximum value for the progress bar
//   parent    the parent ScriptUI window (opt)
//   useCancel flag for having a Cancel button (opt)
//
//   onCancel  This method will be called when the Cancel button is pressed.
//             This method should return 'true' to close the progress window
//
FtpConnection.createProgressPalette = function(title, min, max,
                                               parent, useCancel) {
  var win = new Window('palette', title);
  win.note = win.add('statictext', undefined);
  win.note.preferredSize = [300,25];
  win.bar = win.add('progressbar', undefined, min, max);
  win.bar.preferredSize = [300, 20];

  win.parent = undefined;
  win.recenter = false;
  win.isDone = false;

  win.setMaxvalue = function(v) {
    var win = this;
    win.bar.maxvalue = v;
  };

  if (parent) {
    if (parent instanceof Window) {
      win.parent = parent;
    } else if (useCancel == undefined) {
      useCancel = !!parent;
    }
  }

  if (useCancel) {
    win.onCancel = function() {
      this.isDone = true;
      return true;  // return 'true' to close the window
    }

    win.cancel = win.add('button', undefined, 'Cancel');

    win.cancel.onClick = function() {
      var win = this.parent;
      try {
        win.isDone = true;
        if (win.onCancel) {
          var rc = win.onCancel();
          if (rc || rc == undefined) {
            win.close();
          }
        } else {
          win.close();
        }
      } catch (e) {
        win.close();
      }
    }

    win.onClose = function() {
      this.isDone = true;
      return true;
    }
  }

  win.updateProgress = function(val, text) {
    var win = this;

    if (win.isDone) {
      win.close();
      return;
    }

    if (val != win.bar.value) {
      win.bar.value = val;
    }

    win.note.text = text;
  }

  return win;
};

FtpConnection._implementationStr = function() {
  var clsMethods = ["_toBoolean", "_listProps", "createFrom", "_inventoryFiles",
                    "createProgressPalette"];
  var objMethods = ["forceMkdir", "onFileXfer", "putDirTree"];

  var istr = '';
  for (var i = 0; i < clsMethods.length; i++) {
    var meth = clsMethods[i];
    istr += ("FtpConnection." + meth + " = " + FtpConnection[meth] + ";\n\n");
  }
  for (var i = 0; i < objMethods.length; i++) {
    var meth = objMethods[i];
    istr += ("FtpConnection.prototype." + meth + " = " +
             FtpConnection.prototype[meth] + ";\n\n");
  }

  return istr;
};

//
// This is executed by non-Bridge CS3 apps
// Add more entry points as needed
//
RemoteFtpConnection = function() {
};
RemoteFtpConnection.createFrom = function(conf) {
  var ftp = new RemoteFtpConnection();
  ftp.conf = {};
  for (var idx in conf) {
    ftp.conf[idx] = conf[idx];
  }
  return ftp;
};

RemoteFtpConnection.LOAD_LIB_STR =
  ('// Load the webaccess library\n' +
   'if (ExternalObject.webaccesslib == undefined )  {\n' +
     'if (Folder.fs == "Windows") {\n' +
      'var _pathToLib = Folder.startup.fsName + "/webaccesslib.dll";\n' +
    '} else {\n' +
      'var _pathToLib = Folder.startup.fsName + "/webaccesslib.bundle";\n' +
      '// verify that the path is valid\n' +
    '}\n' +
    'var libfile = new File(_pathToLib);\n' +
    'ExternalObject.webaccesslib = new ExternalObject("lib:" + _pathToLib);\n' +
   '}\n\n');

//
// XXX this is untested
//
RemoteFtpConnection.prototype.testConnection = function() {
  var self = this;

  var br = XBridgeTalk.getAppSpecifier("bridge");
  if (!XBridgeTalk.startApplication(br)) {
    return false;
  }

  function testConnectionBR(ftpConf) {
    var rc = false;
    try {
      //BridgeTalk.bringToFront(BridgeTalk.appName);
      var msg = '';

      var ftp = FtpConnection.createFrom(ftpConf);
      ftp.close();
      rc = true;
      msg = "Connection to " + ftp.url + " is good!";

    } catch (e) {
      msg = "Unable to connect to " + ftpConf.hostname + ". '" + e + "'";
      rc = msg;
    }

    if (ftp) {
      ftp.log(msg);
    }

    return rc.toSource();
  }

  var brCode = (RemoteFtpConnection.LOAD_LIB_STR +
                FTPConfig._implementationStr() +
                FtpConnection._implementationStr() +
                "testConnectionBR = " + testConnectionBR + ";\n\n" +
                 "testConnectionBR(" + self.conf.toSource() + ");");

  var bt = new BridgeTalk();
  bt.target = br;
  bt.onResult = function(res) {
    this._result = res.body;
  }
  bt.body = ("try { " + brCode + "} catch (e) { " +
             "BridgeTalk.bringToFront(BridgeTalk.appName); " +
             "alert('Bridge Error:' + e); }");
  var rc = bt.send(20);   // timeout in 20 second
  if (!rc) {
    return "Unable to connect to Bridge";
  }

  return eval(bt._result);
};
RemoteFtpConnection.prototype.putDirTree = function(folder, target, refresh) {
  var self = this;

  var br = XBridgeTalk.getAppSpecifier("bridge");
  if (!XBridgeTalk.startApplication(br)) {
    return false;
  }

  function putDirTreeBR(ftpConf) {
    BridgeTalk.bringToFront(BridgeTalk.appName);
    var res = '';
    try {
      var ftp = FtpConnection.createFrom(ftpConf);
      res = ftp.putDirTree(ftpConf.sourceDir, ftpConf.targetDir,
                           ftpConf.refresh);

    } catch (e) {
      res = "Error in ftp transfer: " + e;
      BridgeTalk.bringToFront(BridgeTalk.appName);
      alert(res);
    }
    //alert("Transfer started: " + res);
  }

  self.conf.sourceDir = folder.toString();
  self.conf.targetDir = target;
  self.conf.refresh = !!refresh;

  var brCode = (RemoteFtpConnection.LOAD_LIB_STR +
                FTPConfig._implementationStr() +
                FtpConnection._implementationStr() +
                "putDirTreeBR = " + putDirTreeBR + ";\n\n" +
                "putDirTreeBR(" + self.conf.toSource() + ");");

  var bt = new BridgeTalk();
  bt.target = br;
  bt.body = brCode;
  bt.send();

  return true;
};

// START OF TEST
// Copy this code into a separate files and adjust the include directives
// accordingly or just edit the code below directly and execute
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//-include "xlib/FTP.jsx"
//
//-include "xlib/XBridgeTalk.jsx"
//

RemoteFtpConnection._getTestConf = function() {
  var conf = new FTPConfig();

  conf.binary     = true;
  conf.passive    = false;
  if (true) {
    conf.sourceDir  = "/t/test/scratch/out";
    conf.targetDir  = "galleries/out";

    conf.username   = "xbytor";
    conf.password   = "P@ssword";
    conf.hostname   = "192.168.1.114";
  }

  if (false) {
    conf.sourceDir  = "/c/work/wgx/test/out";
    conf.targetDir  = "pub/out";
    conf.hostname   = "";
    conf.username   = "";
    conf.password   = "";
  }

  conf.logFile    = "~/ftp.log";
  conf.logEnabled = true;

  conf.refresh    = true;

  conf.verbose    = false;
  conf.trace      = false;

  return conf;
};
RemoteFtpConnection.test = function() {
  var conf = RemoteFtpConnection._getTestConf();
  var ftp = RemoteFtpConnection.createFrom(conf);

  ftp.putDirTree(new Folder(conf.sourceDir), conf.targetDir);
  $.writeln("Done.");
};

LocalFtpConnectionTest = function(refresh) {
  BridgeTalk.bringToFront(BridgeTalk.appName);
  try {
    var conf = RemoteFtpConnection._getTestConf();

    var ftp = FtpConnection.createFrom(conf);
    res = ftp.putDirTree(conf.sourceDir, conf.targetDir, refresh);

  } catch (e) {
    res = "Error in ftp transfer: " + e;
    alert(res);
  }
};
RemoteFtpConnection.connTest = function() {
  var conf = RemoteFtpConnection._getTestConf();
  var ftp = RemoteFtpConnection.createFrom(conf);

  var rc = ftp.testConnection();
  if (rc == true) {
    alert("Connection to " + conf.hostname + " is good.");
  } else {
    alert("Connection to " + conf.hostname + " is bad: " + rc);
  }
  $.writeln("Done.");
};


//RemoteFtpConnection.test();
//RemoteFtpConnection.connTest();
//LocalFtpConnectionTest(true);

// END OF TEST

"FTP.jsx";
// EOF
