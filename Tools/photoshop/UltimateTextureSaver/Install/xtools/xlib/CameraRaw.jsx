//
// CameraRaw.jsx
//   Some code that makes working with Raw (or Raw-endowed) files. This script
//   requires that Bridge be running.
//
// External Interface
//   The 'files' paremeter to these functions can be a single file or an
//   array of files.
//
// Camera Menu Items in Bridge
//   CameraRaw.applyDefaultSettings(files)
//   CameraRaw.clearSettings(files)
//   CameraRaw.copySettings(files)
//   CameraRaw.pasteSettings(files)
//   CameraRaw.applyPreviousSettings(files)
//
//   CameraRaw.applyPreset(preset, files)
//     'preset' can a name (e.g. 'NikonD80'). If it is, the function looks for
//        the preset file in the 'standard' location.
//     'preset' can also be a File object, in which case it is used directly.
//
// Reading and Writing CR Settings directly
//   CameraRaw.applySettings(settings, files)
//   CameraRaw.getSettings(files)
//   CameraRaw.getSetting(name, files)
//      'settings' in an object. The names of the properties are also the
//      names of the Camera Raw settings. They can be in a simple form
//      (like 'Exposure') or they can have a prefix attached ('crs:Exposure').
//      If a prefix is present, it must be 'crs'. All others will be ignored.
//
//
// More stuff for later.
//   REG QUERY "HKCU\Software\Adobe\Camera Raw\4.1" /v Preferences
//   REG QUERY "HKCU\Software\Adobe\Camera Raw\4.1" /v PrefsTimeStamp
//
//
// $Id: CameraRaw.jsx,v 1.5 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2008, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//

CameraRaw = function() {};

CameraRaw.DEFAULT_TIMEOUT = 10;

CameraRaw.PRESETS_FOLDER = Folder(Folder.userData +
                                  "/Adobe/CameraRaw/Settings");

CameraRaw.NS_URI = 'http://ns.adobe.com/camera-raw-settings/1.0/';
CameraRaw.NS_PREFIX = 'crs';

CameraRaw.isBridge = function() { return app.name == "bridge"; };

CameraRaw.sendBridgeMessage = function(msg, timeout) {
  if (timeout == undefined) {
    timeout = CameraRaw.DEFAULT_TIMEOUT;
  }
  var br = "bridge";
  if (!BridgeTalk.isRunning(br)) {
    return [];
  }

  var bt = new BridgeTalk();
  bt.onResult = function(res) {
    this.result = res.body;
    this.complete = true;
  }

  bt.target = br;
  bt.body = msg;
  bt.send(timeout);
  var str = bt.result;
  var res = undefined;

  if (timeout > 0) {
    res = (str ? eval(str) : '');
  }

  return res;
};

CameraRaw._checkFilesArg = function(files) {
  if (!(files instanceof Array)) {
    if (!(files instanceof File)) {
      throw Error("Bad argument(1): " + files);
    }
    files = [files];
  }

  return files;
};

CameraRaw._createBridgeString = function(ftn, args) {
  var ftnSrc = ftn.toSource();
  var ftnName = ftn.name;
  var argsStr = '';

  if (args.length > 0) {
    argsStr = "arg1";
    for (var i = 1; i < args.length; i++) {
      argsStr += (", arg" + (i+1));
    }
  }
  var brCode = ("function _run(" + argsStr + ") {\n" +
                "  var " + ftnName + " = " + ftnSrc + ";\n\n" +
                "  return " + ftnName + "(" + argsStr + ");\n" +
                "};\n" +
                "_run(");

  if (args.length > 0) {
    var arg = args[0];
    brCode += arg.toSource();

    for (var i = 1; i < args.length; i++) {
      var arg = args[i];
      brCode += ", " + arg.toSource();
    }
  }

  brCode += ");\n";

  return brCode;
};

CameraRaw.chooseBridgeMenuItem = function(menuItem, fargs, timeout) {
  var files = CameraRaw._checkFilesArg(fargs);

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

  var res = undefined;
  if (CameraRaw.isBridge()) {
    var str = applyMenuItem(menuItem, files);
    res = (str ? eval(str) : '');

  } else {
    var brCode = CameraRaw._createBridgeString(applyMenuItem,
                                               [menuItem, files]);
    res = CameraRaw.sendBridgeMessage(brCode);
  }

  return res;
};

CameraRaw.applyDefaultSettings = function(files) {
  return CameraRaw.chooseBridgeMenuItem("CRDefault", files);
};
CameraRaw.clearSettings = function(files) {
  return CameraRaw.chooseBridgeMenuItem("CRClear", files);
};
CameraRaw.copySettings = function(files) {
  return CameraRaw.chooseBridgeMenuItem("CRCopy", files);
};
CameraRaw.pasteSettings = function(files) {
  return CameraRaw.chooseBridgeMenuItem("CRPaste", files);
};
CameraRaw.applyPreviousSettings = function(files) {
  return CameraRaw.chooseBridgeMenuItem("CRPrevious", files);
};

CameraRaw.getCameraRawPresetXMP = function(fname) {
  var file;

  if (fname instanceof File) {
    file = fname;

  } else {
    if (!fname.match(/\.xm(p|l)/i)) {
      fname += ".xmp";
    }
    file = File(CameraRaw.PRESETS_FOLDER + '/' + fname);
  }

  var xmp = undefined;

  if (file.exists) {
    xmp = CameraRaw.readXMLFile(file);
  }

  return xmp;
};

CameraRaw.readXMLFile = function(file) {
  file.encoding = "UTF8";
  file.lineFeed = "unix";
  file.open("r", "TEXT", "????");
  var str = file.read();
  file.close();

  return new XML(str);
};

//
// extractCRSFields
//    poke around in the XML for the camera raw fields
//    and collect them to send to Bridge
//
// This implementation assumes that the XMP file is in the canonical format.
// The function has only been tested with simple xml values (e.g. no arrays).
//
// This would be better done by iterating over a list of elements like this
//     x = xmp.xpath("/x:xmpmeta/rdf:RDF/rdf:Description")
//     x.elements()
//
// This works in Gecko so I assume PSJS is busted
//     x = xmp.xpath('//*[namespace-uri()="http://ns.adobe.com/camera-raw-settings/1.0/"]')
//
// This should also work
//     x = xmp.xpath("//crs:*");
//
// But they don't, so we collect all descendants and look at the URI of the
// names of the nodes
//
CameraRaw._extractCRSFields = function(xmp) {
  var crsURI = CameraRaw.NS_URI;

  var obj = {};
  var parts = xmp.descendants();
  var len = parts.length();
  
  for (var i = 0; i < len; i++) {
    var part = parts.child(i);
    var parent = part.parent();
    var name = parent.name();
    var uri = name.uri;
    if (uri == crsURI) {
      obj["crs:" + name.localName] = part.toString();
    }
  }

  return obj;
};
CameraRaw.applyPreset = function(preset, files) {
  var rc = false;
  var xmp = CameraRaw.getCameraRawPresetXMP(preset);

  if (!xmp) {
    return rc;
  }

  var obj = CameraRaw._extractCRSFields(xmp);

  return CameraRaw.applySettings(obj, files);
};


//
// applySettings
//   settings is an object containing new settings.
//   The name is of the form 'Exposure' or 'crs:Exposure'
//
CameraRaw.applySettings = function(settings, fargs) {
  var files = CameraRaw._checkFilesArg(fargs);

  function applySettings(settings, files) {
    var rc = false;
    var uri = 'http://ns.adobe.com/camera-raw-settings/1.0/';
    var prefix = 'crs';

    try {
      for (var i = 0; i < files.length; i++) {
        var th = new Thumbnail(files[i]);
        var md = th.synchronousMetadata;
        md.namespace = uri;


        for (var idx in settings) {
          var value = settings[idx];
          var key = idx.split(':');
          if (key.length == 2) {
            if (key[0] != prefix) {
              continue;
            }
            key = key[1];
          } else {
            key = key[0];
          }

          md[key] = value; 
        }

        /* it is not clear if these are actually needed */

        /* th.core.metadata.cacheData.status = "bad"; */
        /* th.refresh("metadata"); */
        /* th.core.cameraRaw.cacheData.status = "bad"; */
        /* th.refresh("cameraRaw"); */
      }

      rc = true;

    } catch (e) {
      alert(e + '@' + e.line);
    }

    return rc;
  }
  // EOF

  var res = undefined;
  if (CameraRaw.isBridge()) {
    var str = applySettings(settings, files);
    res = (str ? eval(str) : '');

  } else {
    var brCode = CameraRaw._createBridgeString(applySettings,
                                             [settings, files]);
    res = CameraRaw.sendBridgeMessage(brCode);
  }

  return res;
};

CameraRaw.getSetting = function(name, files) {
  var res;
  var settings = CameraRaw.getSettings(files);

  if (settings instanceof Array) {
    res = [];
    for (var i = 0; i < settings.length; i++) {
      var set = settings[i];
      res.push(set[name]);
    }
  } else {
    res = settings[name];
  }

  return res;
};

//
// The object(s) containing the current settings.
// The name is of the form 'crs:Exposure'
// If more than one file is sent, an array of objects is returned
//     if (!ExternalObject.XMPScript) {
//       ExternalObject.XMPScript = new ExternalObject("lib:AdobeXMPScript");
//     }
//
CameraRaw.getSettings = function(fargs) {
  var files = CameraRaw._checkFilesArg(fargs);

  function getSettings(files) {

    function _extractCRSFields(xmpStr, crsURI, prefix) {
      var obj = {};

      /* the XML resulting from metadata.serialization has simple */
      /* properties stored as attributes instead of elements as you */
      /* would normally find in an XMP file */
      /* Non-simple values are explict XML structures */
      /* This code works around this set of problems */
      /* We are handling this as text instead of as an XML object */
      /* because the XPath implementation in CS3 is busted and the */
      /* potential variability in XML structure makes it too difficult */
      /* to effectively traverse */
      /* RegExp to the rescue! */

      var rex = RegExp('crs:([^=>]+)="([^"]+)"');
      if (xmpStr.match(rex)) {
        var m;
        var str = xmpStr;
        while (m = rex.exec(str)) {
          obj[/* prefix + ':' + */m[1]] = m[2];
          str = str.substring(m.index + m[0].length);
        }

        var rex2 = RegExp('<crs:([^>]+)>');
        if (xmpStr.match(rex2)) {
          var str = xmpStr;
          var m;

          while (m = str.match(rex2)) {
            var name = m[1];
            str = str.substring(m.index + m[0].length);
            var rex3 = RegExp("</crs:" + name + ">");
            m = str.match(rex3);
            var val = str.substring(0, m.index);
            str = str.substring(m.index + m[0].length);
            obj[/*prefix + ':' + */ name] = val.toSource();
          }
        }

      } else {
        /* this code handles standard-format XMP/XML */
        var xmp = new XML(xmpStr);
        var parts = xmp.descendants();
        var len = parts.length();
  
        for (var i = 0; i < len; i++) {
          var part = parts.child(i);
          var parent = part.parent();
          var name = parent.name();
          var uri = name.uri;
          if (uri == crsURI) {
            obj[/*prefix + ':' + */ name.localName] = part.toString();
          }
        }
      }

      return obj;
    }

    var rc = [];
    var uri = 'http://ns.adobe.com/camera-raw-settings/1.0/';
    var prefix = 'crs';

    try {
      for (var i = 0; i < files.length; i++) {
        var th = new Thumbnail(files[i]);
        var md = th.synchronousMetadata;
        var xmpStr = md.serialize();
        var xmp = new XML(xmpStr);
        var obj = _extractCRSFields(xmpStr, uri, prefix);
        rc.push(obj);
      }
    } catch (e) {
      alert(e + '@' + e.line);
    }

    return (rc.length > 1 ? rc.toSource() : rc[0].toSource());
  }
  // EOF

  var res = undefined;
  if (CameraRaw.isBridge()) {
    var str = getSettings(files);
    res = (str ? eval(str) : '');

  } else {
    var brCode = CameraRaw._createBridgeString(getSettings,
                                             [files]);
    res = CameraRaw.sendBridgeMessage(brCode);
  }

  return res;
};

CameraRaw.test = function() {
  // Here's our test file, a clean JPG (no camera raw data)
  var file = File("/c/work/images/crTest/Alena12.jpg");

  // or do it like this to run against a whole folder of images
  // var file = Folder("/c/work/images/crTest").getFiles(/\.jpg/i);
  
  // Clear the settings
  // This will set 'HasSettings' to False (if needed)
  
  CameraRaw.clearSettings(file);

  // Take a look at what the settings are
  var obj = CameraRaw.getSettings(file);
  // debugger;

  // Apply the default settings, a preset, then one specific settings
  CameraRaw.applyDefaultSettings(file);
  CameraRaw.applyPreset("NikonD80", file);
  CameraRaw.applySettings({ Exposure: "1.25"}, file);

  // and verify that we have everything synchronized
  $.writeln(CameraRaw.getSetting('HasSettings', file));
};

//CameraRaw.test();

"CameraRaw.jsx";
// EOF
