#include "audio/PreviewAudioWorkerFactory.h"

#include "audio/BassPreviewAudioBackend.h"

namespace miacode::preview_audio {

PreviewAudioBackendFactory productionPreviewAudioBackendFactory()
{
    return []() -> std::unique_ptr<PreviewAudioBackend> {
        return std::make_unique<BassPreviewAudioBackend>();
    };
}

}  // namespace miacode::preview_audio
