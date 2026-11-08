-- Back up vchat_demo before applying. Run after 001 and 002.
-- Existing chat_history.room_id='class-room' is retained, so no message IDs,
-- content, timestamps or reply links are rewritten. The historical group_name
-- audit must show only '班级群' before this migration is used.
-- User IDs are BIGINT without a foreign key because existing installations
-- use different INT/BIGINT definitions for users.id; the server validates
-- identity against the authenticated session before every room mutation.
CREATE TABLE IF NOT EXISTS rooms (
    room_id VARCHAR(64) NOT NULL PRIMARY KEY,
    room_name VARCHAR(100) NOT NULL,
    description VARCHAR(500) NOT NULL DEFAULT '',
    owner_id BIGINT NULL,
    room_type ENUM('invite_only','public') NOT NULL DEFAULT 'invite_only',
    created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
    KEY ix_rooms_type_name (room_type, room_name),
    KEY ix_rooms_owner (owner_id)
) ENGINE=InnoDB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS room_members (
    room_id VARCHAR(64) NOT NULL,
    user_id BIGINT NOT NULL,
    role ENUM('owner','member') NOT NULL DEFAULT 'member',
    joined_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (room_id,user_id),
    KEY ix_room_members_user (user_id,room_id),
    CONSTRAINT fk_room_members_room FOREIGN KEY (room_id) REFERENCES rooms(room_id)
) ENGINE=InnoDB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS room_invites (
    token_hash CHAR(64) NOT NULL PRIMARY KEY,
    room_id VARCHAR(64) NOT NULL,
    created_by BIGINT NOT NULL,
    expires_at DATETIME(3) NOT NULL,
    used_by BIGINT NULL,
    used_at DATETIME(3) NULL,
    created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    KEY ix_room_invites_room_expiry (room_id,expires_at),
    CONSTRAINT fk_room_invites_room FOREIGN KEY (room_id) REFERENCES rooms(room_id)
) ENGINE=InnoDB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- The lobby is system-owned; every pre-existing account keeps access.
INSERT INTO rooms(room_id,room_name,description,owner_id,room_type)
VALUES('class-room','VChat 大厅','原有公共聊天室',NULL,'public')
ON DUPLICATE KEY UPDATE room_id=room_id;
INSERT INTO room_members(room_id,user_id,role)
SELECT 'class-room',id,'member' FROM users
ON DUPLICATE KEY UPDATE room_id=room_id;

-- Rollback plan: first stop the new server. Preserve chat_history unchanged;
-- export rooms, room_members and room_invites before dropping these three new
-- tables in reverse dependency order. Do not drop a table that has acquired
-- user-created rooms or invitations without exporting those records.
