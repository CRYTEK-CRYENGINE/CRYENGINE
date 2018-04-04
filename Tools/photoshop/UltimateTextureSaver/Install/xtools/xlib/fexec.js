//
// fexec.js
//
// $Id: fexec.js,v 1.14 2014/01/05 22:57:17 anonymous Exp $
// Copyright: (c)2006, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//include "xlib/xexec.js"
//

//
// Irfan - Windows XP only
//
Irfan = new Exec(new File("/c/Program Files/IrfanView/i_view32.exe"));
Irfan.slideshow = function(folder) {
  var args = ["/slideshow=" + folder.fsName, "/closeslideshow"];
  this.execute(args);
};
Irfan.info = function(arg) {
  if (!arg) {
    return '';
  }
  var args = [];

  if (arg instanceof File) {
    args.push('\"' + arg.fsName + '\"');
  } else if (arg instanceof Folder) {
    args.push('\"' + arg.fsName + "\\*\"");
  } else {
    args.push(arg.toString());
  }

  var ifile = new File(this.tmp + "/info.txt");

  args.push("/info=\"" + ifile.fsName + '\"');
  args.push("/killmesoftly");
  args.push("/silent");

  this.executeBlock(args, 10000);

  ifile.open("r");
  var str = ifile.read();
  ifile.close();
  ifile.remove();

  return str;
};

//
// SevenZip - Windows XP only
//
SevenZip = new Exec(new File("/c/Program Files/7-Zip/7z.exe"));
SevenZip.archive = function(zipFile, filelist) {
  var args = ["a", "-tzip", '\"' + decodeURI(zipFile.fsName) + '\"'];
  for (var i = 0; i < filelist.length; i++) {
    args.push('\"' + decodeURI(filelist[i].fsName) + '\"');
  }

  this.executeBlock(args, 10000);
};

//
// Zip - OS X only
//
Zip = new Exec(new File("/usr/bin/zip"));
Zip.archive = function(zipFile, filelist) {
  var args = ['-j \"' + decodeURI(zipFile.fsName) + '\"'];
  for (var i = 0; i < filelist.length; i++) {
    args.push('\"' + decodeURI(filelist[i].fsName) + '\"');
  }
  this.executeBlock(args, 10000);
};

//
// CVS
//
_cvspath = ($.os.match(/windows/i) ?
            "/c/cygwin/bin/cvs.exe" : "/usr/bin/cvs");

CVS = new Exec(new File(_cvspath));
delete _cvspath;

CVS.ROOT = "-d:pserver:anonymous@localhost:/opt/cvsroot";
CVS.commit = function(file, msg) {
  if (!msg) {
    msg = 'nc';
  }
  var args = [CVS.ROOT, "commit", "-m", "\"" + msg + "\""];
  args.push('\"' + decodeURI(file.name) + '\"');

  //var cmd = this.app.toUIString() +        ' ' + args.join(' ');
  //return Exec.system(cmd, 15000, file.parent, true);
  this.executeBlock(args, 15000, file.parent);
};


// Test code

function main() {
  var dir = new Folder("~/images");

//   Irfan.slideshow(dir);
//   SevenZip.archive(new File("/c/tmp/pics.zip"), dir.getFiles("*.jpg"));

  alert(Irfan.info(dir));
  alert(Exec.system("dir \"" + dir.fsName + "\"", 10000));
};

//main();

"Exec.jsx";
// EOF //
// Exec
