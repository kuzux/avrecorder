#include <stdio.h> // for debug output



#include <SFML/Graphics.hpp>
#include "webcam.hpp"
#include "color_converter.hpp"
#include "video_encoder.hpp"

#include <system_error>
#include <stdexcept>

int main(int argc, char** argv) {
    if(argc == 1) {
        printf("usage: record DEVNAME (something like /dev/video0)");
        return 1;
    }

    VideoEncoder::setup();

    char* dev_name = argv[1];
    auto cam = Webcam(dev_name);
    cam.start();

    auto rgbaConv = ColorConverter(640, 480, Colorspace::RGBA);
    auto nv12Conv = ColorConverter(640, 480, Colorspace::NV12);

    auto encoder = VideoEncoder("out.mp4", 640, 480);

    sf::RenderWindow window(sf::VideoMode(640, 480), "Webcam viewer");
    window.setFramerateLimit(15);
    sf::Texture tex;
    tex.create(640, 480);
    sf::Sprite cam_image(tex);

    encoder.start();
    encoder.dump();

    while (window.isOpen())
    {
        auto yuyv = cam.getNextFrame();
        auto rgba = rgbaConv.convertYUYV(yuyv);
        auto nv12 = nv12Conv.convertYUYV(yuyv);

        tex.update(rgba);
        encoder.writeFrame(nv12);

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

    encoder.finish();
}
