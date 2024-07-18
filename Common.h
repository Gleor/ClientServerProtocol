#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <ctime>

// Структуры данных
struct Quality {
    uint8_t valid : 1;
    uint8_t substituted : 1;
    uint8_t overflow : 1;
    uint8_t reserved : 5;
};

struct DigitalPoint {
    uint32_t pointId;
    uint8_t value;
    uint64_t timeTag;
    Quality quality;
};

struct AnalogPoint {
    uint32_t pointId;
    float value;
    uint64_t timeTag;
    Quality quality;
};

enum FrameType {
    DigitalPoints = 1,
    AnalogPoints = 2,
    DigitalControl = 3,
    Start = 4,
    Stop = 5,
    GeneralInterrogation = 6,
    Ack = 7,
    Nack = 8
};

struct Frame {
    uint32_t length;
    FrameType frameType;
    std::vector<uint8_t> payload;
};
