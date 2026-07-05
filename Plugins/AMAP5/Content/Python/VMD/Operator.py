import os
import struct
import sys
try:
    _THIS_DIR = os.path.abspath(os.path.dirname(__file__))
except Exception:
    _THIS_DIR = os.path.abspath(os.getcwd())
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)
import math
try:
    import unreal
    _HAS_UNREAL = True
except Exception:
    unreal = None
    _HAS_UNREAL = False
from PySide6 import QtWidgets, QtCore, QtGui
from . import QtStyle
from .VmdReader import VmdReader, _infer_total_frames
from . import VmdBoneWriter
from . import VmdMorphWriter
from .VrmOperator import _VRM_BONE_MAP, _VRM_MORPH_MAP

class CircleRadioButton(QtWidgets.QRadioButton):
    def __init__(self, text, parent=None):
        super().__init__(text, parent)
        self.setMinimumHeight(20)
        self.setCursor(QtCore.Qt.PointingHandCursor)
        self.setAttribute(QtCore.Qt.WA_Hover, True)

    def hitButton(self, pos):
        return self.rect().contains(pos)

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.Antialiasing)
        rect = self.rect().adjusted(0, 0, -1, -1)
        r = 7
        cx = r + 2
        cy = rect.height() // 2
        if self.isEnabled():
            dot = QtGui.QColor('#4d79ff')
            text_color = QtGui.QColor('#ffffff')
        else:
            dot = QtGui.QColor('#3e4b70')
            text_color = QtGui.QColor('#8b93a8')
        painter.setPen(QtGui.QPen(dot, 1))
        painter.setBrush(QtCore.Qt.NoBrush)
        painter.drawEllipse(cx - r, cy - r, r * 2, r * 2)
        if self.isChecked():
            ri = 4
            painter.setPen(QtCore.Qt.NoPen)
            painter.setBrush(dot)
            painter.drawEllipse(cx - ri, cy - ri, ri * 2, ri * 2)
        painter.setFont(self.font())
        painter.setPen(text_color)
        painter.drawText(rect.adjusted(r * 2 + 10, 0, 0, 0), QtCore.Qt.AlignVCenter | QtCore.Qt.AlignLeft, self.text())


class VmdViewer(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setAcceptDrops(True)
        self.setWindowTitle("VMD Animation tool")
        self.resize(650, 400)
        self.setMinimumSize(650, 400)

        self.setWindowFlags(self.windowFlags() | QtCore.Qt.WindowStaysOnTopHint)

        self.vmd_path = None
        self.vmd = None
        self.skeletal_mesh = None
        self.drag_hover = False

        self._morph_target_names = set()

        self._drag_overlay = QtWidgets.QWidget(self)
        self._drag_overlay.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents, True)
        self._drag_overlay.setStyleSheet("background-color: rgba(102, 158, 204, 77);")
        self._drag_overlay.hide()

        self._setup_style()
        self._build_ui()

    def _setup_style(self):
        QtStyle.apply(self)
    def _build_ui(self):
        outer = QtWidgets.QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(10)

        self.tabs = QtWidgets.QTabWidget()
        outer.addWidget(self.tabs, 1)

        self.tab_info = QtWidgets.QWidget()
        self.tab_bone = QtWidgets.QWidget()
        self.tab_morph = QtWidgets.QWidget()
        self.tabs.addTab(self.tab_info, "info")
        self.tabs.addTab(self.tab_bone, "Bone")
        self.tabs.addTab(self.tab_morph, "Morph")

        self._build_info_tab()
        self._build_bone_tab()
        self._build_morph_tab()


    def _build_info_tab(self):
        layout = QtWidgets.QVBoxLayout(self.tab_info)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)

        self.grp_vmd = QtWidgets.QGroupBox("Data")
        vmd_form = QtWidgets.QFormLayout(self.grp_vmd)
        vmd_form.setContentsMargins(10, 10, 10, 10)
        vmd_form.setSpacing(8)
        try:
            vmd_form.setLabelAlignment(QtCore.Qt.AlignLeft)
        except Exception:
            pass

        self.val_vmd_file = QtWidgets.QLabel("-")
        self.val_bone_keys = QtWidgets.QLabel("-")
        self.val_morph_keys = QtWidgets.QLabel("-")
        self.val_vmd_file.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self.val_bone_keys.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self.val_morph_keys.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)

        vmd_form.addRow("File Name", self.val_vmd_file)
        vmd_form.addRow("Bone Keys", self.val_bone_keys)
        vmd_form.addRow("Morph Keys", self.val_morph_keys)

        self.grp_settings = QtWidgets.QGroupBox("Options")
        set_layout = QtWidgets.QVBoxLayout(self.grp_settings)
        set_layout.setContentsMargins(10, 10, 10, 10)
        set_layout.setSpacing(8)

        dest_layout = QtWidgets.QHBoxLayout()
        dest_layout.setSpacing(12)
        dest_label = QtWidgets.QLabel("送信先")
        dest_label.setMinimumWidth(180)
        self.rb_dest_pmx = CircleRadioButton("PMX")
        self.rb_dest_vrm = CircleRadioButton("VRM")
        self.rb_dest_pmx.setChecked(True)
        self._dest_group = QtWidgets.QButtonGroup(self)
        self._dest_group.addButton(self.rb_dest_pmx)
        self._dest_group.addButton(self.rb_dest_vrm)
        dest_layout.addWidget(dest_label)
        dest_layout.addWidget(self.rb_dest_pmx)
        dest_layout.addWidget(self.rb_dest_vrm)
        dest_layout.addStretch(1)
        set_layout.addLayout(dest_layout)

        mesh_container = QtWidgets.QHBoxLayout()
        mesh_container.setSpacing(8)
        mesh_label = QtWidgets.QLabel("SKM")
        mesh_label.setMinimumWidth(180)
        self.ed_mesh = QtWidgets.QLineEdit("")
        self.ed_mesh.setReadOnly(True)
        self.btn_pick_mesh = QtWidgets.QPushButton("選択状態を取得")
        self.btn_pick_mesh.clicked.connect(self._on_pick_mesh)
        mesh_container.addWidget(mesh_label)
        mesh_container.addWidget(self.ed_mesh, 1)
        mesh_container.addWidget(self.btn_pick_mesh)

        set_layout.addLayout(mesh_container)

        folder_container = QtWidgets.QHBoxLayout()
        folder_container.setSpacing(8)
        folder_label = QtWidgets.QLabel("Folder (Animation Sequence)")
        folder_label.setMinimumWidth(180)
        self.ed_folder = QtWidgets.QLineEdit("")
        self.ed_folder.setReadOnly(True)
        self.btn_pick_folder = QtWidgets.QPushButton("選択状態を取得")
        self.btn_pick_folder.clicked.connect(self._on_pick_folder)
        folder_container.addWidget(folder_label)
        folder_container.addWidget(self.ed_folder, 1)
        folder_container.addWidget(self.btn_pick_folder)

        set_layout.addLayout(folder_container)

        btn_row = QtWidgets.QHBoxLayout()
        btn_row.setSpacing(8)
        self.btn_reset = QtWidgets.QPushButton("リセット")
        self.btn_reset.clicked.connect(self._on_reset_clicked)
        self.btn_import = QtWidgets.QPushButton("アニメーションをインポート")
        self.btn_import.setEnabled(False)
        self.btn_import.clicked.connect(self._on_import_clicked)
        btn_row.addWidget(self.btn_reset, 1)
        btn_row.addWidget(self.btn_import, 4)

        layout.addWidget(self.grp_vmd)
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
        
        bone_header = self.tbl_bone.horizontalHeader()
        bone_header.setSectionResizeMode(0, QtWidgets.QHeaderView.ResizeMode.Fixed)
        bone_header.setSectionResizeMode(1, QtWidgets.QHeaderView.ResizeMode.Stretch)
        bone_header.setSectionResizeMode(2, QtWidgets.QHeaderView.ResizeMode.Stretch)
        
        self.tbl_bone.setAlternatingRowColors(True)
        self.tbl_bone.setStyleSheet(self.tbl_bone.styleSheet() + """
            QTableWidget {
                alternate-background-color: #212121;
            }
        """)

        split = QtWidgets.QSplitter()
        split.addWidget(self.lst_bone)
        split.addWidget(self.tbl_bone)
        split.setStretchFactor(0, 0)
        split.setStretchFactor(1, 1)
        split.setSizes([200, 500])
        layout.addWidget(split)

    def _build_morph_tab(self):
        layout = QtWidgets.QHBoxLayout(self.tab_morph)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        self.lst_morph = QtWidgets.QListWidget()
        self.lst_morph.currentRowChanged.connect(self._on_morph_selected)

        self.tbl_morph = QtWidgets.QTableWidget(0, 2)
        self.tbl_morph.setHorizontalHeaderLabels(["frame", "value"])
        self.tbl_morph.verticalHeader().setVisible(False)
        self.tbl_morph.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self.tbl_morph.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self.tbl_morph.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        
        morph_header = self.tbl_morph.horizontalHeader()
        morph_header.setSectionResizeMode(0, QtWidgets.QHeaderView.ResizeMode.Fixed)
        morph_header.setSectionResizeMode(1, QtWidgets.QHeaderView.ResizeMode.Stretch)
        
        self.tbl_morph.setAlternatingRowColors(True)
        self.tbl_morph.setStyleSheet(self.tbl_morph.styleSheet() + """
            QTableWidget {
                alternate-background-color: #212121;
            }
        """)

        split = QtWidgets.QSplitter()
        split.addWidget(self.lst_morph)
        split.addWidget(self.tbl_morph)
        split.setStretchFactor(0, 0)
        split.setStretchFactor(1, 1)
        split.setSizes([200, 500])
        layout.addWidget(split)

    def dragEnterEvent(self, event):
        md = event.mimeData()
        if md.hasUrls():
            for u in md.urls():
                p = u.toLocalFile()
                if p and p.lower().endswith(".vmd"):
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
            if p and p.lower().endswith(".vmd"):
                self.vmd_path = os.path.normpath(p)
                self.vmd = None

                QtWidgets.QApplication.processEvents()
                
                try:
                    QtWidgets.QApplication.processEvents()
                    
                    self.vmd = VmdReader.read(self.vmd_path)
                    
                    QtWidgets.QApplication.processEvents()
                    
                    bones = self.vmd["bones"]
                    morphs = self.vmd["morphs"]

                    bone_keys_total = sum(len(v) for v in bones.values())
                    morph_keys_total = sum(len(v) for v in morphs.values())

                    self.val_vmd_file.setText(f"{os.path.basename(self.vmd_path)}")
                    self.val_bone_keys.setText(f"{bone_keys_total}")
                    self.val_morph_keys.setText(f"{morph_keys_total}")

                    QtWidgets.QApplication.processEvents()

                    self.lst_bone.clear()
                    for name in self.vmd["bone_order"]:
                        count = len(bones[name])
                        self.lst_bone.addItem(f"{name} [{count}]")

                    self.lst_morph.clear()
                    for name in self.vmd["morph_order"]:
                        count = len(morphs[name])
                        self.lst_morph.addItem(f"{name} [{count}]")

                    QtWidgets.QApplication.processEvents()

                    if self.lst_bone.count() > 0:
                        self.lst_bone.setCurrentRow(0)
                    if self.lst_morph.count() > 0:
                        self.lst_morph.setCurrentRow(0)

                    QtWidgets.QApplication.processEvents()
                    
                except Exception as e:
                    self.vmd = None
                    self.val_vmd_file.setText(f"{os.path.basename(self.vmd_path)}")
                    self.val_bone_keys.setText("-")
                    self.val_morph_keys.setText("-")
                    self.lst_bone.clear()
                    self.lst_morph.clear()
                    self.tbl_bone.setRowCount(0)
                    self.tbl_morph.setRowCount(0)
                    print(f"解析失敗: {e}")
                
                self._update_import_enabled()
                event.acceptProposedAction()
                return

        event.ignore()

    def _on_pick_mesh(self):
        if unreal is None:
            print("Unreal環境ではありません。")
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
            print("Unreal環境ではありません。")
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
        has_vmd = self.vmd is not None and self.vmd_path is not None
        has_mesh = self.skeletal_mesh is not None
        has_folder = bool(self.ed_folder.text().strip())
        self.btn_import.setEnabled(has_vmd and has_mesh and has_folder)

    def _on_reset_clicked(self):
        self.vmd = None
        self.vmd_path = None
        self.skeletal_mesh = None
        self.ed_mesh.setText("")
        self.ed_folder.setText("")
        self.val_vmd_file.setText("-")
        self.val_bone_keys.setText("-")
        self.val_morph_keys.setText("-")
        self.lst_bone.clear()
        self.lst_morph.clear()
        self.tbl_bone.setRowCount(0)
        self.tbl_morph.setRowCount(0)
        self._update_import_enabled()

    def _on_import_clicked(self):
        if unreal is None:
            print("Unreal環境ではありません。")
            return
        if self.vmd is None or self.vmd_path is None:
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

        base_name = os.path.splitext(os.path.basename(self.vmd_path))[0]
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

        fps = 30
        num_frames = _infer_total_frames(self.vmd)
        ctrl = anim_seq.get_editor_property("controller")
        ctrl.open_bracket("VMDモーフ取り込み", False)
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

            bones = self.vmd.get("bones", {}) or {}
            is_vrm = self.rb_dest_vrm.isChecked()
            if is_vrm:
                bones = VmdBoneWriter.prepare_vrm_beta_bones(bones, num_frames, _VRM_BONE_MAP)
            if bones:
                QtWidgets.QApplication.processEvents()
                axis_map = [[1, 0, 0], [0, 0, -1], [0, 1, 0]] if is_vrm else None
                VmdBoneWriter.apply_bones(ctrl, bones, fps, num_frames, self.skeletal_mesh, is_vrm=is_vrm, axis_map=axis_map, is_vrm_test=False)

                QtWidgets.QApplication.processEvents()

            if not self._morph_target_names:
                try:
                    self._morph_target_names = set(str(n) for n in self.skeletal_mesh.get_all_morph_target_names())
                except Exception:
                    self._morph_target_names = set()

            morphs = self.vmd["morphs"]
            if is_vrm:
                morphs = {_VRM_MORPH_MAP.get(k, k): v for k, v in morphs.items()}

            VmdMorphWriter.apply_morphs(ctrl, morphs, skeleton, self._morph_target_names, fps)

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
        if self.vmd is None or row < 0:
            return
        item = self.lst_bone.item(row).text()
        name = item.rsplit(" [", 1)[0]
        keys = self.vmd["bones"].get(name, [])

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

    def _on_morph_selected(self, row: int):
        self.tbl_morph.setRowCount(0)
        if self.vmd is None or row < 0:
            return
        item = self.lst_morph.item(row).text()
        name = item.rsplit(" [", 1)[0]
        keys = self.vmd["morphs"].get(name, [])

        self.tbl_morph.setRowCount(len(keys))
        for i, (frame, w) in enumerate(keys):
            it0 = QtWidgets.QTableWidgetItem(f"{frame}")
            it1 = QtWidgets.QTableWidgetItem(f"{float(w):.4f}")
            
            it0.setTextAlignment(QtCore.Qt.AlignCenter)
            it1.setTextAlignment(QtCore.Qt.AlignCenter)
            
            self.tbl_morph.setItem(i, 0, it0)
            self.tbl_morph.setItem(i, 1, it1)

        table_width = self.tbl_morph.viewport().width()
        self.tbl_morph.setColumnWidth(0, int(table_width * 0.15))



_pyside_app = None
_pyside_win = None


def show_window():
    global _pyside_app, _pyside_win
    app = QtWidgets.QApplication.instance()
    if app is None:
        _pyside_app = QtWidgets.QApplication(sys.argv)
        app = _pyside_app
    if _pyside_win is None:
        _pyside_win = VmdViewer()
    _pyside_win.show()
    _pyside_win.raise_()
    _pyside_win.activateWindow()
    return _pyside_win

if __name__ == "__main__":
    if _HAS_UNREAL:
        show_window()
    else:
        app = QtWidgets.QApplication(sys.argv)
        window = VmdViewer()
        window.show()
        sys.exit(app.exec())
