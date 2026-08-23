#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <iostream>

#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("Assam Maps - Dark Edition");
    app.setOrganizationName("SIH");
    app.setApplicationVersion("1.0.0");

    // Configure system font
    QFont appFont("Segoe UI", 10);
    appFont.setStyleHint(QFont::SansSerif);
    app.setFont(appFont);

    // Global Dark Palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(9, 9, 11));        // zinc-950
    darkPalette.setColor(QPalette::WindowText, QColor(250, 250, 250)); // zinc-50
    darkPalette.setColor(QPalette::Base, QColor(24, 24, 27));        // zinc-900
    darkPalette.setColor(QPalette::AlternateBase, QColor(39, 39, 42)); // zinc-800
    darkPalette.setColor(QPalette::Text, QColor(250, 250, 250));
    darkPalette.setColor(QPalette::Button, QColor(24, 24, 27));
    darkPalette.setColor(QPalette::ButtonText, QColor(250, 250, 250));
    darkPalette.setColor(QPalette::Highlight, QColor(250, 250, 250));
    darkPalette.setColor(QPalette::HighlightedText, QColor(9, 9, 11));
    app.setPalette(darkPalette);

    MapUI::MainWindow window;
    window.show();

    std::cout << "[INFO] Assam Maps - Dark Edition started successfully." << std::endl;
    std::cout << "[INFO] Default mode: Online (OpenStreetMap tiles)" << std::endl;
    std::cout << "[INFO] Use View menu to switch between Online and Offline map modes." << std::endl;

    return app.exec();
}
