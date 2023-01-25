#include "include/flutter_audio_capture/flutter_audio_capture_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/encodable_value.h>
#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <sstream>
#include <thread>
#include <iostream>

namespace {

const char kMethodChannel[] = "ymd.dev/audio_capture_event_channel/method_channel";
const char kEventChannel[] = "ymd.dev/audio_capture_event_channel";

class FlutterAudioCapturePlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar);

  FlutterAudioCapturePlugin();

  virtual ~FlutterAudioCapturePlugin();

 private:
  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
      
};

std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> channel_;
std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;

static volatile bool is_recording = false;
static std::thread* thread = nullptr;

void Send(double * samples, int num_samples) {
  std::vector<double> sample_list(samples, samples + num_samples);

  // flutter::EncodableList planes(bytes);

  flutter::EncodableValue event(sample_list);

  event_sink_->Success(event);
}

void recorder_thread(const std::string cmd, int num_samples) {
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

// static
void FlutterAudioCapturePlugin::RegisterWithRegistrar(
    flutter::PluginRegistrar *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), kMethodChannel,
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<FlutterAudioCapturePlugin>();

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  auto event_channel =
        std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
            registrar->messenger(),
            kEventChannel,
            &flutter::StandardMethodCodec::GetInstance());

  auto event_channel_handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [&](
          const flutter::EncodableValue* arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&&
              events)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        const auto& args = std::get<flutter::EncodableMap>(*arguments);
        auto fl_sr_iter = args.find(flutter::EncodableValue(std::string("sampleRate")));
        int sr = std::get<int>(fl_sr_iter->second);
        auto fl_bs_iter = args.find(flutter::EncodableValue(std::string("bufferSize")));
        int bs = std::get<int>(fl_bs_iter->second);
        char cmd [128];
        int n = sprintf(cmd, "parec -r --rate=%d --format=s16le --channels=1", sr);
        cmd[n] = '\0';

        // start recording
        std::cout << "Starting recording sr=" << sr << ", bs=" << bs << std::endl;
        is_recording = true;
        std::string command(cmd);
        thread = new std::thread(recorder_thread, command, bs);
        event_sink_ = std::move(events);
        return nullptr;
      },
      [](const flutter::EncodableValue* arguments)
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

  registrar->AddPlugin(std::move(plugin));
}

FlutterAudioCapturePlugin::FlutterAudioCapturePlugin() {}

FlutterAudioCapturePlugin::~FlutterAudioCapturePlugin() {}

void FlutterAudioCapturePlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (method_call.method_name().compare("getPlatformVersion") == 0) {
    std::ostringstream version_stream;
    version_stream << "eLinux";
    result->Success(flutter::EncodableValue(version_stream.str()));
  } else {
    result->NotImplemented();
  }
}

}  // namespace

void FlutterAudioCapturePluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  FlutterAudioCapturePlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrar>(registrar));
}
