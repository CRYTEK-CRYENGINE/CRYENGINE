ActionKbInfo = function() {
    var self = this;
    self.fKey = false;
    self.shiftKey = false;
    self.commandKey = false;
    self.commandShiftKey = false;
    return self;
};

var USED_FKEY       = 1;
var USED_SHIFT      = 2;
var USED_CTRL       = 3;
var USED_SHIFTCTRL  = 4;
var NO_SHORTCUT     = "None";

ActionKbInfo.prototype.isAvailable = function() {
    var self = this;
    return !(self.fKey && self.shiftKey && self.commandKey && self.commandShiftKey);
};

ActionKbInfo.prototype.canHave = function(newConfig) {
    var self = this;

    if(newConfig.fKey && self.fKey)
        return false;
    if(newConfig.shiftKey && self.shiftKey)
        return false;
    if(newConfig.commandKey && self.commandKey)
        return false;
    if(newConfig.commandShiftKey && self.commandShiftKey)
        return false;
    
    return true;
};

ActionKbInfo.prototype.getFreeSlots = function() {
    var self = this;
    var freeSlots = [];

    if(!self.fKey)
        freeSlots.push(USED_FKEY);
    
    if(!self.shiftKey)
        freeSlots.push(USED_SHIFT);
    
    if(!self.commandKey)
        freeSlots.push(USED_CTRL);
    
    if(!self.commandShiftKey)
        freeSlots.push(USED_SHIFTCTRL);
    
    
    return freeSlots;
};


var UltimateUI = {
        Buttons : [],
        ConfigPath: "",
	LoadConfig: function (configPath) {
            UltimateUI.ConfigPath = configPath;
		$.ajax({
		    type: "GET",
		    url: configPath,
		    dataType: "xml",
		    async: false,
		    success: UltimateUI.CreateButtons
		});
		
	   },
	AddShortcutsInfo: function() {
                var art = window.parentSandboxBridge.EvalPSScript("KeyboardWorks.APIGetCurrentKBLayout");
                var kbConfig = undefined;
                if(art.data.json != "")
                    kbConfig = JSON.parse(art.data.json);
                else
                    return;
                $('#mainList').find("span.btn_name").each(function() {
                   var buttonName = $(this).html();
                   if(kbConfig.hasOwnProperty(buttonName))
                   {
                       var addString = "";
                       var used = kbConfig[buttonName].used;
                       if(used == USED_SHIFT) {
                           addString = "Shift+";
                       } else if (used == USED_CTRL) {
                           addString = "Ctrl+";                                
                       } else if (used == USED_SHIFTCTRL)
                       {
                           addString = "Ctrl+Shift+";                                                                
                       }
                       $(this).next("span.btn_kb").html((addString).concat(kbConfig[buttonName].fKey));
                   }
                
                    
                });
                return;            
            
        },
	CreateButtons: function (xml) {
                
            
		$(xml).find("UltimateTextureSaver").find("Texture").each(function () {
			var buttonName = $(this).attr ('name');
			var ul = $('<ul class="button-row"></ul>');
		   	var li = $('<li>');
                        var buttonNameContent = "<span class='btn_name'>".concat(buttonName).concat("</span><span class='btn_kb'/>");
                        
		   	var a = $('<a href="#" class="btn btn-default btn-lg btn-block btn-sm"></a>').html(buttonNameContent);
                        UltimateUI.Buttons.push(buttonName)
		   	$(a).on('click', function(e) { 
			   if( e.which == 2 ) {
			      e.preventDefault();
			      UltimateUI.PerformSave(buttonName, true) 
			   }
			   if( e.which == 1 ) {
			      e.preventDefault();
			      UltimateUI.PerformSave(buttonName, false)
			   }
			});
		   	
		   	li.append(a);
		   	
		   	ul.append(li);
		   	var li = $('<li>');
		   	var a = $('<a href="#" class="btn btn-default btn-lg btn-block btn-sm glyphicon glyphicon-plus-sign"></a> ')
		   	$(a).on('click', function(e) { 
			   if( e.which == 1 ) {
			      e.preventDefault();
			      UltimateUI.CreateLayout(buttonName)
			   }
			});
		   	
		   	li.append(a);
		   	ul.append(li);
                        var li2 = $('<li>');
                        li2.append(ul);
		   	$('#mainList').append(li2);
	
		}); 	
		$(xml).find("UltimateTextureSaver").each(function() {
			$('#save_path').html($(this).attr('path'));   
		});
                
                UltimateUI.AddShortcutsInfo();
	},
	CreateLayout: function(button) {		
//		var art = window.parentSandboxBridge.EvalPSScript("UltimateTextureSaver.CommunicationTest");
	//	alert(art.status)
	//	alert(art.data.path)
		 window.parentSandboxBridge.EvalPSScript("UltimateTextureSaver.CreateLayout", button);
	},
	
	PerformSave: function(button, showUI) {
		var showDlg = (showUI ? "true" : "false");
		window.parentSandboxBridge.EvalPSScript("UltimateTextureSaver.Evaluator", "UltimateTextureSaver.Save(\""+button+"\","+ showDlg+")");
	},
	
	DocumentActive: function() {
		var art = window.parentSandboxBridge.EvalPSScript("UltimateTextureSaver.APIDocumentActive");
	
		return art.data.active;
	

	},

	GetSavePath: function() {
		var art = window.parentSandboxBridge.EvalPSScript("UltimateTextureSaver.APIGetSavePath");
		return art.data.path;
	

	},
        DefaultKeyMapping: {},       
        ButtonKeys: {},
        AddButtonKey: function(newKey, used, key) {
        
            if (newKey != undefined && UltimateUI.Buttons.indexOf(newKey) > -1)
            {
                
                UltimateUI.ButtonKeys[newKey] = {fKey :key, used:used};

            }             
        },
        GenerateKeyboardEditor: function() {
            
            var fKeys = [
                {name: NO_SHORTCUT},
                {name: "F2"},{name: "F3"},{name: "F4"},{name: "F5"},{name: "F6"},
                {name: "F7"},{name: "F8"},{name: "F9"},{name: "F10"},{name: "F11"},{name: "F12"}
            ];
            
            var art = window.parentSandboxBridge.EvalPSScript("KeyboardWorks.APILoadData");
            var keyData = JSON.parse(art.data.json);
            // we have key mapping data, at this point UI logic has to be implemented, but how????
            // we need to exclude completelly combinations that are not allowed because they are used by pallete
            // contrary, if key mappings are in CryActions, than we should try to map them to existing textures, or just forget them, if they don't match.
            ////////////////////////////
            // need to store default mapping so it will not be used anywhere.
            UltimateUI.FillKeyMappingData(keyData.usedKeys, UltimateUI.DefaultKeyMapping, false);
            UltimateUI.FillKeyMappingData(keyData.cryKeys, UltimateUI.DefaultKeyMapping, true);    
            for(var key in keyData.cryKeys) {                                           
                var cObj = keyData.cryKeys[key];
               
                if(cObj.hasOwnProperty("fKey"))
                {
                    UltimateUI.AddButtonKey(cObj.fKey, USED_FKEY, key);
                }
                    
                if(cObj.hasOwnProperty("shiftKey"))
                {
                    UltimateUI.AddButtonKey(cObj.shiftKey, USED_SHIFT, key);
                }
                 
                if(cObj.hasOwnProperty("commandKey"))
                {
                    UltimateUI.AddButtonKey(cObj.commandKey, USED_CTRL, key);
                }
                    
                if(cObj.hasOwnProperty("commandShiftKey"))
                {
                    UltimateUI.AddButtonKey(cObj.commandShiftKey, USED_SHIFTCTRL, key);                    
                }
            }
            
            // need to check default key mapping if there is something missing in the system, so it is possible to autmatically assign it
            $.ajax({
                type: "GET",
                url: UltimateUI.ConfigPath,
                dataType: "xml",
                async: false,
                success: function(xml) {
                    $(xml).find("UltimateTextureSaver").find("Texture").each(function () {
                        if(!this.hasAttribute('FKey'))
                            return;                        
                        var buttonName = $(this).attr ('name');
                        
                        if( UltimateUI.ButtonKeys.hasOwnProperty(buttonName))
                            return;
                       
                        var fKey = $(this).attr('FKey');
                        var useShift = false;
                        var useCtrl = false;
                        if(this.hasAttribute('Shift') && $(this).attr('Shift') == "true")
                            useShift = true;
                        if(this.hasAttribute('Ctrl') && $(this).attr('Ctrl') == "true")
                            useCtrl = true;
                        
                        var finalSetting = USED_FKEY;
                        if(useShift && useCtrl)
                            finalSetting = USED_SHIFTCTRL;
                        else if (useShift)
                            finalSetting = USED_SHIFT;
                        else if (useCtrl)
                            finalSetting = USED_CTRL;
                        
                        var possibleSeetings = UltimateUI.DefaultKeyMapping[fKey].getFreeSlots();

                        if (possibleSeetings.indexOf(finalSetting) > -1)
                        {
                           
                            //the slot is free, so we can assign it to current button
                            UltimateUI.AddButtonKey(buttonName, finalSetting, fKey);   
                             switch(finalSetting) {
                                case USED_CTRL:
                                   UltimateUI.DefaultKeyMapping[fKey].commandKey = true;
                                    break;
                                case USED_SHIFTCTRL:
                                   UltimateUI.DefaultKeyMapping[fKey].commandShiftKey = true;                       
                                    break;
                                case USED_SHIFT:
                                   UltimateUI.DefaultKeyMapping[fKey].shiftKey = true;                     
                                    break;
                                default:
                                   UltimateUI.DefaultKeyMapping[fKey].fKey = true;                     
                            }
                        }

                    });
                    
                    
                }
            });
		
            var source = $("#keyboard-row").html();
            var template = Handlebars.compile(source);
            
            
            $.each(UltimateUI.Buttons, function( index, value ) {
                var defFKey = NO_SHORTCUT;
                var defCtrl = false;
                var defShift = false;
                if(UltimateUI.ButtonKeys.hasOwnProperty(value))
                {
                    var curButton = UltimateUI.ButtonKeys[value];
                    defFKey = curButton.fKey;
                    if(curButton.used == USED_SHIFTCTRL)
                    {
                        defCtrl = true;
                        defShift = true;                        
                    } else if (curButton.used == USED_CTRL) {
                        defCtrl = true;
                    }else if (curButton.used == USED_SHIFT) {
                        defShift = true;
                    }
                    
                }
                var data = {
                    name: value,
                    id: index,
                    fKeys: fKeys,
                    ctrl:defCtrl,
                    shift: defShift,
                    fKey: defFKey
                };
                var newRow = $(template(data));
                newRow.data("uts",value);
                // need to set proper settings for a row
                
                $("#kbEditor").append(newRow);
             });
             
            $(".dropdown-menu").parent().on('show.bs.dropdown', function (e) {
                
               // alert($(e.relatedTarget).parent().parent().data("uts"));
                for(var key in UltimateUI.DefaultKeyMapping)
                {

                    if(!UltimateUI.DefaultKeyMapping[key].isAvailable())
                    {
                        $(e.relatedTarget).parent().find("li."+key).addClass("disabled");
                    }                    
                }
                
            });
            
            
            $(".dropdown-menu").parent().on('hidden.bs.dropdown', function (e) {
                $(e.relatedTarget).parent().find("li").removeClass("disabled");
            });  
            
            $(".dropdown-menu li a").click(function(e){
                if($(this).parent().hasClass("disabled")){
                   e.stopPropagation();
                   return;
                }
                // also need to find new settings for a selection
                var currentElement = $(this).parent().parent().parent().find('.dropdown-toggle').find('span.label_name');
                var oldFKey = currentElement.html();
                var newFKey = $(this).text();               
                var KBRow = $(this).closest("div.KBRow");           
                var currentSettings = USED_FKEY;
                if(UltimateUI.ButtonKeys.hasOwnProperty(KBRow.data("uts")))             
                      currentSettings  = UltimateUI.ButtonKeys[KBRow.data("uts")].used;

            
                var  newSettings = currentSettings;
                if(newFKey != NO_SHORTCUT)
                {
                    var possibleSeetings = UltimateUI.DefaultKeyMapping[newFKey].getFreeSlots();

                    if (possibleSeetings.indexOf(currentSettings) == -1)
                    {
                        // ok, so current seting cannot be used, and we have to find another combination, that is usable
                        if(possibleSeetings.indexOf(USED_FKEY) > -1)
                            newSettings = USED_FKEY;
                        else if(possibleSeetings.indexOf(USED_CTRL) > -1)
                            newSettings = USED_CTRL;
                        else if(possibleSeetings.indexOf(USED_SHIFT) > -1)
                            newSettings = USED_SHIFT;
                        else if(possibleSeetings.indexOf(USED_SHIFTCTRL) > -1)
                            newSettings = USED_SHIFTCTRL;
                    }

                } else {
         
                    newSettings = USED_FKEY;
                }
                     
                if(newSettings != currentSettings)
                {
                    KBRow.find("input.ctrl").first().prop("checked", newSettings == USED_CTRL || newSettings == USED_SHIFTCTRL);
                    KBRow.find("input.shift").first().prop("checked", newSettings == USED_SHIFT || newSettings == USED_SHIFTCTRL);
                    
                }                
           
                currentElement.html(newFKey);
                UltimateUI.UpdateCurrentSettings($(this).closest("div.KBRow"), newFKey, oldFKey);
            });        
   

   
            $(".KBRow input").click(function(e){
                
                var domObject = $(this).closest("div.KBRow");
                
                var currentSettings = UltimateUI.GetRowConfig(domObject);
              
            
                var fKey = domObject.find("span.label_name").first().html();
                if(!UltimateUI.DefaultKeyMapping[fKey].canHave(currentSettings))
                {                 
                    // so at this point we know desired configration cannot be changed
                    // should check if by any chance there can be other configuration to serve the needs
                
                    var possible = UltimateUI.DefaultKeyMapping[fKey].getFreeSlots();
                    var isCtrl = $(this).hasClass("ctrl");
                    if(currentSettings.fKey)
                    {
                        // so if we turned off all buttons, but it doesnt wokr, we shoudl check if  turning on second will not work
                        if(isCtrl && possible.indexOf(USED_SHIFT) > -1)
                        {
                            // shift possible, enabling
                            domObject.find("input.shift").first().prop("checked", true);                           
                        } else if (!isCtrl && possible.indexOf(USED_CTRL) > -1) {
                            domObject.find("input.ctrl").first().prop("checked", true);                            
                        }
                    } else if(currentSettings.commandKey && isCtrl && possible.indexOf(USED_SHIFTCTRL) > -1) {
                        domObject.find("input.shift").first().prop("checked", true);
                    } else if(currentSettings.shiftKey && !isCtrl && possible.indexOf(USED_SHIFTCTRL) > -1) {
                        domObject.find("input.ctrl").first().prop("checked", true);
                    } else if(currentSettings.shiftKey && isCtrl && possible.indexOf(USED_FKEY) > -1) {
                        domObject.find("input.shift").first().prop("checked", false);
                    } else  if(currentSettings.commandKey && !isCtrl && possible.indexOf(USED_FKEY) > -1) {
                        domObject.find("input.ctrl").first().prop("checked", false);
                    } else {
                        $(this).prop("checked",!$(this).prop("checked"));
                    }
                }
                UltimateUI.UpdateCurrentSettings($(this).closest("div.KBRow"),fKey,fKey);
            });        
        },
        FillKeyMappingData : function(source, target, checkButton) {
       
            for(var key in source) {
                var cObj = source[key];
                var keyObj = undefined;
                if(!target.hasOwnProperty(key))
                {
                    keyObj = new ActionKbInfo();
                    target[key] = keyObj;    
                }
                
                if(cObj.hasOwnProperty("fKey") && (!checkButton || UltimateUI.Buttons.indexOf(cObj.fKey) > -1))
                {
                    target[key].fKey = true;
                }
                    
                if(cObj.hasOwnProperty("shiftKey") && (!checkButton || UltimateUI.Buttons.indexOf(cObj.shiftKey) > -1))
                {
                    target[key].shiftKey = true;
                }
                if(cObj.hasOwnProperty("commandKey") && (!checkButton || UltimateUI.Buttons.indexOf(cObj.commandKey) > -1))
                {
                    target[key].commandKey = true;
                }
                if(cObj.hasOwnProperty("commandShiftKey") && (!checkButton || UltimateUI.Buttons.indexOf(cObj.commandShiftKey) > -1))
                {
                    target[key].commandShiftKey = true;
                }
                
                
            }

            
            
        },
        GetRowConfig : function (domObject) {
           var actRowConfig = new ActionKbInfo();
           var ctrlKey = domObject.find("input.ctrl").first().prop('checked');
           var shiftKey = domObject.find("input.shift").first().prop('checked');
           if(ctrlKey && shiftKey)
               actRowConfig.commandShiftKey = true;
           else if (ctrlKey)
               actRowConfig.commandKey = true;
           else if (shiftKey)
               actRowConfig.shiftKey= true;
           else
               actRowConfig.fKey = true;
           
           return actRowConfig;
        },
        
        UpdateCurrentSettings : function(domObject, newFkey, oldFkey) {
            if(newFkey == NO_SHORTCUT && newFkey == oldFkey)
                return;
            // need to update cached settings, so when other elements are going to be changed, we changes will be taken into account
            var newSettings = UltimateUI.GetRowConfig(domObject);
            var texConfig = undefined;
            var texName = domObject.data("uts");
            if(UltimateUI.ButtonKeys.hasOwnProperty(texName))
                texConfig = UltimateUI.ButtonKeys[texName];
            
            if(UltimateUI.DefaultKeyMapping.hasOwnProperty(oldFkey) && texConfig != undefined)
            {
                var keyMappingData = UltimateUI.DefaultKeyMapping[oldFkey];
                switch(texConfig.used) {
                      case USED_CTRL:
                         keyMappingData.commandKey = false;
                          break;
                      case USED_SHIFTCTRL:
                         keyMappingData.commandShiftKey = false;                       
                          break;
                      case USED_SHIFT:
                         keyMappingData.shiftKey = false;                     
                          break;
                      default:
                         keyMappingData.fKey = false;                     
                  }
            }
         
            // old fkey info cleaned out, now can modify tex config
            if(newFkey == NO_SHORTCUT && (UltimateUI.ButtonKeys.hasOwnProperty(texName)))
            {
                // if new key is none, we have to delete the infomration from button list
                delete UltimateUI.ButtonKeys[texName];
                return;
            }
            
            // cleanup done, now, have to enter new config
            if(texConfig == undefined) {
                UltimateUI.ButtonKeys[texName] = {};
                texConfig = UltimateUI.ButtonKeys[texName];
            }
           
           if(!UltimateUI.DefaultKeyMapping.hasOwnProperty(newFkey)) {               
                var keyObj = new ActionKbInfo();
                UltimateUI.DefaultKeyMapping[newFkey] = keyObj;    
           }
           
            texConfig.fKey = newFkey;
            if(newSettings.commandShiftKey) {
               texConfig.used = USED_SHIFTCTRL;
                UltimateUI.DefaultKeyMapping[newFkey].commandShiftKey = true;
           }
            else if (newSettings.commandKey) {
               texConfig.used = USED_CTRL; 
               UltimateUI.DefaultKeyMapping[newFkey].commandKey = true;
           }
            else if (newSettings.shiftKey) {
                texConfig.used = USED_SHIFT;
                UltimateUI.DefaultKeyMapping[newFkey].shiftKey = true;
            }               
            else
            {
                texConfig.used = USED_FKEY;
                UltimateUI.DefaultKeyMapping[newFkey].fKey = true;
            }
 
        }
	
        
 
};