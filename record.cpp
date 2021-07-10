#include <stdio.h> // for debug output

#include <SFML/Graphics.hpp>
#include "webcam.hpp"
#include "color_converter.hpp"
#include "video_encoder.hpp"
#include "audio_recorder.hpp"

#include <system_error>
#include <stdexcept>

int main(int argc, char** argv) {
    /* printf("audio capture test\n");
    AudioRecorder rec;
    rec.start();
    
    for(int i = 0; i<1000; i++) {
        uint8_t* buf = rec.getSample();
        int avg = 0;
        
        for(int s=0; s<128; s++) {
            avg += buf[s];
        }

        avg /= 128;
        printf("avg magnitude:%d\r", avg);
    }

    return 0; */

    if(argc == 1) {
        printf("usage: record DEVNAME (something like /dev/video0)");
        return 1;
    }

    VideoEncoder::setup();

    char* dev_name = argv[1];
    auto cam = Webcam(dev_name, 640, 480, 30);
    cam.start();

    auto [width, height] = cam.resolution();
    auto fps = cam.effectiveFps();
    printf("res %dx%d\n", width, height);
    printf("fps %d\n", fps);

    auto rgbaConv = ColorConverter(width, height, Colorspace::RGBA);
    auto nv12Conv = ColorConverter(width, height, Colorspace::NV12);

    auto encoder = VideoEncoder("out.mp4", width, height, fps);

    sf::RenderWindow window(sf::VideoMode(width, height), "Webcam viewer");
    window.setFramerateLimit(fps);
    sf::Texture tex;
    tex.create(width, height);
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
