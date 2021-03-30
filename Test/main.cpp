//
// Created by crablet on 2021/3/28.
//

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>

#include <sys/inotify.h>
#include <unistd.h>

using namespace std::literals;

enum class Behavior
{
    Include,
    Exclude,
    Normal  // 仅仅为了编译通过
};

class FileWatchLinux
{
    struct ActionDetails
    {
        std::string name;
        std::vector<std::function<bool(const std::string&)>> filterVec;
        std::unordered_map<int, std::function<void(const std::string&)>> actionMap;
    };

public:
    FileWatchLinux();
    ~FileWatchLinux();

    FileWatchLinux& Watch(const std::string &path);
    FileWatchLinux& FilterByExtension(Behavior b, const std::string &ext);
    FileWatchLinux& FilterByFilename(Behavior b, const std::string &name);

    template<typename Func>
    FileWatchLinux& OnCreate(Func f);

    template <typename Func>
    FileWatchLinux& OnDelete(Func f);

    template <typename Func>
    FileWatchLinux& OnAccess(Func f);

    void Start(Behavior b);
    void Stop();

private:
    std::vector<std::string> watchVec;
    std::unordered_map<std::string, ActionDetails> detailMap;
    std::vector<int> wdVec;
    int fd;
    std::string currentPath;    // 暂时先这么写，存的是在初始化过程中正在初始化的路径
    std::atomic_bool running;
};

FileWatchLinux& FileWatchLinux::Watch(const std::string &path)
{
    watchVec.push_back(path);
    detailMap[path].name = path;
    currentPath = path;

    return *this;
}

FileWatchLinux& FileWatchLinux::FilterByExtension(Behavior b, const std::string &ext)
{
    auto filter = [&](const std::string &name) -> bool
    {
        if (name.size() < ext.size())
        {
            return false;
        }
        else
        {
            const auto endsWith = [&]()
            {
                for (std::size_t i = name.size() - ext.size(); i < name.size(); ++i)
                {
                    if (name[i] != ext[i])
                    {
                        return false;
                    }
                }

                return true;
            }();

            if (b == Behavior::Include)
            {
                return endsWith;
            }
            else if (b == Behavior::Exclude)
            {
                return !endsWith;
            }
            else
            {
                return false;   // 暂时未写
            }
        }
    };
    detailMap[currentPath].filterVec.emplace_back(filter);

    return *this;
}

FileWatchLinux& FileWatchLinux::FilterByFilename(Behavior b, const std::string &name)
{
    auto filter = [&](const std::string &currentName) -> bool
    {
        const auto equal = currentName == name;
        if (b == Behavior::Include)
        {
            return equal;
        }
        else if (b == Behavior::Exclude)
        {
            return !equal;
        }
        else
        {
            return false;
        }
    };
    detailMap[currentPath].filterVec.emplace_back(filter);

    return *this;
}

template<typename Func>
FileWatchLinux& FileWatchLinux::OnCreate(Func f)
{
    detailMap[currentPath].actionMap[IN_CREATE] = std::move(f);

    return *this;
}

void FileWatchLinux::Start(Behavior b)
{
    for (const auto &r : watchVec)
    {
        wdVec.push_back(inotify_add_watch(fd, r.c_str(), IN_ALL_EVENTS));
    }

    currentPath.clear();

    std::thread filewatcherThread(
    [&]()
    {
        running = true;
        while (running)
        {
            char buffer[(sizeof(inotify_event) + 16)] = { 0 };
            auto numRead = read(fd, buffer, sizeof(buffer));
            for (char *p = buffer; p < buffer + numRead; )
            {
                auto *event = reinterpret_cast<inotify_event*>(p);
                std::string name{event->name, event->len};
                if (event->mask & IN_CREATE)
                {
                    for (auto &r : detailMap)
                    {
                        r.second.actionMap[IN_CREATE](name);
                    }
                }
                else if (event->mask & IN_DELETE)
                {
                    for (auto &r : detailMap)
                    {
                        r.second.actionMap[IN_DELETE](name);
                    }
                }
                else if (event->mask & IN_ACCESS)
                {
                    for (auto &r : detailMap)
                    {
                        r.second.actionMap[IN_ACCESS](name);
                    }
                }
                p += sizeof(inotify_event) + event->len;
            }
        }
    });
    filewatcherThread.detach();
}

FileWatchLinux::FileWatchLinux()
        : fd{inotify_init()}, running{false}
{
}

FileWatchLinux::~FileWatchLinux()
{
    for (const auto &r : wdVec)
    {
        inotify_rm_watch(fd, r);
    }

    close(fd);
}

void FileWatchLinux::Stop()
{
    running = false;
}

template <typename Func>
FileWatchLinux &FileWatchLinux::OnDelete(Func f)
{
    detailMap[currentPath].actionMap[IN_DELETE] = std::move(f);

    return *this;
}

template <typename Func>
FileWatchLinux &FileWatchLinux::OnAccess(Func f)
{
    detailMap[currentPath].actionMap[IN_ACCESS] = std::move(f);

    return *this;}

int main()
{
    FileWatchLinux fileWatch{};
    fileWatch.Watch("/home/crablet/桌面/test/")
            .FilterByExtension(Behavior::Include, ".txt")
            .OnCreate([](const std::string &name)
                      { std::cout << name << '\n'; })
             .Start(Behavior::Normal);
    std::this_thread::sleep_for(1min);
    fileWatch.Stop();

    return 0;
}
