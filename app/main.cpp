#include "import_tool/schema_export_dialog.h"
#include "import_tool/schema_backup_dialog.h"
#include "import_tool/schema_analysis_dialog.h"

#include <QApplication>
#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("DevOps DB Tool");
    app.setOrganizationName("DevOps");

    QDialog mainDlg;
    mainDlg.setWindowTitle("DevOps DB Tool");
    mainDlg.setMinimumSize(700, 750);

    auto* tabs = new QTabWidget(&mainDlg);

    auto* schemaExportTab   = new SchemaExportDialog(tabs);
    auto* schemaBackupTab   = new SchemaBackupDialog(tabs);
    auto* schemaAnalysisTab = new SchemaAnalysisDialog(tabs);

    tabs->addTab(schemaExportTab,   "Описание схемы");
    tabs->addTab(schemaBackupTab,   "Бэкап / Восстановление");
    tabs->addTab(schemaAnalysisTab, "Анализ комментариев");

    auto* layout = new QVBoxLayout(&mainDlg);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(tabs);

    mainDlg.show();
    return app.exec();
}
