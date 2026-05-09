# -*- coding: utf-8 -*-
import os
import sys

try:
    import unreal
except Exception:
    unreal = None

try:
    _THIS_DIR = os.path.abspath(os.path.dirname(__file__))
except Exception:
    _THIS_DIR = os.path.abspath(os.getcwd())

if _THIS_DIR and _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

_menu_registered = False


def _set_global_window_icon():
    try:
        from PySide6.QtGui import QIcon
        from PySide6.QtWidgets import QApplication
    except Exception:
        return
    try:
        icon_path = os.path.join(_THIS_DIR, "res", "Icon128.png")
        app = QApplication.instance()
        if app is not None and os.path.exists(icon_path):
            app.setWindowIcon(QIcon(icon_path))
    except Exception:
        return


def open_load_vmd():
    _set_global_window_icon()
    import importlib
    if _THIS_DIR and _THIS_DIR not in sys.path:
        sys.path.insert(0, _THIS_DIR)
    m = importlib.import_module("VMD.Operator")
    if hasattr(m, "show_window"):
        m.show_window()


def open_load_pmxbulletphysicsnode():
    _set_global_window_icon()
    import importlib
    if _THIS_DIR and _THIS_DIR not in sys.path:
        sys.path.insert(0, _THIS_DIR)
    m = importlib.import_module("PMX.Operator")
    if hasattr(m, "show_window"):
        m.show_window()


def open_load_vrma():
    _set_global_window_icon()
    import importlib
    if _THIS_DIR and _THIS_DIR not in sys.path:
        sys.path.insert(0, _THIS_DIR)
    m = importlib.import_module("VRMA.Operator")
    if hasattr(m, "show_window"):
        m.show_window()


def open_vrm_model():
    _set_global_window_icon()
    import importlib
    if _THIS_DIR and _THIS_DIR not in sys.path:
        sys.path.insert(0, _THIS_DIR)
    m = importlib.import_module("VRM.Operator")
    if hasattr(m, "show_window"):
        m.show_window()


def register_menu():
    global _menu_registered
    if _menu_registered or unreal is None:
        return
    menus = unreal.ToolMenus.get()
    main = menus.extend_menu("MainFrame.MainMenu")
    vp = main.add_sub_menu("MainFrame.MainMenu", "AMAP5", "AMAP5", "AMAP5")

    vp.add_section("Main", "")

    e4 = unreal.ToolMenuEntry(name="AMAP5.PmxModel", type=unreal.MultiBlockType.MENU_ENTRY)
    e4.set_label("PMX IK / Physics tool")
    e4.set_string_command(
        unreal.ToolMenuStringCommandType.PYTHON,
        "",
        "import importlib; m=importlib.import_module('init_unreal'); m.open_load_pmxbulletphysicsnode()",
    )

    e2 = unreal.ToolMenuEntry(name="AMAP5.VmdAnimation", type=unreal.MultiBlockType.MENU_ENTRY)
    e2.set_label("VMD Animation tool")
    e2.set_string_command(
        unreal.ToolMenuStringCommandType.PYTHON,
        "",
        "import importlib; m=importlib.import_module('init_unreal'); m.open_load_vmd()",
    )

    sep = unreal.ToolMenuEntry(name="AMAP5.Separator", type=unreal.MultiBlockType.SEPARATOR)

    e5 = unreal.ToolMenuEntry(name="AMAP5.VrmModel", type=unreal.MultiBlockType.MENU_ENTRY)
    e5.set_label("VRM IK tool")
    e5.set_string_command(
        unreal.ToolMenuStringCommandType.PYTHON,
        "",
        "import importlib; m=importlib.import_module('init_unreal'); m.open_vrm_model()",
    )

    e6 = unreal.ToolMenuEntry(name="AMAP5.VrmaAnimation", type=unreal.MultiBlockType.MENU_ENTRY)
    e6.set_label("VRMA Animation tool")
    e6.set_string_command(
        unreal.ToolMenuStringCommandType.PYTHON,
        "",
        "import importlib; m=importlib.import_module('init_unreal'); m.open_load_vrma()",
    )

    vp.add_menu_entry("Main", e4)
    vp.add_menu_entry("Main", e2)
    vp.add_menu_entry("Main", sep)
    vp.add_menu_entry("Main", e5)
    vp.add_menu_entry("Main", e6)

    menus.refresh_all_widgets()
    _menu_registered = True


if unreal is not None:
    try:
        register_menu()
    except Exception:
        pass
