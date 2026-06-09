#pragma once
#include "import_tool/connection_config.h"
#include <QWidget>
#include <QList>
#include <QMap>

class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QComboBox;
class QListWidget;
class QSqlDatabase;

class SchemaAnalysisDialog : public QWidget {
    Q_OBJECT
public:
    explicit SchemaAnalysisDialog(QWidget* parent = nullptr);

    enum class Health { Good, Warning, Bad };

private slots:
    void onTestConnection();
    void onAnalyze();
    void onSchemaSelected();
    void onFilterChanged(int index);

private:
    struct Item {
        QString object;
        QString comment;
    };

    struct Result {
        QString schemaName;
        QString schemaComment;

        int totalTables       = 0;
        int tablesWithComment = 0;
        int tablesRussianOnly = 0;
        int tablesHasLatin    = 0;

        int totalColumns       = 0;
        int columnsWithComment = 0;
        int columnsRussianOnly = 0;
        int columnsHasLatin    = 0;

        QList<Item> tablesNoComment;
        QList<Item> tablesWithLatin;
        QList<Item> columnsNoComment;
        QList<Item> columnsWithLatin;
    };

    ConnectionConfig readDbConfig() const;
    void saveSettings();
    void loadSettings();
    void analyzeSchema(QSqlDatabase& db, const QString& schema, Result& r);
    void updateStats(const Result& r);
    void populateProblems(int filterIndex);
    void updateFilterLabels(const Result& r);
    Health schemaHealth(const Result& r) const;

    static bool    hasLatin(const QString& s);
    static bool    hasCyrillic(const QString& s);
    static QString pct(int n, int total);

    // Connection (no schema field — all schemas are scanned)
    QLineEdit* m_host     = nullptr;
    QSpinBox*  m_port     = nullptr;
    QLineEdit* m_database = nullptr;
    QLineEdit* m_user     = nullptr;
    QLineEdit* m_password = nullptr;

    QPushButton* m_testConnBtn     = nullptr;
    QLabel*      m_connStatusLabel = nullptr;

    QPushButton* m_analyzeBtn = nullptr;

    // Left panel
    QListWidget* m_schemaList = nullptr;

    // Right panel
    QLabel*       m_statsLabel  = nullptr;
    QComboBox*    m_filterCombo = nullptr;
    QTableWidget* m_table       = nullptr;

    QMap<QString, Result> m_results;
};
