import os
import sys
from PySide6 import QtCore, QtWidgets
from . import Operator
from . import Style


class _CompactListDelegate(QtWidgets.QStyledItemDelegate):
    def __init__(self, height, parent=None):
        super().__init__(parent)
        self._height = height

    def sizeHint(self, option, index):
        size = super().sizeHint(option, index)
        size.setHeight(self._height)
        return size


class MainWindow(QtWidgets.QDialog):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PMX IK / Physics tool")
        self.resize(650, 400)
        self.setMinimumSize(650, 400)
        self.setAcceptDrops(True)

        self._pmx_path = ""
        self._anim_bp = None
        self._rb_data = []
        self._joint_data = []
        self._ik_data = []
        self._grant_data = []
        self._current_ik_links = []
        self._drag_hover = False
        self._drag_overlay = QtWidgets.QWidget(self)
        self._drag_overlay.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents, True)
        self._drag_overlay.setStyleSheet("background-color: rgba(102, 158, 204, 77);")
        self._drag_overlay.hide()

        flags = (QtCore.Qt.Window | QtCore.Qt.WindowStaysOnTopHint
                 | QtCore.Qt.WindowMinimizeButtonHint
                 | QtCore.Qt.WindowMaximizeButtonHint
                 | QtCore.Qt.WindowCloseButtonHint)
        self.setWindowFlags(flags)

        root = QtWidgets.QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)
        tabs = QtWidgets.QTabWidget()
        root.addWidget(tabs)

        tab_info    = QtWidgets.QWidget()
        tab_ik      = QtWidgets.QWidget()
        tab_grant   = QtWidgets.QWidget()
        tab_physics = QtWidgets.QWidget()
        tab_joint   = QtWidgets.QWidget()
        tabs.addTab(tab_info,    "info")
        tabs.addTab(tab_ik,      "IK")
        tabs.addTab(tab_grant,   "Append")
        tabs.addTab(tab_physics, "Rigidbody")
        tabs.addTab(tab_joint,   "Joint")

        self._build_info_tab(tab_info)
        self._build_ik_tab(tab_ik)
        self._build_grant_tab(tab_grant)
        self._build_physics_tab(tab_physics)
        self._build_joint_tab(tab_joint)

        Style.apply(self)
        Operator.update_import_enabled(self)

    def _build_info_tab(self, tab):
        layout = QtWidgets.QVBoxLayout(tab)

        gb_basic = QtWidgets.QGroupBox("Data")
        form = QtWidgets.QFormLayout(gb_basic)
        self._lbl_file  = QtWidgets.QLabel("-")
        self._lbl_model = QtWidgets.QLabel("-")
        form.addRow("File Name", self._lbl_file)
        form.addRow("Model", self._lbl_model)
        layout.addWidget(gb_basic)

        gb_opt = QtWidgets.QGroupBox("Options")
        grid = QtWidgets.QGridLayout(gb_opt)
        grid.addWidget(QtWidgets.QLabel("ABP"), 0, 0)
        self._le_abp = QtWidgets.QLineEdit()
        self._le_abp.setReadOnly(True)
        grid.addWidget(self._le_abp, 0, 1)
        btn_pick = QtWidgets.QPushButton("選択状態を取得")
        btn_pick.clicked.connect(lambda: Operator.pick_anim_bp(self))
        grid.addWidget(btn_pick, 0, 2)
        layout.addWidget(gb_opt)

        btn_row = QtWidgets.QHBoxLayout()
        self._btn_reset = QtWidgets.QPushButton("リセット")
        self._btn_reset.clicked.connect(lambda: Operator.reset_window(self))
        self._btn_import_ik = QtWidgets.QPushButton("IK（ABP）")
        self._btn_import_ik.setEnabled(False)
        self._btn_import_ik.clicked.connect(lambda: Operator.import_ik(self))
        self._btn_import_bullet = QtWidgets.QPushButton("BulletPhysics（ABP）")
        self._btn_import_bullet.setEnabled(False)
        self._btn_import_bullet.clicked.connect(lambda: Operator.import_physics(self))
        btn_row.addWidget(self._btn_reset, 1)
        btn_row.addWidget(self._btn_import_ik, 2)
        btn_row.addWidget(self._btn_import_bullet, 2)
        layout.addLayout(btn_row)
        layout.addStretch(1)

    def _build_ik_tab(self, tab):
        layout = QtWidgets.QHBoxLayout(tab)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        self._ik_list = QtWidgets.QListWidget()
        self._ik_list.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._ik_list.currentRowChanged.connect(self._on_ik_selected)
        self._ik_list.setFixedWidth(120)
        layout.addWidget(self._ik_list, 0)

        right = QtWidgets.QWidget()
        right_layout = QtWidgets.QVBoxLayout(right)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(10)

        form_box = QtWidgets.QGroupBox("IK")
        form = QtWidgets.QFormLayout(form_box)
        form.setContentsMargins(10, 10, 10, 10)
        form.setSpacing(8)
        self._lbl_ik_bone_class = QtWidgets.QLabel("-")
        self._lbl_ik_bone_index = QtWidgets.QLabel("-")
        self._lbl_ikbone = QtWidgets.QLabel("-")
        self._lbl_target = QtWidgets.QLabel("-")
        self._lbl_loop   = QtWidgets.QLabel("-")
        self._lbl_angle  = QtWidgets.QLabel("-")
        form.addRow("Bone Class", self._lbl_ik_bone_class)
        form.addRow("Bone Index", self._lbl_ik_bone_index)
        form.addRow("IKBone",     self._lbl_ikbone)
        form.addRow("TargetBone", self._lbl_target)
        form.addRow("Loop",       self._lbl_loop)
        form.addRow("Angle",      self._lbl_angle)
        right_layout.addWidget(form_box)

        links_box = QtWidgets.QGroupBox("Link")
        links_layout = QtWidgets.QHBoxLayout(links_box)
        links_layout.setContentsMargins(10, 10, 10, 10)
        links_layout.setSpacing(10)

        self._ik_link_list = QtWidgets.QListWidget()
        self._ik_link_list.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._ik_link_list.currentRowChanged.connect(self._on_ik_link_selected)
        self._ik_link_list.setFixedWidth(120)
        links_layout.addWidget(self._ik_link_list, 0)

        limit_panel  = QtWidgets.QWidget()
        limit_layout = QtWidgets.QVBoxLayout(limit_panel)
        limit_layout.setContentsMargins(0, 0, 0, 0)
        limit_layout.setSpacing(8)

        limit_form = QtWidgets.QFormLayout()
        limit_form.setContentsMargins(0, 0, 0, 0)
        limit_form.setSpacing(8)
        self._lbl_linkbone       = QtWidgets.QLabel("-")
        self._lbl_enable_limit_x = QtWidgets.QLabel("-")
        self._lbl_enable_limit_y = QtWidgets.QLabel("-")
        self._lbl_enable_limit_z = QtWidgets.QLabel("-")
        limit_form.addRow("LinkBone",       self._lbl_linkbone)
        limit_form.addRow("Enable Limit X", self._lbl_enable_limit_x)
        limit_form.addRow("Enable Limit Y", self._lbl_enable_limit_y)
        limit_form.addRow("Enable Limit Z", self._lbl_enable_limit_z)
        limit_layout.addLayout(limit_form, 0)

        grid = QtWidgets.QGridLayout()
        grid.setHorizontalSpacing(6)
        grid.setVerticalSpacing(6)
        grid.addWidget(QtWidgets.QLabel(""),         0, 0)
        grid.addWidget(QtWidgets.QLabel("MinLimit"), 0, 1)
        grid.addWidget(QtWidgets.QLabel("MaxLimit"), 0, 2)
        self._lbl_xmin = QtWidgets.QLabel("-")
        self._lbl_xmax = QtWidgets.QLabel("-")
        self._lbl_ymin = QtWidgets.QLabel("-")
        self._lbl_ymax = QtWidgets.QLabel("-")
        self._lbl_zmin = QtWidgets.QLabel("-")
        self._lbl_zmax = QtWidgets.QLabel("-")
        grid.addWidget(QtWidgets.QLabel("X"), 1, 0)
        grid.addWidget(self._lbl_xmin, 1, 1)
        grid.addWidget(self._lbl_xmax, 1, 2)
        grid.addWidget(QtWidgets.QLabel("Y"), 2, 0)
        grid.addWidget(self._lbl_ymin, 2, 1)
        grid.addWidget(self._lbl_ymax, 2, 2)
        grid.addWidget(QtWidgets.QLabel("Z"), 3, 0)
        grid.addWidget(self._lbl_zmin, 3, 1)
        grid.addWidget(self._lbl_zmax, 3, 2)
        limit_layout.addLayout(grid, 1)
        limit_layout.addStretch(1)

        links_layout.addWidget(limit_panel, 1)
        right_layout.addWidget(links_box, 1)
        layout.addWidget(right, 1)

    def _build_grant_tab(self, tab):
        layout = QtWidgets.QHBoxLayout(tab)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        self._grant_list = QtWidgets.QListWidget()
        self._grant_list.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._grant_list.currentRowChanged.connect(self._on_grant_selected)
        self._grant_list.setFixedWidth(150)
        layout.addWidget(self._grant_list, 0)

        detail_box = QtWidgets.QGroupBox("付与親")
        form = QtWidgets.QFormLayout(detail_box)
        form.setContentsMargins(10, 10, 10, 10)
        form.setSpacing(8)
        self._lbl_gr_bone_class  = QtWidgets.QLabel("-")
        self._lbl_gr_bone_index  = QtWidgets.QLabel("-")
        self._lbl_gr_bone        = QtWidgets.QLabel("-")
        self._lbl_gr_parent_bone = QtWidgets.QLabel("-")
        self._lbl_gr_weight      = QtWidgets.QLabel("-")
        self._lbl_gr_rotation    = QtWidgets.QLabel("-")
        self._lbl_gr_translation = QtWidgets.QLabel("-")
        form.addRow("Bone Class", self._lbl_gr_bone_class)
        form.addRow("Bone Index", self._lbl_gr_bone_index)
        form.addRow("ボーン",   self._lbl_gr_bone)
        form.addRow("付与親",   self._lbl_gr_parent_bone)
        form.addRow("付与率",   self._lbl_gr_weight)
        form.addRow("付与回転", self._lbl_gr_rotation)
        form.addRow("付与移動", self._lbl_gr_translation)

        right = QtWidgets.QVBoxLayout()
        right.addWidget(detail_box)
        right.addStretch(1)
        layout.addLayout(right, 1)

    def _build_physics_tab(self, tab):
        layout = QtWidgets.QVBoxLayout(tab)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        splitter.setChildrenCollapsible(False)
        splitter.setHandleWidth(0)
        layout.addWidget(splitter)

        self._rb_list = QtWidgets.QListWidget()
        self._rb_list.setItemDelegate(_CompactListDelegate(20, self._rb_list))
        self._rb_list.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self._rb_list.setVerticalScrollMode(QtWidgets.QAbstractItemView.ScrollPerPixel)
        self._rb_list.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self._rb_list.setUniformItemSizes(True)
        self._rb_list.setSpacing(0)
        self._rb_list.setMinimumWidth(144)
        self._rb_list.currentRowChanged.connect(self._on_rb_selection_changed)
        splitter.addWidget(self._rb_list)

        detail_widget = QtWidgets.QWidget()
        detail_layout = QtWidgets.QVBoxLayout(detail_widget)
        detail_layout.setContentsMargins(0, 0, 0, 0)
        detail_layout.setSpacing(0)

        self._lbl_rb_name            = QtWidgets.QLabel("-")
        self._lbl_rb_bone            = QtWidgets.QLabel("-")
        self._lbl_rb_type            = QtWidgets.QLabel("-")
        self._lbl_rb_shape           = QtWidgets.QLabel("-")
        self._lbl_rb_bone.setWordWrap(True)
        self._lbl_rb_radius          = QtWidgets.QLabel("-")
        self._lbl_rb_width           = QtWidgets.QLabel("-")
        self._lbl_rb_height          = QtWidgets.QLabel("-")
        self._lbl_rb_depth           = QtWidgets.QLabel("-")
        self._lbl_rb_pos_x           = QtWidgets.QLabel("-")
        self._lbl_rb_pos_y           = QtWidgets.QLabel("-")
        self._lbl_rb_pos_z           = QtWidgets.QLabel("-")
        self._lbl_rb_rot_x           = QtWidgets.QLabel("-")
        self._lbl_rb_rot_y           = QtWidgets.QLabel("-")
        self._lbl_rb_rot_z           = QtWidgets.QLabel("-")
        self._lbl_rb_rot_w           = QtWidgets.QLabel("-")
        self._lbl_radius_label       = QtWidgets.QLabel("半径")
        self._lbl_width_label        = QtWidgets.QLabel("幅")
        self._lbl_height_label       = QtWidgets.QLabel("高さ")
        self._lbl_depth_label        = QtWidgets.QLabel("奥行")
        self._lbl_rb_group           = QtWidgets.QLabel("-")
        self._lbl_rb_pass_group      = QtWidgets.QLabel("-")
        self._lbl_rb_pass_group.setWordWrap(True)
        self._lbl_rb_mass            = QtWidgets.QLabel("-")
        self._lbl_rb_linear_damping  = QtWidgets.QLabel("-")
        self._lbl_rb_angular_damping = QtWidgets.QLabel("-")
        self._lbl_rb_restitution     = QtWidgets.QLabel("-")
        self._lbl_rb_friction        = QtWidgets.QLabel("-")

        top_layout = QtWidgets.QHBoxLayout()
        top_layout.setContentsMargins(0, 0, 0, 0)
        top_layout.setSpacing(0)

        gb_a = QtWidgets.QGroupBox("A")
        layout_a = QtWidgets.QVBoxLayout(gb_a)
        layout_a.setContentsMargins(6, 6, 6, 6)
        layout_a.setSpacing(0)
        form_a = QtWidgets.QFormLayout()
        form_a.setLabelAlignment(QtCore.Qt.AlignLeft)
        form_a.setFormAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
        form_a.setHorizontalSpacing(12)
        form_a.setVerticalSpacing(2)
        form_a.addRow("名前",     self._lbl_rb_name)
        form_a.addRow("関連ボーン", self._lbl_rb_bone)
        form_a.addRow("剛体タイプ", self._lbl_rb_type)
        form_a.addRow("形状",     self._lbl_rb_shape)
        layout_a.addLayout(form_a)

        gb_b = QtWidgets.QGroupBox("B")
        layout_b = QtWidgets.QVBoxLayout(gb_b)
        layout_b.setContentsMargins(6, 6, 6, 6)
        layout_b.setSpacing(0)
        form_b = QtWidgets.QFormLayout()
        form_b.setLabelAlignment(QtCore.Qt.AlignLeft)
        form_b.setFormAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
        form_b.setHorizontalSpacing(12)
        form_b.setVerticalSpacing(2)
        form_b.addRow("グループ",    self._lbl_rb_group)
        form_b.addRow("衝突グループ", self._lbl_rb_pass_group)
        layout_b.addLayout(form_b)
        layout_b.addStretch(1)

        top_layout.addWidget(gb_a, 1)
        top_layout.addWidget(gb_b, 1)
        detail_layout.addLayout(top_layout, 1)

        bottom_layout = QtWidgets.QHBoxLayout()
        bottom_layout.setContentsMargins(0, 0, 0, 0)
        bottom_layout.setSpacing(0)

        gb_c = QtWidgets.QGroupBox("C")
        grid_c = QtWidgets.QGridLayout(gb_c)
        grid_c.setContentsMargins(6, 6, 6, 6)
        grid_c.setHorizontalSpacing(15)
        grid_c.setVerticalSpacing(2)
        grid_c.setColumnStretch(1, 1)
        grid_c.addWidget(self._lbl_radius_label, 0, 0); grid_c.addWidget(self._lbl_rb_radius, 0, 1)
        grid_c.addWidget(self._lbl_width_label,  1, 0); grid_c.addWidget(self._lbl_rb_width,  1, 1)
        grid_c.addWidget(self._lbl_height_label, 2, 0); grid_c.addWidget(self._lbl_rb_height, 2, 1)
        grid_c.addWidget(self._lbl_depth_label,  3, 0); grid_c.addWidget(self._lbl_rb_depth,  3, 1)
        grid_c.addWidget(QtWidgets.QLabel("位置 X"), 4, 0); grid_c.addWidget(self._lbl_rb_pos_x, 4, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 Y"), 5, 0); grid_c.addWidget(self._lbl_rb_pos_y, 5, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 Z"), 6, 0); grid_c.addWidget(self._lbl_rb_pos_z, 6, 1)
        grid_c.addWidget(QtWidgets.QLabel("回転 X"), 7, 0); grid_c.addWidget(self._lbl_rb_rot_x, 7, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 Y"), 8, 0); grid_c.addWidget(self._lbl_rb_rot_y, 8, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 Z"), 9, 0); grid_c.addWidget(self._lbl_rb_rot_z, 9, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 W"), 10, 0); grid_c.addWidget(self._lbl_rb_rot_w, 10, 1)
        grid_c.setRowStretch(11, 1)

        gb_d = QtWidgets.QGroupBox("D")
        layout_d = QtWidgets.QVBoxLayout(gb_d)
        layout_d.setContentsMargins(6, 6, 6, 6)
        layout_d.setSpacing(0)
        form_d = QtWidgets.QFormLayout()
        form_d.setLabelAlignment(QtCore.Qt.AlignLeft)
        form_d.setFormAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
        form_d.setHorizontalSpacing(12)
        form_d.setVerticalSpacing(2)
        form_d.addRow("質量",   self._lbl_rb_mass)
        form_d.addRow("移動減衰", self._lbl_rb_linear_damping)
        form_d.addRow("回転減衰", self._lbl_rb_angular_damping)
        form_d.addRow("反発力", self._lbl_rb_restitution)
        form_d.addRow("摩擦力", self._lbl_rb_friction)
        layout_d.addLayout(form_d)
        layout_d.addStretch(1)

        bottom_layout.addWidget(gb_c, 1)
        bottom_layout.addWidget(gb_d, 1)
        detail_layout.addLayout(bottom_layout, 3)

        scroll = QtWidgets.QScrollArea()
        scroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        scroll.setWidget(detail_widget)
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        splitter.addWidget(scroll)
        splitter.setSizes([144, 506])

    def _build_joint_tab(self, tab):
        layout = QtWidgets.QVBoxLayout(tab)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        splitter.setChildrenCollapsible(False)
        splitter.setHandleWidth(0)
        layout.addWidget(splitter)

        self._jt_list = QtWidgets.QListWidget()
        self._jt_list.setItemDelegate(_CompactListDelegate(20, self._jt_list))
        self._jt_list.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self._jt_list.setVerticalScrollMode(QtWidgets.QAbstractItemView.ScrollPerPixel)
        self._jt_list.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self._jt_list.setUniformItemSizes(True)
        self._jt_list.setMinimumWidth(144)
        self._jt_list.setSpacing(0)
        self._jt_list.currentRowChanged.connect(self._on_joint_selection_changed)
        splitter.addWidget(self._jt_list)

        self._lbl_jt_name       = QtWidgets.QLabel("-")
        self._lbl_jt_kind       = QtWidgets.QLabel("-")
        self._lbl_jt_body_a     = QtWidgets.QLabel("-")
        self._lbl_jt_body_b     = QtWidgets.QLabel("-")
        self._lbl_jt_w_posx     = QtWidgets.QLabel("-")
        self._lbl_jt_w_posy     = QtWidgets.QLabel("-")
        self._lbl_jt_w_posz     = QtWidgets.QLabel("-")
        self._lbl_jt_calc_pos_x = QtWidgets.QLabel("-")
        self._lbl_jt_calc_pos_y = QtWidgets.QLabel("-")
        self._lbl_jt_calc_pos_z = QtWidgets.QLabel("-")
        self._lbl_jt_w_rot_x    = QtWidgets.QLabel("-")
        self._lbl_jt_w_rot_y    = QtWidgets.QLabel("-")
        self._lbl_jt_w_rot_z    = QtWidgets.QLabel("-")
        self._lbl_jt_calc_rot_x = QtWidgets.QLabel("-")
        self._lbl_jt_calc_rot_y = QtWidgets.QLabel("-")
        self._lbl_jt_calc_rot_z = QtWidgets.QLabel("-")
        self._lbl_jt_calc_rot_w = QtWidgets.QLabel("-")
        self._lbl_jt_move_lo_x  = QtWidgets.QLabel("-")
        self._lbl_jt_move_lo_y  = QtWidgets.QLabel("-")
        self._lbl_jt_move_lo_z  = QtWidgets.QLabel("-")
        self._lbl_jt_move_hi_x  = QtWidgets.QLabel("-")
        self._lbl_jt_move_hi_y  = QtWidgets.QLabel("-")
        self._lbl_jt_move_hi_z  = QtWidgets.QLabel("-")
        self._lbl_jt_angle_lo_x = QtWidgets.QLabel("-")
        self._lbl_jt_angle_lo_y = QtWidgets.QLabel("-")
        self._lbl_jt_angle_lo_z = QtWidgets.QLabel("-")
        self._lbl_jt_angle_hi_x = QtWidgets.QLabel("-")
        self._lbl_jt_angle_hi_y = QtWidgets.QLabel("-")
        self._lbl_jt_angle_hi_z = QtWidgets.QLabel("-")
        self._lbl_jt_spm_x      = QtWidgets.QLabel("-")
        self._lbl_jt_spm_y      = QtWidgets.QLabel("-")
        self._lbl_jt_spm_z      = QtWidgets.QLabel("-")
        self._lbl_jt_spr_x      = QtWidgets.QLabel("-")
        self._lbl_jt_spr_y      = QtWidgets.QLabel("-")
        self._lbl_jt_spr_z      = QtWidgets.QLabel("-")

        detail_widget = QtWidgets.QWidget()
        detail_layout = QtWidgets.QVBoxLayout(detail_widget)
        detail_layout.setContentsMargins(0, 0, 0, 0)
        detail_layout.setSpacing(0)

        top_layout = QtWidgets.QHBoxLayout()
        top_layout.setContentsMargins(0, 0, 0, 0)
        top_layout.setSpacing(0)

        gb_a = QtWidgets.QGroupBox("A")
        layout_a = QtWidgets.QVBoxLayout(gb_a)
        layout_a.setContentsMargins(6, 4, 6, 6)
        layout_a.setSpacing(0)
        form_a = QtWidgets.QFormLayout()
        form_a.setLabelAlignment(QtCore.Qt.AlignLeft)
        form_a.setFormAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
        form_a.setHorizontalSpacing(12)
        form_a.setVerticalSpacing(2)
        form_a.addRow("名前",     self._lbl_jt_name)
        form_a.addRow("種類",     self._lbl_jt_kind)
        form_a.addRow("接続剛体A", self._lbl_jt_body_a)
        form_a.addRow("接続剛体B", self._lbl_jt_body_b)
        layout_a.addLayout(form_a)
        layout_a.addStretch(1)

        gb_b = QtWidgets.QGroupBox("B")
        grid_b = QtWidgets.QGridLayout(gb_b)
        grid_b.setContentsMargins(6, 6, 6, 6)
        grid_b.setHorizontalSpacing(15)
        grid_b.setVerticalSpacing(2)
        grid_b.setColumnStretch(1, 1)
        grid_b.addWidget(QtWidgets.QLabel("位置 X"), 0, 0); grid_b.addWidget(self._lbl_jt_calc_pos_x, 0, 1)
        grid_b.addWidget(QtWidgets.QLabel("　　 Y"), 1, 0); grid_b.addWidget(self._lbl_jt_calc_pos_y, 1, 1)
        grid_b.addWidget(QtWidgets.QLabel("　　 Z"), 2, 0); grid_b.addWidget(self._lbl_jt_calc_pos_z, 2, 1)
        grid_b.addWidget(QtWidgets.QLabel("回転 X"), 3, 0); grid_b.addWidget(self._lbl_jt_calc_rot_x, 3, 1)
        grid_b.addWidget(QtWidgets.QLabel("　　 Y"), 4, 0); grid_b.addWidget(self._lbl_jt_calc_rot_y, 4, 1)
        grid_b.addWidget(QtWidgets.QLabel("　　 Z"), 5, 0); grid_b.addWidget(self._lbl_jt_calc_rot_z, 5, 1)
        grid_b.addWidget(QtWidgets.QLabel("　　 W"), 6, 0); grid_b.addWidget(self._lbl_jt_calc_rot_w, 6, 1)
        grid_b.setRowStretch(7, 1)

        top_layout.addWidget(gb_a, 1)
        top_layout.addWidget(gb_b, 1)
        detail_layout.addLayout(top_layout, 9)

        bottom_layout = QtWidgets.QHBoxLayout()
        bottom_layout.setContentsMargins(0, 0, 0, 0)
        bottom_layout.setSpacing(0)

        gb_c = QtWidgets.QGroupBox("C")
        grid_c = QtWidgets.QGridLayout(gb_c)
        grid_c.setContentsMargins(6, 6, 6, 6)
        grid_c.setHorizontalSpacing(15)
        grid_c.setVerticalSpacing(2)
        grid_c.setColumnStretch(1, 1)
        grid_c.addWidget(QtWidgets.QLabel("位置 minX"), 0, 0);  grid_c.addWidget(self._lbl_jt_move_lo_x, 0, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 minY"), 1, 0);  grid_c.addWidget(self._lbl_jt_move_lo_y, 1, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 minZ"), 2, 0);  grid_c.addWidget(self._lbl_jt_move_lo_z, 2, 1)
        grid_c.addWidget(QtWidgets.QLabel("位置 maxX"), 3, 0);  grid_c.addWidget(self._lbl_jt_move_hi_x, 3, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 maxY"), 4, 0);  grid_c.addWidget(self._lbl_jt_move_hi_y, 4, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 maxZ"), 5, 0);  grid_c.addWidget(self._lbl_jt_move_hi_z, 5, 1)
        grid_c.addWidget(QtWidgets.QLabel("回転 minX"), 6, 0);  grid_c.addWidget(self._lbl_jt_angle_lo_x, 6, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 minY"), 7, 0);  grid_c.addWidget(self._lbl_jt_angle_lo_y, 7, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 minZ"), 8, 0);  grid_c.addWidget(self._lbl_jt_angle_lo_z, 8, 1)
        grid_c.addWidget(QtWidgets.QLabel("回転 maxX"), 9, 0);  grid_c.addWidget(self._lbl_jt_angle_hi_x, 9, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 maxY"), 10, 0); grid_c.addWidget(self._lbl_jt_angle_hi_y, 10, 1)
        grid_c.addWidget(QtWidgets.QLabel("　　 maxZ"), 11, 0); grid_c.addWidget(self._lbl_jt_angle_hi_z, 11, 1)
        grid_c.setRowStretch(12, 1)

        gb_d = QtWidgets.QGroupBox("D")
        grid_d = QtWidgets.QGridLayout(gb_d)
        grid_d.setContentsMargins(6, 6, 6, 6)
        grid_d.setHorizontalSpacing(15)
        grid_d.setVerticalSpacing(2)
        grid_d.setColumnStretch(1, 1)
        grid_d.addWidget(QtWidgets.QLabel("位置 X"), 0, 0); grid_d.addWidget(self._lbl_jt_spm_x, 0, 1)
        grid_d.addWidget(QtWidgets.QLabel("　　 Y"), 1, 0); grid_d.addWidget(self._lbl_jt_spm_y, 1, 1)
        grid_d.addWidget(QtWidgets.QLabel("　　 Z"), 2, 0); grid_d.addWidget(self._lbl_jt_spm_z, 2, 1)
        grid_d.addWidget(QtWidgets.QLabel("回転 X"), 3, 0); grid_d.addWidget(self._lbl_jt_spr_x, 3, 1)
        grid_d.addWidget(QtWidgets.QLabel("　　 Y"), 4, 0); grid_d.addWidget(self._lbl_jt_spr_y, 4, 1)
        grid_d.addWidget(QtWidgets.QLabel("　　 Z"), 5, 0); grid_d.addWidget(self._lbl_jt_spr_z, 5, 1)
        grid_d.setRowStretch(6, 1)

        bottom_layout.addWidget(gb_c, 1)
        bottom_layout.addWidget(gb_d, 1)
        detail_layout.addLayout(bottom_layout, 11)

        scroll = QtWidgets.QScrollArea()
        scroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        scroll.setWidget(detail_widget)
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        splitter.addWidget(scroll)
        splitter.setSizes([144, 506])

    # ------------------------------------------------------------------
    #  ドラッグ&ドロップ
    # ------------------------------------------------------------------

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            for url in event.mimeData().urls():
                if url.toLocalFile().lower().endswith(".pmx"):
                    self._drag_hover = True
                    self._drag_overlay.setGeometry(self.rect())
                    self._drag_overlay.raise_()
                    self._drag_overlay.show()
                    event.acceptProposedAction()
                    return
        event.ignore()

    def dragLeaveEvent(self, event):
        self._drag_hover = False
        self._drag_overlay.hide()
        event.accept()

    def dropEvent(self, event):
        self._drag_hover = False
        self._drag_overlay.hide()
        for url in event.mimeData().urls():
            p = url.toLocalFile()
            if p.lower().endswith(".pmx"):
                Operator.set_pmx(self, p)
                event.acceptProposedAction()
                return
        event.ignore()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._drag_overlay.setGeometry(self.rect())
        if self._drag_hover:
            self._drag_overlay.raise_()

    # ------------------------------------------------------------------
    #  データ適用
    # ------------------------------------------------------------------

    def _apply_ik(self, ik_list):
        self._ik_data = ik_list or []
        self._ik_list.clear()
        self._current_ik_links = []
        self._lbl_ik_bone_class.setText("-")
        self._lbl_ik_bone_index.setText("-")
        self._lbl_ikbone.setText("-")
        self._lbl_target.setText("-")
        self._lbl_loop.setText("-")
        self._lbl_angle.setText("-")
        self._ik_link_list.clear()
        self._set_ik_link_detail_none()
        for r in self._ik_data:
            self._ik_list.addItem(str(r.get("ik_bone", "")))
        if self._ik_data:
            self._ik_list.setCurrentRow(0)

    def _apply_grants(self, grants):
        self._grant_data = grants or []
        self._grant_list.clear()
        self._set_grant_detail_none()
        for g in self._grant_data:
            self._grant_list.addItem(str(g.get("bone_name", "")))
        if self._grant_data:
            self._grant_list.setCurrentRow(0)

    def _apply_rigid_bodies(self, rbs):
        self._clear_physics()
        self._rb_data = rbs
        for i, rb in enumerate(rbs):
            self._rb_list.addItem(f"{i}: {rb.get('rb_name') or '(no name)'}")
        if rbs:
            self._rb_list.setCurrentRow(0)

    def _apply_joints(self, joints):
        self._joint_data = joints
        self._jt_list.clear()
        for i, jt in enumerate(joints):
            self._jt_list.addItem(f"{i}: {jt.get('joint_name') or '(no name)'}")
        if joints:
            self._jt_list.setCurrentRow(0)

    def _clear_physics(self):
        self._rb_data = []
        self._rb_list.clear()
        for lbl in (self._lbl_rb_name, self._lbl_rb_bone, self._lbl_rb_type,
                    self._lbl_rb_shape, self._lbl_rb_group, self._lbl_rb_pass_group,
                    self._lbl_rb_radius, self._lbl_rb_width, self._lbl_rb_height,
                    self._lbl_rb_depth, self._lbl_rb_pos_x, self._lbl_rb_pos_y,
                    self._lbl_rb_pos_z, self._lbl_rb_rot_x, self._lbl_rb_rot_y,
                    self._lbl_rb_rot_z, self._lbl_rb_rot_w, self._lbl_rb_mass,
                    self._lbl_rb_linear_damping, self._lbl_rb_angular_damping,
                    self._lbl_rb_restitution, self._lbl_rb_friction):
            lbl.setText("-")
        self._joint_data = []
        self._jt_list.clear()
        for lbl in (self._lbl_jt_name, self._lbl_jt_kind, self._lbl_jt_body_a, self._lbl_jt_body_b,
                    self._lbl_jt_w_posx, self._lbl_jt_w_posy, self._lbl_jt_w_posz,
                    self._lbl_jt_calc_pos_x, self._lbl_jt_calc_pos_y, self._lbl_jt_calc_pos_z,
                    self._lbl_jt_w_rot_x, self._lbl_jt_w_rot_y, self._lbl_jt_w_rot_z,
                    self._lbl_jt_calc_rot_x, self._lbl_jt_calc_rot_y, self._lbl_jt_calc_rot_z,
                    self._lbl_jt_calc_rot_w,
                    self._lbl_jt_move_lo_x, self._lbl_jt_move_lo_y, self._lbl_jt_move_lo_z,
                    self._lbl_jt_move_hi_x, self._lbl_jt_move_hi_y, self._lbl_jt_move_hi_z,
                    self._lbl_jt_angle_lo_x, self._lbl_jt_angle_lo_y, self._lbl_jt_angle_lo_z,
                    self._lbl_jt_angle_hi_x, self._lbl_jt_angle_hi_y, self._lbl_jt_angle_hi_z,
                    self._lbl_jt_spm_x, self._lbl_jt_spm_y, self._lbl_jt_spm_z,
                    self._lbl_jt_spr_x, self._lbl_jt_spr_y, self._lbl_jt_spr_z):
            lbl.setText("-")

    # ------------------------------------------------------------------
    #  詳細クリア
    # ------------------------------------------------------------------

    def _set_ik_link_detail_none(self):
        self._lbl_linkbone.setText("-")
        self._lbl_enable_limit_x.setText("-")
        self._lbl_enable_limit_y.setText("-")
        self._lbl_enable_limit_z.setText("-")
        self._lbl_xmin.setText("-")
        self._lbl_xmax.setText("-")
        self._lbl_ymin.setText("-")
        self._lbl_ymax.setText("-")
        self._lbl_zmin.setText("-")
        self._lbl_zmax.setText("-")

    def _set_grant_detail_none(self):
        self._lbl_gr_bone_class.setText("-")
        self._lbl_gr_bone_index.setText("-")
        self._lbl_gr_bone.setText("-")
        self._lbl_gr_parent_bone.setText("-")
        self._lbl_gr_weight.setText("-")
        self._lbl_gr_rotation.setText("-")
        self._lbl_gr_translation.setText("-")

    # ------------------------------------------------------------------
    #  イベントハンドラ
    # ------------------------------------------------------------------

    def _on_ik_selected(self, row):
        if row is None or row < 0 or row >= len(self._ik_data):
            self._lbl_ik_bone_class.setText("-")
            self._lbl_ik_bone_index.setText("-")
            self._lbl_ikbone.setText("-")
            self._lbl_target.setText("-")
            self._lbl_loop.setText("-")
            self._lbl_angle.setText("-")
            self._ik_link_list.clear()
            self._current_ik_links = []
            self._set_ik_link_detail_none()
            return
        r = self._ik_data[row]
        self._lbl_ik_bone_class.setText(str(r.get("BoneClass", r.get("bone_class", ""))))
        self._lbl_ik_bone_index.setText(str(r.get("BoneIndex", r.get("bone_index", ""))))
        self._lbl_ikbone.setText(str(r.get("ik_bone", "")))
        self._lbl_target.setText(str(r.get("target_bone", "")))
        self._lbl_loop.setText(str(r.get("loop", "")))
        self._lbl_angle.setText(str(r.get("angle", "")))
        self._ik_link_list.clear()
        links = r.get("links", []) or []
        self._current_ik_links = links
        for it in links:
            self._ik_link_list.addItem(str(it.get("link_bone", "")))
        if links:
            self._ik_link_list.setCurrentRow(0)
        else:
            self._set_ik_link_detail_none()

    def _on_ik_link_selected(self, row):
        if row is None or row < 0 or row >= len(self._current_ik_links):
            self._set_ik_link_detail_none()
            return
        it = self._current_ik_links[row]
        self._lbl_linkbone.setText(str(it.get("link_bone", "")))
        lim_x = bool(it.get("enable_lim_x", False))
        lim_y = bool(it.get("enable_lim_y", False))
        lim_z = bool(it.get("enable_lim_z", False))
        self._lbl_enable_limit_x.setText("ON" if lim_x else "OFF")
        self._lbl_enable_limit_y.setText("ON" if lim_y else "OFF")
        self._lbl_enable_limit_z.setText("ON" if lim_z else "OFF")
        vmin = it.get("min_lim", None)
        vmax = it.get("max_lim", None)
        if isinstance(vmin, list) and isinstance(vmax, list) and len(vmin) == 3 and len(vmax) == 3:
            self._lbl_xmin.setText(f"{vmin[0]:.2f}")
            self._lbl_ymin.setText(f"{vmin[1]:.2f}")
            self._lbl_zmin.setText(f"{vmin[2]:.2f}")
            self._lbl_xmax.setText(f"{vmax[0]:.2f}")
            self._lbl_ymax.setText(f"{vmax[1]:.2f}")
            self._lbl_zmax.setText(f"{vmax[2]:.2f}")
        else:
            self._lbl_xmin.setText("-"); self._lbl_ymin.setText("-"); self._lbl_zmin.setText("-")
            self._lbl_xmax.setText("-"); self._lbl_ymax.setText("-"); self._lbl_zmax.setText("-")

    def _on_grant_selected(self, row):
        if row is None or row < 0 or row >= len(self._grant_data):
            self._set_grant_detail_none()
            return
        g = self._grant_data[row]
        self._lbl_gr_bone_class.setText(str(g.get("BoneClass", g.get("bone_class", ""))))
        self._lbl_gr_bone_index.setText(str(g.get("BoneIndex", g.get("bone_index", ""))))
        self._lbl_gr_bone.setText(str(g.get("bone_name", "")))
        self._lbl_gr_parent_bone.setText(str(g.get("grant_bone_name", "")))
        self._lbl_gr_weight.setText(str(g.get("grant_weight", "")))
        self._lbl_gr_rotation.setText("ON" if g.get("grant_rotation") else "OFF")
        self._lbl_gr_translation.setText("ON" if g.get("grant_translation") else "OFF")

    def _on_rb_selection_changed(self, index):
        if index < 0 or index >= len(self._rb_data):
            return
        rb = self._rb_data[index]
        shape   = int(rb.get("shape", 0))
        rb_type = int(rb.get("type", 0))

        def fmt(v):
            try:
                return str(round(float(v), 7))
            except Exception:
                return str(v)

        type_names = {0: "0: ボーン追従", 1: "1: 物理演算", 2: "2: 物理+ボーン位置合わせ"}
        shape_names = {0: "0: 球", 1: "1: 箱", 2: "2: カプセル"}

        pass_group_value = rb.get("pass_group", "")
        try:
            mask = int(pass_group_value)
            pass_group_text = " ".join(str(i) for i in range(16) if (mask & (1 << i))) or "-"
        except Exception:
            pass_group_text = str(pass_group_value)

        self._lbl_rb_name.setText(str(rb.get("rb_name", "")))
        self._lbl_rb_bone.setText(str(rb.get("rb_bone_name", "")))
        self._lbl_rb_type.setText(type_names.get(rb_type, str(rb_type)))
        self._lbl_rb_shape.setText(shape_names.get(shape, str(shape)))
        self._lbl_rb_group.setText(str(int(rb.get("group", 0))))
        self._lbl_rb_pass_group.setText(pass_group_text)
        self._lbl_rb_radius.setText(fmt(rb.get("radius", "")))
        self._lbl_rb_width.setText(fmt(rb.get("width", "")))
        self._lbl_rb_height.setText(fmt(rb.get("height", "")))
        self._lbl_rb_depth.setText(fmt(rb.get("depth", "")))
        self._lbl_rb_pos_x.setText(fmt(rb.get("calc_pos_x", "")))
        self._lbl_rb_pos_y.setText(fmt(rb.get("calc_pos_y", "")))
        self._lbl_rb_pos_z.setText(fmt(rb.get("calc_pos_z", "")))
        self._lbl_rb_rot_x.setText(fmt(rb.get("calc_rot_x", "")))
        self._lbl_rb_rot_y.setText(fmt(rb.get("calc_rot_y", "")))
        self._lbl_rb_rot_z.setText(fmt(rb.get("calc_rot_z", "")))
        self._lbl_rb_rot_w.setText(fmt(rb.get("calc_rot_w", "")))
        self._lbl_rb_mass.setText(fmt(rb.get("mass", "")))
        self._lbl_rb_linear_damping.setText(fmt(rb.get("linear_damping", "")))
        self._lbl_rb_angular_damping.setText(fmt(rb.get("angular_damping", "")))
        self._lbl_rb_restitution.setText(fmt(rb.get("restitution", "")))
        self._lbl_rb_friction.setText(fmt(rb.get("friction", "")))

        show_radius = shape in (0, 2)
        show_box    = shape == 1
        show_height = shape in (1, 2)
        self._lbl_radius_label.setVisible(show_radius); self._lbl_rb_radius.setVisible(show_radius)
        self._lbl_width_label.setVisible(show_box);     self._lbl_rb_width.setVisible(show_box)
        self._lbl_height_label.setVisible(show_height); self._lbl_rb_height.setVisible(show_height)
        self._lbl_depth_label.setVisible(show_box);     self._lbl_rb_depth.setVisible(show_box)

    def _on_joint_selection_changed(self, index):
        if index < 0 or index >= len(self._joint_data):
            return
        jt = self._joint_data[index]

        def fmt(v):
            try:
                return str(round(float(v), 7))
            except Exception:
                return str(v)

        kind = int(jt.get("kind", 0))
        kind_names = {0: "0: 標準"}
        body_a_idx  = int(jt.get("body_a", -1))
        body_b_idx  = int(jt.get("body_b", -1))
        body_a_name = self._rb_data[body_a_idx]["rb_name"] if 0 <= body_a_idx < len(self._rb_data) else ""
        body_b_name = self._rb_data[body_b_idx]["rb_name"] if 0 <= body_b_idx < len(self._rb_data) else ""
        body_a_text = f"{body_a_idx}: {body_a_name}" if body_a_name else str(body_a_idx)
        body_b_text = f"{body_b_idx}: {body_b_name}" if body_b_name else str(body_b_idx)

        self._lbl_jt_name.setText(str(jt.get("joint_name", "")))
        self._lbl_jt_kind.setText(kind_names.get(kind, str(kind)))
        self._lbl_jt_body_a.setText(body_a_text)
        self._lbl_jt_body_b.setText(body_b_text)
        self._lbl_jt_w_posx.setText(fmt(jt.get("j_w_pos.x", "")))
        self._lbl_jt_w_posy.setText(fmt(jt.get("j_w_pos.y", "")))
        self._lbl_jt_w_posz.setText(fmt(jt.get("j_w_pos.z", "")))
        self._lbl_jt_calc_pos_x.setText(fmt(jt.get("calc_pos_x", "")))
        self._lbl_jt_calc_pos_y.setText(fmt(jt.get("calc_pos_y", "")))
        self._lbl_jt_calc_pos_z.setText(fmt(jt.get("calc_pos_z", "")))
        self._lbl_jt_w_rot_x.setText(fmt(jt.get("j_w_rot.x", "")))
        self._lbl_jt_w_rot_y.setText(fmt(jt.get("j_w_rot.y", "")))
        self._lbl_jt_w_rot_z.setText(fmt(jt.get("j_w_rot.z", "")))
        self._lbl_jt_calc_rot_x.setText(fmt(jt.get("calc_rot_x", "")))
        self._lbl_jt_calc_rot_y.setText(fmt(jt.get("calc_rot_y", "")))
        self._lbl_jt_calc_rot_z.setText(fmt(jt.get("calc_rot_z", "")))
        self._lbl_jt_calc_rot_w.setText(fmt(jt.get("calc_rot_w", "")))
        self._lbl_jt_move_lo_x.setText(fmt(jt.get("calc_move_lo_x", "")))
        self._lbl_jt_move_lo_y.setText(fmt(jt.get("calc_move_lo_y", "")))
        self._lbl_jt_move_lo_z.setText(fmt(jt.get("calc_move_lo_z", "")))
        self._lbl_jt_move_hi_x.setText(fmt(jt.get("calc_move_hi_x", "")))
        self._lbl_jt_move_hi_y.setText(fmt(jt.get("calc_move_hi_y", "")))
        self._lbl_jt_move_hi_z.setText(fmt(jt.get("calc_move_hi_z", "")))
        self._lbl_jt_angle_lo_x.setText(fmt(jt.get("calc_angle_lo_x", "")))
        self._lbl_jt_angle_lo_y.setText(fmt(jt.get("calc_angle_lo_y", "")))
        self._lbl_jt_angle_lo_z.setText(fmt(jt.get("calc_angle_lo_z", "")))
        self._lbl_jt_angle_hi_x.setText(fmt(jt.get("calc_angle_hi_x", "")))
        self._lbl_jt_angle_hi_y.setText(fmt(jt.get("calc_angle_hi_y", "")))
        self._lbl_jt_angle_hi_z.setText(fmt(jt.get("calc_angle_hi_z", "")))
        self._lbl_jt_spm_x.setText(fmt(jt.get("calc_spring_move_x", "")))
        self._lbl_jt_spm_y.setText(fmt(jt.get("calc_spring_move_y", "")))
        self._lbl_jt_spm_z.setText(fmt(jt.get("calc_spring_move_z", "")))
        self._lbl_jt_spr_x.setText(fmt(jt.get("calc_spring_rotate_x", "")))
        self._lbl_jt_spr_y.setText(fmt(jt.get("calc_spring_rotate_y", "")))
        self._lbl_jt_spr_z.setText(fmt(jt.get("calc_spring_rotate_z", "")))
