#include <stdio.h> // for debug output

#include <SFML/Graphics.hpp>
#include "webcam.hpp"
#include "color_converter.hpp"

int main(int argc, char** argv) {
    if(argc == 1) {
        printf("usage: record DEVNAME (something like /dev/video0)");
        return 1;
    }

    char* dev_name = argv[1];
    auto cam = Webcam(dev_name);
    cam.start();

    auto conv = ColorConverter(640, 480, 4);

    sf::RenderWindow window(sf::VideoMode(640, 480), "Webcam viewer");
    sf::Texture tex;
    tex.create(640, 480);
    sf::Sprite cam_image(tex);

    while (window.isOpen())
    {
        auto buf = conv.convertYUYV(cam.getNextFrame());

        tex.update(buf);
        // printf("got buffer %x\n", buf);

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
}

