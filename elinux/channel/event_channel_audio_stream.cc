#include "event_channel_audio_stream.h"

#include <flutter/event_stream_handler_functions.h>
#include <flutter/standard_method_codec.h>

#include <vector>

namespace {
constexpr char kEventChannel[] = "ymd.dev/audio_capture_event_channel";
};  // namespace

EventChannelAudioStream::EventChannelAudioStream(
    flutter::PluginRegistrar* registrar) {
  auto event_channel =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          registrar->messenger(), kEventChannel,
          &flutter::StandardMethodCodec::GetInstance());

  auto event_channel_handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [&,this](
          const flutter::EncodableValue* arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        auto fl_sr_iter = arguments.find(flutter::EncodableValue(std::string("sampleRate")));
        int sr = std::get<int>(fl_sr_iter->second);
        auto fl_bs_iter = arguments.find(flutter::EncodableValue(std::string("bufferSize")));
        int bs = std::get<int>(fl_bs_iter->second);
        char cmd [128];
        int n = sprintf(cmd, "parec -r --rate=%d --format=s16le --channels=1", sr);
        cmd[n] = '\0';

        // start recording
        g_print("Starting recording sr=%d, bs=%d\n", sr, bs);
        is_recording = true;
        std::string command(cmd);
        thread = new std::thread(recorder_thread, command, bs);
        event_sink_ = std::move(events);
        return nullptr;
      },
      [this,&](const flutter::EncodableValue* arguments)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        event_sink_ = nullptr;
        
        is_recording = false;
        if (thread != nullptr) {
          thread->join();
          delete thread;
        }

        return nullptr;
      });
  event_channel->SetStreamHandler(std::move(event_channel_handler));
}

void EventChannelAudioStream::recorder_thread(const std::string cmd, int num_samples) {
  FILE* pipe = popen(cmd.c_str(), "r");
  int16_t raw_samples[num_samples];
  double * samples = (double *) malloc(num_samples * sizeof(double));
  if (!pipe) throw std::runtime_error("popen() failed!");
  try {
    while (is_recording) {
      size_t read = fread(raw_samples, sizeof(int16_t), num_samples, pipe);

      // convert
      for (int i = 0; i < read; i++) {
        samples[i] = ((double) raw_samples[i]) * 1e-4;
      }

      // send
      Send(samples, read);
    }
  } catch (...) {
    pclose(pipe);
    delete samples;
    throw;
  }
  delete samples;
  pclose(pipe);
}


// See: [setImageStreamImageAvailableListener] in
// flutter/plugins/packages/camera/camera/android/src/main/java/io/flutter/plugins/camera/Camera.java
void EventChannelAudioStream::Send(double * samples, int num_samples) {
  std::vector<double> sample_list(samples, samples + num_samples);

  // flutter::EncodableList planes(bytes);

  flutter::EncodableValue event(sample_list);

  event_sink_->Success(event);
}
