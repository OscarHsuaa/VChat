# 多任务聊天室服务端协议（第一阶段）

第一阶段增加服务端和数据库能力；第二阶段第一部分的 Qt 客户端已接入 `list_mine`、`switch`、`members`，默认进入 `class-room`（VChat 大厅），可切换已加入的房间。创建、邀请和发现公开房间的客户端界面尚未实现。

## 身份与帧格式

先按现有 `LOGIN|username|password` 获取 `LOGIN_META|accountId|sessionToken`，再发送 `JOIN|nickname|sessionToken`。无有效会话不能进入聊天室。服务端从会话读取用户 ID，忽略 ROOM 请求中的任何自报身份。

每条 TCP 消息以 `\n` 结束。房间控制请求：

```text
ROOM|<action>|<base64(UTF-8 JSON object)>\n
```

统一响应：

```text
ROOM|result|<base64(UTF-8 JSON object)>\n
```

响应至少含 `action` 与 `ok`；失败时含 `error`。令牌只在创建邀请的响应中返回一次，不写入服务器日志。请求正文限制为 8192 个字符。

| action | 请求字段 | 权限与行为 |
| --- | --- | --- |
| `create` | `room_name`、`description`、`room_type`（省略时为 `invite_only`） | 已登录用户创建房间，自动成为 `owner`。 |
| `list_mine` | `{}` | 返回当前账号加入的房间。 |
| `list_public` | `{}` | 返回尚未加入的公开房间。 |
| `invite` | `room_id`、`expires_minutes`（默认 60，范围 1～10080） | 仅创建者可签发一次性随机令牌；数据库仅存 SHA-256 摘要。 |
| `join_invite` | `token` | 原子校验未过期、未使用及未重复加入；成功后消耗令牌。 |
| `join_public` | `room_id` | 已登录用户加入公开房间；重复加入被拒绝。 |
| `leave` | `room_id` | 普通成员可退出非当前活动房间；默认大厅和创建者不可直接退出。 |
| `members` | `room_id` | 仅成员可查询成员列表。 |
| `history` | `room_id` | 仅成员可读取历史；响应后依次发送 `HISTORY|...` 和 `HISTORY_END`。历史 JSON 含 `room_id`。 |
| `switch` | `room_id` | 仅成员可切换当前房间；随后发送该房间历史、AI 快照与在线列表。 |

普通 `CHAT2`、`AI_CANCEL`、`AI_REGENERATE` 始终作用于服务器记录的**当前活动房间**，不信任请求中的 `room_id`。普通消息和 AI 流事件只广播给当前活动在同一房间的连接。不同房间使用独立 `AiRoom` 上下文及不同的数值请求 ID 区间。实时 `CHAT2`、历史帧和 AI 流事件均包含 `room_id`；客户端切换后以该字段过滤历史和实时消息。`switch` 成功响应后，服务端直接调用与 `history` 操作相同的历史查询，依序发送历史、AI 快照和在线列表；客户端串行处理切换请求，避免快速点击时旧响应覆盖新房间。

## 迁移与回滚

先备份数据库，例如在安全的、仓库以外的位置运行 `mysqldump -u <user> -p vchat_demo > <backup-path>`。确认旧 `messages.group_name` 均为「班级群」且 `chat_history.room_id` 均为 `class-room` 后，按顺序执行现有 `001_chat_history.sql`、`002_ai_history_state.sql` 和 [`003_rooms.sql`](migrations/003_rooms.sql)。第三个脚本只创建新表和默认大厅，保留旧消息的 `room_id`、内容、时间、ID 与回复关系；重复执行不会重复加入旧用户。

回滚服务端时，先停止新版服务器。旧版服务端可以继续使用原 `chat_history`，新表可暂时保留。若必须删除新表，先导出新房间及邀请数据，再按 `room_invites`、`room_members`、`rooms` 的顺序删除；**不要删除或重建 `chat_history`**。若已创建真实新房间，不应在未导出其数据前删表。
