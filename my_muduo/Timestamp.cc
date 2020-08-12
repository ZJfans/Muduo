#include"Timestamp.h"
#include<time.h>

//无参构造
Timestamp::Timestamp()
    :microSecondsSinceEpoch_(0)
    {}
    //有参构造
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch)
    {}
//获取当前时间
Timestamp Timestamp::now()
{
    time_t ti = time(NULL);
    return Timestamp(time(NULL));
}
/*看了看time_t是个什么东西
time_t Timestamp::nnn(){
    return time(NULL);
}*/
std::string Timestamp::toString() const
{
    char buf[128] = {0};
    tm * tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf,128,"%4d/%02d/%02d %02d:%02d:%02d",
        tm_time->tm_year+1900,
        tm_time->tm_mon+1,
        tm_time->tm_mday,
        tm_time->tm_hour,
        tm_time->tm_min,
        tm_time->tm_sec
    );
    return buf;
}
//测试时间
/*#include<iostream>
int main(){
    std::cout << Timestamp::now().toString() << std::endl;
}*/

