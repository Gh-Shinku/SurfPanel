#pragma once

#include <QByteArray>
#include <filesystem>

bool WriteFileAtomically(const std::filesystem::path &path,
                         const QByteArray &contents);
