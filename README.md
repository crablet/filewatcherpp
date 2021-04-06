# FileWatcher++

这是一个基于C++11的单头文件跨平台文件监视库。  
您只需要包含**filewatcherpp.hpp**即可使用。  
建立该库的目的是为了提供一个更好用的C++跨平台文件监视器，并且尽量使得API通俗易懂，抛弃晦涩难懂的语法，零上手难度。  
该库仅使用C++标准库及操作系统标准接口，无任何第三方依赖。

## 例子
```c++
#include <chrono>

#include "filewatcherpp.hpp"

using namespace std::literals;

int main()
{
    FileWatchLinux fileWatch{};
    fileWatch.Watch("path/to/watch")
             .FilterByExtension(Behavior::Include, ".txt")
             .OnCreate([](const std::string &name)
                      { std::cout << name << '\n'; })
             .SetOption(Option::Debug)
             .Start(Behavior::Normal);
    std::this_thread::sleep_for(5min);
    fileWatch.Stop();

    return 0;
}
```