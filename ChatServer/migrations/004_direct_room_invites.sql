-- Apply after 003_rooms.sql. Back up the database first.
-- Legacy room_invites remains unchanged: it contains bearer-token invitations.
-- This table adds addressed invitations with a recipient and an inbox state.
CREATE TABLE IF NOT EXISTS room_direct_invites (
    invite_id CHAR(36) NOT NULL PRIMARY KEY,
    room_id VARCHAR(64) NOT NULL,
    recipient_user_id BIGINT NOT NULL,
    invited_by BIGINT NOT NULL,
    status ENUM('pending','accepted','declined') NOT NULL DEFAULT 'pending',
    created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    expires_at DATETIME(3) NOT NULL,
    responded_at DATETIME(3) NULL,
    UNIQUE KEY uq_direct_room_recipient (room_id, recipient_user_id),
    KEY ix_direct_recipient_status (recipient_user_id, status, expires_at),
    CONSTRAINT fk_direct_invite_room FOREIGN KEY (room_id) REFERENCES rooms(room_id)
) ENGINE=InnoDB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- Re-running CREATE TABLE IF NOT EXISTS leaves existing users, rooms, messages,
-- legacy invite tokens and addressed invitations untouched.
-- Rollback: stop the new server, export room_direct_invites, then DROP only
-- room_direct_invites. Never drop or rebuild chat_history or room_members.
