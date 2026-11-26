-- Apply to the VChat business database before enabling message notifications.
-- Existing members start at the migration's history boundary. Re-running the
-- migration preserves all established read positions.
SET @notification_cursor_ddl = (
    SELECT IF(COUNT(*) = 0,
        'ALTER TABLE room_members ADD COLUMN last_read_sequence_id BIGINT UNSIGNED NULL',
        'SELECT ''Read position already present''')
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'room_members'
      AND COLUMN_NAME = 'last_read_sequence_id'
);
PREPARE notification_cursor_stmt FROM @notification_cursor_ddl;
EXECUTE notification_cursor_stmt;
DEALLOCATE PREPARE notification_cursor_stmt;

UPDATE room_members m LEFT JOIN (
    SELECT room_id, MAX(sequence_id) AS last_sequence_id
    FROM chat_history WHERE message_type = 'user' GROUP BY room_id
) h ON h.room_id = m.room_id
SET m.last_read_sequence_id = COALESCE(h.last_sequence_id, 0)
WHERE m.last_read_sequence_id IS NULL;

ALTER TABLE room_members MODIFY COLUMN last_read_sequence_id BIGINT UNSIGNED NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS chat_message_mentions (
    sequence_id BIGINT UNSIGNED NOT NULL,
    user_id BIGINT NOT NULL,
    PRIMARY KEY (sequence_id, user_id),
    CONSTRAINT fk_message_mentions_history FOREIGN KEY (sequence_id)
        REFERENCES chat_history(sequence_id) ON DELETE CASCADE
) ENGINE=InnoDB;
