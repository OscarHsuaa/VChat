-- Apply once to vchat_demo. Existing users and messages are untouched.
CREATE TABLE IF NOT EXISTS chat_history (
    sequence_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    room_id VARCHAR(64) NOT NULL DEFAULT 'class-room',
    message_id CHAR(36) NOT NULL,
    sender_id BIGINT NULL,
    sender_name VARCHAR(255) NOT NULL,
    message_type ENUM('user','ai') NOT NULL,
    content MEDIUMTEXT NOT NULL,
    reply_to_message_id CHAR(36) NULL,
    original_request_id BIGINT UNSIGNED NULL,
    visibility ENUM('team','personal') NOT NULL DEFAULT 'team',
    recipient_user_id BIGINT NULL,
    legacy_message_id INT NULL,
    created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    UNIQUE KEY uq_chat_history_message (message_id),
    UNIQUE KEY uq_chat_history_legacy (legacy_message_id),
    KEY ix_chat_history_room_order (room_id, sequence_id),
    KEY ix_chat_history_reply (reply_to_message_id),
    KEY ix_chat_history_visibility (room_id, visibility, recipient_user_id, sequence_id)
) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- Backfill previously saved user messages. Their original sender IDs and
-- client message IDs were never stored, so they cannot be reconstructed.
-- The unique legacy ID makes this safe to run again.
INSERT INTO chat_history
    (room_id, message_id, sender_id, sender_name, message_type, content,
     visibility, created_at, legacy_message_id)
SELECT 'class-room', UUID(), NULL, sender, 'user',
       CASE WHEN LEFT(content, CHAR_LENGTH(sender) + 2) = CONCAT(sender, ': ')
            THEN SUBSTRING(content, CHAR_LENGTH(sender) + 3)
            ELSE content END,
       'team', COALESCE(sent_at, NOW(3)), id
FROM messages
WHERE group_name = '班级群' OR group_name IS NULL
ON DUPLICATE KEY UPDATE legacy_message_id = legacy_message_id;
