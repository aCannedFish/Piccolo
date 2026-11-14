# 完成日志

## 工作范围

参考 [GAMES104 课程作业](https://games104.boomingtech.com/sc/course-work/)，在当前 Piccolo 版本中补齐课程涉及的功能。课程文档使用的部分 Pilot 接口已经变化，下文记录当前版本的对应实现。每项任务验证后独立提交，并推送到 `ygt/games104-coursework` 分支。

| 任务 | 目标 | 状态 |
| --- | --- | --- |
| 构建基础 | 整理现有 macOS 适配，验证编辑器构建 | 已完成构建检查 |
| 调色 | 验证二维 LUT 插值，提供可复现的自定义 LUT | 已完成 |
| 动画融合 | 多片段采样、逐骨骼权重和旋转插值 | 待实现 |
| 动画切换 | 根据运动状态切换走、跑、跳动画 | 待实现 |
| 角色碰撞 | 形状扫描、平台落地、墙面滑动 | 待实现 |
| 参数编辑 | 通过反射编辑跳跃参数并在运行时生效 | 待实现 |

## 01 构建基础

日期：2026-11-14。

### 起点

仓库起点为 `f505370`。开始任务时已有构建、编辑器配置、架构文档及 Color Grading 的未提交改动。用户已确认将这些改动检查后作为独立基础提交推送。本节列出的实现来自该工作区，本次检查确认其用途和构建结果。`imgui.ini` 是本机窗口布局，不纳入提交。

### 代码改动

- `CMakePresets.json`、`build_macos.sh`：使用 Ninja 分开构建 Debug 和 Release，生成编译数据库。
- 根目录及编辑器 `CMakeLists.txt`：按配置输出到 `bin/Debug`、`bin/Release`，生成匹配的资源路径，修正构建后复制命令。
- `engine/3rdparty/CMakeLists.txt`：兼容 CMake 4，为新 Clang 检测并设置 Jolt 的警告选项。
- `sol/optional_implementation.hpp`：引用类型的 `emplace` 重新绑定引用，并返回绑定的对象，修复新版编译器报错。
- `meta_parser`、`precompile`：使用当前 Xcode 的 libclang 和 SDK，按 C++17 解析，清理头文件列表中的空白，修正列表分隔符。
- `.vscode/`、`.clang-format`、`.gitignore`、`README.md`：保留共享调试配置、修正格式配置中的重复键，并说明构建入口。
- `ARCHITECTURE.md`：保留已有模块说明和代码索引。
- `color_grading.frag`：现有实现将蓝色通道映射到相邻 LUT 切片，分别进行双线性采样，再沿蓝色通道插值。

### 设计思路

构建配置、反射生成工具和资源目录需要一致。Parser 使用当前 SDK 解析运行时头文件，再生成序列化与反射代码；编辑器链接运行时后复制资源。Debug 与 Release 使用独立目录，便于调试和验证。

调色贴图采用横向排列的蓝色切片，尺寸为 `N² × N`。红、绿通道定位切片中的像素，半像素偏移让坐标落在 texel 中心。后续任务会为这段已有实现补充说明和测试。

### 阅读顺序

先读 `README.md` 的 macOS 构建说明，再读 `CMakePresets.json` 和 `build_macos.sh`。追踪代码生成时，从 `engine/source/precompile/precompile.cmake` 进入 `MetaParser::parseProject()`。运行时模块入口见 `ARCHITECTURE.md` 的快速定位表。

### 验证结果

- `cmake --build --preset macos-debug --parallel 6` 成功，完成 shader 编译、39 组反射元数据生成和编辑器链接。
- `bash -n build_macos.sh` 通过。
- CMake presets 和四份 VS Code 配置通过 JSON 解析。
- 架构文档的本地文件链接检查通过。

本节完成构建验证。编辑器运行检查及功能测试记录在后续任务中。

## 02 调色

日期：2026-11-14。基础提交 `e83563a` 已推送。

### 代码改动

- `engine/shader/glsl/color_grading.frag`：保留已有的三线性插值算法，使用完整变量名，补充色彩空间、切片布局、texel 中心和插值过程的注释，并统一换行。
- `scripts/generate_color_lut.py`：新增只依赖 Python 标准库的 LUT 生成器，支持 identity 和 warm 两种映射，输出带校验和的 RGB PNG。
- `engine/asset/texture/lut/color_grading_warm.png`：新增 256 × 16 的暖色 LUT；`rendering.global.json` 默认加载该资源。
- `tests/test_color_lut.py`：验证恒等映射、切片交界、输入钳制、暖色映射量化误差和资源可重复生成。

### 设计思路

对应 PA02 的 Color Grading 与自定义 LUT 要求。Tone Mapping 的输出已经是 sRGB，LUT 直接存储调整后的 sRGB 值。暖色映射略微增加红绿通道的交叉贡献，并降低蓝通道。生成公式的每行系数和不超过 1，输出保持在有效范围内。

红绿通道使用纹理的线性过滤，蓝通道使用两个切片之间的插值。红色到达 1 时仍采样本切片的最后一个 texel 中心，避免相邻切片串色。alpha 从输入保留。

### 阅读顺序

从 `rendering.global.json` 的 `color_grading_map` 开始，沿 `RenderSystem::initialize()`、`RenderResource::uploadGlobalRenderResource()`、`ColorGradingPass::setupDescriptorSet()` 找到纹理绑定，再读片元 shader。LUT 的布局和调色公式见生成脚本。

重新生成默认资源：

```sh
python3 scripts/generate_color_lut.py engine/asset/texture/lut/color_grading_warm.png
python3 -m unittest discover -s tests -p 'test_color_lut.py' -v
cmake --build --preset macos-debug --parallel 6
```

### 验证结果

4 项数值与资源测试通过，200 个固定随机样本的误差均在半个 8-bit 量化级以内。Debug shader 编译和编辑器链接成功。CPU 测试检查采样约定与资源数据，shader 另由 glslang 编译验证。

基础编辑器进程成功初始化渲染、加载默认世界及 29 个刚体，日志未出现 validation error。当前 UI 工具无法绑定此独立可执行文件的窗口，画面和交互尚未人工验收。PA02 的额外后处理 Pass 属于选做项，本任务范围为调色与自定义 LUT。
