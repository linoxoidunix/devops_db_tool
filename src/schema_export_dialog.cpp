#include "import_tool/schema_export_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QSettings>
#include <QMap>
#include <QVector>

// ─────────────────────────────────────────────────────────────────────────────

SchemaExportDialog::SchemaExportDialog(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    // ── Подключение ──────────────────────────────────────────────────────────
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
    m_schema   = new QLineEdit("public", this);

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

    connForm->addRow("Host:", hpRow);
    connForm->addRow("База данных:", m_database);
    connForm->addRow("Пользователь:", m_user);
    connForm->addRow("Пароль:", m_password);
    connForm->addRow("Схема:", m_schema);
    connForm->addRow(testRow);
    root->addWidget(connGroup);

    // ── Директория вывода ─────────────────────────────────────────────────────
    auto* outGroup  = new QGroupBox("Директория вывода", this);
    auto* outLayout = new QHBoxLayout(outGroup);
    m_outDir      = new QLineEdit(QDir::homePath(), this);
    m_browseBtn   = new QPushButton("Обзор…", this);
    m_copyDirBtn  = new QPushButton("Скопировать путь к директории", this);
    outLayout->addWidget(m_outDir, 1);
    outLayout->addWidget(m_browseBtn);
    outLayout->addWidget(m_copyDirBtn);
    root->addWidget(outGroup);

    // ── Опции ─────────────────────────────────────────────────────────────────
    auto* optGroup  = new QGroupBox("Опции отчёта", this);
    auto* optLayout = new QVBoxLayout(optGroup);
    m_includeFkCheck = new QCheckBox("Включить описание Foreign Key в колонках", this);
    m_includeFkCheck->setChecked(false);
    optLayout->addWidget(m_includeFkCheck);
    root->addWidget(optGroup);

    // ── Кнопка генерации ─────────────────────────────────────────────────────
    m_generateRptBtn = new QPushButton("Сгенерировать описание схемы (RTF)", this);
    m_generateRptBtn->setMinimumHeight(36);
    m_generateRptBtn->setStyleSheet("QPushButton { font-weight: bold; }");
    root->addWidget(m_generateRptBtn);

    // ── Последний файл ────────────────────────────────────────────────────────
    auto* fileGroup  = new QGroupBox("Последний сгенерированный файл", this);
    auto* fileLayout = new QHBoxLayout(fileGroup);
    m_lastFilePath = new QLineEdit(this);
    m_lastFilePath->setReadOnly(true);
    m_lastFilePath->setPlaceholderText("Путь появится после генерации…");
    m_copyFileBtn  = new QPushButton("Скопировать путь", this);
    m_copyFileBtn->setEnabled(false);
    fileLayout->addWidget(m_lastFilePath, 1);
    fileLayout->addWidget(m_copyFileBtn);
    root->addWidget(fileGroup);

    // ── Лог ───────────────────────────────────────────────────────────────────
    auto* logGroup  = new QGroupBox("Лог", this);
    auto* logLayout = new QVBoxLayout(logGroup);
    m_logArea = new QTextEdit(this);
    m_logArea->setReadOnly(true);
    m_logArea->setFontFamily("Monospace");
    m_logArea->setMinimumHeight(180);
    logLayout->addWidget(m_logArea);
    root->addWidget(logGroup, 1);

    connect(m_testConnBtn,    &QPushButton::clicked, this, &SchemaExportDialog::onTestConnection);
    connect(m_browseBtn,      &QPushButton::clicked, this, &SchemaExportDialog::onBrowseDir);
    connect(m_copyDirBtn,     &QPushButton::clicked, this, &SchemaExportDialog::onCopyDir);
    connect(m_generateRptBtn, &QPushButton::clicked, this, &SchemaExportDialog::onGenerateReport);
    connect(m_copyFileBtn,    &QPushButton::clicked, this, &SchemaExportDialog::onCopyFilePath);

    loadSettings();
}

// ── Persistence ───────────────────────────────────────────────────────────────

void SchemaExportDialog::loadSettings() {
    QSettings s;
    s.beginGroup("SchemaExport");
    m_host->setText(s.value("host", "127.0.0.1").toString());
    m_port->setValue(s.value("port", 5432).toInt());
    m_database->setText(s.value("database").toString());
    m_user->setText(s.value("user", "postgres").toString());
    m_schema->setText(s.value("schema", "public").toString());
    m_outDir->setText(s.value("outDir", QDir::homePath()).toString());
    s.endGroup();
}

void SchemaExportDialog::saveSettings() {
    QSettings s;
    s.beginGroup("SchemaExport");
    s.setValue("host",     m_host->text());
    s.setValue("port",     m_port->value());
    s.setValue("database", m_database->text());
    s.setValue("user",     m_user->text());
    s.setValue("schema",   m_schema->text());
    s.setValue("outDir",   m_outDir->text());
    s.endGroup();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

ConnectionConfig SchemaExportDialog::readDbConfig() const {
    ConnectionConfig cfg;
    cfg.host     = m_host->text().trimmed();
    cfg.port     = m_port->value();
    cfg.database = m_database->text().trimmed();
    cfg.user     = m_user->text().trimmed();
    cfg.password = m_password->text();
    cfg.schema   = m_schema->text().trimmed();
    return cfg;
}

void SchemaExportDialog::log(const QString& msg) {
    m_logArea->append(msg);
    QApplication::processEvents();
}

void SchemaExportDialog::setLastFile(const QString& path) {
    m_lastFilePath->setText(path);
    m_copyFileBtn->setEnabled(!path.isEmpty());
}

QStringList SchemaExportDialog::runQuery(const ConnectionConfig& cfg, const QString& sql) {
    QTemporaryFile tmp;
    tmp.setAutoRemove(true);
    if (!tmp.open()) return {};
    tmp.write(sql.toUtf8());
    tmp.flush();

    QProcess psql;
    psql.setEnvironment(QProcess::systemEnvironment()
        << QString("PGPASSWORD=%1").arg(cfg.password));
    psql.start("psql", {
        "-h", cfg.host, "-p", QString::number(cfg.port),
        "-U", cfg.user, "-d", cfg.database,
        "-t", "-A", "-F", "\x1f",
        "-f", tmp.fileName()
    });
    psql.waitForFinished(15000);

    const QString out = QString::fromUtf8(psql.readAllStandardOutput());
    return out.split('\n', Qt::SkipEmptyParts);
}

// ── RTF helpers ───────────────────────────────────────────────────────────────

static QString rtfEscape(const QString& s) {
    QString r;
    r.reserve(s.size() * 2);
    for (const QChar ch : s) {
        const ushort u = ch.unicode();
        if (u == '\\') { r += "\\\\"; }
        else if (u == '{')  { r += "\\{"; }
        else if (u == '}')  { r += "\\}"; }
        else if (u < 128)   { r += ch; }
        else { r += QString("\\u%1?").arg(static_cast<int>(u)); }
    }
    return r;
}

static QString rtfCell(const QString& text, bool bold = false) {
    QString s = bold
        ? "\\pard\\intbl\\b " + rtfEscape(text) + "\\b0\\cell "
        : "\\pard\\intbl " + rtfEscape(text) + "\\cell ";
    return s;
}

// ── RTF report ────────────────────────────────────────────────────────────────

QString SchemaExportDialog::buildRtfReport(const ConnectionConfig& cfg, bool includeFk) {
    const QString schema = cfg.schema;

    // Check connectivity
    if (runQuery(cfg, "SELECT 1").isEmpty()) {
        log("✘ Не удалось подключиться к БД — проверьте параметры.");
        return {};
    }

    // Schema comment
    QString schemaComment;
    {
        const auto rows = runQuery(cfg, QString(
            "SELECT COALESCE("
            "REPLACE(REPLACE(obj_description(n.oid,'pg_namespace'),E'\\n',' '),chr(31),' '),"
            "'')"
            " FROM pg_namespace n WHERE n.nspname='%1'").arg(schema));
        if (!rows.isEmpty()) schemaComment = rows.first().trimmed();
    }
    log(QString("  Схема: %1").arg(schema));

    // Tables
    struct TableInfo { QString name, comment; };
    QVector<TableInfo> tables;
    {
        const auto rows = runQuery(cfg, QString(
            "SELECT t.table_name,"
            "COALESCE(REPLACE(REPLACE(obj_description(pc.oid,'pg_class'),E'\\n',' '),chr(31),' '),'') "
            "FROM information_schema.tables t "
            "JOIN pg_catalog.pg_class pc ON pc.relname=t.table_name "
            "JOIN pg_catalog.pg_namespace pn"
            "  ON pn.oid=pc.relnamespace AND pn.nspname=t.table_schema "
            "WHERE t.table_schema='%1' AND t.table_type='BASE TABLE' "
            "ORDER BY t.table_name").arg(schema));
        for (const QString& row : rows) {
            const QStringList p = row.split('\x1f');
            tables.push_back({p.value(0).trimmed(), p.value(1).trimmed()});
        }
    }
    log(QString("  Таблиц найдено: %1").arg(tables.size()));

    // Column widths in twips (1 inch = 1440 twips)
    // 4 or 5 columns in A4 landscape (about 12240 usable twips)
    const int colW1 = 2400; // field name
    const int colW2 = 3600; // description (RU)
    const int colW3 = 1800; // data type
    const int colW4 = 2400; // FK (optional)
    const int totalW3 = colW1 + colW2 + colW3;
    const int totalW4 = colW1 + colW2 + colW3 + colW4;

    auto makeRowDef = [&](bool fk) -> QString {
        QString s = "\\trowd\\trgaph108\\trleft0"
                    "\\trbrdrb\\brdrs\\brdrw10"
                    "\\trbrdrr\\brdrs\\brdrw10"
                    "\\trbrdrl\\brdrs\\brdrw10"
                    "\\trbrdrt\\brdrs\\brdrw10 ";
        int x = colW1;
        s += QString("\\cellx%1 ").arg(x); x += colW2;
        s += QString("\\cellx%1 ").arg(x); x += colW3;
        s += QString("\\cellx%1 ").arg(x);
        if (fk) { x += colW4; s += QString("\\cellx%1 ").arg(x); }
        return s;
    };
    Q_UNUSED(totalW3)
    Q_UNUSED(totalW4)

    // Build RTF
    QString rtf;
    rtf.reserve(1 << 20);

    rtf += "{\\rtf1\\ansi\\ansicpg1251\\deff0\n"
           "{\\fonttbl{\\f0\\froman\\fcharset204 Times New Roman;}"
           "{\\f1\\fmodern\\fcharset204 Courier New;}}\n"
           "{\\colortbl ;\\red0\\green0\\blue0;\\red100\\green100\\blue100;}\n"
           "\\widowctrl\\hyphauto\n"
           "\\paperw16838\\paperh11906\\lndscpsxn\n" // A4 landscape
           "\\margl1440\\margr1440\\margt1440\\margb1440\n"
           "\\f0\\fs24\n\n";

    // Schema header
    rtf += "{\\pard\\sb200\\sa100\\b\\fs28 "
           + rtfEscape(QString("Схема: %1").arg(schema))
           + "\\b0\\par}\n";
    if (!schemaComment.isEmpty())
        rtf += "{\\pard\\sa100\\i " + rtfEscape(schemaComment) + "\\i0\\par}\n";

    for (const auto& tbl : tables) {
        log(QString("  Обработка: %1").arg(tbl.name));

        // Table heading
        rtf += "\n{\\pard\\sb200\\sa80\\b\\fs26 "
               + rtfEscape(QString("Таблица: %1").arg(tbl.name))
               + "\\b0\\par}\n";
        if (!tbl.comment.isEmpty())
            rtf += "{\\pard\\sa80\\i " + rtfEscape(tbl.comment) + "\\i0\\par}\n";

        // Columns
        struct ColInfo { QString name, type, comment; };
        QVector<ColInfo> cols;
        {
            const auto rows = runQuery(cfg, QString(
                "SELECT c.column_name,"
                "CASE"
                " WHEN c.data_type='USER-DEFINED' THEN c.udt_name"
                " WHEN c.data_type='character varying'"
                "      AND c.character_maximum_length IS NOT NULL"
                "   THEN 'varchar('||c.character_maximum_length||')'"
                " WHEN c.data_type='timestamp without time zone' THEN 'timestamp'"
                " WHEN c.data_type='timestamp with time zone' THEN 'timestamptz'"
                " WHEN c.data_type='double precision' THEN 'float8'"
                " ELSE c.data_type END,"
                "COALESCE(REPLACE(REPLACE(pgd.description,E'\\n',' '),chr(31),' '),'') "
                "FROM information_schema.columns c "
                "JOIN pg_catalog.pg_class pc ON pc.relname=c.table_name "
                "JOIN pg_catalog.pg_namespace pn"
                "  ON pn.oid=pc.relnamespace AND pn.nspname=c.table_schema "
                "LEFT JOIN pg_catalog.pg_description pgd"
                "  ON pgd.objoid=pc.oid AND pgd.objsubid=c.ordinal_position "
                "WHERE c.table_schema='%1' AND c.table_name='%2' "
                "ORDER BY c.ordinal_position").arg(schema, tbl.name));
            for (const QString& row : rows) {
                const QStringList p = row.split('\x1f');
                cols.push_back({p.value(0).trimmed(), p.value(1).trimmed(),
                                p.value(2).trimmed()});
            }
        }

        // FK map
        QMap<QString, QString> fkMap;
        if (includeFk) {
            const auto rows = runQuery(cfg, QString(
                "SELECT kcu.column_name,"
                "ccu.table_schema||'.'||ccu.table_name||'('||ccu.column_name||')' "
                "FROM information_schema.key_column_usage kcu "
                "JOIN information_schema.referential_constraints rc"
                "  ON rc.constraint_name=kcu.constraint_name"
                "  AND rc.constraint_schema=kcu.table_schema "
                "JOIN information_schema.constraint_column_usage ccu"
                "  ON ccu.constraint_name=rc.unique_constraint_name "
                "WHERE kcu.table_schema='%1' AND kcu.table_name='%2'")
                .arg(schema, tbl.name));
            for (const QString& row : rows) {
                const QStringList p = row.split('\x1f');
                if (p.size() >= 2) fkMap[p[0].trimmed()] = p[1].trimmed();
            }
        }

        // Header row
        rtf += makeRowDef(includeFk);
        rtf += rtfCell("Наименование поля", true);
        rtf += rtfCell("Описание (рус.)",   true);
        rtf += rtfCell("Тип данных",         true);
        if (includeFk) rtf += rtfCell("Foreign Key", true);
        rtf += "\\row\n";

        // Data rows
        for (const auto& col : cols) {
            rtf += makeRowDef(includeFk);
            rtf += rtfCell(col.name);
            rtf += rtfCell(col.comment);
            rtf += rtfCell(col.type);
            if (includeFk) rtf += rtfCell(fkMap.value(col.name));
            rtf += "\\row\n";
        }
        rtf += "\\pard\\par\n";
    }

    rtf += "}\n";
    return rtf;
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void SchemaExportDialog::onTestConnection() {
    const auto cfg = readDbConfig();
    m_connStatusLabel->setText("Проверка…");
    m_testConnBtn->setEnabled(false);
    QApplication::processEvents();

    QProcess psql;
    psql.setEnvironment(QProcess::systemEnvironment()
        << QString("PGPASSWORD=%1").arg(cfg.password));
    psql.start("psql", {
        "-h", cfg.host, "-p", QString::number(cfg.port),
        "-U", cfg.user, "-d", cfg.database,
        "-c", "SELECT 1", "-t", "-A"
    });
    psql.waitForFinished(10000);
    m_testConnBtn->setEnabled(true);

    if (psql.exitCode() == 0) {
        m_connStatusLabel->setText("<span style='color:green'>✔ Соединение успешно</span>");
    } else {
        const QString err = QString::fromUtf8(psql.readAllStandardError()).trimmed();
        m_connStatusLabel->setText(
            QString("<span style='color:red'>✗ %1</span>")
            .arg(err.isEmpty() ? "Ошибка соединения" : err.toHtmlEscaped()));
    }
}

void SchemaExportDialog::onBrowseDir() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Выберите директорию вывода", m_outDir->text());
    if (!dir.isEmpty()) m_outDir->setText(dir);
}

void SchemaExportDialog::onCopyDir() {
    const QString dir = m_outDir->text().trimmed();
    if (dir.isEmpty()) return;
    QApplication::clipboard()->setText(dir);
    log("Путь к директории скопирован: " + dir);
}

void SchemaExportDialog::onCopyFilePath() {
    const QString path = m_lastFilePath->text().trimmed();
    if (path.isEmpty()) return;
    QApplication::clipboard()->setText(path);
    log("Путь к файлу скопирован: " + path);
}

void SchemaExportDialog::onGenerateReport() {
    const auto cfg = readDbConfig();
    if (cfg.database.isEmpty() || cfg.schema.isEmpty()) {
        QMessageBox::warning(this, "Описание схемы", "Укажите базу данных и схему.");
        return;
    }
    const QString outDir = m_outDir->text().trimmed();
    if (outDir.isEmpty()) {
        QMessageBox::warning(this, "Описание схемы", "Укажите директорию вывода.");
        return;
    }

    QDir d(outDir);
    if (!d.exists() && !d.mkpath(".")) {
        log("✘ Не удалось создать директорию: " + outDir);
        return;
    }

    m_logArea->clear();
    log(QString("Генерация RTF-описания схемы «%1»…").arg(cfg.schema));
    saveSettings();

    m_generateRptBtn->setEnabled(false);
    const QString rtf = buildRtfReport(cfg, m_includeFkCheck->isChecked());
    m_generateRptBtn->setEnabled(true);

    if (rtf.isEmpty()) return;

    const QString ts   = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString path = d.absoluteFilePath(
        QString("schema_%1_description_%2.rtf").arg(cfg.schema, ts));

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        log("✘ Не удалось создать файл: " + path);
        return;
    }
    f.write(rtf.toLocal8Bit());
    f.close();

    log("✔ RTF-отчёт сохранён: " + path);
    setLastFile(path);
}
