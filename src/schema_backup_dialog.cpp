#include "import_tool/schema_backup_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QSettings>
#include <QTextStream>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────

SchemaBackupDialog::SchemaBackupDialog(QWidget* parent)
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

    // ── Экспорт ───────────────────────────────────────────────────────────────
    auto* expGroup  = new QGroupBox("Экспорт схемы (2 скрипта)", this);
    auto* expLayout = new QVBoxLayout(expGroup);

    auto* dirRow = new QHBoxLayout;
    m_outDir       = new QLineEdit(QDir::homePath(), this);
    m_browseOutBtn = new QPushButton("Обзор…", this);
    dirRow->addWidget(new QLabel("Директория:", this));
    dirRow->addWidget(m_outDir, 1);
    dirRow->addWidget(m_browseOutBtn);

    m_exportBtn = new QPushButton("Экспортировать (создать 2 скрипта)", this);
    m_exportBtn->setMinimumHeight(36);
    m_exportBtn->setStyleSheet("QPushButton { font-weight: bold; }");

    auto* hint = new QLabel(
        "Скрипт 1: схема без FK-ограничений\n"
        "Скрипт 2: только FK-ограничения (ALTER TABLE … FOREIGN KEY)", this);
    hint->setStyleSheet("color: #555; font-size: 10pt;");

    expLayout->addLayout(dirRow);
    expLayout->addWidget(hint);
    expLayout->addWidget(m_exportBtn);
    root->addWidget(expGroup);

    // ── Импорт ────────────────────────────────────────────────────────────────
    auto* impGroup  = new QGroupBox("Импорт схемы", this);
    auto* impLayout = new QVBoxLayout(impGroup);

    // Script 1
    auto* s1Label = new QLabel("Шаг 1 — схема без FK:", this);
    s1Label->setStyleSheet("font-weight: bold;");
    auto* s1Row   = new QHBoxLayout;
    m_script1Path   = new QLineEdit(this);
    m_script1Path->setPlaceholderText("schema_…_nodeps_….sql");
    m_browseScript1   = new QPushButton("Обзор…", this);
    m_applyScript1Btn = new QPushButton("Применить к БД", this);
    m_applyScript1Btn->setMinimumHeight(32);
    s1Row->addWidget(m_script1Path, 1);
    s1Row->addWidget(m_browseScript1);
    s1Row->addWidget(m_applyScript1Btn);

    // Script 2
    auto* s2Label = new QLabel("Шаг 2 — только FK:", this);
    s2Label->setStyleSheet("font-weight: bold;");
    auto* s2Row   = new QHBoxLayout;
    m_script2Path   = new QLineEdit(this);
    m_script2Path->setPlaceholderText("schema_…_fk_only_….sql");
    m_browseScript2   = new QPushButton("Обзор…", this);
    m_applyScript2Btn = new QPushButton("Применить к БД", this);
    m_applyScript2Btn->setMinimumHeight(32);
    s2Row->addWidget(m_script2Path, 1);
    s2Row->addWidget(m_browseScript2);
    s2Row->addWidget(m_applyScript2Btn);

    impLayout->addWidget(s1Label);
    impLayout->addLayout(s1Row);
    impLayout->addWidget(s2Label);
    impLayout->addLayout(s2Row);
    root->addWidget(impGroup);

    // ── Лог ───────────────────────────────────────────────────────────────────
    auto* logGroup  = new QGroupBox("Лог", this);
    auto* logLayout = new QVBoxLayout(logGroup);
    m_logArea = new QTextEdit(this);
    m_logArea->setReadOnly(true);
    m_logArea->setFontFamily("Monospace");
    m_logArea->setMinimumHeight(160);
    logLayout->addWidget(m_logArea);
    root->addWidget(logGroup, 1);

    connect(m_testConnBtn,    &QPushButton::clicked, this, &SchemaBackupDialog::onTestConnection);
    connect(m_browseOutBtn,   &QPushButton::clicked, this, &SchemaBackupDialog::onBrowseOutDir);
    connect(m_exportBtn,      &QPushButton::clicked, this, &SchemaBackupDialog::onExportBoth);
    connect(m_browseScript1,  &QPushButton::clicked, this, &SchemaBackupDialog::onBrowseScript1);
    connect(m_browseScript2,  &QPushButton::clicked, this, &SchemaBackupDialog::onBrowseScript2);
    connect(m_applyScript1Btn,&QPushButton::clicked, this, &SchemaBackupDialog::onApplyScript1);
    connect(m_applyScript2Btn,&QPushButton::clicked, this, &SchemaBackupDialog::onApplyScript2);

    loadSettings();
}

// ── Persistence ───────────────────────────────────────────────────────────────

void SchemaBackupDialog::loadSettings() {
    QSettings s;
    s.beginGroup("SchemaBackup");
    m_host->setText(s.value("host", "127.0.0.1").toString());
    m_port->setValue(s.value("port", 5432).toInt());
    m_database->setText(s.value("database").toString());
    m_user->setText(s.value("user", "postgres").toString());
    m_schema->setText(s.value("schema", "public").toString());
    m_outDir->setText(s.value("outDir", QDir::homePath()).toString());
    s.endGroup();
}

void SchemaBackupDialog::saveSettings() {
    QSettings s;
    s.beginGroup("SchemaBackup");
    s.setValue("host",     m_host->text());
    s.setValue("port",     m_port->value());
    s.setValue("database", m_database->text());
    s.setValue("user",     m_user->text());
    s.setValue("schema",   m_schema->text());
    s.setValue("outDir",   m_outDir->text());
    s.endGroup();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

ConnectionConfig SchemaBackupDialog::readDbConfig() const {
    ConnectionConfig cfg;
    cfg.host     = m_host->text().trimmed();
    cfg.port     = m_port->value();
    cfg.database = m_database->text().trimmed();
    cfg.user     = m_user->text().trimmed();
    cfg.password = m_password->text();
    cfg.schema   = m_schema->text().trimmed();
    return cfg;
}

void SchemaBackupDialog::log(const QString& msg) {
    m_logArea->append(msg);
    QApplication::processEvents();
}

bool SchemaBackupDialog::applyScript(const QString& filepath, const ConnectionConfig& cfg) {
    if (!QFile::exists(filepath)) {
        log("✘ Файл не найден: " + filepath);
        return false;
    }
    QProcess psql;
    psql.setEnvironment(QProcess::systemEnvironment()
        << QString("PGPASSWORD=%1").arg(cfg.password));
    psql.start("psql", {
        "-h", cfg.host, "-p", QString::number(cfg.port),
        "-U", cfg.user, "-d", cfg.database,
        "-v", "ON_ERROR_STOP=1",
        "-f", filepath
    });
    psql.waitForFinished(120000);

    const QString out = QString::fromUtf8(psql.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(psql.readAllStandardError()).trimmed();
    if (!out.isEmpty()) log(out);
    if (!err.isEmpty()) log(err);

    if (psql.exitCode() != 0) {
        log(QString("✘ psql завершился с кодом %1.").arg(psql.exitCode()));
        return false;
    }
    return true;
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void SchemaBackupDialog::onTestConnection() {
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

// ── Export ────────────────────────────────────────────────────────────────────

void SchemaBackupDialog::onBrowseOutDir() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Выберите директорию вывода", m_outDir->text());
    if (!dir.isEmpty()) m_outDir->setText(dir);
}

void SchemaBackupDialog::onExportBoth() {
    const auto cfg = readDbConfig();
    if (cfg.database.isEmpty() || cfg.schema.isEmpty()) {
        QMessageBox::warning(this, "Экспорт", "Укажите базу данных и схему.");
        return;
    }
    const QString outDir = m_outDir->text().trimmed();
    if (outDir.isEmpty()) {
        QMessageBox::warning(this, "Экспорт", "Укажите директорию вывода.");
        return;
    }

    QDir d(outDir);
    if (!d.exists() && !d.mkpath(".")) {
        log("✘ Не удалось создать директорию: " + outDir);
        return;
    }

    m_logArea->clear();
    saveSettings();

    const QString ts    = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString tmpPath = d.absoluteFilePath(
        QString("schema_%1_full_%2.sql").arg(cfg.schema, ts));

    log(QString("Запуск pg_dump для схемы «%1»…").arg(cfg.schema));

    // Run pg_dump to get full DDL
    QProcess pgdump;
    pgdump.setEnvironment(QProcess::systemEnvironment()
        << QString("PGPASSWORD=%1").arg(cfg.password));
    pgdump.start("pg_dump", {
        "-h", cfg.host, "-p", QString::number(cfg.port),
        "-U", cfg.user, "-d", cfg.database,
        "-n", cfg.schema,
        "--schema-only",
        "-f", tmpPath
    });

    m_exportBtn->setEnabled(false);
    pgdump.waitForFinished(120000);
    m_exportBtn->setEnabled(true);

    const QString pgErr = QString::fromUtf8(pgdump.readAllStandardError()).trimmed();
    if (!pgErr.isEmpty()) log(pgErr);

    if (pgdump.exitCode() != 0) {
        log(QString("✘ pg_dump завершился с кодом %1.").arg(pgdump.exitCode()));
        return;
    }
    log("  pg_dump завершён, разбиваем на 2 скрипта…");

    // Read full dump
    QFile full(tmpPath);
    if (!full.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log("✘ Не удалось прочитать временный файл.");
        return;
    }
    const QString content = QString::fromUtf8(full.readAll());
    full.close();
    QFile::remove(tmpPath);

    // Split: FK statements are ALTER TABLE … ADD CONSTRAINT … FOREIGN KEY …
    // pg_dump emits them as multi-line blocks separated by blank lines.
    // Strategy: split by double-newline blocks, classify each block.
    const QRegularExpression fkRe(
        R"(ALTER\s+TABLE\s+\S+\s+ADD\s+CONSTRAINT\s+\S+\s+FOREIGN\s+KEY)",
        QRegularExpression::CaseInsensitiveOption);

    // We split by statement (ending with ';'), preserving comments
    QStringList noFkLines;
    QStringList fkLines;

    // Process line by line, collecting multi-line statements
    const QStringList lines = content.split('\n');
    QStringList stmtBuf;
    bool inFkBlock = false;

    auto flushStmt = [&]() {
        if (stmtBuf.isEmpty()) return;
        const QString stmt = stmtBuf.join('\n');
        if (fkRe.match(stmt).hasMatch())
            fkLines << stmt;
        else
            noFkLines << stmt;
        stmtBuf.clear();
        inFkBlock = false;
    };

    for (const QString& line : lines) {
        // A line ending with ';' closes a statement
        stmtBuf << line;
        if (line.trimmed().endsWith(';'))
            flushStmt();
    }
    flushStmt(); // trailing content

    const QString nodepsPath = d.absoluteFilePath(
        QString("schema_%1_nodeps_%2.sql").arg(cfg.schema, ts));
    const QString fkPath = d.absoluteFilePath(
        QString("schema_%1_fk_only_%2.sql").arg(cfg.schema, ts));

    auto writeFile = [&](const QString& path, const QStringList& parts,
                         const QString& header) -> bool {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            log("✘ Не удалось записать: " + path);
            return false;
        }
        QTextStream ts(&f);
        ts << header << "\n";
        for (const QString& s : parts)
            ts << s << "\n";
        return true;
    };

    const QString h1 = QString("-- Script 1: schema '%1' WITHOUT foreign key constraints\n"
                                "-- Generated: %2\n"
                                "-- Apply before Script 2\n")
                            .arg(cfg.schema, ts);
    const QString h2 = QString("-- Script 2: ONLY foreign key constraints for schema '%1'\n"
                                "-- Generated: %2\n"
                                "-- Apply AFTER Script 1\n")
                            .arg(cfg.schema, ts);

    if (!writeFile(nodepsPath, noFkLines, h1)) return;
    if (!writeFile(fkPath,     fkLines,   h2)) return;

    log(QString("✔ Скрипт 1 (без FK, %1 блоков): %2")
            .arg(noFkLines.size()).arg(nodepsPath));
    log(QString("✔ Скрипт 2 (только FK, %1 ограничений): %2")
            .arg(fkLines.size()).arg(fkPath));

    // Pre-fill import fields
    m_script1Path->setText(nodepsPath);
    m_script2Path->setText(fkPath);
}

// ── Import ────────────────────────────────────────────────────────────────────

void SchemaBackupDialog::onBrowseScript1() {
    const QString f = QFileDialog::getOpenFileName(
        this, "Выберите скрипт 1 (схема без FK)",
        m_outDir->text(), "SQL files (*.sql)");
    if (!f.isEmpty()) m_script1Path->setText(f);
}

void SchemaBackupDialog::onBrowseScript2() {
    const QString f = QFileDialog::getOpenFileName(
        this, "Выберите скрипт 2 (только FK)",
        m_outDir->text(), "SQL files (*.sql)");
    if (!f.isEmpty()) m_script2Path->setText(f);
}

void SchemaBackupDialog::onApplyScript1() {
    const QString path = m_script1Path->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Импорт", "Укажите путь к скрипту 1.");
        return;
    }
    const auto cfg = readDbConfig();
    if (cfg.database.isEmpty()) {
        QMessageBox::warning(this, "Импорт", "Укажите базу данных.");
        return;
    }
    m_logArea->clear();
    log("Применение скрипта 1 (схема без FK)…");
    m_applyScript1Btn->setEnabled(false);
    const bool ok = applyScript(path, cfg);
    m_applyScript1Btn->setEnabled(true);
    log(ok ? "✔ Скрипт 1 применён успешно."
           : "✘ Ошибка при применении скрипта 1.");
}

void SchemaBackupDialog::onApplyScript2() {
    const QString path = m_script2Path->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Импорт", "Укажите путь к скрипту 2.");
        return;
    }
    const auto cfg = readDbConfig();
    if (cfg.database.isEmpty()) {
        QMessageBox::warning(this, "Импорт", "Укажите базу данных.");
        return;
    }
    m_logArea->clear();
    log("Применение скрипта 2 (FK-ограничения)…");
    m_applyScript2Btn->setEnabled(false);
    const bool ok = applyScript(path, cfg);
    m_applyScript2Btn->setEnabled(true);
    log(ok ? "✔ Скрипт 2 применён успешно."
           : "✘ Ошибка при применении скрипта 2.");
}
