#include "main_controller.h"
#include "main_window.h"

#include <QApplication>
#include <QStyleFactory>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char ** argv)
{
  // AA_EnableHighDpiScaling was added in Qt 5.6.
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
//  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  // Fix issues caused by dragging an application between two monitors with
  // different DPIs on Windows.
#ifdef _WIN32
  SetProcessDPIAware();
#endif

  // On non-Windows systems, use Qt's fusion style instead of a
  // native style.
#ifndef _WIN32
  QApplication::setStyle(QStyleFactory::create("fusion"));
#endif

  QApplication app(argc, argv);
  main_controller controller;
  main_window window;
  controller.set_window(&window);
  window.set_controller(&controller);
  window.show();
  return app.exec();
}
