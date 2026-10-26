<h1 align="center">piggywrite</h1>

<p align="center">
  <strong>一个轻量、支持中文的 C++17 命令行文本编辑器</strong><br>
  <sub>A tiny UTF-8 terminal text editor for Windows.</sub>
</p>

<p align="center">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white">
  <img alt="Windows 10/11 x64" src="https://img.shields.io/badge/Windows-10%2F11%20x64-0078D4?logo=windows11&logoColor=white">
  <img alt="Version" src="https://img.shields.io/badge/version-1.0-brightgreen">
  <img alt="Inno Setup 7" src="https://img.shields.io/badge/installer-Inno%20Setup%207-2F4F4F">
</p>

<p align="center">
  <a href="https://gango-studio.github.io/">官网</a>
  ·
  <a href="#安装">安装</a>
  ·
  <a href="#使用">使用</a>
  ·
  <a href="#从源码构建">从源码构建</a>
  ·
  <a href="#制作安装包">制作安装包</a>
</p>

---

**piggywrite** 是一个用 C++17 编写的极简命令行文本编辑器。它没有第三方库依赖，启动后是浏览模式，按 `t` 进入编辑，按 `Esc` 回到浏览，再输入 `#s` 保存退出或 `#q` 放弃退出。

它面向 Windows Terminal / PowerShell，支持 UTF-8 中文、完整的光标移动、文件 BOM 和换行风格保留。Windows 安装包会按当前用户安装，无需管理员权限，并自动把 `piggywrite` 加入用户 `PATH`。

## 特性

- **纯 C++17**：无第三方库，单个可执行文件，静态链接 MinGW 运行库。
- **中文友好**：内部按 Unicode 码点编辑，删除中文不会破坏 UTF-8 字节。
- **双模式操作**：浏览模式负责导航和保存命令，编辑模式专注输入。
- **完整键盘操作**：方向键、Home/End、PageUp/PageDown、Enter、Backspace、Delete、Tab。
- **保留文件细节**：读取和保存 UTF-8 BOM、CRLF/LF 换行风格、文件末尾换行。
- **保存失败不退出**：权限不足或目录不存在时在状态栏显示错误，不丢失当前内容。
- **轻量终端界面**：行号、状态栏、未保存提示、自动滚动，使用备用屏幕并在退出后恢复终端。
- **一键安装**：Inno Setup 安装包自动创建快捷方式、加入 `PATH`、提供卸载程序。
- **完全离线**：不联网、不收集数据。

## 安装

### 方式一：Windows 安装包（推荐）

1. 打开本仓库的 **Releases** 页面。
2. 下载 `piggywrite-1.0-setup.exe`。
3. 双击运行安装程序。
4. 安装完成后，**重新打开 PowerShell 或 Windows Terminal**。
5. 输入：

```powershell
piggywrite notes.txt
```

安装程序会：

- 按当前用户安装到 `%LOCALAPPDATA%\Programs\piggywrite`
- 不需要管理员权限
- 自动把安装目录加入用户 `PATH`
- 创建开始菜单快捷方式，可选创建桌面快捷方式
- 安装卸载程序；卸载时自动移除 `PATH` 条目

> 安装包目前未做数字签名，Windows SmartScreen 可能提示“未知发布者”。
> 可点击“更多信息 → 仍要运行”。正式分发建议使用代码签名证书。

### 方式二：从源码构建

见下方的 [从源码构建](#从源码构建)。

## 使用

### 命令行

```powershell
piggywrite
piggywrite notes.txt
piggywrite "C:\Users\Gango\Documents\my notes.txt"
piggywrite --help
```

| 命令 | 作用 |
| --- | --- |
| `piggywrite` | 打开当前目录的 `untitled.txt`，不存在则新建 |
| `piggywrite <文件>` | 打开指定文件；不存在则新建，保存时写入磁盘 |
| `piggywrite --help` | 显示帮助 |
| `piggywrite -h` | 显示帮助 |
| `piggywrite /?` | 显示帮助 |

说明：

- 相对路径基于启动时的当前目录。
- 一次只能打开一个文件。
- 程序内没有“另存为”；保存目标固定为启动时指定的文件。
- 文件不存在时，只有执行保存命令才会真正创建文件。

### 操作流程

```text
启动
  ↓
按 t 进入编辑模式
  ↓
输入内容
  ↓
按 Esc 回到浏览模式
  ↓
输入 #s 保存并退出
  或 #q 不保存退出
```

### 浏览模式按键

| 按键 | 作用 |
| --- | --- |
| `t` / `T` | 进入编辑模式 |
| `#s` | 保存并退出 |
| `#q` | 不保存，直接退出 |
| `Esc` | 退出；有未保存修改时需按两次确认 |
| `Ctrl+S` | 保存并退出 |
| `Ctrl+Q` | 不保存退出 |
| `Ctrl+C` | 等同于 `Esc` |
| `←` `→` `↑` `↓` | 移动光标 |
| `Home` / `End` | 行首 / 行尾 |
| `PageUp` / `PageDown` | 向上 / 向下移动 10 行 |

命令前缀说明：

- 先按 `#`，再按小写 `s` 或 `q`，命令会立即执行，不需要回车。
- `#` 后按其他键会取消命令前缀。
- `#S`、`#Q` 不会被识别为命令。
- `#s` / `#q` **只在浏览模式生效**，所以正文中可以正常输入这两个字符串。

### 编辑模式按键

| 按键 | 作用 |
| --- | --- |
| 普通字符 | 插入文本，支持中文和 UTF-8 |
| `Enter` | 换行 |
| `Backspace` | 删除光标前一个字符；行首会合并上一行 |
| `Delete` | 删除光标处字符；行尾会合并下一行 |
| `Tab` | 插入制表符，按 4 列宽度显示 |
| `←` `→` `↑` `↓` | 移动光标 |
| `Home` / `End` | 行首 / 行尾 |
| `PageUp` / `PageDown` | 向上 / 向下移动 10 行 |
| `Esc` | 返回浏览模式 |
| `Ctrl+S` | 保存并退出 |
| `Ctrl+Q` | 不保存退出 |
| `Ctrl+C` | 等同于 `Esc` |

## 保存与退出

- `#s` / `Ctrl+S`：写入启动时指定的文件，成功后退出。
- 保存失败时程序不会退出，状态栏会显示红色错误信息。
- `#q` / `Ctrl+Q`：直接退出，不保存，也不弹出确认。
- 浏览模式按 `Esc` 时，如果存在未保存修改，需要再按一次确认。
- 程序不会自动创建不存在的父目录；目标目录不存在时保存会失败。
- 直接关闭终端窗口可能导致未保存内容丢失。

## 文件处理

- 磁盘读写使用 UTF-8，内部使用 UTF-32 码点编辑。
- 读取时识别并保留 UTF-8 BOM。
- 保留原文件的 CRLF/LF 换行风格和末尾换行。
- 非法 UTF-8 文件会被拒绝加载，并给出行内字节偏移，避免保存时破坏原文件。
- 新文件默认无 BOM、LF 换行。
- 不支持 GBK、UTF-16 等编码的自动转换。

## 从源码构建

### 环境要求

- CMake 3.16+
- 任意 C++17 编译器（推荐 MinGW-w64 或 Visual Studio）
- Windows 10/11 x64

### 使用 CMake

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

生成：

```text
build\piggywrite.exe
```

如果使用 Visual Studio，可以去掉 `-G "MinGW Makefiles"`，让 CMake 选择默认生成器。

### 使用 g++ 直接编译

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    src/main.cpp src/terminal.cpp src/editor.cpp -o piggywrite.exe `
    -lshell32 -static-libgcc -static-libstdc++
```

### 一键构建脚本

在 Windows + MinGW 环境下，也可以直接运行：

```powershell
.\build.bat
```

## 制作安装包

安装 [Inno Setup 7](https://jrsoftware.org/isinfo.php) 后运行：

```powershell
.\installer\build-installer.bat
```

脚本会先编译程序，再生成：

```text
installer\dist\piggywrite-1.0-setup.exe
```

安装脚本源码：[`installer/piggywrite.iss`](installer/piggywrite.iss)

当前安装包配置：

- 发布者：Gango Studios & Sam Mu
- 官网：<https://gango-studio.github.io/>
- 按当前用户安装，免管理员权限
- 自动加入用户 `PATH`
- 支持 64 位 Windows 10/11
- 中文安装界面

## 项目结构

```text
piggywrite/
├─ src/
│  ├─ main.cpp          # 命令行参数、界面绘制、主循环
│  ├─ editor.hpp        # 文档模型、键位和模式接口
│  ├─ editor.cpp        # Unicode、文档编辑、模式状态机
│  ├─ terminal.hpp      # 终端抽象
│  └─ terminal.cpp      # Windows / POSIX 按键与 ANSI 输出
├─ tests/
│  └─ test_editor.cpp   # 文档、光标、UTF-8、命令等自动测试
├─ installer/
│  ├─ piggywrite.iss    # Inno Setup 安装脚本
│  └─ build-installer.bat
├─ CMakeLists.txt
├─ build.bat
└─ README.md
```

## 测试

```powershell
ctest --test-dir build --output-on-failure
```

测试覆盖：

- 文本插入、删除、换行和光标移动
- 中文和 UTF-8 编解码
- BOM、CRLF/LF、末尾换行
- 非法 UTF-8
- 浏览/编辑模式和 `#s` / `#q` 命令
- 保存失败和未保存退出确认


## 作者

**Gango Studios & Sam Mu**

- 官网：<https://gango-studio.github.io/>
- 安装包构建：[Inno Setup 7](https://jrsoftware.org/isinfo.php)

---

如果这个项目对你有帮助，欢迎在 GitHub 上点一个 Star。
