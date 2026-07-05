# -*- coding: utf-8 -*-
import os
import sys

try:
    _THIS_DIR = os.path.abspath(os.path.dirname(__file__))
    _PARENT_DIR = os.path.dirname(_THIS_DIR)
except Exception:
    _THIS_DIR = os.path.abspath(os.getcwd())
    _PARENT_DIR = _THIS_DIR

if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)
if _PARENT_DIR not in sys.path:
    sys.path.insert(0, _PARENT_DIR)

try:
    import unreal
    _HAS_UNREAL = True
except Exception:
    unreal = None
    _HAS_UNREAL = False

from PySide6 import QtWidgets, QtCore, QtGui
from . import QtStyle
from .VrmaReader import VrmaReader, _infer_total_frames
from . import VrmaBoneWriter
from VMD import VmdMorphWriter

_TABLE_STYLE = """
    QTableWidget {
        alternate-background-color: #212121;
    }
"""


class VrmaViewer(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setAcceptDrops(True)
        self.setWindowTitle("VRMA Animation tool")
        self.resize(650, 400)
        self.setMinimumSize(650, 400)
        self.setWindowFlags(self.windowFlags() | QtCore.Qt.WindowStaysOnTopHint)

        self.vrma_path = None
        self.vrma = None
        self.skeletal_mesh = None
        self.drag_hover = False
        self._morph_target_names = set()

        self._drag_overlay = QtWidgets.QWidget(self)
        self._drag_overlay.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents, True)
        self._drag_overlay.setStyleSheet("background-color: rgba(102, 158, 204, 77);")
        self._drag_overlay.hide()

        QtStyle.apply(self)
        self._build_ui()

    def _build_ui(self):
        outer = QtWidgets.QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(10)

        self.tabs = QtWidgets.QTabWidget()
        outer.addWidget(self.tabs, 1)

        self.tab_info = QtWidgets.QWidget()
        self.tab_bone = QtWidgets.QWidget()
        self.tab_expr = QtWidgets.QWidget()
        self.tabs.addTab(self.tab_info, "info")
        self.tabs.addTab(self.tab_bone, "Bone")
        self.tabs.addTab(self.tab_expr, "Expression")

        self._build_info_tab()
        self._build_bone_tab()
        self._build_expr_tab()


    def _build_info_tab(self):
        layout = QtWidgets.QVBoxLayout(self.tab_info)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)

        self.grp_file = QtWidgets.QGroupBox("Data")
        file_form = QtWidgets.QFormLayout(self.grp_file)
        file_form.setContentsMargins(10, 10, 10, 10)
        file_form.setSpacing(8)
        try:
            file_form.setLabelAlignment(QtCore.Qt.AlignLeft)
        except Exception:
            pass

        self.val_file = QtWidgets.QLabel("-")
        self.val_bone_keys = QtWidgets.QLabel("-")
        self.val_expr_keys = QtWidgets.QLabel("-")
        self.val_coord_scale = QtWidgets.QLabel("-")
        self.val_file.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self.val_bone_keys.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self.val_expr_keys.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self.val_coord_scale.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)

        file_form.addRow("File Name", self.val_file)
        file_form.addRow("Bone Keys", self.val_bone_keys)
        file_form.addRow("Expression Keys", self.val_expr_keys)
        file_form.addRow("Import Scale", self.val_coord_scale)

        self.grp_settings = QtWidgets.QGroupBox("Options")
        set_layout = QtWidgets.QVBoxLayout(self.grp_settings)
        set_layout.setContentsMargins(10, 10, 10, 10)
        set_layout.setSpacing(8)

        mesh_row = QtWidgets.QHBoxLayout()
        mesh_row.setSpacing(8)
        mesh_label = QtWidgets.QLabel("SKM")
        mesh_label.setMinimumWidth(180)
        self.ed_mesh = QtWidgets.QLineEdit("")
        self.ed_mesh.setReadOnly(True)
        self.btn_pick_mesh = QtWidgets.QPushButton("選択状態を取得")
        self.btn_pick_mesh.clicked.connect(self._on_pick_mesh)
        mesh_row.addWidget(mesh_label)
        mesh_row.addWidget(self.ed_mesh, 1)
        mesh_row.addWidget(self.btn_pick_mesh)
        set_layout.addLayout(mesh_row)

        folder_row = QtWidgets.QHBoxLayout()
        folder_row.setSpacing(8)
        folder_label = QtWidgets.QLabel("Folder (Animation Sequence)")
        folder_label.setMinimumWidth(180)
        self.ed_folder = QtWidgets.QLineEdit("")
        self.ed_folder.setReadOnly(True)
        self.btn_pick_folder = QtWidgets.QPushButton("選択状態を取得")
        self.btn_pick_folder.clicked.connect(self._on_pick_folder)
        folder_row.addWidget(folder_label)
        folder_row.addWidget(self.ed_folder, 1)
        folder_row.addWidget(self.btn_pick_folder)
        set_layout.addLayout(folder_row)

        btn_row = QtWidgets.QHBoxLayout()
        btn_row.setSpacing(8)
        self.btn_reset = QtWidgets.QPushButton("リセット")
        self.btn_reset.clicked.connect(self._on_reset_clicked)
        self.btn_import = QtWidgets.QPushButton("アニメーションをインポート")
        self.btn_import.setEnabled(False)
        self.btn_import.clicked.connect(self._on_import_clicked)
        btn_row.addWidget(self.btn_reset, 1)
        btn_row.addWidget(self.btn_import, 4)

        layout.addWidget(self.grp_file)
        layout.addWidget(self.grp_settings)
        layout.addLayout(btn_row)
        layout.addStretch(1)

    def _build_bone_tab(self):
        layout = QtWidgets.QHBoxLayout(self.tab_bone)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        self.lst_bone = QtWidgets.QListWidget()
        self.lst_bone.currentRowChanged.connect(self._on_bone_selected)

        self.tbl_bone = QtWidgets.QTableWidget(0, 3)
        self.tbl_bone.setHorizontalHeaderLabels(["frame", "pos (x, y, z)", "rot (x, y, z, w)"])
        self.tbl_bone.verticalHeader().setVisible(False)
        self.tbl_bone.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self.tbl_bone.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self.tbl_bone.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self.tbl_bone.setAlternatingRowColors(True)
        self.tbl_bone.setStyleSheet(self.tbl_bone.styleSheet() + _TABLE_STYLE)

        hdr = self.tbl_bone.horizontalHeader()
        hdr.setSectionResizeMode(0, QtWidgets.QHeaderView.ResizeMode.Fixed)
        hdr.setSectionResizeMode(1, QtWidgets.QHeaderView.ResizeMode.Stretch)
        hdr.setSectionResizeMode(2, QtWidgets.QHeaderView.ResizeMode.Stretch)

        split = QtWidgets.QSplitter()
        split.addWidget(self.lst_bone)
        split.addWidget(self.tbl_bone)
        split.setStretchFactor(0, 0)
        split.setStretchFactor(1, 1)
        split.setSizes([200, 500])
        layout.addWidget(split)

    def _build_expr_tab(self):
        layout = QtWidgets.QHBoxLayout(self.tab_expr)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        self.lst_expr = QtWidgets.QListWidget()
        self.lst_expr.currentRowChanged.connect(self._on_expr_selected)

        self.tbl_expr = QtWidgets.QTableWidget(0, 2)
        self.tbl_expr.setHorizontalHeaderLabels(["frame", "value"])
        self.tbl_expr.verticalHeader().setVisible(False)
        self.tbl_expr.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self.tbl_expr.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self.tbl_expr.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self.tbl_expr.setAlternatingRowColors(True)
        self.tbl_expr.setStyleSheet(self.tbl_expr.styleSheet() + _TABLE_STYLE)

        hdr = self.tbl_expr.horizontalHeader()
        hdr.setSectionResizeMode(0, QtWidgets.QHeaderView.ResizeMode.Fixed)
        hdr.setSectionResizeMode(1, QtWidgets.QHeaderView.ResizeMode.Stretch)

        split = QtWidgets.QSplitter()
        split.addWidget(self.lst_expr)
        split.addWidget(self.tbl_expr)
        split.setStretchFactor(0, 0)
        split.setStretchFactor(1, 1)
        split.setSizes([200, 500])
        layout.addWidget(split)

    def dragEnterEvent(self, event):
        md = event.mimeData()
        if md.hasUrls():
            for u in md.urls():
                p = u.toLocalFile()
                if p and p.lower().endswith(".vrma"):
                    self.drag_hover = True
                    self._drag_overlay.setGeometry(self.rect())
                    self._drag_overlay.raise_()
                    self._drag_overlay.show()
                    event.acceptProposedAction()
                    return
        event.ignore()

    def dragLeaveEvent(self, event):
        self.drag_hover = False
        self._drag_overlay.hide()
        event.accept()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._drag_overlay.setGeometry(self.rect())
        if self.drag_hover:
            self._drag_overlay.raise_()

    def dropEvent(self, event):
        self.drag_hover = False
        self._drag_overlay.hide()
        md = event.mimeData()
        if not md.hasUrls():
            event.ignore()
            return
        for u in md.urls():
            p = u.toLocalFile()
            if p and p.lower().endswith(".vrma"):
                self.vrma_path = os.path.normpath(p)
                self.vrma = None
                QtWidgets.QApplication.processEvents()
                try:
                    QtWidgets.QApplication.processEvents()
                    self.vrma = VrmaReader.read(self.vrma_path)
                    QtWidgets.QApplication.processEvents()

                    bones = self.vrma["bones"]
                    expressions = self.vrma["expressions"]
                    bone_keys_total = sum(len(v) for v in bones.values())
                    expr_keys_total = sum(len(v) for v in expressions.values())

                    self.val_file.setText(os.path.basename(self.vrma_path))
                    self.val_bone_keys.setText(str(bone_keys_total))
                    self.val_expr_keys.setText(str(expr_keys_total))
                    coord_scale = float(self.vrma.get("coordinate_scale", 1.0))
                    coord_n = int(self.vrma.get("coordinate_scale_n", 0))
                    self.val_coord_scale.setText(f"{coord_scale:g}倍 (100^{coord_n})")

                    QtWidgets.QApplication.processEvents()

                    self.lst_bone.clear()
                    for name in self.vrma["bone_order"]:
                        count = len(bones[name])
                        self.lst_bone.addItem(f"{name} [{count}]")

                    self.lst_expr.clear()
                    for name in self.vrma["expression_order"]:
                        count = len(expressions[name])
                        self.lst_expr.addItem(f"{name} [{count}]")

                    QtWidgets.QApplication.processEvents()

                    if self.lst_bone.count() > 0:
                        self.lst_bone.setCurrentRow(0)
                    if self.lst_expr.count() > 0:
                        self.lst_expr.setCurrentRow(0)

                    QtWidgets.QApplication.processEvents()

                except Exception as e:
                    self.vrma = None
                    self.val_file.setText(os.path.basename(self.vrma_path))
                    self.val_bone_keys.setText("-")
                    self.val_expr_keys.setText("-")
                    self.val_coord_scale.setText("-")
                    self.lst_bone.clear()
                    self.lst_expr.clear()
                    self.tbl_bone.setRowCount(0)
                    self.tbl_expr.setRowCount(0)
                    print(f"解析失敗: {e}")

                self._update_import_enabled()
                event.acceptProposedAction()
                return
        event.ignore()

    def _on_pick_mesh(self):
        if unreal is None:
            return
        assets = unreal.EditorUtilityLibrary.get_selected_assets()
        mesh = None
        for a in assets:
            try:
                if isinstance(a, unreal.SkeletalMesh):
                    mesh = a
                    break
            except Exception:
                continue
        if mesh is None:
            self.skeletal_mesh = None
            self.ed_mesh.setText("")
            self._update_import_enabled()
            return
        self.skeletal_mesh = mesh
        try:
            self.ed_mesh.setText(mesh.get_path_name())
        except Exception:
            try:
                self.ed_mesh.setText(mesh.get_name())
            except Exception:
                self.ed_mesh.setText("")
        try:
            names = mesh.get_all_morph_target_names()
            self._morph_target_names = set(str(n) for n in names)
        except Exception:
            self._morph_target_names = set()
        self._update_import_enabled()

    def _on_pick_folder(self):
        if unreal is None:
            return
        folder = None
        try:
            folder = unreal.EditorUtilityLibrary.get_current_content_browser_path()
        except Exception:
            folder = None
        if not folder:
            folder = "/Game"
        self.ed_folder.setText(str(folder))
        self._update_import_enabled()

    def _update_import_enabled(self):
        has_vrma = self.vrma is not None and self.vrma_path is not None
        has_mesh = self.skeletal_mesh is not None
        has_folder = bool(self.ed_folder.text().strip())
        self.btn_import.setEnabled(has_vrma and has_mesh and has_folder)

    def _on_reset_clicked(self):
        self.vrma = None
        self.vrma_path = None
        self.skeletal_mesh = None
        self.ed_mesh.setText("")
        self.ed_folder.setText("")
        self.val_file.setText("-")
        self.val_bone_keys.setText("-")
        self.val_expr_keys.setText("-")
        self.val_coord_scale.setText("-")
        self.lst_bone.clear()
        self.lst_expr.clear()
        self.tbl_bone.setRowCount(0)
        self.tbl_expr.setRowCount(0)
        self._update_import_enabled()

    def _on_import_clicked(self):
        if unreal is None:
            return
        if self.vrma is None or self.vrma_path is None:
            return
        if self.skeletal_mesh is None:
            return
        folder = self.ed_folder.text().strip()
        if not folder:
            folder = "/Game"
        skeleton = None
        try:
            skeleton = self.skeletal_mesh.get_editor_property("skeleton")
        except Exception:
            skeleton = None
        if skeleton is None:
            return

        self.btn_import.setEnabled(False)
        QtWidgets.QApplication.processEvents()

        fps = 30
        num_frames = _infer_total_frames(self.vrma, fps)

        base_name = os.path.splitext(os.path.basename(self.vrma_path))[0]
        asset_name = f"{base_name}_Anim"
        asset_path = f"{folder}/{asset_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            i = 1
            while True:
                asset_name = f"{base_name}_Anim_{i:02d}"
                asset_path = f"{folder}/{asset_name}"
                if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
                    break
                i += 1

        factory = unreal.AnimSequenceFactory()
        try:
            factory.target_skeleton = skeleton
        except Exception:
            try:
                factory.set_editor_property("target_skeleton", skeleton)
            except Exception:
                pass
        try:
            factory.preview_skeletal_mesh = self.skeletal_mesh
        except Exception:
            try:
                factory.set_editor_property("preview_skeletal_mesh", self.skeletal_mesh)
            except Exception:
                pass

        anim_seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name=asset_name,
            package_path=folder,
            asset_class=unreal.AnimSequence,
            factory=factory,
        )
        if anim_seq is None:
            self._update_import_enabled()
            return

        try:
            anim_seq.set_editor_property("interpolation", unreal.AnimInterpolationType.LINEAR)
        except Exception:
            pass

        ue_hips_ref = None
        try:
            tf = unreal.AnimationLibrary.get_bone_pose_for_frame(
                anim_seq, "J_Bip_C_Hips", 0, False
            )
            ue_hips_ref = (
                float(tf.translation.x),
                float(tf.translation.y),
                float(tf.translation.z),
            )
        except Exception:
            ue_hips_ref = None

        ctrl = anim_seq.get_editor_property("controller")
        ctrl.open_bracket("VRMAインポート", False)
        try:
            try:
                ctrl.set_frame_rate(unreal.FrameRate(fps, 1), False)
            except Exception:
                pass
            try:
                ctrl.set_number_of_frames(unreal.FrameNumber(num_frames), False)
            except Exception:
                try:
                    ctrl.set_number_of_frames(num_frames, False)
                except Exception:
                    pass

            bones = self.vrma.get("bones", {}) or {}
            if bones:
                QtWidgets.QApplication.processEvents()
                VrmaBoneWriter.apply_bones(ctrl, bones, fps, num_frames, self.skeletal_mesh,
                                           hips_node_rest=self.vrma.get("hips_node_rest"),
                                           ue_hips_ref=ue_hips_ref,
                                           position_import_scale=self.vrma.get("coordinate_scale", 1.0))
                QtWidgets.QApplication.processEvents()

            if not self._morph_target_names:
                try:
                    self._morph_target_names = set(str(n) for n in self.skeletal_mesh.get_all_morph_target_names())
                except Exception:
                    self._morph_target_names = set()

            expressions = self.vrma.get("expressions", {}) or {}
            VmdMorphWriter.apply_morphs(ctrl, expressions, skeleton, self._morph_target_names, fps)

        finally:
            try:
                ctrl.close_bracket(False)
            except Exception:
                pass

        try:
            unreal.EditorAssetLibrary.save_loaded_asset(anim_seq)
        except Exception:
            pass

        QtWidgets.QApplication.processEvents()
        self._update_import_enabled()

    def _on_bone_selected(self, row: int):
        self.tbl_bone.setRowCount(0)
        if self.vrma is None or row < 0:
            return
        item = self.lst_bone.item(row).text()
        name = item.rsplit(" [", 1)[0]
        keys = self.vrma["bones"].get(name, [])
        self.tbl_bone.setRowCount(len(keys))
        for i, key in enumerate(keys):
            frame = key[0]
            pos = key[1]
            rot = key[2]
            it0 = QtWidgets.QTableWidgetItem(f"{frame}")
            it1 = QtWidgets.QTableWidgetItem(f"{pos[0]:.4f}, {pos[1]:.4f}, {pos[2]:.4f}")
            it2 = QtWidgets.QTableWidgetItem(f"{rot[0]:.4f}, {rot[1]:.4f}, {rot[2]:.4f}, {rot[3]:.4f}")
            it0.setTextAlignment(QtCore.Qt.AlignCenter)
            it1.setTextAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
            it2.setTextAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
            self.tbl_bone.setItem(i, 0, it0)
            self.tbl_bone.setItem(i, 1, it1)
            self.tbl_bone.setItem(i, 2, it2)
        table_width = self.tbl_bone.viewport().width()
        self.tbl_bone.setColumnWidth(0, int(table_width * 0.15))

    def _on_expr_selected(self, row: int):
        self.tbl_expr.setRowCount(0)
        if self.vrma is None or row < 0:
            return
        item = self.lst_expr.item(row).text()
        name = item.rsplit(" [", 1)[0]
        keys = self.vrma["expressions"].get(name, [])
        self.tbl_expr.setRowCount(len(keys))
        for i, (frame, w) in enumerate(keys):
            it0 = QtWidgets.QTableWidgetItem(f"{frame}")
            it1 = QtWidgets.QTableWidgetItem(f"{float(w):.4f}")
            it0.setTextAlignment(QtCore.Qt.AlignCenter)
            it1.setTextAlignment(QtCore.Qt.AlignCenter)
            self.tbl_expr.setItem(i, 0, it0)
            self.tbl_expr.setItem(i, 1, it1)
        table_width = self.tbl_expr.viewport().width()
        self.tbl_expr.setColumnWidth(0, int(table_width * 0.15))


_pyside_app = None
_pyside_win = None


def show_window():
    global _pyside_app, _pyside_win
    app = QtWidgets.QApplication.instance()
    if app is None:
        _pyside_app = QtWidgets.QApplication(sys.argv)
    if _pyside_win is None:
        _pyside_win = VrmaViewer()
    _pyside_win.show()
    _pyside_win.raise_()
    _pyside_win.activateWindow()
    return _pyside_win


if __name__ == "__main__":
    if _HAS_UNREAL:
        show_window()
    else:
        app = QtWidgets.QApplication(sys.argv)
        window = VrmaViewer()
        window.show()
        sys.exit(app.exec())
