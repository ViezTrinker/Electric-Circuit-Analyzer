/*!
 * \file main.cpp
 * \brief Starts the Electric Circuit Analyzer window.
 */

#include <QApplication>

#include "ui/main_window.h"

int main(int argumentCount, char* argumentValues[])
{
   QApplication application(argumentCount, argumentValues);
   application.setApplicationName("Electric Circuit Analyzer");
   Analyzer::MainWindow window;
   window.show();
   return application.exec();
}
