#include "Logger.hh"
#include <iostream>
#include <ctime>
#include "Timestamp.hh"
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

void Logger::log(std::string msg)
{
    
    std::cout << "[" << Timestamp::now().toString() << "] " ;
    switch(logLevel_)
    {
        case LogLevel::INFO:
            std::cout << "[INFO] " << msg << std::endl;
            break;
        case LogLevel::ERROR:
            std::cout << "[ERROR] " << msg << std::endl;
            break;
        case LogLevel::FATAL:
            std::cout << "[FATAL] " << msg << std::endl;
            break;
        case LogLevel::DEBUG:
            std::cout << "[DEBUG] " << msg << std::endl;
            break;
        default:
            std::cout << "[UNKNOWN] " << msg << std::endl;
            break;
    }
   
}  

// int main(){
//     LOG_DEBUG("%s","test");
//     return 0;
// }