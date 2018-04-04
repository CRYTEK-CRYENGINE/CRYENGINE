//
// IniFile.jsx
//
// $Id: IniFile.jsx,v 1.3 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2006, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//

//
// IniFile is a set of functions for reading and writing ini files
// in a consistent fashion across a broad number of scripts.
//
function IniFile(fptr) {
};

//
// Return fptr if its a File or Folder, if not, make it one
//
IniFile.convertFptr = function(fptr) {
  var f;
  if (fptr.constructor == String) {
    f = File(fptr);
  } else if (fptr instanceof File || fptr instanceof Folder) {
    f = fptr;
  } else {
    throw IOError("Bad file \"" + fptr + "\" specified.");
  }
  return f;
};

//
// Return an ini string to an object. Use 'ini' as the object if it's specified
//
IniFile.iniFromString = function(str, ini) {
  var lines = str.split(/\r|\n/);
  var rexp = new RegExp(/([^:]+):(.*)$/);

  if (!ini) {
    ini = {};
  }
    
  for (var i = 0; i < lines.length; i++) {
    var line = IniFile.trim(lines[i]);
    if (!line || line.charAt(0) == '#') {
      continue;
    }
    var ar = rexp.exec(line);
    if (!ar) {
      alert("Bad line in config: \"" + line + "\"");
      return undefined;
    }
    ini[IniFile.trim(ar[1])] = IniFile.trim(ar[2]);
  }

  return ini;
};


//
// Return an ini file to an object. Use 'ini' as the object if it's specified
//
IniFile.read = function(iniFile, ini) {
  if (!ini) {
    ini = {};
  }
  if (!iniFile) {
    return ini;
  }
  var file = IniFile.convertFptr(iniFile);

  if (!file) {
    throw "Bad ini file specified: \"" + iniFile + "\".";
  }

  if (!file.exists) {
  }
  if (file.exists && file.open("r")) {
    var str = file.read();
    ini = IniFile.iniFromString(str, ini);
    file.close();
  }
  return ini;
};

//
// Return an ini string coverted from an object
//
IniFile.iniToString = function(ini) {
  var str = '';
  for (var idx in ini) {
    if (idx.charAt(0) == '_') {         // private stuff
      continue;
    }
    if (idx == 'typename') {
      continue;
    }
    var val = ini[idx];
    
    if (val.constructor == String ||
        val.constructor == Number ||
        val.constructor == Boolean ||
        typeof(val) == "object") {
      str += (idx + ": " + val.toString() + "\n");
    }
  }
  return str;
};

//
// Write an object to an ini file overwriting whatever was there before
//
IniFile.overwrite = function(iniFile, ini) {
  if (!ini || !iniFile) {
    return;
  }
  var file = IniFile.convertFptr(iniFile);

  if (!file) {
    throw "Bad ini file specified: \"" + iniFile + "\".";
  }
 
  if (!file.open("w")) {
    throw "Unable to open ini file " + file + ": " + file.error;
  }

  var str = IniFile.iniToString(ini);
  file.write(str);
  file.close();

  return ini;
};
IniFile.trim = function(value) {
   return value.replace(/^[\s]+|[\s]+$/g, '');
};


//
// Updating the ini file retains the ini file layout including any externally
// add comments, blank lines, and the property sequence
//
IniFile.update = function(iniFile, ini) {
  if (!ini || !iniFile) {
    return;
  }
  var file = IniFile.convertFptr(iniFile);

  // we can only update the file if it exists
  var update = file.exists;
  var str = '';

  if (update) {
    file.open("r");
    str = file.read();
    file.close();
    
    for (var idx in ini) {
      if (idx.charAt(0) == '_') {         // private stuff
        continue;
      }
      if (idx == "typename") {
        continue;
      }

      var val = ini[idx];

      if (val == undefined) {
        val = '';
      }
      
      if (typeof val == "string" ||
          typeof val == "number" ||
          typeof val == "boolean" ||
          typeof val == "object") {
        idx += ':';
        var re = RegExp('^' + idx, 'm');

        if (re.test(str)) {
          re = RegExp('^' + idx + '[^\n]+', 'm');
          str = str.replace(re, idx + ' ' + val);
        } else {
          str += '\n' + idx + ' ' + val;
        }
      }
    }
  } else {
    str = IniFile.iniToString(ini);
  }

  if (str) {
    if (!file.open("w")) {
      throw "Unable to open ini file " + file + ": " + file.error;
    }
    file.write(str);
    file.close();
  }

  return ini;
};

// By default, I update ini files instead of overwriting them.
IniFile.write = IniFile.update;


// convert an object into an easy-to-read string
listProps = function(obj) {
  var s = '';
  for (var x in obj) {
    s += x + ":\t";
    try {
      var o = obj[x];
      s += (typeof o == "function") ? "[function]" : o;
    } catch (e) {
    }
    s += "\r\n";
  }
  return s;
};


// a simple demo of the INI functions
IniFile.demo1 = function() {
  var obj  = {
    name: "bob",
    age: 24
  };
  
  alert(listProps(obj));

  IniFile.write("~/testfile.ini", obj);

  var z = IniFile.read("~/testfile.ini", obj);

  alert(listProps(z));
};

// a simple demo of the INI functions
IniFile.demo2 = function() {
  var f = new File("~/testfile.ini");

  var obj = {};
  obj.city = "singapore";

  IniFile.read(f, obj);
  var z = IniFile.write(f, obj);

  alert(listProps(z));
};

// IniFile.demo1();
// IniFile.demo2();

"IniFile.jsx";

// EOF
