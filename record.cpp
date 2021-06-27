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

    sf::Font font;
    if(!font.loadFromFile("OpenSans-SemiBold.ttf"))
        throw std::runtime_error("Could not load font");

    sf::Text text("Press R to record", font);
    text.setCharacterSize(24);
    text.setFillColor(sf::Color::Red);
    text.setStyle(sf::Text::Bold);
    text.setPosition(10, 5);

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::R) {
                if(!encoder.running()) {
                    encoder.start();
                    text.setString("Press R to stop recording");
                } else {
                    encoder.finish();
                    text.setString("Stopped recording");
                }
            }
        }

        auto yuyv = cam.getNextFrame();

        auto rgba = rgbaConv.convertYUYV(yuyv);
        tex.update(rgba);

        if(encoder.running()) {
            auto nv12 = nv12Conv.convertYUYV(yuyv);
            encoder.writeFrame(nv12);
        }

        window.clear();
        window.draw(cam_image);
        window.draw(text);
        window.display();
    }

    if(encoder.running()) encoder.finish();
}
