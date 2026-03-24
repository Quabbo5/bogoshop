from tkinter import *
from tkinter import filedialog
from PIL import Image, ImageDraw, ImageTk
import os
import json
import io
import threading


class ProfileMixin:

    _PROFILE_FILE = "profile.json"
    _PROFILE_PIC  = "profile_pic.jpg"  # base name; actual = profile_pic_<username>.jpg

    _avatar_colors = [
        "#E06C75","#E5C07B","#98C379","#56B6C2",
        "#61AFEF","#C678DD","#D19A66","#7EC8A4",
    ]

    def _make_avatar_img(self, username: str, size: int = 80,
                         pil_img=None) -> "ImageTk.PhotoImage":
        """Build a circular-ish square avatar. pil_img overrides letter fallback."""
        from PIL import ImageFont
        if pil_img is not None:
            img = pil_img.convert("RGB").resize((size, size))
        else:
            color = self._avatar_colors[hash(username) % len(self._avatar_colors)]
            img = Image.new("RGB", (size, size), color)
            draw = ImageDraw.Draw(img)
            letter = (username[0] if username else "?").upper()
            font = None
            for path in ["/System/Library/Fonts/Helvetica.ttc",
                         "/System/Library/Fonts/SFNSDisplay.ttf",
                         "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]:
                try:
                    font = ImageFont.truetype(path, int(size * 0.48))
                    break
                except Exception:
                    pass
            if font is None:
                try:    font = ImageFont.load_default(size=int(size * 0.48))
                except Exception: font = ImageFont.load_default()
            bb = draw.textbbox((0, 0), letter, font=font)
            tw, th = bb[2] - bb[0], bb[3] - bb[1]
            draw.text(((size - tw) // 2, (size - th) // 2 - 2),
                      letter, fill="#ffffff", font=font)
        return ImageTk.PhotoImage(img)

    def _profile_pic_path(self) -> str:
        username = self.community.username or "__guest__"
        return f"profile_pic_{username}.jpg"

    def _profile_load(self) -> dict:
        username = self.community.username or "__guest__"
        try:
            if not os.path.exists(self._PROFILE_FILE):
                return {"bio": ""}
            with open(self._PROFILE_FILE) as f:
                all_data = json.load(f)
            # migrate old flat format {"bio": "..."} → {username: {"bio": "..."}}
            if "bio" in all_data and not any(isinstance(v, dict) for v in all_data.values()):
                all_data = {username: all_data}
                with open(self._PROFILE_FILE, "w") as f:
                    json.dump(all_data, f, indent=2)
            return all_data.get(username, {"bio": ""})
        except Exception:
            return {"bio": ""}

    def _profile_save(self, data: dict):
        username = self.community.username or "__guest__"
        try:
            all_data = {}
            if os.path.exists(self._PROFILE_FILE):
                with open(self._PROFILE_FILE) as f:
                    all_data = json.load(f)
        except Exception:
            all_data = {}
        all_data[username] = data
        with open(self._PROFILE_FILE, "w") as f:
            json.dump(all_data, f, indent=2)

    def _open_profile(self):
        self._nav_stack.clear()
        self._current_view = lambda: self._open_profile()
        for w in self.wiki_view.winfo_children():
            w.destroy()

        BG = "#1A1A1A"
        data = self._profile_load()

        if not self.community.logged_in:
            hdr = Frame(self.wiki_view, bg="#221F3A")
            hdr.pack(fill=X)
            Label(hdr, text="Profile", font="Plus_Jakarta_Sans 14 bold",
                  bg="#221F3A", fg="#f5f5f5", padx=14, pady=8).pack(side=LEFT)
            Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)
            Label(self.wiki_view,
                  text="Log in via the Community tab to view your profile.",
                  font="Plus_Jakarta_Sans 10 italic", bg=BG, fg="#555555"
                  ).pack(pady=40)
            self._wiki_open = True
            self.wiki_view.lift()
            self.root.bind("<Escape>", self._close_wiki_inline)
            return

        username = self.community.username

        # ── banner ────────────────────────────────────────────────────────
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        Label(hdr, text="Profile", font="Plus_Jakarta_Sans 14 bold",
              bg="#221F3A", fg="#f5f5f5", padx=14, pady=8).pack(side=LEFT)
        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)

        # ── scrollable body ───────────────────────────────────────────────
        outer = Frame(self.wiki_view, bg=BG)
        outer.pack(fill=BOTH, expand=1)
        canvas = Canvas(outer, bg=BG, highlightthickness=0, bd=0)
        sb = Scrollbar(outer, orient=VERTICAL, command=canvas.yview,
                       bg="#1a1a1a", troughcolor="#0a0a0a",
                       activebackground="#333333", relief="flat", borderwidth=0, width=10)
        canvas.configure(yscrollcommand=sb.set)
        sb.pack(side=RIGHT, fill=Y)
        canvas.pack(side=LEFT, fill=BOTH, expand=1)
        body = Frame(canvas, bg=BG)
        _cw = canvas.create_window((0, 0), window=body, anchor="nw")
        canvas.bind("<Configure>", lambda e: canvas.itemconfig(_cw, width=e.width))
        body.bind("<Configure>", lambda _: canvas.configure(scrollregion=canvas.bbox("all")))
        self._bind_scroll(canvas)

        # ── profile card ──────────────────────────────────────────────────
        card = Frame(body, bg="#242424")
        card.pack(fill=X, padx=20, pady=20)

        # profile picture
        pic_frame = Frame(card, bg="#242424")
        pic_frame.pack(side=LEFT, padx=16, pady=16)

        self._profile_pic_label = Label(pic_frame, bg="#2D2D2D", cursor="hand2")
        self._profile_pic_label.pack()
        Label(pic_frame, text="click to change", font="Plus_Jakarta_Sans 7 italic",
              bg="#242424", fg="#555555").pack(pady=(4, 0))

        def _set_pic_label(pil_img=None):
            tk_img = self._make_avatar_img(username, size=80, pil_img=pil_img)
            self._profile_pic_label.config(image=tk_img, width=80, height=80)
            self._profile_pic_label.image = tk_img

        # Show local cache immediately, then try server
        _local = self._profile_pic_path()
        _set_pic_label(Image.open(_local) if os.path.exists(_local) else None)

        def _fetch_server_profile():
            prof = self.community.fetch_profile(username)
            url  = prof.get("avatar_url", "")
            bio  = prof.get("bio", "")
            if url:
                try:
                    import urllib.request as _ur
                    with _ur.urlopen(url, timeout=6) as r:
                        raw = r.read()
                    pil = Image.open(io.BytesIO(raw))
                    pil.save(_local, format="JPEG", quality=92)
                    body.after(0, lambda: _set_pic_label(pil))
                except Exception:
                    pass
            if bio:
                def _fill_bio():
                    bio_box.delete("1.0", END)
                    bio_box.insert("1.0", bio)
                body.after(0, _fill_bio)

        threading.Thread(target=_fetch_server_profile, daemon=True).start()

        def _change_pic():
            path = filedialog.askopenfilename(
                filetypes=[("Images", "*.jpg *.jpeg *.png *.webp")])
            if not path:
                return
            pil = Image.open(path).convert("RGB")
            w, h = pil.size
            m = min(w, h)
            pil = pil.crop(((w - m) // 2, (h - m) // 2,
                             (w + m) // 2, (h + m) // 2)).resize((200, 200))
            pil.save(self._profile_pic_path(), format="JPEG", quality=92)
            _set_pic_label(pil)
            def _upload():
                url = self.community.upload_avatar(pil)
                if url:
                    self.community.upsert_profile(avatar_url=url)
            threading.Thread(target=_upload, daemon=True).start()

        self._profile_pic_label.bind("<Button-1>", lambda _: _change_pic())

        # username + bio
        info = Frame(card, bg="#242424")
        info.pack(side=LEFT, fill=BOTH, expand=1, padx=(0, 16), pady=16)

        Label(info, text=username,
              font="Plus_Jakarta_Sans 16 bold", bg="#242424", fg="#f5f5f5",
              anchor="w").pack(fill=X)

        Label(info, text="Bio", font="Plus_Jakarta_Sans 8", bg="#242424",
              fg="#666666", anchor="w").pack(fill=X, pady=(10, 2))
        bio_box = Text(info, height=3, bg="#2D2D2D", fg="#c0c0c0",
                       insertbackground="#f5f5f5", relief=FLAT,
                       font="Plus_Jakarta_Sans 10",
                       highlightthickness=1, highlightbackground="#444444")
        bio_box.pack(fill=X)
        bio_box.insert("1.0", data.get("bio", ""))

        def _save_bio():
            bio = bio_box.get("1.0", END).strip()
            data["bio"] = bio
            self._profile_save(data)
            self._log("Profile saved.", "ok")
            threading.Thread(target=lambda: self.community.upsert_profile(bio=bio),
                             daemon=True).start()

        self._btn(info, "Save", _save_bio,
                  font="Plus_Jakarta_Sans 8 bold", padx=10, pady=4
                  ).pack(anchor="e", pady=(6, 0))

        # ── user posts ────────────────────────────────────────────────────
        Frame(body, bg="#2A2A2A", height=1).pack(fill=X, padx=20)
        Label(body, text="Your Posts",
              font="Plus_Jakarta_Sans 11 bold", bg=BG, fg="#f5f5f5",
              anchor="w", padx=20).pack(fill=X, pady=(12, 6))

        import urllib.request

        loading_lbl = Label(body, text="Loading…", font="Plus_Jakarta_Sans 9 italic",
                            bg=BG, fg="#555555", anchor="w", padx=20)
        loading_lbl.pack(fill=X)

        def _load_posts():
            posts = self.community.fetch_user_posts(username)
            body.after(0, lambda: _render_posts(posts))

        def _render_posts(posts):
            loading_lbl.destroy()
            if not posts:
                Label(body, text="No posts yet.",
                      font="Plus_Jakarta_Sans 9 italic", bg=BG, fg="#555555",
                      anchor="w", padx=20).pack(fill=X, pady=(0, 20))
                return
            gallery = Frame(body, bg=BG, padx=10)
            gallery.pack(fill=X, pady=(0, 20))

            def _loader(post):
                with urllib.request.urlopen(post["image_url"], timeout=8) as r:
                    data = r.read()
                return Image.open(io.BytesIO(data)).convert("RGB")

            def _meta(post):
                t = post.get("title", "untitled")
                if len(t) > 22: t = t[:20] + "…"
                return [
                    (t,                        "Plus_Jakarta_Sans 8 bold",   "#f5f5f5"),
                    (post.get("effects", ""),  "Plus_Jakarta_Sans 7 italic", "#666666"),
                ]

            self._make_responsive_gallery(
                canvas, gallery, posts, _loader, _meta,
                lambda post, lbl: self._open_post_view(post, lbl),
                cache_key=lambda post: post.get("image_url", "")
            )

        threading.Thread(target=_load_posts, daemon=True).start()

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._close_wiki_inline)

    def _open_user_profile(self, username: str):
        """Read-only profile view for any user, opened from a post."""
        import urllib.request
        self._nav_push()
        self._current_view = lambda: self._open_user_profile(username)
        for w in self.wiki_view.winfo_children():
            w.destroy()

        BG = "#1A1A1A"

        # banner
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        self._btn(hdr, "← Back", self._nav_back,
                  bg="#221F3A", fg="#888888",
                  font="Plus_Jakarta_Sans 9", padx=12, pady=8).pack(side=LEFT)
        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)

        # scrollable body
        outer = Frame(self.wiki_view, bg=BG)
        outer.pack(fill=BOTH, expand=1)
        canvas = Canvas(outer, bg=BG, highlightthickness=0, bd=0)
        sb = Scrollbar(outer, orient=VERTICAL, command=canvas.yview,
                       bg="#1a1a1a", troughcolor="#0a0a0a",
                       activebackground="#333333", relief="flat", borderwidth=0, width=10)
        canvas.configure(yscrollcommand=sb.set)
        sb.pack(side=RIGHT, fill=Y)
        canvas.pack(side=LEFT, fill=BOTH, expand=1)
        body = Frame(canvas, bg=BG)
        _cw = canvas.create_window((0, 0), window=body, anchor="nw")
        canvas.bind("<Configure>", lambda e: canvas.itemconfig(_cw, width=e.width))
        body.bind("<Configure>", lambda _: canvas.configure(scrollregion=canvas.bbox("all")))
        self._bind_scroll(canvas)

        # avatar + username card
        card = Frame(body, bg="#242424")
        card.pack(fill=X, padx=20, pady=20)

        # letter avatar placeholder, replaced when server photo loads
        _tk_av = self._make_avatar_img(username, size=80)
        av_lbl = Label(card, image=_tk_av, bg="#242424", width=80, height=80)
        av_lbl.image = _tk_av
        av_lbl.pack(side=LEFT, padx=16, pady=16)

        info = Frame(card, bg="#242424")
        info.pack(side=LEFT, fill=X, expand=1, padx=(0, 16), pady=16)
        Label(info, text=username, font="Plus_Jakarta_Sans 16 bold",
              bg="#242424", fg="#f5f5f5", anchor="w").pack(fill=X)

        bio_lbl = Label(info, text="", font="Plus_Jakarta_Sans 9 italic",
                        bg="#242424", fg="#888888",
                        anchor="w", justify=LEFT, wraplength=400)
        bio_lbl.pack(fill=X, pady=(4, 0))

        def _fetch_user_profile():
            prof = self.community.fetch_profile(username)
            bio  = prof.get("bio", "")
            url  = prof.get("avatar_url", "")
            if bio:
                body.after(0, lambda: bio_lbl.config(text=bio))
            if url:
                try:
                    with urllib.request.urlopen(url, timeout=6) as r:
                        raw = r.read()
                    pil = Image.open(io.BytesIO(raw)).convert("RGB")
                    tk_img = self._make_avatar_img(username, size=80, pil_img=pil)
                    def _upd(img=tk_img):
                        av_lbl.config(image=img)
                        av_lbl.image = img
                    body.after(0, _upd)
                except Exception:
                    pass

        threading.Thread(target=_fetch_user_profile, daemon=True).start()

        # posts
        Frame(body, bg="#2A2A2A", height=1).pack(fill=X, padx=20)
        Label(body, text="Posts", font="Plus_Jakarta_Sans 11 bold",
              bg=BG, fg="#f5f5f5", anchor="w", padx=20).pack(fill=X, pady=(12, 6))

        loading_lbl = Label(body, text="Loading…",
                            font="Plus_Jakarta_Sans 9 italic",
                            bg=BG, fg="#555555", anchor="w", padx=20)
        loading_lbl.pack(fill=X)

        def _load_posts():
            posts = self.community.fetch_user_posts(username)
            body.after(0, lambda: _render(posts))

        def _render(posts):
            loading_lbl.destroy()
            if not posts:
                Label(body, text="No posts yet.",
                      font="Plus_Jakarta_Sans 9 italic", bg=BG, fg="#555555",
                      anchor="w", padx=20).pack(fill=X, pady=(0, 20))
                return
            gallery = Frame(body, bg=BG, padx=10)
            gallery.pack(fill=X, pady=(0, 20))

            def _loader(post):
                with urllib.request.urlopen(post["image_url"], timeout=8) as r:
                    data = r.read()
                return Image.open(io.BytesIO(data)).convert("RGB")

            def _meta(post):
                t = post.get("title", "untitled")
                if len(t) > 22: t = t[:20] + "…"
                desc = post.get("description", "") or ""
                if len(desc) > 28: desc = desc[:26] + "…"
                return [
                    (t,    "Plus_Jakarta_Sans 8 bold",   "#f5f5f5"),
                    (desc, "Plus_Jakarta_Sans 7 italic", "#666666"),
                ]

            self._make_responsive_gallery(
                canvas, gallery, posts, _loader, _meta,
                lambda post, lbl: self._open_post_view(post, lbl),
                cache_key=lambda post: post.get("image_url", "")
            )

        threading.Thread(target=_load_posts, daemon=True).start()

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._show_community_panel)
