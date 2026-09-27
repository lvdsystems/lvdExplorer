#include "ui/dialogs/aboutdialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    // lvdUi is a static library, so the linker otherwise drops
    // aboutassets.qrc's object file entirely (nothing else references a
    // symbol from it) and its resources never get registered -- this is
    // Qt's own documented fix for resources embedded in a static lib.
    Q_INIT_RESOURCE(aboutassets);

    setWindowTitle(tr("About lvdExplorer"));
    setMinimumWidth(320);

    // A centered vertical branding block (logo card, company name, app
    // name, version, links, description, license), mirroring lvdterm's
    // own About dialog layout in place of this dialog's previous
    // icon-left/text-right single HTML blob.
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignHCenter);

    // Logo card: a plain white surface behind the lvd Systems wordmark.
    // Unlike the generated app icon elsewhere in the UI, this is a fixed
    // raster logo with a dark gradient baked into the image itself, so it
    // needs a light backdrop to stay legible regardless of the app's own
    // light/dark theme -- same reasoning as lvdterm's own logo card.
    auto *logoCard = new QFrame(this);
    logoCard->setStyleSheet(QStringLiteral("QFrame { background-color: white; border-radius: 6px; }"));
    auto *logoCardLayout = new QVBoxLayout(logoCard);
    logoCardLayout->setContentsMargins(16, 12, 16, 12);
    auto *logoLabel = new QLabel(logoCard);
    const QPixmap logo(QStringLiteral(":/branding/logo_lvd.png"));
    if (!logo.isNull())
        logoLabel->setPixmap(logo.scaledToWidth(280, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignHCenter);
    logoCardLayout->addWidget(logoLabel);
    layout->addWidget(logoCard, 0, Qt::AlignHCenter);

    auto *companyLabel = new QLabel(QStringLiteral("LVD Systems S.r.l."), this);
    QFont companyFont = companyLabel->font();
    companyFont.setPointSize(companyFont.pointSize() + 6);
    companyFont.setBold(true);
    companyLabel->setFont(companyFont);
    companyLabel->setAlignment(Qt::AlignHCenter);

    auto *appNameLabel = new QLabel(QStringLiteral("lvdExplorer"), this);
    QFont appNameFont = appNameLabel->font();
    appNameFont.setPointSize(appNameFont.pointSize() + 2);
    appNameLabel->setFont(appNameFont);
    appNameLabel->setAlignment(Qt::AlignHCenter);

    auto *versionLabel = new QLabel(tr("Version %1").arg(QApplication::applicationVersion()), this);
    versionLabel->setAlignment(Qt::AlignHCenter);

    auto *linksLabel = new QLabel(this);
    linksLabel->setTextFormat(Qt::RichText);
    linksLabel->setOpenExternalLinks(true);
    linksLabel->setText(QStringLiteral("<a href=\"https://www.lvdsystems.eu\">www.lvdsystems.eu</a> &nbsp;|&nbsp; "
                                        "<a href=\"https://www.lvdopen.eu\">www.lvdopen.eu</a>"));
    linksLabel->setAlignment(Qt::AlignHCenter);

    auto *descriptionLabel = new QLabel(tr("An open-source, Qt6-based multi-pane file manager."), this);
    descriptionLabel->setAlignment(Qt::AlignHCenter);
    descriptionLabel->setWordWrap(true);

    auto *licenseLabel = new QLabel(tr("Licensed under the GNU General Public License v3."), this);
    licenseLabel->setAlignment(Qt::AlignHCenter);
    licenseLabel->setWordWrap(true);

    auto *qtLabel = new QLabel(tr("Built with Qt %1").arg(QString::fromLatin1(QT_VERSION_STR)), this);
    qtLabel->setAlignment(Qt::AlignHCenter);
    qtLabel->setStyleSheet(QStringLiteral("color: gray;"));
    QFont qtFont = qtLabel->font();
    qtFont.setPointSize(qtFont.pointSize() - 1);
    qtLabel->setFont(qtFont);

    layout->addWidget(companyLabel);
    layout->addWidget(appNameLabel);
    layout->addWidget(versionLabel);
    layout->addWidget(linksLabel);
    layout->addWidget(descriptionLabel);
    layout->addWidget(licenseLabel);
    layout->addWidget(qtLabel);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *licenseButton = buttons->addButton(tr("License..."), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    connect(licenseButton, &QPushButton::clicked, this, &AboutDialog::showLicense);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttons);
}

void AboutDialog::showLicense()
{
    QFile file(QStringLiteral(":/license/LICENSE"));
    const QString text = file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll())
        : tr("Could not load the license text.");

    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("License"));
    dialog->resize(700, 600);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto *dialogLayout = new QVBoxLayout(dialog);
    auto *textEdit = new QPlainTextEdit(dialog);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(text);
    textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    dialogLayout->addWidget(textEdit);

    auto *dialogButtons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(dialogButtons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialogButtons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    dialogLayout->addWidget(dialogButtons);

    dialog->show();
}
