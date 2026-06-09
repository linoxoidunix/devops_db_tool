#include "import_tool/schema_analysis_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QHeaderView>
#include <QSettings>
#include <QMessageBox>
#include <QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <cmath>

static const QString ANALYSIS_CONN = "schema_analysis_conn";

// ── ctor ──────────────────────────────────────────────────────────────────────

SchemaAnalysisDialog::SchemaAnalysisDialog(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    // ── Connection (no schema field — all schemas are scanned) ────────────────
    auto* connGroup = new QGroupBox("Подключение к БД", this);
    auto* connForm  = new QFormLayout(connGroup);

    m_host     = new QLineEdit("127.0.0.1", this);
    m_port     = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(5432);
    m_database = new QLineEdit(this);
    m_user     = new QLineEdit("postgres", this);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);

    auto* hpRow = new QHBoxLayout;
    hpRow->addWidget(m_host, 3);
    hpRow->addWidget(new QLabel("Порт:", this));
    hpRow->addWidget(m_port, 1);

    m_testConnBtn     = new QPushButton("Проверить соединение", this);
    m_connStatusLabel = new QLabel(this);
    m_connStatusLabel->setTextFormat(Qt::RichText);
    auto* testRow = new QHBoxLayout;
    testRow->addWidget(m_testConnBtn);
    testRow->addWidget(m_connStatusLabel, 1);

    connForm->addRow("Host:",         hpRow);
    connForm->addRow("База данных:",  m_database);
    connForm->addRow("Пользователь:", m_user);
    connForm->addRow("Пароль:",       m_password);
    connForm->addRow(testRow);
    root->addWidget(connGroup);

    // ── Analyse button ────────────────────────────────────────────────────────
    m_analyzeBtn = new QPushButton("Анализировать", this);
    m_analyzeBtn->setMinimumHeight(36);
    m_analyzeBtn->setStyleSheet("QPushButton { font-weight: bold; }");
    root->addWidget(m_analyzeBtn);

    // ── Main split: left (schema list) + right (stats + problems) ────────────
    auto* splitLayout = new QHBoxLayout;

    // Left — schema list
    auto* leftGroup  = new QGroupBox("Схемы", this);
    auto* leftLayout = new QVBoxLayout(leftGroup);
    m_schemaList = new QListWidget(this);
    m_schemaList->setMinimumWidth(150);
    m_schemaList->setMaximumWidth(230);
    leftLayout->addWidget(m_schemaList);
    splitLayout->addWidget(leftGroup);

    // Right — stats + problems
    auto* rightWidget = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* statsGroup  = new QGroupBox("Статистика", rightWidget);
    auto* statsLayout = new QVBoxLayout(statsGroup);
    m_statsLabel = new QLabel("Выберите схему слева.", this);
    m_statsLabel->setTextFormat(Qt::RichText);
    m_statsLabel->setWordWrap(true);
    m_statsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_statsLabel->setStyleSheet("QLabel { font-family: monospace; font-size: 10pt; }");
    statsLayout->addWidget(m_statsLabel);
    rightLayout->addWidget(statsGroup);

    auto* probGroup  = new QGroupBox("Проблемные объекты", rightWidget);
    auto* probLayout = new QVBoxLayout(probGroup);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("Таблицы без комментария");
    m_filterCombo->addItem("Таблицы с латинскими буквами");
    m_filterCombo->addItem("Колонки без комментария");
    m_filterCombo->addItem("Колонки с латинскими буквами");

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({"Объект", "Комментарий"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setMinimumHeight(180);

    probLayout->addWidget(m_filterCombo);
    probLayout->addWidget(m_table, 1);
    rightLayout->addWidget(probGroup, 1);

    splitLayout->addWidget(rightWidget, 1);
    root->addLayout(splitLayout, 1);

    connect(m_testConnBtn, &QPushButton::clicked,
            this, &SchemaAnalysisDialog::onTestConnection);
    connect(m_analyzeBtn, &QPushButton::clicked,
            this, &SchemaAnalysisDialog::onAnalyze);
    connect(m_schemaList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem*, QListWidgetItem*) { onSchemaSelected(); });
    connect(m_filterCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SchemaAnalysisDialog::onFilterChanged);

    loadSettings();
}

// ── Persistence ───────────────────────────────────────────────────────────────

void SchemaAnalysisDialog::loadSettings() {
    QSettings s;
    s.beginGroup("SchemaAnalysis");
    m_host->setText(s.value("host",  "127.0.0.1").toString());
    m_port->setValue(s.value("port", 5432).toInt());
    m_database->setText(s.value("database").toString());
    m_user->setText(s.value("user",  "postgres").toString());
    s.endGroup();
}

void SchemaAnalysisDialog::saveSettings() {
    QSettings s;
    s.beginGroup("SchemaAnalysis");
    s.setValue("host",     m_host->text());
    s.setValue("port",     m_port->value());
    s.setValue("database", m_database->text());
    s.setValue("user",     m_user->text());
    s.endGroup();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

bool SchemaAnalysisDialog::hasLatin(const QString& s) {
    static const QRegularExpression re("[A-Za-z]");
    return re.match(s).hasMatch();
}

bool SchemaAnalysisDialog::hasCyrillic(const QString& s) {
    for (const QChar& c : s)
        if (c.unicode() >= 0x0400 && c.unicode() <= 0x04FF)
            return true;
    return false;
}

QString SchemaAnalysisDialog::pct(int n, int total) {
    if (total == 0) return "—";
    return QString("%1%").arg(qRound(100.0 * n / total));
}

ConnectionConfig SchemaAnalysisDialog::readDbConfig() const {
    ConnectionConfig cfg;
    cfg.host     = m_host->text().trimmed();
    cfg.port     = m_port->value();
    cfg.database = m_database->text().trimmed();
    cfg.user     = m_user->text().trimmed();
    cfg.password = m_password->text();
    return cfg;
}

SchemaAnalysisDialog::Health SchemaAnalysisDialog::schemaHealth(const Result& r) const {
    if (r.tablesNoComment.isEmpty() && r.columnsNoComment.isEmpty() &&
        r.tablesWithLatin.isEmpty() && r.columnsWithLatin.isEmpty())
        return Health::Good;

    const int tPct = r.totalTables  > 0 ? r.tablesWithComment  * 100 / r.totalTables  : 100;
    const int cPct = r.totalColumns > 0 ? r.columnsWithComment * 100 / r.totalColumns : 100;
    if (tPct >= 80 && cPct >= 80)
        return Health::Warning;
    return Health::Bad;
}

static QColor healthBgColor(SchemaAnalysisDialog::Health h) {
    switch (h) {
        case SchemaAnalysisDialog::Health::Good:    return QColor(180, 240, 180);
        case SchemaAnalysisDialog::Health::Warning: return QColor(255, 215, 100);
        case SchemaAnalysisDialog::Health::Bad:     return QColor(255, 150, 150);
    }
    return {};
}

// ── Test connection ───────────────────────────────────────────────────────────

void SchemaAnalysisDialog::onTestConnection() {
    const auto cfg = readDbConfig();
    m_connStatusLabel->setText("Проверка…");
    m_testConnBtn->setEnabled(false);
    QApplication::processEvents();

    static const QString TEST_CONN = "schema_analysis_test_conn";
    if (QSqlDatabase::contains(TEST_CONN))
        QSqlDatabase::removeDatabase(TEST_CONN);

    {
        auto db = QSqlDatabase::addDatabase("QPSQL", TEST_CONN);
        db.setHostName(cfg.host);
        db.setPort(cfg.port);
        db.setDatabaseName(cfg.database);
        db.setUserName(cfg.user);
        db.setPassword(cfg.password);

        if (db.open()) {
            m_connStatusLabel->setText("<span style='color:green'>✔ Соединение успешно</span>");
            db.close();
        } else {
            m_connStatusLabel->setText(
                QString("<span style='color:red'>✗ %1</span>")
                .arg(db.lastError().text().toHtmlEscaped()));
        }
    }
    QSqlDatabase::removeDatabase(TEST_CONN);
    m_testConnBtn->setEnabled(true);
}

// ── Per-schema analysis ───────────────────────────────────────────────────────

void SchemaAnalysisDialog::analyzeSchema(QSqlDatabase& db, const QString& schema, Result& r) {
    r.schemaName = schema;

    {
        QSqlQuery q(db);
        q.prepare(
            "SELECT COALESCE(obj_description(n.oid,'pg_namespace'),'')"
            " FROM pg_namespace n WHERE n.nspname = :s");
        q.bindValue(":s", schema);
        if (q.exec() && q.next())
            r.schemaComment = q.value(0).toString().trimmed();
    }

    {
        QSqlQuery q(db);
        q.prepare(
            "SELECT t.table_name,"
            "       COALESCE(obj_description(pc.oid,'pg_class'),'')"
            " FROM information_schema.tables t"
            " JOIN pg_catalog.pg_class pc ON pc.relname = t.table_name"
            " JOIN pg_catalog.pg_namespace pn"
            "      ON pn.oid = pc.relnamespace AND pn.nspname = t.table_schema"
            " WHERE t.table_schema = :s AND t.table_type = 'BASE TABLE'"
            " ORDER BY t.table_name");
        q.bindValue(":s", schema);
        q.exec();
        while (q.next()) {
            const QString name    = q.value(0).toString();
            const QString comment = q.value(1).toString().trimmed();
            ++r.totalTables;
            if (!comment.isEmpty()) {
                ++r.tablesWithComment;
                if (hasCyrillic(comment) && !hasLatin(comment))
                    ++r.tablesRussianOnly;
                if (hasLatin(comment)) {
                    ++r.tablesHasLatin;
                    r.tablesWithLatin.append({name, comment});
                }
            } else {
                r.tablesNoComment.append({name, {}});
            }
        }
    }

    {
        QSqlQuery q(db);
        q.prepare(
            "SELECT c.table_name, c.column_name,"
            "       COALESCE(pgd.description,'')"
            " FROM information_schema.columns c"
            " JOIN pg_catalog.pg_class pc ON pc.relname = c.table_name"
            " JOIN pg_catalog.pg_namespace pn"
            "      ON pn.oid = pc.relnamespace AND pn.nspname = c.table_schema"
            " LEFT JOIN pg_catalog.pg_description pgd"
            "      ON pgd.objoid = pc.oid AND pgd.objsubid = c.ordinal_position"
            " WHERE c.table_schema = :s"
            " ORDER BY c.table_name, c.ordinal_position");
        q.bindValue(":s", schema);
        q.exec();
        while (q.next()) {
            const QString obj     = q.value(0).toString() + "." + q.value(1).toString();
            const QString comment = q.value(2).toString().trimmed();
            ++r.totalColumns;
            if (!comment.isEmpty()) {
                ++r.columnsWithComment;
                if (hasCyrillic(comment) && !hasLatin(comment))
                    ++r.columnsRussianOnly;
                if (hasLatin(comment)) {
                    ++r.columnsHasLatin;
                    r.columnsWithLatin.append({obj, comment});
                }
            } else {
                r.columnsNoComment.append({obj, {}});
            }
        }
    }
}

// ── Main analyse slot ─────────────────────────────────────────────────────────

void SchemaAnalysisDialog::onAnalyze() {
    const auto cfg = readDbConfig();
    if (cfg.database.isEmpty()) {
        QMessageBox::warning(this, "Анализ", "Укажите базу данных.");
        return;
    }

    saveSettings();
    m_analyzeBtn->setEnabled(false);
    m_schemaList->clear();
    m_results.clear();
    m_statsLabel->setText("Подключение…");
    m_table->setRowCount(0);
    QApplication::processEvents();

    if (QSqlDatabase::contains(ANALYSIS_CONN))
        QSqlDatabase::removeDatabase(ANALYSIS_CONN);

    {
        auto db = QSqlDatabase::addDatabase("QPSQL", ANALYSIS_CONN);
        db.setHostName(cfg.host);
        db.setPort(cfg.port);
        db.setDatabaseName(cfg.database);
        db.setUserName(cfg.user);
        db.setPassword(cfg.password);

        if (!db.open()) {
            m_statsLabel->setText(
                QString("<b style='color:red'>Ошибка подключения:</b> %1")
                .arg(db.lastError().text().toHtmlEscaped()));
            m_analyzeBtn->setEnabled(true);
            QSqlDatabase::removeDatabase(ANALYSIS_CONN);
            return;
        }

        // Collect all user schemas
        QStringList schemas;
        {
            QSqlQuery q(db);
            q.exec(
                "SELECT nspname FROM pg_namespace"
                " WHERE nspname NOT IN ('pg_catalog','information_schema','pg_toast')"
                "   AND nspname NOT LIKE 'pg_%'"
                " ORDER BY nspname");
            while (q.next())
                schemas << q.value(0).toString();
        }

        if (schemas.isEmpty()) {
            m_statsLabel->setText("Пользовательские схемы не найдены.");
            db.close();
            QSqlDatabase::removeDatabase(ANALYSIS_CONN);
            m_analyzeBtn->setEnabled(true);
            return;
        }

        for (const QString& schema : schemas) {
            m_statsLabel->setText(QString("Анализ схемы «%1»…").arg(schema));
            QApplication::processEvents();
            Result r;
            analyzeSchema(db, schema, r);
            m_results[schema] = std::move(r);
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(ANALYSIS_CONN);

    // Populate schema list with health colors
    for (auto it = m_results.cbegin(); it != m_results.cend(); ++it) {
        const Health h = schemaHealth(it.value());
        QString prefix;
        switch (h) {
            case Health::Good:    prefix = "✓  "; break;
            case Health::Warning: prefix = "⚠  "; break;
            case Health::Bad:     prefix = "✗  "; break;
        }
        auto* item = new QListWidgetItem(prefix + it.key());
        item->setBackground(healthBgColor(h));
        item->setData(Qt::UserRole, it.key());
        m_schemaList->addItem(item);
    }

    m_analyzeBtn->setEnabled(true);

    // Auto-select: first bad schema, otherwise first item
    int selectRow = 0;
    for (int i = 0; i < m_schemaList->count(); ++i) {
        const QString name = m_schemaList->item(i)->data(Qt::UserRole).toString();
        if (schemaHealth(m_results.value(name)) == Health::Bad) {
            selectRow = i;
            break;
        }
    }
    m_schemaList->setCurrentRow(selectRow);
}

// ── Schema selected ───────────────────────────────────────────────────────────

void SchemaAnalysisDialog::onSchemaSelected() {
    const auto* item = m_schemaList->currentItem();
    if (!item) return;

    const QString schema = item->data(Qt::UserRole).toString();
    if (!m_results.contains(schema)) return;

    const Result& r = m_results[schema];
    updateStats(r);
    updateFilterLabels(r);
    populateProblems(m_filterCombo->currentIndex());
}

// ── Display ───────────────────────────────────────────────────────────────────

void SchemaAnalysisDialog::updateStats(const Result& r) {
    auto statusSpan = [](bool good, const QString& text) -> QString {
        return QString("<span style='color:%1'>%2</span>")
            .arg(good ? "green" : "red", text);
    };

    auto statRow = [](const QString& label, int n, int total) -> QString {
        const QString p   = SchemaAnalysisDialog::pct(n, total);
        const bool ok     = (n == total);
        const QString color = ok ? "green"
                            : (total > 0 && n * 100 / total >= 80 ? "darkorange" : "red");
        return QString(
            "<tr>"
            "<td>%1</td>"
            "<td align='right'><b>%2</b>&nbsp;/&nbsp;%3</td>"
            "<td>&nbsp;<span style='color:%4'>(%5)</span></td>"
            "</tr>")
            .arg(label).arg(n).arg(total).arg(color).arg(p);
    };

    const QString schComment = r.schemaComment.isEmpty()
        ? statusSpan(false, "пустой")
        : statusSpan(true,  "заполнен");
    const QString schLatin = (!r.schemaComment.isEmpty() && hasLatin(r.schemaComment))
        ? "&nbsp;&nbsp;<span style='color:darkorange'>⚠ содержит латиницу</span>"
        : "";

    QString html;
    html += QString("<b>Схема «%1»</b><br>").arg(r.schemaName.toHtmlEscaped());
    html += QString("&nbsp;&nbsp;Комментарий: %1%2<br>").arg(schComment, schLatin);
    if (!r.schemaComment.isEmpty())
        html += QString("&nbsp;&nbsp;<i>«%1»</i>").arg(r.schemaComment.toHtmlEscaped());
    html += "<br>";

    html += QString("<br><b>Таблицы</b> — всего: %1<br>").arg(r.totalTables);
    html += "<table cellspacing='2'>";
    html += statRow("&nbsp;&nbsp;С непустым комментарием:", r.tablesWithComment,      r.totalTables);
    html += statRow("&nbsp;&nbsp;Только кириллица:        ", r.tablesRussianOnly,      r.totalTables);
    html += statRow("&nbsp;&nbsp;Есть латинские буквы:    ", r.tablesHasLatin,         r.totalTables);
    html += statRow("&nbsp;&nbsp;Без комментария:         ", r.tablesNoComment.size(), r.totalTables);
    html += "</table>";

    html += QString("<br><b>Колонки</b> — всего: %1<br>").arg(r.totalColumns);
    html += "<table cellspacing='2'>";
    html += statRow("&nbsp;&nbsp;С непустым комментарием:", r.columnsWithComment,      r.totalColumns);
    html += statRow("&nbsp;&nbsp;Только кириллица:        ", r.columnsRussianOnly,      r.totalColumns);
    html += statRow("&nbsp;&nbsp;Есть латинские буквы:    ", r.columnsHasLatin,         r.totalColumns);
    html += statRow("&nbsp;&nbsp;Без комментария:         ", r.columnsNoComment.size(), r.totalColumns);
    html += "</table>";

    m_statsLabel->setText(html);
}

void SchemaAnalysisDialog::updateFilterLabels(const Result& r) {
    m_filterCombo->blockSignals(true);
    m_filterCombo->setItemText(0, QString("Таблицы без комментария (%1)")
                               .arg(r.tablesNoComment.size()));
    m_filterCombo->setItemText(1, QString("Таблицы с латинскими буквами (%1)")
                               .arg(r.tablesWithLatin.size()));
    m_filterCombo->setItemText(2, QString("Колонки без комментария (%1)")
                               .arg(r.columnsNoComment.size()));
    m_filterCombo->setItemText(3, QString("Колонки с латинскими буквами (%1)")
                               .arg(r.columnsWithLatin.size()));
    m_filterCombo->blockSignals(false);
}

void SchemaAnalysisDialog::populateProblems(int index) {
    const auto* item = m_schemaList->currentItem();
    if (!item) { m_table->setRowCount(0); return; }

    const QString schema = item->data(Qt::UserRole).toString();
    if (!m_results.contains(schema)) { m_table->setRowCount(0); return; }

    const Result& r = m_results[schema];
    const QList<Item>* items = nullptr;
    switch (index) {
        case 0: items = &r.tablesNoComment;  break;
        case 1: items = &r.tablesWithLatin;  break;
        case 2: items = &r.columnsNoComment; break;
        case 3: items = &r.columnsWithLatin; break;
        default: return;
    }

    m_table->setRowCount(0);
    for (const auto& i : *items) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(i.object));
        m_table->setItem(row, 1, new QTableWidgetItem(i.comment));
    }
}

void SchemaAnalysisDialog::onFilterChanged(int index) {
    populateProblems(index);
}
