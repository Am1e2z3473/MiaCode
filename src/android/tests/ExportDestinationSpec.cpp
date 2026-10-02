#include "android/ExportDestination.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

using namespace miacode::android;
static void check(bool passed, const char* message) {
    if (!passed) { std::fprintf(stderr, "Export destination: %s\n", message); std::exit(1); }
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir root;
    check(root.isValid(), "temporary settings directory");
    app.setOrganizationName("MiaCodeSpec"); app.setApplicationName("ExportDestination");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root.path());
    const QString directory = root.path() + "/batch";
    const QString single = directory + "/song.mp4";
    rememberExportDestination(single, "content://provider/single", false, "Download/song.mp4");
    check(exportDestinationDisplayPath(single) == "Download/song.mp4", "selected document label shows its public location");
    check(exportDestination(single).value("uri") == "content://provider/single", "exact document target survives a new settings instance");
    check(exportDestination(directory + "/other.mp4").isEmpty(), "one chosen document cannot authorize siblings");
    rememberExportDestination(directory, "content://provider/tree", true, "Download/Album");
    check(exportDestinationDisplayPath(directory + "/nested/song.wav") == "Download/Album/nested/song.wav", "nested tree label follows the published relative path");
    check(exportDestinationDisplayPath(directory) == "Download/Album", "selected folder label excludes the internal staging directory");
    check(exportDestination(single).value("uri") == "content://provider/single", "a specific file takes priority over a selected folder");
    const auto nested = exportDestination(directory + "/album/song.wav");
    check(nested.value("uri") == "content://provider/tree" && nested.value("relativePath") == "album/song.wav", "selected tree covers nested outputs with their relative names");
    check(exportDestination(root.path() + "/batch-other/song.mp4").isEmpty(), "tree authorization does not leak to a folder with the same prefix");
    check(exportDestination(directory + "/../outside.mp4").isEmpty(), "normalized parent traversal cannot inherit the tree");
    check(exportDestination(directory + "/album/../song.mp4").value("uri") == "content://provider/single", "equivalent paths refer to the same selected document");
    std::puts("Export destination isolation: passed");
}
