# -*- coding: utf-8 -*-
import json
import os
import sys

try:
    _THIS_DIR = os.path.abspath(os.path.dirname(__file__))
    _PARENT_DIR = os.path.dirname(_THIS_DIR)
except Exception:
    _THIS_DIR = os.path.abspath(os.getcwd())
    _PARENT_DIR = _THIS_DIR

if _PARENT_DIR not in sys.path:
    sys.path.insert(0, _PARENT_DIR)

try:
    import unreal
    _HAS_UNREAL = True
except Exception:
    unreal = None
    _HAS_UNREAL = False

from PySide6 import QtCore, QtWidgets
from PMX import Style

_pyside_app = None
_window = None

_PMX_IK_PAYLOAD = json.dumps([
    {
        "bone_index": 1,
        "loop": 50,
        "angle": 57.2958,
        "ik_bone": "IK_L_Foot",
        "target_bone": "J_Bip_L_Foot",
        "links": [
            {
                "link_bone": "J_Bip_L_LowerLeg",
                "enable_lim_x": True,
                "enable_lim_y": False,
                "enable_lim_z": False,
                "min_lim": [-180.0, 0.0, 0.0],
                "max_lim": [-0.5, 0.0, 0.0],
            },
            {
                "link_bone": "J_Bip_L_UpperLeg",
                "enable_lim_x": False,
                "enable_lim_y": False,
                "enable_lim_z": False,
            },
        ],
    },
    {
        "bone_index": 2,
        "loop": 5,
        "angle": 57.2958,
        "ik_bone": "IK_L_ToeBase",
        "target_bone": "J_Bip_L_ToeBase",
        "links": [
            {
                "link_bone": "J_Bip_L_Foot",
                "enable_lim_x": False,
                "enable_lim_y": False,
                "enable_lim_z": False,
            },
        ],
    },
    {
        "bone_index": 3,
        "loop": 50,
        "angle": 57.2958,
        "ik_bone": "IK_R_Foot",
        "target_bone": "J_Bip_R_Foot",
        "links": [
            {
                "link_bone": "J_Bip_R_LowerLeg",
                "enable_lim_x": True,
                "enable_lim_y": False,
                "enable_lim_z": False,
                "min_lim": [-180.0, 0.0, 0.0],
                "max_lim": [-0.5, 0.0, 0.0],
            },
            {
                "link_bone": "J_Bip_R_UpperLeg",
                "enable_lim_x": False,
                "enable_lim_y": False,
                "enable_lim_z": False,
            },
        ],
    },
    {
        "bone_index": 4,
        "loop": 5,
        "angle": 57.2958,
        "ik_bone": "IK_R_ToeBase",
        "target_bone": "J_Bip_R_ToeBase",
        "links": [
            {
                "link_bone": "J_Bip_R_Foot",
                "enable_lim_x": False,
                "enable_lim_y": False,
                "enable_lim_z": False,
            },
        ],
    },
])


class VrmModelWindow(QtWidgets.QDialog):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("VRM IK tool")
        self.resize(500, 265)
        self.setMinimumSize(500, 265)

        flags = (
            QtCore.Qt.Window
            | QtCore.Qt.WindowStaysOnTopHint
            | QtCore.Qt.WindowMinimizeButtonHint
            | QtCore.Qt.WindowCloseButtonHint
        )
        self.setWindowFlags(flags)

        self._skeletal_mesh = None
        self._anim_blueprint = None

        root = QtWidgets.QVBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(10)

        gb = QtWidgets.QGroupBox("Options")
        form_layout = QtWidgets.QVBoxLayout(gb)
        form_layout.setContentsMargins(10, 10, 10, 10)
        form_layout.setSpacing(8)

        mesh_row = QtWidgets.QHBoxLayout()
        mesh_row.setSpacing(8)
        mesh_label = QtWidgets.QLabel("SKM")
        mesh_label.setMinimumWidth(60)
        self._le_mesh = QtWidgets.QLineEdit()
        self._le_mesh.setReadOnly(True)
        btn_pick = QtWidgets.QPushButton("選択状態を取得")
        btn_pick.clicked.connect(self._on_pick_mesh)
        mesh_row.addWidget(mesh_label)
        mesh_row.addWidget(self._le_mesh, 1)
        mesh_row.addWidget(btn_pick)
        form_layout.addLayout(mesh_row)

        anim_bp_row = QtWidgets.QHBoxLayout()
        anim_bp_row.setSpacing(8)
        anim_bp_label = QtWidgets.QLabel("ABP")
        anim_bp_label.setMinimumWidth(60)
        self._le_anim_bp = QtWidgets.QLineEdit()
        self._le_anim_bp.setReadOnly(True)
        btn_pick_anim_bp = QtWidgets.QPushButton("選択状態を取得")
        btn_pick_anim_bp.clicked.connect(self._on_pick_anim_bp)
        anim_bp_row.addWidget(anim_bp_label)
        anim_bp_row.addWidget(self._le_anim_bp, 1)
        anim_bp_row.addWidget(btn_pick_anim_bp)
        form_layout.addLayout(anim_bp_row)

        root.addWidget(gb)

        btn_row = QtWidgets.QHBoxLayout()
        btn_row.setSpacing(8)
        self._btn_reset = QtWidgets.QPushButton("リセット")
        self._btn_reset.clicked.connect(self._on_reset)
        self._btn_add = QtWidgets.QPushButton("IK（Add Bone）")
        self._btn_add.setEnabled(False)
        self._btn_add.clicked.connect(self._on_add_bones)
        self._btn_send_ik = QtWidgets.QPushButton("IK（ABP）")
        self._btn_send_ik.setEnabled(False)
        self._btn_send_ik.clicked.connect(self._on_send_ik)
        btn_row.addWidget(self._btn_reset, 1)
        btn_row.addWidget(self._btn_add, 2)
        btn_row.addWidget(self._btn_send_ik, 2)
        root.addLayout(btn_row)
        root.addStretch(1)

        Style.apply(self)

    def _on_pick_mesh(self):
        if unreal is None:
            return
        assets = unreal.EditorUtilityLibrary.get_selected_assets()
        for a in assets:
            try:
                if isinstance(a, unreal.SkeletalMesh):
                    self._skeletal_mesh = a
                    self._le_mesh.setText(a.get_path_name())
                    self._btn_add.setEnabled(True)
                    return
            except Exception:
                continue
        self._skeletal_mesh = None
        self._le_mesh.setText("")
        self._btn_add.setEnabled(False)

    def _on_pick_anim_bp(self):
        if unreal is None:
            return
        assets = unreal.EditorUtilityLibrary.get_selected_assets()
        for a in assets:
            try:
                if isinstance(a, unreal.AnimBlueprint):
                    self._anim_blueprint = a
                    self._le_anim_bp.setText(a.get_path_name())
                    self._btn_send_ik.setEnabled(True)
                    return
            except Exception:
                continue
        self._anim_blueprint = None
        self._le_anim_bp.setText("")
        self._btn_send_ik.setEnabled(False)

    def _on_reset(self):
        self._skeletal_mesh = None
        self._le_mesh.setText("")
        self._btn_add.setEnabled(False)
        self._anim_blueprint = None
        self._le_anim_bp.setText("")
        self._btn_send_ik.setEnabled(False)

    def _on_add_bones(self):
        if unreal is None or self._skeletal_mesh is None:
            return
        entries = [
            ("IK_L_Foot", "Root"),
            ("IK_R_Foot", "Root"),
            ("IK_L_ToeBase", "IK_L_Foot"),
            ("IK_R_ToeBase", "IK_R_Foot"),
        ]
        remove_order = [
            "IK_L_ToeBase",
            "IK_R_ToeBase",
            "IK_L_Foot",
            "IK_R_Foot",
        ]
        modifier = unreal.SkeletonModifier()
        if not modifier.set_skeletal_mesh(self._skeletal_mesh):
            unreal.log("[Vrm] SkeletalMeshの設定に失敗しました。")
            return
        existing_names = {str(name) for name in modifier.get_all_bone_names()}
        for name in remove_order:
            if name in existing_names:
                modifier.remove_bone(name, False)
        for name, parent in entries:
            target = name.replace("IK_", "J_Bip_", 1)
            parent_target = parent.replace("IK_", "J_Bip_", 1) if parent.startswith("IK_") else parent
            if target not in existing_names or parent_target not in existing_names:
                unreal.log(f"[Vrm] ボーン追加をスキップしました: {name}")
                continue
            target_global = modifier.get_bone_transform(target, True)
            parent_global = modifier.get_bone_transform(parent_target, True)
            local_transform = target_global.make_relative(parent_global)
            modifier.add_bone(name, parent, local_transform)
        if modifier.commit_skeleton_to_skeletal_mesh():
            unreal.log("[Vrm] IKボーンを追加しました。")
        else:
            unreal.log("[Vrm] IKボーンの追加に失敗しました。")

    def _on_send_ik(self):
        if unreal is None or self._anim_blueprint is None:
            return
        result = unreal.IK_LoaderImportLibrary.apply_pmx_ik_to_anim_blueprint(
            self._anim_blueprint, _PMX_IK_PAYLOAD
        )
        if result:
            unreal.log("[Pmx] IK送信しました。")
        else:
            unreal.log("[Pmx] IK送信に失敗しました。")

def show_window():
    global _pyside_app, _window
    app = QtWidgets.QApplication.instance()
    if app is None:
        _pyside_app = QtWidgets.QApplication(sys.argv)
    if _window is None:
        _window = VrmModelWindow()
        _window.finished.connect(lambda _: _on_close())
    _window.show()
    _window.raise_()
    _window.activateWindow()


def _on_close():
    global _window
    _window = None
