# Piccolo Engine (formerly Pilot Engine)

<p align="center">
  <a href="https://games104.boomingtech.com">
    <img src="engine/source/editor/resource/PiccoloEngine.png" width="400" alt="Piccolo Engine logo">
  </a>
</p>

**Piccolo Engine** is a tiny game engine used for the [GAMES104](https://games104.boomingtech.com) course.

## Continuous build status

|    Build Type     |                                                                                      Status                                                                                      |
| :---------------: | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------: |
| **Build Windows** | [![Build Windows](https://github.com/BoomingTech/Piccolo/actions/workflows/build_windows.yml/badge.svg)](https://github.com/BoomingTech/Piccolo/actions/workflows/build_windows.yml) |
|  **Build Linux**  |    [![Build Linux](https://github.com/BoomingTech/Piccolo/actions/workflows/build_linux.yml/badge.svg)](https://github.com/BoomingTech/Piccolo/actions/workflows/build_linux.yml)    |
|  **Build macOS**  |    [![Build macOS](https://github.com/BoomingTech/Piccolo/actions/workflows/build_macos.yml/badge.svg)](https://github.com/BoomingTech/Piccolo/actions/workflows/build_macos.yml)    |

## Prerequisites

To build Piccolo, you must first install the following tools.

### Windows 10/11
- Visual Studio 2019 (or more recent)
- CMake 3.19 (or more recent)
- Git 2.1 (or more recent)

### macOS
- Xcode with the macOS SDK and command line tools
- CMake 3.21 (or more recent)
- Ninja
- Git 2.1 (or more recent)

### Ubuntu 20.04
 - apt install the following packages
```
sudo apt install libxrandr-dev
sudo apt install libxrender-dev
sudo apt install libxinerama-dev
sudo apt install libxcursor-dev
sudo apt install libxi-dev
sudo apt install libglvnd-dev
sudo apt install libvulkan-dev
sudo apt install cmake
sudo apt install clang
sudo apt install libc++-dev
sudo apt install libglew-dev
sudo apt install libglfw3-dev
sudo apt install vulkan-validationlayers
sudo apt install mesa-vulkan-drivers
```
- [NVIDIA driver](https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html#runfile) (The AMD and Intel driver is open-source, and thus is installed automatically by mesa-vulkan-drivers)

## Build Piccolo

### Build on Windows
You may execute the **build_windows.bat**. This batch file will generate the projects, and build the **Release** config of **Piccolo Engine** automatically. After successful build, you can find PiccoloEditor.exe in `bin/Release`.

Or you can use the following command to generate the **Visual Studio** project firstly, then open the solution in the build directory and build it manually.
```
cmake -S . -B build
```

### Build on macOS

The macOS presets use Ninja and the active Xcode toolchain. The build supports Apple Silicon and uses the bundled Vulkan libraries.

Run either command from the repository directory:

```sh
./build_macos.sh debug
./build_macos.sh release
```

The configurations use `build/macos-debug` and `build/macos-release`. Each build copies its executable, resources, and assets to `bin/Debug` or `bin/Release`.

```sh
./bin/Debug/PiccoloEditor
./bin/Release/PiccoloEditor
```

The scripts also copy the selected configuration's compilation database to `compile_commands.json` in the repository directory.

You can configure and build directly with CMake:

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug --parallel
```

Use `macos-release` for Release. Direct CMake commands generate the compilation database inside the build directory.

### VS Code

Open the repository directory in VS Code and install the extensions recommended by `.vscode/extensions.json`.

1. Run `CMake: Select Configure Preset` and select `macos-debug` or `macos-release`. CMake Tools copies its compilation database to the repository directory after configuration.
2. Run the default build task with `Cmd+Shift+B`. Use `Tasks: Run Build Task` to select the Release task.
3. Select `PiccoloEditor Debug` or `PiccoloEditor Release` in Run and Debug, then press F5. The launch configuration builds the selected version first.

The first build generates the reflection and shader headers required for code analysis. clangd reads the compiler flags, include directories, and definitions from `compile_commands.json`. The workspace disables Microsoft C/C++ IntelliSense to keep clangd as the language server.

GLSL IntelliSense searches `engine/shader/include` and uses the bundled macOS `glslangValidator` with Vulkan 1.0 rules, matching the shader build. The workspace associates shader headers and `.inl` files with GLSL. Run `GLSL: Restart Language Server` from the Command Palette to refresh an existing session.

### Build on Ubuntu 20.04
You can execute the **build_linux.sh** to build the binaries.

## Documentation
For documentation, please refer to the Wiki section.

## 课程实现

本分支参考 [GAMES104 课程作业](https://games104.boomingtech.com/sc/course-work/)，实现了 LUT 调色、暗角后处理、走跑跳动画、角色碰撞与跳跃参数编辑。修改原因、设计说明、代码阅读顺序和验证结果见 [LOG.md](LOG.md)。模块索引见 [ARCHITECTURE.md](ARCHITECTURE.md)。

默认角色支持 WASD 移动、Shift 加速、空格跳跃。编辑器选中 Player 后，展开 MotorComponent → MotorComponentRes，可修改 `m_jump_initial_speed`；`m_use_jump_initial_speed` 控制使用初速度还是原来的跳跃高度。

macOS 触控板在编辑器模式下支持“Option + 双指滑动”旋转视角：将指针放在场景视口内，按住任一侧 Option，横向滑动控制左右转动，纵向滑动控制俯仰。不按 Option 时，双指上下滑动仍用于缩放。该操作无需按下触控板。

## 暗角配置

独立的 VignettePass 位于 FXAA 之后、UI 合成之前。它让场景边缘逐渐变暗，保持界面文字的原始亮度。配置在 [rendering.global.json](engine/asset/global/rendering.global.json) 的 `vignette` 字段：

```json
"vignette": {
  "enabled": true,
  "strength": 0.35,
  "inner_radius": 0.5,
  "outer_radius": 1.5
}
```

`strength` 范围为 0 到 1，内外半径控制暗化的起止位置。半径 1 等于半个游戏视口高度。将 `enabled` 设为 false 可关闭效果。修改资源后重新构建并启动编辑器；程序中可调用 `RenderSystem::setVignetteSettings()`，让参数在下一帧生效。

## 运行测试

运行时回归测试使用 CTest，覆盖动画融合与状态切换、Jolt 碰撞、参数反射和暗角配置。测试直接链接生产运行时，无须创建窗口。macOS Debug 的命令如下：

```sh
cmake --preset macos-debug -DPICCOLO_BUILD_TESTS=ON
cmake --build build/macos-debug --parallel 6 --target \
  PiccoloEditor PiccoloAnimationTests PiccoloControllerTests PiccoloMotorTests PiccoloVignetteTests \
  PiccoloGameplaySmoke PiccoloVignetteGpuTests PiccoloPostProcessSmoke
ctest --test-dir build/macos-debug --output-on-failure
python3 -m unittest discover -s tests -p 'test_color_lut.py' -v
```

Release 使用 `macos-release` preset 和 `build/macos-release` 目录。其他平台可在原有 CMake 配置命令中加入 `-DPICCOLO_BUILD_TESTS=ON`。

完整场景测试需要显示设备和 Vulkan，会打开窗口，模拟跳跃和移动，并通过反射修改下一次起跳速度。它使用临时运行状态，不保存关卡，单独执行：

```sh
build/macos-debug/tests/PiccoloGameplaySmoke "$PWD/bin/Debug/PiccoloEditor.ini"
```

暗角 GPU 测试会读回实际像素，检查颜色、alpha、视口偏移与 framebuffer 重建。后处理组合测试使用临时配置，覆盖前向/延迟、FXAA 开关、暗角开关和窗口缩放，并检查 Vulkan 日志。两者同样需要显示设备和 Vulkan：

```sh
build/macos-debug/tests/PiccoloVignetteGpuTests
python3 scripts/test_post_process.py \
  build/macos-debug/tests/PiccoloPostProcessSmoke bin/Debug/PiccoloEditor.ini
```

调色 LUT 可用标准库脚本重新生成：

```sh
python3 scripts/generate_color_lut.py engine/asset/texture/lut/color_grading_warm.png
```

## Extra

### Vulkan Validation Layer: Validation Error
We have noticed some developers on Windows encounted PiccoloEditor.exe could run normally but reported an exception Vulkan Validation Layer: Validation Error
when debugging. You can solve this problem by installing Vulkan SDK (official newest version will do).

### Generate Compilation Database

You can build `compile_commands.json` with the following commands when `Unix Makefiles` generaters are avaliable. `compile_commands.json` is the file
required by `clangd` language server, which is a backend for cpp lsp-mode in Emacs.

For Windows:

``` powershell
cmake -DCMAKE_TRY_COMPILE_TARGET_TYPE="STATIC_LIBRARY" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B compile_db_temp -G "Unix Makefiles"
copy compile_db_temp\compile_commands.json .
```

### Using Physics Debug Renderer
Currently Physics Debug Renderer is only available on Windows. You can use the following command to generate the solution with the debugger project.

``` powershell
cmake -S . -B build -DENABLE_PHYSICS_DEBUG_RENDERER=ON
```

Note:
1. Please clean the build directory before regenerating the solution. We've encountered building problems in regenerating directly with previous CMakeCache.
2. Physics Debug Renderer will run when you start PiccoloEditor. We've synced the camera position between both scenes. But the initial camera mode in Physics Debug Renderer is wrong. Scrolling down the mouse wheel once will change the camera of Physics Debug Renderer to the correct mode.
