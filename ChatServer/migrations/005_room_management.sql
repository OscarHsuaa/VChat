-- Back up the database before applying. This migration preserves every room,
-- membership, invitation and chat_history row. Safe to run more than once.
SET @vchat_has_dissolved_at = (
  SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'rooms' AND COLUMN_NAME = 'dissolved_at'
);
SET @vchat_sql = IF(@vchat_has_dissolved_at = 0,
  'ALTER TABLE rooms ADD COLUMN dissolved_at DATETIME(3) NULL DEFAULT NULL',
  'SELECT 1');
PREPARE vchat_stmt FROM @vchat_sql;
EXECUTE vchat_stmt;
DEALLOCATE PREPARE vchat_stmt;

-- Rollback, only after stopping the new server and exporting room state:
-- ALTER TABLE rooms DROP COLUMN dissolved_at;
-- Do not roll back after any room has been dissolved unless its dissolved_at
-- values have been exported; dropping this column would reactivate those rooms.
