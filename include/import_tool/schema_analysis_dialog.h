#pragma once
#include "import_tool/connection_config.h"
#include <QWidget>
#include <QList>

class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QComboBox;

class SchemaAnalysisDialog : public QWidget {
    Q_OBJECT
public:
    explicit SchemaAnalysisDialog(QWidget* parent = nullptr);

private slots:
    void onAnalyze();
    void onFilterChanged(int index);

private:
    struct Item {
        QString object;
        QString comment;
    };

    struct Result {
        QString schemaName;
        QString schemaComment;

        int totalTables        = 0;
        int tablesWithComment  = 0;
        int tablesRussianOnly  = 0;
        int tablesHasLatin     = 0;

        int totalColumns        = 0;
        int columnsWithComment  = 0;
        int columnsRussianOnly  = 0;
        int columnsHasLatin     = 0;

        QList<Item> tablesNoComment;
        QList<Item> tablesWithLatin;
        QList<Item> columnsNoComment;
        QList<Item> columnsWithLatin;
    };

    ConnectionConfig readDbConfig() const;
    void saveSettings();
    void loadSettings();
    void updateStats(const Result& r);
    void populateProblems(int filterIndex);

    static bool   hasLatin(const QString& s);
    static bool   hasCyrillic(const QString& s);
    static QString pct(int n, int total);

    // Connection
    QLineEdit* m_host     = nullptr;
    QSpinBox*  m_port     = nullptr;
    QLineEdit* m_database = nullptr;
    QLineEdit* m_user     = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_schema   = nullptr;

    QPushButton*  m_analyzeBtn  = nullptr;
    QLabel*       m_statsLabel  = nullptr;
    QComboBox*    m_filterCombo = nullptr;
    QTableWidget* m_table       = nullptr;

    Result m_result;
};
