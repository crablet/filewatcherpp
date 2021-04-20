//
// Created by crablet on 2021/3/28.
//

#include <chrono>

#include "../filewatcherpp.hpp"

using namespace std::literals;

int main()
{
    FileWatcherLinux fileWatcher{};
    fileWatcher.Watch("/home/chenqi31/桌面/test/")
               .FilterByExtension(Behavior::Include, ".txt")
               .FilterByExtension(Behavior::Include, { ".exe", ".jpg", ".pdf" })
               .OnCreate([](const std::string &name)
                        { std::cout << name << '\n'; })
               .SetOption(Option::Debug)
               .Start(Behavior::Normal);
    std::this_thread::sleep_for(5min);
    fileWatcher.Stop();

    return 0;
}
