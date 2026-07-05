from PySide6 import QtWidgets

def apply(widget: QtWidgets.QWidget):
    widget.setStyleSheet("""
            QWidget {
                font-family: "Segoe UI", "Yu Gothic UI", "Meiryo UI", sans-serif;
                font-size: 9pt;
                background-color: #222222;
                color: #c8cdd6;
            }
            QDialog {
                background-color: #222222;
            }
            QLabel {
                color: #c8cdd6;
                background: transparent;
            }
            QTabWidget::pane {
                border: 0;
                top: 0;
            }
            QTabBar::tab {
                background: #333333;
                color: #8a9099;
                padding: 7px 25px;
                border: 0;
                min-width: 60px;
            }
            QTabBar::tab:selected {
                background: #222222;
                color: #d8dce4;
                border-bottom: 2px solid #4a6fa5;
            }
            QTabBar::tab:hover:!selected {
                background: #3a3a3a;
                color: #b0b8c4;
            }
            QGroupBox {
                font-weight: 600;
                border: 1px solid #3d3d3d;
                border-radius: 0;
                margin: 0;
                padding: 9px 0 0 0;
                background-color: #222222;
                color: #c8cdd6;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 8px;
                padding: 0 3px;
                color: #6a8fc0;
                background-color: #222222;
            }
            QPushButton {
                background-color: #2e4a70;
                color: #d8dce4;
                border: none;
                border-radius: 0;
                padding: 6px 14px;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: #3a5a84;
            }
            QPushButton:pressed {
                background-color: #243d5c;
            }
            QPushButton:disabled {
                background-color: #333333;
                color: #666666;
            }
            QLineEdit {
                background-color: #222222;
                border: 1px solid #3d3d3d;
                border-radius: 0;
                padding: 5px 8px;
                color: #c8cdd6;
                selection-background-color: #2e4a70;
            }
            QLineEdit:focus {
                border: 1px solid #4a6fa5;
            }
            QListWidget {
                background-color: #222222;
                border: 0;
                outline: 0;
                padding: 0;
                margin: 0;
            }
            QListWidget::item {
                margin: 0;
                padding: 0 8px;
                border: 0;
                color: #b0b8c4;
                background: transparent;
            }
            QListWidget::item:hover {
                background-color: #2e2e2e;
                color: #d8dce4;
            }
            QListWidget::item:selected {
                background-color: #383838;
                color: #d8dce4;
                border-left: 2px solid #4a6fa5;
                padding-left: 6px;
            }
            QScrollArea {
                border: 0;
                background: transparent;
            }
            QScrollBar:vertical {
                background: #222222;
                width: 9px;
                margin: 0;
                border: 0;
            }
            QScrollBar::handle:vertical {
                background: #3d3d3d;
                min-height: 24px;
                border-radius: 0;
            }
            QScrollBar::handle:vertical:hover {
                background: #4d4d4d;
            }
            QScrollBar::add-line:vertical,
            QScrollBar::sub-line:vertical,
            QScrollBar::add-page:vertical,
            QScrollBar::sub-page:vertical {
                background: none;
                border: 0;
                height: 0;
            }
            QSplitter::handle {
                background: transparent;
            }
            QMenu {
                background-color: #222222;
                border: 1px solid #3d3d3d;
                padding: 2px 0;
            }
            QMenu::item {
                color: #c8cdd6;
                padding: 5px 32px 5px 16px;
            }
            QMenu::item:selected {
                background-color: #2e4a70;
                color: #d8dce4;
            }
    """)
