#pragma once

#include<iostream>
#include<string>

class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    //static time_t nnn();好奇
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};