#pragma once

#include <mutex>

#include "bass.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>

namespace miacode::audio {

inline bool ensureBassFlacPluginLoaded(int* errorCode = nullptr)
{
    static std::mutex mutex;
    static HPLUGIN plugin = 0;
    std::lock_guard<std::mutex> lock(mutex);
    if (plugin == 0) {
#ifdef Q_OS_IOS
        const QString path = QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("Frameworks/bassflac.framework/bassflac"));
        plugin = BASS_PluginLoad(QFile::encodeName(path).constData(), 0);
#else
        plugin = BASS_PluginLoad("bassflac", 0);
#endif
        if (plugin == 0) {
            if (errorCode != nullptr) {
                *errorCode = static_cast<int>(BASS_ErrorGetCode());
            }
            return false;
        }
    }
    if (errorCode != nullptr) {
        *errorCode = 0;
    }
    return true;
}

} // namespace miacode::audio
