import importlib
import json
import os
import sys

from PySide6 import QtWidgets

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

from . import PmxReader
from . import PmxWriter
from . import Window

_window = None
_pyside_app = None


def show_window():
    global _window, _pyside_app
    try:
        importlib.reload(Window)
    except Exception:
        pass
    app = QtWidgets.QApplication.instance()
    if app is None:
        _pyside_app = QtWidgets.QApplication(sys.argv)
    if _window is not None:
        try:
            _window.raise_()
            _window.activateWindow()
            return
        except Exception:
            _window = None
    if hasattr(Window, "MainWindow"):
        w = Window.MainWindow()
        w.finished.connect(lambda _: _on_close())
        _window = w
        w.show()
        return
    try:
        import unreal
        unreal.log_error("PmxBulletPhysics.Window.MainWindow not found. Replace PmxBulletPhysics/Window.py and restart editor.")
    except Exception:
        raise AttributeError("PmxBulletPhysics.Window has no attribute 'MainWindow'")


def set_pmx(window, path):
    """PMXファイルを1パスで読み込み、全セクションのデータをウィンドウへ適用する。"""
    window._pmx_path = path
    window._lbl_file.setText(os.path.basename(path))
    try:
        info = PmxReader.read_pmx(path)
        window._lbl_model.setText(
            info.get("model_name") or os.path.splitext(os.path.basename(path))[0]
        )
        window._apply_rigid_bodies(info.get("bodies") or [])
        window._apply_joints(info.get("joints") or [])
        window._apply_ik(info.get("ik") or [])
        window._apply_grants(info.get("grants") or [])
    except Exception as e:
        try:
            import unreal
            unreal.log_warning(str(e))
        except Exception:
            pass
        window._clear_physics()
        window._apply_ik([])
        window._apply_grants([])
    update_import_enabled(window)


def pick_anim_bp(window):
    try:
        import unreal
        assets = unreal.EditorUtilityLibrary.get_selected_assets()
    except Exception:
        return
    for a in assets:
        try:
            if isinstance(a, unreal.AnimBlueprint):
                window._anim_bp = a
                window._le_abp.setText(a.get_path_name())
                update_import_enabled(window)
                return
        except Exception:
            continue


def reset_window(window):
    window._pmx_path = ""
    window._anim_bp = None
    window._lbl_file.setText("-")
    window._lbl_model.setText("-")
    window._le_abp.setText("")
    window._clear_physics()
    window._apply_ik([])
    window._apply_grants([])
    update_import_enabled(window)


def update_import_enabled(window):
    ok = bool(window._pmx_path) and window._anim_bp is not None
    window._btn_import_ik.setEnabled(ok)
    window._btn_import_bullet.setEnabled(ok)


def import_ik(window):
    import unreal
    anim_bp = unreal.load_asset(window._anim_bp.get_path_name())
    if anim_bp is None or not isinstance(anim_bp, unreal.AnimBlueprint):
        unreal.log_warning("[Pmx] AnimBPのロードに失敗しました。")
        return
    payload = json.dumps(window._ik_data, ensure_ascii=False)
    unreal.IK_LoaderImportLibrary.apply_pmx_ik_to_anim_blueprint(anim_bp, payload)
    PmxWriter.apply_grants_to_anim_blueprint(window._grant_data, anim_bp)
    unreal.EditorAssetLibrary.save_loaded_asset(anim_bp)
    unreal.log(f"[Pmx] IK完了: {len(window._ik_data)}件を適用しました。")


def import_physics(window):
    import unreal
    anim_bp = unreal.load_asset(window._anim_bp.get_path_name())
    if anim_bp is None or not isinstance(anim_bp, unreal.AnimBlueprint):
        unreal.log_warning("[Pmx] AnimBPのロードに失敗しました。")
        return
    if not PmxWriter.apply_physics_and_joints(window._rb_data, window._joint_data, anim_bp):
        unreal.log_warning("[Pmx] インポートに失敗しました。")
        return
    PmxWriter.apply_grants_to_anim_blueprint(window._grant_data, anim_bp)
    unreal.EditorAssetLibrary.save_loaded_asset(anim_bp)
    unreal.log(f"[Pmx] 完了: {len(window._rb_data)}件の剛体, {len(window._joint_data)}件のジョイントを適用しました。")


def _on_close():
    global _window
    _window = None
