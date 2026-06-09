#include "import_tool/about_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>

#ifndef APP_VERSION
#  define APP_VERSION "0.1.0"
#endif

// Укажите сюда адрес для обратной связи:
static const char* CONTACT_EMAIL = "your@email.com";

AboutWidget::AboutWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 40, 40, 40);
    root->setSpacing(0);

    // ── App name ──────────────────────────────────────────────────────────────
    auto* nameLabel = new QLabel("DevOps DB Tool", this);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(22);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(nameLabel);

    // ── Version ───────────────────────────────────────────────────────────────
    auto* versionLabel = new QLabel(QString("Версия %1").arg(APP_VERSION), this);
    QFont verFont = versionLabel->font();
    verFont.setPointSize(11);
    versionLabel->setFont(verFont);
    versionLabel->setStyleSheet("color: #666;");
    versionLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(versionLabel);
    root->addSpacing(24);

    // ── Description ───────────────────────────────────────────────────────────
    auto* descLabel = new QLabel(
        "Инструмент для работы со схемами PostgreSQL:\n"
        "экспорт RTF-описания схемы, резервное копирование\n"
        "и восстановление, анализ качества комментариев.", this);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #444; font-size: 11pt;");
    root->addWidget(descLabel);
    root->addSpacing(32);

    // ── Separator ─────────────────────────────────────────────────────────────
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep);
    root->addSpacing(24);

    // ── Contact ───────────────────────────────────────────────────────────────
    auto* contactTitle = new QLabel("Сообщить об ошибке или предложить новую функцию:", this);
    contactTitle->setAlignment(Qt::AlignCenter);
    contactTitle->setStyleSheet("font-size: 10pt; color: #555;");
    root->addWidget(contactTitle);
    root->addSpacing(8);

    auto* emailLabel = new QLabel(
        QString("<a href='mailto:%1'>%1</a>").arg(CONTACT_EMAIL), this);
    emailLabel->setTextFormat(Qt::RichText);
    emailLabel->setOpenExternalLinks(true);
    emailLabel->setAlignment(Qt::AlignCenter);
    QFont emailFont = emailLabel->font();
    emailFont.setPointSize(12);
    emailLabel->setFont(emailFont);
    root->addWidget(emailLabel);

    root->addStretch(1);
}
