-- Run after 003_rooms.sql. Back up vchat_demo before applying.
-- Existing passwords and user rows are untouched. New accounts store a
-- PBKDF2-SHA256 digest in password_hash; legacy password receives '!'.
SET @has_hash := (SELECT COUNT(*) FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='users' AND COLUMN_NAME='password_hash');
SET @ddl := IF(@has_hash=0,
  'ALTER TABLE users ADD COLUMN password_hash VARCHAR(128) NULL DEFAULT NULL', 'SELECT 1');
PREPARE registration_stmt FROM @ddl;
EXECUTE registration_stmt;
DEALLOCATE PREPARE registration_stmt;

-- The index protects against concurrent duplicate registration. If existing
-- usernames are duplicated, this statement fails without deleting any data;
-- inspect duplicates before retrying the migration.
SET @has_unique := (SELECT COUNT(*) FROM information_schema.STATISTICS AS s
  WHERE s.TABLE_SCHEMA=DATABASE() AND s.TABLE_NAME='users'
    AND s.COLUMN_NAME='username' AND s.NON_UNIQUE=0
    AND (SELECT COUNT(*) FROM information_schema.STATISTICS AS u
      WHERE u.TABLE_SCHEMA=DATABASE() AND u.TABLE_NAME='users'
        AND u.INDEX_NAME=s.INDEX_NAME)=1);
SET @ddl := IF(@has_unique=0,
  'ALTER TABLE users ADD UNIQUE KEY uq_users_username (username)', 'SELECT 1');
PREPARE registration_stmt FROM @ddl;
EXECUTE registration_stmt;
DEALLOCATE PREPARE registration_stmt;
