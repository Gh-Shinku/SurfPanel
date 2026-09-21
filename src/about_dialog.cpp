#include "about_dialog.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSysInfo>
#include <QVBoxLayout>

namespace {

QString BuildDescription() {
#ifdef NDEBUG
  constexpr auto kBuildType = "Release";
#else
  constexpr auto kBuildType = "Debug";
#endif
  return QString("%1 · Qt %2 · %3")
      .arg(kBuildType)
      .arg(QString::fromLatin1(qVersion()))
      .arg(QSysInfo::buildCpuArchitecture());
}

QLabel *CreateRichLabel(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setTextFormat(Qt::RichText);
  label->setTextInteractionFlags(Qt::TextBrowserInteraction);
  label->setOpenExternalLinks(true);
  label->setWordWrap(true);
  return label;
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
  setObjectName("aboutDialog");
  setWindowTitle("About SurfPanel");
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowIcon(QIcon(":/icons/SurfPanel.ico"));
  setModal(false);
  setMinimumWidth(440);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 22, 24, 18);
  layout->setSpacing(14);

  auto *headingLayout = new QHBoxLayout();
  headingLayout->setSpacing(16);
  auto *icon = new QLabel(this);
  icon->setPixmap(windowIcon().pixmap(56, 56));
  icon->setFixedSize(56, 56);
  headingLayout->addWidget(icon, 0, Qt::AlignTop);

  auto *titleLayout = new QVBoxLayout();
  titleLayout->setSpacing(2);
  auto *title = new QLabel("SurfPanel", this);
  QFont titleFont = title->font();
  titleFont.setPointSizeF(titleFont.pointSizeF() + 5);
  titleFont.setBold(true);
  title->setFont(titleFont);
  titleLayout->addWidget(title);

  auto *version =
      new QLabel(QString("Version %1").arg(SURFPANEL_VERSION), this);
  version->setObjectName("aboutVersion");
  titleLayout->addWidget(version);
  auto *build = new QLabel(BuildDescription(), this);
  build->setObjectName("aboutBuild");
  titleLayout->addWidget(build);
  headingLayout->addLayout(titleLayout, 1);
  layout->addLayout(headingLayout);

  auto *summary = new QLabel(
      "A lightweight command palette and extensible desktop daemon.", this);
  summary->setWordWrap(true);
  layout->addWidget(summary);

  auto *github = CreateRichLabel(
      "Source code and issue tracker: "
      "<a href=\"https://github.com/shinku/SurfPanel\">github.com/shinku/"
      "SurfPanel</a>",
      this);
  github->setObjectName("aboutGithub");
  layout->addWidget(github);

  auto *separator = new QFrame(this);
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  layout->addWidget(separator);

  auto *softwareTitle = new QLabel("Open-source software", this);
  QFont sectionFont = softwareTitle->font();
  sectionFont.setBold(true);
  softwareTitle->setFont(sectionFont);
  layout->addWidget(softwareTitle);

  auto *software = CreateRichLabel(
      "<b>Qt 6</b> — cross-platform application framework (LGPL/GPL)<br>"
      "<b>toml11</b> — TOML parser and serializer (MIT)<br>"
      "<b>MinGW-w64 and GCC Runtime Libraries</b> — Windows toolchain and "
      "runtime<br>"
      "<b>Inno Setup 6</b> — Windows installer builder",
      this);
  software->setObjectName("aboutOpenSource");
  layout->addWidget(software);

  auto *license = CreateRichLabel(
      "SurfPanel is distributed under the "
      "<a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">GNU Lesser "
      "General Public License v3</a>.<br>"
      "Copyright © " +
          QString::number(QDate::currentDate().year()) + " Shinku.",
      this);
  license->setObjectName("aboutLicense");
  layout->addWidget(license);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  layout->addWidget(buttons);
}

void AboutDialog::setDarkMode(bool dark) {
  const QString foreground = dark ? "#F1F1F1" : "#202020";
  const QString background = dark ? "#202020" : "#F9F9F9";
  const QString secondary = dark ? "#B5B5B5" : "#606060";
  const QString border = dark ? "#454545" : "#D6D6D6";
  QPalette themedPalette = palette();
  themedPalette.setColor(QPalette::Window, QColor(background));
  themedPalette.setColor(QPalette::WindowText, QColor(foreground));
  themedPalette.setColor(QPalette::Text, QColor(foreground));
  themedPalette.setColor(QPalette::Link, QColor(dark ? "#60CDFF" : "#005FB8"));
  setPalette(themedPalette);
  setStyleSheet(QString(R"(
    QDialog { background: %1; color: %2; }
    QLabel { color: %2; }
    QLabel#aboutBuild { color: %3; }
    QFrame { color: %4; }
    QPushButton {
      min-width: 72px;
      padding: 4px 12px;
      color: %2;
      background: %1;
      border: 1px solid %4;
      border-radius: 3px;
    }
    QPushButton:hover { background: %5; }
  )")
                    .arg(background, foreground, secondary, border,
                         dark ? "#353535" : "#EAEAEA"));
}
