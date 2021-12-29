/*
 * MIT License
 *
 * Copyright (c) 2021 Gerald Young (Yoyobuae)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <linux/input.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include <chrono>

#include "input.hpp"

class Keyboard {
public:
    Keyboard();
    ~Keyboard();

    bool open(const char *device);
    void close();
    bool isOpen();

    bool read();

    bool a[KEY_CNT];

private:
    int fd;
    bool state[KEY_CNT];
};

Keyboard::Keyboard()
    : fd(-1)
{
    memset(state, 0, sizeof(state));
    memset(a, 0, sizeof(a));
}

Keyboard::~Keyboard()
{
    close();
}

bool Keyboard::open(const char *device)
{
    if (device == nullptr)
        return false;

    if (fd >= 0)
        close();

    fd = ::open(device, O_RDONLY);

    if (fd < 0)
        return false;

    int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }

    return true;
}

void Keyboard::close()
{
    if (fd >= 0)
        ::close(fd);
}

bool Keyboard::isOpen()
{
    if (fd >= 0) {
        return true;
    } else {
        return false;
    }
}

bool Keyboard::read()
{
    bool ret = false;

    for (int c = 0; c < KEY_MAX; c++) {
        if (a != nullptr && a[c] && !state[c]) {
            a[c] = false;
            ret = true;
        }
    }

    if (fd < 0)
        return false;

    struct input_event ev;

    do {
        auto bytes_read = ::read(fd, &ev, sizeof(ev));

        if (bytes_read < 0) {
            break;
        }

        if (bytes_read == 0) {
            break;
        }

        if (bytes_read < static_cast<decltype(bytes_read)>(sizeof(ev))) {
            break;
        }

        if (ev.type == EV_KEY && ev.code > 0 && ev.code < KEY_MAX) {
            switch (ev.value) {
                case 0:
                    state[ev.code] = false;
                    ret = true;
                    break;
                case 1:
                    state[ev.code] = true;
                    if (a != nullptr) a[ev.code] = true;
                    ret = true;
                    break;
                default:
                    // Do nothing
                    break;
            }
        }
    } while (true);

    return ret;
}

Keyboard keyboard;
static std::chrono::time_point<std::chrono::steady_clock> lastGetKeyTime;

bool getKey(int key)
{
    if (!keyboard.isOpen()) {
        DIR *d;
        struct dirent *dir;
        d = opendir("/dev/input/by-id/");
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                printf("%s\n", dir->d_name);
            }
            closedir(d);
        }
        keyboard.open("/dev/input/by-id/usb-Genius_SlimStar_335-event-kbd");
    }

    if (keyboard.isOpen()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<int, std::ratio<1, 60>>>(now - lastGetKeyTime);
        if (elapsed.count() > 0) {
            keyboard.read();
        }

        if (key >= 0 && key < KEY_MAX)
            return keyboard.a[key];
    }

    return false;
}