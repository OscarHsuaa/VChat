-- Apply once before starting the upgraded ChatServer. Existing rows stay in
-- place; NULL means an older completed AI reply or a normal user message.
-- This script is idempotent on MySQL versions without ADD COLUMN IF NOT EXISTS.
SET @vchat_ai_state_ddl = (
    SELECT IF(COUNT(*) = 0,
        'ALTER TABLE chat_history ADD COLUMN ai_state ENUM(''pending'',''completed'',''failed'',''cancelled'',''timed_out'') NULL DEFAULT NULL',
        'SELECT ''ai_state already present''')
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chat_history' AND COLUMN_NAME = 'ai_state'
);
PREPARE vchat_ai_state_stmt FROM @vchat_ai_state_ddl;
EXECUTE vchat_ai_state_stmt;
DEALLOCATE PREPARE vchat_ai_state_stmt;

-- Rollback only if reverting the server code: ALTER TABLE chat_history DROP COLUMN ai_state;
-- Dropping this column loses only pending/failed state; message IDs, order and
-- reply content remain untouched. Back up the schema before any rollback.
