#include "plugins/clipboard_filter/text_transformer.h"

#include <QChar>
#include <QStringList>

namespace {

bool IsCjk(QChar character) {
  switch (character.script()) {
  case QChar::Script_Han:
  case QChar::Script_Hangul:
  case QChar::Script_Hiragana:
  case QChar::Script_Katakana:
    return true;
  default:
    return false;
  }
}

bool IsLatinOrDigit(QChar character) {
  return character.script() == QChar::Script_Latin || character.isDigit();
}

bool IsCjkLatinBoundary(QChar left, QChar right) {
  return (IsCjk(left) && IsLatinOrDigit(right)) ||
         (IsLatinOrDigit(left) && IsCjk(right));
}

QString RemoveCjkLatinSpacing(const QString &text) {
  QString result;
  result.reserve(text.size());

  qsizetype index = 0;
  while (index < text.size()) {
    if (!text[index].isSpace()) {
      result += text[index];
      ++index;
      continue;
    }

    qsizetype next = index;
    while (next < text.size() && text[next].isSpace()) {
      ++next;
    }
    if (!result.isEmpty() && next < text.size() &&
        IsCjkLatinBoundary(result.back(), text[next])) {
      index = next;
      continue;
    }

    result += text.mid(index, next - index);
    index = next;
  }

  return result;
}

bool IsEnglishHyphenation(const QString &left, const QString &right) {
  if (left.size() < 2 || right.isEmpty()) {
    return false;
  }

  const QChar hyphen = left.back();
  if (hyphen != '-' && hyphen.unicode() != 0x00AD) {
    return false;
  }

  const QChar previous = left[left.size() - 2];
  const QChar next = right.front();
  return previous.script() == QChar::Script_Latin &&
         next.script() == QChar::Script_Latin && next.isLower();
}

bool IsClosingPunctuation(QChar character) {
  static const QString punctuation =
      QString::fromUtf8(".,;:!?%)]}>，。；：！？、）》】」』”’");
  return punctuation.contains(character);
}

bool IsOpeningPunctuation(QChar character) {
  static const QString punctuation = QString::fromUtf8("([{<（《【「『“‘");
  return punctuation.contains(character);
}

bool NeedsJoinSpace(QChar left, QChar right) {
  const bool leftIsHyphen =
      left == '-' || left.unicode() == 0x2010 || left.unicode() == 0x2011;
  if (IsCjk(left) || IsCjk(right) || leftIsHyphen ||
      IsOpeningPunctuation(left) || IsClosingPunctuation(right)) {
    return false;
  }
  return true;
}

} // namespace

QString IdentityTextTransformer::transform(const QString &text) const {
  return text;
}

QString PdfTextTransformer::transform(const QString &text) const {
  QString normalized = text;
  normalized.replace("\r\n", "\n");
  normalized.replace('\r', '\n');

  const QStringList rawLines = normalized.split('\n', Qt::KeepEmptyParts);
  QString result;
  QString previousLine;

  for (qsizetype index = 0; index < rawLines.size(); ++index) {
    const QString line = RemoveCjkLatinSpacing(rawLines[index].trimmed());
    if (index > 0) {
      if (previousLine.isEmpty() || line.isEmpty()) {
        result += '\n';
      } else if (IsEnglishHyphenation(previousLine, line)) {
        result.chop(1);
      } else if (NeedsJoinSpace(previousLine.back(), line.front())) {
        result += ' ';
      }
    }
    result += line;
    previousLine = line;
  }

  return result;
}
