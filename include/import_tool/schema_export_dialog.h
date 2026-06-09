#pragma once
#include "import_tool/connection_config.h"
#include <QWidget>

class QLineEdit;
class QSpinBox;
class QTextEdit;
class QPushButton;
class QCheckBox;

class SchemaExportDialog : public QWidget {
    Q_OBJECT
public:
    explicit SchemaExportDialog(QWidget* parent = nullptr);

private slots:
    void onBrowseDir();
    void onCopyDir();
    void onGenerateReport();
    void onCopyFilePath();

private:
    ConnectionConfig readDbConfig() const;
    void loadSettings();
    void saveSettings();
    void log(const QString& msg);
    void setLastFile(const QString& path);
    QStringList runQuery(const ConnectionConfig& cfg, const QString& sql);
    QString buildRtfReport(const ConnectionConfig& cfg, bool includeFk);

    QLineEdit*   m_host     = nullptr;
    QSpinBox*    m_port     = nullptr;
    QLineEdit*   m_database = nullptr;
    QLineEdit*   m_user     = nullptr;
    QLineEdit*   m_password = nullptr;
    QLineEdit*   m_schema   = nullptr;

    QLineEdit*   m_outDir     = nullptr;
    QPushButton* m_browseBtn  = nullptr;
    QPushButton* m_copyDirBtn = nullptr;

    QLineEdit*   m_lastFilePath = nullptr;
    QPushButton* m_copyFileBtn  = nullptr;

    QCheckBox*   m_includeFkCheck = nullptr;

    QPushButton* m_generateRptBtn = nullptr;

    QTextEdit*   m_logArea = nullptr;
};
