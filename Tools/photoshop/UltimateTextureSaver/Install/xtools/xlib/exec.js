//
// Usage of this file is now officially deprecated
//
// exec.js
//    Execute external applications in PSCS-JS
// 
// Usage:
//    exec(cmd[, args])
//    bash(cmd[, args])
//      Executes the command 'cmd' with optional arguments. The
//      name of the file containing the output of that command
//      is returned.
//
// Examples:
//    exec("i_view32.exe", "/slideshow=c:\\images\\");
//
//    f = bash("~/exif/exifdump", fileName);
//    data = loadFile(f);
//
// Notes
//   The use of bash on XP requires that cygwin be installed.
//   Paths in the ExecOptions are XP paths. I'll put mac stuff
//      in later. Change if you don't like it.
//
// $Id: exec.js,v 1.6 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//

function ExecOptions() {
  var self = this;
  self.tempdir       = "c:\\temp\\";
  self.bash          = "c:\\cygwin\\bin\\bash";
  self.scriptPrefix  = "doExec-";
  self.captureOutput = true;
  self.captureFile   = "doExec.out"; // set this to undefined to get
                                     // a unique output file each time
  self.outputPrefix  = "doExec-";
};

function ExecRunner(opts) {
  var self = this;

  self.opts = opts || new ExecOptions();

  self.bash = function bash(exe, args) {
    var self = this;
    return self.doExec(self.opts.bash, "-c \"" + exe + " "  +
                       (args || ''), "  2>&1");
  };

  self.exec = function exec(exe, args) {
    var self = this;
    return self.doExec(exe, (args || ''), "");
  };

  self.doExec = function doExec(exe, args, redir) {
    var self = this;
    var opts = self.opts;
    if (exe == undefined) { throw "Must specify program to exec"; }

    args = (!args) ? '' : ' ' + args;

    // create a (hopefully) unique script name
    var ts = new Date().getTime();
    var scriptName = opts.tempdir + opts.scriptPrefix + ts + ".bat";

    var cmdline = exe + args;   // the command line in the .bat file
    
    // redirected output handling
    var outputFile = undefined;
    if (opts.captureOutput) {
      if (opts.captureFile == undefined) {
        outputFile = opts.tempdir + opts.outputPrefix + ts + ".out";
      } else {
        outputFile = opts.tempdir + opts.captureFile;
        new File(outputFile).remove();
      }
      // stderr redirect in cygwin
      cmdline += "\" > \"" + outputFile + "\" " + redir;
    }
    
    var script = new File(scriptName);
    if (!script.open("w")) {
      throw "Unable to open script file: " + script.fsName;
    }
    script.writeln(cmdline);
    script.close();
    if (!script.execute()) {
      throw "Execution failed for " + script.fsName;
    }

    return outputFile;
  }
};

function bash(exe, args) {
  return (new ExecRunner()).bash(exe, args, " 2>&1");
};
function exec(exe, args) {
  return (new ExecRunner()).exec(exe, args, "");
};

"exec.js"
// EOF
