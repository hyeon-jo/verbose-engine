#include "control_app.hpp"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    
    ControlApp window;
    window.show();
    
    return app.exec();
} 