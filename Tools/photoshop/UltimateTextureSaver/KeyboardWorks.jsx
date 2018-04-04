//
// ActionsPaletteToFiles
//
// $Id: ActionsPaletteToFiles.jsx,v 1.12 2012/06/15 00:50:23 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include

//@include "UltimateTextureSaverUI/js/json2.js"

//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/PSConstants.js"
//@include "xlib/Stream.js"
//@include "xlib/stdlib.js"
//@include "xlib/Action.js"
//@include "xlib/ActionStream.js"
//@include "xlib/ActionSelector.jsx"
//@include "xlib/xml/atn2bin.jsx"

ActionKbInfo = function() {
    var self = this;
    self.fKey = undefined;              // int16  This is really the function key!
    self.shiftKey = undefined;       // boolean/byte
    self.commandKey = undefined;     //
    self.commandShiftKey = undefined;     //
};
var USED_FKEY       = 1;
var USED_SHIFT      = 2;
var USED_CTRL       = 3;
var USED_SHIFTCTRL  = 4;
var NO_SHORTCUT     = "None";

var ACTION_SET_NAME = "UltimateTextureSaver";
var TARGET_FOLDER = new Folder(Folder.myDocuments.fsName + "\\UltimateTextureSaver");
var TEMP_ACTION_NAME = "UltimateTextureSaver.atn";
var BUTTON_CONFIG = "ButtonsConfig.json";
var SCRIPTS_FOLDER = "Scripts";
{
    if(!TARGET_FOLDER.exists)
        TARGET_FOLDER.create();    
}

var KeyboardWorks = {
    LoadData: function() {
        var opts = {
            source: new File(app.preferencesFolder + "/Actions Palette.psp")
        };

        var keyMappings = {};
        var cryKeyMappings = {};
 
        for(k = 2; k < 13; k++) {
            keyMappings["F"+k] = new ActionKbInfo();
            cryKeyMappings["F"+k] = new ActionKbInfo();
        }

        var palFile = ActionsPaletteFile.readFrom(opts.source);        
        // we have palete file, need to analyze it for used keyboard shorcuts and those in cryactions should be treat differently.
        for (index = 0; index < palFile.actionsPalette.actionSets.length; ++index) {
            var currentSet = palFile.actionsPalette.actionSets[index];
            var targetMap = keyMappings;
            if(currentSet.name == ACTION_SET_NAME) {
                // need to check if by any chance atn file is not newer than palette file, so than atn file will get loaded instead
                var atnFile = new File(TARGET_FOLDER.fsName.concat("\\").concat(TEMP_ACTION_NAME));
                if(atnFile.exists && atnFile.modified > opts.source.modified) {
                    // action file exists and also was modified later than palete file, should load its contents
                    var actFile = new ActionFile();
                    actFile.read(atnFile);
                    currentSet = actFile.getActionSet(); 
                }
        
                targetMap = cryKeyMappings;
            }
                
            for (j = 0; j < currentSet.actions.length; ++j) {
                 var ca = currentSet.actions[j]; 
                 if(ca.index === 0)
                     continue;

                 var fKey = "F"+ca.index;
                 if(ca.shiftKey && ca.commandKey)
                 {
                     targetMap[fKey].commandShiftKey = ca.name;
                 } else if (ca.shiftKey) {
                     targetMap[fKey].shiftKey = ca.name;                
                 } else if (ca.commandKey) {
                     targetMap[fKey].commandKey = ca.name;                
                 } else {
                   targetMap[fKey].fKey = ca.name;  

                 }            
             }
        }
        
        return {usedKeys: keyMappings, cryKeys: cryKeyMappings};
    
    },
    LoadDefaultKBConfig: function() {
      var config =File(configPath);
            if(!config.exists)
            {
                alert("File does not exist");
                return;  
            }
              
            config.open('r');
            var configXML = config.read();
            config.close();
            var finalXML = XML (configXML);
            
            for (var texture in finalXML.Texture) {     
     
                var newTexture = new UTSTexture(finalXML.Texture[texture]);     
                UltimateTextureSaver.Textures[newTexture.name] = newTexture;
               
            }  
    },
    APILoadData: function() {
        var xml = '<object>';                 
        xml += '<property id="json"><string>'+JSON.stringify(KeyboardWorks.LoadData())+'</string></property>';
        xml += '</object>';
        return xml;
    },
    GetCurrentKBLayout: function() {
        var configFile = new File(TARGET_FOLDER.fsName.concat("\\").concat(BUTTON_CONFIG));
        if(!configFile.exists)
            return "";
        
        return Stdlib.readFromFile(configFile);
    },
    APIGetCurrentKBLayout: function() {
        var xml = '<object>';                 
        xml += '<property id="json"><string>'+KeyboardWorks.GetCurrentKBLayout()+'</string></property>';
        xml += '</object>';
        return xml;        
    },
    KBLayoutExists: function() {
        var configFile = new File(TARGET_FOLDER.fsName.concat("\\").concat(BUTTON_CONFIG));
        return configFile.exists;            
    },
    APIKBLayoutExists: function() {
        var xml = '<object>'; 
        var exists = "<true/>";
        if(!KeyboardWorks.KBLayoutExists()) {
             exists = "<false/>"; 
        }
        xml += '<property id="exists">'+exists+'</property>';
        xml += '</object>';
        return xml;  
    },
    SaveNewKBLayout: function(jsonStr) {
        
        var data = JSON.parse(jsonStr);
        var af = new ActionFile();

        af.file = TARGET_FOLDER.fsName.concat("\\").concat(TEMP_ACTION_NAME) ;
        if (af.file) af.file = new File(af.file);

        var as = new ActionSet();
        as.parent = af;
        as.name = ACTION_SET_NAME;
        as.count = data.length;
        as.expanded = true;
        
        var scriptsFolder = new Folder(TARGET_FOLDER.fsName.concat("\\").concat(SCRIPTS_FOLDER));
      
        if(scriptsFolder.exists) {
            var scriptFiles = scriptsFolder.getFiles();            
            for (var f = 0; f < scriptFiles.length; f++) {                
                scriptFiles[f].remove();
            }
        }
        
        for (var key in data) {
            var scriptPath = KeyboardWorks.WriteTextureScript(key);       
            var actItem = new ActionItem();
            actItem.setEvent("AdobeScriptAutomation Scripts");
           
            var desc3 = new ActionDescriptor();  
            var idjsCt = charIDToTypeID( "jsCt" );  
            desc3.putPath( idjsCt, new File( scriptPath));  
            actItem.setDescriptor(desc3);
            //executeAction( idAdobeScriptAutomationScripts, desc3, DialogModes.NO );  
            var act = new Action();
            act.parent = as;
            act.name = key;             
            var actData = data[key];
            act.index = parseInt(actData.fKey.substring(1));
            if(actData.used == USED_SHIFTCTRL) {
                act.shiftKey = true;
                act.commandKey = true;                
            } else if (actData.used == USED_CTRL) {
                act.shiftKey = false;
                act.commandKey = true;                
            } else if (actData.used == USED_SHIFT) {
                act.shiftKey = true;
                act.commandKey = false;              
            }
            act.add(actItem);
            as.add(act);

        }
        af.setActionSet(as);
        af.write(TARGET_FOLDER.fsName.concat("\\").concat(TEMP_ACTION_NAME));
        try
        {
            var idDlt = charIDToTypeID( "Dlt " );
            var desc4 = new ActionDescriptor();
            var idnull = charIDToTypeID( "null" );
            var ref4 = new ActionReference();
            var idASet = charIDToTypeID( "ASet" );
            ref4.putName( idASet, ACTION_SET_NAME );
            desc4.putReference( idnull, ref4 );
            executeAction( idDlt, desc4, DialogModes.NO );
            
        }
        catch(err) {
            
        }

        Stdlib.writeToFile(new File(TARGET_FOLDER.fsName.concat("\\").concat(BUTTON_CONFIG)), jsonStr);
        
        app.load (new File(TARGET_FOLDER.fsName.concat("\\").concat(TEMP_ACTION_NAME)));
        
    },
    WriteTextureScript: function(texture_name) {
        var scriptText = '' +
        'var PHSPJSRegistry = new ExternalObject("lib:"+"c:/Program Files/Adobe/JavaScriptExtensions/PhotoshopJSRegistry.dll");\n'+
        'var engine_path = PHSPJSRegistry.getRegistryValue("HKEY_CURRENT_USER","Software\\\\Crytek\\\\Settings", "ENG_RootPath");\n'+
        'engine_path = engine_path.replace(/\\\\/g, "/");\n'+
        '$.evalFile(engine_path.concat("/Tools/photoshop/UltimateTextureSaver/UltimateTextureSaver.jsx"));\n'+
        'UltimateTextureSaver.LoadTextureDefinitions(engine_path.concat("/Tools/photoshop/UltimateTextureSaver/UltimateTextureSaver.xml"));\n'+
        'UltimateTextureSaver.Save("'+texture_name+'", false);\n';
        
        var scriptsFolder = new Folder(TARGET_FOLDER.fsName.concat("\\").concat(SCRIPTS_FOLDER));
      
        if(!scriptsFolder.exists) {
            scriptsFolder.create();
        }
        var scriptPath = new File(scriptsFolder.fsName.concat("\\").concat(texture_name).concat(".jsx"));
        Stdlib.writeToFile(scriptPath, scriptText);
        return scriptPath.fsName;
    }
};


// EOF 