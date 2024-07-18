#include "Common.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <iomanip>

std::string convertEpochToReadableDate(time_t epochTime) {
    // Convert epoch time to tm structure
    struct tm* timeinfo = localtime(&epochTime);

    // Create a buffer to hold the formatted date
    char buffer[80];

    // Format the date and time
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Return the formatted date as a string
    return std::string(buffer);
}

// Функция для создания кадра
Frame createFrame(FrameType type, const std::vector<uint8_t>& payload) {
    Frame frame;
    frame.length = 4 + 1 + payload.size(); // Длина кадра
    frame.frameType = type;
    frame.payload = payload;
    return frame;
}

// Функция для разбора кадра
Frame parseFrame(const std::vector<uint8_t>& data) {
    Frame frame;
    frame.length = *reinterpret_cast<const uint32_t*>(data.data());
    frame.frameType = static_cast<FrameType>(data[4]);
    frame.payload.assign(data.begin() + 5, data.end());
    return frame;
}

int connectToServer(const char* address, uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error\n";
        return -1;
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    int result = inet_pton(AF_INET, address, &serv_addr.sin_addr);

    if (result <= 0) {
        if (result == 0) {
            std::cerr << "Invalid address\n";
        } else {
            std::cerr << "Address not supported\n";
        }
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed\n";
        return -1;
    }

    return sock;
}

void sendFrame(int sock, const Frame& frame) {
    std::vector<uint8_t> data;
    data.resize(frame.length);
    std::memcpy(data.data(), &frame.length, 4);
    data[4] = frame.frameType;
    std::memcpy(data.data() + 5, frame.payload.data(), frame.payload.size());
    send(sock, data.data(), data.size(), 0);
}

Frame receiveFrame(int sock) {
    uint32_t length;
    recv(sock, &length, 4, 0);
    std::vector<uint8_t> data(length);
    std::memcpy(data.data(), &length, 4);
    recv(sock, data.data() + 4, length - 4, 0);
    return parseFrame(data);
}


void processDigitialFrame (Frame &dataFrame) {
    if (dataFrame.frameType == DigitalPoints) {
        // Обработка цифровых сигналов
        size_t offset = 0;
        uint8_t causeOfTransmission = dataFrame.payload[offset++];
        uint16_t count = *reinterpret_cast<uint16_t*>(&dataFrame.payload[offset]);
        offset += 2;

        std::vector<DigitalPoint> digitalPoints;
        for (uint16_t i = 0; i < count; ++i) {
            DigitalPoint point;
            point.pointId = *reinterpret_cast<uint32_t*>(&dataFrame.payload[offset]);
            offset += 4;
            point.value = dataFrame.payload[offset++];
            point.timeTag = *reinterpret_cast<uint64_t*>(&dataFrame.payload[offset]);
            offset += 8;
            point.quality = *reinterpret_cast<Quality*>(&dataFrame.payload[offset]);
            offset += 1;
            digitalPoints.push_back(point);
        }

        // Вывод цифровых сигналов
        for (const auto& point : digitalPoints) {
            std::cout << "PointId= " << point.pointId
                    << ", Value= " << static_cast<int>(point.value)
                    << ", TimeTag = " << convertEpochToReadableDate(point.timeTag)
                    << ", Quality = [" << (point.quality.valid ? "Valid" : "Invalid")
                    << (point.quality.substituted ? ", Substituted" : "")
                    << (point.quality.overflow ? ", Overflow" : "")
                    << "]\n";
        }
    } else {
        std::cerr << "Not a digital frame";
    }
}

void processAnalogFrame (Frame &dataFrame) {
    if (dataFrame.frameType == AnalogPoints) {
        // Обработка аналоговых сигналов
        size_t offset = 0;
        uint8_t causeOfTransmission = dataFrame.payload[offset++];
        uint16_t count = *reinterpret_cast<uint16_t*>(&dataFrame.payload[offset]);
        offset += 2;

        std::vector<AnalogPoint> analogPoints;
        for (uint16_t i = 0; i < count; ++i) {
            AnalogPoint point;
            point.pointId = *reinterpret_cast<uint32_t*>(&dataFrame.payload[offset]);
            offset += 4;
            point.value = *reinterpret_cast<float*>(&dataFrame.payload[offset]);
            offset += 4;
            point.timeTag = *reinterpret_cast<uint64_t*>(&dataFrame.payload[offset]);
            offset += 8;
            point.quality = *reinterpret_cast<Quality*>(&dataFrame.payload[offset]);
            offset += 1;
            analogPoints.push_back(point);
        }

        // Вывод аналоговых сигналов
        for (const auto& point : analogPoints) {
            std::cout << "PointId= " << point.pointId
                    << ", Value= " << std::fixed << std::setprecision(1) << point.value
                    << ", TimeTag = " << convertEpochToReadableDate(point.timeTag)
                    << ", Quality = [" << (point.quality.valid ? "Valid" : "Invalid")
                    << (point.quality.substituted ? ", Substituted" : "")
                    << (point.quality.overflow ? ", Overflow" : "")
                    << "]\n";
        }
    }
}

int main() {
    // cpptest.08z.ru
    std::cout << "Connecting to cpptest.08z.ru:12567\n";
    int sock = connectToServer("188.166.108.92", 12567);
    if (sock < 0) return -1;
    std::cout << "Connected\n\n";

    // Отправка запроса Start
    std::cout << "Sending START\n";
    Frame startFrame = createFrame(Start, {});
    sendFrame(sock, startFrame);

    std::cout << "Receiving ACK\n\n";
    Frame ackFrame = receiveFrame(sock);
    if (ackFrame.frameType != Ack) {
        std::cerr << "Failed to start session\n";
        return -1;
    }

    std::cout << "Sending GI\n";
    // Отправка запроса GeneralInterrogation
    Frame giFrame = createFrame(GeneralInterrogation, {});
    sendFrame(sock, giFrame);
    std::cout << "Receiving ACK\n\n";
    ackFrame = receiveFrame(sock);
    if (ackFrame.frameType != Ack) {
        std::cerr << "Failed to perform general interrogation\n";
        return -1;
    }

    // Получение данных DigitalPoints и AnalogPoints
    std::cout << "Receiving DIGITIAL_POINTS\n";
    Frame dataFrame = receiveFrame(sock);

    processDigitialFrame(dataFrame);

    std::cout << std::endl;

    std::cout << "Receiving ANALOG_POINTS\n";
    dataFrame = receiveFrame(sock);
    processAnalogFrame(dataFrame);
    
    std::cout << std::endl;
    // Отправка запроса DigitalControl
    std::vector<uint8_t> payload(5);
    uint32_t pointId = 2;
    uint8_t value = 1;
    std::memcpy(payload.data(), &pointId, 4);
    payload[4] = value;
    Frame dcFrame = createFrame(DigitalControl, payload);

    std::cout << "Sending DIGITIAL_CONTROL\n" << "Pointld= 2, Value= 1\n";
    sendFrame(sock, dcFrame);

    std::cout << "Receiving ACK\n";
    ackFrame = receiveFrame(sock);

    if (ackFrame.frameType != Ack) {
        std::cerr << "Failed to perform digital control\n";
        return -1;
    }

    std::cout << std::endl;

    std::cout << "Receiving DIGITIAL_POINTS\n";
    dataFrame = receiveFrame(sock);

    processDigitialFrame(dataFrame);

    std::cout << std::endl;
    // Отправка запроса Stop
    Frame stopFrame = createFrame(Stop, {});

    std::cout << "Sending STOP\n";
    sendFrame(sock, stopFrame);

    std::cout << "Receiving ACK\n";
    ackFrame = receiveFrame(sock);
    if (ackFrame.frameType != Ack) {
        std::cerr << "Failed to stop session\n";
        return -1;
    }
    
    std::cout << std::endl;
    close(sock);
    std::cout << "Connection closed\n";
    return 0;
}
