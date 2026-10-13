#include "export/cover_export/CoverBatchExportPolicy.h"

#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace miacode::cover_export;
    const bool valid = boundedBatchCoverFrameSeconds(30.0, 10.0) == 10.0
        && boundedBatchCoverFrameSeconds(30.0, 10.0, 8.95) == 8.95
        && boundedBatchCoverFrameSeconds(7.0, 10.0, 8.95) == 7.0
        && boundedBatchCoverFrameSeconds(-2.0, 10.0) == 0.0
        && boundedBatchCoverFrameSeconds(5.0, 0.0) == 0.0
        && boundedBatchCoverFrameSeconds(5.0, 10.0, 0.0) == 0.0
        && batchCoverOutputPath(QStringLiteral("C:/exports"), QStringLiteral("C:/exports/song(2).wav"))
            == QStringLiteral("C:/exports/song(2)_cover.png")
        && batchCoverOutputPath(QStringLiteral("C:/covers"), QStringLiteral("C:/exports/song.mp4"))
            == QStringLiteral("C:/covers/song_cover.png");
    if (!valid) QTextStream(stderr) << "Batch cover frame bounds or companion name failed" << Qt::endl;
    return valid ? 0 : 1;
}
