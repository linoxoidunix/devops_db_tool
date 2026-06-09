#include "import_tool/schema_export_dialog.h"
#include "import_tool/schema_backup_dialog.h"
#include "import_tool/schema_analysis_dialog.h"
#include "import_tool/about_widget.h"

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QStackedWidget>
#include <QFrame>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("DevOps DB Tool");
    app.setOrganizationName("DevOps");

    QDialog mainDlg;
    mainDlg.setWindowTitle("DevOps DB Tool");
    mainDlg.setMinimumSize(860, 750);

    // ── Pages ─────────────────────────────────────────────────────────────────
    auto* stack = new QStackedWidget(&mainDlg);
    stack->addWidget(new SchemaExportDialog(stack));    // 0
    stack->addWidget(new SchemaBackupDialog(stack));    // 1
    stack->addWidget(new SchemaAnalysisDialog(stack));  // 2
    stack->addWidget(new AboutWidget(stack));           // 3

    // ── Left: search + nav list ───────────────────────────────────────────────
    auto* search = new QLineEdit(&mainDlg);
    search->setPlaceholderText("Поиск…");
    search->setClearButtonEnabled(true);

    auto* navList = new QListWidget(&mainDlg);
    navList->setFixedWidth(190);
    navList->setFrameShape(QFrame::NoFrame);
    navList->setSpacing(2);
    navList->setStyleSheet(R"(
        QListWidget {
            background: #f0f0f0;
            outline: none;
        }
        QListWidget::item {
            padding: 8px 10px;
            border-radius: 4px;
            color: #333;
        }
        QListWidget::item:selected {
            background: #0078d4;
            color: white;
        }
        QListWidget::item:hover:!selected {
            background: #d8e8f8;
        }
    )");

    const QStringList names = {
        "Описание схемы",
        "Бэкап / Восстановление",
        "Анализ комментариев",
        "О программе",
    };
    for (int i = 0; i < names.size(); ++i) {
        auto* item = new QListWidgetItem(names[i]);
        item->setData(Qt::UserRole, i);
        navList->addItem(item);
    }
    navList->setCurrentRow(0);

    // Filter nav items by search text
    QObject::connect(search, &QLineEdit::textChanged, navList,
                     [navList](const QString& text) {
        const QString lower = text.toLower().trimmed();
        QListWidgetItem* firstVisible = nullptr;
        for (int i = 0; i < navList->count(); ++i) {
            auto* item = navList->item(i);
            const bool match = lower.isEmpty() || item->text().toLower().contains(lower);
            item->setHidden(!match);
            if (match && !firstVisible)
                firstVisible = item;
        }
        if (navList->currentItem() && navList->currentItem()->isHidden() && firstVisible)
            navList->setCurrentItem(firstVisible);
    });

    // Switch page on selection
    QObject::connect(navList, &QListWidget::currentItemChanged,
                     stack, [stack](QListWidgetItem* current, QListWidgetItem*) {
        if (current)
            stack->setCurrentIndex(current->data(Qt::UserRole).toInt());
    });

    // ── Left panel widget ─────────────────────────────────────────────────────
    auto* leftWidget = new QWidget(&mainDlg);
    leftWidget->setFixedWidth(200);
    leftWidget->setStyleSheet("background: #f0f0f0;");
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(6, 6, 6, 6);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(search);
    leftLayout->addWidget(navList, 1);

    // ── Vertical separator ────────────────────────────────────────────────────
    auto* separator = new QFrame(&mainDlg);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);

    // ── Main layout ───────────────────────────────────────────────────────────
    auto* mainLayout = new QHBoxLayout(&mainDlg);
    mainLayout->setContentsMargins(0, 0, 4, 4);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(leftWidget);
    mainLayout->addWidget(separator);
    mainLayout->addWidget(stack, 1);

    mainDlg.show();
    return app.exec();
}
