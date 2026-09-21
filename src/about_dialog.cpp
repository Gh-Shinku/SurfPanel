#include "about_dialog.h"

#include "palette_theme.h"
#include "window_effects.h"

#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSysInfo>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr auto kGithubUrl = "https://github.com/Gh-Shinku/SurfPanel";
constexpr auto kLicenseUrl = "https://www.gnu.org/licenses/lgpl-3.0.html";

QFont UiFont(int pixelSize, QFont::Weight weight = QFont::Normal) {
  QFont font;
  font.setFamilies({"Segoe UI Variable Text", "Segoe UI"});
  font.setPixelSize(pixelSize);
  font.setWeight(weight);
  return font;
}

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

class AboutContentWidget final : public QWidget {
public:
  explicit AboutContentWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setObjectName("aboutContent");
    setMouseTracking(true);
    setAccessibleName("About SurfPanel information");
    setAccessibleDescription(
        "SurfPanel version and build information, GitHub project link, "
        "open-source software credits, and license link.");
    setProperty("versionText", QString("Version %1").arg(SURFPANEL_VERSION));
    setProperty("buildText", BuildDescription());
    setProperty("githubUrl", kGithubUrl);
    setProperty("openSourceText", "Qt 6; toml11; MinGW-w64; GCC; Inno Setup 6");
    setProperty("licenseText", "GNU Lesser General Public License v3");
    setProperty("renderingMode", "QPainter");
    setProperty("bodyPixelSize", 14);
  }

  QSize sizeHint() const override { return {472, 400}; }

  void setDarkMode(bool dark) {
    dark_ = dark;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const auto colors = ColorsForPalette(dark_);
    const QColor text = colors.text;
    const QColor secondary = colors.secondary;
    const QColor link = dark_ ? QColor("#60CDFF") : QColor("#005FB8");

    constexpr int kIconExtent = 56;
    const QIcon icon(":/icons/SurfPanel.ico");
    if (!icon.isNull()) {
      painter.drawPixmap(
          QRect(0, 0, kIconExtent, kIconExtent),
          icon.pixmap(QSize(kIconExtent, kIconExtent), devicePixelRatioF()));
    }

    const QFont titleFont = UiFont(22, QFont::DemiBold);
    const QFont bodyFont = UiFont(14, QFont::Medium);
    const QFont secondaryFont = UiFont(12, QFont::Medium);
    const QFont sectionFont = UiFont(14, QFont::DemiBold);

    painter.setPen(text);
    painter.setFont(titleFont);
    painter.drawText(QPointF(76, 23), "SurfPanel");
    painter.setFont(bodyFont);
    painter.drawText(QPointF(76, 45), property("versionText").toString());
    painter.setFont(secondaryFont);
    painter.setPen(secondary);
    painter.drawText(QPointF(76, 64), property("buildText").toString());

    painter.setFont(bodyFont);
    painter.setPen(text);
    painter.drawText(QPointF(0, 106),
                     "A lightweight command palette and extensible desktop "
                     "daemon.");

    const QString githubPrefix = "Source code and issue tracker: ";
    painter.drawText(QPointF(0, 146), githubPrefix);
    const int githubX = QFontMetrics(bodyFont).horizontalAdvance(githubPrefix);
    githubLinkRect_ =
        drawLink(&painter, QPoint(githubX, 146),
                 "github.com/Gh-Shinku/SurfPanel", link, bodyFont);

    painter.setPen(dark_ ? QColor("#666666") : QColor("#8A8A8A"));
    painter.drawLine(QPointF(0, 171.5), QPointF(width(), 171.5));

    painter.setPen(text);
    painter.setFont(sectionFont);
    painter.drawText(QPointF(0, 207), "Open-source software");

    drawComponent(&painter, 247, "Qt 6",
                  "cross-platform application framework (LGPL/GPL)", text,
                  secondary, secondaryFont, sectionFont);
    drawComponent(&painter, 272, "toml11", "TOML parser and serializer (MIT)",
                  text, secondary, secondaryFont, sectionFont);
    drawComponent(&painter, 297, "MinGW-w64 and GCC Runtime Libraries",
                  "Windows toolchain and runtime", text, secondary,
                  secondaryFont, sectionFont);
    drawComponent(&painter, 322, "Inno Setup 6", "Windows installer builder",
                  text, secondary, secondaryFont, sectionFont);

    painter.setFont(bodyFont);
    painter.setPen(text);
    painter.drawText(QPointF(0, 361), "SurfPanel is distributed under the");
    licenseLinkRect_ =
        drawLink(&painter, QPoint(0, 384),
                 "GNU Lesser General Public License v3", link, bodyFont);
    painter.setFont(secondaryFont);
    painter.setPen(secondary);
    painter.drawText(
        QPointF(QFontMetrics(bodyFont).horizontalAdvance(
                    "GNU Lesser General Public License v3") +
                    12,
                384),
        QString("Copyright © %1 Shinku.").arg(QDate::currentDate().year()));
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    const bool overLink = githubLinkRect_.contains(event->position()) ||
                          licenseLinkRect_.contains(event->position());
    setCursor(overLink ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      if (githubLinkRect_.contains(event->position())) {
        QDesktopServices::openUrl(QUrl(kGithubUrl));
        return;
      }
      if (licenseLinkRect_.contains(event->position())) {
        QDesktopServices::openUrl(QUrl(kLicenseUrl));
        return;
      }
    }
    QWidget::mouseReleaseEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    unsetCursor();
    QWidget::leaveEvent(event);
  }

private:
  static QRectF drawLink(QPainter *painter, const QPoint &baseline,
                         const QString &label, const QColor &color,
                         QFont font) {
    font.setUnderline(true);
    painter->setFont(font);
    painter->setPen(color);
    painter->drawText(baseline, label);
    const QFontMetrics metrics(font);
    return {QPointF(baseline.x(), baseline.y() - metrics.ascent()),
            QSizeF(metrics.horizontalAdvance(label), metrics.height())};
  }

  static void drawComponent(QPainter *painter, int baseline,
                            const QString &name, const QString &description,
                            const QColor &nameColor,
                            const QColor &descriptionColor,
                            const QFont &descriptionFont,
                            const QFont &nameFont) {
    painter->setPen(nameColor);
    painter->setFont(nameFont);
    painter->drawText(QPointF(0, baseline), name);
    const int descriptionX =
        QFontMetrics(nameFont).horizontalAdvance(name) + 12;
    painter->setFont(descriptionFont);
    painter->setPen(descriptionColor);
    painter->drawText(QPointF(descriptionX, baseline), "— " + description);
  }

  bool dark_ = false;
  QRectF githubLinkRect_;
  QRectF licenseLinkRect_;
};

} // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
  setObjectName("aboutDialog");
  setWindowTitle("About SurfPanel");
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowIcon(QIcon(":/icons/SurfPanel.ico"));
  setModal(false);
  setMinimumWidth(520);
  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(true);
  setFont(UiFont(14));

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 22, 24, 18);
  layout->setSpacing(12);

  content_ = new AboutContentWidget(this);
  layout->addWidget(content_);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  layout->addWidget(buttons);
}

void AboutDialog::setDarkMode(bool dark) {
  const QString foreground = dark ? "#F5F5F5" : "#1F1F1F";
  const QString background = dark ? "#202020" : "#FFFFFF";
  const QString border = dark ? "#5A5A5A" : "#767676";
  QPalette themedPalette = palette();
  themedPalette.setColor(QPalette::Window, QColor(background));
  themedPalette.setColor(QPalette::WindowText, QColor(foreground));
  themedPalette.setColor(QPalette::Button, QColor(background));
  themedPalette.setColor(QPalette::ButtonText, QColor(foreground));
  setPalette(themedPalette);
  setStyleSheet(
      QString(R"(
    QDialog#aboutDialog { background-color: %1; color: %2; }
    QDialog#aboutDialog QPushButton {
      min-width: 72px;
      padding: 4px 12px;
      color: %2;
      background: %1;
      border: 1px solid %3;
      border-radius: 3px;
    }
    QDialog#aboutDialog QPushButton:hover { background: %4; }
  )")
          .arg(background, foreground, border, dark ? "#353535" : "#EAEAEA"));
  static_cast<AboutContentWidget *>(content_)->setDarkMode(dark);
  ApplyNativeWindowTheme(winId(), dark);
}
