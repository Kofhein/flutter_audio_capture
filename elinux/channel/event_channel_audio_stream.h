#ifndef PACKAGES_AUDIO_CAPTURE_ELINUX_AUDIO_CAPTURE_CHANNELS_EVENT_CHANNEL_AUDIO_STREAM_H_
#define PACKAGES_AUDIO_CAPTURE_ELINUX_AUDIO_CAPTURE_CHANNELS_EVENT_CHANNEL_AUDIO_STREAM_H_

#include <flutter/encodable_value.h>
#include <flutter/event_channel.h>
#include <flutter/plugin_registrar.h>

#include <string>
#include <thread>

class EventChannelAudioStream {
 public:
  EventChannelAudioStream(flutter::PluginRegistrar* registrar);
  ~EventChannelAudioStream() = default;

 private:
  void Send(double * samples, int num_samples);
  void recorder_thread(const std::string cmd, int num_samples);
  
  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;
  volatile bool is_recording = false;
  std::thread* thread = nullptr;
};

#endif  // PACKAGES_AUDIO_CAPTURE_ELINUX_AUDIO_CAPTURE_CHANNELS_EVENT_CHANNEL_AUDIO_STREAM_H_
