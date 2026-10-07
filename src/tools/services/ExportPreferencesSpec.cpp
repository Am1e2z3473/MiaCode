#include "app/services/VideoExportPreferences.h"
#include "export/video_export/EncoderProbeCache.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {
class MemoryEncoderCache final : public miacode::video_export::EncoderProbeCache {
public:
    QString preferredHardwareEncoder() const override { return codec; }
    void rememberPreferredHardwareEncoder(const QString& value) override { codec = value; }
    QString codec;
};
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << '\n'; ok = false; }
    };
    QJsonObject section{
        {QStringLiteral("hud_font_paths"), QJsonObject{{QStringLiteral("timestamp"), QStringLiteral("font.ttf")}}},
        {QStringLiteral("unknown_sibling"), QJsonObject{{QStringLiteral("value"), 42}}},
        {QStringLiteral("intro_card_type"), QStringLiteral("standard")},
    };
    const QJsonObject original = section;
    bool failWrites = false;
    miacode::app_preferences::VideoExportPreferences preferences(
        [&] { return section; }, [&](const QJsonObject& value) {
            if (failWrites) return false;
            section = value;
            return true;
        });
    QJsonObject next = preferences.load();
    expect(!next.contains(QStringLiteral("intro_card_type")), "existing migration remains in app adapter");
    next.insert(QStringLiteral("fps"), 60);
    expect(preferences.save(next), "load modify save succeeds");
    expect(section.value(QStringLiteral("hud_font_paths")) == original.value(QStringLiteral("hud_font_paths"))
               && section.value(QStringLiteral("unknown_sibling")) == original.value(QStringLiteral("unknown_sibling")),
           "export task update preserves HUD and unknown sibling fields");
    const QJsonObject successful = section;
    failWrites = true;
    next.insert(QStringLiteral("fps"), 30);
    expect(!preferences.save(next) && section == successful, "adapter propagates storage failure");

    using namespace miacode::video_export;
    installEncoderProbeCache({});
    expect(!encoderProbeCache(), "export can run without a machine cache");
    const auto first = std::make_shared<MemoryEncoderCache>();
    installEncoderProbeCache(first);
    const auto inFlight = encoderProbeCache();
    const auto replacement = std::make_shared<MemoryEncoderCache>();
    installEncoderProbeCache(replacement);
    inFlight->rememberPreferredHardwareEncoder(QStringLiteral("h264_nvenc"));
    expect(first->codec == QStringLiteral("h264_nvenc") && replacement->codec.isEmpty(),
           "replacement does not redirect an in-flight cache operation");
    installEncoderProbeCache({});
    expect(!encoderProbeCache() && inFlight->preferredHardwareEncoder() == QStringLiteral("h264_nvenc"),
           "uninstall preserves lifetime of active callers");
    return ok ? 0 : 1;
}
