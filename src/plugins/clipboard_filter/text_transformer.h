#pragma once

#include <QString>

class TextTransformer {
public:
  virtual ~TextTransformer() = default;
  virtual QString transform(const QString &text) const = 0;
};

class IdentityTextTransformer final : public TextTransformer {
public:
  QString transform(const QString &text) const override;
};

class PdfTextTransformer final : public TextTransformer {
public:
  QString transform(const QString &text) const override;
};
