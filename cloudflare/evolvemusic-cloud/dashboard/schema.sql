PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS users (
  id TEXT PRIMARY KEY,
  token_hash TEXT NOT NULL UNIQUE,
  created_at INTEGER NOT NULL,
  last_seen_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS user_state (
  user_id TEXT PRIMARY KEY,
  favorites_json TEXT NOT NULL DEFAULT '[]',
  history_json TEXT NOT NULL DEFAULT '[]',
  settings_json TEXT NOT NULL DEFAULT '{}',
  updated_at INTEGER NOT NULL,
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS provider_sessions (
  user_id TEXT NOT NULL,
  provider TEXT NOT NULL,
  encrypted_session TEXT NOT NULL,
  updated_at INTEGER NOT NULL,
  PRIMARY KEY (user_id, provider),
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS track_catalog (
  id TEXT PRIMARY KEY,
  provider TEXT NOT NULL DEFAULT 'cloud',
  source_id TEXT NOT NULL,
  title TEXT NOT NULL,
  artist TEXT NOT NULL DEFAULT '',
  album TEXT NOT NULL DEFAULT '',
  cover TEXT NOT NULL DEFAULT '',
  duration INTEGER NOT NULL DEFAULT 0,
  access TEXT NOT NULL DEFAULT 'cloud',
  r2_key TEXT,
  created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_track_catalog_title
  ON track_catalog(title);

CREATE INDEX IF NOT EXISTS idx_track_catalog_artist
  ON track_catalog(artist);

CREATE INDEX IF NOT EXISTS idx_provider_sessions_user
  ON provider_sessions(user_id);
