//
// Created by crablet on 2021/3/28.
//

#include <chrono>

#include "filewatcherpp.hpp"

using namespace std::literals;

int main()
{
    FileWatchLinux fileWatch{};
    fileWatch.Watch("/home/crablet/桌面/test/")
             .FilterByExtension(Behavior::Include, ".txt")
             .OnCreate([](const std::string &name)
                      { std::cout << name << '\n'; })
             .Start(Behavior::Normal);
    std::this_thread::sleep_for(5s);
    fileWatch.Stop();

    return 0;
}
