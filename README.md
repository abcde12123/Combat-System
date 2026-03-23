# UE5 Soulslike Combat System

基于 Unreal Engine 5 开发的 3D 战斗系统，旨在还原“魂类”游戏的打击感与操作体验。

## 核心技术点
- **战斗逻辑 (C++ & BP)：** 结合 Animation Montage 与 Notify 窗口实现精准的伤害判定。
- **目标锁定 (Targeting)：** 实现了基视野插值的平滑锁定系统。
- **攻击检测：**实现了基于球体拓展的扇形碰撞检测
- **动作衔接：** 包含轻重攻击连招（Combo）、翻滚闪避（Dodge）及受击硬直（Hit Stun）。
- **组件化设计：** 战斗属性（血量、体力）与行为逻辑高度解耦，易于扩展新角色。

## 🛠️ 开发环境
- Unreal Engine 5.5
- JetBrains Rider

## 📸 项目展示
