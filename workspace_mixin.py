from tkinter import *
from PIL import Image, ImageTk
import os
import json
import threading


class WorkspaceMixin:

    _WS_DIR  = "workspace"
    _WS_INDEX = "workspace/index.json"

    def _ws_load_index(self) -> list:
        if not os.path.exists(self._WS_INDEX):
            return []
        try:
            with open(self._WS_INDEX) as f:
                return json.load(f)
        except Exception:
            return []

    def _ws_save_index(self, entries: list):
        os.makedirs(self._WS_DIR, exist_ok=True)
        with open(self._WS_INDEX, "w") as f:
            json.dump(entries, f, indent=2)

    def _save_to_workspace(self):
        if self.current_image is None:
            self._log("No image to save to workspace.", "err")
            return
        os.makedirs(self._WS_DIR, exist_ok=True)
        from datetime import datetime
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{ts}.jpg"
        path = os.path.join(self._WS_DIR, filename)
        self.current_image.convert("RGB").save(path, format="JPEG", quality=92)
        entries = self._ws_load_index()
        entries.insert(0, {
            "filename": filename,
            "date":     datetime.now().strftime("%d %b %Y  %H:%M"),
            "effects":  ", ".join(self._applied_effects) or "—"
        })
        self._ws_save_index(entries)
        self._log(f"Saved to workspace ({filename}).", "ok")

    def _open_workspace(self):
        self._nav_stack.clear()
        self._current_view = lambda: self._open_workspace()
        for w in self.wiki_view.winfo_children():
            w.destroy()

        BG = "#1A1A1A"

        # banner
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        Label(hdr, text="Workspace", font="Plus_Jakarta_Sans 14 bold",
              bg="#221F3A", fg="#f5f5f5", padx=14, pady=8).pack(side=LEFT)
        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)

        entries = self._ws_load_index()

        # scrollable grid
        outer = Frame(self.wiki_view, bg=BG)
        outer.pack(fill=BOTH, expand=1)
        canvas = Canvas(outer, bg=BG, highlightthickness=0, bd=0)
        sb = Scrollbar(outer, orient=VERTICAL, command=canvas.yview,
                       bg="#1a1a1a", troughcolor="#0a0a0a",
                       activebackground="#333333", relief="flat", borderwidth=0, width=10)
        canvas.configure(yscrollcommand=sb.set)
        sb.pack(side=RIGHT, fill=Y)
        canvas.pack(side=LEFT, fill=BOTH, expand=1)
        gallery = Frame(canvas, bg=BG, padx=10, pady=10)
        _cw = canvas.create_window((0, 0), window=gallery, anchor="nw")
        canvas.bind("<Configure>", lambda e: canvas.itemconfig(_cw, width=e.width))
        def _update_scrollregion(_=None):
            canvas.update_idletasks()
            bb = canvas.bbox("all")
            if bb:
                canvas.configure(scrollregion=(0, 0, bb[2], bb[3]))
                canvas.yview_moveto(0)
        gallery.bind("<Configure>", _update_scrollregion)
        self._bind_scroll(canvas)

        if not entries:
            Label(gallery, text="No saved images yet.\nUse  to save the current image.",
                  font="Plus_Jakarta_Sans 10 italic", bg=BG, fg="#555555",
                  justify=CENTER).pack(pady=40)
            self._wiki_open = True
            self.wiki_view.lift()
            self.root.bind("<Escape>", self._close_wiki_inline)
            return

        valid = [e for e in entries
                 if os.path.exists(os.path.join(self._WS_DIR, e["filename"]))]

        def _loader(entry):
            return Image.open(os.path.join(self._WS_DIR, entry["filename"])).convert("RGB")

        def _meta(entry):
            fx = entry.get("effects", "—")
            if len(fx) > 22: fx = fx[:20] + "…"
            return [
                (entry.get("date", ""), "Plus_Jakarta_Sans 7",        "#888888"),
                (fx,                   "Plus_Jakarta_Sans 7 italic",  "#666666"),
            ]

        self._make_responsive_gallery(
            canvas, gallery, valid, _loader, _meta,
            lambda entry, lbl: self._open_workspace_item(entry, lbl),
            cache_key=lambda entry: entry.get("filename", "")
        )

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._close_wiki_inline)

    def _open_workspace_item(self, entry: dict, img_lbl):
        for w in self.wiki_view.winfo_children():
            w.destroy()

        BG = "#1A1A1A"

        # banner with back + upload button on right
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        self._btn(hdr, "← Back", self._nav_back,
                  bg="#221F3A", fg="#888888",
                  font="Plus_Jakarta_Sans 9", padx=12, pady=8).pack(side=LEFT)
        self._btn(hdr, "↑  Upload", self._upload_post,
                  bg="#221F3A", fg="#f5f5f5",
                  font="Plus_Jakarta_Sans 9 bold", padx=12, pady=8).pack(side=RIGHT)
        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)

        Label(self.wiki_view,
              text=f"{entry.get('date', '')}   ·   {entry.get('effects', '—')}",
              font="Plus_Jakarta_Sans 8 italic", bg="#2A2A2A", fg="#888888",
              anchor="w", padx=12, pady=4).pack(fill=X)
        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)

        # image
        img_container = Label(self.wiki_view, bg=BG, text="loading…",
                               fg="#444444", font="Plus_Jakarta_Sans 10")
        img_container.pack(pady=20, padx=20)

        def _load():
            try:
                p = getattr(img_lbl, "_path", None) or os.path.join(
                    self._WS_DIR, entry["filename"])
                img = Image.open(p).convert("RGB")
                img.thumbnail((680, 680))
                tk_img = ImageTk.PhotoImage(img)
                img_container.config(image=tk_img, text="")
                img_container.image = tk_img
                # make it available for _upload_post
                self.current_image = Image.open(p).convert("RGBA")
                self._applied_effects = [e.strip() for e in
                                         entry.get("effects", "").split(",") if e.strip()]
            except Exception as e:
                img_container.config(text=f"Failed: {e}", fg="#E06C75")

        threading.Thread(target=_load, daemon=True).start()

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._open_workspace)
