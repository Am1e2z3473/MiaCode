#pragma once

#include <QString>
#include <QVector>

namespace miacode::audio_decode {

struct DecodedMonoAudio {
    QVector<float> samples;
    int sampleRate = 0;
    double durationSeconds = 0.0;

    bool isEmpty() const { return samples.isEmpty() || sampleRate <= 0; }
};

DecodedMonoAudio decodeFileToMono(
    const QString& path,
    int targetSampleRate);
double probeFileDurationSeconds(const QString& path);

}  // namespace miacode::audio_decode
