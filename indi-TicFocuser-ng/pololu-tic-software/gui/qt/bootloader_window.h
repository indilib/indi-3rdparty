#pragma once

#include <bootloader.h>

#include <QMainWindow>

class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTimer;

class bootloader_window : public QMainWindow, bootloder_status_listener
{
  Q_OBJECT

public:
  bootloader_window(QWidget * parent = 0);

signals:
  void upload_complete();

private:
  QLineEdit * filename_input;
  QPushButton * browse_button;
  QComboBox * device_chooser;
  bool device_was_selected = false;
  QLabel * progress_label;
  QProgressBar * progress_bar;
  QPushButton * program_button;
  QTimer * update_timer;

  void setup_window();
  void set_interface_enabled(bool enabled);
  void set_status(const char * status, uint32_t progress, uint32_t max_progress);
  void clear_status();
  void show_error_message(const std::string &);
  bool confirm_warning(const std::string &);

private slots:
  void on_update_timer_timeout();
  void on_browse_button_clicked();
  void on_program_button_clicked();
};
