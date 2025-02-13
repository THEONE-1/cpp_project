#include "Timestamp.hh"

Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch) : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

Timestamp Timestamp::now() {
    return Timestamp(time(NULL));
}

std::string Timestamp::toString() const {
    char buf[64]={0};
    tm* tm_time = localtime(&microSecondsSinceEpoch_);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_time);
    return buf;
}