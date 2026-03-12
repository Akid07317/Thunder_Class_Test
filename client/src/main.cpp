#include <QApplication>
#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Thunder Class");

    app.setStyleSheet(R"(
        QMainWindow {
            background-color: #f5f6fa;
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid #dcdde1;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 6px;
        }
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 13px;
            min-height: 24px;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #2471a3;
        }
        QPushButton:disabled {
            background-color: #bdc3c7;
            color: #7f8c8d;
        }
        QPushButton#loginBtn, QPushButton#connectBtn {
            background-color: #27ae60;
            font-size: 14px;
            padding: 8px 24px;
        }
        QPushButton#loginBtn:hover, QPushButton#connectBtn:hover {
            background-color: #219a52;
        }
        QLineEdit, QSpinBox, QComboBox {
            border: 1px solid #bdc3c7;
            border-radius: 4px;
            padding: 5px 8px;
            background-color: white;
            font-size: 13px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #3498db;
        }
        QTableWidget {
            border: 1px solid #dcdde1;
            border-radius: 4px;
            gridline-color: #ecf0f1;
            background-color: white;
            alternate-background-color: #f8f9fa;
        }
        QTableWidget::item {
            padding: 4px;
        }
        QHeaderView::section {
            background-color: #ecf0f1;
            padding: 6px;
            border: none;
            border-bottom: 1px solid #bdc3c7;
            font-weight: bold;
        }
        QTextEdit {
            border: 1px solid #dcdde1;
            border-radius: 4px;
            background-color: white;
        }
        QListWidget {
            border: 1px solid #dcdde1;
            border-radius: 4px;
            background-color: white;
        }
        QLabel#loginTitle {
            font-size: 28px;
            font-weight: bold;
            color: #2c3e50;
        }
        QLabel#errorLabel {
            color: #e74c3c;
            font-size: 12px;
        }
        QStatusBar {
            background-color: #ecf0f1;
            color: #2c3e50;
        }
        QRadioButton {
            spacing: 6px;
        }
        QScrollArea {
            border: none;
        }
    )");

    MainWindow window;
    window.show();

    return app.exec();
}
