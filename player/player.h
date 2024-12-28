#pragma once

#include <cstdio>
#include <cstdint>

#include <string>
#include <string_view>
#include <chrono>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

struct Player {
  AVFormatContext *fmtCtx = nullptr;
  AVCodecContext *codecCtx = nullptr;
  struct SwsContext *swsCtx = nullptr;
  std::string url = "";
  uint32_t w = 0, h = 0;
  uint8_t *buffer = nullptr;

  AVFrame *frame = nullptr;  //av_frame_alloc();
  AVFrame *rgbFrame = nullptr;  //av_frame_alloc();
  int vidStream = -1;

  double fps = 0.0;
  double duration = 0.0;
  double elapsed = 0.0;
  double avgProcessTimeInMs = 0.0;

  int frameSize() {
    return av_image_get_buffer_size(AV_PIX_FMT_RGB24, w, h, 1);
  }

  Player(std::string_view url_, uint32_t w_ = 0, uint32_t h_ = 0)
      :
      url(url_),
      w(w_),
      h(h_) {
    if (avformat_open_input(&fmtCtx, url.c_str(), nullptr, nullptr) != 0) {
      printf("AVPlayer failed to open '%s'\n", url.data());
      w = h = 0;
      url = "";
      return;
    }
    avformat_find_stream_info(fmtCtx, nullptr);

    vidStream = -1;
    for (int i = 0; i < (int) fmtCtx->nb_streams; i++)
      if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        vidStream = i;
        break;
      }
    printf(" - video stream is %d\n", vidStream);

    fps = av_q2d(fmtCtx->streams[vidStream]->avg_frame_rate);
    printf(" - average FPS is %lf\n", fps);
    duration = double(fmtCtx->duration) / (double) AV_TIME_BASE;

    printf(" - duration is %lf seconds (~%zu minutes)\n", duration,
           size_t(duration / 60.0));

    codecCtx = avcodec_alloc_context3(nullptr);
    avcodec_parameters_to_context(codecCtx,
                                  fmtCtx->streams[vidStream]->codecpar);
    printf(" - %d x %d\n", codecCtx->width, codecCtx->height);

    const AVCodec *codec = avcodec_find_decoder(codecCtx->codec_id);
    avcodec_open2(codecCtx, codec, nullptr);
    printf(" - codec '%s'\n", codec->name);

    if (!w)
      w = codecCtx->width;
    if (!h)
      h = codecCtx->height;

    int nbytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, w, h, 1);
    buffer = (uint8_t*) av_malloc(nbytes);
    swsCtx = sws_getContext(codecCtx->width, codecCtx->height,
                            codecCtx->pix_fmt, w, h, AV_PIX_FMT_RGB24,
                            SWS_BICUBIC, nullptr, nullptr, nullptr);
    frame = av_frame_alloc();
    rgbFrame = av_frame_alloc();
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer,
                         AV_PIX_FMT_RGB24, w, h, 1);
  }

  void resize(uint32_t w_, uint32_t h_) {
    if (!url.length())
      return;
    if (w == w_ && h == h_)
      return;

    w = w_;
    h = h_;
    if (!w)
      w = codecCtx->width;
    if (!h)
      h = codecCtx->height;

    if (buffer)
      av_free(buffer);
    int nbytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, w, h, 1);
    buffer = (uint8_t*) av_malloc(nbytes);

    if (rgbFrame)
      av_frame_free(&rgbFrame);
    rgbFrame = av_frame_alloc();
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer,
                         AV_PIX_FMT_RGB24, w, h, 1);

    if (swsCtx)
      sws_freeContext(swsCtx);
    swsCtx = sws_getContext(codecCtx->width, codecCtx->height,
                            codecCtx->pix_fmt, w, h, AV_PIX_FMT_RGB24,
                            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
  }

  bool play() {
    AVPacket packet;
    if (-1 == vidStream)
      return false;

    auto start = std::chrono::high_resolution_clock::now();
    while (av_read_frame(fmtCtx, &packet) >= 0) {
      if (packet.stream_index == vidStream)
        break;
      av_packet_unref(&packet);
    }
    int resp = avcodec_send_packet(codecCtx, &packet);
    if (resp < 0) {
      printf("avcodec_send_packet() error: '%s'\n", av_err2str(resp));
      av_packet_unref(&packet);
      return false;
    }

    bool playing = true;
    do {
      resp = avcodec_receive_frame(codecCtx, frame);
    } while (resp == AVERROR(EAGAIN));

    if (resp == AVERROR_EOF) {
      playing = false;
    } else if (resp < 0) {
      printf("avcodec_receive_frame() error: '%s'\n", av_err2str(resp));
      playing = false;
    } else {
      elapsed =
          (frame->pts != AV_NOPTS_VALUE) ?
              frame->pts * av_q2d(fmtCtx->streams[vidStream]->time_base) : 0.0;
//      int nbytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, w, h, 1);
//      sws_scale(swsCtx, frame->data, frame->linesize, 0, codecCtx->height, rgbFrame->data, rgbFrame->linesize);
//      {
//        std::lock_guard<std::mutex> lk(mutex);
//        memcpy(buffer, rgbFrame->data[0], nbytes);
//      }
    }
    av_packet_unref(&packet);
    auto stop = std::chrono::high_resolution_clock::now();
    double ms =
        double(
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start)
                .count()) * 1e-6;
    avgProcessTimeInMs = 0.0 == ms ? ms : 0.9 * avgProcessTimeInMs + 0.1 * ms;
    return playing;
  }

  bool seek(double seconds) {
    int64_t timeStamp = int64_t(seconds * AV_TIME_BASE);
    if (av_seek_frame(fmtCtx, vidStream, timeStamp, AVSEEK_FLAG_BACKWARD)
        >= 0) {
      avcodec_flush_buffers(codecCtx);
      play();
    }
    return false;
  }

  void decode() {
    int nbytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, w, h, 1);
    sws_scale(swsCtx, frame->data, frame->linesize, 0, codecCtx->height,
              rgbFrame->data, rgbFrame->linesize);
    memcpy(buffer, rgbFrame->data[0], nbytes);
  }

  ~Player() {
    if (buffer) {
      av_free(buffer);
      buffer = nullptr;
    }
    if (swsCtx) {
      sws_freeContext(swsCtx);
      swsCtx = nullptr;
    }
    if (frame) {
      av_frame_free(&frame);
      frame = nullptr;
    }
    if (rgbFrame) {
      av_frame_free(&rgbFrame);
      rgbFrame = nullptr;
    }
    if (codecCtx) {
      avcodec_free_context(&codecCtx);
      codecCtx = nullptr;
    }
    if (fmtCtx) {
      avformat_close_input(&fmtCtx);
      fmtCtx = nullptr;
    }
  }
};
