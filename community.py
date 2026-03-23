from supabase import create_client, Client
import io, os, json
from dotenv import load_dotenv

load_dotenv()

SUPABASE_URL         = os.getenv("SUPABASE_URL")
SUPABASE_KEY         = os.getenv("SUPABASE_KEY")
SUPABASE_SERVICE_KEY = os.getenv("SUPABASE_SERVICE_KEY")
BUCKET               = "community-images"
SESSION_FILE         = ".session.json"

class CommunityClient:
    def __init__(self):
        self.sb: Client = create_client(SUPABASE_URL, SUPABASE_KEY)
        self.session = None
        self.username = None
        self._restore_session()

    # --- session persistence ---

    def _save_session(self):
        try:
            with open(SESSION_FILE, "w") as f:
                json.dump({
                    "refresh_token": self.session.refresh_token,
                    "username":      self.username
                }, f)
        except Exception:
            pass

    def _restore_session(self):
        if not os.path.exists(SESSION_FILE):
            return
        try:
            with open(SESSION_FILE) as f:
                data = json.load(f)
            res = self.sb.auth.refresh_session(data["refresh_token"])
            if res.session:
                self.session  = res.session
                self.username = data.get("username", "")
                self._apply_token(res.session.access_token)
        except Exception:
            os.remove(SESSION_FILE)

    def _clear_session(self):
        if os.path.exists(SESSION_FILE):
            os.remove(SESSION_FILE)

    # --- auth ---

    def register(self, email: str, password: str, username: str) -> str | None:
        try:
            res = self.sb.auth.sign_up({
                "email": email,
                "password": password,
                "options": {"data": {"username": username}}
            })
            if res.user is None:
                return "Registration failed."
            self.session  = res.session
            self.username = username
            self._apply_token(res.session.access_token)
            self._save_session()
            return None
        except Exception as e:
            return str(e)

    def login(self, email: str, password: str) -> str | None:
        try:
            res = self.sb.auth.sign_in_with_password({"email": email, "password": password})
            if res.session is None:
                return "Invalid email or password."
            self.session  = res.session
            self.username = res.user.user_metadata.get("username", email.split("@")[0])
            self._apply_token(res.session.access_token)
            self._save_session()
            return None
        except Exception as e:
            return str(e)

    def _apply_token(self, token: str):
        self.sb.postgrest.auth(token)
        self.sb.storage.session = type("S", (), {"access_token": token})()

    def logout(self):
        self.sb.auth.sign_out()
        self.session  = None
        self.username = None
        self._clear_session()

    @property
    def logged_in(self) -> bool:
        return self.session is not None

    # --- posts ---

    def upload_post(self, image_pil, title: str, effects: list[str], description: str = "") -> str | None:
        if not self.logged_in:
            return "Not logged in."
        try:
            import requests as _req

            token    = self.session.access_token
            user_id  = self.session.user.id
            service_headers = {"Authorization": f"Bearer {SUPABASE_SERVICE_KEY}", "apikey": SUPABASE_SERVICE_KEY}
            auth_headers    = {"Authorization": f"Bearer {token}", "apikey": SUPABASE_KEY}
            filename = f"{user_id}/{title.replace(' ', '_')}.jpg"

            buf = io.BytesIO()
            image_pil.convert("RGB").save(buf, format="JPEG", quality=88)
            buf.seek(0)

            upload_url = f"{SUPABASE_URL}/storage/v1/object/{BUCKET}/{filename}"
            r = _req.post(upload_url, headers={**service_headers, "Content-Type": "image/jpeg",
                                               "x-upsert": "true"}, data=buf.getvalue())
            if r.status_code not in (200, 201):
                return f"Storage error: {r.text}"

            image_url = f"{SUPABASE_URL}/storage/v1/object/public/{BUCKET}/{filename}"

            r = _req.post(
                f"{SUPABASE_URL}/rest/v1/posts",
                headers={**auth_headers, "Content-Type": "application/json", "Prefer": "return=minimal"},
                json={"user_id": user_id, "username": self.username,
                      "image_url": image_url, "title": title,
                      "effects": ", ".join(effects), "description": description}
            )
            if r.status_code not in (200, 201):
                return f"DB error: {r.text}"

            return None
        except Exception as e:
            return str(e)

    def fetch_posts(self, limit: int = 40) -> list[dict]:
        try:
            res = self.sb.table("posts").select("*").order("created_at", desc=True).limit(limit).execute()
            return res.data or []
        except Exception:
            return []

    def fetch_user_posts(self, username: str, limit: int = 100) -> list[dict]:
        try:
            res = (self.sb.table("posts").select("*")
                   .eq("username", username)
                   .order("created_at", desc=True)
                   .limit(limit).execute())
            return res.data or []
        except Exception:
            return []
