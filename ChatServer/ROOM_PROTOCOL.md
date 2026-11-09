# 多任务聊天室服务端协议（第一阶段）

服务端与 Qt 客户端已接入房间列表、切换、创建和定向成员邀请。客户端默认进入 `class-room`（VChat 大厅）；公开房间发现界面仍待开发。

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

响应至少含 `action` 与 `ok`；失败时含 `error`。请求可选 `request_id`，响应原样回传，客户端用于忽略过期搜索结果和匹配提交。令牌只在旧式 `invite` 响应中返回一次，不写入服务器日志。请求正文限制为 8192 个字符。

| action | 请求字段 | 权限与行为 |
| --- | --- | --- |
| `create` | `room_name`、`description`、`room_type`（省略时为 `invite_only`） | 已登录用户创建房间，自动成为 `owner`。 |
| `list_mine` | `{}` | 返回当前账号加入的房间。 |
| `list_public` | `{}` | 返回尚未加入的公开房间。 |
| `invite` | `room_id`、`expires_minutes`（默认 60，范围 1～10080） | 仅创建者可签发一次性随机令牌；数据库仅存 SHA-256 摘要。 |
| `search_users` | `room_id`、`query`、可选 `request_id` | 仅房主可搜索已注册用户；返回最多 40 人，包含 `user_id`、昵称、成员/待邀/在线状态，不返回本人。 |
| `send_user_invites` | `room_id`、`user_ids`（1～20 个账号 ID 字符串）、可选 `request_id` | 仅房主可定向邀请；整批事务提交，拒绝不存在、本人、已加入、已有有效待处理邀请和重复 ID。邀请 7 天内有效。 |
| `pending_invites` | `{}` | 返回当前已认证账号收到的未过期、未处理邀请。 |
| `respond_invite` | `invite_id`、`decision`（`accept` 或 `decline`）、可选 `request_id` | 仅接收者可处理；事务锁定邀请并在接受时加入成员表。重复处理及过期邀请被拒绝。 |
| `join_invite` | `token` | 原子校验未过期、未使用及未重复加入；成功后消耗令牌。 |
| `join_public` | `room_id` | 已登录用户加入公开房间；重复加入被拒绝。 |
| `details` | `room_id`、可选 `request_id` | 仅成员可读取房间名称、类型、简介、房主昵称、人数与创建时间。 |
| `update_details` | `room_id`、`room_name`、`description`、`request_id` | 仅房主可修改非大厅房间；名称 2～30 个 Unicode 字符，简介至多 200 字；成功后广播 `room_updated`。 |
| `remove_member` | `room_id`、`user_id`、`request_id` | 仅房主可移除普通成员；被移除者失去访问权限并收到 `access_revoked`。 |
| `leave` | `room_id`、`request_id` | 普通成员可退出聊天室；默认大厅和房主不可直接退出。服务端将该账号的活动连接切至大厅。 |
| `dissolve` | `room_id`、`request_id` | 仅房主可软解散非大厅房间；保留历史及成员审计记录，拒绝后续读写和加入，广播 `room_dissolved`。 |
| `members` | `room_id` | 仅成员可查询成员列表。 |
| `history` | `room_id` | 仅成员可读取历史；响应后依次发送 `HISTORY|...` 和 `HISTORY_END`。历史 JSON 含 `room_id`。 |
| `switch` | `room_id` | 仅成员可切换当前房间；随后发送该房间历史、AI 快照与在线列表。 |

服务端可发送 `ROOM|notice|<base64(JSON)>\n`：`invite_received` 提醒接收者刷新待办；`invites_updated` 在登录或处理邀请后触发刷新；`members_changed` 提醒重新查询成员；`room_updated` 携带更新后的详情；`access_revoked` 通知被移除或主动退出的账号；`room_dissolved` 通知房间成员。后两种事件要求客户端移除导航项并切回有权限的房间。通知不携带敏感邀请令牌。成员查询结果包含服务端计算的 `online` 与 `role`；在线人数只计真实用户，不含 Vivi。客户端接受邀请后刷新房间列表，保留当前房间和输入草稿，由用户自行切换进入新房间。

普通 `CHAT2`、`AI_CANCEL`、`AI_REGENERATE` 始终作用于服务器记录的**当前活动房间**，不信任请求中的 `room_id`。普通消息和 AI 流事件只广播给当前活动在同一房间的连接。不同房间使用独立 `AiRoom` 上下文及不同的数值请求 ID 区间。实时 `CHAT2`、历史帧和 AI 流事件均包含 `room_id`；客户端切换后以该字段过滤历史和实时消息。`switch` 成功响应后，服务端直接调用与 `history` 操作相同的历史查询，依序发送历史、AI 快照和在线列表；客户端串行处理切换请求，避免快速点击时旧响应覆盖新房间。

## 迁移与回滚

先备份数据库，例如在安全的、仓库以外的位置运行 `mysqldump -u <user> -p vchat_demo > <backup-path>`。旧库先按顺序应用 `001_chat_history.sql`、`002_ai_history_state.sql`、[`003_rooms.sql`](migrations/003_rooms.sql)，详见脚本前置条件。定向邀请再应用 [`004_direct_room_invites.sql`](migrations/004_direct_room_invites.sql)。本阶段再应用 [`005_room_management.sql`](migrations/005_room_management.sql)，只为 `rooms` 添加可空的 `dissolved_at`，不会改动消息或原房间内容；脚本可重复执行。新版服务器启动前必须完成 005。

回滚服务端时，先停止新版服务器。旧版服务端可以继续使用原 `chat_history`，新表可暂时保留。若必须移除 004，先导出 `room_direct_invites`，再仅删除该新表。若必须移除 003，也要先导出新房间及邀请数据；**不要删除或重建 `chat_history`**。若已创建真实新房间，不应在未导出其数据前删表。
