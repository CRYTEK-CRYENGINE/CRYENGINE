//
// test stuff for atn2bin code
//
//@show include
//@includepath "/c/Program Files/Adobe/xtools"
//
//@include "xlib/Action.js"
//
var atn2binTestMode = true;
var act2bin_test = true;

// var atn2bin;
// if (!atn2bin) {
//   eval('//@include "/c/work/xml/atn2bin.jsx";\r');
//}

//
//============================== Test Stuff ===================================
//

var workDir = "/c/work/xml";
var dataDir = "/c/work/dat";
log = function() {};
log.writeln = function(str) {
  if (log.debugMode) {
    $.writeln(str);
  } else {
    log.str += str + "\r\n";
  }
};
log.show = function() {
  if (log.debugMode) {
  } else {
    if (log.str) alert(log.str);
  }
}
log.str = '';
log.debugMode = false;

log.writeln(new Date());


//-@include "atntest.js"

fillDesc = function() {
  var id70 = cTID("Fl  ");   // Sym
  var desc = new ActionDescriptor();
  desc.putEnumerated(cTID("Usng"), cTID("FlCn"), cTID("FrgC"));
  desc.putUnitDouble(cTID("Opct"), cTID("#Prc"), 100.0);
  var id76 = cTID("Md  ");
  var id77 = cTID("BlnM");
  var id78 = cTID("Nrml");
  desc.putEnumerated(cTID("Md  "), cTID("BlnM"), cTID("Nrml"));
  return desc;
};

undoDesc = function() {
  var id79 = cTID("slct");
  var desc = new ActionDescriptor();
  var ref = new ActionReference();
  ref.putEnumerated(cTID("HstS"), cTID("Ordn"), cTID("Prvs"));
  desc.putReference(cTID("null"), ref);
  return desc;
}

ActionSet.testWrite = function() {
  var act = new Action();
  act.expanded = true;
  act.name = "Act1";

  var item = new ActionItem();
  item.setEvent("fill");
  item.name = PSConstants.reverseNameLookup(cTID("Fl  "));
  item.descriptor = fillDesc();

  act.add(item);

  item = new ActionItem();
  item.setEvent("select");
  item.name = PSConstants.reverseNameLookup(cTID("slct"));
  item.descriptor = undoDesc();
  item.dialogOptions = 2;
  act.add(item);

  var as = new ActionSet();
  as.name = "ATN";
  as.expanded = true;
  as.add(act);

  //debugger;
  var str = new Stream();
  as.write(str);
  writeToFile(dataDir + "/atntest.bed", str.toStream());
};

ActionSet.testFile = function(inf, outf) {
  var af = ActionFile.readFrom(inf);

  af.write(outf);

  var cmp = Stdlib.compareFiles(inf, outf);
  return !cmp ? true : confirm(cmp);
};

ActionSet.testRW = function(fname, outFolder) {
  if (fname.charAt(0) != '/') {
    fname = dataDir + "/" + fname;
  }
  
  var inf = new File(fname);
  if (outFolder) {
    outf = new File(outFolder + "/" + inf.name);
  } else {
    outf = new File(inf.path + "/out/" + inf.name);
  }
  
  log.writeln("Processing " + fname + "\r\t" + outf);

  return ActionSet.testFile(inf, outf);
};
ActionSet.testReadWrite = function(folder, outFolder) {

  var files = Stdlib.getFiles(folder, /\.atn$/);

  for (var i = 0; i < files.length; i++) {
    if (!ActionSet.testRW(files[i].toString(), outFolder)) {
      break;
    }
  }
};

ActionSet.testRead = function(folder) {
  var files = Stdlib.getFiles(folder, /\.atn$/);
  var failures = '';
  for (var i = 0; i < files.length; i++) {
    try {
      ActionFile.readFrom(files[i]);
    } catch (e) {
      failures += files[i].name + " : " + e + "\r";
    }
  }
  if (failures) failures = "Failed reading files:\r" + failures;
  alert(ActionFile.objCount + " action files\r" +
        Action.objCount + " actions\r" +
        failures);
};

ActionsPalette.testRuntime = function() {
  var pal = new ActionsPalette();
  pal.readRuntime();
  var str = '';
  for (var i = 0; i < pal.count; i++) {
    var as = pal.byIndex(i);
    str += '[' + i + "] " + as.name + ':\r';
    for (var j = 0; j < as.count; j++) {
      var act = as.byIndex(j);
      str += '\t' + '[' + j + "] " + act.name + '\r';
    }
  }
  alert(str);
};

ActionsPalette.testFile = function() {
  var pal = new ActionsPalette();
  pal.readFile();
};

//ActionSet.testWrite();
//ActionSet.testReadWrite(new Folder(dataDir));
// ActionSet.testReadWrite(new Folder(app.path + "/Presets/Photoshop Actions"),
//                         new Folder(dataDir + "/out"));
//ActionSet.testRead(new Folder(dataDir + "/test"));

ActionSet.testRead(new Folder(app.path + "/Presets/Photoshop Actions"));

//ActionsPalette.testRuntime();

log.writeln("Stop  = " + Date());
log.show();

Stream.writeToFile(new File(Folder.temp + "/jslog.txt"), log.str);

"atn2bin-test.jsx"
