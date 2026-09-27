#pragma once

#include <QDialog>

// Help > About lvdExplorer... -- lvd Systems branding, version, license,
// and links to the project/company sites. Layout mirrors lvdterm's own
// About dialog (see ui/resources/aboutassets.qrc for the shared logo
// asset).
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);

private:
    void showLicense();
};
