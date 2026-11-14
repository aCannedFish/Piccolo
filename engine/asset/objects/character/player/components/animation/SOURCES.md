# 动画来源

`data/` 中以下八个动画片段来自 [BoomingTech/Piccolo 的 PA03 作业分支](https://github.com/BoomingTech/Piccolo/tree/b46af1a2b998b5976063b7a6b091cdd1bb62ccca/engine/asset/objects/character/player/components/animation/data)，固定提交为 `b46af1a2b998b5976063b7a6b091cdd1bb62ccca`。文件保留上游原始内容，沿用仓库根目录的 MIT 许可证。

- `idle.animation_clip.json`
- `walk.animation_clip.json`
- `run.animation_clip.json`
- `walk_forward_start.animation_clip.json`
- `walk_forward_end.animation_clip.json`
- `jump_start.animation_clip.json`
- `jump_loop.animation_clip.json`
- `jump_end.animation_clip.json`

上游 skeleton 数据与当前仓库的数据完全一致。状态配置位于 `player.object.json`：idle、walk_run、jump_loop 循环播放，其余状态播放至末帧。walk_run 以速度比例 1 和 2 作为走路、跑步采样点，在两者之间线性混合，并使用共享播放相位。

PA03 文档区分静止起跳和移动起跳的状态。当前资源配置让这两条路径共用起跳、腾空和落地片段，地面运动状态由 Motor 的速度及接触结果决定。
