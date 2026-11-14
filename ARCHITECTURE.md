# 引擎架构

本文按当前仓库的实现整理 Piccolo 的目录、模块职责和代码入口。查找具体功能时，先看“快速定位”；追踪功能如何运行时，再看对应模块的调用流程。文中路径均相对仓库根目录，源码链接可直接打开对应文件。

## 快速定位

| 要找的功能 | 代码入口 | 先读的类或函数 |
| --- | --- | --- |
| 程序入口、启动配置 | [main.cpp](engine/source/editor/source/main.cpp) | `main()` |
| 每帧更新、逻辑与渲染顺序 | [engine.cpp](engine/source/runtime/engine.cpp) | `PiccoloEngine::tickOneFrame()` |
| 全局系统的创建与销毁 | [global_context.cpp](engine/source/runtime/function/global/global_context.cpp) | `RuntimeGlobalContext::startSystems()`、`shutdownSystems()` |
| 世界加载、关卡切换与保存 | [world_manager.cpp](engine/source/runtime/function/framework/world/world_manager.cpp) | `WorldManager::loadWorld()`、`loadLevel()`、`saveCurrentLevel()` |
| 对象创建、删除、关卡更新 | [level.cpp](engine/source/runtime/function/framework/level/level.cpp) | `Level::createObject()`、`deleteGObjectByID()`、`tick()` |
| 组件装载与更新 | [object.cpp](engine/source/runtime/function/framework/object/object.cpp) | `GObject::load()`、`tick()`、`shouldComponentTick()` |
| 位置、旋转、缩放 | [transform_component.cpp](engine/source/runtime/function/framework/component/transform/transform_component.cpp) | `TransformComponent::setPosition()`、`setRotation()`、`setScale()` |
| 游戏模式按键、鼠标输入 | [input_system.cpp](engine/source/runtime/function/input/input_system.cpp) | `InputSystem::onKey()`、`onCursorPos()` |
| 编辑器快捷键、视口相机操作 | [editor_input_manager.cpp](engine/source/editor/source/editor_input_manager.cpp) | `EditorInputManager::registerInput()`、`processEditorCommand()` |
| 角色移动、加速、跳跃 | [motor_component.cpp](engine/source/runtime/function/framework/component/motor/motor_component.cpp) | `MotorComponent::tickPlayerMotor()` |
| 第一、第三人称与自由相机 | [camera_component.cpp](engine/source/runtime/function/framework/component/camera/camera_component.cpp) | `CameraComponent::tick()` |
| 骨骼动画播放与混合 | [animation_component.cpp](engine/source/runtime/function/framework/component/animation/animation_component.cpp)、[skeleton.cpp](engine/source/runtime/function/animation/skeleton.cpp) | `AnimationComponent::tick()`、`Skeleton::applyAnimation()` |
| 刚体、射线、碰撞查询 | [physics_scene.cpp](engine/source/runtime/function/physics/physics_scene.cpp) | `PhysicsScene::createRigidBody()`、`raycast()`、`sweep()`、`isOverlap()` |
| Lua 脚本与组件字段访问 | [lua_component.cpp](engine/source/runtime/function/framework/component/lua/lua_component.cpp) | `LuaComponent::postLoadResource()`、`tick()`、`invoke()` |
| 粒子发射与 GPU 模拟 | [particle_manager.cpp](engine/source/runtime/function/particle/particle_manager.cpp)、[particle_pass.cpp](engine/source/runtime/function/render/passes/particle_pass.cpp) | `ParticleManager::createParticleEmitter()`、`ParticlePass::simulate()` |
| 渲染系统与每帧准备 | [render_system.cpp](engine/source/runtime/function/render/render_system.cpp) | `RenderSystem::initialize()`、`tick()`、`processSwapData()` |
| 前向、延迟渲染与 Pass 顺序 | [render_pipeline.cpp](engine/source/runtime/function/render/render_pipeline.cpp)、[main_camera_pass.cpp](engine/source/runtime/function/render/passes/main_camera_pass.cpp) | `RenderPipeline::forwardRender()`、`deferredRender()`；`MainCameraPass::draw()`、`drawForward()` |
| BRDF、光照、IBL | [mesh_lighting.h](engine/shader/include/mesh_lighting.h)、[mesh_lighting.inl](engine/shader/include/mesh_lighting.inl) | `BRDF()` 及直接光、环境光计算 |
| 阴影、后处理、物体拾取 | [渲染 Pass](#渲染-pass) | 按效果查找 Pass 与 shader 对照表 |
| 模型、纹理加载与 GPU 上传 | [render_resource_base.cpp](engine/source/runtime/function/render/render_resource_base.cpp)、[render_resource.cpp](engine/source/runtime/function/render/render_resource.cpp) | `loadMeshData()`、`loadTexture()`、`uploadGameObjectRenderResource()` |
| Vulkan 设备、交换链、同步、资源创建 | [vulkan_rhi.cpp](engine/source/runtime/function/render/interface/vulkan/vulkan_rhi.cpp) | `VulkanRHI::initialize()`、`prepareBeforePass()`、`submitRendering()` |
| 编辑器菜单、对象列表、属性面板 | [editor_ui.cpp](engine/source/editor/source/editor_ui.cpp) | `EditorUI::showEditorUI()`、`showEditorDetailWindow()` |
| 选中对象、移动旋转缩放工具 | [editor_scene_manager.cpp](engine/source/editor/source/editor_scene_manager.cpp) | `EditorSceneManager::onGObjectSelected()`、`moveEntity()` |
| JSON 资源读写与路径解析 | [asset_manager.h](engine/source/runtime/resource/asset_manager/asset_manager.h)、[config_manager.cpp](engine/source/runtime/resource/config_manager/config_manager.cpp) | `AssetManager::loadAsset()`、`saveAsset()`；`ConfigManager::initialize()` |
| 反射、序列化与代码生成 | [反射生成](#反射生成) | 运行时反射接口、Parser、Mustache 模板 |
| 构建目标、shader 编译、IDE 配置 | [构建调试](#构建调试) | CMake、构建脚本、`.vscode/` |

## 目录总览

```text
Piccolo/
├── engine/
│   ├── source/
│   │   ├── runtime/             引擎运行时
│   │   │   ├── engine.*        启停、主循环、帧率统计
│   │   │   ├── core/           数学、日志、反射等基础代码
│   │   │   ├── function/       世界、组件、渲染、物理等运行逻辑
│   │   │   ├── resource/       配置、资源读写、资源数据结构
│   │   │   └── platform/       文件枚举与路径工具
│   │   ├── editor/             编辑器入口、界面与交互
│   │   │   ├── include/        编辑器头文件
│   │   │   ├── source/         编辑器实现
│   │   │   └── resource/       字体、图标等编辑器资源
│   │   ├── meta_parser/        基于 Clang 的反射代码生成工具
│   │   ├── precompile/         反射生成的 CMake 配置与输入列表
│   │   ├── _generated/         自动生成的反射和序列化代码
│   │   └── test/               上游保留目录，当前 CMake 未启用
│   ├── shader/
│   │   ├── glsl/               顶点、片元、几何和计算 shader
│   │   ├── include/            shader 共用结构、常量和光照计算
│   │   └── generated/          自动生成的 SPIR-V 和 C++ 头文件
│   ├── asset/                  世界、关卡、对象、模型、材质和纹理
│   ├── configs/                开发与部署使用的启动配置
│   ├── template/               反射和序列化的 Mustache 模板
│   ├── 3rdparty/               Vulkan、GLFW、ImGui、Jolt 等依赖
│   └── jolt-asset/             Jolt 物理调试渲染资源
├── cmake/                      shader 编译和资源嵌入脚本
├── scripts/                    辅助脚本
├── .vscode/                    编辑器设置、构建任务和调试配置
├── .github/、.gitlab/           CI 配置
├── CMakeLists.txt              根构建入口
├── CMakePresets.json           当前 macOS Debug、Release 预设
├── build_macos.sh              macOS 配置与构建入口
├── build_linux.sh              Linux 构建入口
├── build_windows.bat           Windows 构建入口
├── build/                      CMake、Ninja 等构建产物
├── bin/                        可执行程序与运行资源
└── compile_commands.json       clangd 使用的 C++ 编译数据库
```

日常修改最常涉及 `engine/source/runtime/function/`、`engine/source/editor/`、`engine/shader/` 和 `engine/asset/`。`core/` 提供基础类型，`resource/` 定义资源数据与读写方式，`platform/` 提供文件系统工具。窗口和 GLFW 输入回调封装位于 `function/render/window_system.*`。

## 启动流程

当前可执行程序是 `PiccoloEditor`。它创建运行时，再创建编辑器，由编辑器驱动每帧更新。

```text
editor/source/main.cpp :: main()
  ├── PiccoloEngine::startEngine(PiccoloEditor.ini)
  │     ├── TypeMetaRegister::metaRegister()
  │     └── RuntimeGlobalContext::startSystems()
  ├── PiccoloEngine::initialize()
  ├── PiccoloEditor::initialize(engine)
  │     ├── EditorGlobalContext::initialize()
  │     └── EditorUI::initialize()
  └── PiccoloEditor::run()
        ├── EditorSceneManager::tick()
        ├── EditorInputManager::tick()
        └── PiccoloEngine::tickOneFrame()
              ├── logicalTick()
              │     ├── WorldManager::tick()
              │     └── InputSystem::tick()
              ├── RenderSystem::swapLogicRenderData()
              ├── rendererTick() → RenderSystem::tick()
              └── WindowSystem::pollEvents()
```

[RuntimeGlobalContext](engine/source/runtime/function/global/global_context.h) 持有配置、文件系统、日志、资源、物理、世界、窗口、输入、粒子、渲染和调试绘制系统。查找某个系统在哪里创建、依赖哪些系统，先读它的 `startSystems()`；查找清理顺序，读 `shutdownSystems()`。

主循环按单线程顺序执行逻辑和渲染。逻辑侧通过 `RenderSwapContext` 提交对象、相机和粒子数据，渲染侧在交换后处理这些数据。

## 运行模块

### 对象框架

代码位于 [function/framework/](engine/source/runtime/function/framework/)。世界、关卡、对象、组件的组织关系如下：

```text
WorldManager
  └── Level
        ├── GObject
        │     └── Component
        ├── Character
        └── PhysicsScene
```

| 模块 | 位置 | 职责与入口 |
| --- | --- | --- |
| 世界 | [world/](engine/source/runtime/function/framework/world/) | `WorldManager` 加载世界和默认关卡，维护活动关卡，提供重载与保存入口 |
| 关卡 | [level/](engine/source/runtime/function/framework/level/) | `Level` 持有对象集合、活动角色和物理场景，负责对象创建、删除、加载、保存和每帧更新 |
| 关卡调试 | [level_debugger.cpp](engine/source/runtime/function/framework/level/level_debugger.cpp) | `LevelDebugger` 绘制关卡对象的调试信息 |
| 对象 | [object/](engine/source/runtime/function/framework/object/) | `GObject` 持有名称、ID、对象定义路径和组件列表；`load()` 装载组件，`tick()` 分发更新 |
| 组件基类 | [component.h](engine/source/runtime/function/framework/component/component.h) | `Component` 定义资源装载后的 `postLoadResource()`、每帧 `tick()` 和脏标记 |

`GObject::load()` 先装载关卡实例中的组件，再从对象定义中补齐其他组件。同类型组件以实例中的配置为准。组件装载后通过 `postLoadResource()` 建立与所属对象及其他系统的联系。

编辑模式下，`GObject::tick()` 按 `g_editor_tick_component_types` 筛选要更新的组件。默认注册位于 [editor.cpp](engine/source/editor/source/editor.cpp)，包含 `TransformComponent` 和 `MeshComponent`。查找组件在编辑模式下的更新行为时，从这里和 `shouldComponentTick()` 开始。

### 组件实现

组件的行为代码集中在 `function/framework/component/`。资源字段主要定义在 `resource/res_type/components/`，部分组件也直接在自身头文件中声明可序列化字段。

| 组件 | 实现位置 | 主要功能 |
| --- | --- | --- |
| `TransformComponent` | [transform/](engine/source/runtime/function/framework/component/transform/) | 管理位置、旋转、缩放和双缓冲 Transform；标记变更并同步刚体变换 |
| `MeshComponent` | [mesh/](engine/source/runtime/function/framework/component/mesh/) | 装载子网格与材质引用，将模型矩阵、骨骼矩阵等变更写入 `RenderSwapContext` |
| `AnimationComponent` | [animation/](engine/source/runtime/function/framework/component/animation/) | 装载骨架，推进动画时间，计算混合状态并输出骨骼变换 |
| `MotorComponent` | [motor/](engine/source/runtime/function/framework/component/motor/) | 将游戏输入转换为角色移动，计算速度、跳跃、位移和目标位置 |
| `CameraComponent` | [camera/](engine/source/runtime/function/framework/component/camera/) | 更新第一人称、第三人称或自由相机，将视图矩阵提交给渲染系统 |
| `RigidBodyComponent` | [rigidbody/](engine/source/runtime/function/framework/component/rigidbody/) | 创建、移除刚体，同步对象变换；缩放变化时重建碰撞形状 |
| `ParticleComponent` | [particle/](engine/source/runtime/function/framework/component/particle/) | 创建发射器，提交发射器更新和变换 |
| `LuaComponent` | [lua/](engine/source/runtime/function/framework/component/lua/) | 创建 Lua 环境，执行脚本，通过反射读取、设置组件字段或调用方法 |

### 其他系统

| 模块 | 位置 | 主要功能与阅读入口 |
| --- | --- | --- |
| 输入 | [function/input/](engine/source/runtime/function/input/) | `InputSystem` 将按键映射为 `GameCommand`，计算鼠标方向变化；`initialize()` 注册窗口回调 |
| 角色 | [function/character/](engine/source/runtime/function/character/) | `Character` 关联活动角色对象，维护角色位置、朝向和自由相机切换 |
| 角色控制器 | [function/controller/](engine/source/runtime/function/controller/) | `CharacterController::move()` 使用物理查询处理位移与碰撞 |
| 动画 | [function/animation/](engine/source/runtime/function/animation/) | `AnimationManager` 缓存动画资源；`AnimationLoader` 读取资源；`Skeleton`、`Node` 组织骨架并计算动画结果 |
| 物理管理 | [physics_manager.cpp](engine/source/runtime/function/physics/physics_manager.cpp) | `PhysicsManager` 初始化物理支持，创建和销毁各关卡的 `PhysicsScene` |
| 物理场景 | [physics_scene.cpp](engine/source/runtime/function/physics/physics_scene.cpp) | `PhysicsScene` 封装 Jolt 物理世界，执行模拟、刚体创建、射线、形状扫掠和重叠查询 |
| Jolt 适配 | [physics/jolt/](engine/source/runtime/function/physics/jolt/) | 引擎与 Jolt 类型转换、碰撞层和碰撞形状适配 |
| 粒子管理 | [function/particle/](engine/source/runtime/function/particle/) | `ParticleManager` 读取全局粒子配置，组织发射器描述和创建请求；GPU 模拟位于渲染 Pass 与计算 shader |
| UI 接口 | [function/ui/](engine/source/runtime/function/ui/) | `WindowUI` 定义 `initialize()` 和 `preRender()`，由编辑器实现界面内容 |

追踪角色移动时，可以按 `InputSystem → MotorComponent → CharacterController → PhysicsScene → TransformComponent` 阅读。控制器分开扫描竖直和水平位移，向 Motor 返回落地与触顶状态。追踪骨骼动画时，可以按 `AnimationComponent → AnimationFSM → AnimationManager → Skeleton → MeshComponent → mesh.vert` 阅读。状态资源决定播放片段，Skeleton 按逐骨骼权重混合局部变换。

根目录 [tests/](tests/) 包含动画、控制器、反射回归测试，以及需显示设备的完整场景测试。构建和运行命令见 [README.md](README.md#运行测试)，各项实现说明见 [LOG.md](LOG.md)。

## 渲染模块

### 核心职责

CPU 侧代码位于 [function/render/](engine/source/runtime/function/render/)，GPU 侧代码位于 [engine/shader/](engine/shader/)。

| 模块 | 位置 | 主要功能 |
| --- | --- | --- |
| 系统入口 | [render_system.cpp](engine/source/runtime/function/render/render_system.cpp) | 创建 RHI、场景、相机、资源和管线；每帧处理交换数据、可见性与渲染分支 |
| 管线组织 | [render_pipeline.cpp](engine/source/runtime/function/render/render_pipeline.cpp) | 创建和初始化 Pass，连接输入资源，组织前向、延迟渲染及交换链重建后的更新 |
| 主相机 | [passes/main_camera_pass.cpp](engine/source/runtime/function/render/passes/main_camera_pass.cpp) | 配置主 Render Pass 的 attachment、subpass、依赖和 framebuffer，执行场景绘制及后处理 |
| Pass 公共结构 | [render_pass_base.h](engine/source/runtime/function/render/render_pass_base.h)、[render_pass.h](engine/source/runtime/function/render/render_pass.h) | 公共初始化接口、framebuffer、pipeline、descriptor 信息，以及主相机 attachment 和 subpass 枚举 |
| 场景与可见性 | [render_scene.cpp](engine/source/runtime/function/render/render_scene.cpp) | 维护渲染实体、灯光和对象 ID 映射，更新相机与灯光可见对象列表 |
| 相机 | [render_camera.cpp](engine/source/runtime/function/render/render_camera.cpp) | 计算视图、投影和相机方向，处理移动、旋转、缩放与视口宽高比 |
| CPU 资源读取 | [render_resource_base.cpp](engine/source/runtime/function/render/render_resource_base.cpp) | 读取普通纹理、HDR 纹理、OBJ、JSON 网格和材质数据 |
| GPU 资源管理 | [render_resource.cpp](engine/source/runtime/function/render/render_resource.cpp) | 上传网格、材质、IBL 和 LUT，管理 GPU buffer、image、sampler 及每帧数据 |
| 逻辑数据交换 | [render_swap_context.h](engine/source/runtime/function/render/render_swap_context.h) | 定义逻辑和渲染之间交换的对象、相机、粒子数据与双缓冲 |
| 渲染数据类型 | [render_common.h](engine/source/runtime/function/render/render_common.h)、[render_type.h](engine/source/runtime/function/render/render_type.h)、[render_object.h](engine/source/runtime/function/render/render_object.h)、[render_entity.h](engine/source/runtime/function/render/render_entity.h) | 每帧与每次绘制的数据布局、网格材质描述和渲染实体 |
| 几何辅助 | [render_helper.cpp](engine/source/runtime/function/render/render_helper.cpp) | 视锥构建、包围盒相交等可见性计算 |
| 图形接口 | [interface/rhi.h](engine/source/runtime/function/render/interface/rhi.h)、[interface/rhi_struct.h](engine/source/runtime/function/render/interface/rhi_struct.h) | RHI 接口及资源、管线、同步等参数类型 |
| Vulkan 实现 | [interface/vulkan/](engine/source/runtime/function/render/interface/vulkan/) | `VulkanRHI` 实现设备、交换链、命令、descriptor、同步和资源创建；`VulkanUtil` 提供资源操作工具 |
| 窗口 | [window_system.cpp](engine/source/runtime/function/render/window_system.cpp) | GLFW 窗口、事件轮询、输入回调和窗口状态 |
| 调试绘制 | [debugdraw/](engine/source/runtime/function/render/debugdraw/) | 调试点、线、三角形、包围盒和文字的分组、缓冲与绘制 |

### 数据流程

以网格变换更新为例，逻辑数据进入 GPU 的过程如下：

```text
TransformComponent / AnimationComponent
  → MeshComponent::tick()
  → RenderSwapContext::getLogicSwapData().addDirtyGameObject()
  → PiccoloEngine::tickOneFrame() 交换数据
  → RenderSystem::processSwapData()
      ├── RenderResourceBase 读取需要的网格、材质、纹理
      ├── RenderResource 上传或复用 GPU 资源
      └── RenderScene 新增或更新 RenderEntity
  → RenderScene::updateVisibleObjects()
  → RenderPipeline
  → MainCameraPass / 各个 Pass
  → VulkanRHI 提交命令并呈现
```

对象存在于关卡中却没有出现在画面上时，沿这条流程依次查看组件是否提交数据、渲染实体是否建立、可见性列表是否包含该对象、对应 Pass 是否绘制。

### 渲染 Pass

当前默认使用延迟管线，设置位于 [render_system.h](engine/source/runtime/function/render/render_system.h)。`RenderPipeline::deferredRender()`、`forwardRender()` 先绘制方向光和点光源阴影，再调用主相机 Pass。主相机内部通过 subpass 顺序执行场景光照、后处理和 UI 合成。

主相机中的 Pass 与 Vulkan subpass 的对应关系由 `MainCameraPass::setupRenderPass()`、各 Pass 的 `setupPipelines()` 和主相机的绘制函数共同确定。查找先后顺序时，读 `draw()`、`drawForward()`；查找输入输出图像时，读 `setupRenderPass()` 和 `RenderPipeline::initialize()`。

`MainCameraPass::draw()` 的执行顺序是 G-buffer → 延迟光照 → 前向阶段的粒子绘制 → Tone Mapping → Color Grading → FXAA（启用时）→ Vignette → UI → 合成。前向路径在场景绘制阶段调用 `drawMeshLighting()`、`drawSkybox()` 和粒子绘制，后续执行相同的后处理与 UI 流程。

下表中的 C++ 文件均位于 `engine/source/runtime/function/render/passes/`，shader 均位于 `engine/shader/glsl/`。

| 功能 | C++ 实现 | shader |
| --- | --- | --- |
| 主相机、G-buffer、场景光照 | [main_camera_pass.cpp](engine/source/runtime/function/render/passes/main_camera_pass.cpp) | [mesh.vert](engine/shader/glsl/mesh.vert)、[mesh_gbuffer.frag](engine/shader/glsl/mesh_gbuffer.frag)、[deferred_lighting.frag](engine/shader/glsl/deferred_lighting.frag)、[mesh.frag](engine/shader/glsl/mesh.frag) |
| 方向光阴影 | [directional_light_pass.cpp](engine/source/runtime/function/render/passes/directional_light_pass.cpp) | `mesh_directional_light_shadow.vert / .frag` |
| 点光源阴影 | [point_light_pass.cpp](engine/source/runtime/function/render/passes/point_light_pass.cpp) | `mesh_point_light_shadow.vert / .geom / .frag` |
| Tone Mapping | [tone_mapping_pass.cpp](engine/source/runtime/function/render/passes/tone_mapping_pass.cpp) | [tone_mapping.frag](engine/shader/glsl/tone_mapping.frag) |
| Color Grading | [color_grading_pass.cpp](engine/source/runtime/function/render/passes/color_grading_pass.cpp) | [color_grading.frag](engine/shader/glsl/color_grading.frag) |
| FXAA | [fxaa_pass.cpp](engine/source/runtime/function/render/passes/fxaa_pass.cpp) | `fxaa.vert / .frag` |
| 暗角 | [vignette_pass.cpp](engine/source/runtime/function/render/passes/vignette_pass.cpp) | [vignette.frag](engine/shader/glsl/vignette.frag) |
| ImGui 绘制 | [ui_pass.cpp](engine/source/runtime/function/render/passes/ui_pass.cpp) | 使用 ImGui Vulkan backend |
| 场景与 UI 合成 | [combine_ui_pass.cpp](engine/source/runtime/function/render/passes/combine_ui_pass.cpp) | [combine_ui.frag](engine/shader/glsl/combine_ui.frag) |
| 物体拾取 | [pick_pass.cpp](engine/source/runtime/function/render/passes/pick_pass.cpp) | `mesh_inefficient_pick.vert / .frag` |
| 粒子模拟和绘制 | [particle_pass.cpp](engine/source/runtime/function/render/passes/particle_pass.cpp) | `particle_kickoff.comp`、`particle_emit.comp`、`particle_simulate.comp`、`particlebillboard.vert / .frag` |
| 天空盒、编辑器操作轴 | `main_camera_pass.cpp` 中的 `drawSkybox()`、`drawAxis()` | `skybox.vert / .frag`、`axis.vert / .frag` |

Tone Mapping、Color Grading 等后处理使用 [post_process.vert](engine/shader/glsl/post_process.vert) 绘制全屏三角形。`color_grading.frag` 在横向排列的两个蓝色切片之间插值，红绿通道使用线性过滤，alpha 保留输入值。LUT 只创建一个 mip 层级，并显式采样第 0 层，避免切片间串色。默认暖色 LUT 由 [生成脚本](scripts/generate_color_lut.py) 生成。

VignettePass 读取 `backup_buffer_odd`，将暗化后的场景写入已有的 `post_process_buffer_even`。UI subpass 保留这个附件，CombineUIPass 将它与 UI 附件合成。视口和效果参数通过 32 字节 push constants 传入 shader，运行时修改参数不会重建管线。配置与代码入口见 [VignetteSettings](engine/source/runtime/resource/res_type/data/vignette_settings.h) 和 `RenderSystem::setVignetteSettings()`。

### 共用代码

| 文件 | 内容 |
| --- | --- |
| [constants.h](engine/shader/include/constants.h) | shader 共用常量 |
| [structures.h](engine/shader/include/structures.h) | shader 共用数据结构 |
| [gbuffer.h](engine/shader/include/gbuffer.h) | G-buffer 数据及编码、解码逻辑 |
| [mesh_lighting.h](engine/shader/include/mesh_lighting.h) | GGX、Fresnel、几何项和 BRDF 计算 |
| [mesh_lighting.inl](engine/shader/include/mesh_lighting.inl) | 点光源、方向光、阴影和 IBL 的光照计算，供前向与延迟光照 shader 引入 |

修改材质光照公式时，从 `mesh_lighting.h`、`mesh_lighting.inl` 开始；修改 G-buffer 布局时，同时查看 `mesh_gbuffer.frag`、`gbuffer.h`、`deferred_lighting.frag` 和主相机的 attachment 配置。

## 编辑器

编辑器代码位于 [engine/source/editor/](engine/source/editor/)。头文件在 `include/`，实现文件在 `source/`。

| 功能 | 文件 | 入口与职责 |
| --- | --- | --- |
| 编辑器生命周期 | [editor.cpp](engine/source/editor/source/editor.cpp) | `PiccoloEditor::initialize()` 创建界面与编辑器上下文；`run()` 驱动主循环 |
| 编辑器全局对象 | [editor_global_context.cpp](engine/source/editor/source/editor_global_context.cpp) | `EditorGlobalContext` 连接窗口、渲染系统、引擎、场景管理器与输入管理器 |
| 界面总入口 | [editor_ui.cpp](engine/source/editor/source/editor_ui.cpp) | `preRender()` 调用 `showEditorUI()`，组织各面板 |
| 菜单、保存和重载 | 同上 | `showEditorMenu()`，调用 `WorldManager` 的保存、重载入口 |
| 对象列表 | 同上 | `showEditorWorldObjectsWindow()` |
| 组件属性面板 | 同上 | `showEditorDetailWindow()`、`createClassUI()`、`createLeafNodeUI()`，通过反射访问字段 |
| 资源浏览与实例化 | 同上 | `showEditorFileContentWindow()`、`onFileContentItemClicked()`；点击对象资源后调用 `Level::createObject()` |
| 游戏视口与运行模式 | 同上 | `showEditorGameWindow()` |
| 选中对象与操作轴 | [editor_scene_manager.cpp](engine/source/editor/source/editor_scene_manager.cpp) | `onGObjectSelected()`、`updateCursorOnAxis()`、`moveEntity()`、`drawSelectedEntityAxis()` |
| 快捷键、鼠标、视口相机 | [editor_input_manager.cpp](engine/source/editor/source/editor_input_manager.cpp) | 注册回调，处理编辑器命令，将点击和拖动交给场景管理器 |
| 资源目录树 | [editor_file_service.cpp](engine/source/editor/source/editor_file_service.cpp) | `EditorFileService::buildEngineFileTree()` 枚举资源并构建浏览树 |
| 操作轴网格 | [axis.cpp](engine/source/editor/source/axis.cpp) | 操作轴几何资源 |

查找鼠标选中对象的流程，可以按 `EditorInputManager::onMouseButtonClicked()` → `EditorSceneManager::getGuidOfPickedMesh()` → `RenderSystem` → `PickPass` 阅读。拾取结果经渲染实例 ID 与 `GObjectID` 的映射，最终交给 `onGObjectSelected()`。

界面内容由 `EditorUI` 生成，提交绘制由 `UIPass` 完成，场景与界面的最终合成由 `CombineUIPass` 完成。

## 资源数据

### 文件位置

| 目录或文件 | 保存的内容 |
| --- | --- |
| [asset/world/](engine/asset/world/) | 世界名称、关卡 URL 列表、默认关卡 |
| [asset/level/](engine/asset/level/) | 关卡重力、活动角色名称、对象实例及组件配置 |
| [asset/objects/](engine/asset/objects/) | 对象定义、组件数据、模型、材质和动画资源，按角色、环境等分组 |
| [asset/objects/basic/](engine/asset/objects/basic/) | 立方体、球、胶囊等基础模型 |
| [asset/texture/](engine/asset/texture/) | 默认纹理、天空盒、IBL、BRDF LUT、调色 LUT 等共用纹理 |
| [rendering.global.json](engine/asset/global/rendering.global.json) | 天空盒、IBL、BRDF LUT、调色 LUT、环境光、方向光、初始相机、FXAA 与暗角配置 |
| [particle.global.json](engine/asset/global/particle.global.json) | 粒子模拟与渲染的全局参数 |
| [configs/development/](engine/configs/development/) | 开发启动配置模板 |
| [configs/deployment/](engine/configs/deployment/) | 部署启动配置模板，随可执行程序复制到输出目录 |

当前默认资源引用关系为：

```text
PiccoloEditor.ini
  → asset/world/hello.world.json
  → asset/level/1-1.level.json
  → 对象实例的 definition
  → asset/objects/.../*.object.json
  → 组件中的网格、材质、动画等资源引用
```

例如，默认关卡中的 `Player` 引用 [player.object.json](engine/asset/objects/character/player/player.object.json)。修改出生位置先看关卡中的实例 Transform；修改角色的通用组件配置先看对象定义。

### 资源类型

资源的 C++ 数据结构位于 [resource/res_type/](engine/source/runtime/resource/res_type/)。

| 子目录 | 主要类型 |
| --- | --- |
| [common/](engine/source/runtime/resource/res_type/common/) | `WorldRes`、`LevelRes`、`ObjectDefinitionRes`、`ObjectInstanceRes` |
| [components/](engine/source/runtime/resource/res_type/components/) | 网格、相机、动画、运动、刚体、粒子等组件的资源字段 |
| [data/](engine/source/runtime/resource/res_type/data/) | 材质、网格顶点、骨架、动画片段、混合状态、骨骼映射与遮罩、基础碰撞形状 |
| [global/](engine/source/runtime/resource/res_type/global/) | `GlobalRenderingRes`、`GlobalParticleRes` |

[AssetManager](engine/source/runtime/resource/asset_manager/asset_manager.h) 将 JSON 解析后交给 `Serializer` 读取为资源对象，也负责将资源对象序列化并保存。纹理和网格的文件解码入口位于 `RenderResourceBase`。

[ConfigManager](engine/source/runtime/resource/config_manager/config_manager.cpp) 解析 `PiccoloEditor.ini`，记录资源根目录、默认世界和全局配置位置。`AssetManager::getFullPath()` 按配置中的根目录解析资源路径。查找资源加载失败时，从实际读取的 INI、资源 URL 和这里的路径拼接开始。

运行资源位于 `bin/Debug/asset/` 或 `bin/Release/asset/`。源码中的资源位于 `engine/asset/`，由编辑器的 CMake 构建后步骤复制到输出目录；该步骤定义在 [editor/CMakeLists.txt](engine/source/editor/CMakeLists.txt)。

## 反射生成

反射同时用于资源序列化、组件的多态构造、编辑器属性显示和 Lua 字段访问。源码中的宏标记、构建时的 Parser 和运行时接口共同完成这些功能。

| 部分 | 位置 | 职责 |
| --- | --- | --- |
| 反射标记与访问接口 | [core/meta/reflection/](engine/source/runtime/core/meta/reflection/) | `REFLECTION_TYPE`、`CLASS`、`META`、`REFLECTION_BODY`，以及类型、字段、方法和数组访问器 |
| JSON 与序列化接口 | [core/meta/serializer/](engine/source/runtime/core/meta/serializer/)、[json.h](engine/source/runtime/core/meta/json.h) | `Serializer` 读写接口与 JSON 类型 |
| Parser 程序入口 | [meta_parser/parser/main.cpp](engine/source/meta_parser/parser/main.cpp) | 读取生成参数，调用 `MetaParser::parse()`、`generateFiles()` |
| C++ 解析 | [meta_parser/parser/parser/](engine/source/meta_parser/parser/parser/) | 基于 Clang 提取类型、字段、方法和元数据 |
| 类型与元数据处理 | [language_types/](engine/source/meta_parser/parser/language_types/)、[meta/](engine/source/meta_parser/parser/meta/)、[cursor/](engine/source/meta_parser/parser/cursor/) | 描述 C++ 类、字段、方法、标记信息和 Clang cursor |
| 生成器 | [generator/](engine/source/meta_parser/parser/generator/) | `ReflectionGenerator`、`SerializerGenerator` 组织模板数据并生成文件 |
| 模板 | [engine/template/](engine/template/) | Mustache 格式的反射与序列化代码模板 |
| 构建接入 | [precompile.cmake](engine/source/precompile/precompile.cmake)、[precompile.json.in](engine/source/precompile/precompile.json.in) | 汇总运行时和编辑器头文件，配置参数并运行 `PiccoloParser` |
| 生成结果 | [source/_generated/](engine/source/_generated/) | `reflection/` 和 `serializer/` 下的类型访问与 JSON 读写代码 |

```text
带反射标记的头文件
  → PiccoloParser 解析
  → Generator 与 engine/template/ 模板
  → engine/source/_generated/
  → TypeMetaRegister::metaRegister() 注册类型
  → 编辑器、资源加载、Lua 使用反射接口
```

新增或调整可序列化字段时，修改声明该字段的源头文件及其反射标记，然后重新生成。调整生成代码的共同形式时，修改生成器或模板。运行时注册的接入位置是 [reflection_register.cpp](engine/source/runtime/core/meta/reflection/reflection_register.cpp)。

## 基础模块

| 位置 | 功能 |
| --- | --- |
| [core/math/](engine/source/runtime/core/math/) | 向量、矩阵、四元数、Transform、包围盒、随机数和数学工具 |
| [core/base/](engine/source/runtime/core/base/) | 通用宏、断言、日志调用宏和哈希工具 |
| [core/log/](engine/source/runtime/core/log/) | `LogSystem` 对 spdlog 的封装 |
| [core/color/](engine/source/runtime/core/color/) | 颜色数据类型 |
| [platform/file_service/](engine/source/runtime/platform/file_service/) | `FileSystem::getFiles()` 递归枚举文件 |
| [platform/path/](engine/source/runtime/platform/path/) | 相对路径、路径分段、文件名与扩展名处理 |

主要第三方依赖都在 `engine/3rdparty/`，构建接入位于该目录的 [CMakeLists.txt](engine/3rdparty/CMakeLists.txt)。

| 依赖 | 用途 |
| --- | --- |
| `VulkanSDK`、`vulkanmemoryallocator` | Vulkan 接口、工具与 GPU 内存分配 |
| `glfw` | 窗口、输入与 Vulkan surface 接入 |
| `imgui` | 编辑器界面与 Vulkan UI backend |
| `JoltPhysics` | 物理模拟和碰撞查询 |
| `stb`、`tinyobjloader` | 图像与 OBJ 文件读取 |
| `json11` | JSON 解析与输出 |
| `spdlog` | 日志 |
| `lua-5.4.4`、`sol2-3.3.0` | Lua 运行环境与 C++ 绑定 |

## 构建调试

### 构建目标

| 目标或配置 | 定义位置 | 作用 |
| --- | --- | --- |
| 根工程 | [CMakeLists.txt](CMakeLists.txt) | 设置 C++ 标准、构建目录约束和输出目录，引入 `engine/` |
| 引擎构建 | [engine/CMakeLists.txt](engine/CMakeLists.txt) | 选择平台 Vulkan 库和 shader 编译器，组织依赖、运行时、编辑器与代码生成 |
| `PiccoloEditor` | [editor/CMakeLists.txt](engine/source/editor/CMakeLists.txt) | 编辑器可执行程序，链接 `PiccoloRuntime`，复制程序、INI、字体图标和运行资源 |
| `PiccoloRuntime` | [runtime/CMakeLists.txt](engine/source/runtime/CMakeLists.txt) | 引擎运行时库，链接渲染、物理、脚本等依赖，使用生成的反射与 shader 头文件 |
| `PiccoloShaderCompile` | [shader/CMakeLists.txt](engine/shader/CMakeLists.txt)、[ShaderCompile.cmake](cmake/ShaderCompile.cmake) | GLSL 编译和 shader 头文件生成 |
| `PiccoloParser` | [meta_parser/CMakeLists.txt](engine/source/meta_parser/CMakeLists.txt) | 反射与序列化代码生成工具 |
| `PiccoloPreCompile` | [precompile.cmake](engine/source/precompile/precompile.cmake) | 在构建运行时前执行 Parser |
| macOS 预设 | [CMakePresets.json](CMakePresets.json) | `macos-debug`、`macos-release` 的 Ninja 构建配置 |
| macOS 构建脚本 | [build_macos.sh](build_macos.sh) | 配置、导出编译数据库并构建选定配置 |

shader 的构建过程为：

```text
engine/shader/glsl/*
  → glslangValidator（搜索路径 engine/shader/include/）
  → engine/shader/generated/spv/*.spv
  → cmake/GenerateShaderCPPFile.cmake
  → engine/shader/generated/cpp/*.h
  → C++ Pass 创建 shader module
```

修改 GPU 程序时编辑 `glsl/` 或 `include/` 中的源文件，然后构建。生成的 `.spv` 和 C++ 头文件由构建流程更新。

### IDE 配置

| 文件 | 内容 |
| --- | --- |
| [.vscode/settings.json](.vscode/settings.json) | CMake 预设、clangd、GLSL include 搜索路径、shader 校验器和语言关联 |
| [.vscode/extensions.json](.vscode/extensions.json) | CMake Tools、clangd、GLSL IntelliSense、CodeLLDB 推荐 |
| [.vscode/tasks.json](.vscode/tasks.json) | Debug 与 Release 构建任务 |
| [.vscode/launch.json](.vscode/launch.json) | `PiccoloEditor` 调试启动配置 |
| `compile_commands.json` | C++ 编译参数、宏与头文件目录，供 clangd 使用 |

C++ 头文件解析问题先检查编译数据库和生成头文件；GLSL include 解析问题先检查 `glslLsp.includePaths`。当前 GLSL 工作区设置使用仓库内的 macOS `glslangValidator` 和 Vulkan 1.0 规则。

## 常见修改

| 修改目标 | 推荐查找顺序 |
| --- | --- |
| 添加组件 | `framework/component/` → 所需资源字段与反射标记 → 对象 JSON → 代码生成；需要在编辑模式更新时，再检查 `editor.cpp` 的组件注册 |
| 修改对象外观 | 关卡实例的 `definition` → 对象的 Mesh 配置 → 材质 JSON → 纹理引用；调整光照公式时继续读 `mesh_lighting.h / .inl` |
| 添加后处理 Pass | `passes/` 中的现有 Pass → 新 shader → `render_pass.h` 的枚举 → `MainCameraPass::setupRenderPass()` 的 attachment、subpass 和依赖 → `RenderPipeline` 的创建与资源连接 → 主相机绘制与重建流程 |
| 调整渲染资源绑定 | shader 的 `set / binding` → Pass 的 `setupDescriptorSetLayout()`、`setupDescriptorSet()` 与 descriptor 更新 → `RenderResource`；新增 descriptor 时检查 `VulkanRHI::createDescriptorPool()` |
| 添加编辑器面板 | `EditorUI::showEditorUI()` → 新面板函数 → 所需的 `EditorSceneManager` 或运行时接口 |
| 修改游戏按键 | `InputSystem`、`GameCommand` → 消费该命令的组件，如 `MotorComponent`、`CameraComponent` |
| 修改编辑器快捷键 | `EditorInputManager` → `EditorSceneManager` 或 `EditorUI` 中对应操作 |
| 修改默认世界或全局渲染参数 | `PiccoloEditor.ini` → `asset/world/` 或 `asset/global/` → 对应 `res_type` 与读取该配置的系统 |

首次阅读建议从 `main.cpp → editor.cpp → engine.cpp → global_context.cpp` 开始，先确认系统如何创建和每帧如何运行。随后按具体任务进入对象框架、渲染或编辑器模块。
