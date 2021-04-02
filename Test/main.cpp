//
// Created by crablet on 2021/3/28.
//

#include <chrono>

#include "filewatcherpp.hpp"

using namespace std::literals;

int main()
{
    FileWatchLinux fileWatch{};
    fileWatch.Watch("/home/chenqi31/桌面/test/")
             .FilterByExtension(Behavior::Include, ".txt")
             .OnCreate([](const std::string &name)
                      { std::cout << name << '\n'; })
             .SetOption(Option::Debug)
             .Start(Behavior::Normal);
    std::this_thread::sleep_for(2min);
    fileWatch.Stop();

    return 0;
}
