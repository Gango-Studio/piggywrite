#pragma once

#include <string>

#ifndef _WIN32
#include <termios.h>
#endif

#include "editor.hpp"

namespace piggywrite {

class Terminal {
public:
    Terminal() = default;
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    bool init(std::string& error);
    void shutdown();

    Key readKey();
    int columns() const;
    int rows() const;

    void write(const std::string& utf8);

private:
    bool initialized_ = false;

#ifdef _WIN32
    void* inputHandle_ = nullptr;
    void* outputHandle_ = nullptr;
    unsigned long oldInputMode_ = 0;
    unsigned long oldOutputMode_ = 0;
#else
    struct termios oldTermios_ {};
    bool termiosSaved_ = false;
#endif
};

}  // namespace piggywrite
