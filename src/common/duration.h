#ifndef DURATION_H_
#define DURATION_H_

#include <chrono>

using time_point = std::chrono::time_point<std::chrono::steady_clock>;
using time_point_system = std::chrono::time_point<std::chrono::system_clock>;

using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;
using seconds = std::chrono::seconds;
using hours = std::chrono::hours;
using days = std::chrono::duration<long, std::ratio_multiply<hours::period, std::ratio<24>>::type>;

using namespace std::chrono_literals;
using namespace std::literals;
using namespace std::literals::chrono_literals;

inline time_point now() {
    return std::chrono::steady_clock::now();
}

inline time_point_system nowSystem() {
    return std::chrono::system_clock::now();
}

inline size_t getTimestampMs(const time_point_system &tp) {
    return std::chrono::duration_cast<milliseconds>(tp.time_since_epoch()).count();
}

inline size_t getSecondNumber(const size_t millisecond) {
    const milliseconds ms(millisecond);
    return std::chrono::duration_cast<seconds>(ms).count();    
}

inline size_t getHourNumber(const size_t millisecond) {
    const milliseconds ms(millisecond);
    return std::chrono::duration_cast<hours>(ms).count();
}

inline size_t getDayNumber(const size_t millisecond) {
    return getHourNumber(millisecond) / 24;
}

class Timer {
public:
    
    Timer() 
        : beginTime(::now())
    {}
    
    void stop() {
        stoppedTime = ::now();
        isStopped = true;
    }
    
    size_t countMs() {
        if (!isStopped) {
            stop();
        }
        return std::chrono::duration_cast<milliseconds>(stoppedTime - beginTime).count();
    }
    
private:
    
    const time_point beginTime;
    
    bool isStopped = false;
    
    time_point stoppedTime;
    
};

#endif // DURATION_H_