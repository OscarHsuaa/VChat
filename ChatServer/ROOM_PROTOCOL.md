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
| `invite` | `room_id`、`expires_minutes`（默认 60，范围 1～10080） | 房主或获邀请权限的成员可签发一次性随机令牌；数据库仅存 SHA-256 摘要。 |
| `search_users` | `room_id`、`query`、可选 `request_id` | 房主或获邀请权限的成员可搜索已注册用户；返回最多 40 人，包含 `user_id`、昵称、成员/待邀/在线状态，不返回本人。 |
| `send_user_invites` | `room_id`、`user_ids`（1～20 个账号 ID 字符串）、可选 `request_id` | 房主或获邀请权限的成员可定向邀请；整批事务提交，拒绝不存在、本人、已加入、已有有效待处理邀请和重复 ID。邀请 7 天内有效。 |
| `pending_invites` | `{}` | 返回当前已认证账号收到的未过期、未处理邀请。 |
| `respond_invite` | `invite_id`、`decision`（`accept` 或 `decline`）、可选 `request_id` | 仅接收者可处理；事务锁定邀请并在接受时加入成员表。重复处理及过期邀请被拒绝。 |
| `join_invite` | `token` | 原子校验未过期、未使用及未重复加入；成功后消耗令牌。 |
| `join_public` | `room_id` | 已登录用户仅可直接加入 `public` 且 `join_policy=direct` 的房间；重复加入被拒绝。 |
| `details` | `room_id`、可选 `request_id` | 仅成员可读取房间名称、类型、简介、房主昵称、人数与创建时间。 |
| `permissions` | `room_id`、`request_id` | 仅成员可读取当前房间的隐私、加入规则和成员权限。 |
| `update_permissions` | `room_id`、`permissions`、`request_id` | 仅房主可修改非大厅房间；`permissions` 包含 `room_type` (`invite_only`/`public`)、`join_policy` (`direct`)、`allow_member_invite/edit/kick` 与固定为 `false` 的 `allow_member_manage_permissions`。成功后广播 `permissions_updated`。 |
| `update_details` | `room_id`、`room_name`、`description`、`request_id` | 房主或获编辑权限的成员可修改非大厅房间；名称 2～30 个 Unicode 字符，简介至多 200 字；成功后广播 `room_updated`。 |
| `remove_member` | `room_id`、`user_id`、`request_id` | 房主或获移除权限的成员可移除普通成员；房主不可移除，被移除者失去访问权限并收到 `access_revoked`。 |
| `leave` | `room_id`、`request_id` | 普通成员可退出聊天室；默认大厅和房主不可直接退出。服务端将该账号的活动连接切至大厅。 |
| `dissolve` | `room_id`、`request_id` | 仅房主可软解散非大厅房间；保留历史及成员审计记录，拒绝后续读写和加入，广播 `room_dissolved`。 |
| `members` | `room_id` | 仅成员可查询成员列表。 |
| `history` | `room_id` | 仅成员可读取历史；响应后依次发送 `HISTORY|...` 和 `HISTORY_END`。历史 JSON 含 `room_id`。 |
| `switch` | `room_id` | 仅成员可切换当前房间；随后发送该房间历史、AI 快照与在线列表。 |

服务端可发送 `ROOM|notice|<base64(JSON)>\n`：`invite_received` 提醒接收者刷新待办；`invites_updated` 在登录或处理邀请后触发刷新；`members_changed` 提醒重新查询成员；`room_updated` 携带更新后的详情；`permissions_updated` 携带房间详情与最新权限，通知成员同步操作入口；`access_revoked` 通知被移除或主动退出的账号；`room_dissolved` 通知房间成员。后两种事件要求客户端移除导航项并切回有权限的房间。通知不携带敏感邀请令牌。成员查询结果包含服务端计算的 `online` 与 `role`；在线人数只计真实用户，不含 Vivi。客户端接受邀请后刷新房间列表，保留当前房间和输入草稿，由用户自行切换进入新房间。

`room_type` 复用原有房间可见性字段，旧房间的类型不变。新增权限默认全部关闭，房主始终有管理权。`join_policy=approval` 的申请与审核业务尚未实现：客户端禁用该选项，服务端拒绝保存，`join_public` 只接受 `direct`，不允许通过伪造协议绕过审核。公开房间发现页面仍待开发。

普通 `CHAT2`、`AI_CANCEL`、`AI_REGENERATE` 始终作用于服务器记录的**当前活动房间**，不信任请求中的 `room_id`。普通消息和 AI 流事件只广播给当前活动在同一房间的连接。不同房间使用独立 `AiRoom` 上下文及不同的数值请求 ID 区间。实时 `CHAT2`、历史帧和 AI 流事件均包含 `room_id`；客户端切换后以该字段过滤历史和实时消息。`switch` 成功响应后，服务端直接调用与 `history` 操作相同的历史查询，依序发送历史、AI 快照和在线列表；客户端串行处理切换请求，避免快速点击时旧响应覆盖新房间。

## 迁移与回滚

先备份数据库，例如在安全的、仓库以外的位置运行 `mysqldump -u <user> -p vchat_demo > <backup-path>`。旧库先按顺序应用 `001_chat_history.sql`、`002_ai_history_state.sql`、[`003_rooms.sql`](migrations/003_rooms.sql)，详见脚本前置条件。定向邀请再应用 [`004_direct_room_invites.sql`](migrations/004_direct_room_invites.sql)。本阶段再应用 [`005_room_management.sql`](migrations/005_room_management.sql)，只为 `rooms` 添加可空的 `dissolved_at`，不会改动消息或原房间内容；脚本可重复执行。新版服务器启动前必须完成 005。

权限与隐私功能还需执行 [`006_room_permissions.sql`](migrations/006_room_permissions.sql)，为 `rooms` 新增加入规则与成员权限列，默认关闭授权，不修改旧房间类型及历史记录。脚本每列独立检查，可重复执行；**先备份并停止旧服务器，再执行 006，最后启动新版服务器**。若回滚旧版服务器，新增列可保留；只有已导出新权限值且明确不再使用时才考虑删除新列，避免配置丢失。

未执行 006 时，新版服务器仍按原有结构返回「我的聊天室」、公开房间及聊天室详情，普通成员权限使用安全默认值；权限设置读取和保存会明确提示先迁移。房间列表 SQL 失败会返回 `ok:false` 与错误，客户端不应将其显示为「尚未加入聊天室」。

回滚服务端时，先停止新版服务器。旧版服务端可以继续使用原 `chat_history`，新表可暂时保留。若必须移除 004，先导出 `room_direct_invites`，再仅删除该新表。若必须移除 003，也要先导出新房间及邀请数据；**不要删除或重建 `chat_history`**。若已创建真实新房间，不应在未导出其数据前删表。

## 新消息提醒

`ROOM|result|…` 的 `list_mine` 成功响应在已部署迁移时带 `notifications_supported: true`。客户端收到该标记后才发送 `NOTIFY`，服务器也只向成功订阅的连接发送此类帧。格式为 `NOTIFY|<action>|<Base64 UTF-8 JSON>`，沿用换行帧边界。

| 方向 | action | JSON 字段及含义 |
| --- | --- | --- |
| 客户端 → 服务端 | `sync` | `{}`。按会话身份重新取得全部加入房间的未读状态。登录、重连和房间列表更新后调用。 |
| 服务端 → 客户端 | `snapshot` | `rooms` 数组，每项含 `room_id`、`unread`、`mentions`、`read_sequence_id`、`through_sequence_id`。数量为整数，序号为十进制字符串。`through_sequence_id` 是已读位置与最后一条未读消息序号的较大值。 |
| 服务端 → 客户端 | `message` | `room_id`、`message_id`、`sender_id`、`sequence_id`、`type`、`mentioned`、`time`。目前 `type=user`。`mentioned` 只描述当前事件接收账号，`time` 为 UTC ISO 8601，不含聊天正文。 |
| 客户端 → 服务端 | `read` | `room_id`、`sequence_id`。到达最新消息位置且消息已展示后提交已展示的序号。服务端要求房间是该连接当前活动房间、账号仍是成员且序号属于该房间普通消息；已读位置只能前进。 |
| 服务端 → 客户端 | `room` | 单房间状态，字段与 `snapshot.rooms` 的一项一致。已读更新后回传给该账号已订阅的连接。 |
| 服务端 → 客户端 | `retry` | `{}`。本次未读查询或已读更新失败，客户端延迟重新同步。 |

`CHAT2`、`HISTORY` 增加可选 `sequence_id`，供客户端确认哪些消息已展示。其他房间只推送通知元数据。普通成员消息（包括旧式文本消息）计入未读，自己的消息、系统/在线事件、Vivi 回复及总结分享不计入。人类 @ 使用服务端校验过的 `mentions[].type=user` 与账号 ID，目标可以在其他房间或离线，但须是发送房间的成员。客户端用每个房间的已观察序号丢弃重复通知，重连时以服务端快照为准。

支持全体提及的服务端会在 `list_mine` 响应中附带 `mention_all_supported: true`。客户端从现有 `members` 结果确认房间至少有 3 名真人成员后，显示「@全体成员」。选择后沿用 `CHAT2.mentions`，发送 `type=all`、`id=all`、`name=全体成员` 和正文中的 UTF-16 `start`、`length`。纯文本里的同名词不会产生全体提醒。

服务端重新核对当前房间、成员数及正文中的完整提及标识，从成员表取得发送时的接收账号，排除发送者。Vivi 不在真人成员表中，也不会因全体提及而应答。重复选择或与个人提及重叠时，同一账号仅保留一条提及记录。提醒对象和消息在同一事务提交，实时提醒继续使用 `NOTIFY.message.mentioned`，离线时由 `NOTIFY.snapshot` 恢复。全体提及无需新增迁移，依赖现有 009。历史记录保留原始正文。

### 部署 009

备份业务数据库并停止旧服务端，执行 [`009_message_notifications.sql`](migrations/009_message_notifications.sql)，再重启升级后的服务端及客户端。例如在仓库根目录的 Windows `cmd.exe` 运行：

```cmd
mysql --default-character-set=utf8mb4 -u root -p vchat_demo < ChatServer\migrations\009_message_notifications.sql
```

脚本为 `room_members` 添加 `last_read_sequence_id`，并新建 `chat_message_mentions`，旧消息正文和序号保持原样。迁移时以各房间当前最后一条普通消息初始化已有成员，避免旧历史被算作新未读。重复迁移保留已建立的读进度。新成员只统计加入后的消息，退出或被移除时已读位置随成员记录删除。兼容旧客户端和未迁移的服务端；回滚旧服务端时可保留这两个存储项，以免丢失已读位置和提及数据。
