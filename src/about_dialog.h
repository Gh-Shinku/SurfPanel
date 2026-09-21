#pragma once

#include <QDialog>

class QWidget;

class AboutDialog final : public QDialog {
public:
  explicit AboutDialog(QWidget *parent = nullptr);
  void setDarkMode(bool dark);

private:
  QWidget *content_ = nullptr;
};
