//
// xexec.js
//
// $Id: xexec.js,v 1.43 2014/01/05 22:56:36 anonymous Exp $
// Copyright: (c)2006, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

isWindows = function() {
  return $.os.match(/windows/i);
};
isMac = function() {
  return !isWindows();
};

Exec = function(app){
  this.app = app;         // A File object to the application
  if (app) {
    this.name = app.name;
  }

  // You can use a different tmp folder
  this.tmp = (isMac() ? '/private/var/tmp' : Folder.temp);
};

Exec.ERROR_CODE = 9003;

Exec.scriptsFolder =
  new Folder(app.path + '/' +
             localize("$$$/ScriptingSupport/InstalledScripts=Presets/Scripts"));


Exec.ext = (isWindows() ? ".bat" : ".sh");

Exec.hasSystemCall = function() { return !!app.system };

Exec.bashApp = (isWindows() ? "c:\\cygwin\\bin\\bash" : "/bin/bash");

Exec.setMacRoot = function(folder) {
  if (!folder) {
    folder = new Folder(Exec.scriptsFolder);
  }
  if (!folder.exists) {
    //
  }
  Exec.macRoot = folder;

  Exec.macApp = Exec.macRoot + "/macexec.app";
  Exec.macScript = Exec.macApp + "/Contents/macexec";
};

Exec.prototype.toCommandStr = function(args) {
  var str = '';

  if (this.app) {
    str = '\"' + decodeURI(this.app.fsName) + '\"'; // the app
  }

  if (args) {
    // if a file or folder was passed in, convert it to an array of one
    if (args instanceof File || args instanceof Folder) {
      args = ['\"' + args + '\"'];
    }

    // if its an array, append it to the command (could use Array.join)
    if (args.constructor == Array) {
      for (var i = 0; i < args.length; i++) {
        var arg = args[i];
        str += ' ' + arg;
      }
    } else {
      // if its something else (like a prebuilt command string), just append it
      str += ' ' + args.toString();
    }
  }
  return str;
};

Exec.prototype.getBatName = function() {
  if (!Exec.hasSystemCall() && isMac()) {
    return Exec.macScript;
  }

  var nm = '';
  var ts = new Date().getTime();

  if (this.app) {
    nm = this.tmp + '/' + this.app.name + '-' + ts + Exec.ext;
  } else {
    nm = this.tmp + "/exec-" + ts + Exec.ext;
  }
  return nm;
};

Exec.system = function(cmd, timeout, folder, redir, keepBat) {
  Exec.log("Exec.system(\"" + cmd + "\", " + timeout + ");");

  redir = (redir != false); // default is true

  if (!Exec.hasSystemCall() && isMac()) {
    return Exec.systemMac(cmd, timeout, folder, redir, keepBat);
  }
  var ts = new Date().getTime();
  var e = new Exec();
  e.name = "system";
  var outf = new File(e.tmp + "/exec-" + ts + ".out");

  if (redir) {
    cmd += "> \"" + outf.fsName + "\"\n";
  }

  e.executeBlock(cmd, timeout, folder, keepBat);

  var str = '';
  if (redir) {
    outf.open("r");
    str = outf.read();
    outf.close();
  }

  if (Exec.log.enabled) {
    Exec.log("==========================================================");
    Exec.log(str);
    Exec.log("==========================================================");
  }

  return str;
};

Exec.systemMac = function(cmd, timeout, folder, redir, keepBat) {
  if (Exec.hasSystemCall()) {
    return Exec.system(cmd, timeout, folder, redir, keepBat)
  }

  Exec.log("Exec.systemMac(\"" + cmd + "\", " + timeout + ");");

  if (!Exec.macRoot) {
    Exec.setMacRoot();
  }
  if (!Exec.macRoot.exists) {
    Error.runtimeError(Exec.ERROR_CODE, "This machine has not been correctly " +
                       "configured to run Exec.");
  }

  var ts = new Date().getTime();
  var e = new Exec();
  e.name = "systemMac";

  var outf = new File(e.tmp + "/exec-" + ts + ".out");
  var outname = outf.fsName;
  var semaphore = new File(outname + ".sem");

  if (folder) {
    folder = decodeURI(folder.fsName);
  }

  var script = File(Exec.macScript);
  script.length = 0;
  script.lineFeed = "unix";
  script.open("e");
  script.writeln("#!/bin/sh");

  if (folder) {
    script.writeln("cd \"" + folder + "\"");
  }

  var envScript = File(Exec.macScript + ".env");
  if (envScript.exists) {
    script.writeln("source \"" + envScript.fsName + "\"");
  }

  if (redir) {
    cmd += " > \"" + outname + "\"";
  }
  script.writeln(cmd);
  script.writeln("\necho Done > \"" + semaphore.fsName + "\"");
  script.writeln("exit 0");
  var len = script.tell();
  script.close();
  script.length = len;

  var app = File(Exec.macApp);
  var rc = app.execute();
  var msg = (rc ? "execute ok" : "execute failed: \"" + app.error + "\"");

  Exec.log(msg);

  if (!rc) {
    Error.runtimeError(Exec.ERROR_CODE, msg);
  }

  try {
    e.block(semaphore, timeout);

  } finally {
    semaphore.remove();
  }

  outf.open("r");
  var str = outf.read();
  outf.close();
  outf.remove();

  if (Exec.log.enabled) {
    Exec.log("==========================================================");
    Exec.log(str);
    Exec.log("==========================================================");
  }

  return str;
};

Exec.bash = function(cmd, timeout) {
  Exec.log("Exec.bash(\"" + cmd + "\", " + timeout + ");");

  var ts = new Date().getTime();
  var e = new Exec();
  e.name = "bash";

  var outf = new File(e.tmp + "/exec-" + ts + ".out");
  var cmdLine = Exec.bashApp;
  cmdLine += " -c \"" + cmd + "\" > \"" + outf.fsName + "\" 2>&1";
  e.executeBlock(cmdLine, timeout);

  outf.open("r");
  var str = outf.read();
  outf.close();
  outf.remove();

  if (Exec.log.enabled) {
    Exec.log("==========================================================");
    Exec.log(str);
    Exec.log("==========================================================");
  }
  return str;
};

Exec.openScript = function(script) {
  if (isWindows()) {
    script.open("w");

  } else {
    Exec.openMacScript(script);
  }
};
Exec.openMacScript = function(script) {
  script.length = 0;
  script.lineFeed = "unix";
  script.length = 0;
  script.open("e");
  script.writeln("#!/bin/bash");
};
Exec.closeScript = function(script) {
  if (isWindows()) {
    script.close();

  } else {
// osascript -e 'tell application "Terminal"' -e 'close front window' -e 'end tell'
    if (!Exec.hasSystemCall()) {
      script.writeln("/usr/bin/osascript -e 'tell application \"Terminal\"' " +
                     "-e 'close front window' -e 'end tell'");
    }

    // script.writeln("exit\n\n\n\n");

    var len = script.tell();
    script.close();
    // script.length = len;
  }
};

Exec.fsFileName = function(file) {
  return decodeURI(file.fsName);
};
Exec.prototype.execute = function(argList, folder) {
  var str = this.toCommandStr(argList);
  var bat = new File(this.getBatName());
  Exec.openScript(bat);

  if (folder) {
    //$.level = 1; debugger;
    var dir = Exec.fsFileName(folder);
    bat.writeln("cd \"" + dir + '\"');
    if (isWindows()) {
      bat.writeln(dir.substring(0, 2));
    }
  }

  bat.writeln(str);

  if (isWindows()) {
    bat.writeln("del \"" + Exec.fsFileName(bat) + "\" >NUL");
  }

  Exec.closeScript(bat);

  if (Exec.log.enabled) {
//     bat.open("r");
//     var str = bat.read();
//     bat.close();
    Exec.log("==========================================================");
    Exec.log(str);
    Exec.log("==========================================================");
  }

  bat.execute();
};

Exec.prototype.executeCmd = function(cmd, folder) {
  var bat = new File(this.getBatName());

  Exec.openScript(bat);

  if (folder) {
    //$.level = 1; debugger;
    var dir = Exec.fsFileName(folder);
    bat.writeln("cd \"" + dir + '\"');
    if (isWindows()) {
      bat.writeln(dir.substring(0, 2));
    }
  }
  bat.writeln(str);
  if (isWindows()) {
    bat.writeln("del \"" + Exec.fsFileName(bat) + "\" >NUL");
  }
  Exec.closeScript(bat);
  bat.execute();
};


Exec.blockForFile = function(file, timeout, progressBarTitle) {
  var pbar = undefined;
  if (progressBarTitle) {
    pbar = GenericUI.createProgressPalette(progressBarTitle, 0, 10,
                                           undefined, true);
    pbar.updateProgress(0);
  }

  try {
    //$.level = 1; debugger;
    Exec.log("Exec.blockForFile(" + file.toUIString() + ", " + timeout + ");");
    if (timeout == undefined) {
      timeout = 2000;
      Exec.log("timeout set to " + 2000);
    }
    if (timeout) {
      var parts = 20;   // default to 1/20 of timeout intervals

      // if the timeout is more than 20 seconds, check every 2 seconds
      if (timeout > 20000) {
        parts = Math.ceil(timeout/2000);
      }

      timeout = timeout / parts;

      Exec.log("parts = " + parts + ", timeout= " + timeout);

      while (!file.exists && parts) {
        if (pbar) {
          if (pbar.done) {
            break;
          }
          if (pbar.bar.value == 10) {
            pbar.bar.value = 0;
          }
          pbar.updateProgress(pbar.bar.value+1);
        }
        Exec.log('.');
        $.sleep(timeout);
        parts--;
      }
      Exec.log("wait complete. file.exists = " + file.exists +
               ", parts = " + parts);
    }

  } finally {
    if (pbar) {
      pbar.close();
    }
  }
  return file.exists;
};

Exec.prototype.block = function(semaphore, timeout) {
  if (!Exec.blockForFile(semaphore, timeout)) {
    var msg = "Timeout exceeded for program " + this.name;
    Exec.log(msg);
    Error.runtimeError(Exec.ERROR_CODE, msg);
  }
};

Exec.prototype.executeBlock = function(argList, timeout, folder, keepBatfile) {
  Exec.log("Exec.executeBlock(\"" + argList + "\", " + timeout + ", " +
           folder + ", " + keepBatfile + ");");

  var str = this.toCommandStr(argList);

  Exec.log("str = " + str);

  var bat = new File(this.getBatName());
  var semaphore = new File(bat.toString() + ".sem");
  var rm = (isWindows() ? "del" : "/bin/rm -f");
  Exec.log("bat = " + bat.toUIString() + ", sem = " + semaphore.toUIString() +
           ", rm = " + rm);

  var semname = semaphore.toUIString();

  var bstr = '';
  if (folder) {
    var dir = Exec.fsFileName(folder);
    bstr += "cd \"" + dir + "\"\n";
    if (isWindows()) {
      bstr += dir.substring(0, 2) + '\n';
    }
  }
  bstr += str + "\n\n";
  bstr += "echo Done > \"" + Exec.fsFileName(semaphore) + "\"\n";
  if (keepBatfile != true) { // && isWindows()) {
    bstr += rm + " \"" + Exec.fsFileName(bat) + "\"\n";
  }
  Exec.log(bstr);

  Exec.openScript(bat);
  bat.writeln(bstr);
  Exec.closeScript(bat);

  if (Exec.hasSystemCall()) {
    var cmdApp = (isWindows() ? "" : Exec.bashApp);
    var syscmd = '( ' +cmdApp + ' "' + bat.fsName + '") &';

    var rc = app.system(syscmd);

    var msg = (rc ? "execute ok" : "execute failed: \"" +
               bat.error + "\"");

    Exec.log(msg);
    if (rc) {
      Error.runtimeError(Exec.ERROR_CODE, msg);
    }
  } else {
    var rc = bat.execute();
    var msg = (rc ? "execute ok" : "execute failed: \"" +
               bat.error + "\"");

    Exec.log(msg);
    if (!rc) {
      Error.runtimeError(Exec.ERROR_CODE, msg);
    }
  }

  try {
    this.block(semaphore, timeout);

  } finally {
    semaphore.remove();
  }
};

Exec.log = function(msg) {
  var file;

  if (!Exec.log.enabled) {
    return;
  }

  if (!Exec.log.filename) {
    return;
  }

  if (!Exec.log.fptr) {
    file = new File(Exec.log.filename);
    if (file.exists) file.remove();
    if (!file.open("w")) {
      Error.runtimeError(9002, "Unable to open log file " +
                         file + ": " + file.error);
    }
    Exec.log.fptr = file;
    msg = "Exec Versions $Revision: 1.43 $\n\n" + msg;
  } else {
    file = Exec.log.fptr;
    if (!file.open("e"))  {
      Error.runtimeError(9002, "Unable to open log file " + file +
                         ": " + file.error);
    }
    file.seek(0, 2); // jump to the end of the file
  }
  var now = new Date();
  if (now.toISO) {
    now = now.toISO();
  } else {
    now = now.toString();
  }

  if (!file.writeln(now + " - " + msg)) {
    Error.runtimeError(Exec.ERROR_CODE, "Unable to write to log file " +
                       file + ": " + file.error);
  }

  file.close();
};
Exec.log.setFileName = function(name) {
  if (isWindows()) {
    var folder = new Folder("/c/temp");
    if (!folder.exists) {
      var path = folder.toUIString();
      folder = Folder.temp;
      Exec.log(path + " does not exist, changing to " + folder.toUIString());
    }
    var file = new File(folder + '/' + name);
    if (file.exists) {
      file.remove();
    }
    Exec.log.filename = file.absoluteURI;

  } else {
    var file = new File("/tmp/" + name);
    if (!file.parent.exists) {
      var path = file.toUIString();
      file = new File(Folder.temp + name);
      Exec.log(path + " does not exist, changing to " + file.toUIString());
    }
    if (file.exists) {
      file.remove();
    }
    Exec.log.filename = file.absoluteURI;
  }
};

Exec.log.setFileName("exec-psjs.log");
Exec.log.enabled = false;

if (!Exec.hasSystemCall() && isMac()) {
  Exec.setMacRoot();
}

"xexec.js";
// EOF
