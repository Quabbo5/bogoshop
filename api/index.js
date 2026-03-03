/**
 * Bogoshop License API
 * Hosted on Railway — POST /verify to activate a key
 */
const express = require('express');
const { Pool }  = require('pg');
const crypto    = require('crypto');

const app = express();
app.use(express.json());

const pool = new Pool({
    connectionString: process.env.DATABASE_URL,
    ssl: { rejectUnauthorized: false }
});

/* ── Key-Validation (portiert aus license.c) ──────────────────────────── */
const KEY_ALPHA   = "ACDEFGHJKLMNPQRTVWXY3479";
const KEY_SECRET2 = 0x3D7C1F94E2B6A850n;
const BASE        = 24n;
const CHECK_MOD   = BASE ** 5n; // 7962624n

function validateKey(key) {
    const clean = key.replace(/-/g, '').toUpperCase();
    if (clean.length !== 20) return false;

    const digits = [];
    for (const c of clean) {
        const v = KEY_ALPHA.indexOf(c);
        if (v < 0) return false;
        digits.push(BigInt(v));
    }

    // Erste 15 Ziffern → mixed data (base-24 decode)
    let data = 0n;
    for (let i = 0; i < 15; i++) data = data * BASE + digits[i];

    // Erwartete Prüfsumme
    const expected = (data ^ KEY_SECRET2) % CHECK_MOD;

    // Letzte 5 Ziffern → tatsächliche Prüfsumme
    let actual = 0n;
    for (let i = 15; i < 20; i++) actual = actual * BASE + digits[i];

    return expected === actual;
}

/* ── DB Setup ─────────────────────────────────────────────────────────── */
async function initDB() {
    await pool.query(`
        CREATE TABLE IF NOT EXISTS activations (
            id          SERIAL PRIMARY KEY,
            key_hash    TEXT NOT NULL,
            machine_id  TEXT NOT NULL,
            activated_at TIMESTAMPTZ DEFAULT NOW(),
            UNIQUE(key_hash, machine_id)
        )
    `);
    await pool.query(`
        CREATE TABLE IF NOT EXISTS revoked_keys (
            key_hash   TEXT PRIMARY KEY,
            revoked_at TIMESTAMPTZ DEFAULT NOW(),
            reason     TEXT
        )
    `);
}

function hashKey(key) {
    return crypto.createHash('sha256')
        .update(key.replace(/-/g, '').toUpperCase())
        .digest('hex');
}

/* ── POST /verify ─────────────────────────────────────────────────────── */
app.post('/verify', async (req, res) => {
    const { key, machine_id } = req.body ?? {};

    if (!key || !machine_id)
        return res.status(400).json({ valid: false, message: 'missing fields' });

    // 1. Mathematische Prüfung
    if (!validateKey(key))
        return res.json({ valid: false, message: 'invalid key' });

    const keyHash = hashKey(key);

    // 2. Widerruf prüfen
    const revoked = await pool.query(
        'SELECT 1 FROM revoked_keys WHERE key_hash=$1', [keyHash]);
    if (revoked.rows.length > 0)
        return res.json({ valid: false, message: 'key revoked' });

    // 3. Aktivierungslimit prüfen (max 2 Geräte pro Key)
    const { rows } = await pool.query(
        'SELECT machine_id FROM activations WHERE key_hash=$1', [keyHash]);

    const alreadyActivated = rows.some(r => r.machine_id === machine_id);
    const MAX_DEVICES = 2;

    if (!alreadyActivated && rows.length >= MAX_DEVICES)
        return res.json({ valid: false, message: 'max devices reached' });

    // 4. Aktivierung speichern (idempotent)
    await pool.query(
        `INSERT INTO activations (key_hash, machine_id)
         VALUES ($1, $2) ON CONFLICT DO NOTHING`,
        [keyHash, machine_id]
    );

    res.json({ valid: true, message: 'ok' });
});

/* ── GET /health ──────────────────────────────────────────────────────── */
app.get('/health', (_, res) => res.json({ status: 'ok' }));

/* ── Admin: GET /activations?key=XXXXX ───────────────────────────────── */
app.get('/activations', async (req, res) => {
    const adminToken = req.headers['x-admin-token'];
    if (adminToken !== process.env.ADMIN_TOKEN)
        return res.status(401).json({ error: 'unauthorized' });

    const { key } = req.query;
    if (!key) return res.status(400).json({ error: 'missing key' });

    const keyHash = hashKey(key);
    const { rows } = await pool.query(
        'SELECT machine_id, activated_at FROM activations WHERE key_hash=$1 ORDER BY activated_at',
        [keyHash]
    );
    res.json({ key_hash: keyHash, activations: rows });
});

/* ── Admin: POST /revoke ──────────────────────────────────────────────── */
app.post('/revoke', async (req, res) => {
    const adminToken = req.headers['x-admin-token'];
    if (adminToken !== process.env.ADMIN_TOKEN)
        return res.status(401).json({ error: 'unauthorized' });

    const { key, reason } = req.body ?? {};
    if (!key) return res.status(400).json({ error: 'missing key' });

    const keyHash = hashKey(key);
    await pool.query(
        `INSERT INTO revoked_keys (key_hash, reason)
         VALUES ($1, $2) ON CONFLICT DO NOTHING`,
        [keyHash, reason ?? null]
    );
    res.json({ revoked: true, key_hash: keyHash });
});

/* ── Start ────────────────────────────────────────────────────────────── */
const PORT = process.env.PORT || 3000;
initDB()
    .then(() => app.listen(PORT, () => console.log(`bogoshop-api on :${PORT}`)))
    .catch(err => { console.error('DB init failed:', err); process.exit(1); });
