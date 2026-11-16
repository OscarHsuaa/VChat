-- Add durable, room-scoped Vivi summary shares without changing existing rows.
-- Run against the VChat database before starting the upgraded ChatServer.
SET @vchat_summary_type_ddl = (
    SELECT IF(COLUMN_TYPE LIKE '%''discussion_summary''%',
        'SELECT ''discussion_summary already present''',
        'ALTER TABLE chat_history MODIFY COLUMN message_type ENUM(''user'',''ai'',''discussion_summary'') NOT NULL')
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chat_history' AND COLUMN_NAME = 'message_type'
);
PREPARE vchat_summary_type_stmt FROM @vchat_summary_type_ddl;
EXECUTE vchat_summary_type_stmt;
DEALLOCATE PREPARE vchat_summary_type_stmt;

SET @vchat_summary_request_ddl = (
    SELECT IF(COUNT(*) = 0,
        'ALTER TABLE chat_history ADD COLUMN share_request_id CHAR(36) NULL',
        'SELECT ''share_request_id already present''')
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chat_history' AND COLUMN_NAME = 'share_request_id'
);
PREPARE vchat_summary_request_stmt FROM @vchat_summary_request_ddl;
EXECUTE vchat_summary_request_stmt;
DEALLOCATE PREPARE vchat_summary_request_stmt;

SET @vchat_summary_index_ddl = (
    SELECT IF(COUNT(*) = 0,
        'ALTER TABLE chat_history ADD UNIQUE KEY uq_chat_history_share_request (share_request_id)',
        'SELECT ''share request index already present''')
    FROM information_schema.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'chat_history'
      AND INDEX_NAME = 'uq_chat_history_share_request'
);
PREPARE vchat_summary_index_stmt FROM @vchat_summary_index_ddl;
EXECUTE vchat_summary_index_stmt;
DEALLOCATE PREPARE vchat_summary_index_stmt;
