#pragma once

#include <QDialog>

class QLabel;
class QShowEvent;

class AboutDialog final : public QDialog {
public:
  explicit AboutDialog(QWidget *parent = nullptr);
  void setDarkMode(bool dark);

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateIconPixmap();

  QLabel *icon_ = nullptr;
  bool screenChangeConnected_ = false;
};
