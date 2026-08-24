#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace MapUI {

class SidebarTerminal : public QWidget {
    Q_OBJECT

private:
    QWidget* headerBar;
    QLabel* lblStatusDot;
    QLabel* lblHeaderTitle;
    QTextBrowser* outputBrowser;

    void setupUi();
    void printWelcomeBanner();

public:
    explicit SidebarTerminal(QWidget* parent = nullptr);

    void appendOutput(const QString& htmlText);
    void appendLog(const QString& prefix, const QString& message, const QString& colorHex = "#38BDF8");
    void clear();
};

} // namespace MapUI
