//
// StylesFile
//
// $Id: StylesFile.js,v 1.4 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//

StylesFile = function() {
  var self = this;
  self.file = null;
  self.styles = [];
};
StylesFile.prototype.typename = "StylesFile";

throwFileError = function(f, msg) {
  if (msg == undefined) msg = '';
  throw msg + + '\"' + f + "\": " + f.error + '.';
};

StylesFile.readFile = function(file) {
  file.open("r") || throwFileError(file, "Unable to open input file ");
  file.encoding = 'BINARY';
  var str = file.read();
  file.close();
  return str;
};

StylesFile.prototype.read = function(fptr) {
  var self = this;

  self.file = (fptr instanceof File) ? fptr : new File(fptr);
  var str = StylesFile.readFile(self.file);

  var re = /(\x00\w|\x00\d)(\x00\w|\x00\s|\x00\d|\x00\.|\x00\')+\x00\x00\$[-a-z\d]+/g;//'

  var parts = str.match(re);

  if (parts == null) {
    return;
  }

  for (var i = 0; i < parts.length; i++) {
    var p = parts[i];
    var sp = p.replace(/\x00/g, '').split('$');
    self.styles.push(new Styles(sp[0], '$' + sp[1]));
  }
};

Styles = function(name, id) {
  var self = this;
  self.name = name;
  self.id = id;
};
Styles.prototype.typename = "Styles";
Styles.prototype.toString = function() {
  return "{ name : \"" + this.name + "\", id : \"" + this.id + "\" }";
};

function _readStylesFile(infile) {
  var pf = new StylesFile();
  pf.read(infile);

  var str = '';
  for (var i = 0; i < pf.styles.length; i++) {
    str += pf.styles[i] + ",\r\n";
  }
  return str;
};

function main() {
  var stylesFolder = new Folder(app.path + "/Presets/Styles");
  var stylesFiles = stylesFolder.getFiles("*.asl");

  for (var i = 0; i < stylesFiles.length; i++) {
    var pfile = stylesFiles[i];
    var str = decodeURI(pfile.name) + "\r\n";
    str += _readStylesFile(pfile);
    alert(str);
  }
};

main();

"StylesFile.js";
// EOF
