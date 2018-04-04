//
// XBridgeTalk.jsx
//
// $Id: XBridgeTalk.jsx,v 1.45 2012/04/02 19:34:11 anonymous Exp $
// Copyright: (c)2008, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//

XBridgeTalk = function() {};

XBridgeTalk.ERROR_CODE = 9006;
XBridgeTalk.IO_ERROR_CODE = 9002;  // same as Stdlib.IO_ERROR_CODE

//
// XBridgeTalk._getSource
//   In CS2, the function 'toSource()' is broken for functions. The return
//   value includes the text of the function plus whatever else is left
//   in the file.
//   This function gets around the problem if you follow some conventions.
//   First, the function definition _must_NOT_ end with a ';'.
//   Second, the next line must contain '// EOF'
//   The result string can then be passed on to Bridge as needed.
//   ftn is the (variable) name of the function
//
XBridgeTalk._getSource = function(ftn) {
  if (XBridgeTalk.isCS2()) {
    var str = ftn.toSource();
    var len = str.indexOf("// EOF");
    return (len == -1) ? str : str.substring(0, len) + ')';
  }

  return ftn.toSource();
};

XBridgeTalk.hasBridge = function() {
  try {
    BridgeTalk;
    return true;

  } catch (e) {
    return false;
  }
};

XBridgeTalk.isCS2 = function() {
  var lvl = $.level;
  $.level = 0;
  try {
    BridgeTalk;
  } catch (e) {
    $.level = lvl;
    return false;
  }
  $.level = lvl;
  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^9\./);
  }
  if (appName == 'bridge') {
    return version.match(/^1\./);
  }
  if (appName == 'estoolkit') {
    return version.match(/^1\./);
  }
  if (appName == 'indesign') {
    return version.match(/^4\./);
  }
  if (appName == 'golive') {
    return version.match(/^8\./);
  }
  if (appName == 'acrobat') {
    return version.match(/^7\./);
  }
  if (appName == 'helpcenter') {
    return version.match(/^1\./);
  }

  return false;
};

XBridgeTalk.isCS3 = function() {
  var lvl = $.level;
  $.level = 0;
  try {
    BridgeTalk;
  } catch (e) {
    $.level = lvl;
    return false;
  }
  $.level = lvl;
  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^10\./);
  }
  if (appName == 'bridge') {
    return version.match(/^2\./);
  }
  if (appName == 'estoolkit') {
    return version.match(/^2\./);
  }
  if (appName == 'indesign') {
    return version.match(/^5\./);
  }
  if (appName == 'devicecentral') {
    return version.match(/^1\./);
  }
  return false;
};

XBridgeTalk.isCS4 = function() {
  var lvl = $.level;
  $.level = 0;
  try {
    BridgeTalk;
  } catch (e) {
    $.level = lvl;
    return false;
  }
  $.level = lvl;
  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^11\./);
  }
  if (appName == 'bridge') {
    return version.match(/^3\./);
  }
  if (appName == 'estoolkit') {
    return version.match(/^3\./);
  }
  if (appName == 'indesign') {
    return version.match(/^6\./);
  }
  if (appName == 'devicecentral') {
    return version.match(/^1\./);
  }
  return false;
};

XBridgeTalk.isCS5 = function() {
  var lvl = $.level;
  $.level = 0;
  try {
    BridgeTalk;
  } catch (e) {
    $.level = lvl;
    return false;
  }
  $.level = lvl;
  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^12\./);
  }
  if (appName == 'bridge') {
    return version.match(/^4\./);
  }
  // XXX fix this..
  if (appName == 'estoolkit') {
    return version.match(/^3\.5/);
  }

  return false;
};

XBridgeTalk.isCS6 = function() {
  var lvl = $.level;
  $.level = 0;
  try {
    BridgeTalk;
  } catch (e) {
    $.level = lvl;
    return false;
  }
  $.level = lvl;
  var appName = BridgeTalk.appName;
  var version = BridgeTalk.appVersion;

  if (appName == 'photoshop') {
    return app.version.match(/^13\./);
  }
  if (appName == 'bridge') {
    return version.match(/^5\./);
  }
  // XXX fix this..
  if (appName == 'estoolkit') {
    return version.match(/^3\.8/);
  }

  return false;
};

XBridgeTalk.log = function(msg) {
  var self = this;
  var file;

  function currentTime() {
    var date = new Date();
    var str = '';
    var timeDesignator = 'T';
    function _zeroPad(val) { return (val < 10) ? '0' + val : val; }
    str = (date.getFullYear() + '-' +
           _zeroPad(date.getMonth()+1,2) + '-' +
           _zeroPad(date.getDate(),2));
    str += (timeDesignator +
            _zeroPad(date.getHours(),2) + ':' +
            _zeroPad(date.getMinutes(),2) + ':' +
            _zeroPad(date.getSeconds(),2));
    return str;
  };

  msg = currentTime() + " - " + msg;

  if (!self.log.enabled) {
    return;
  }
  if (!self.log.filename) {
    return;
  }

  if (!self.log.fptr) {
    file = new File(self.log.filename);
    if (!file.open("e"))  {
      Error.runtimeError(XBridgeTalk.IO_ERROR_CODE,
                         "Unable to open log file(1) " +
                         file + ": " + file.error);
    }

    if (self.log.append) {
      file.seek(0, 2); // jump to the end of the file

    } else {
      file.close();
      file.length = 0;
      file.open("w");
    }

    self.log.fptr = file;

  } else {

    file = self.log.fptr;
    if (!file.open("e"))  {
      Error.runtimeError(XBridgeTalk.IO_ERROR_CODE,
                         "Unable to open log file(2) " +
                         file + ": " + file.error);
    }
    file.seek(0, 2); // jump to the end of the file
  }

  if (!$.os.match(/windows/i)) {
    file.lineFeed = "unix";
  }

  if (self.log.encoding) {
    file.encoding = self.log.encoding;
  }

  if (!file.writeln(msg)) {
    Error.runtimeError(XBridgeTalk.IO_ERROR_CODE,
                       "Unable to write to log file(3) " +
                       file + ": " + file.error);
  }

  file.close();
};
XBridgeTalk.exceptionMessage = function(e) {
  var str = '';
  var fname = (!e.fileName ? '???' : decodeURI(e.fileName));
  str += "   Message: " + e.message + '\n';
  str += "   File: " + fname + '\n';
  str += "   Line: " + (e.line || '???') + '\n';
  str += "   Error Name: " + e.name + '\n';
  str += "   Error Number: " + e.number + '\n';

  if (e.source) {
    var srcArray = e.source.split("\n");
    var a = e.line - 10;
    var b = e.line + 10;
    var c = e.line - 1;
    if (a < 0) {
      a = 0;
    }
    if (b > srcArray.length) {
      b = srcArray.length;
    }
    for ( var i = a; i < b; i++ ) {
      if ( i == c ) {
        str += "   Line: (" + (i + 1) + ") >> " + srcArray[i] + '\n';
      } else {
        str += "   Line: (" + (i + 1) + ")    " + srcArray[i] + '\n';
      }
    }
  }

  if ($.stack) {
    str += '\n' + $.stack + '\n';
  }

  return str;
};
XBridgeTalk._getPreferencesFolder = function() {
  var appData = Folder.userData;

  var folder = new Folder(appData + "/xtools");

  if (!folder.exists) {
    folder.create();
  }

  return folder;
};

XBridgeTalk.log.filename = (XBridgeTalk._getPreferencesFolder() +
                            "/xbridge.log");
XBridgeTalk.log.enabled = false;
XBridgeTalk.log.append = false;
XBridgeTalk.log.setFile = function(file, encoding) {
  XBridgeTalk.log.filename = filename;
  XBridgeTalk.log.enabled = filename != undefined;
  XBridgeTalk.log.encoding = encoding;
};

XBridgeTalk.getAppSpecifier = function(appName) {
  var rev = undefined;

//   $.level = 1; debugger;

  if (XBridgeTalk.isCS2()) {
    if (appName == 'photoshop') {
        rev = '9';
    }
    if (appName == 'bridge') {
        rev = '1';
    }
    if (appName == 'estoolkit') {
        rev = '1';
    }
    if (appName == 'golive') {
        rev = '8';
    }
    if (appName == 'acrobat') {
        rev = '7';
    }
    if (appName == 'helpcenter') {
        rev = '1';
    }
    // add other apps here
  }

  if (XBridgeTalk.isCS3()) {
    if (appName == 'photoshop') {
        rev = '10';
    }
    if (appName == 'bridge') {
        rev = '2';
    }
    if (appName == 'estoolkit') {
        rev = '2';
    }
    // add other apps here
  }

  if (XBridgeTalk.isCS4()) {
    if (appName == 'photoshop') {
        rev = '11';
    }
    if (appName == 'bridge') {
        rev = '3';
    }
    if (appName == 'estoolkit') {
        rev = '3';
    }
    // add other apps here
  }

  if (XBridgeTalk.isCS5()) {
    if (appName == 'photoshop') {
        rev = '12';
    }
    if (appName == 'bridge') {
        rev = '4';
    }
    if (appName == 'estoolkit') {
        rev = '3.5';
    }
    // add other apps here
  }

  if (XBridgeTalk.isCS6()) {
    if (appName == 'photoshop') {
        rev = '13';
    }
    if (appName == 'bridge') {
        rev = '5';
    }
    if (appName == 'estoolkit') {
        rev = '3.8';
    }
    // add other apps here
  }

  return (rev ? (appName + '-' + rev) : appName);

//   if (!prefix) {
//     return undefined;
//   }

//   var targets = BridgeTalk.getTargets(null);

//   var appSpec = undefined;
//   var rex = new RegExp('^' + prefix);

//   // find the most recent minor version
//   for (var i = 0; i < targets.length; i++) {
//     var spec = targets[i];
//     if (spec.match(rex)) {
//       appSpec = spec;
//     }
//   }
//   return appSpec;
};

XBridgeTalk.splitSpecifier = function(appSpec) {
  var rex = /([a-z]+)(_\d+)?(?:-([\d\.]+)(-[a-z]{2}_[a-z]{2})?)?/;
  var str = appSpec.toString();
  var m = str.match(rex);
  // m[1] name (required)
  // m[2] instance (opt)
  // m[3] version (opt)
  // m[4] locale (opt)
  if (!m) {
    return undefined;
  }
  var spec = {};
  spec.name = m[1];
  if (m[2]) {
    spec.instance = m[2].substr(1);
  }
  if (m[3]) {
    spec.version = m[3];
    var v = spec.version.split('.');
    spec.majorVersion = v[0];
    spec.minorVersion = (v[1] != undefined) ? v[1] : "0";
  }
  if (m[4]) {
    spec.locale = m[4].substr(1);
  }

  function specAsString() {
    var self = this;
    var str = self.name;
    if (self.instance) {
      str += '_' + self.instance;
    }
    if (self.version) {
      str += '-' + self.version;
    }
    if (self.locale) {
      str += '-' + self.locale;
    }

    return str;
  }

  spec.toString = specAsString;

  return spec;
};

XBridgeTalk.runSpecTest = function() {
  var specTest = ["photoshop",
                  "bridge-2.0",
                  "indesign_1-5.0",
                  "illustrator-13.0",
                  "illustrator-13.0-de_de",
                  ];
  var str = '';
  for (var i = 0; i < specTest.length; i++) {
    str += XBridgeTalk.splitSpecifier(specTest[i]) + '\r\n';
  }
  return str;
};


XBridgeTalk.START_DELAY = 1000;           // 2 seconds
XBridgeTalk.START_TIMEOUT = 1000 * 120;   // 2 minutes

XBridgeTalk.DEFAULT_TIMEOUT = 10;  // seconds

XBridgeTalk.ping = function(appSpec, timeout) {
  if (timeout == undefined) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }
  var bt = new BridgeTalk();
  bt.target = appSpec;
  bt.body = "var t = \'" + appSpec + "\';";
  bt.onResult = function(btObj) {
    return true;
  };
  return bt.sendSync(timeout);
};

XBridgeTalk.isRunning = function(appSpec) {
  var ok = false;

  var spec = XBridgeTalk.splitSpecifier(appSpec);
  var name = spec.name;

  spec = XBridgeTalk.getAppSpecifier(name);

  if (BridgeTalk.isRunning(spec)) {
    try {
      if (XBridgeTalk.ping(spec)) {
        ok = true;
      }

    } catch (e) {
      XBridgeTalk.log(XBridgeTalk.exceptionMessage(e));
    }

    if (!ok) {
      Error.runtimeError(XBridgeTalk.IO_ERROR_CODE,
                         "Unable to communicate with " + appSpec +
                         ". 'ping' timed out.");
    }
  } else {
    if (BridgeTalk.isRunning(name)) {
      Error.runtimeError(XBridgeTalk.IO_ERROR_CODE,
                         "Unable to communicate with " + spec +
                         ". Possible wrong version running.");
    }
  }

  return ok;
};

XBridgeTalk.startApplication = function(appSpec) {
  XBridgeTalk.log("XBridgeTalk.startApplication(" + appSpec + ")");

  if (!appSpec) {
    Error.runtimeError(19, "appSpec");
  }
  var spec = XBridgeTalk.splitSpecifier(appSpec);
  if (!spec) {
    Error.runtimeError(19, appSpec);
  }

  var appSpec = spec.toString();
  if (!XBridgeTalk.isApplicationInstalled(appSpec)) {
    Error.runtimeError(56, appSpec);
  }

  var appName = spec.name;

  if (BridgeTalk.isRunning(appSpec)) {
    var ok = false;
    try {
      if (XBridgeTalk.ping(appSpec)) {
        ok = true;
      }

    } catch (e) {
      XBridgeTalk.log(XBridgeTalk.exceptionMessage(e));
    }

    if (!ok) {
      Error.runtimeError(XBridgeTalk.IO_ERROR_CODE,
                         "Unable to communicate with " + appSpec +
                         ". Possible wrong version running.");
    }
  }

  if (!BridgeTalk.isRunning(appSpec)) {
    XBridgeTalk.log("Launching " + appSpec);
    BridgeTalk.launch(appSpec);

    var cnt = 0;
    while (!BridgeTalk.isRunning(appSpec)) {
      $.sleep(XBridgeTalk.START_DELAY);

      if ((++cnt) * XBridgeTalk.START_DELAY >= XBridgeTalk.START_TIMEOUT) {
        XBridgeTalk.log(appSpec + " failed to launch(1)");
        return false;
      }
    }

    var cnt = 0;
    var startProp = "start" + appName;
    XBridgeTalk[startProp] = false;
    var startupComplete = false;

    while (!XBridgeTalk[startProp]) {

      // fix this block
      var bt = new BridgeTalk();
      bt.target = appSpec;
      bt.body = "var t = \'" + appSpec + "\';";
      bt.onResult = function(btObj) {
        var str = ('XBridgeTalk.start' + appName + ' = true;');
        eval(str);
      };

      bt.send();
      $.sleep(XBridgeTalk.START_DELAY);
      BridgeTalk.pump();
      if ((++cnt) * XBridgeTalk.START_DELAY >= XBridgeTalk.START_TIMEOUT) {
        delete XBridgeTalk[startProp];
        XBridgeTalk.log(appSpec + " failed to launch(2)");
        return false;
      }
    }
    delete XBridgeTalk[startProp];
  }

  return true;
};

XBridgeTalk.isApplicationInstalled = function(appSpec) {
  var apps = BridgeTalk.getTargets(null);
  for (var i = 0; i < apps.length; i++) {
    if (apps[i].startsWith(appSpec)) {
      return true;
    }
  }
  return false;
};

XBridgeTalk.getBridgeSelection = function(timeout) {
  if (timeout == undefined) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  function getBridgeFiles() {
    var v = app.document.selections;
    var files = [];
    for (var i = 0; i < v.length; i++) {
      var t = v[i];
      if (t != undefined) { files.push(t.spec); }
    }
    return files.toSource();
  }
  // EOF

  var src = XBridgeTalk._getSource(getBridgeFiles);

  var brCode = ("function _run() {\n" +
                "  var getBridgeFiles = " + src +
                ";\n\n" +
                "  return getBridgeFiles();\n" +
                "};\n" +
                "_run();\n");

  XBridgeTalk.log("XBridgeTalk.getBridgeSelection()");

//   var _dbg = "try {\n" + brCode + "} catch (e) {\nalert(e)\n}";
//   brCode = _dbg;

  var br = "bridge";
  if (!BridgeTalk.isRunning(br)) {
    XBridgeTalk.log("Bridge is not running.");
    return undefined;
  }
  var bt = new BridgeTalk();
  bt.target = br;
  bt.body = brCode;
  var str = bt.sendSync(timeout);
  var res = str ? eval(str) : [];
  XBridgeTalk.log(res);

  return res;
};

XBridgeTalk.send = function(brCode, timeout) {
  var br = "bridge";
  if (!BridgeTalk.isRunning(br)) {
    XBridgeTalk.log("Bridge is not running.");
    return undefined;
  }
  var bt = new BridgeTalk();
  bt.target = br;
  bt.body = brCode;
  var str = bt.sendSync(timeout);
  var res = (str ? eval(str) : '');

  XBridgeTalk.log(res);

  return res;
};


//
//XBridgeTalk.getMetadata(File("/c/tmp/207.jpg"))
//
XBridgeTalk.getMetadata = function(files, timeout) {
  // $.level = 1; debugger;

  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  if (XBridgeTalk.isCS2()) {
    Error.runtimeError(XBridgeTalk.ERROR_CODE,
                       "XBridgeTalk.getMetadata does not work in CS2");
  }

  var isFile = false;
  if (!(files instanceof Array)) {
    if (!(files instanceof File)) {
      Error.runtimeError(19, files);
    }
    files = [files];
    isFile = true;
  }

  function getMetadata(files) {
    var mds = [];
    for (var i = 0; i < files.length; i++) {
      var t = new Thumbnail(files[i]);
      mds.push(t.synchronousMetadata.serialize());
    }
    return mds.toSource();
  }
  // EOF

  var src = getMetadata.toSource();

  var brCode = ("function _run(files) {\n" +
                 "  var getMetadata = " + src + ";\n\n" +
                 "  return getMetadata(files);\n" +
                "};\n" +
                "_run(" + files.toSource() + ");\n");

  XBridgeTalk.log("XBridgeTalk.getMetadata()");

  var br = "bridge";
  if (!BridgeTalk.isRunning(br)) {
    XBridgeTalk.log("Bridge is not running.");
    return undefined;
  }
  var bt = new BridgeTalk();
  bt.target = br;
  bt.body = brCode;
  var str = bt.sendSync(timeout);
  var res = (str ? eval(str) : '');

  XBridgeTalk.log(res);

  return ((isFile && res && res.length == 1) ? res[0] : res);
};

//
//XBridgeTalk.getBitDepth(File("/c/tmp/207.jpg"))
//XBridgeTalk.getBitDepth(doc.fullName);
//
XBridgeTalk.getBitDepth = function(files, timeout) {
  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  if (XBridgeTalk.isCS2()) {
    Error.runtimeError(XBridgeTalk.ERROR_CODE,
                       "XBridgeTalk.getMetadata does not work in CS2");
  }

  var isFile = false;
  if (!(files instanceof Array)) {
    if (!(files instanceof File)) {
      Error.runtimeError(19, files);
    }
    files = [files];
    isFile = true;
  }

  function getBitDepth(files) {
    var bds = [];
    for (var i = 0; i < files.length; i++) {
      var t = new Thumbnail(files[i]);
      app.synchronousMode = true;
      t.core.preview.preview;
      app.synchronousMode = false;
      bds.push(t.core.quickMetadata.bitDepth);
    }

    return (bds.length == 1) ? bds[0] : bds.toSource();
  }

  var brCode = ("function _run(files) {\n" +
                 "  var getBitDepth = " + getBitDepth.toSource() + ";\n\n" +
                 "  return getBitDepth(files);\n" +
                "};\n" +
                "_run(" + files.toSource() + ");\n");

  XBridgeTalk.log("XBridgeTalk.getBitDepth()");

  var res = XBridgeTalk.send(brCode, timeout);

  XBridgeTalk.log(res);

  return ((isFile && res && res.length == 1) ? res[0] : res);
};

//
//XBridgeTalk.getKeywords(File("/c/tmp/207.jpg"))
//
XBridgeTalk.getKeywords = function(files, timeout) {
  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  var isFile = false;
  if (!(files instanceof Array)) {
    if (!(files instanceof File)) {
      Error.runtimeError(19, files);
    }
    files = [files];
    isFile = true;
  }

  function getKeywordsBR(files) {
    function _getKeywords(md) {
      var kwds = [];
      var ns = md.namespace;
      md.namespace = 'http://ns.adobe.com/photoshop/1.0/';
      try {
        for (var i = 0; i < md.Keywords.length; ++i) {
          kwds.push(md.Keywords[i]);
        }
      } finally {
        md.namespace = ns;
      }
      return kwds;
    }
    try {
      var res = [];
      for (var i = 0; i < files.length; i++) {
        var file = files[i];
        var th = new Thumbnail(file);
        var md = th.synchronousMetadata;
        var kwds = _getKeywords(md);
        var obj = {
          filename: file.absoluteURI,
          keywords: kwds
        };
        res.push(obj);
      }

      return res.toSource();

    } catch (e) {
      var msg = "Internal error (getKeywordsBR): " + Stdlib.exceptionMessage(e);
      alert(msg);
      Stdlib.log(msg);
      // $.level = 1; debugger;
      return [];
    }
  }
  // EOF

  var src = XBridgeTalk._getSource(getKeywordsBR);

  var brCode = ("function _run(files) {\n" +
                "  var getKeywordsBR = " + src +  ";\n\n" +
                "  return getKeywordsBR(files);\n" +
                "};\n" +
                "_run(" + files.toSource() + ");\n");

  var _dbg = ("$.writeln('parse OK'); try {\n" + brCode + "} " +
              "catch (e) {\nalert(e)\n}");

  // brCode = _dbg;

  XBridgeTalk.log("XBridgeTalk.getKeywords(" + files + ", " + timeout + ")");

  var str = XBridgeTalk.send(brCode, timeout);

  var res = (str ? eval(str) : []);

  XBridgeTalk.log("XBridgeTalk.getKeywords => " + res);

  return ((isFile && res && res.length == 1) ? res[0].keywords : res);
};

//
//XBridgeTalk.getMetadataValue(File("/c/tmp/207.jpg"),"photoshop:City");
//
XBridgeTalk.getMetadataValue = function(files, name, timeout) {
  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  var isFile = false;

  if (!(files instanceof Array)) {
    if (!(files instanceof File)) {
      Error.runtimeError(19, files);
    }
    files = [files];
    isFile = true;
  }

  //
  // the bs bs in the getMetadata function has to do with the fact
  // that getting \ characters correctly to Bridge via BridgeTalk
  // is busted. RegExp literals appear to be busted, too.
  //
  function getMetadata(files, cname) {
    var res = [];
    try {
      var bs = '\\'.charCodeAt(0);
      var bsCh = String.fromCharCode(bs);
      var rexStr = "^(.*" + bsCh + "/)([^" + bsCh + "/]+)$";
      var rex = RegExp(rexStr);
      var m = cname.match(rex);
      var ns = m[1];
      var pname = m[2];
      for (var i = 0; i < files.length; i++) {
        var th = new Thumbnail(files[i]);
        var md = th.synchronousMetadata;
        md.namespace = ns;
        res.push(md[pname]);
      }
    } catch (e) {
      alert(e);
    }
    return res.toSource();
  }
  // EOF

  var src = XBridgeTalk._getSource(getMetadata);

  var cname = XMPNameSpaces.getCanonicalName(name);
  if (!cname) {
    Error.runtimeError(XBridgeTalk.ERROR_CODE,
                       "Invalid metadata name: " + name);
  }

  var brCode = ("function _run(files, name, value) {\n" +
                "  var getMetadata = " + src + ";\n\n" +
                "  return getMetadata(files, name);\n" +
                "};\n" +
                "_run(" + files.toSource() + ", " + cname.toSource() + ");\n");

  var _dbg = ("$.writeln('parse OK'); try {\n" + brCode + "} " +
              "catch (e) {\nalert(e)\n}");

  // brCode = "$.writeln('parse OK');";

  XBridgeTalk.log("XBridgeTalk.getMetadata()");

  var res = XBridgeTalk.send(brCode, timeout);

  XBridgeTalk.log(res);

  return ((isFile && res && res.length == 1) ? res[0] : res);
};

//
//XBridgeTalk.getMetadataValues(File("/c/tmp/207.jpg"),["photoshop:City"]);
//
XBridgeTalk.getMetadataValues = function(file, names, timeout) {
  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  if (!(file instanceof File)) {
    Error.runtimeError(19, files);
  }

  //
  // the bs bs in the getMetadata function has to do with the fact
  // that getting \ characters correctly to Bridge via BridgeTalk
  // is busted. RegExp literals appear to be busted, too.
  //
  function getMetadata(file, cnames, prefixedNames) {
    var res = {};

    try {
      var th = new Thumbnail(file);
      var md = th.synchronousMetadata;
      var bs = '\\'.charCodeAt(0);
      var bsCh = String.fromCharCode(bs);
      var rexStr = "^(.*" + bsCh + "/)([^" + bsCh + "/]+)$";
      var rex = RegExp(rexStr);

      for (var i = 0; i < cnames.length; i++) {
        var cname = cnames[i];
        var m = cname.match(rex);
        var ns = m[1];
        var pname = m[2];
        md.namespace = ns;
        var nm = prefixedNames[i];
        var v = md[pname];
        if (v instanceof Array && v.length == 1) {
          res[nm] = v[0];
        } else {
          res[nm] =v;
        }
      }
    } catch (e) {
      alert(e);
    }
    return res.toSource();
  }
  // EOF

  var src = XBridgeTalk._getSource(getMetadata);

  var cnames = [];
  var pnames = [];
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    var cname = XMPNameSpaces.getCanonicalName(name);
    if (!cname) {
      Error.runtimeError(XBridgeTalk.ERROR_CODE,
                         "Invalid metadata name: " + name);
    }
    cnames.push(cname);
    if (name instanceof QName) {
      pnames[i] = XMPNameSpaces.convertQName(name);
    } else {
      pnames[i] = name;
    }
  }

  var brCode = ("function _run(file, names, prefixes) {\n" +
                "  var getMetadata = " + src + ";\n\n" +
                "  return getMetadata(file, names, prefixes);\n" +
                "};\n" +
                "_run(" + file.toSource() + ", " + cnames.toSource() +
                ", " + pnames.toSource() + ");\n");

  var _dbg = ("$.writeln('parse OK'); try {\n" + brCode + "} " +
              "catch (e) {\nalert(e)\n}");
//   brCode = _dbg;

  XBridgeTalk.log("XBridgeTalk.getMetadataValues()");

  var res = XBridgeTalk.send(brCode, timeout);

  XBridgeTalk.log(res);

  return res;
};

//
//XBridgeTalk.setMetadataValue(File("/c/tmp/207.jpg"),"photoshop:City","NYC");
//
XBridgeTalk.setMetadataValue = function(files, name, value, timeout) {
  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  if (!(files instanceof Array)) {
    if (!(files instanceof File)) {
      Error.runtimeError(19, files);
    }
    files = [files];
  }

  //
  // the bs bs in the setMetadata function has to do with the fact
  // that getting \ characters correctly to Bridge via BridgeTalk
  // is busted. RegExp literals appear to be busted, too.
  //
  function setMetadata(files, cname, value) {
    var rc = false;
    try {
      var bs = '\\'.charCodeAt(0);
      var bsCh = String.fromCharCode(bs);
      var rexStr = "^(.*" + bsCh + "/)([^" + bsCh + "/]+)$";
      var rex = RegExp(rexStr);
      var m = cname.match(rex);
      var ns = m[1];
      var pname = m[2];

      try {
        XMPMeta;
      } catch (e) {
        if (!ExternalObject.AdobeXMPScript) {
          ExternalObject.AdobeXMPScript = new ExternalObject('lib:AdobeXMPScript');
        }
      }
      for (var i = 0; i < files.length; i++) {
        var file = files[i];
        var xmpFile = new XMPFile(decodeURI(file.fsName), XMPConst.UNKNOWN,
                                  XMPConst.OPEN_FOR_UPDATE);
        var xmp = xmpFile.getXMP();

        if (value instanceof Array) {
          for (var j = 0; j < value.length; j++) {
            var val = value[j];
            xmp.appendArrayItem(ns, pname, val, 0, XMPConst.ARRAY_IS_ORDERED);
          }
        } else {
          xmp.deleteProperty(ns, pname);
          xmp.setProperty(ns, pname, value);
        }

        if (xmpFile.canPutXMP(xmp)) {
          xmpFile.putXMP(xmp);
        }

        xmpFile.closeFile(XMPConst.CLOSE_UPDATE_SAFELY);
      }

      rc = true;
    } catch (e) {
      alert(e);
    }
    return rc;
  }
  // EOF

  var src = XBridgeTalk._getSource(setMetadata);

  var cname = XMPNameSpaces.getCanonicalName(name);
  if (!cname) {
    Error.runtimeError(XBridgeTalk.ERROR_CODE,
                       "Invalid metadata name: " + name);
  }

  var brCode = ("function _run(files, name, value) {\n" +
                "  var setMetadata = " + src + ";\n\n" +
                "  return setMetadata(files, name, value);\n" +
                "};\n" +
                "_run(" + files.toSource() + ", " + cname.toSource() +
                ", " + value.toSource() + ");\n");

  var _dbg = ("$.writeln('parse OK'); try {\n" + brCode + "} " +
              "catch (e) {\nalert(e)\n}");
//   brCode = _dbg;

  XBridgeTalk.log("XBridgeTalk.setMetadata()");

  var res = XBridgeTalk.send(brCode, timeout);

  XBridgeTalk.log(res);

  return res;
};

//
// var obj = {};
// obj["photoshop:City"] = "NYC";
// obj["iptcCore:Provider"] = "XBytor";
//
//XBridgeTalk.setMetadataValues(File("/c/tmp/207.jpg"), obj);
//
XBridgeTalk.setMetadataValues = function(files, nvPairs, timeout) {
  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  if (!(files instanceof Array)) {
    if (!(files instanceof File)) {
      Error.runtimeError(19, files);
    }
    files = [files];
  }

  //
  // the bs bs in the setMetadata function has to do with the fact
  // that getting \ characters correctly to Bridge via BridgeTalk
  // is busted. RegExp literals appear to be busted, too.
  //
  // nvPairs is an object that looks like
  // { 'http://ns.adobe.com/camera-raw-settings/1.0/HasSettings': 'True' }
  //
  function setMetadata(files, nvPairs) {
    var rc = false;
    try {
      var bs = '\\'.charCodeAt(0);
      var bsCh = String.fromCharCode(bs);
      var rexStr = "^(.*" + bsCh + "/)([^" + bsCh + "/]+)$";
      var rex = RegExp(rexStr);

      try {
        XMPMeta;
      } catch (e) {
        if (!ExternalObject.AdobeXMPScript) {
          ExternalObject.AdobeXMPScript = new ExternalObject('lib:AdobeXMPScript');
        }
      }

      for (var i = 0; i < files.length; i++) {
        var file = files[i];
        var xmpFile = new XMPFile(decodeURI(file.fsName), XMPConst.UNKNOWN,
                                  XMPConst.OPEN_FOR_UPDATE);
        var xmp = xmpFile.getXMP();

        for (var cname in nvPairs) {
          var value = nvPairs[cname];

          var m = cname.match(rex);
          var ns = m[1];
          var pname = m[2];

          if (value instanceof Array) {
            for (var j = 0; j < value.length; j++) {
              var val = value[j];
              xmp.appendArrayItem(ns, pname, val, 0, XMPConst.ARRAY_IS_ORDERED);
            }
          } else {
            xmp.deleteProperty(ns, pname);
            xmp.setProperty(ns, pname, value);
          }
        }

        if (xmpFile.canPutXMP(xmp)) {
          xmpFile.putXMP(xmp);
        }

        xmpFile.closeFile(XMPConst.CLOSE_UPDATE_SAFELY);
      }

      rc = true;
    } catch (e) {
      alert(e);
    }
    return rc;
  }
  // EOF

  var src = XBridgeTalk._getSource(setMetadata);

  var mdObj = {};
  for (var name in nvPairs) {
    var cname = XMPNameSpaces.getCanonicalName(name);
    if (!cname) {
      Error.runtimeError(XBridgeTalk.ERROR_CODE,
                         "Invalid metadata name: " + name);
    }

    mdObj[cname] = nvPairs[name];
  }

  var brCode = ("function _run(files, name, value) {\n" +
                "  var setMetadata = " + src + ";\n\n" +
                "  return setMetadata(files, name, value);\n" +
                "};\n" +
                "_run(" + files.toSource() + ", " + mdObj.toSource() + ");\n");

  var _dbg = ("$.writeln('parse OK'); try {\n" + brCode + "} " +
              "catch (e) {\nalert(e)\n}");
//   brCode = _dbg;

  XBridgeTalk.log("XBridgeTalk.setMetadata()");

  var res = XBridgeTalk.send(brCode, timeout);

  XBridgeTalk.log(res);

  return res;
};

//
//
//XBridgeTalk.copyMetadataValues(File("/c/tmp/207.jpg"),
//                               File("/c/tmp/208.jpg"),
//                               ["photoshop:City", "dc:creator"]);
//
XBridgeTalk.APPEND_ON_COPY = 1;  // append array items
XBridgeTalk.REMOVE_ON_COPY = 2;  // always remove existing property value
XBridgeTalk.ON_COPY_DEFAULT = XBridgeTalk.APPEND_ON_COPY;

XBridgeTalk.copyMetadataValues = function(file, dest, names, opts, timeout) {
  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  if (opts == undefined) {
    opts = XMPTools.ON_COPY_DEFAULT;
  }

  if (!file.exists) {
    Error.runtimeError(9002, "File does not exist: " + file);
  }
  if (!dest.exists) {
    Error.runtimeError(9002, "File does not exist: " + dest);
  }
  if (!names || names.length == 0) {
    return false;
  }

  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    if (name instanceof QName) {
      continue;
    }

    if (name.constructor == String) {
      var ar = name.split(':');
      if (ar.length == 2) {
        names[i] = XMPNameSpaces.getQName(name);
        continue;
      }
    }
    Error.runtimeError(9002, "Bad metadata descriptor: " + name);
  }

  //
  // the bs bs in the setMetadata function has to do with the fact
  // that getting \ characters correctly to Bridge via BridgeTalk
  // is busted. RegExp literals appear to be busted, too.
  //
  // nvPairs is an object that looks like
  // { 'http://ns.adobe.com/camera-raw-settings/1.0/HasSettings': 'True' }
  //
  function copyMetadata(file, dest, names, opts) {
    function copyProperties(src, dest, qnames, opts) {
      for (var i = 0; i < qnames.length; i++) {
        var qname = qnames[i];
        var ns = qname.uri;
        var name = qname.localName;

        var prop = src.getProperty(ns, name);
        if (!prop) {
          continue;
        }
        dest.deleteProperty(ns, name);
        XMPUtils.duplicateSubtree(src, dest, ns, name, ns, name);
      }
    };

    var rc = false;
    try {
      try {
        XMPMeta;
      } catch (e) {
        if (!ExternalObject.AdobeXMPScript) {
          ExternalObject.AdobeXMPScript = new ExternalObject('lib:AdobeXMPScript');
        }
      }
      for (var i = 0; i < names.length; i++) {
        var name = names[i];
        if (name instanceof QName) {
          continue;
        }
        /*
        if (name.constructor == String) {
          var ar = name.split(':');
          if (ar.length != 2) {
            alert("Bad Metatdata descriptor: " + name);
            continue;
          }
        }
         */
        if (typeof name == "object") {
          names[i] = new QName(name.uri, name.localName);
          continue;
        }
        alert("Bad Metatdata descriptor: " + name);
      }

      var srcXMP = new XMPFile(decodeURI(file.fsName), XMPConst.UNKNOWN,
                               XMPConst.OPEN_FOR_READ).getXMP();

      var xmpFile = new XMPFile(decodeURI(dest.fsName), XMPConst.UNKNOWN,
                                XMPConst.OPEN_FOR_UPDATE);
      var xmp = xmpFile.getXMP();

      copyProperties(srcXMP, xmp, names, opts);

      if (xmpFile.canPutXMP(xmp)) {
        xmpFile.putXMP(xmp);
      }

      xmpFile.closeFile(XMPConst.CLOSE_UPDATE_SAFELY);

      rc = true;

    } catch (e) {
      alert(e);

      try { if (xmpFile) xmpFile.closeFile(); } catch (e) {}
    }

    return rc;
  }
  // EOF

  var src = XBridgeTalk._getSource(copyMetadata);

  var brCode = ("function _run(src, dest, names, opts) {\n" +
                "  var copyMetadata = " + src + ";\n\n" +
                "  return copyMetadata(src, dest, names, opts);\n" +
                "};\n" +
                "_run(" + file.toSource() + ", " + dest.toSource() + ", " +
                names.toSource() + ", " + opts.toSource() + ");\n");

  var _dbg = ("$.writeln('parse OK'); try {\n" + brCode + "} " +
              "catch (e) {\nalert(e)\n}");
  //brCode = _dbg;

  //brCode = "$.writeln('testing');";

  XBridgeTalk.log("XBridgeTalk.copyMetadata()");

  var res = XBridgeTalk.send(brCode, timeout);

  XBridgeTalk.log(res);

  return res;
};

//
//XBridgeTalk.chooseMenuItem("CRClear", File("/c/tmp/207.jpg"))
//
XBridgeTalk.chooseMenuItem = function(menuItem, files, timeout) {
  if (!timeout) {
    timeout = XBridgeTalk.DEFAULT_TIMEOUT;
  }

  if (!(files instanceof Array)) {
    if (!(files instanceof File)) {
      Error.runtimeError(19, files);
    }
    files = [files];
  }

  function applyMenuItem(menuItem, files) {
    try {
      /* we only want to select the files we want to operate on */
      app.document.deselectAll();

      for (var i = 0; i < files.length; i++) {
        var th = new Thumbnail(files[i]);

        app.document.select(th);
      }

      /* now choose the menu item for all of the selected files */
      app.document.chooseMenuItem(menuItem);

    } catch (e) {
      alert(e + '@' + e.line);
    }

    return true;
  }
  // EOF

  var src = applyMenuItem.toSource();

  var brCode = ("function _run(menuItem, files) {\n" +
                "  var applyMenuItem = " + src + ";\n\n" +
                "  return applyMenuItem(menuItem, files);\n" +
                "};\n" +
                "_run(\"" + menuItem + "\", " + files.toSource() + ");\n");

  XBridgeTalk.log("XBridgeTalk.applyMenuItem()");

  var res = XBridgeTalk.send(brCode, timeout);

  XBridgeTalk.log(res);

  return res;
};

XBridgeTalk._init = function() {
  if (!XBridgeTalk.hasBridge()) {
    Error.runtimeError(XBridgeTalk.ERROR_CODE,
                       "This application does not support Bridge");
  }

  BridgeTalk.prototype.sendSync = function(timeout) {
    XBridgeTalk.log("BridgeTalk.sendSync(" + timeout + ")");

    var self = this;

    //if (isCS3() || isCS4) {
    //  return this.send(timeout);
    //}

    self.onResult = function(res) {
      // $.writeln('onResult');
      this.result = res.body;
      this.complete = true;
    };
    self.complete = false;
    self.result = undefined;

    self.send();

    // ??? fix this so that its a doubling decay timeout
    if (timeout) {
      for (var i = 0; i < timeout; i++) {
        var rc = BridgeTalk.pump();       // process any outstanding messages
        // $.writeln(rc);
        if (!self.complete) {
          $.sleep(1000);
        } else {
          break;
        }
      }
    }

    var res = self.result;
    self.result = self.complete = self.onResult = undefined;

    XBridgeTalk.log("BridgeTalk.sendSync => " + res);

    return res;
  };

  BridgeTalk.prototype.sendSynch = BridgeTalk.prototype.sendSync;
};
XBridgeTalk._init();

XBridgeTalk.test = function() {
  var br = "bridge";
  if (!br) {
    alert("Bridge not installed");
    return;
  }
  if (!XBridgeTalk.isRunning(br)) {
    alert("Bridge is not running. Starting now...");
    if (!XBridgeTalk.startApplication(br)) {
      alert("Failed to launch " + br);
      return;
    }
    alert("Bridge started");
  }

  var bt = new BridgeTalk();
  bt.target = br;
  bt.body = "new Date().toString()";
  var res = bt.sendSync(10);
  alert(res);

  alert(XBridgeTalk.getBridgeSelection());

  //XBridgeTalk.getMetadata(File("/c/tmp/207.jpg"))
  //XBridgeTalk.getBitDepth(File("/c/tmp/207.jpg"))
  //XBridgeTalk.getBitDepth(doc.fullName);
  //XBridgeTalk.getKeywords(File("/c/tmp/207.jpg"))
  //XBridgeTalk.setMetadataValue(File("/c/tmp/207.jpg"),"photoshop:City","NYC");

  // obj = {};
  // obj["photoshop:City"] = "Antwerp";
  // obj["iptcCore:Provider"] = "XBytor";
  // XBridgeTalk.setMetadataValues(File("/c/tmp/207.jpg"), obj);
};

//XBridgeTalk.test();

"xbridge.jsx";
// EOF

