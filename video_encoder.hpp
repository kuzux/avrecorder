#include <stdio.h>
#include <string>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/opt.h>
}

class VideoEncoder {
    std::string filename;

    size_t width;
    size_t height;

    bool started = false;

    AVOutputFormat* av_fmt = nullptr;
    AVFormatContext* format_ctx = nullptr;
    AVCodec* codec = nullptr;
    AVStream* stream = nullptr;
    AVCodecContext* codec_ctx = nullptr;

    AVFrame* frame = nullptr;
    AVPacket pkt;

    size_t frame_id;
public:
    static void setup() {
        av_register_all();
        avcodec_register_all();
    }

    VideoEncoder(const char* filename, size_t width, size_t height) :
        filename(filename),
        width(width),
        height(height) {

        // avcodec code taken from https://stackoverflow.com/a/59559256/1936271

        av_fmt = av_guess_format(nullptr, "out.mp4", nullptr);
        if(!av_fmt) 
            throw std::runtime_error("Could not guess format\n");

        if(av_fmt->video_codec == AV_CODEC_ID_H264) {
            printf("got h264\n");
        }
        if(av_fmt->audio_codec == AV_CODEC_ID_AAC) {
            printf("got aac\n");
        }

        int rc = avformat_alloc_output_context2(&format_ctx, av_fmt, nullptr, filename);
        if(rc < 0) 
            throw std::system_error(rc, std::generic_category(), "avformat_alloc_output_context2");

        codec = avcodec_find_encoder(av_fmt->video_codec);
        if(!codec)
            throw std::runtime_error("Could not create video encoder");

        stream = avformat_new_stream(format_ctx, codec);
        if(!stream)
            throw std::runtime_error("Could not create video stream");

        codec_ctx = avcodec_alloc_context3(codec);
        if(!codec_ctx)
            throw std::runtime_error("Could not allocate codec context");

        stream->codecpar->codec_id = codec->id;
        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        stream->codecpar->width = width;
        stream->codecpar->height = height;
        stream->codecpar->format = AV_PIX_FMT_NV12;
        stream->codecpar->bit_rate = 400000;
        rc = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if(rc < 0) 
            throw std::system_error(rc, std::generic_category(), "avcodec_parameters_to_context");

        codec_ctx->time_base = (AVRational){ 1, 15 };
        codec_ctx->max_b_frames = 2;
        codec_ctx->gop_size = 15;
        codec_ctx->framerate = (AVRational){ 15, 1 };
        av_opt_set(codec_ctx, "preset", "ultrafast", 0);
        avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    }

    ~VideoEncoder() {

    }

    void start() {
        int rc = avcodec_open2(codec_ctx, codec, nullptr);
        if(rc < 0)
            throw std::system_error(rc, std::generic_category(), "avcodec_open2");

        rc = avio_open(&format_ctx->pb, "out.mp4", AVIO_FLAG_WRITE);
        if(rc < 0)
            throw std::system_error(rc, std::generic_category(), "avio_open");

        rc = avformat_write_header(format_ctx, nullptr);
        if(rc < 0)
            throw std::system_error(rc, std::generic_category(), "avformat_write_header");

        frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_NV12;
        frame->width = width;
        frame->height = height;
        rc = av_frame_get_buffer(frame, 0);
        if(rc < 0)
            throw std::system_error(rc, std::generic_category(), "av_frame_get_buffer");

        frame_id = 0;
        started = true;
    }

    void dump() const {
        av_dump_format(format_ctx, 0, filename.c_str(), 1);
    }

    void writeFrame(const uint8_t* nv12) {
        frame->pts = (90000/15) * frame_id++; // no idea why i have to scale it up like that
        memcpy(frame->data[0], nv12, width*height);
        memcpy(frame->data[1], nv12+width*height, width*height/2);
        int rc = avcodec_send_frame(codec_ctx, frame);
        if(rc < 0) 
            throw std::system_error(rc, std::generic_category(), "avcodec_send_frame");

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
    }

    void finish() {
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

        int rc = av_write_trailer(format_ctx);
        if(rc < 0)
            throw std::system_error(rc, std::generic_category(), "av_write_trailer");

        rc = avio_close(format_ctx->pb);
        if(rc < 0)
            throw std::system_error(rc, std::generic_category(), "avio_close");

        started = false;
    }

    bool running() const {
        return started;
    }
};