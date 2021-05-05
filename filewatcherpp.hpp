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
#include <algorithm>

class FileWatcherLinux;
class FileWatcherWindows;
class FileWatcherMacOS;

#if defined(__linux__) || defined(linux) || defined(__linux)
#include <sys/inotify.h>
#include <unistd.h>

using FileWatcher = FileWatcherLinux;

#elif defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <WinBase.h>
#include <fileapi.h>

using FileWatcher = FileWatcherWindows;

#elif defined(__APPLE__) || defined(__MACH__)
using FileWatcher = FileWatcherMacOS;
#endif

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
    Debug = 1 << 0, // 打开Debug模式，在控制台会有一些输出
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

        // 真正执行按文件扩展名过滤的函数
        bool DoFilterByExtension(const std::string &name) const;

        // 真正执行按文件名过滤的函数
        bool DoFilterByFilename(const std::string &name) const;
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

    // 根据用户传入的函数f过滤特定的文件，f返回true则留下，返回false则不留下
    FileWatcherBase& FilterByUserDefined(std::function<bool(const std::string&)> f);

    // 当被观察文件（夹）有创建动作时会执行回调函数f
    virtual FileWatcherBase& OnCreate(std::function<void(const std::string&)> f) = 0;

    // 当被观察文件（夹）有删除动作时会执行回调函数f
    virtual FileWatcherBase& OnDelete(std::function<void(const std::string&)> f) = 0;

    // 当被观察文件（夹）有访问动作时会执行回调函数f
    virtual FileWatcherBase& OnAccess(std::function<void(const std::string&)> f) = 0;

    // 当被观察文件（夹）有修改动作时会执行回调函数f
    virtual FileWatcherBase& OnModified(std::function<void(const std::string&)> f) = 0;

    // 选择执行时候的选项
    FileWatcherBase& SetOption(Option o);

    // 根据Bahavior的行为开始观察
    virtual void Start(Behavior b) = 0; 

    // 结束观察
    void Stop();

protected:
    std::vector<std::string> watchVec;  // 被监控的路径名的集合
    std::unordered_map<std::string, ActionDetails> detailMap;   // key: 被监控的路径名; value: 被监控的地方有事件了可能会执行的操作
    std::vector<int> wdVec;     // 观察返回的wd的集合
    std::string currentPath;    // 暂时先这么写，存的是在初始化过程中正在初始化的路径ywnwa.11

    std::atomic_bool running;   // 控制开始和停止

    int option; // 通过选项来控制程序除回调函数外的其他行为

    constexpr static auto MAXNAMELEN = 320; // 被监控文件的最大长度
};

FileWatcherBase::FileWatcherBase()
    : running{false}, option{}
{
}

FileWatcherBase& FileWatcherBase::Watch(const std::string &path)
{
    watchVec.push_back(path);       // 添加路径至被监控的集合
    detailMap[path].name = path;    // 创建一个与之相关的detailMap对象，顺便记录下路径
    currentPath = path;             // 记录下前正在处理的路径，这是暂时性的策略

    return *this;
}

FileWatcherBase& FileWatcherBase::FilterByExtension(Behavior b, const std::string &ext)
{
    switch (b)
    {
        case Behavior::Include:
            detailMap[currentPath].extInclude.insert(ext);

            break;

        case Behavior::Exclude:
            detailMap[currentPath].extExclude.insert(ext);

            break;

        default:
            // 报错
            break;
    }

    return *this;
}

FileWatcherBase& FileWatcherBase::FilterByExtension(Behavior b, std::initializer_list<std::string> extList)
{
    switch (b)
    {
        case Behavior::Include:
            detailMap[currentPath].extInclude.insert(extList);

            break;

        case Behavior::Exclude:
            detailMap[currentPath].extExclude.insert(extList);

            break;

        default:
            // 报错
            break;
    }

    return *this;
}

bool FileWatcherBase::ActionDetails::DoFilterByExtension(const std::string &name) const
{
    // 过滤出使得`extInclude == false && extInclude == true`为真的文件名
    return std::any_of(extInclude.cbegin(), extInclude.cend(),
                       [&](const std::string &ext)
                       {
                           if (name.size() < ext.size())
                           {
                               return false;
                           }
                           else
                           {
                               return name.compare(name.size() - ext.size(), ext.size(), ext) == 0;
                           }
                       })
        && std::all_of(extExclude.cbegin(), extExclude.cend(),
                       [&](const std::string &ext)
                       {
                           if (name.size() < ext.size())
                           {
                               return true;
                           }
                           else
                           {
                               return name.compare(name.size() - ext.size(), ext.size(), ext) != 0;
                           }
                       });
}

FileWatcherBase& FileWatcherBase::FilterByFilename(Behavior b, const std::string &name)
{
    switch (b)
    {
        case Behavior::Include:
            detailMap[currentPath].nameInclude.insert(name);

            break;

        case Behavior::Exclude:
            detailMap[currentPath].nameExclude.insert(name);

            break;

        case Behavior::Equal:
            detailMap[currentPath].nameEqual.insert(name);

            break;

        case Behavior::Unequal:
            detailMap[currentPath].nameUnequal.insert(name);

            break;

        default:
            // 报错

            break;
    }

    return *this;
}

FileWatcherBase& FileWatcherBase::FilterByFilename(Behavior b, std::initializer_list<std::string> nameList)
{
    switch (b)
    {
        case Behavior::Include:
            detailMap[currentPath].nameInclude.insert(nameList);

            break;

        case Behavior::Exclude:
            detailMap[currentPath].nameExclude.insert(nameList);

            break;

        case Behavior::Equal:
            detailMap[currentPath].nameEqual.insert(nameList);

            break;

        case Behavior::Unequal:
            detailMap[currentPath].nameUnequal.insert(nameList);

            break;

        default:
            // 报错

            break;
    }

    return *this;
}

bool FileWatcherBase::ActionDetails::DoFilterByFilename(const std::string &name) const
{
    // 过滤逻辑：只留下令`(include || equal) && (exclude || unequal)`为真的数据
    return (
                std::any_of(nameInclude.cbegin(), nameInclude.cend(),
                           [&](const std::string &nameInclude)
                           {
                               return name.find(nameInclude) != std::string::npos;
                           })
             ||
                std::any_of(nameEqual.cbegin(), nameEqual.cend(),
                            [&](const std::string &nameEqual)
                            {
                                return name == nameEqual;
                            })
           )
        &&
           (
                std::all_of(nameExclude.cbegin(), nameExclude.cend(),
                            [&](const std::string &nameExclude)
                            {
                                return name.find(nameExclude) == std::string::npos;
                            })
             ||  std::all_of(nameUnequal.cbegin(), nameUnequal.cend(),
                            [&](const std::string &nameUnequal)
                            {
                                return name != nameUnequal;
                            })
          );
}

void FileWatcherBase::Stop()
{
    // 暂停暂时就先令标志位为false然后线程自动退出
    // TODO: 标志位为false后响应函数也应该及时停止，不再进入
    running = false;
}

FileWatcherBase& FileWatcherBase::SetOption(Option o)
{
    // 添加选项全用|=运算符操作某一位
    option |= static_cast<int>(o);

    // TODO: 添加一个类似UnsetOption或者RemoveOption之类的接口

    return *this;
}

FileWatcherBase& FileWatcherBase::FilterByUserDefined(std::function<bool(const std::string&)> f)
{
    // 直接添加用户自定义的删除器到对应路径的filterVec中即可
    detailMap[currentPath].filterVec.push_back(std::move(f));

    return *this;
}

#if defined(__linux__) || defined(linux) || defined(__linux)
class FileWatcherLinux : public FileWatcherBase
{
public:
    FileWatcherLinux();
    ~FileWatcherLinux() override;

    FileWatcherBase& OnCreate(std::function<void(const std::string&)> f) override;
    FileWatcherBase& OnDelete(std::function<void(const std::string&)> f) override;
    FileWatcherBase& OnAccess(std::function<void(const std::string&)> f) override;
    FileWatcherBase& OnModified(std::function<void(const std::string&)> f) override;

    void Start(Behavior b) override;

private:
    int fd;
};

FileWatcherLinux::FileWatcherLinux()
    : fd{inotify_init()}
{
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
                std::string name{event->name, event->len};  // 文件名
                name.erase(std::find(name.begin(), name.end(), '\0'), name.end());  // 删除末尾所有多余的\0

                auto eventMaskAction = [&](int eventMask)
                {
                    if (event->mask & eventMask)
                    {
                        for (auto &r : detailMap)   // 遍历所有的监控目录
                        {
                            auto fPtr = r.second.actionMap.find(eventMask); // 不直接使用[]是因为[]无法判断找不到的情况
                            if (fPtr != r.second.actionMap.end())   // 如果某个监控目录有对IN_CREATE的反应
                            {
                                auto filters = r.second.filterVec;  // 所有过滤器的集合
                                // 遍历所有的过滤器，只要有过滤器返回true即接受
                                bool ok = std::any_of(filters.begin(), filters.end(),
                                                      [&](auto &f)
                                                      {
                                                          return f(name);
                                                      })
                                       || r.second.DoFilterByExtension(name)
                                       || r.second.DoFilterByFilename(name);
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

FileWatcherBase& FileWatcherLinux::OnCreate(std::function<void(const std::string&)> f)
{
    detailMap[currentPath].actionMap[IN_CREATE] = std::move(f);

    return *this;
}

FileWatcherBase& FileWatcherLinux::OnDelete(std::function<void(const std::string&)> f)
{
    detailMap[currentPath].actionMap[IN_DELETE] = std::move(f);

    return *this;
}

FileWatcherBase& FileWatcherLinux::OnAccess(std::function<void(const std::string&)> f)
{
    detailMap[currentPath].actionMap[IN_ACCESS] = std::move(f);

    return *this;
}

FileWatcherBase& FileWatcherLinux::OnModified(std::function<void(const std::string&)> f)
{
    detailMap[currentPath].actionMap[IN_MODIFY] = std::move(f);

    return *this;
}
#endif

#if defined(_WIN32) || defined(_WIN64)
class FileWatcherWindows : public FileWatcherBase
{
public:
    FileWatcherWindows();
    ~FileWatcherWindows() override;

    FileWatcherBase& OnCreate(std::function<void(const std::string&)> f) override;
    FileWatcherBase& OnDelete(std::function<void(const std::string&)> f) override;
    FileWatcherBase& OnAccess(std::function<void(const std::string&)> f) override;
    FileWatcherBase& OnModified(std::function<void(const std::string&)> f) override;

    void Start(Behavior b) override;
};

FileWatcherWindows::FileWatcherWindows()
{
}

FileWatcherWindows::~FileWatcherWindows()
{
}

FileWatcherBase& FileWatcherWindows::OnCreate(std::function<void(const std::string&)> f)
{
    return *this;
}

FileWatcherBase& FileWatcherWindows::OnDelete(std::function<void(const std::string&)> f)
{
    return *this;
}

FileWatcherBase& FileWatcherWindows::OnAccess(std::function<void(const std::string&)> f)
{
    return *this;
}

FileWatcherBase& FileWatcherWindows::OnModified(std::function<void(const std::string&)> f)
{
    return *this;
}

void FileWatcherWindows::Start(Behavior b)
{
    if (option & static_cast<int>(Option::Debug))
    {
        std::cout << "FileWatcherWindows is starting.\n";
    }

    currentPath.clear();

    // TODO: 如何支持多目录同时监控？Windows函数只支持单目录监控。
}

#endif

#if defined(__APPLE__) || defined(__MACH__)
class FileWatcherMacOS : public FileWatcherBase
{

};
#endif
