#ifndef _UTILITY_H_
#define _UTILITY_H_

/// Copyright(c) 2013 Frank Fang
///
/// Inline utility functions
///
/// @author Frank Fang (fanghm@gmail.com)
/// @date   2013/07/06

#include <iostream>     // std::cout, std::endl
#include <sstream>      // std::ostringstream
#include <iomanip>      // std::setfill, std::setw, std::setiosflags
#include <algorithm> 	// std::transform, std::find_if
#include <functional>   // std::ptr_fun, std::not1
#include <cctype>       // std::isspace
#include <stdlib.h>     // srand, rand 
#include <time.h>       // time 

// convert to upper case
static inline std::string upper(std::string &str) {
    std::transform(str.begin(), str.end(),str.begin(), ::toupper);
    return str;
}

// convert to lower case
static inline std::string lower(std::string &str) {
    std::transform(str.begin(), str.end(),str.begin(), ::tolower);
    return str;
}

// trim leading spaces
static inline std::string &ltrim(std::string &str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return str;
}

// trim trailing spaces
static inline std::string &rtrim(std::string &str) {
    str.erase(std::find_if(str.rbegin(), str.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), str.end());
    return str;
}

// trim spaces at both ends
static inline std::string &trim(std::string &str) {
    return ltrim(rtrim(str));
}

// convert binary data to hexadecimal string with hex prefix "0x"
static inline std::string tohex(std::string data, bool big_endian = false, bool upper_case = false) {
    if (data.empty()) return 0x00;

    std::ostringstream ret;
    ret << "0x";
    
    std::string::size_type len = data.length();
    unsigned int byte;
    
    for (std::string::size_type i = 0; i < len; ++i) {
        byte = (big_endian ? data[i]: data[len - i - 1]) & 0xff;
        
        ret << std::hex << std::setfill('0') << std::setw(2) 
        	<< (upper_case ? std::uppercase : std::nouppercase) 
        	<< byte;
    }

    return ret.str();
}

static inline std::string rands() {
    srand (static_cast<unsigned int>(time(NULL)));

    std::ostringstream os;
    os << rand();

    return os.str();
}

// tiny logging facility
// To use it, g_log_level must be defined in one of the cpp files

enum LogLevels {kError, kDebug, kInfo };
extern LogLevels g_log_level;

static void Log(enum LogLevels level, std::string msg) {
    if (level > g_log_level) return;

    std::ostringstream os;
    switch(level) {
    case kError:
        os << "ERROR: ";
        break;

    case kDebug:
        os << "DEBUG: ";
        break;

    default:
        os << "INFO: ";
    }

    os << msg;
    std::cout << os.str() << std::endl;
}

// logging shortcuts
static void Error(std::string msg) { Log(kError, msg); }
static void Debug(std::string msg) { Log(kDebug, msg); }
static void Info(std::string msg)  { Log(kInfo,  msg); }

#endif  // _UTILITY_H_
