#include "android/MobileExportAudio.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <QtEndian>
#include <cstdio>
#include <functional>
#include <stdexcept>

namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
void writeStereo(const QString& path, int frames, const std::function<QPair<qint16,qint16>(int)>& sample) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) throw std::runtime_error("Cannot write test audio");
    QDataStream out(&file); out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4); out << quint32(36 + frames * 4);
    out.writeRawData("WAVEfmt ",8); out << quint32(16) << quint16(1) << quint16(2)
        << quint32(48000) << quint32(192000) << quint16(4) << quint16(16);
    out.writeRawData("data",4); out << quint32(frames * 4);
    for (int i=0;i<frames;++i) { const auto v=sample(i); out << v.first << v.second; }
}
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir folder;
    check(folder.isValid(), "private test storage");
    try {
        const QString track=folder.path()+"/track.wav";
        writeStereo(track,4800,[](int i){return QPair<qint16,qint16>(i,-i);});
        writeStereo(folder.path()+"/clock.wav",4800,[](int){return QPair<qint16,qint16>(3000,5000);});
        writeStereo(folder.path()+"/touch_Hold_riser.wav",4800,[](int i){return QPair<qint16,qint16>(10000+i,-10000-i);});
        miacode::video_export::VideoExportAudioRenderPlan plan;
        plan.alignedTotalSeconds=.1; plan.sfxDirectory=folder.path();
        plan.backgroundTrack={true,track,.02,.01,.04,.5};
        plan.scheduledSfxPlaybacks.append({"clock","clock",.04,.5,.01});
        plan.touchholdSpanPlaybacks.append({"touchhold","touchhold",.06,.02,.25,.02});
        std::atomic_bool cancelled{false};
        const QString path=miacode::android::renderExportWav(plan,{},0,folder.path()+"/mix",cancelled);
        QFile file(path); check(file.open(QIODevice::ReadOnly),"read mixed WAV");
        const auto bytes=file.readAll();
        check(bytes.size()==44+4800*4,"aligned duration and stereo PCM size");
        check(bytes.mid(0,4)=="RIFF" && bytes.mid(8,4)=="WAVE","RIFF container");
        check(qFromLittleEndian<quint32>(bytes.constData()+24)==48000,"48kHz output");
        const auto near=[&](int frame,int channel,int expected,const char* message){
            const auto value=qFromLittleEndian<qint16>(bytes.constData()+44+frame*4+channel*2);
            check(qAbs(value-expected)<=3,message);
        };
        near(959,0,0,"silence before BGM start");
        near(1000,0,260,"BGM source offset and gain"); near(1000,1,-260,"stereo channels remain independent");
        near(2000,0,2260,"scheduled SFX mixes into left channel"); near(2000,1,1740,"scheduled SFX mixes into right channel");
        near(2400,0,960,"SFX stops at its owned duration");
        near(2900,0,2745,"touchhold resumes at source offset"); near(2900,1,-2745,"touchhold stereo preservation");
        near(3840,0,0,"touchhold span ends exactly"); near(4799,1,0,"output tail remains silent");
        cancelled.store(true);
        bool stopped=false;
        try { miacode::android::renderExportWav(plan,{},0,folder.path()+"/cancelled",cancelled); }
        catch(const std::exception&) { stopped=true; }
        check(stopped,"cancelled decode cannot publish a completed WAV");
        check(!QFile::exists(folder.path()+"/cancelled/audio.wav"),"cancelled output is not committed");
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); ++failures; }
    std::printf("Mobile export audio: %s\n",failures?"failed":"passed");
    return failures?1:0;
}
