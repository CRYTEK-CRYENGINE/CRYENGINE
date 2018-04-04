#!/usr/bin/perl
#
# $Id: xlatepsdk.pl,v 1.7 2013/05/09 12:32:48 anonymous Exp $
# Generates PSConstants.js from PIStringTerminology.h, PITerminology.h, and PI3d
#
#

my @PSConstants =
(
  "Class",
  "Enum",
  "Event",
  "Form",
  "Key",
  "Type",
  "Unit"
);

print<<EOT;
//
// PSConstants
// Javascript definitions for Class, Enum, Event, Form, Key,
//    Type, and Unit symbols
//
//  \$Id\$
//
PSConstants = function PSConstants() {};
// PSConstants.prototype = new Array();

PSConstants.symbolTypes = new Object();
PSConstants.add = function(kind, name) {
  if (!name) { throw "Internal error PSConstants.add()"; }
  kind._name = name;
  kind._reverseName = new Object();
  kind._reverseSym = new Object();
  kind._add = function(name, sym) {
    if (!name || !sym) { throw "Internal error kind._add()"; }
    kind[name] = app.charIDToTypeID(sym);
    // collision detection...
    // if (kind._reverseName[kind[name]]) {
    //   $.writeln('PS' + kind._name + ', ' + sym + ', ' +
    //             kind._reverseName[kind[name]] + ', ' + name);
    // }
    kind._reverseName[kind[name]] = name;
    kind._reverseSym[kind[name]] = sym;
  };

  PSConstants.symbolTypes[kind._name] = kind;
};

// deprecated version
PSConstants._reverseNameLookup = function(id) {
  var tbl = PSConstants.symbolTypes;

  for (var name in tbl) {
    //writeln(id + " " + tbl + " " + name + " " + tbl[name]);
    var kind = tbl[name];
    var r = kind._reverseName[id];
    if (r) return r;
  }
  return undefined;
};
// deprecated version
PSConstants._reverseSymLookup = function(id) {
  var tbl = PSConstants.symbolTypes;

  for (var name in tbl) {
    //writeln(id + " " + tbl + " " + name + " " + tbl[name]);
    var kind = tbl[name];
    var r = kind._reverseSym[id];
    if (r) return r;
  }
  return undefined;
};


PSConstants._getTbl = function(id, ns) {
  var tbl = PSConstants.symbolTypes;

  if (ns) {
    // if a namespace is specified, it is searched first,
    // followed by String and then the rest...

    var nm;
    tbl = [];
    if (ns.constructor == String) {
      nm = ns;

    } else if (ns._name) {
      nm = ns._name;

    } else {
      Error.runtimeError(9100, "Bad map specified: " + ns.toString());
    }

    tbl.push(PSConstants.symbolTypes[nm]);

    for (var name in PSConstants.symbolTypes) {
      if (name != nm && name != "String") {
        tbl.push(PSConstants.symbolTypes[name]);
      }
    }

    if (nm != "String") {
      tbl.push(PSConstants.symbolTypes["String"]);
    }
  }

  return tbl;
};


// 'ns' is the optional 'namespace' in these reverse lookup functions.
// It can be either a string ("Class") or a
// table object from PSConstants (PSClass). Using 'ns' will help
// these functions return the most appropriate results since collisions
// happen. For instance, cTID('Rds ') is the id for PSKey.Radius
// and PSEnum.Reds.
//
PSConstants.reverseNameLookup = function(id, ns) {
 var tbl = PSConstants._getTbl(id, ns);

  for (var name in tbl) {
    //writeln(id + " " + tbl + " " + name + " " + tbl[name]);
    var kind = tbl[name];
    var r = kind._reverseName[id];
    if (r) return r;
  }
  return undefined;
};
PSConstants.reverseSymLookup = function(id, ns) {
  var tbl = PSConstants._getTbl(id, ns);

  for (var name in tbl) {
    //writeln(id + " " + tbl + " " + name + " " + tbl[name]);
    var kind = tbl[name];
    var r = kind._reverseSym[id];
    if (r) return r;
  }
  return undefined;
};
PSConstants.reverseStringLookup = function(id) {
  return PSString._reverseSym[id];
};
// PSContants._massageName = function(name) {
//   name = name.replace(/\\s/g, '');
//   return name;
// };
PSConstants.lookup = function(kname) {
  kname = kname.replace(/\\s/g, '');
  var tbl = PSConstants.symbolTypes;
  for (var name in tbl) {
    //writeln(id + " " + tbl + " " + name + " " + tbl[name]);
    var kind = tbl[name];
    var r = kind[kname];
    if (r) return r;
  }
  return undefined;
};
PSConstants.lookupSym = function(kname) {
  kname = kname.replace(/\\s/g, '');
  var id = PSConstants.lookup(kname);
  return !id ? undefined : PSConstants.reverseSymLookup(id);
};
PSConstants.list = function(kind) {
  var tbl = PSConstants.symbolTypes[kind._name];
  var lst = '';
  for (var name in tbl) {
    if (name.match(/^[A-Z]/)) {
      lst += (kind._name + '.' + name + " = '" + kind[name] + '\';\r\n');
    }
  }
  return lst;
};
PSConstants.listAll = function() {
  var tbl = PSConstants.symbolTypes;
  var lst = '';
  for (var name in tbl) {
    var kind = tbl[name];
    lst += PSConstants.list(kind);
  }
  return lst;
};

EOT

foreach (@PSConstants) {
  my $name = "PS$_";
  print "$name = function $name() {};\n";
  print "PSConstants.add($name, \"$_\");\n\n";
}

print<<EOT;

PSString = function PSString() {};
PSConstants.add(PSString, "String");
PSString._add = function(name, sym) {
  if (!name) { throw "Internal error PSString._add()"; }
  if (!sym) sym = name;
  var kind = this;
  kind[name] = app.stringIDToTypeID(sym);
  kind._reverseName[kind[name]] = sym;
  kind._reverseSym[kind[name]] = sym;

  if (!kind[sym]) {
    PSString._add(sym);
  }
};


EOT

my $last = '';

while (<>) {
  chomp;

  if (/^#define\s+([a-z]+)([0-9A-Z][0-9a-zA-Z]*)\s+\'(.{4})\'/) {
    my $tag = ucfirst $1;
    next unless grep { $tag eq $_} @PSConstants;
    if ($tag ne $last) {
      print "\n";
      $last = $tag;
    }

    print "PS", $tag, "._add(\"$2\", \"$3\");\n";
#    print "print(\"$tag.$2\ = \\\"$3\\\"\");\n";

  } elsif (/^#define\s+k([0-9a-zA-Z]+)Str\s+\"(.+)\"/ ||
           /^#define\s+key([0-9a-zA-Z]+)\s+\"(.+)\"/ ||
           /^#define\s+k([0-9a-zA-Z]+)\s+\"(.+)\"/ ) {
    my $sym = $2;
    if ($sym ne $1) {
      print "PSString", "._add(\"$1\", \"$2\");\n";
    } else {
      print "PSString", "._add(\"$1\");\n";
    }
  }
}

print<<EOT;

// this fixes the part where "target" whacks/collides-with "null"
PSString._add("Null", "null");
PSString._reverseName[PSString.Null] = "Null";

PSString._reverseName[PSString.rasterizeLayer] = "rasterizeLayer";

PSConstants.test = function() {
// this really is not any kind of test yet...
  print('name   = ' + PSClass._name);
  print('action = ' + PSClass.Action);
  print('reverse of ' + PSClass.Action + " = " +
      PSConstants.reverseNameLookup(PSClass.Action));
  print(PSConstants.listAll());
};

"PSConstants.js";
// EOF
EOT
