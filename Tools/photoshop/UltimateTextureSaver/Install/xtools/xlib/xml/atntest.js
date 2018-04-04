//
// Test code
//
//
function test1() {
  var desc = new ActionDescriptor();  // Make

  var sref = new ActionReference();   // Snapshot
  sref.putClass(cTID("SnpS"));
  desc.putReference(cTID("null"), sref);
  
  var fref = new ActionReference();    // Current History State
  fref.putProperty(cTID("HstS"), cTID("CrnH"));
  desc.putReference(cTID("From"), fref );
  
  return desc;
}

function test2() {
  function Style() {
    var self = this;
    self.name = "TEMP";
    self.scale = 100.0;
    self.masterFX = true;
    self.frameStyle = '';
    self.frameFile = '';
    self.blendMode = '';
    self.opacity = 100.0;
    self.size = 3.00;
    self.fillOpacity = 100.0;
    self.color = new SolidColor();
  };
  var style = new Style();

  var mkDesc = new ActionDescriptor();      // Make
  var sref = new ActionReference();         //Style
  sref.putClass(cTID("Styl"));
  mkDesc.putReference(cTID("null"), sref);

  var stDesc = new ActionDescriptor();          // Using
  stDesc.putString(cTID("Nm  "), style.name);   // Name

  stDesc.putObject(sTID("documentMode"), sTID("documentMode"),
                   new ActionDescriptor());

  var leDesc = new ActionDescriptor();    // Layer effects
  leDesc.putUnitDouble(cTID("Scl "), cTID("#Prc"), style.scale);  // scale
  leDesc.putBoolean(sTID("masterFXSwitch"), style.masterFX);

  var fxDesc = new ActionDescriptor();    // Frame FX
  fxDesc.putBoolean(cTID("enab"), true);
  fxDesc.putEnumerated(cTID("Styl"), cTID("FStl"), cTID("OutF"));  // Frame Style
  fxDesc.putEnumerated(cTID("PntT"), cTID("FrFl"), cTID("SClr"));  // Frame Fill
  fxDesc.putEnumerated(cTID("Md  "), cTID("BlnM"), cTID("Nrml"));  // Blend Mode
  fxDesc.putUnitDouble(cTID("Opct"), cTID("#Prc"), style.opacity); // Opacity 
  fxDesc.putUnitDouble(cTID("Sz  "), cTID("#Pxl"), style.size);    // Size

  var clDesc = new ActionDescriptor();   // Color
  clDesc.putDouble(cTID("Rd  "), style.color.rgb.red);
  clDesc.putDouble(cTID("Grn "), style.color.rgb.green);
  clDesc.putDouble(cTID("Bl  "), style.color.rgb.blue);

  fxDesc.putObject(cTID("Clr "), cTID("RGBC"), clDesc);

  leDesc.putObject(cTID("FrFX"), cTID("FrFX"), fxDesc);

  stDesc.putObject(cTID("Lefx"), cTID("Lefx"), leDesc);

  var boDesc = new ActionDescriptor();                   // Blend Options
  boDesc.putUnitDouble(sTID("fillOpacity"), cTID("#Prc"), style.fillOpacity);
  stDesc.putObject(sTID("blendOptions"), sTID("blendOptions"), boDesc);

  mkDesc.putObject(cTID("Usng"), cTID("Styl"), stDesc);
  mkDesc.putBoolean(sTID("blendOptions"), true);
  mkDesc.putBoolean(cTID("Lefx"), true);
  return mkDesc;
};

function test3() {
    var id43 = charIDToTypeID( "setd" );
    var desc9 = new ActionDescriptor();
    var id44 = charIDToTypeID( "null" );
        var ref2 = new ActionReference();
        var id45 = charIDToTypeID( "Lyr " );
        var id46 = charIDToTypeID( "Ordn" );
        var id47 = charIDToTypeID( "Trgt" );
        ref2.putEnumerated( id45, id46, id47 );
    desc9.putReference( id44, ref2 );
    var id48 = charIDToTypeID( "T   " );
        var desc10 = new ActionDescriptor();
        var id49 = stringIDToTypeID( "fillOpacity" );
        var id50 = charIDToTypeID( "#Prc" );
        desc10.putUnitDouble( id49, id50, 100.000000 );
        var id51 = charIDToTypeID( "Lefx" );
            var desc11 = new ActionDescriptor();
            var id52 = charIDToTypeID( "Scl " );
            var id53 = charIDToTypeID( "#Prc" );
            desc11.putUnitDouble( id52, id53, 347.222222 );
            var id54 = charIDToTypeID( "DrSh" );
                var desc12 = new ActionDescriptor();
                var id55 = charIDToTypeID( "enab" );
                desc12.putBoolean( id55, true );
                var id56 = charIDToTypeID( "Md  " );
                var id57 = charIDToTypeID( "BlnM" );
                var id58 = charIDToTypeID( "Mltp" );
                desc12.putEnumerated( id56, id57, id58 );
                var id59 = charIDToTypeID( "Clr " );
                    var desc13 = new ActionDescriptor();
                    var id60 = charIDToTypeID( "Rd  " );
                    desc13.putDouble( id60, 0.000000 );
                    var id61 = charIDToTypeID( "Grn " );
                    desc13.putDouble( id61, 0.000000 );
                    var id62 = charIDToTypeID( "Bl  " );
                    desc13.putDouble( id62, 0.000000 );
                var id63 = charIDToTypeID( "RGBC" );
                desc12.putObject( id59, id63, desc13 );
                var id64 = charIDToTypeID( "Opct" );
                var id65 = charIDToTypeID( "#Prc" );
                desc12.putUnitDouble( id64, id65, 75.000000 );
                var id66 = charIDToTypeID( "uglg" );
                desc12.putBoolean( id66, true );
                var id67 = charIDToTypeID( "lagl" );
                var id68 = charIDToTypeID( "#Ang" );
                desc12.putUnitDouble( id67, id68, 120.000000 );
                var id69 = charIDToTypeID( "Dstn" );
                var id70 = charIDToTypeID( "#Pxl" );
                desc12.putUnitDouble( id69, id70, 7.000000 );
                var id71 = charIDToTypeID( "Ckmt" );
                var id72 = charIDToTypeID( "#Pxl" );
                desc12.putUnitDouble( id71, id72, 0.000000 );
                var id73 = charIDToTypeID( "blur" );
                var id74 = charIDToTypeID( "#Pxl" );
                desc12.putUnitDouble( id73, id74, 17.000000 );
                var id75 = charIDToTypeID( "Nose" );
                var id76 = charIDToTypeID( "#Prc" );
                desc12.putUnitDouble( id75, id76, 0.000000 );
                var id77 = charIDToTypeID( "AntA" );
                desc12.putBoolean( id77, false );
                var id78 = charIDToTypeID( "TrnS" );
                    var desc14 = new ActionDescriptor();
                    var id79 = charIDToTypeID( "Nm  " );
                    desc14.putString( id79, "Linear" );
                var id80 = charIDToTypeID( "ShpC" );
                desc12.putObject( id78, id80, desc14 );
                var id81 = stringIDToTypeID( "layerConceals" );
                desc12.putBoolean( id81, true );
            var id82 = charIDToTypeID( "DrSh" );
            desc11.putObject( id54, id82, desc12 );
            var id83 = charIDToTypeID( "ebbl" );
                var desc15 = new ActionDescriptor();
                var id84 = charIDToTypeID( "enab" );
                desc15.putBoolean( id84, true );
                var id85 = charIDToTypeID( "hglM" );
                var id86 = charIDToTypeID( "BlnM" );
                var id87 = charIDToTypeID( "Scrn" );
                desc15.putEnumerated( id85, id86, id87 );
                var id88 = charIDToTypeID( "hglC" );
                    var desc16 = new ActionDescriptor();
                    var id89 = charIDToTypeID( "Rd  " );
                    desc16.putDouble( id89, 225.000000 );
                    var id90 = charIDToTypeID( "Grn " );
                    desc16.putDouble( id90, 241.996109 );
                    var id91 = charIDToTypeID( "Bl  " );
                    desc16.putDouble( id91, 241.996109 );
                var id92 = charIDToTypeID( "RGBC" );
                desc15.putObject( id88, id92, desc16 );
                var id93 = charIDToTypeID( "hglO" );
                var id94 = charIDToTypeID( "#Prc" );
                desc15.putUnitDouble( id93, id94, 48.000000 );
                var id95 = charIDToTypeID( "sdwM" );
                var id96 = charIDToTypeID( "BlnM" );
                var id97 = charIDToTypeID( "Mltp" );
                desc15.putEnumerated( id95, id96, id97 );
                var id98 = charIDToTypeID( "sdwC" );
                    var desc17 = new ActionDescriptor();
                    var id99 = charIDToTypeID( "Rd  " );
                    desc17.putDouble( id99, 78.000000 );
                    var id100 = charIDToTypeID( "Grn " );
                    desc17.putDouble( id100, 54.000000 );
                    var id101 = charIDToTypeID( "Bl  " );
                    desc17.putDouble( id101, 16.000000 );
                var id102 = charIDToTypeID( "RGBC" );
                desc15.putObject( id98, id102, desc17 );
                var id103 = charIDToTypeID( "sdwO" );
                var id104 = charIDToTypeID( "#Prc" );
                desc15.putUnitDouble( id103, id104, 32.000000 );
                var id105 = charIDToTypeID( "bvlT" );
                var id106 = charIDToTypeID( "bvlT" );
                var id107 = charIDToTypeID( "SfBL" );
                desc15.putEnumerated( id105, id106, id107 );
                var id108 = charIDToTypeID( "bvlS" );
                var id109 = charIDToTypeID( "BESl" );
                var id110 = charIDToTypeID( "InrB" );
                desc15.putEnumerated( id108, id109, id110 );
                var id111 = charIDToTypeID( "uglg" );
                desc15.putBoolean( id111, true );
                var id112 = charIDToTypeID( "lagl" );
                var id113 = charIDToTypeID( "#Ang" );
                desc15.putUnitDouble( id112, id113, 120.000000 );
                var id114 = charIDToTypeID( "Lald" );
                var id115 = charIDToTypeID( "#Ang" );
                desc15.putUnitDouble( id114, id115, 30.000000 );
                var id116 = charIDToTypeID( "srgR" );
                var id117 = charIDToTypeID( "#Prc" );
                desc15.putUnitDouble( id116, id117, 100.000000 );
                var id118 = charIDToTypeID( "blur" );
                var id119 = charIDToTypeID( "#Pxl" );
                desc15.putUnitDouble( id118, id119, 45.000000 );
                var id120 = charIDToTypeID( "bvlD" );
                var id121 = charIDToTypeID( "BESs" );
                var id122 = charIDToTypeID( "In  " );
                desc15.putEnumerated( id120, id121, id122 );
                var id123 = charIDToTypeID( "TrnS" );
                    var desc18 = new ActionDescriptor();
                    var id124 = charIDToTypeID( "Nm  " );
                    desc18.putString( id124, "Linear" );
                var id125 = charIDToTypeID( "ShpC" );
                desc15.putObject( id123, id125, desc18 );
                var id129 = stringIDToTypeID( "useShape" );
                desc15.putBoolean( id129, false );
                var id126 = stringIDToTypeID( "antialiasGloss" );
                desc15.putBoolean( id126, false );
                var id127 = charIDToTypeID( "Sftn" );
                var id130 = stringIDToTypeID( "useTexture" );
                desc15.putBoolean( id130, false );
                var id128 = charIDToTypeID( "#Pxl" );
                desc15.putUnitDouble( id127, id128, 0.000000 );
            var id131 = charIDToTypeID( "ebbl" );
            desc11.putObject( id83, id131, desc15 );
            var id132 = charIDToTypeID( "ChFX" );
                var desc19 = new ActionDescriptor();
                var id133 = charIDToTypeID( "enab" );
                desc19.putBoolean( id133, true );
                var id134 = charIDToTypeID( "Md  " );
                var id135 = charIDToTypeID( "BlnM" );
                var id136 = charIDToTypeID( "Ovrl" );
                desc19.putEnumerated( id134, id135, id136 );
                var id137 = charIDToTypeID( "Clr " );
                    var desc20 = new ActionDescriptor();
                    var id138 = charIDToTypeID( "Rd  " );
                    desc20.putDouble( id138, 0.000000 );
                    var id139 = charIDToTypeID( "Grn " );
                    desc20.putDouble( id139, 0.000000 );
                    var id140 = charIDToTypeID( "Bl  " );
                    desc20.putDouble( id140, 0.000000 );
                var id141 = charIDToTypeID( "RGBC" );
                desc19.putObject( id137, id141, desc20 );
                var id142 = charIDToTypeID( "AntA" );
                desc19.putBoolean( id142, false );
                var id143 = charIDToTypeID( "Invr" );
                desc19.putBoolean( id143, false );
                var id144 = charIDToTypeID( "Opct" );
                var id145 = charIDToTypeID( "#Prc" );
                desc19.putUnitDouble( id144, id145, 58.000000 );
                var id146 = charIDToTypeID( "lagl" );
                var id147 = charIDToTypeID( "#Ang" );
                desc19.putUnitDouble( id146, id147, -156.000000 );
                var id148 = charIDToTypeID( "Dstn" );
                var id149 = charIDToTypeID( "#Pxl" );
                desc19.putUnitDouble( id148, id149, 76.000000 );
                var id150 = charIDToTypeID( "blur" );
                var id151 = charIDToTypeID( "#Pxl" );
                desc19.putUnitDouble( id150, id151, 49.000000 );
                var id152 = charIDToTypeID( "MpgS" );
                    var desc21 = new ActionDescriptor();
                    var id153 = charIDToTypeID( "Nm  " );
                    desc21.putString( id153, "Gaussian" );
                var id154 = charIDToTypeID( "ShpC" );
                desc19.putObject( id152, id154, desc21 );
            var id155 = charIDToTypeID( "ChFX" );
            desc11.putObject( id132, id155, desc19 );
            var id156 = charIDToTypeID( "GrFl" );
                var desc22 = new ActionDescriptor();
                var id157 = charIDToTypeID( "enab" );
                desc22.putBoolean( id157, true );
                var id158 = charIDToTypeID( "Md  " );
                var id159 = charIDToTypeID( "BlnM" );
                var id160 = charIDToTypeID( "Nrml" );
                desc22.putEnumerated( id158, id159, id160 );
                var id161 = charIDToTypeID( "Opct" );
                var id162 = charIDToTypeID( "#Prc" );
                desc22.putUnitDouble( id161, id162, 100.000000 );
                var id163 = charIDToTypeID( "Grad" );
                    var desc23 = new ActionDescriptor();
                    var id164 = charIDToTypeID( "Nm  " );
                    desc23.putString( id164, "Chrome" );
                    var id165 = charIDToTypeID( "GrdF" );
                    var id166 = charIDToTypeID( "GrdF" );
                    var id167 = charIDToTypeID( "CstS" );
                    desc23.putEnumerated( id165, id166, id167 );
                    var id168 = charIDToTypeID( "Intr" );
                    desc23.putDouble( id168, 4096.000000 );
                    var id169 = charIDToTypeID( "Clrs" );
                        var list1 = new ActionList();
                            var desc24 = new ActionDescriptor();
                            var id170 = charIDToTypeID( "Clr " );
                                var desc25 = new ActionDescriptor();
                                var id171 = charIDToTypeID( "Rd  " );
                                desc25.putDouble( id171, 33.571984 );
                                var id172 = charIDToTypeID( "Grn " );
                                desc25.putDouble( id172, 76.770428 );
                                var id173 = charIDToTypeID( "Bl  " );
                                desc25.putDouble( id173, 107.003891 );
                            var id174 = charIDToTypeID( "RGBC" );
                            desc24.putObject( id170, id174, desc25 );
                            var id175 = charIDToTypeID( "Type" );
                            var id176 = charIDToTypeID( "Clry" );
                            var id177 = charIDToTypeID( "UsrS" );
                            desc24.putEnumerated( id175, id176, id177 );
                            var id178 = charIDToTypeID( "Lctn" );
                            desc24.putInteger( id178, 0 );
                            var id179 = charIDToTypeID( "Mdpn" );
                            desc24.putInteger( id179, 50 );
                        var id180 = charIDToTypeID( "Clrt" );
                        list1.putObject( id180, desc24 );
                            var desc26 = new ActionDescriptor();
                            var id181 = charIDToTypeID( "Clr " );
                                var desc27 = new ActionDescriptor();
                                var id182 = charIDToTypeID( "Rd  " );
                                desc27.putDouble( id182, 40.801556 );
                                var id183 = charIDToTypeID( "Grn " );
                                desc27.putDouble( id183, 136.805447 );
                                var id184 = charIDToTypeID( "Bl  " );
                                desc27.putDouble( id184, 203.996109 );
                            var id185 = charIDToTypeID( "RGBC" );
                            desc26.putObject( id181, id185, desc27 );
                            var id186 = charIDToTypeID( "Type" );
                            var id187 = charIDToTypeID( "Clry" );
                            var id188 = charIDToTypeID( "UsrS" );
                            desc26.putEnumerated( id186, id187, id188 );
                            var id189 = charIDToTypeID( "Lctn" );
                            desc26.putInteger( id189, 567 );
                            var id190 = charIDToTypeID( "Mdpn" );
                            desc26.putInteger( id190, 50 );
                        var id191 = charIDToTypeID( "Clrt" );
                        list1.putObject( id191, desc26 );
                            var desc28 = new ActionDescriptor();
                            var id192 = charIDToTypeID( "Clr " );
                                var desc29 = new ActionDescriptor();
                                var id193 = charIDToTypeID( "Rd  " );
                                desc29.putDouble( id193, 233.000000 );
                                var id194 = charIDToTypeID( "Grn " );
                                desc29.putDouble( id194, 249.996109 );
                                var id195 = charIDToTypeID( "Bl  " );
                                desc29.putDouble( id195, 255.000000 );
                            var id196 = charIDToTypeID( "RGBC" );
                            desc28.putObject( id192, id196, desc29 );
                            var id197 = charIDToTypeID( "Type" );
                            var id198 = charIDToTypeID( "Clry" );
                            var id199 = charIDToTypeID( "UsrS" );
                            desc28.putEnumerated( id197, id198, id199 );
                            var id200 = charIDToTypeID( "Lctn" );
                            desc28.putInteger( id200, 2032 );
                            var id201 = charIDToTypeID( "Mdpn" );
                            desc28.putInteger( id201, 50 );
                        var id202 = charIDToTypeID( "Clrt" );
                        list1.putObject( id202, desc28 );
                            var desc30 = new ActionDescriptor();
                            var id203 = charIDToTypeID( "Clr " );
                                var desc31 = new ActionDescriptor();
                                var id204 = charIDToTypeID( "Rd  " );
                                desc31.putDouble( id204, 93.003891 );
                                var id205 = charIDToTypeID( "Grn " );
                                desc31.putDouble( id205, 68.482490 );
                                var id206 = charIDToTypeID( "Bl  " );
                                desc31.putDouble( id206, 0.731518 );
                            var id207 = charIDToTypeID( "RGBC" );
                            desc30.putObject( id203, id207, desc31 );
                            var id208 = charIDToTypeID( "Type" );
                            var id209 = charIDToTypeID( "Clry" );
                            var id210 = charIDToTypeID( "UsrS" );
                            desc30.putEnumerated( id208, id209, id210 );
                            var id211 = charIDToTypeID( "Lctn" );
                            desc30.putInteger( id211, 2481 );
                            var id212 = charIDToTypeID( "Mdpn" );
                            desc30.putInteger( id212, 52 );
                        var id213 = charIDToTypeID( "Clrt" );
                        list1.putObject( id213, desc30 );
                            var desc32 = new ActionDescriptor();
                            var id214 = charIDToTypeID( "Clr " );
                                var desc33 = new ActionDescriptor();
                                var id215 = charIDToTypeID( "Rd  " );
                                desc33.putDouble( id215, 217.000000 );
                                var id216 = charIDToTypeID( "Grn " );
                                desc33.putDouble( id216, 159.334630 );
                                var id217 = charIDToTypeID( "Bl  " );
                                desc33.putDouble( id217, 0.007782 );
                            var id218 = charIDToTypeID( "RGBC" );
                            desc32.putObject( id214, id218, desc33 );
                            var id219 = charIDToTypeID( "Type" );
                            var id220 = charIDToTypeID( "Clry" );
                            var id221 = charIDToTypeID( "UsrS" );
                            desc32.putEnumerated( id219, id220, id221 );
                            var id222 = charIDToTypeID( "Lctn" );
                            desc32.putInteger( id222, 2823 );
                            var id223 = charIDToTypeID( "Mdpn" );
                            desc32.putInteger( id223, 50 );
                        var id224 = charIDToTypeID( "Clrt" );
                        list1.putObject( id224, desc32 );
                            var desc34 = new ActionDescriptor();
                            var id225 = charIDToTypeID( "Clr " );
                                var desc35 = new ActionDescriptor();
                                var id226 = charIDToTypeID( "Rd  " );
                                desc35.putDouble( id226, 251.996109 );
                                var id227 = charIDToTypeID( "Grn " );
                                desc35.putDouble( id227, 245.000000 );
                                var id228 = charIDToTypeID( "Bl  " );
                                desc35.putDouble( id228, 225.000000 );
                            var id229 = charIDToTypeID( "RGBC" );
                            desc34.putObject( id225, id229, desc35 );
                            var id230 = charIDToTypeID( "Type" );
                            var id231 = charIDToTypeID( "Clry" );
                            var id232 = charIDToTypeID( "UsrS" );
                            desc34.putEnumerated( id230, id231, id232 );
                            var id233 = charIDToTypeID( "Lctn" );
                            desc34.putInteger( id233, 4096 );
                            var id234 = charIDToTypeID( "Mdpn" );
                            desc34.putInteger( id234, 50 );
                        var id235 = charIDToTypeID( "Clrt" );
                        list1.putObject( id235, desc34 );
                    desc23.putList( id169, list1 );
                    var id236 = charIDToTypeID( "Trns" );
                        var list2 = new ActionList();
                            var desc36 = new ActionDescriptor();
                            var id237 = charIDToTypeID( "Opct" );
                            var id238 = charIDToTypeID( "#Prc" );
                            desc36.putUnitDouble( id237, id238, 100.000000 );
                            var id239 = charIDToTypeID( "Lctn" );
                            desc36.putInteger( id239, 0 );
                            var id240 = charIDToTypeID( "Mdpn" );
                            desc36.putInteger( id240, 50 );
                        var id241 = charIDToTypeID( "TrnS" );
                        list2.putObject( id241, desc36 );
                            var desc37 = new ActionDescriptor();
                            var id242 = charIDToTypeID( "Opct" );
                            var id243 = charIDToTypeID( "#Prc" );
                            desc37.putUnitDouble( id242, id243, 100.000000 );
                            var id244 = charIDToTypeID( "Lctn" );
                            desc37.putInteger( id244, 4096 );
                            var id245 = charIDToTypeID( "Mdpn" );
                            desc37.putInteger( id245, 50 );
                        var id246 = charIDToTypeID( "TrnS" );
                        list2.putObject( id246, desc37 );
                    desc23.putList( id236, list2 );
                var id247 = charIDToTypeID( "Grdn" );
                desc22.putObject( id163, id247, desc23 );
                var id248 = charIDToTypeID( "Angl" );
                var id249 = charIDToTypeID( "#Ang" );
                desc22.putUnitDouble( id248, id249, -90.000000 );
                var id250 = charIDToTypeID( "Type" );
                var id251 = charIDToTypeID( "GrdT" );
                var id252 = charIDToTypeID( "Lnr " );
                desc22.putEnumerated( id250, id251, id252 );
                var id253 = charIDToTypeID( "Rvrs" );
                desc22.putBoolean( id253, false );
                var id254 = charIDToTypeID( "Algn" );
                desc22.putBoolean( id254, true );
                var id255 = charIDToTypeID( "Scl " );
                var id256 = charIDToTypeID( "#Prc" );
                desc22.putUnitDouble( id255, id256, 100.000000 );
                var id257 = charIDToTypeID( "Ofst" );
                    var desc38 = new ActionDescriptor();
                    var id258 = charIDToTypeID( "Hrzn" );
                    var id259 = charIDToTypeID( "#Prc" );
                    desc38.putUnitDouble( id258, id259, 0.000000 );
                    var id260 = charIDToTypeID( "Vrtc" );
                    var id261 = charIDToTypeID( "#Prc" );
                    desc38.putUnitDouble( id260, id261, 0.000000 );
                var id262 = charIDToTypeID( "Pnt " );
                desc22.putObject( id257, id262, desc38 );
            var id263 = charIDToTypeID( "GrFl" );
            desc11.putObject( id156, id263, desc22 );
        var id264 = charIDToTypeID( "Lefx" );
        desc10.putObject( id51, id264, desc11 );
    var id265 = charIDToTypeID( "Lyr " );
    desc9.putObject( id48, id265, desc10 );
    return desc9;
};


testActionDescriptor = function() {
  var obj = new ActionDescriptor();
  obj.putBoolean(cTID("enab"), true);
  obj.putClass(cTID("Usng"), cTID("PcTl"));
  obj.putData(cTID("UsrS"), "some raw data that is really a string");
  obj.putDouble(cTID("Rd  "), 125.25);
  obj.putEnumerated(cTID("Styl"), cTID("FStl"), cTID("OutF"));
  obj.putInteger(sTID("rowCount"), 20);

    var lst = new ActionList();
    lst.putBoolean(cTID("AntA"), false);
  obj.putList(cTID("Pts "), lst);
  
    var ad = new ActionDescriptor();
    ad.putBoolean(cTID("Annt"), true);
  obj.putObject(cTID("T   "), cTID("Lyr "), ad);
  obj.putPath(cTID("jsCt"), new File("/c/ScriptingListenerJS.log"));

    var sref = new ActionReference();
    sref.putClass(cTID("Styl"));
  obj.putReference(cTID("null"), sref);
  obj.putString(sTID("profile"), "sRGB IEC61966-2.1");
  obj.putUnitDouble(cTID("Scl "), cTID("#Prc"), 347.5);

  return obj;
};
testActionList = function() {
  var result = new ActionDescriptor();
  var obj = new ActionList();

  obj.putBoolean(true);
  obj.putClass(cTID("PcTl"));
  obj.putData("some raw data that is really a string");
  obj.putDouble(125.25);
  obj.putEnumerated(cTID("FStl"), cTID("OutF"));
  obj.putInteger(20);

    var lst = new ActionList();
    lst.putBoolean(cTID("AntA"), false);
  obj.putList(lst);
  
    var ad = new ActionDescriptor();
    ad.putBoolean(cTID("Annt"), true);
  obj.putObject(cTID("Lyr "), ad);
  obj.putPath(new File("/c/ScriptingListenerJS.log"));

    var sref = new ActionReference();
    sref.putClass(cTID("Styl"));
  obj.putReference(sref);
  obj.putString("sRGB IEC61966-2.1");
  obj.putUnitDouble(cTID("#Prc"), 347.5);
  result.putList(cTID("null"), obj);
  return result;
};
testSimpleActionReference = function() {
  var result = new ActionDescriptor();
  var lst = new ActionList();
  var ref

  var pdesc = new ActionDescriptor();
  ref = new ActionReference();
  ref.putName(cTID("Actn"), "Test" );
  ref.putName(cTID("ASet"), "XActions" );
  pdesc.putReference(cTID("Link"), ref);
  
  ref = new ActionReference();
  ref.putIndex(PSClass.Action, 20);
  ref.putIndex(PSClass.ActionSet, 15);
  lst.putReference(ref);

  ref = new ActionReference();
  ref.putIndex(cTID("Cmnd"), 2 );
  ref.putName(cTID("Actn"), "Test" );
  ref.putName(cTID("ASet"), "XActions" );
  lst.putReference(ref);
 
  result.putObject(cTID("Ply "), cTID("Ply "), pdesc);
  result.putList(sTID("null"), lst);

  return result;
}
testActionReference = function() {
  var result = new ActionDescriptor();
  var lst = new ActionList();
  var obj;

//   obj = new ActionReference();
//   obj.putClass(cTID("Path"));
//   lst.putReference(obj);

  obj = new ActionReference();
  obj.putEnumerated(cTID("PntT"), cTID("FrFl"), cTID("SClr"));
  lst.putReference(obj);

  obj = new ActionReference();
  obj.putIdentifier(cTID("PcTl"), sTID("documentMode"));
  obj.putEnumerated(cTID("PntT"), cTID("FrFl"), cTID("SClr"));
  lst.putReference(obj);

  obj = new ActionReference();
  obj.putIndex(cTID("Cmnd"), 2);
  lst.putReference(obj);

  obj = new ActionReference();
  obj.putName(cTID("Actn"), "TestName");
  lst.putReference(obj);

  obj = new ActionReference();
  obj.putOffset(cTID("HstS"), -4);
  lst.putReference(obj);

  obj = new ActionReference();
  obj.putProperty(cTID("Chnl"), cTID("fsel"));
  lst.putReference(obj);

  result.putList(cTID("null"), lst);
  return result;
}

//
//
test4 = function() {
    var desc57 = new ActionDescriptor();
    var id282 = charIDToTypeID( "null" );
        var ref44 = new ActionReference();
        var id283 = charIDToTypeID( "Actn" );
        ref44.putName( id283, "jsh" );
         var id284 = charIDToTypeID( "ASet" );
         ref44.putName( id284, "XActions" );
    desc57.putReference( id282, ref44 );
    return desc57;
};

atnbintest = function() {
  var id70 = cTID("Fl  ");
  var desc = new ActionDescriptor();
  desc.putEnumerated(cTID("Usng"), cTID("FlCn"), cTID("FrgC"));
  desc.putUnitDouble(cTID("Opct"), cTID("#Prc"), 100.0);
  var id76 = cTID("Md  ");
  var id77 = cTID("BlnM");
  var id78 = cTID("Nrml");
  desc.putEnumerated(cTID("Md  "), cTID("BlnM"), cTID("Nrml"));
  return desc;
};

atnbin2test = function() {
  var id79 = cTID("slct");
  var desc = new ActionDescriptor();
  var ref = new ActionReference();
  ref.putEnumerated(cTID("HstS"), cTID("Ordn"), cTID("Prvs"));
  desc.putReference(cTID("null"), ref);
  return desc;
}

