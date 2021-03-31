//
// Created by crablet on 2021/4/1.
//

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <functional>
#include <thread>
#include <atomic>

#include <sys/inotify.h>
#include <unistd.h>

enum class Behavior
{
    Include,
    Exclude,
    Normal  // 仅仅为了编译通过
};

class FileWatchBase
{
    struct ActionDetails
    {
        std::string name;
        std::vector<std::function<bool(const std::string&)>> filterVec;
        std::unordered_map<int, std::function<void(const std::string&)>> actionMap;
    };

public:
    FileWatchBase();
    virtual ~FileWatchBase() = default;

    FileWatchBase& Watch(const std::string &path);
    FileWatchBase& FilterByExtension(Behavior b, const std::string &ext);
    FileWatchBase& FilterByFilename(Behavior b, const std::string &name);

    virtual FileWatchBase& OnCreate(std::function<void(const std::string)> f) = 0;
    virtual FileWatchBase& OnDelete(std::function<void(const std::string)> f) = 0;
    virtual FileWatchBase& OnAccess(std::function<void(const std::string)> f) = 0;

    virtual void Start(Behavior b) = 0;
    void Stop();

protected:
    std::vector<std::string> watchVec;
    std::unordered_map<std::string, ActionDetails> detailMap;
    std::vector<int> wdVec;
    std::string currentPath;    // 暂时先这么写，存的是在初始化过程中正在初始化的路径
    std::atomic_bool running;

    constexpr auto MAXNAMELEN = 320;
};

FileWatchBase& FileWatchBase::Watch(const std::string &path)
{
    watchVec.push_back(path);
    detailMap[path].name = path;
    currentPath = path;

    return *this;
}

FileWatchBase& FileWatchBase::FilterByExtension(Behavior b, const std::string &ext)
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

FileWatchBase& FileWatchBase::FilterByFilename(Behavior b, const std::string &name)
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

FileWatchBase::FileWatchBase()
        : running{false}
{
}

void FileWatchBase::Stop()
{
    running = false;
}

class FileWatchLinux : public FileWatchBase
{
public:
    FileWatchLinux();
    ~FileWatchLinux() override;

    FileWatchBase& OnCreate(std::function<void(const std::string)> f) override;
    FileWatchBase& OnDelete(std::function<void(const std::string)> f) override;
    FileWatchBase& OnAccess(std::function<void(const std::string)> f) override;

    void Start(Behavior b) override;

private:
    int fd;
};

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
            char buffer[(sizeof(inotify_event) + MAXNAMELEN)] = { 0 };
            auto numRead = read(fd, buffer, sizeof(buffer));
            for (char *p = buffer; p < buffer + numRead; )
            {
                auto *event = reinterpret_cast<inotify_event*>(p);
                const std::string name{event->name, event->len};
                if (event->mask & IN_CREATE)
                {
                    for (auto &r : detailMap)
                    {
                        auto fPtr = r.second.actionMap.find(IN_CREATE);
                        if (fPtr != r.second.actionMap.end())
                        {
                            fPtr->second(name);
                        }
                    }
                }
                else if (event->mask & IN_DELETE)
                {
                    for (auto &r : detailMap)
                    {
                        auto fPtr = r.second.actionMap.find(IN_DELETE);
                        if (fPtr != r.second.actionMap.end())
                        {
                            fPtr->second(name);
                        }
                    }
                }
                else if (event->mask & IN_ACCESS)
                {
                    for (auto &r : detailMap)
                    {
                        auto fPtr = r.second.actionMap.find(IN_ACCESS);
                        if (fPtr != r.second.actionMap.end())
                        {
                            fPtr->second(name);
                        }
                    }
                }
                p += sizeof(inotify_event) + event->len;
            }
        }
    });
    filewatcherThread.detach();
}

FileWatchLinux::~FileWatchLinux()
{
    for (const auto &r : wdVec)
    {
        inotify_rm_watch(fd, r);
    }

    close(fd);
}

FileWatchBase& FileWatchLinux::OnCreate(std::function<void(const std::string)> f)
{
    detailMap[currentPath].actionMap[IN_CREATE] = std::move(f);

    return *this;
}

FileWatchBase& FileWatchLinux::OnDelete(std::function<void(const std::string)> f)
{
    detailMap[currentPath].actionMap[IN_DELETE] = std::move(f);

    return *this;
}

FileWatchBase& FileWatchLinux::OnAccess(std::function<void(const std::string)> f)
{
    detailMap[currentPath].actionMap[IN_ACCESS] = std::move(f);

    return *this;
}

FileWatchLinux::FileWatchLinux()
        : fd{inotify_init()}
{
}
