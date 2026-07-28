---
name: shincalendar
description: ShinCalendar 日程管理技能。用于从 TOML schema 初始化 SQLite 数据库、添加/查询/删除日程备忘及标签。当用户提到日程、备忘、日历、memo、calendar、ShinCalendar，或需要管理日程记录（添加、查看、搜索、删除）时，使用此技能。也适用于用户需要从 TOML 文件定义数据库表结构并创建 SQLite 数据库的场景。
---

# ShinCalendar 日程管理

基于 TOML schema 定义和 SQLite 的日程管理系统。支持初始化数据库、添加日程、查询日程、删除日程四个核心操作。

所有数据文件（数据库和 schema）统一放在 `.claude/data/` 目录下，方便整体拷贝移动。

## 文件结构

```
.claude/
├── skills/shincalendar/
│   ├── SKILL.md
│   └── scripts/shincalendar.py
└── data/
    ├── ShinCalendar.schema.toml   — 表结构定义
    └── ShinCalendar.db            — SQLite 数据库文件
```

## 工作流程

根据用户意图选择对应的操作：

### 1. 初始化数据库

当用户首次使用或需要从 TOML schema 创建数据库时：

```bash
python -X utf8 .claude/skills/shincalendar/scripts/shincalendar.py init .claude/data/ShinCalendar.schema.toml .claude/data/ShinCalendar.db
```

**Schema TOML 格式要点**：TOML 中表定义使用 `[tables.表名]`，列定义使用 inline table 数组。其中 `constraints` 伪列（`name="constraints"`）存放表级外键约束，`type` 字段包含完整的 FOREIGN KEY 定义，解析时需要拆分逗号并提取为表级约束，而非列定义。

### 2. 添加日程

当用户要记录新日程时：

1. 从用户描述中提取：内容、开始时间、结束时间、标签（可选）
2. 日期格式统一为 `YYYY-MM-DD`。用户说"7.31"即 `2026-07-31`，"8.29到9.3"即 `2026-08-29` 到 `2026-09-03`。当前年份为对话中的年份。
3. 使用脚本添加：

```bash
python -X utf8 .claude/skills/shincalendar/scripts/shincalendar.py add .claude/data/ShinCalendar.db --content "内容" --start "YYYY-MM-DD" --end "YYYY-MM-DD" --tags "标签1,标签2"
```

- 标签支持逗号分隔的多个标签，不存在时自动创建
- `--end` 为可选参数（单日日程可不填）

### 3. 查询日程

```bash
# 查看所有日程
python -X utf8 .claude/skills/shincalendar/scripts/shincalendar.py list .claude/data/ShinCalendar.db

# 搜索关键词
python -X utf8 .claude/skills/shincalendar/scripts/shincalendar.py search .claude/data/ShinCalendar.db --keyword "关键词"

# 按标签筛选
python -X utf8 .claude/skills/shincalendar/scripts/shincalendar.py search .claude/data/ShinCalendar.db --tag "标签名"

# 按日期范围筛选
python -X utf8 .claude/skills/shincalendar/scripts/shincalendar.py search .claude/data/ShinCalendar.db --from "YYYY-MM-DD" --to "YYYY-MM-DD"
```

查询结果以表格形式展示给用户，包含：ID、内容、开始时间、结束时间、标签。

### 4. 删除日程

```bash
# 软删除（标记 is_deleted=1，数据保留）
python -X utf8 .claude/skills/shincalendar/scripts/shincalendar.py delete .claude/data/ShinCalendar.db --id <memo-id>

# 物理删除（真正从数据库移除）
python -X utf8 .claude/skills/shincalendar/scripts/shincalendar.py delete .claude/data/ShinCalendar.db --id <memo-id> --hard
```

默认使用软删除，符合 schema 设计意图。除非用户明确要求彻底删除，才使用 `--hard`。

## 注意事项

- 所有 Python 命令都加 `-X utf8` 参数，避免 Windows 终端中文乱码
- 数据库和 schema 文件统一放在 `.claude/data/` 目录下，方便整体拷贝
- 如果数据库文件不存在，提示用户先执行初始化
- 时间相关的描述需要仔细理解用户意图，例如"今天开始到8月7日前的四个周六"需要计算具体日期
