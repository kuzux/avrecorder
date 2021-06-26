class ColorConverter {
    size_t width;
    size_t height;
    size_t bpp;

    uint8_t* out;

    void yuvToRgba(uint8_t* rgba, uint8_t y, uint8_t u, uint8_t v) {
        float Y = (int16_t)y - 16;
        float U = (int16_t)u - 128;
        float V = (int16_t)v - 128;

        float R = 1.164 * Y             + 1.596 * V;
        float G = 1.164 * Y - 0.392 * U - 0.813 * V;
        float B = 1.164 * Y + 2.017 * U;

        if(R > 255) R = 255;
        if(G > 255) G = 255;
        if(B > 255) B = 255;

        if(R < 0) R = 0;
        if(G < 0) G = 0;
        if(B < 0) B = 0;

        rgba[0] = (uint8_t)R;
        rgba[1] = (uint8_t)G;
        rgba[2] = (uint8_t)B;
        rgba[3] = 255;
    }
public:
    ColorConverter(size_t width, size_t height, size_t output_bytes_per_pixel)
        : width(width),
        height(height),
        bpp(output_bytes_per_pixel) {
        out = new uint8_t[width * height * bpp];
    }

    ~ColorConverter() {
        delete out;
    }

    uint8_t* convertYUYV(uint8_t* in) {
        for(size_t i=0; i<height; i++) {
            for(size_t j=0; j<width/2; j++) {
                size_t yuv_base = 2*i*width + 4*j;
                uint8_t y1 = in[yuv_base];
                uint8_t u  = in[yuv_base+1];
                uint8_t y2 = in[yuv_base+2];
                uint8_t v  = in[yuv_base+3];

                size_t rgba_base = 4*(i*width+2*j);
                yuvToRgba(out+rgba_base,   y1, u, v);
                yuvToRgba(out+rgba_base+4, y2, u, v);
            }
        }

        return out;
    }
};