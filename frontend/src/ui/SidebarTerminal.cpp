#include "SidebarTerminal.h"
#include <QScrollBar>
#include <QDateTime>

namespace MapUI {

SidebarTerminal::SidebarTerminal(QWidget* parent) : QWidget(parent) {
    setupUi();
    printWelcomeBanner();
}

void SidebarTerminal::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 1. Header Bar (shadcn zinc with green indicator & "LOGS" title)
    headerBar = new QWidget(this);
    headerBar->setObjectName("terminalHeader");
    headerBar->setStyleSheet(R"(
        QWidget#terminalHeader {
            background-color: #18181B;
            border-top: 1px solid #27272A;
            border-bottom: 1px solid #141416;
            min-height: 24px;
            max-height: 24px;
        }
    )");

    auto* hbLayout = new QHBoxLayout(headerBar);
    hbLayout->setContentsMargins(10, 2, 8, 2);
    hbLayout->setSpacing(6);

    // Logs Status Dot (xterm green active indicator)
    lblStatusDot = new QLabel("●", headerBar);
    lblStatusDot->setStyleSheet("color: #10B981; font-size: 9px; font-weight: bold;");
    hbLayout->addWidget(lblStatusDot);

    // Header Title: LOGS
    lblHeaderTitle = new QLabel("LOGS", headerBar);
    lblHeaderTitle->setStyleSheet(R"(
        color: #E4E4E7;
        font-family: 'Segoe UI', Inter, -apple-system, sans-serif;
        font-size: 10px;
        font-weight: 700;
        letter-spacing: 0.8px;
    )");
    hbLayout->addWidget(lblHeaderTitle);
    hbLayout->addStretch(1);

    rootLayout->addWidget(headerBar);

    // 2. xterm.js Output Canvas
    outputBrowser = new QTextBrowser(this);
    outputBrowser->setReadOnly(true);
    outputBrowser->setOpenExternalLinks(true);
    outputBrowser->setStyleSheet(R"(
        QTextBrowser {
            background-color: #09090B;
            color: #D4D4D8;
            border: none;
            font-family: 'Consolas', 'Cascadia Code', 'Fira Code', 'Ubuntu Mono', monospace;
            font-size: 11px;
            line-height: 1.35;
            padding: 6px 8px;
        }
        QScrollBar:vertical {
            background: #09090B;
            width: 5px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #27272A;
            min-height: 16px;
            border-radius: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #3F3F46;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
    rootLayout->addWidget(outputBrowser, 1);

    // Flexible Height Configuration for resizable QSplitter
    setMinimumHeight(60);
}

void SidebarTerminal::printWelcomeBanner() {
    outputBrowser->clear();
    QString banner = R"(<pre style="margin:0; font-family:'Consolas','Courier New',monospace; font-size:10px; line-height:1.2; color:#38BDF8;">
<span style="color:#06B6D4;">  ██████╗ ███████╗██████╗ ██████╗ </span>
<span style="color:#06B6D4;">  ██╔══██╗██╔════╝██╔══██╗██╔══██╗</span>
<span style="color:#38BDF8;">  ██████╔╝█████╗  ██║  ██║██████╔╝</span>
<span style="color:#38BDF8;">  ██╔══██╗██╔══╝  ██║  ██║██╔══██╗</span>
<span style="color:#60A5FA;">  ██║  ██║███████╗██████╔╝██║  ██║</span>
<span style="color:#60A5FA;">  ╚═╝  ╚═╝╚══════╝╚═════╝ ╚═╝  ╚═╝</span>
</pre>
<span style="color:#10B981; font-weight:bold;">RedR Engine v1.0.0</span> · <span style="color:#A78BFA; font-weight:bold;">BlazeNeuro</span><br/>
<span style="color:#71717A;">Continuous real-time hydrodynamic telemetry & system logs initialized.</span><br/>
)";
    outputBrowser->setHtml(banner);
}

void SidebarTerminal::appendOutput(const QString& htmlText) {
    outputBrowser->append(htmlText);
    outputBrowser->verticalScrollBar()->setValue(outputBrowser->verticalScrollBar()->maximum());
}

void SidebarTerminal::appendLog(const QString& prefix, const QString& message, const QString& colorHex) {
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logLine = QString("<span style='color:#71717A;'>[%1]</span> <span style='color:%2; font-weight:bold;'>[%3]</span> <span style='color:#E4E4E7;'>%4</span>")
        .arg(timeStr, colorHex, prefix, message);
    appendOutput(logLine);
}

void SidebarTerminal::clear() {
    outputBrowser->clear();
}

} // namespace MapUI
