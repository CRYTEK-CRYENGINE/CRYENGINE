//
// ToolPresetsFile
//
// $Id: ToolPresetsFile.js,v 1.3 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//

ToolPresetsFile = function() {
  var self = this;
  self.file = null;
  self.patterns = [];
};
ToolPresetsFile.prototype.typename = "ToolPresetsFile";

throwFileError = function(f, msg) {
  if (msg == undefined) msg = '';
  throw msg + + '\"' + f + "\": " + f.error + '.';
};

ToolPresetsFile.readFile = function(file) {
  file.open("r") || throwFileError(file, "Unable to open input file ");
  file.encoding = 'BINARY';
  var str = file.read();
  file.close();
  return str;
};

ToolPresetsFile.prototype.read = function(fptr) {
  var self = this;

  self.file = (fptr instanceof File) ? fptr : new File(fptr);
  var str = ToolPresetsFile.readFile(self.file);

  var re = /(\x00\w|\x00\d)(\x00\w|\x00\s|\x00\d)+\x00\x00\$[-a-z\d]+/g;
  var parts = str.match(re);

  if (parts == null) {
    return;
  }

  for (var i = 0; i < parts.length; i++) {
    var p = parts[i];
    var sp = p.replace(/\x00/g, '').split('$');
    self.patterns.push(new ToolPresets(sp[0], '$' + sp[1]));
  }
};

ToolPresets = function(name, id) {
  var self = this;
  self.name = name;
  self.id = id;
};
ToolPresets.prototype.typename = "ToolPresets";
ToolPresets.prototype.toString = function() {
  return "{ name : \"" + this.name + "\", id : \"" + this.id + "\" }";
};

function readToolPresetsFile(infile) {
  var pf = new ToolPresetsFile();
  pf.read(infile);

  var str = '';
  for (var i = 0; i < pf.patterns.length; i++) {
    str += pf.patterns[i] + ",\r\n";
  }
  return str;
};

function main() {
  var patternFolder = new Folder(app.path + "/Presets/Tools");
  var patternFiles = patternFolder.getFiles("*.tpl");

  for (var i = 0; i < patternFiles.length; i++) {
    var pfile = patternFiles[i];
    var str = decodeURI(pfile.name) + "\r\n";
    str += readToolPresetsFile(pfile);
    alert(str);
  }
};

main();

"ToolPresets.js";
// EOF
