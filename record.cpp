#include <stdio.h> // for debug output

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/opt.h>
}

#include <SFML/Graphics.hpp>
#include "webcam.hpp"
#include "color_converter.hpp"

#include <system_error>
#include <stdexcept>

int main(int argc, char** argv) {
    if(argc == 1) {
        printf("usage: record DEVNAME (something like /dev/video0)");
        return 1;
    }

    // avcodec code taken from https://stackoverflow.com/a/59559256/1936271
    av_register_all();
    avcodec_register_all();

    AVOutputFormat* av_fmt;
    av_fmt = av_guess_format(nullptr, "out.mp4", nullptr);
    if(!av_fmt) 
        throw std::runtime_error("Could not guess format\n");

    if(av_fmt->video_codec == AV_CODEC_ID_H264) {
        printf("got h264\n");
    }
    if(av_fmt->audio_codec == AV_CODEC_ID_AAC) {
        printf("got aac\n");
    }

    AVFormatContext* format_ctx = nullptr;
    int rc = avformat_alloc_output_context2(&format_ctx, av_fmt, nullptr, "out.mp4");
    if(rc < 0) 
        throw std::system_error(rc, std::generic_category(), "avformat_alloc_output_context2");

    AVCodec* codec = nullptr;
    codec = avcodec_find_encoder(av_fmt->video_codec);
    if(!codec)
        throw std::runtime_error("Could not create video encoder");

    AVStream* stream = nullptr;
    stream = avformat_new_stream(format_ctx, codec);
    if(!stream)
        throw std::runtime_error("Could not create video stream");

    AVCodecContext* codec_ctx = nullptr;
    codec_ctx = avcodec_alloc_context3(codec);
    if(!codec_ctx)
        throw std::runtime_error("Could not allocate codec context");

    stream->codecpar->codec_id = codec->id;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->width = 640;
    stream->codecpar->height = 480;
    stream->codecpar->format = AV_PIX_FMT_NV12;
    stream->codecpar->bit_rate = 4000000;
    rc = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    if(rc < 0) 
        throw std::system_error(rc, std::generic_category(), "avcodec_parameters_to_context");

    codec_ctx->time_base = (AVRational){ 1, 1 };
    codec_ctx->max_b_frames = 2;
    codec_ctx->gop_size = 20;
    codec_ctx->framerate = (AVRational){ 20, 1 };
    av_opt_set(codec_ctx, "preset", "ultrafast", 0);
    avcodec_parameters_from_context(stream->codecpar, codec_ctx);

    rc = avcodec_open2(codec_ctx, codec, nullptr);
    if(rc < 0)
        throw std::system_error(rc, std::generic_category(), "avcodec_open2");

    rc = avio_open(&format_ctx->pb, "out.mp4", AVIO_FLAG_WRITE);
    if(rc < 0)
        throw std::system_error(rc, std::generic_category(), "avio_open");

    rc = avformat_write_header(format_ctx, nullptr);
    if(rc < 0)
        throw std::system_error(rc, std::generic_category(), "avformat_write_header");

    AVFrame* frame = nullptr;
    frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_NV12;
    frame->width = 640;
    frame->height = 480;
    rc = av_frame_get_buffer(frame, 0);
    if(rc < 0)
        throw std::system_error(rc, std::generic_category(), "av_frame_get_buffer");

    av_dump_format(format_ctx, 0, "test.mp4", 1);

    char* dev_name = argv[1];
    auto cam = Webcam(dev_name);
    cam.start();

    auto rgbaConv = ColorConverter(640, 480, Colorspace::RGBA);
    auto nv12Conv = ColorConverter(640, 480, Colorspace::NV12);

    sf::RenderWindow window(sf::VideoMode(640, 480), "Webcam viewer");
    window.setFramerateLimit(20);
    sf::Texture tex;
    tex.create(640, 480);
    sf::Sprite cam_image(tex);

    size_t frameId = 0;

    while (window.isOpen())
    {
        auto yuyv = cam.getNextFrame();
        auto rgba = rgbaConv.convertYUYV(yuyv);
        auto nv12 = nv12Conv.convertYUYV(yuyv);

        tex.update(rgba);
        frame->pts = 5200 * frameId++; // no idea why i have to scale it up like that
        memcpy(frame->data[0], nv12, 640*480);
        memcpy(frame->data[1], nv12+640*480, 640*480/2);
        rc = avcodec_send_frame(codec_ctx, frame);
        if(rc < 0) 
            throw std::system_error(rc, std::generic_category(), "avcodec_send_frame");

        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;
        rc = avcodec_receive_packet(codec_ctx, &pkt);
        if(rc < 0 && rc != -11) {
            throw std::system_error(rc, std::generic_category(), "avcodec_receive_packet");
        } else if (rc >= 0) {
            av_interleaved_write_frame(format_ctx, &pkt);
        }
        av_packet_unref(&pkt);

        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        window.clear();
        window.draw(cam_image);
        window.display();
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    for (;;) {
        avcodec_send_frame(codec_ctx, NULL);
        if (avcodec_receive_packet(codec_ctx, &pkt) == 0) {
            av_interleaved_write_frame(format_ctx, &pkt);
            av_packet_unref(&pkt);
        }
        else {
            break;
        }
    }

    rc = av_write_trailer(format_ctx);
    if(rc < 0)
        throw std::system_error(rc, std::generic_category(), "av_write_trailer");

    rc = avio_close(format_ctx->pb);
    if(rc < 0)
        throw std::system_error(rc, std::generic_category(), "avio_close");
}
