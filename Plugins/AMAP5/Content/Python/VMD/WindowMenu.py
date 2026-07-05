import os
import sys
import unreal

_here = os.path.dirname(os.path.abspath(__file__))
if _here not in sys.path:
    sys.path.insert(0, _here)

def register():
    menus = unreal.ToolMenus.get()
    tools_menu = menus.extend_menu("LevelEditor.MainMenu.Tools")
    entry = unreal.ToolMenuEntry(name="VmdLoader.Open", type=unreal.MultiBlockType.MENU_ENTRY)
    entry.set_label("Vmd Loader")
    entry.set_tool_tip("Open Vmd Loader")
    entry.set_string_command(unreal.ToolMenuStringCommandType.PYTHON, "", "import Operator; Operator.show_window()")
    tools_menu.add_menu_entry("VmdLoader", entry)
    menus.refresh_all_widgets()
