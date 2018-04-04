//
// test stuff for atn2xml code
//
//@target photoshop
//
//@includepath "/c/Program Files/Adobe/xtools"
//
//@include "xlib/Action.js"
//
//
var atn2xml_test = true;
var atn2xmlTestMode = true;

var atn2bin;
if (!atn2bin) {
  eval('//@include "/c/work/xml/atn2bin.jsx";\r');
}

// var atn2xml;
// if (!atn2xml) {
//   eval('//@include "/c/work/xml/atn2xml.jsx";\r');
// }

//@include "/c/work/xml/atntest.js"

var workDir = "/c/work/xml";
var dataDir = "/c/work/dat";

//
//================================ Test Stuff =================================
//

XMLTest2 = function() {
  var ad = createTestAD2();
  var id = cTID("Mk  ");
  var str = ad.serialize(id);
  return str;
};

XMLTest = function() {
  ////debugger;
  var ad = createTestAD1();
  var id = cTID("Mk  ");
  var str = ad.serialize(id);
  return str;
};

if (false) {
  var xml = XMLTest();
  Stdlib.writeToFile(new File(dataDir + "/atn.xml"), xml);
  var str = Stream.readFrom(dataDir + "/atn.xml");
  var ad = XMLReader.deserialize(str);
  var ad1 = createTestAD1();
  var str  = ad.toStream();
  var str1 = ad1.toStream();
  alert(str1 == str);
  Stream.writeTo(dataDir + "/ad1.bin", ad1.toStream());
  Stream.writeTo(dataDir + "/ad1x.bin", ad.toStream());
  if (ad.isEqual(ad1)) {
    alert("atn->xml->atn OKAY");
  } else {
    alert("atn->xml->atn FAILED");
  }
}

strcmp = function(s1, s2) {
  alert(s1.length + ":" + s2.length);
  if (s1.length != s2.length) return false;
  for (var i = 0; i < s1.length; i++) {
    if (s1.charAt(i) != s2.charAt(i)) {
      debugger;
      return false;
    }
  }
  return true;
};

var useFiles = true;
var testResults = '';
testDriver = function(ftn, name, writeXmlOnly) {
  // writeXmlOnly = true; // XXX
  var ad;
  if (typeof ftn == "function") {
    ad = ftn();
  } else {
    ad = ftn;
  }
  var adstr = ad.toStream();
  if (useFiles) Stream.writeTo(dataDir + "/" + name + ".bin", adstr);
  var xstr =  ad.serialize(cTID("Mk  "));
  if (useFiles) Stream.writeTo(dataDir + "/" + name + ".xml", xstr);

  if (!writeXmlOnly) {
    var xad = XMLReader.deserialize(xstr);
    var xadstr = xad.toStream();
    if (useFiles) Stream.writeTo(dataDir + "/" + name + "-x.bin", xadstr);

    var ok = (xadstr == adstr);
    testResults += (name + (ok ? " PASS" : " FAIL")) + '\r';
    if (useFiles && !ok) {
      xstr =  xad.serialize(cTID("Mk  "));
      Stream.writeTo(dataDir + "/" + name + "-out.xml", xstr);
    }
  }
};

//
// experimental for running ScriptingListenerJS output
//
testExecuteAction = function(id, ad, mode) {
  if (this.count == undefined) this.count = 0;
  this.count++;
  testDriver(ad, "exec-" + this.count);
};
runExecTest = function() {
  try {
    str = Stream.readFrom(workDir + "/extest.js");
    str = str.replace(/executeAction/g, "testExecuteAction");
    eval(str);
  } finally {
  }
};

testActionFile = function(inf, outf) {
  var actf = ActionFile.readFrom(inf);

  var str = actf.serialize(inf.absoluteURI);
  Stdlib.writeToFile(outf, str);
};
testActionsPaletteFile = function() {
  var apf = new ActionsPaletteFile();
  var file = new File(app.preferencesFolder + "/Actions Palette.psp");
  apf.read(file);
  var str = apf.serialize(file.toString());
  Stdlib.writeToFile(new File("/c/work/dat/ActionsPalette.xml"), str);
};

//runExecTest();

// testDriver(test1, "test1");
// testDriver(test2, "test2");
// testDriver(test3, "test3");
// testDriver(test4, "test4");
// testDriver(testActionDescriptor, "testActionDescriptor");
// testDriver(testActionList, "testActionList");
// testDriver(testActionReference, "testActionReference");
// testDriver(testSimpleActionReference, "testSimpleActionReference", true);

//testActionFile(new File(dataDir + "/XMLTest.atn"), new File(dataDir + "/XMLTest.xml"));

testActionsPaletteFile();
//testDriver(atnbintest, "atnbintest");

if (testResults) {
  alert(testResults);
}

"atn2xml-test.jsx";
