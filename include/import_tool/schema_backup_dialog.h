#pragma once
#include "import_tool/connection_config.h"
#include <QWidget>

class QLineEdit;
class QSpinBox;
class QTextEdit;
class QPushButton;

class SchemaBackupDialog : public QWidget {
    Q_OBJECT
public:
    explicit SchemaBackupDialog(QWidget* parent = nullptr);

private slots:
    void onBrowseOutDir();
    void onExportBoth();
    void onBrowseScript1();
    void onBrowseScript2();
    void onApplyScript1();
    void onApplyScript2();

private:
    ConnectionConfig readDbConfig() const;
    void loadSettings();
    void saveSettings();
    void log(const QString& msg);
    bool applyScript(const QString& filepath, const ConnectionConfig& cfg);

    // Connection
    QLineEdit* m_host     = nullptr;
    QSpinBox*  m_port     = nullptr;
    QLineEdit* m_database = nullptr;
    QLineEdit* m_user     = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_schema   = nullptr;

    // Export
    QLineEdit*   m_outDir      = nullptr;
    QPushButton* m_browseOutBtn = nullptr;
    QPushButton* m_exportBtn    = nullptr;

    // Import script 1
    QLineEdit*   m_script1Path    = nullptr;
    QPushButton* m_browseScript1  = nullptr;
    QPushButton* m_applyScript1Btn = nullptr;

    // Import script 2
    QLineEdit*   m_script2Path    = nullptr;
    QPushButton* m_browseScript2  = nullptr;
    QPushButton* m_applyScript2Btn = nullptr;

    // Log
    QTextEdit* m_logArea = nullptr;
};
