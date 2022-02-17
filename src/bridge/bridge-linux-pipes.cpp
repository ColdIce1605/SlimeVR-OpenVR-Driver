#include "bridge.hpp"
#if defined(linux) && defined(BRIDGE_USE_PIPES)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>


// changed from windows version as it seems to cause problems
// lets make it a little bit more sane looking
// this change also needs to be done on the server side
#define PIPE_NAME "/tmp/pipe-SlimeVRDriver"

unsigned long lastReconnectFrame = 0;

int fd;
BridgeStatus currentBridgeStatus = BRIDGE_DISCONNECTED;
char buffer[1024];

void updatePipe(SlimeVRDriver::VRDriver &driver);
void resetPipe(SlimeVRDriver::VRDriver &driver);
void attemptPipeConnect(SlimeVRDriver::VRDriver &driver);

BridgeStatus runBridgeFrame(SlimeVRDriver::VRDriver &driver) {
    switch(currentBridgeStatus) {
        case BRIDGE_DISCONNECTED:
            attemptPipeConnect(driver);
        break;
        case BRIDGE_ERROR:
            resetPipe(driver);
        break;
        case BRIDGE_CONNECTED:
            updatePipe(driver);
        break;
    }

    return currentBridgeStatus;
}

bool getNextBridgeMessage(messages::ProtobufMessage &message, SlimeVRDriver::VRDriver &driver) {
    if(currentBridgeStatus == BRIDGE_CONNECTED) {
        uint32_t messageLength = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
        if(messageLength > 1024) {
            // TODO Buffer overflow
        }
        if(read(fd, buffer, messageLength)) {
            if(message.ParseFromArray(buffer + 4, messageLength - 4))
                return true;
            } else {
                currentBridgeStatus = BRIDGE_ERROR;
                driver.Log("Bridge error: " + std::to_string(errno));
        }
    } else {
        currentBridgeStatus = BRIDGE_ERROR;
        driver.Log("Bridge error: " + std::to_string(errno));
    }
    return false;
}

bool sendBridgeMessage(messages::ProtobufMessage &message, SlimeVRDriver::VRDriver &driver) {
    if(currentBridgeStatus == BRIDGE_CONNECTED) {
        uint32_t size = (uint32_t) message.ByteSizeLong();
        if(size > 1020) {
            driver.Log("Message too big");
            return false;
        }
        message.SerializeToArray(buffer + 4, size);
        size += 4;
        buffer[0] = size & 0xFF;
        buffer[1] = (size >> 8) & 0xFF;
        buffer[2] = (size >> 16) & 0xFF;
        buffer[3] = (size >> 24) & 0xFF;
        if(write(fd, buffer, size)) {
            return true;
        }
        currentBridgeStatus = BRIDGE_ERROR;
        driver.Log("Bridge error: " + std::to_string(errno));
    }
    return false;
}

void updatePipe(SlimeVRDriver::VRDriver &driver) {
}

void resetPipe(SlimeVRDriver::VRDriver &driver) {
    if(fd != -1) {
        close(fd);
        unlink(PIPE_NAME);
        fd = -1;
        currentBridgeStatus = BRIDGE_DISCONNECTED;
        driver.Log("Pipe was reset");
    }
}

void attemptPipeConnect(SlimeVRDriver::VRDriver &driver) {
    mkfifo(PIPE_NAME, 0666);
    fd = open(PIPE_NAME, O_CREAT, O_RDWR); // Read, Write
    if(fd != -1) {
        currentBridgeStatus = BRIDGE_CONNECTED;
        driver.Log("Pipe was connected");
        return;
    }
}

#endif