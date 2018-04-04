#target photoshop
//
// RtfParser.jsx
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/stdlib.js"
//
//@include "xlib/Stream.js"

Stdlib.log.filename = "/c/work/tabula/rtfparser.log";
Stdlib.log.enabled = true;

$.level = 1; 

RDS = function(nm) {
  this.toString = function() { return "RDS." + nm; };
};
RDS.NORMAL = new RDS("NORMAL");
RDS.SKIP = new RDS("SKIP");

RIS = function(nm) {
  this.toString = function() { return "RIS." + nm; };
};
RIS.NORMAL = new RIS("NORMAL");
RIS.BIN = new RIS("BIN");
RIS.HEX = new RIS("HEX");


RtfState = function() {
  var self = this;

  self.nextState = undefined;
  self.chp = new CHP();
  self.pap = new PAP();
  self.sep = new SEP();
  self.dop = new DOP();
  self.rds = RDS.NORMAL;
  self.ris = RIS.NORMAL;
};

KWD = function(nm) {
  this.toString = function() { return "KWD." + nm; };
};
KWD.CHAR = new KWD("CHAR");
KWD.DEST = new KWD("DEST");
KWD.PROP = new KWD("PROP");
KWD.SPEC = new KWD("SPEC");

IPROP = function(nm) {
  this.idx = IPROP.idx++;
  this.nm = "IPROP." + nm;
  this.valueOf = function() { return this.idx; }
  this.toString = function() { return this.nm; };
};
IPROP.idx = 0;

IPROP.BOLD = new IPROP("BOLD");
IPROP.ITALIC = new IPROP("ITALIC");
IPROP.UNDERLINE = new IPROP("UNDERLINE");
IPROP.LEFT_IND = new IPROP("LEFT_IND");
IPROP.RIGHT_IND = new IPROP("RIGHT_IND");
IPROP.FIRST_IND = new IPROP("FIRST_IND");
IPROP.COLS = new IPROP("COLS");
IPROP.PGN_X = new IPROP("PGN_X");
IPROP.PGN_Y = new IPROP("PGN_Y");
IPROP.XA_PAGE = new IPROP("XA_PAGE");
IPROP.YA_PAGE = new IPROP("YA_PAGE");
IPROP.XA_LEFT = new IPROP("XA_LEFT");
IPROP.XA_RIGHT = new IPROP("XA_RIGHT");
IPROP.YA_TOP = new IPROP("YA_TOP");
IPROP.YA_BOTTOM = new IPROP("YA_BOTTOM");
IPROP.PGN_START = new IPROP("PGN_START");
IPROP.SBK = new IPROP("SBK");
IPROP.PGN_FORMAT = new IPROP("PGN_FORMAT");
IPROP.FACINGP = new IPROP("FACINGP");
IPROP.LANDSCAPE = new IPROP("LANDSCAPE");
IPROP.JUST = new IPROP("JUST");
IPROP.PARD = new IPROP("PARD");
IPROP.PLAIN = new IPROP("PLAIN");
IPROP.SECTD = new IPROP("SECTD");
IPROP.SUPER = new IPROP("SUPER");
IPROP.NOSUPERSUBSECTD = new IPROP("NOSUPERSUBSECTD");
IPROP.MAX = new IPROP("MAX");

ACTION = function(nm) {
  this.toString = function() { return "ACTION." + nm; };
};
ACTION.SPEC = new ACTION("SPEC");
ACTION.BYTE = new ACTION("BYTE");
ACTION.WORD = new ACTION("WORD");

PROPTYPE = function(nm) {
  this.toString = function() { return "PROPTYPE." + nm; };
};
PROPTYPE.CHP = new PROPTYPE("CHP");
PROPTYPE.PAP = new PROPTYPE("PAP");
PROPTYPE.SEP = new PROPTYPE("SEP");
PROPTYPE.DOP = new PROPTYPE("DOP");

PROP = function() {
  var self = this;

  self.actn = undefined;   // ACTION
  self.proptype = undefined; // PROPTYPE
  self.pname = 0;
};
PROP.prototype.toString = function() {
  var self = this;
  var str = "[PROP ";
  str += (self.actn ? self.actn : "''");
  str += " " + (self.proptype ? self.proptype : "''");
  str += " " + (self.pname ? self.pname : "''");
  str += "]";

  return str;
};

IPFN = function(nm) {
  this.toString = function() { return "IPFN." + nm; };
};
IPFN.BIN = new IPFN("BIN");
IPFN.HEX = new IPFN("HEX");
IPFN.SKIP_DEST = new IPFN("SKIP_DEST");
IPFN.SUPERSCRIPT = new IPFN("SUPERSCRIPT");
IPFN.NOSUPERSUB = new IPFN("NOSUPERSUB");

IDEST = function(nm) {
  this.toString = function() { return "IDEST." + nm; };
};
IDEST.PICT = new IDEST("PICT");
IDEST.SKIP = new IDEST("SKIP");

SYM = function() {
  var self = this;

  self.keyword = '';
  self.dflt = 0;
  self.fPassDflt = false;
  self.kwd = KWD.CHAR; // KWD
  self.idx = 0;
};
SYM.prototype.toString = function() {
  var self = this;
  return "[SYM " + self.keyword + "]"; 
};

// Character properties
CHP = function() {
  var self = this;
  self.bold = false;
  self.underline = false;
  self.italic = false;
  self.superscript = false;
};
CHP.prototype.toString = function() {
  var self = this;
  var str = "[CHP";
  if (self.bold)      { str += " bold"; }
  if (self.italic)    { str += " italic"; }
  if (self.underline) { str += " underline"; }
  if (self.superscript)     { str += " superscript"; }
  str += "]";

  return str;
};


JUST = function(nm) {
  this.toString = function() { return "JUST." + nm; };
};
JUST.L = new JUST("L");
JUST.R = new JUST("R");
JUST.C = new JUST("C");
JUST.F = new JUST("F");

PAP = function() {
  var self = this;
  self.xaLeft = 0;
  self.xaRight = 0;
  self.xaFirst = 0;
  self.just = JUST.L; // JUST
};
PAP.prototype.toString = function() {
  var self = this;
  var str = "[PAP " + self.xaLeft + " " + self.xaRight + " " + self.xaFirst;
  str += " " + (self.just ? self.just : "''");
  return str;
};

SBK = function(nm) {
  this.toString = function() { return "SBK." + nm; };
};
SBK.NON = new SBK("NON");
SBK.COL = new SBK("COL");
SBK.EVN = new SBK("EVN");
SBK.ODD = new SBK("ODD");
SBK.PG = new SBK("PG");

PGN = function(nm) {
  this.toString = function() { return "PGN." + nm; };
};
PGN.DEC = new PGN("DEC");
PGN.UROM = new PGN("UROM");
PGN.LROM = new PGN("LROM");
PGN.ULTR = new PGN("ULTR");
PGN.LLTR = new PGN("LLTR");

SEP = function() {
  var self = this;

  self.cCols = 0;
  self.sbk = SBK.NON; // SBK
  self.xaPgn = 0;
  self.yaPgn = 0;
  self.pgn = PGN.DEC; // PGN
};
SEP.prototype.toString = function() {
  var self = this;
  var str = "[SEP " + self.cCols;
  str += " " + (self.sbk ? self.sbk : "''");
  str += " " + self.xaPgn + " " + self.yaPgn;
  str += " " + (self.pgn ? self.pgn : "''");
  str += "]";
  return str;
};

DOP = function() {
  var self = this;
  self.xaPage = 0;
  self.yaPage = 0;
  self.xaLeft = 0;
  self.yaTop = 0;
  self.xaRight = 0;
  self.yaBottom = 0;
  self.pgnStart = 0;
  self.fFacingp = false;
  self.fLandscape = false;
};
DOP.prototype.toString = function() {
  var self = this;
  var str = "[DOP";
  var props = ["xaPage","yaPage","xaLeft","yaTop","xaRight","yaBottom",
               "pgnStart","fFacingp","fLandscape"];
  for (var i = 0; i < props.length; i++) {
    str += " " + self[props[i]];
  }
  str += "]";
  return str;
};


RtfResultCode = function(nm, v) {
  this.toString = function() { return "RtfResultCode." + nm; };
  this.valueOf = function() { return v; }
};
RtfResultCode.OK = new RtfResultCode("OK", 0);
RtfResultCode.STACK_UNDERFLOW = new RtfResultCode("STACK_UNDERFLOW", 1);
RtfResultCode.STACK_OVERFLOW = new RtfResultCode("STACK_OVERFLOW", 2);
RtfResultCode.UNMATCHED_BRACE = new RtfResultCode("UNMATCHED_BRACE", 3);
RtfResultCode.INVALID_HEX = new RtfResultCode("INVALID_HEX", 4);
RtfResultCode.BAD_TABLE = new RtfResultCode("BAD_TABLE", 5);
RtfResultCode.ASSERTION = new RtfResultCode("ASSERTION", 6);
RtfResultCode.END_OF_FILE = new RtfResultCode("END_OF_FILE", 7);

PROP.rgprop = [
  // actn        proptype  pname
  [ ACTION.BYTE, PROPTYPE.CHP, "bold"  ],
  [ ACTION.BYTE, PROPTYPE.CHP, "italic"  ],
  [ ACTION.BYTE, PROPTYPE.CHP, "underline"  ],
  [ ACTION.WORD, PROPTYPE.PAP, "xaLeft"  ],
  [ ACTION.WORD, PROPTYPE.PAP, "xaRight"  ],
  [ ACTION.WORD, PROPTYPE.PAP, "xaFirst"  ],
  [ ACTION.WORD, PROPTYPE.SEP, "cCols"  ],
  [ ACTION.WORD, PROPTYPE.SEP, "xaPgn"  ],
  [ ACTION.WORD, PROPTYPE.SEP, "yaPgn"  ],
  [ ACTION.WORD, PROPTYPE.DOP, "xaPage"  ],
  [ ACTION.WORD, PROPTYPE.DOP, "yaPage"  ],
  [ ACTION.WORD, PROPTYPE.DOP, "xaLeft"  ],
  [ ACTION.WORD, PROPTYPE.DOP, "xaRight"  ],
  [ ACTION.WORD, PROPTYPE.DOP, "yaTop"  ],
  [ ACTION.WORD, PROPTYPE.DOP, "yaBottom"  ],
  [ ACTION.WORD, PROPTYPE.DOP, "pgnStart"  ],
  [ ACTION.BYTE, PROPTYPE.SEP, "sbk"  ],
  [ ACTION.BYTE, PROPTYPE.SEP, "pgnFormat"  ],
  [ ACTION.BYTE, PROPTYPE.DOP, "fFacingp"  ],
  [ ACTION.BYTE, PROPTYPE.DOP, "fLandscape"  ],
  [ ACTION.BYTE, PROPTYPE.PAP, "just"  ],
  [ ACTION.SPEC, PROPTYPE.PAP, ""  ],
  [ ACTION.SPEC, PROPTYPE.CHP, ""  ],
  [ ACTION.SPEC, PROPTYPE.SEP, ""  ],
  [ ACTION.SPEC, PROPTYPE.CHP, ""  ],
];

PROP.tableInit = function() {
  var table = [];
  var props = PROP.rgprop;

  for (var i = 0; i < props.length; i++) {
    var pC = props[i];
    var p = new PROP();
    p.actn = pC[0];
    p.proptype = pC[1];
    p.pname = pC[2];

    table.push(p);
  }
  return table;
};
PROP.table = PROP.tableInit();

SYM.rgsymRtf = [
// keyword        dflt   fPassDflt   kwd       idx
  [  "b",            1,    false,  KWD.PROP, IPROP.BOLD ],
  [  "u",            1,    false,  KWD.PROP, IPROP.UNDERLINE ],
  [  "i",            1,    false,  KWD.PROP, IPROP.ITALIC ],
  [  "li",           0,    false,  KWD.PROP, IPROP.LEFT_IND ],
  [  "ri",           0,    false,  KWD.PROP, IPROP.RIGHT_IND ],
  [  "fi",           0,    false,  KWD.PROP, IPROP.FIRST_IND ],
  [  "cols",         1,    false,  KWD.PROP, IPROP.COLS ],
  [  "sbknone", SBK.NON,    true,  KWD.PROP, IPROP.SBK ],
  [  "sbkcol",  SBK.COL,    true,  KWD.PROP, IPROP.SBK ],
  [  "sbkeven", SBK.EVN,    true,  KWD.PROP, IPROP.SBK ],
  [  "sbkodd",  SBK.ODD,    true,  KWD.PROP, IPROP.SBK ],
  [  "sbkpage", SBK.PG,     true,  KWD.PROP, IPROP.SBK ],
  [  "pgnx",         0,    false,  KWD.PROP, IPROP.PGN_X ],
  [  "pgny",         0,    false,  KWD.PROP, IPROP.PGN_Y ],
  [  "pgndec",   PGN.DEC,   true,  KWD.PROP, IPROP.PGN_FORMAT ],
  [  "pgnucrm",  PGN.UROM,  true,  KWD.PROP, IPROP.PGN_FORMAT ],
  [  "pgnlcrm",  PGN.LROM,  true,  KWD.PROP, IPROP.PGN_FORMAT ],
  [  "pgnucltr", PGN.ULTR,  true,  KWD.PROP, IPROP.PGN_FORMAT ],
  [  "pgnlcltr", PGN.LLTR,  true,  KWD.PROP, IPROP.PGN_FORMAT ],
  [  "qc",      JUST.C,     true,  KWD.PROP, IPROP.JUST ],
  [  "ql",      JUST.L,     true,  KWD.PROP, IPROP.JUST ],
  [  "qr",      JUST.R,     true,  KWD.PROP, IPROP.JUST ],
  [  "qj",      JUST.J,     true,  KWD.PROP, IPROP.JUST ],
  [  "paperw",   11240,    false,  KWD.PROP, IPROP.XA_PAGE ],
  [  "paperh",   15480,    false,  KWD.PROP, IPROP.YA_PAGE ],
  [  "margl",     1800,    false,  KWD.PROP, IPROP.XA_LEFT ],
  [  "margr",     1800,    false,  KWD.PROP, IPROP.XA_RIGHT ],
  [  "margt",     1800,    false,  KWD.PROP, IPROP.YA_TOP ],
  [  "margb",     1800,    false,  KWD.PROP, IPROP.YA_BOTTOM ],
  [  "pgnstart",     1,     true,  KWD.PROP, IPROP.PGN_START ],
  [  "facingp",      1,     true,  KWD.PROP, IPROP.FACINGP ],
  [  "landscape",    1,     true,  KWD.PROP, IPROP.LANDSCAPE ],
  [  "par",          0,    false,  KWD.CHAR, 0x0a ],
  [  "\0x0a",        0,    false,  KWD.CHAR, 0x0a ],
  [  "\0x0d",        0,    false,  KWD.CHAR, 0x0a ],
  [  "tab",          0,    false,  KWD.CHAR, 0x09 ],
  [  "ldblquote",    0,    false,  KWD.CHAR, '"' ],
  [  "rdblquote",    0,    false,  KWD.CHAR, '"' ],
  [  "bin",          0,    false,  KWD.SPEC, IPFN.BIN ],
  [  "*",            0,    false,  KWD.SPEC, IPFN.SKIP_DEST ],
  [  "'",            0,    false,  KWD.SPEC, IPFN.HEX ],
  [  "author",       0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "buptim",       0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "colortbl",     0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "comment",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "doccomm",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "fonttbl",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "fcharset",     0,    false,  KWD.DEST, IDEST.SKIP ],  // XXX
  [  "footer",       0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "footerf",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "footerl",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "footerr",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "footnote",     0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "ftncn",        0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "ftnsep",       0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "ftnsepc",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "header",       0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "headerf",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "headerl",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "headerr",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "info",         0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "keywords",     0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "operator",     0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "pict",         0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "printim",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "private1",     0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "revtim",       0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "rxe",          0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "stylesheet",   0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "subject",      0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "tc",           0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "title",        0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "txe",          0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "xe",           0,    false,  KWD.DEST, IDEST.SKIP ],
  [  "{",            0,    false,  KWD.CHAR, '{' ],
  [  "}",            0,    false,  KWD.CHAR, '}' ],
  [  "\\",           0,    false,  KWD.CHAR, '\\' ],
  [  "super",        0,     false,  KWD.SPEC, IPFN.SUPERSCRIPT ],
  [  "nosupersub",   0,     false,  KWD.SPEC, IPFN.NOSUPERSUB ],
];
SYM.tableInit = function() {
  var table = [];

  var syms = SYM.rgsymRtf;
  for (var i = 0; i < syms.length; i++) {
    var symC = syms[i];
    var sym = new SYM();
    sym.keyword = symC[0];
    sym.dflt = symC[1];
    sym.fPassDflt = symC[2];
    sym.kwd = symC[3];
    sym.idx = symC[4];

    table.push(sym);
  }
  return table;
};
SYM.table = SYM.tableInit();

RtfParser = function() {
  var self = this;

  self.cGroup = 0;
  self.fSkipDestIfUnk = false;
  self.cbBin = 0;
  self.lParam = 0;

  self.rds = RDS.NORMAL;
  self.ris = RIS.NORMAL;
  self.chp = new CHP();
  self.pap = new PAP();
  self.dop = new DOP();
  self.state = undefined; // RtfState
};

RtfParser.prototype.parseFile = function(fptr) {
  var self = this;
  Stdlib.log("RtfParser.parseFile(" + fptr + ")");

  var str = Stdlib.readFromFile(fptr);
  self.parseString(str);
};
RtfParser.prototype.parseString = function(str) {
  var self = this;
  if (str.constructor != String) {
    throw "Expecting String";
  }

  var stream = new Stream(str);
  return self.parse(stream);
};

RtfParser.prototype.parse = function(str) {
  var self = this;
  var ch;

  //$.level = 1; debugger;

  while ((ch = str.readByteChar()) != Stream.EOF) {
    var ec = 0;
    var cNibble = 2;
    var b = 0;

    if (self.cGroup < 0) {
      return RtfResultCode.STACK_UNDERFLOW;
    }

    if (self.ris == RIS.BIN) {
      if ((ec = self.parseChar(ch)) != RtfResultCode.OK) {
        return ec;
      }
    } else {
      switch (ch) {
      case '{':
        if ((ec = self.pushRtfState()) != RtfResultCode.OK) {
          return ec;
        }
        break;
      case '}':
        if ((ec = self.popRtfState()) != RtfResultCode.OK) {
          return ec;
        }
        break;
      case '\\':
        if ((ec = self.parseRtfKeyword(str)) != RtfResultCode.OK) {
          return ec;
        }
        break;
//       case 0x0d:
//       case 0x0a:
      case '\r':
      case '\n':
        break;
      default:
        if (self.ris == RIS.NORMAL) {
          if ((ec = self.parseChar(ch)) != RtfResultCode.OK) {
            return ec;
          }
        } else {
          if (self.ris == RIS.HEX) {
            return RtfResultCode.ASSERTION;
          }
          b = b << 4;
          if (ch.match(/\d/)) {
            b += ch - '0';
          } else {
            if (ch.match(/[a-z]/)) {
              if (ch < 'a' || ch > 'f') {
                return RtfResultCode.INVALID_HEX;
              }
              b += ch - 'a';
            } else {
              if (ch < 'A' || ch > 'F') {
                return RtfResultCode.INVALID_HEX;
              }
              b += ch - 'A';
            }
          }
          cNibble--;
          if (!cNibble) {
            if ((ec = self.parseChar(ch)) != RtfResultCode.OK) {
              return ec;
            }
            cNibble = 2;
            b = 0;
          }
        }
        break;
      }
    }
  }

  if (self.cGroup < 0) {
    return RtfResultCode.STACK_UNDERFLOW;
  }
  if (self.cGroup > 0) {
    return RtfResultCode.UNMATCHED_BRACE;
  }

  return RtfResultCode.OK;
};

RtfParser.prototype.pushRtfState = function() {
  var self = this;
  var state = new RtfState();

  Stdlib.log("RtfParser.pushRtfState()");
  state.next = self.state;
  var props = ["chp","pap","sep","dop","rds","ris"];
  for (var i = 0; i < props.length; i++) {
    var idx = props[i];
    state[idx] = self[idx];
  }
  self.rds = RDS.NORMAL;
  self.state = state;
  self.cGroup++;
  
  return RtfResultCode.OK;
};
RtfParser.prototype.popRtfState = function() {
  var self = this;

  Stdlib.log("RtfParser.popRtfState()");

  if (!self.state) {
    return RtfResultCode.STACK_UNDERFLOW;
  }

  var state = self.state;
  var ec;

  if (self.rds != state.rds) {
    if ((ec = self.endGroupAction(self.rds)) != RtfResultCode.OK) {
      return ec;
    }
  }
  var props = ["chp","pap","sep","dop","rds","ris"];
  for (var i = 0; i < props.length; i++) {
    var idx = props[i];
    self[idx] = state[idx];
  }
  self.state = self.state.next;
  self.cGroup--;
  
  return RtfResultCode.OK;
};

RtfParser.prototype.parseRtfKeyword = function(str) {
  var self = this;
  var ch;
  var fParam = false;
  var fNeg = false;
  var param = 0;
  var keyword = '';
  var parameter = '';

  if ((ch = str.readByteChar()) == Stream.EOF) {
    return RtfParser.END_OF_FILE;
  }

  if (!ch.match(/[a-zA-Z]/)) {
    keyword = ch;
    return self.translateKeyword(keyword, 0, fParam);
  }

  while (ch.match(/[a-zA-Z]/)) {
    keyword += ch;
    ch = str.readByteChar();
  }
  if (ch == '-') {
    fNeg = true;
    if ((ch = str.readByteChar()) == Stream.EOF) {
      return RtfParser.END_OF_FILE;
    }
  }

  if (ch.match(/\d/)) {
    fParam = true;
    while (ch.match(/\d/)) {
      parameter += ch;
      ch = str.readByteChar();
    }
    param = Number(parameter);
    if (fNeg) {
      param = -param;
    }
    self.lParam = Number(parameter);
    if (fNeg) {
      lParam = -lParam;
    }
  }

  if (ch != ' ') {
    str.ptr--;
  }

  return self.translateKeyword(keyword, param, fParam);
};

RtfParser.prototype.parseChar = function(ch) {
  var self = this;

  if (self.ris == RIS.BIN && --self.cbBin <= 0) {
    self.ris = RIS.NORMAL;
  }

  switch (self.rds) {
    case RDS.SKIP: return RtfResultCode.OK;
    case RDS.NORMAL: return self.printChar(ch);
    default:
      return RtfResultCode.OK;
  }
};

RtfParser.prototype.printChar = function(ch) {
  var self = this;
  if (typeof(ch) == "number") {
    if (ch == 0) {
      return RtfResultCode.OK;
    }
    ch = String.fromCharCode(ch);
  }
  if (ch.charCodeAt(0) == 0) {
    return RtfResultCode.OK;
  }
  self.result += ch;
  Stdlib.log("RtfParser.printChar(" + ch + ")");
  return RtfResultCode.OK;
};

RtfParser.prototype.applyPropChange = function(iprop, val) {
  var self = this;

  //$.level = 1; debugger;

  Stdlib.log("RtfParser.applyPropChange(" + iprop.toString() +
             ", " + val += ")");

  if (self.rds == RDS.SKIP) {
    return RtfResultCode.OK;
  }
  var pb;
  var p = PROP.table[iprop.valueOf()];
  switch (p.proptype) {
    case PROPTYPE.DOP: pb = self.dop; break;
    case PROPTYPE.SEP: pb = self.sep; break;
    case PROPTYPE.PAP: pb = self.pap; break;
    case PROPTYPE.CHP: pb = self.chp; break;
    default:
      if (p.actn != ACTION.SPEC) {
        return RtfResultCode.BAD_TABLE;
      }
      break;
  }

  switch (p.actn) {
    case ACTION.BYTE: pb[p.pname] = val; break;
    case ACTION.WORD: pb[p.pname] = val; break;
    case ACTION.SPEC: return self.parseSpecialProperty(iprop, val); break;
    default: return RtfResultCode.BAD_TABLE;
  }

  return RtfResultCode.OK;
};
RtfParser.prototype.parseSpecialProperty = function(iprop, val) {
  var self = this;

  switch (iprop) { 
    case IPROP.SUPER:       self.chp.superscript = true; break;
    case IPROP.NOSUPERSUB:  self.chp.superscript = true; break;
    case IPROP.PLAIN:  self.chp = new CHP(); break;
    case IPROP.PARD:   self.pap = new PAP(); break;
    case IPROP.SECTD:  self.sep = new SEP(); break;
    default: return RtfResultCode.BAD_TABLE; 
  }

  return RtfResultCode.OK;
};

RtfParser.prototype.translateKeyword = function(kwd, param, fParam) {
  var self = this;

  if (kwd == "super") {
    //$.level = 1; debugger;
  }

  Stdlib.log("RtfParser.translateKeyword(" + kwd + ", " +
             param + ", " + fParam + ")");

  var tbl = SYM.table;
  var sym;
  for (var i = 0; i < tbl.length; i++) {
    if (kwd == tbl[i].keyword) {
      sym = tbl[i];
      break;
    }
  }

  if (!sym) {
    if (self.fSkipDestIfUnk) {
      self.rds = RDS.SKIP;
    }
    self.fSkipDestIfUnk = false;
    return RtfResultCode.OK;
  }
  
  self.fSkipDestIfUnk = false;

  switch (sym.kwd) {
    case KWD.PROP:
      if (sym.fPassDflt || !self.fParam) {
        self.param = sym.dflt;
      }
      return self.applyPropChange(sym.idx, param);
    case KWD.CHAR:
      return self.parseChar(sym.idx);
    case KWD.DEST:
      return self.changeDest(sym.idx);
    case KWD.SPEC:
      return self.parseSpecialKeyword(sym.idx);
    default:
      return RtfResultCode.BAD_TABLE; 
  }

  return RtfResultCode.OK;
};

RtfParser.prototype.changeDest = function(idest) {
  var self = this;
  if (self.rds == RDS.SKIP) {
    return RtfResultCode.OK;
  }

  switch (idest) {
    default:
    self.rds = RDS.SKIP;
    break;
  }
  return RtfResultCode.OK;
};
RtfParser.prototype.endGroupAction = function(rds) {
  return RtfResultCode.OK;
};
RtfParser.prototype.parseSpecialKeyword = function(ipfn) {
  var self = this;
  if (self.rds == RDS.SKIP && ipfn != IPFN.BIN) {
    return RtfResultCode.OK;
  }

  switch (ipfn) {
    case IPFN.BIN:
    self.ris = RIS.BIN;
    self.cbBin = self.lParam;
    break;

    case IPFN.SKIP_DEST:
    self.fSkipDestIfUnk = true;
    break;

    case IPFN.HEX:
    self.ris = RIS.HEX;
    break;

    case IPFN.SUPERSCRIPT:
    Stdlib.log("In Super");
    break;

    case IPFN.NOSUPERSUB:
    Stdlib.log("Out Super");
    break;

    default:
    return RtfResultCode.BAD_TABLE;
  }

  return RtfResultCode.OK;
};
 
RtfParser.test= function() {
  var parser = new RtfParser();
  parser.result = '';
  parser.parseFile("/c/work/tabula/test.rtf");
  //parser.parseFile("/c/work/tabula/Homer/homer.rtf");
  alert(parser.result);
};

RtfParser.test();

"RrtParser.jsx";
// EOF
