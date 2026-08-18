#include "core/application_controller.h"
#include "gui/main_window.h"
#include <QApplication>

int main(int argc,char **argv){QApplication app(argc,argv);QCoreApplication::setApplicationName("OpenAstroSuite");QCoreApplication::setOrganizationName("OpenAstroLink");oas::ApplicationController controller;oas::MainWindow window(&controller);window.show();return app.exec();}
