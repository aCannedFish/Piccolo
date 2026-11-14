# 完成日志

## 工作范围

参考 [GAMES104 课程作业](https://games104.boomingtech.com/sc/course-work/)，在当前 Piccolo 版本中补齐课程涉及的功能。课程文档使用的部分 Pilot 接口已经变化，下文记录当前版本的对应实现。每项任务验证后独立提交，并推送到 `ygt/games104-coursework` 分支。

| 任务 | 目标 | 状态 |
| --- | --- | --- |
| 构建基础 | 整理现有 macOS 适配，验证编辑器构建 | 已完成构建检查 |
| 调色 | 验证二维 LUT 插值，提供可复现的自定义 LUT | 已完成 |
| 动画融合 | 多片段采样、逐骨骼权重和旋转插值 | 已完成 |
| 动画切换 | 根据运动状态切换走、跑、跳动画 | 已完成 |
| 角色碰撞 | 形状扫描、平台落地、墙面滑动 | 已完成 |
| 参数编辑 | 通过反射编辑跳跃参数并在运行时生效 | 已完成 |

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

基础编辑器进程成功初始化渲染、加载默认世界及 29 个刚体。完整启动日志中存在 `VK_KHR_portability_subset` 未启用的 validation error，修复记录见下一节。当前 UI 工具无法绑定此独立可执行文件的窗口，画面和交互尚未人工验收。PA02 的额外后处理 Pass 属于选做项，本任务范围为调色与自定义 LUT。

## 03 设备扩展

日期：2026-11-14。调色提交 `22c9992` 已推送。

### 问题原因

启动日志报告 `VUID-VkDeviceCreateInfo-pProperties-04451`。根据 [Vulkan 设备创建规则](https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceCreateInfo.html)，设备提供 `VK_KHR_portability_subset` 时，创建逻辑设备必须启用该扩展。当前 macOS 的 MoltenVK 提供此扩展。

### 代码改动

修改 `VulkanRHI::createLogicalDevice()`：枚举选中物理设备的扩展，将其提供的 portability subset 加入本次设备创建列表。设备筛选仍使用原有必需扩展列表；其他平台的原生 Vulkan 设备可以继续使用。

### 阅读顺序

先看 `checkDeviceExtensionSupport()` 的设备筛选，再看 `createLogicalDevice()` 中的扩展枚举和 `VkDeviceCreateInfo`。扩展名称使用字符串，与仓库中的旧版 Vulkan 头文件兼容。

### 验证结果

Debug 构建通过。重新启动编辑器并等待默认世界加载完成，再运行两秒；进程保持运行，日志中没有 `Validation Error` 或 `[error]`。本次启动加载了新的暖色 LUT。检查完成后终止测试进程。

同时更正上一节最初遗漏的 validation error 记录。

## 04 动画融合

日期：2026-11-14。设备扩展修复 `fd86a13` 已推送。

### 问题原因

当前 `Skeleton::applyAnimation()` 固定遍历第一个片段，权重也固定为 1。短轨道会修改后续通道使用的帧索引，空轨道可能越界。资源管理器把 skeleton 路径传给了动画映射加载器，并在总权重为零时继续做除法。这些问题会影响 PA03 要求的融合效果。

### 代码改动

- `skeleton.cpp/.h`：遍历全部有效片段，独立采样每个轨道，累加逐骨骼平移、缩放和旋转；应用局部变换后按父子顺序更新骨骼。重建失败时清空旧骨骼指针和数量。
- `animation_system.cpp/.h`：使用正确的 skeleton 缓存，检查片段数组和骨骼遮罩，屏蔽禁用骨骼后归一化权重。总权重为零时保留零权重。
- `animation_component.cpp/.h`：按各自的时长推进所有片段。
- `skeleton_data.h`、`blend_state.h`：为根骨骼父索引和片段数量设置确定的默认值。
- 根 `CMakeLists.txt`、`tests/`：新增可选 CTest 目标，直接链接生产运行时，测试无须窗口或 GPU。

### 设计思路

现有动画资源存储相对 bind pose 的局部偏移，保留这一约定。先混合偏移，再对 bind pose 应用一次。平移和缩放按总权重归一化；旋转先统一四元数符号，再加权求和并归一化。等价的 `q` 和 `-q` 因此不会互相抵消。

轨道时间由片段的总帧数计算，每条轨道独立处理末帧。空平移、旋转、缩放轨道分别使用零位移、单位旋转、单位缩放。没有有效贡献的骨骼保持 bind pose。

随附资源包含 58 根骨骼和 213 项遮罩。遮罩检查要求覆盖所有骨骼，允许资源保留多余项。这个兼容规则来自实际资源测试。

### 阅读顺序

从 `AnimationComponent::tick()` 看时间推进，再看 `AnimationManager::getBlendStateWithClipData()` 的资源与权重处理，最后看 `Skeleton::applyAnimation()` 的采样、累加和层级更新。`tests/animation_tests.cpp` 中的两根骨骼夹具便于手算结果。

### 验证结果

```sh
cmake --preset macos-debug -DPICCOLO_BUILD_TESTS=ON
cmake --build build/macos-debug --target PiccoloAnimationTests PiccoloEditor --parallel 6
ctest --test-dir build/macos-debug --output-on-failure
```

Debug 构建通过，CTest 的动画目标通过 6 组回归用例：加权位置与父子传播、旋转及缩放、空轨道和不同轨道长度、零权重与无效映射、骨骼重建、真实资源权重归一化。测试使用显式异常检查，在 Release 下也会执行。

## 05 角色碰撞

日期：2026-11-14。动画融合提交 `447fb40` 已推送。

### 问题原因

原控制器只检查终点是否重叠，碰到墙壁后取消全部位移，下落也随之停止。Motor 用固定 `z = 0` 模拟地面，无法在平台上结束跳跃。PA03 的碰撞要求需要沿路径查询接触位置，并将接触状态传回 Motor。

### 代码改动

- `character_controller.cpp/.h`：使用胶囊体扫描，先处理竖直位移，再处理水平位移。碰墙后将剩余位移投影到接触面，最多继续扫描三次。新增地面与天花板状态，以及显式传入 PhysicsScene 的重载。
- `physics_scene.cpp/.h`：统一 sweep 的外向法线，清空未命中查询结果，为临时 Jolt Shape 使用引用计数所有权。形状尺寸已包含缩放，扫描矩阵使用单位缩放，避免重复缩放。
- `motor_component.cpp`：通过接触状态结束落地、触顶后转入下落、离开平台后开始下落。无物理控制器的对象保留原有地面平面，并修正重复计入竖直位移的问题。
- `tests/controller_tests.cpp`：用真实 Jolt 场景建立地面、墙、平台和天花板。

### 设计思路

接触查询只接受朝运动方向迎来的表面，切向接触不会阻止平移。角色与表面保留 0.005 米距离，减少数值误差导致的初始重叠。最终位置向下探测 0.05 米，用于确认支撑和离开平台。可站立表面的法线与竖直方向夹角不超过 45 度。

Motor 继续计算期望速度，控制器负责实际位移和接触。落地时清零竖直速度；触顶时开始下落。修改跳跃高度为零也不会关闭已有的下落过程。

### 阅读顺序

从 `MotorComponent::calculateTargetPosition()` 看接触反馈，再进入 `CharacterController::move()` 的竖直、水平和地面探测流程。底层查询见 `PhysicsScene::sweep()`。测试夹具中的尺寸可以直接对应预期落点。

### 验证结果

Debug 编辑器及两个测试目标构建成功。CTest 全部通过，新增 6 组控制器用例覆盖平台落地与再次起跳、20 米位移撞墙与沿墙滑动、贴墙下落、离开平台、触顶、扫描法线和缩放约定。测试同时检查未命中及零方向查询会清空旧结果。

测试初次运行在退出时触发异步日志销毁顺序问题。测试夹具现于进程退出前释放日志系统，重新运行正常退出。

## 06 动画切换

日期：2026-11-14。角色碰撞提交 `b782e66` 已推送。

### 代码改动

- `animation_fsm.cpp/.h`：增加 idle、walk_start、walk_run、walk_stop、jump_start、jump_loop、jump_end 状态，处理运动、跳跃、落地和片段结束信号。
- `AnimationStateRes`：为状态配置名称、BlendState 和可选速度采样点，沿用现有反射与序列化流程。
- `AnimationComponent`：从 Motor 读取当前运动状态，推进循环或单次动画；新增显式信号入口，供预览工具和测试复用。速度采样点之间只启用相邻两个片段。
- `MotorComponent::isJumping()`：提供起跳和下落状态。
- `player.object.json`：配置完整状态资源，并将 Motor 排在 Animation 前，使动画读取本帧碰撞结果。旧资源默认继续使用原有 BlendState 播放路径。
- `data/`：导入官方 PA03 分支的八个动画片段，来源见同目录上级的 `SOURCES.md`。

### 资源来源

通过官方仓库的 `games104/homework03-animation-physics` 分支定位资源，固定上游提交 `b46af1a2b998b5976063b7a6b091cdd1bb62ccca`。上游 skeleton JSON 与本仓库完全一致，片段复用当前骨骼及映射。资源保持上游原始内容。

### 设计思路

跳跃优先于地面运动切换；起跳片段结束后进入腾空循环，接触地面后进入落地片段。落地可以提前打断起跳，新的跳跃也可以打断落地。idle、walk_run、jump_loop 循环，其余状态在末帧等待下一次状态更新。

walk_run 使用速度比例 1、2 作为走和跑的采样点。权重决定混合时长，两个片段使用同一相位，避免各自循环造成脚步错位。状态机只判断状态，播放和资源访问由 AnimationComponent 完成。静止与移动起跳共用当前资源中的跳跃片段。

### 阅读顺序

先读 `animation_fsm.cpp` 的状态转换，再看 `AnimationComponent::updateAnimation()` 如何选择资源和推进时间。随后看 `player.object.json` 的 `locomotion_states`。实际骨骼混合继续进入上一任务的 Skeleton 代码。

### 验证结果

Debug 编辑器及测试构建通过。动画测试增加到 9 组，控制器 6 组继续通过。新增测试覆盖跳跃优先级、提前落地、重新起跳、停止后恢复移动，以及真实玩家资源的 idle → walk → jump → land → idle 播放。还验证了速度中点的 0.5/0.5 权重、共享相位和旧资源循环兼容性。

## 07 跳跃参数

日期：2026-11-14。动画切换提交 `d19e5b6` 已推送。

### 代码改动

- `MotorComponentRes`：新增 `m_use_jump_initial_speed` 和 `m_jump_initial_speed`，分别控制参数模式和竖直起跳速度，单位为米每秒。类已有 `Fields` 标签，新字段自动参与反射和序列化。
- `MotorComponent`：新增 `calculateJumpInitialSpeed()`。起跳时读取当前参数，速度模式直接使用初速度，高度模式使用 `sqrt(2gh)`。负参数按零处理。
- `player.object.json`：默认启用 4.5 米每秒的初速度；移除每帧覆盖跳跃高度并输出日志的演示 Lua 组件。
- `tests/motor_tests.cpp`：检查两种参数模式、生成的反射访问器、保存与重新读取、旧资源默认行为。

### 设计思路

对应 PA04 的参数反射和运行时生效要求。已有资源缺少模式字段时默认使用高度模式，原来的跳跃高度保持有效。默认玩家使用速度模式，参数在每次起跳时求值，空中修改参数不会直接重置当前速度。

编辑器已为 `bool` 注册 Checkbox、为 `float` 注册 InputFloat。`createClassUI()` 递归展开 MotorComponent 的 `m_motor_res`，即可显示新字段。无需为该组件增加专用面板。

### 阅读顺序

先读 `resource/res_type/components/motor.h` 的字段和默认值，再看 `MotorComponent::calculateJumpInitialSpeed()` 及 `calculatedDesiredVerticalMoveSpeed()` 的调用。跟踪 UI 时，从 `EditorUI::createClassUI()` 进入 `createLeafNodeUI()`，查找 `m_editor_ui_creator` 的 bool 与 float 控件。

使用时选中 Player，展开 Components Details → MotorComponent → m_motor_res。勾选 `m_use_jump_initial_speed` 后编辑 `m_jump_initial_speed`，进入游戏模式按空格测试；取消勾选后使用 `m_jump_height`。

### 验证结果

Debug 编辑器、动画、控制器和 Motor 测试构建通过，CTest 三个目标全部通过。新增 3 组 Motor 用例覆盖高度公式、速度直接设置、负值、反射修改后计算结果、JSON 保存与重载、旧资源保持高度模式。

反射用例使用与默认玩家相同的 PhysicsControllerConfig，检查完整组件资源的序列化。面板控件类型已经通过生成元数据和现有 UI 注册代码核对，交互画面仍待人工查看。

## 08 空指针

日期：2026-11-14。跳跃参数提交 `b479ac7` 已推送。

### 问题原因

参数保存测试发现，没有 ControllerConfig 的 Motor 资源会将空指针写成空类型名的对象，读取时触发类型名断言。无控制器本来就是 Motor 支持的模式，需要保存这种资源配置。

### 代码改动

`Serializer::writePointer()` 和反射指针的 `write()` 将空指针写为 JSON null；`readPointer()` 在读取 null 时保留空指针。非空指针继续使用原有的类型名称与内容表示。

### 阅读顺序

先看 `core/meta/serializer/serializer.h` 的指针读写入口，再看生成的 MotorComponentRes 序列化代码如何调用这些入口。`testEmptyControllerRoundTrip()` 用无控制器资源完成保存与读取，同时检查普通可选指针。

### 验证结果

Debug 编辑器与三个 CTest 目标通过。Motor 测试增至 4 组，空控制器保存为 null，重载后保留跳跃参数且不触发断言。
