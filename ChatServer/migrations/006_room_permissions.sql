-- Apply after 005_room_management.sql, after a database backup.
-- Each ALTER is conditional so this script can be run again safely.
SET @vchat_has_column = (SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rooms' AND COLUMN_NAME='join_policy');
SET @vchat_sql = IF(@vchat_has_column=0,
  'ALTER TABLE rooms ADD COLUMN join_policy ENUM(''direct'',''approval'') NOT NULL DEFAULT ''direct''', 'SELECT 1');
PREPARE vchat_stmt FROM @vchat_sql; EXECUTE vchat_stmt; DEALLOCATE PREPARE vchat_stmt;

SET @vchat_has_column = (SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rooms' AND COLUMN_NAME='allow_member_invite');
SET @vchat_sql = IF(@vchat_has_column=0,
  'ALTER TABLE rooms ADD COLUMN allow_member_invite TINYINT(1) NOT NULL DEFAULT 0', 'SELECT 1');
PREPARE vchat_stmt FROM @vchat_sql; EXECUTE vchat_stmt; DEALLOCATE PREPARE vchat_stmt;

SET @vchat_has_column = (SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rooms' AND COLUMN_NAME='allow_member_edit');
SET @vchat_sql = IF(@vchat_has_column=0,
  'ALTER TABLE rooms ADD COLUMN allow_member_edit TINYINT(1) NOT NULL DEFAULT 0', 'SELECT 1');
PREPARE vchat_stmt FROM @vchat_sql; EXECUTE vchat_stmt; DEALLOCATE PREPARE vchat_stmt;

SET @vchat_has_column = (SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rooms' AND COLUMN_NAME='allow_member_kick');
SET @vchat_sql = IF(@vchat_has_column=0,
  'ALTER TABLE rooms ADD COLUMN allow_member_kick TINYINT(1) NOT NULL DEFAULT 0', 'SELECT 1');
PREPARE vchat_stmt FROM @vchat_sql; EXECUTE vchat_stmt; DEALLOCATE PREPARE vchat_stmt;

SET @vchat_has_column = (SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rooms' AND COLUMN_NAME='allow_member_manage_permissions');
SET @vchat_sql = IF(@vchat_has_column=0,
  'ALTER TABLE rooms ADD COLUMN allow_member_manage_permissions TINYINT(1) NOT NULL DEFAULT 0', 'SELECT 1');
PREPARE vchat_stmt FROM @vchat_sql; EXECUTE vchat_stmt; DEALLOCATE PREPARE vchat_stmt;

-- Existing room_type and all membership/message/invitation rows remain intact.
-- Rollback only after exporting current permission values and stopping the new server.
