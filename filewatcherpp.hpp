//    Copyright (C) 2021  Qi Chen <love4uandgzfc@gmail.com>
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

//
// Created by crablet on 2021/4/1.
//

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
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
    Equal,
    Unequal,
    Normal  // 仅仅为了编译通过
};

enum class Option
{
    Debug = 1 << 0,
};

class FileWatcherBase
{
    struct ActionDetails
    {
        std::string name;
        std::vector<std::function<bool(const std::string&)>> filterVec;
        std::unordered_map<int, std::function<void(const std::string&)>> actionMap;

        std::unordered_set<std::string> extInclude;    // 过滤器要留下的扩展名
        std::unordered_set<std::string> extExclude;    // 过滤器要排除的扩展名
        std::unordered_set<std::string> nameInclude;   // 过滤器要留下的文件名
        std::unordered_set<std::string> nameExclude;   // 过滤器要排除的文件名
        std::unordered_set<std::string> nameEqual;     // 过滤器要留下的文件名
        std::unordered_set<std::string> nameUnequal;   // 过滤器要排除的文件名

        bool DoFilterByExtension(const std::string &name);
    };

public:
    FileWatcherBase();
    virtual ~FileWatcherBase() = default;

    // 添加path进入观察队列
    FileWatcherBase& Watch(const std::string &path);

    // 根据b确定的行为过滤带有特定后缀的文件
    FileWatcherBase& FilterByExtension(Behavior b, const std::string &ext);

    // 根据b确定的行为过滤带有特定后缀的文件
    FileWatcherBase& FilterByExtension(Behavior b, std::initializer_list<std::string> extList);

    // 根据b确定的行为过滤带有特定名称的文件
    FileWatcherBase& FilterByFilename(Behavior b, const std::string &name);

    // 根据b确定的行为过滤带有特定名称的文件
    FileWatcherBase& FilterByFilename(Behavior b, std::initializer_list<std::string> nameList);

    // 当被观察文件（夹）有创建动作时会执行回调函数f
    virtual FileWatcherBase& OnCreate(std::function<void(const std::string)> f) = 0;

    // 当被观察文件（夹）有删除动作时会执行回调函数f
    virtual FileWatcherBase& OnDelete(std::function<void(const std::string)> f) = 0;
    virtual FileWatcherBase& OnAccess(std::function<void(const std::string)> f) = 0;

    FileWatcherBase& SetOption(Option o);

    virtual void Start(Behavior b) = 0; // 根据Bahavior的行为开始观察
    void Stop();

protected:
    std::vector<std::string> watchVec;  // 被监控的路径名的集合
    std::unordered_map<std::string, ActionDetails> detailMap;   // key: 被监控的路径名; value: 被监控的地方有事件了可能会执行的操作
    std::vector<int> wdVec;     // 观察返回的wd的集合
    std::string currentPath;    // 暂时先这么写，存的是在初始化过程中正在初始化的路径
    std::atomic_bool running;   // 控制开始和停止

    int option; // 通过选项来控制程序除回调函数外的其他行为

    constexpr static auto MAXNAMELEN = 320; // 被监控文件的最大长度
};

FileWatcherBase& FileWatcherBase::Watch(const std::string &path)
{
    watchVec.push_back(path);
    detailMap[path].name = path;
    currentPath = path;

    return *this;
}

FileWatcherBase& FileWatcherBase::FilterByExtension(Behavior b, const std::string &ext)
{
    auto filter = [&](const std::string &name) -> bool
    {
        if (name.size() < ext.size())
        {
            return false;
        }
        else
        {
            const auto endsWith = name.compare(name.size() - ext.size(), ext.size(), ext) == 0;

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
                return false;   // 这里应该会报错
            }
        }
    };
    detailMap[currentPath].filterVec.emplace_back(filter);

    return *this;
}

FileWatcherBase& FileWatcherBase::FilterByFilename(Behavior b, const std::string &name)
{
    auto filter = [&](const std::string &currentName) -> bool
    {
        const auto equal = currentName == name;
        const auto include = currentName.find(name) != std::string::npos;

        if (b == Behavior::Include)
        {
            return include;
        }
        else if (b == Behavior::Exclude)
        {
            return !include;
        }
        else if (b == Behavior::Equal)
        {
            return equal;
        }
        else if (b == Behavior::Unequal)
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

FileWatcherBase::FileWatcherBase()
        : running{false}, option{}
{
}

void FileWatcherBase::Stop()
{
    running = false;
}

FileWatcherBase &FileWatcherBase::SetOption(Option o)
{
    option |= static_cast<int>(o);

    return *this;
}

// TODO: 后面不应该由filter直接捕获列表，而是在类中保存几个std::vector<std::string>或类似的容器然后Inlcude加，Exclude减，最后计算出要留下的文件
FileWatcherBase& FileWatcherBase::FilterByExtension(Behavior b, std::initializer_list<std::string> extList)
{
    auto filter = [&](const std::string &name) -> bool
    {
        for (const auto &ext : extList)
        {
            if (name.size() < ext.size())
            {
                continue;
            }
            else
            {
                const auto endsWith = name.compare(name.size() - ext.size(), ext.size(), ext) == 0;

                if (b == Behavior::Include)
                {
                    if (endsWith)
                    {
                        return true;
                    }
                }
                else if (b == Behavior::Exclude)    // FIXME: 不是只要有一个不符合就返回true的，这个逻辑有问题
                {
                    if (!endsWith)
                    {
                        return true;
                    }
                }
                else
                {
                    // 这里应该会报错
                }
            }
        }
        return false;
    };
    detailMap[currentPath].filterVec.emplace_back(filter);

    return *this;
}

FileWatcherBase &FileWatcherBase::FilterByFilename(Behavior b, std::initializer_list<std::string> nameList)
{
    auto filter = [&](const std::string &currentName) -> bool
    {
        const auto equal = std::any_of(nameList.begin(), nameList.end(), [&](const std::string &name)
                           {
                               return currentName == name;
                           });
        const auto include = std::any_of(nameList.begin(), nameList.end(), [&](const std::string &name)
                             {
                                 return currentName.find(name) != std::string::npos;
                             });

        if (b == Behavior::Include)
        {
            return include;
        }
        else if (b == Behavior::Exclude)
        {
            return !include;
        }
        else if (b == Behavior::Equal)
        {
            return equal;
        }
        else if (b == Behavior::Unequal)
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

bool FileWatcherBase::ActionDetails::DoFilterByExtension(const std::string &name)
{
    return std::any_of(extInclude.cbegin(), extInclude.cend(),
                       [&](const std::string &ext)
                       {
                           return name.compare(name.size() - ext.size(), ext.size(), ext) == 0;
                       })
       && std::all_of(extExclude.cbegin(), extExclude.cend(),
                      [&](const std::string &ext)
                      {
                          return name.compare(name.size() - ext.size(), ext.size(), ext) != 0;
                      });
}

class FileWatcherLinux : public FileWatcherBase
{
public:
    FileWatcherLinux();
    ~FileWatcherLinux() override;

    FileWatcherBase& OnCreate(std::function<void(const std::string)> f) override;
    FileWatcherBase& OnDelete(std::function<void(const std::string)> f) override;
    FileWatcherBase& OnAccess(std::function<void(const std::string)> f) override;

    void Start(Behavior b) override;

private:
    int fd;
};

void FileWatcherLinux::Start(Behavior b)
{
    if (option & static_cast<int>(Option::Debug))
    {
        std::cout << "FileWatcherLinux is starting.\n";
    }

    for (const auto &r : watchVec)
    {
        wdVec.push_back(inotify_add_watch(fd, r.c_str(), IN_ALL_EVENTS));
    }

    currentPath.clear();

    std::thread filewatcherThread(
    [&]()
    {
        if (option & static_cast<int>(Option::Debug))
        {
            std::cout << "FileWatcherLinux is running.\n";
        }

        running = true;
        while (running)
        {
            char buffer[(sizeof(inotify_event) + MAXNAMELEN)] = { 0 };
            auto numRead = read(fd, buffer, sizeof(buffer));
            for (char *p = buffer; p < buffer + numRead; )
            {
                auto *event = reinterpret_cast<inotify_event*>(p);
                std::string name{event->name, event->len};
                name.erase(std::find(name.begin(), name.end(), '\0'), name.end());  // 删除末尾所有多余的\0

                auto eventMaskAction = [&](int eventMask)
                {
                    if (event->mask & eventMask)
                    {
                        for (auto &r : detailMap)   // 遍历所有的监控目录
                        {
                            auto fPtr = r.second.actionMap.find(eventMask);
                            auto filters = r.second.filterVec;
                            if (fPtr != r.second.actionMap.end())   // 如果某个监控目录有对IN_CREATE的反应
                            {
                                // 遍历所有的过滤器，只要有过滤器返回true即接受
                                bool ok = std::any_of(filters.begin(), filters.end(),
                                                      [&](auto &f)
                                                      {
                                                          return f(name);
                                                      });
                                ok &= r.second.DoFilterByExtension(name);
                                if (ok)
                                {
                                    fPtr->second(name); // 就去执行相应该有的反应
                                }
                            }
                        }
                    }
                };

                eventMaskAction(IN_CREATE);
                eventMaskAction(IN_DELETE);
                eventMaskAction(IN_ACCESS);

                p += sizeof(inotify_event) + event->len;
            }
        }
    });
    filewatcherThread.detach();
}

FileWatcherLinux::~FileWatcherLinux()
{
    if (option & static_cast<int>(Option::Debug))   // 检测是否存在Debug选项，这个操作应该封装成函数或宏
    {
        std::cout << "FileWatcherLinux is closing.\n";
    }

    for (const auto &r : wdVec)
    {
        inotify_rm_watch(fd, r);
    }

    close(fd);
}

FileWatcherBase& FileWatcherLinux::OnCreate(std::function<void(const std::string)> f)
{
    detailMap[currentPath].actionMap[IN_CREATE] = std::move(f);

    return *this;
}

FileWatcherBase& FileWatcherLinux::OnDelete(std::function<void(const std::string)> f)
{
    detailMap[currentPath].actionMap[IN_DELETE] = std::move(f);

    return *this;
}

FileWatcherBase& FileWatcherLinux::OnAccess(std::function<void(const std::string)> f)
{
    detailMap[currentPath].actionMap[IN_ACCESS] = std::move(f);

    return *this;
}

FileWatcherLinux::FileWatcherLinux()
        : fd{inotify_init()}
{
}
