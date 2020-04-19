import ConfigParser
from waflib.Configure import conf
import waflib.Logs as waf_logs
from time import time, sleep
from Tkinter import *
import Tkinter as tk
import ttk
import tkSimpleDialog
import tkMessageBox
import threading
import logging

###############################################################################
def _verify_option(waf_ctx, category_name, option_name, new_value):
	return waf_ctx.verify_settings_option(option_name, str(new_value))

###############################################################################
def _save_option(waf_ctx, category_name, option_name, new_value):		
	new_value = str(new_value)
	section_name = str(category_name)
	waf_ctx.set_settings_option(section_name, option_name, new_value)
				
###############################################################################
def _create_option_widget(waf_ctx, category_name, option_name, value, default_description, default_value, target_content_area, fn_on_value_changed = None):
	(hint_list, hint_value_list, hint_desc_list, mode) = waf_ctx.hint_settings_option(category_name, option_name, "")
	if hint_list:
		if mode == "multi":
			return UiOption_CheckBoxList(waf_ctx, category_name, option_name, value, default_description, default_value, hint_list, hint_value_list, hint_desc_list, target_content_area, fn_on_value_changed)		
		elif mode == "single":
			return UiOption_RadioButtonList(waf_ctx, category_name, option_name, value, default_description, default_value, hint_list, hint_value_list, hint_desc_list, target_content_area, fn_on_value_changed)
		elif mode == "boolean":
			return UiOption_CheckBox(waf_ctx, category_name, option_name, hint_value_list[0], default_description, default_value, target_content_area, fn_on_value_changed)
	elif value == 'False' or value == 'True':
			return UiOption_CheckBox(waf_ctx, category_name, option_name, value             , default_description, default_value, target_content_area, fn_on_value_changed)
	elif value.isdigit():
		return UiOption_SpinBox(waf_ctx, category_name, option_name, value, default_description, default_value, target_content_area, fn_on_value_changed)
	else:
		return UiOption_EntryBox(waf_ctx, category_name, option_name, value, default_description, default_value, target_content_area, fn_on_value_changed)	
	
###############################################################################		
class ForceValidationDialog(tkSimpleDialog.Dialog):
	def __init__(self, waf_ctx, parent_window, category_name, option_name, value, default_description, default_value, fn_on_value_changed = None):
		self.waf_ctx = waf_ctx
		self.category_name = category_name
		self.option_name = option_name.strip()
		self.value = value
		self.default_value = default_value
		self.default_description = default_description
		self.option_widget = None
		self.fn_on_value_changed = fn_on_value_changed
		tkSimpleDialog.Dialog.__init__(self, parent_window) # No super because calls is python old style class type
				
	def body(self, content_area):		
		self.option_widget = _create_option_widget(self.waf_ctx, self.category_name, self.option_name, self.value, self.default_description, self.default_value, content_area, self.fn_on_value_changed)
		if self.default_value:
			lbl = tk.Label(content_area, text="Default Value: %s" % self.default_value, font = "-slant italic -size 10")
			lbl.pack(fill=tk.X)
			
		self.wm_attributes("-topmost", 1)
		self.focus_force()	
		
	def validate(self):
		return self.option_widget.revalidate()
			
	def apply(self):
		self.result = self.option_widget.get_value()
		
###############################################################################
class UiOption_Base(object):
	
	def __init__(self, waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed=None):
		self.waf_ctx = waf_ctx
		self.category_name = category_name
		self.option_name = option_name
		self.prev_valid_value = value
		self.description = description
		self.default_value = default_value
		self.input_widget  = None
		self.error_text = StringVar()		
		self.fn_on_value_changed = fn_on_value_changed
		
		self.content_area = ttk.Frame(target_content_area)
		self.content_area.pack(fill=tk.BOTH)
		
		info_frame = ttk.Frame(self.content_area)
		info_frame.pack(fill=tk.X)
		
		# Create option widget
		self.create_input_widget(info_frame)
						
		lbl_title = tk.Label(info_frame, text=option_name, anchor=tk.W, font = "-weight bold -size 11")
		lbl_title.pack(fill=tk.BOTH)		
		
		if description:
			lbl_desc = tk.Label(info_frame, text=description, anchor=tk.W, justify = tk.LEFT, font = "-slant italic -size 10")
			lbl_desc.pack(fill=tk.X, padx=10)
		
		self.lbl_error = tk.Label(self.content_area, textvariable=self.error_text, anchor=tk.W, justify = tk.LEFT, fg="red", font = "-size 10")
		self.lbl_error.pack(fill=tk.X)
		self.lbl_error.pack_forget()
		
		ttk.Separator(self.content_area, orient=HORIZONTAL).pack(side=tk.BOTTOM, fill=tk.BOTH)
		
		# Call verification function once to ensure value is valid (except it is empty)
		# we won't write to the config as the prev_value == value
		if value:
			self.revalidate()
		
	def get_value(self):
		return None
		
	def set_value(self, new_value):
		pass
		
	def set_default_value(self):
		self.set_value(self.default_value)
		
	def create_input_widget(self, target_content_area):
		pass
		
	def display_input_dialog(self):
		pass
		
	def handle_value_changed(self, value):	
		# Validate value
		new_value = str(value)
		(valValid, warning, error) = _verify_option(self.waf_ctx, self.category_name, self.option_name, new_value)
		
		# Set error indicator
		if not warning and not error:
			self.error_text.set("")
			self.lbl_error.pack_forget()
		elif warning and error:
			self.lbl_error.config(fg="red")
			self.error_text.set(warning + '\n' + error)
			self.lbl_error.pack(fill=tk.X)
		elif error:
			self.lbl_error.config(fg="red")
			self.error_text.set(error)
			self.lbl_error.pack(fill=tk.X)
		elif warning:
			self.lbl_error.config(fg="orange")
			self.error_text.set(warning)
			self.lbl_error.pack(fill=tk.X)
		
		prev_value_backup = self.prev_valid_value
		# Write to file and internal waf options system
		save_option = valValid == True and not warning and not error
		if save_option and self.prev_valid_value != new_value:
			_save_option(self.waf_ctx, self.category_name, self.option_name, new_value)
			self.prev_valid_value = new_value
		
		if self.fn_on_value_changed:
			self.fn_on_value_changed(self.category_name, self.option_name, valValid, new_value)
		
		return valValid
		
	def revalidate(self, optional_value = None):
		if optional_value:
			return self.handle_value_changed(optional_value)
		else:
			return self.handle_value_changed(self.get_value())
		
	def destroy(self):
		self.content_area.destroy()
		
	def set_focus_out_event(self, state):
		pass
		
###############################################################################
class UiOption_CheckBox(UiOption_Base):
	def __init__(self, waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed=None):
		self.ttk_checkbox_value = StringVar()
		self.ttk_checkbox_value.set(value)
		super(UiOption_CheckBox, self).__init__(waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed)
		
	def create_input_widget(self, target_content_area):
		self.input_widget = ttk.Checkbutton(target_content_area, variable=self.ttk_checkbox_value, command=self.on_clicked, onvalue="True", offvalue="False")
		self.input_widget.bind("<ButtonRelease-3>", lambda event: self.set_default_value()) # bind right-click -> default value
		self.input_widget.pack(side=tk.RIGHT)
		
	def get_value(self):
		return self.ttk_checkbox_value.get()
		
	def set_value(self, new_value):
		value = str(new_value)
		(valValid, warning, error) = _verify_option(self.waf_ctx, self.category_name, self.option_name, new_value)
		
		if not valValid:
			return
			
		self.ttk_checkbox_value.set(value)
		self.on_clicked()
		
	def on_clicked(self):
		if not self.revalidate():
			self.ttk_checkbox_value.set(self.prev_valid_value)
		
###############################################################################
class UiOption_CheckBoxList(UiOption_Base):
	def __init__(self, waf_ctx, category_name, option_name, value, description, default_value, item_names, item_values, item_descriptions, target_content_area, fn_on_value_changed=None):
		self.item_names = item_names	
		self.item_values = item_values
		self.item_descriptions = item_descriptions			
		self.checkbox_value_list = {}
		super(UiOption_CheckBoxList, self).__init__(waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed)		
		
	def create_input_widget(self, target_content_area):	
		self.options_container = ttk.Labelframe(target_content_area)
		self.options_container.pack(side=tk.BOTTOM, fill=tk.X, padx=10)
		
		value_list = self.prev_valid_value.split(',')
		for index, option_name in enumerate(self.item_names):
			ttk_checkbox_value = StringVar()
			
			for value in value_list:
				if value == option_name:
					ttk_checkbox_value.set(option_name)
					break

			frame = ttk.Frame(self.options_container)
			frame.pack(fill=tk.X)
		
			checkbox = ttk.Checkbutton(frame, variable=ttk_checkbox_value, onvalue=self.item_values[index], offvalue="")
			checkbox.config(command=self.on_clicked)
			checkbox.bind("<ButtonRelease-3>", lambda event: self.set_default_value()) # bind right-click -> default value
			checkbox.pack(side=LEFT)
			
			lbl_checkbox = tk.Label(frame, text=option_name, font = "-weight bold -size 10")
			lbl_checkbox.pack(side=LEFT)
			
			if self.item_descriptions[index]:
				lbl_checkbox_desc = tk.Label(frame, text="(%s)" % self.item_descriptions[index], font = "-slant italic -size 9")
				lbl_checkbox_desc.pack(side=LEFT)
		
			self.checkbox_value_list[option_name] = ttk_checkbox_value
				
	def get_value(self):
		values = []
		for (key, value) in self.checkbox_value_list.iteritems():
			checkbox_value = value.get()
			if checkbox_value:
				values.append(checkbox_value)
		return ','.join(values)
		
	def set_value(self, new_value):	
		value = str(new_value)
		(valValid, warning, error) = _verify_option(self.waf_ctx, self.category_name, self.option_name, new_value)
		
		if not valValid:
			return
			
		values = new_value.split(',')		
		for (key, value) in self.checkbox_value_list.iteritems():
			if key in values:
				value = 'True'
			else:
				value = 'False'
		
	def on_clicked(self):
		if not self.revalidate():
			self.set_value(self.prev_valid_value)
			
###############################################################################
class UiOption_RadioButtonList(UiOption_Base):
	def __init__(self, waf_ctx, category_name, option_name, value, description, default_value, item_names, item_values, item_descriptions, target_content_area, fn_on_value_changed=None):
		self.item_names = item_names	
		self.item_values = item_values
		self.item_descriptions = item_descriptions			
		self.ttk_radio_button_value = StringVar()
		self.ttk_radio_button_value.set(value)
		
		super(UiOption_RadioButtonList, self).__init__(waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed)		
		
	def create_input_widget(self, target_content_area):	
		self.options_container = ttk.Labelframe(target_content_area)
		self.options_container.pack(side=tk.BOTTOM, fill=tk.X, padx=10)
		
		for index, option_name in enumerate(self.item_names):	
			frame = ttk.Frame(self.options_container)
			frame.pack(fill=tk.X)
			radiobutton = ttk.Radiobutton(frame, text=option_name, variable=self.ttk_radio_button_value, command=self.on_clicked, value=self.item_values[index])
			radiobutton.bind("<ButtonRelease-3>", lambda event: self.set_default_value()) # bind right-click -> default value
			radiobutton.pack(side=LEFT)
		
			try:
				if self.item_descriptions[index]:
					lbl_checkbox_desc = tk.Label(frame, text="(%s)" % self.item_descriptions[index], font = "-slant italic -size 9")
					lbl_checkbox_desc.pack(side=LEFT)
			except:
				pass
						
	def get_value(self):
		return self.ttk_radio_button_value.get()
		
	def set_value(self, new_value):	
		value = str(new_value)
		(valValid, warning, error) = _verify_option(self.waf_ctx, self.category_name, self.option_name, new_value)
		
		if not valValid:
			return
			
		self.ttk_radio_button_value = new_value
		
	def on_clicked(self):
		if not self.revalidate():
			self.ttk_radio_button_value.set(self.prev_valid_value)
	
			
###############################################################################
class UiOption_SpinBox(UiOption_Base):
	def __init__(self, waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed=None):
		self.ttk_spinner_value = StringVar()
		self.ttk_spinner_value.set(value)
		super(UiOption_SpinBox, self).__init__(waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed)
		
	def create_input_widget(self, target_content_area):
		self.input_widget = tk.Spinbox(target_content_area, textvariable=self.ttk_spinner_value, command=self.on_clicked, from_=0, to=99999, width=6)
		self.input_widget.bind("<ButtonRelease-3>", lambda event: self.set_default_value()) # bind right-click -> default value
		self.input_widget.pack(side=tk.RIGHT)
		
	def get_value(self):
		return self.ttk_spinner_value.get()
		
	def set_value(self, new_value):	
		value = str(new_value)
		(valValid, warning, error) = _verify_option(self.waf_ctx, self.category_name, self.option_name, new_value)
		
		if not valValid:
			return
			
		self.ttk_spinner_value.set(new_value)
		self.on_clicked()
		
	def on_clicked(self):
		if not self.revalidate():
			self.ttk_spinner_value.set(self.prev_valid_value)

###############################################################################
class UiOption_EntryBox(UiOption_Base):
	def __init__(self, waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed=None):
		self.entry_var = StringVar()
		self.entry_var.set(value)
		self.prev_valid_value = ""
		super(UiOption_EntryBox, self).__init__(waf_ctx, category_name, option_name, value, description, default_value, target_content_area, fn_on_value_changed)
		
	def create_input_widget(self, target_content_area):		
		self.input_widget = ttk.Entry(target_content_area, textvariable=self.entry_var, width=45)
		self.input_widget.icursor(tk.END)
		self.input_widget.bind("<ButtonRelease-3>", lambda event: self.set_default_value()) # bind right-click -> default value
		self.input_widget.pack(side=tk.RIGHT)
		
		self.set_focus_out_event(True)
		self.input_widget.bind("<Return>",self.on_changed)

	def get_value(self):
		return self.entry_var.get()
		
	def set_value(self, new_value):	
		value = str(new_value)
		(valValid, warning, error) = _verify_option(self.waf_ctx, self.category_name, self.option_name, new_value)
		
		if not valValid:
			return
			
		self.entry_var.set(new_value)
		self.on_changed(None)
			
	def on_changed(self, event):
		self.revalidate()
		
	def set_focus_out_event(self, state):
		if state:
			self.input_widget.bind("<FocusOut>",self.on_focus_out)
		else:
			self.input_widget.unbind("<FocusOut>")
		
	def on_focus_out(self, event):
		if not self.prev_valid_value == self.get_value():
			if not self.revalidate():
				self.entry_var.set(self.prev_valid_value)
				self.revalidate()

###############################################################################
class UiCategory_Base(object):
	def __init__(self, category_name, parent):		
		self.parent = parent
		self.category_name = category_name
		self.options_list = []
		self.options_widgets = {}		
		self.waf_ctx = self.parent.waf_ctx
		
		# Get options for section
		if hasattr(self.waf_ctx, 'default_settings'):
			self.default_options = self.waf_ctx.default_settings.get(self.category_name, [])
			
		# Get all valid option in order from the default settings
		for items in self.default_options:
				attribute = items.get('attribute', None)
				if attribute:
					self.options_list.append(attribute)
		
		# Add new tab
		self.outer_frame = ttk.Frame(self.parent.tab_widget)
		self.outer_frame.pack(expand=tk.YES, fill= tk.BOTH)
		
		scroll_x = Scrollbar(self.outer_frame, orient=HORIZONTAL)
		scroll_y = Scrollbar(self.outer_frame, orient=VERTICAL)
		
		self.canvas = Canvas(self.outer_frame, bd=0, height=350, highlightthickness = 0,	xscrollcommand=scroll_x.set, yscrollcommand=scroll_y.set)
		scroll_x.config(command=self.canvas.xview)
		scroll_y.config(command=self.canvas.yview)
		
		self.content_area = Frame(self.canvas)
		self.canvas.create_window(0,0,window=self.content_area, anchor=NW)
		
		# Control mouse wheel on in/out focus
		self.outer_frame.bind("<FocusIn>",  lambda event, self=self: self.canvas.bind_all("<MouseWheel>", lambda event,  self=self: self._on_mousewheel(event)) )
		self.outer_frame.bind("<FocusOut>", lambda event, self=self: self.canvas.unbind_all("<MouseWheel>") )						
		
		scroll_y.pack(side=tk.RIGHT, fill=tk.Y)
		scroll_x.pack(side=tk.BOTTOM, fill=tk.X)
		self.canvas.pack(side=tk.LEFT, expand=tk.YES, fill=tk.BOTH)
		
		self.parent.tab_widget.add(self.outer_frame, text=category_name)
		self.parent.tab_widget.pack(anchor=W)
				
	def _on_mousewheel(self, event):
		if event.delta > 0:
			self.canvas.yview('scroll',  -1, 'units')
		elif event.delta < 0:
			self.canvas.yview('scroll', 1, 'units')		
		
	def create_option_widget(self, option_name, target_content_area):
		option_name = option_name.strip()
		
		# Ensure we are not adding the same option twice
		if self.options_widgets.get(option_name, None):
			return
		
		# Create options for category
		value = None
		default_value = ""
		default_description = ""
		if self.parent.config_parser.has_section(self.category_name) and self.parent.config_parser.has_option(self.category_name, option_name):
			value = self.parent.config_parser.get(self.category_name, option_name)				
			(default_value, default_description) = self.waf_ctx.get_default_settings(self.category_name, option_name)

		# Check if this option is new (new options use default value
		is_new_option = False
		new_option_section = self.parent.new_options.get(self.category_name, None)
		if new_option_section:
			if option_name in new_option_section:
				is_new_option = True
			
		# Before creating the widget, validate that it is valid
		(valValid, warning, error) = _verify_option(self.waf_ctx, self.category_name, option_name, value)
		
		# Spawn input dialog to inform user
		if not valValid or is_new_option:	
			d = ForceValidationDialog(self.waf_ctx, self.content_area, self.category_name, option_name, value, default_description, default_value, None)
			if d.result:					
				value = d.result
					
		self.options_widgets[option_name] = _create_option_widget(self.waf_ctx, self.category_name, option_name, value, default_description, default_value, target_content_area, self.resolve_option_dependencies)
		
		# Update to re-calculate size and set scroll area
		self.content_area.update_idletasks()		
		self.canvas.configure(scrollregion=(0, 0, self.content_area.winfo_width(), self.content_area.winfo_height()))
					
	def resolve_option_dependencies(self, category_name, option_name, is_valid, value):
		pass
		
	def validate_category(self):
		return (True, "")
		
	def focus(self):
		self.parent.tab_widget.select(self.outer_frame)		
		
###############################################################################
class UiCategory(UiCategory_Base):
	def __init__(self, category_name, parent):
		super(UiCategory, self).__init__(category_name, parent)
				
		# Create widgets
		for option_name in self.options_list:
			self.create_option_widget(option_name, self.content_area)			

################################################################################
class AnsiColorConsole(tk.Text):

	# Ansi color -> TkInter colors
	ansi_color_to_tk = {
						'30' : 'Black',
						'31' : 'Red',
						'32' : 'Green',
						'33' : 'Yellow',
						'34' : 'Blue',
						'35' : 'Purple',
						'36' : 'Cyan',
						'37' : 'White'
					}

	# regexes to filter out ansi color codes
	color_pat = re.compile('\x01?\x1b\[([\d+;]*?)m\x02?')
	inner_color_pat = re.compile("^(\d+;?)+$")
		
	def __init__(self, parent):
		Text.__init__(self, parent, bg='Black', font='Helvetica 10 bold', wrap="none", state='disabled')
		self.known_tags = set([])
		self.register_tag("37", "White")
		self.reset_to_default_attribs()
		
	def reset_to_default_attribs(self):
		self.tag = '37'
		self.foregroundcolor = 'White'
		self.backgroundcolor = 'Black'
		
	def register_tag(self, txt, foreground):
		self.tag_config(txt, foreground=foreground)
		self.known_tags.add(txt)	
	
	def clear(self):
		self.configure(state='normal')
		self.delete("1.0", tk.END)
		self.configure(state='disabled')
	
	def write(self, text):
		# Split text to color codes i.e. "0;23" and strip out stuff such as "<ESC>", "\", "[" 
		segments = AnsiColorConsole.color_pat.split(text)
		if segments:
			for text in segments:
				# Check for color pattern or regular text
				if AnsiColorConsole.inner_color_pat.match(text):
					# Check if tag already registered
					if text not in self.known_tags:
						# Add tag
						parts = text.split(";")
						for part in parts:
							if part in AnsiColorConsole.ansi_color_to_tk:
								self.foregroundcolor = AnsiColorConsole.ansi_color_to_tk[part]
							else:
								for ch in part:
									if ch == '0' :
										self.reset_to_default_attribs()
		
						self.register_tag(text,	foreground=self.foregroundcolor)
						
					# Set active tag
					self.tag = text
				elif text == '':
					self.tag = '37'
				else:
					# No color pattern, hence insert text with active tag
					self.configure(state='normal')
					self.insert(END,text,self.tag)
					self.configure(state='disabled')
				
################################################################################
class LogHandler():
	#########################
	class SysLogHandler(waf_logs.IWafLogHandler):							
		def __init__(self, parent):
			self.parent = parent		
			super(LogHandler.SysLogHandler, self).__init__()
							
		def write_msg(self, msg):
			self.parent.write_to_console(msg)
			
		def write_err(self,msg):			
			self.parent.write_to_console(msg)
		
	#########################
	class WafLogHandler(logging.Handler):
		def __init__(self, parent):
			self.parent = parent
			super(LogHandler.WafLogHandler, self).__init__()
		
		def emit(self, record):
			msg = self.format(record)				
			self.parent.write_to_console('%s%s' % (msg, '\n'))
			
	#########################
	def __init__(self, parent):
		self.parent = parent
		self.sys_log_handler = None
		self.waf_log_handler = None
		
	def attach(self):
		return
		# Attach handlers
		if not self.sys_log_handler:
			self.sys_log_handler = self.SysLogHandler(self.parent)
			waf_logs.set_external_log_handler("options_log", self.sys_log_handler)		
		
		if not self.waf_log_handler:
			self.waf_log_handler = self.WafLogHandler(self.parent)
			self.waf_log_handler.setFormatter(waf_logs.formatter())
			log = logging.getLogger('waflib')
			log.addHandler(self.waf_log_handler)
		
	def detach(self):
		return
		# Remove handlers
		if self.sys_log_handler:
			waf_logs.remove_external_log_handler("options_log")
			self.sys_log_handler = None
			
		if self.waf_log_handler:
			log = logging.getLogger('waflib')
			log.removeHandler(self.waf_log_handler)
			self.waf_log_handler = None		

################################################################################
class IGuiTask(object):
	def __init__(self):
		self.is_active = False
	
	def is_task_active(self):
		return self.is_active
		
	def create_gui(self,  root, parent, content_area):
		self.is_active = True
		
	def destroy_gui(self):
		self.is_active = False
		
	def get_result(self):
		pass
		
	def cancel_task(self):
		return True
	
################################################################################
class GuiTask_GetOption(IGuiTask):	
	def __init__(self, ctx, section_name, option_name, value, decription_override):
		self.ctx = ctx
		self.section_name = section_name
		self.option_name = option_name
		self.value = value
		self.decription_override = decription_override		
		super(GuiTask_GetOption, self).__init__()
				
	def create_gui(self, root, parent, content_area):
		super(GuiTask_GetOption, self).create_gui(root, parent, content_area)
		self.root = root
		self.parent = parent
		
		self.inner_topframe = ttk.Frame(content_area)
		self.inner_topframe.pack(fill=tk.BOTH, expand=TRUE)
		
		# Option widget
		(default_value, default_description) = self.ctx.get_default_settings(self.section_name, self.option_name)
		decription = self.decription_override if self.decription_override else default_description
		self.option_widget = _create_option_widget(self.ctx, self.section_name, self.option_name, self.value, decription, default_value, self.inner_topframe, None)
		self.option_widget.set_focus_out_event(False)
		
		# Button box		
		box = tk.Frame(self.inner_topframe)
		w = ttk.Button(box, text="Confirm", width=10, command=self.ok, default=ACTIVE)
		w.pack(side=LEFT, padx=5, pady=5)		
		
		w = ttk.Button(box, text="Exit", width=10, command=self.parent.cancel)
		w.pack(side=LEFT, padx=5, pady=5)
		
		# Bind "Enter" and "Escape" key
		self.root.bind("<Return>", self.ok)
		self.root.bind("<Escape>", self.parent.cancel)   
    
		box.pack()
		
	def destroy_gui(self):	
		# Destroy widget
		self.inner_topframe.destroy()
		
		# Unbind "Enter" and "Escape" key
		self.root.unbind("<Return>")
		self.root.unbind("<Escape>")
		
		super(GuiTask_GetOption, self).destroy_gui()
				
	def get_result(self):
		return self.option_widget.get_value()

	def cancel_task(self):
		if not tkMessageBox.askyesno('Exit WAF', 'All changes will be lost.\n\nReally quit?'):
			return False
			
		self.parent.signal_task_finished(self)
		return True
		
	def ok(self, event=None):
		# Validate choice
		if not self.option_widget.revalidate():
			tkMessageBox.showinfo('Invalid selected option', 'Current selected option is invalid. \n\n Please "change" option to a valid state in order to procced.')
			return # Choice not valid ... do not allow to proceed 
	
		self.parent.signal_task_finished(self)

################################################################################
class GuiTask_AskYesNo(IGuiTask):
	def __init__(self, question):
		self.question = question
		self.result = None
		super(GuiTask_AskYesNo, self).__init__()
				
	def create_gui(self, root, parent, content_area):
		super(GuiTask_AskYesNo, self).create_gui(root, parent, content_area)
		self.parent = parent
		
		self.result = tkMessageBox.askyesno('', self.question)
		self.parent.signal_task_finished(self)		
		
	def destroy_gui(self):
		super(GuiTask_AskYesNo, self).destroy_gui()
		
	def get_result(self):
		return self.result
		
###############################################################################
class GuiTask_ModifyWafConfig(IGuiTask):
	
	def __init__(self, waf_ctx, config_parser):
		self.waf_ctx = waf_ctx
		self.config_parser = config_parser
		self.categories = {}
		self.new_options = {}
		super(GuiTask_ModifyWafConfig, self).__init__()
		
	def create_gui(self, root, parent, content_area):
		super(GuiTask_ModifyWafConfig, self).create_gui(root, parent, content_area)
		self.root = root
		self.root.resizable(1, 1)
		self.parent = parent
				
		# Create inner frame
		self.content_area = ttk.Frame(content_area, padding="3 3 5 5", borderwidth=3, relief="sunken")
		self.content_area.pack(fill=tk.BOTH, expand=tk.Y)
		
		# Button box			
		box = tk.Frame(self.content_area)
		
		w = tk.Button(box, text="Exit", width=13, command=lambda self=self: self.cancel_task())
		w.pack(side=LEFT, padx=5, pady=5)		    
		
		# Create tab widget
		self.inner_frame = ttk.Frame(self.content_area)
		self.tab_widget = ttk.Notebook(self.inner_frame)
		self.tab_widget.pack(fill=tk.BOTH, expand=tk.Y, padx=5, pady=5)
		
		# Load WAF Options categories 
		self.read_waf_option_config()
		
		self.inner_frame.pack(fill=tk.BOTH, expand=tk.Y)
		box.pack()
		
	def cancel_task(self):
		if self.config_parser and tkMessageBox.askyesno('Exit WAF', 'Save changes?'):			
			(retVal, error) = self.validate_categories()
			if retVal:
				self.waf_ctx.save_user_settings(self.config_parser)
			else:
				if not tkMessageBox.askyesno('Error on save WAF', 'Unable to save.\n\n%s\n\nExit without saving?' % error):
					return False
			

		self.parent.signal_task_finished(self)
		return True
						
	def destroy_gui(self):
		super(GuiTask_ModifyWafConfig, self).destroy_gui()
		
	def get_result(self):
		return None
		
	def validate_categories(self):
		for key, value in self.categories.iteritems():
			(retVal, error) = value.validate_category()
			if not retVal:
				return False, error
			
		return True, ""
		
	def read_waf_option_config(self):		
		# Loop over all sections
		for section in self.waf_ctx.default_settings:
			self.categories[section] =  UiCategory(section, self)
			
			# Force project selection to be tab in focus
			if(section == "Visual Studio Project Generator"):
				self.categories[section].focus()
		
################################################################################
class GuiTask_WafMenu(IGuiTask):
	def __init__(self, menu_desc):
		self.result = None
		self.menu_desc = menu_desc
		
		super(GuiTask_WafMenu, self).__init__()
				
	def create_gui(self, root, parent, content_area):
		super(GuiTask_WafMenu, self).create_gui(root, parent, content_area)
		self.parent = parent
		self.root = root
				
		# Create buttons
		for menu_desc in self.menu_desc:
			group = LabelFrame(content_area, text=menu_desc[0], padx=5, pady=5)
			group.pack(padx=10, pady=10)		
			
			for menu_item in menu_desc[1]:
				b = tk.Button(group, text=menu_item[0], width=25, command = lambda value=menu_item[1]:self.on_button_click(value))
				b.pack()
		
	def destroy_gui(self):
		super(GuiTask_WafMenu, self).destroy_gui()
		
	def get_result(self):
		return self.result
		
	def on_button_click(self, value):
		self.result = value
		self.parent.signal_task_finished(self)		
		
################################################################################
class ThreadedGuiMenu(threading.Thread):	
					
	def __init__(self, run_on_seperate_thread = False):
		self.log_handler = LogHandler(self)
		self.inner_topframe = None
		self.inner_bottomframe = None
		self.msg_queue = []
		self.queue_wlock = threading.Lock()
		self.active_task = None	
		self.result = None
		self.gui_up = False
		self.console = None
		self.console_visible = False
		self.alive = True
		self.run_on_seperate_thread = run_on_seperate_thread
		threading.Thread.__init__(self)
		
		# Start gui thread
		if self.run_on_seperate_thread: 
			self.start()
		
			# Wait for gui to start-up
			while not self.gui_up:
				pass
		
	def destroy(self):
		self.alive = False
		
		# Wait for gui to shut-down
		while self.gui_up:
			pass
		
	def center_window(self):
		self.root.withdraw()
		self.root.update_idletasks()

		x = (self.root.winfo_screenwidth() - self.root.winfo_reqwidth()) / 2
		y = (self.root.winfo_screenheight() - self.root.winfo_reqheight()) / 2
		self.root.geometry("+%d+%d" % (x, y))

		self.root.deiconify()
		
	def run(self):
		# Create widgets on thread
		self.root = tk.Tk()
		self.root.withdraw() # immediately withdraw to avoid user seeing window being repositioned
		
		self.root.title("CryWaf")
		#self.root.wm_attributes("-topmost", 1)		
		self.root.update()				
		self.center_window()
		self.root.focus_force()	
		
		# Hook close window event
		self.root.protocol("WM_DELETE_WINDOW", self.cancel)
		
		# Layout
		self.topframe = ttk.Frame(self.root)
		self.topframe.pack(side=TOP, anchor=tk.NW, fill=tk.BOTH, expand=TRUE)
									
		# Add handlers		
		self.log_handler.attach()
		self.root.wait_visibility()
		self.gui_up = True
		
		# Run mainloop
		self.alive = True
		self.root.after(50, self.check_for_task) # periodically check for new task
		self.root.mainloop()
		
		self.gui_up = False
		self.root = None
		
	def create_console(self):
		self.bottomframe = ttk.Frame(self.root, padding="3 3 3 3", borderwidth=2, relief="sunken")
		self.bottomframe.pack(side=BOTTOM, anchor=tk.NW, fill=tk.BOTH, expand=TRUE)
		
		self.console = AnsiColorConsole(self.bottomframe)
		
		# Attach scroll bars
		scroll_x = tk.Scrollbar(self.bottomframe, command=self.console.xview, orient=HORIZONTAL)
		self.console.configure(xscrollcommand=scroll_x.set)
		scroll_x.pack(side=BOTTOM, fill=X, expand=FALSE)
		
		scroll_y = tk.Scrollbar(self.bottomframe, command=self.console.yview, orient=VERTICAL)
		self.console.configure(yscrollcommand=scroll_y.set)
		scroll_y.pack(side=RIGHT, fill=Y, expand=FALSE)
				
		self.console.pack(side=TOP, anchor=tk.NW, fill=tk.BOTH, expand=TRUE)
		
	def hide_console(self):
		self.bottomframe.pack_forget()
		console_visible = False
		
	def show_console(self):
		self.bottomframe.pack(side=BOTTOM, anchor=tk.NW, fill=tk.BOTH, expand=TRUE)
		console_visible = True
		
	def check_for_task(self):
		# Kill window
		if not self.alive:
			if self.active_task:
				self.signal_task_finished(self.active_task)
			self.log_handler.detach()
			self.root.destroy()
			self.root = None
			return
			
		if self.active_task and not self.active_task.is_task_active():
			self.active_task.create_gui(self.root, self, self.topframe)

		if self.msg_queue:			
		
			# Create console if it does not exist yet
			if not self.console:
				self.create_console()
				
			if not self.console_visible:
				self.show_console()
			
			# Write to console
			self.queue_wlock.acquire()
			for msg in self.msg_queue:
				if not 'please press any key to continue' in msg.lower():
					self.console.write(msg)
				else:
					tkMessageBox.showerror("Error", "WAF encountered an internal error.\n\nWAF GUI is closing.")		
					self.cancel()
				
			self.msg_queue = []			
			self.queue_wlock.release()
		
		# Requeue event
		self.root.after(50, self.check_for_task)
			
	def write_to_console(self, msg):			
		self.queue_wlock.acquire()
		self.msg_queue.append(msg)
		self.queue_wlock.release()
		
	def signal_task_finished(self, task):
		value = task.get_result()
		task.destroy_gui()
		self.active_task = None
		self.result  = value
		
		if not self.run_on_seperate_thread:
			self.alive = False
						
	def wait_for_task(self):	
		if self.run_on_seperate_thread:
			# Wait for task to finish
			while self.result == None:
				if self.root == None:
					sys.exit(1)
		else:
			self.run()
							
	def cancel(self, event=None):	
		if self.active_task:
			if not self.active_task.cancel_task():
				return
			
		# Exit application
		self.root.destroy()
		self.root = None
		
		if not self.run_on_seperate_thread:
			sys.exit(1)
		
	def get_attribute(self, ctx, section_name, option_name, value, decription_override = None):
		self.result = None
		self.active_task = GuiTask_GetOption(ctx, section_name, option_name, value, decription_override)
		self.wait_for_task() # Blocking call
		return self.result
		
	def get_choice(self, question):
		self.result = None
		self.active_task = GuiTask_AskYesNo(question)		
		self.wait_for_task() # Blocking call
		return self.result
		
	def get_waf_menu_choice(self, menu_desc):
		self.result = None
		self.active_task = GuiTask_WafMenu(menu_desc)		
		self.wait_for_task() # Blocking call
		return self.result
		
	def modify_user_options(self, ctx):
		# Ensure default settings have been loaded
		(config_parser, new_options) = ctx.load_user_settings()	
		
		self.result = None
		self.active_task = GuiTask_ModifyWafConfig(ctx, config_parser)
		self.wait_for_task() # Blocking call
		return self.result		
		
g_waf_gui = None
###############################################################################

def _gui_show(ctx):
	global g_waf_gui
	if not g_waf_gui:
		g_waf_gui = ThreadedGuiMenu()	
			
	if not hasattr(ctx, 'gui_console') or not ctx.gui_console:		
		ctx.gui_console = g_waf_gui

###############################################################################
@conf
def gui_get_attribute(self, section_name, option_name, value, decription_override = None):
	_gui_show(self)
	return self.gui_console.get_attribute(self, section_name, option_name, value)
	
###############################################################################
@conf
def gui_get_choice(self, question):
	_gui_show(self)
	return self.gui_console.get_choice(question)

###############################################################################
@conf
def gui_get_waf_menu_choice(self, menu_desc):
	_gui_show(self)
	return self.gui_console.get_waf_menu_choice(menu_desc)
	
###############################################################################
@conf	
def gui_modify_user_options(self):
	_gui_show(self)
	return self.gui_console.modify_user_options(self)
