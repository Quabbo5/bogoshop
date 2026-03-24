from tkinter import *
from PIL import Image, ImageTk
import re
import os


class WikiMixin:

    _REF_RE = re.compile(r'<([^>]+)>')

    def _insert_inline(self, widget, text, tag):
        """Insert text with **bold** and *italic* inline, no trailing newline."""
        parts = text.split("**")
        for i, part in enumerate(parts):
            if i % 2 == 1:
                widget.insert(END, part, "bold")
            else:
                for j, ipart in enumerate(part.split("*")):
                    widget.insert(END, ipart, "italic" if j % 2 == 1 else tag)

    def _insert_with_bold_italic(self, widget, line, tag):
        self._insert_inline(widget, line, tag)
        widget.insert(END, "\n")

    def _render_markdown(self, widget, text):
        widget.config(state=NORMAL)
        widget.delete("1.0", END)

        widget.tag_config("h1",     font="Plus_Jakarta_Sans 18 bold", foreground="#f5f5f5")
        widget.tag_config("h2",     font="Plus_Jakarta_Sans 14 bold", foreground="#c0c0c0")
        widget.tag_config("bold",   font="Plus_Jakarta_Sans 11 bold", foreground="#B5AFED")
        widget.tag_config("italic", font="Plus_Jakarta_Sans 11 italic", foreground="#6C65AA")
        widget.tag_config("normal", font="Plus_Jakarta_Sans 11",      foreground="#f5f5f5")
        widget.tag_config("code",   font=("Source Code Pro", 11),     foreground="#dbe9ff")

        _link_idx = [0]

        def _insert_line(raw_line, base_tag):
            """Insert a line, turning <ref.md> and <123456789> into clickable links."""
            segments = self._REF_RE.split(raw_line)
            for k, seg in enumerate(segments):
                if k % 2 == 1:          # matched group — a reference
                    ref = seg
                    tname = f"_ref_{_link_idx[0]}"; _link_idx[0] += 1
                    widget.tag_config(tname, foreground="#7EC8A4", underline=True,
                                      font="Plus_Jakarta_Sans 11")
                    widget.insert(END, f"<{ref}>", tname)
                    if ref.endswith(".md"):
                        widget.tag_bind(tname, "<Button-1>",
                                        lambda _, r=ref: self._open_wiki_by_file(r))
                    elif ref.isdigit():
                        widget.tag_bind(tname, "<Button-1>",
                                        lambda _, r=ref: self._open_post_by_short_id(r))
                    widget.tag_bind(tname, "<Enter>", lambda _: widget.config(cursor="hand2"))
                    widget.tag_bind(tname, "<Leave>", lambda _: widget.config(cursor=""))
                else:
                    self._insert_inline(widget, seg, base_tag)
            widget.insert(END, "\n")

        in_code_block = False
        for line in text.split("\n"):
            if line.startswith("```"):
                in_code_block = not in_code_block
                continue
            if in_code_block:
                widget.insert(END, line + "\n", "code")
                continue
            if line.startswith("# "):
                _insert_line(line[2:], "h1")
            elif line.startswith("## "):
                _insert_line(line[3:], "h2")
            elif line.startswith("- "):
                _insert_line("• " + line[2:], "normal")
            else:
                _insert_line(line, "normal")

        widget.config(state=DISABLED)

    def _inline_text(self, parent, content: str, bg: str, fg: str = "#dddddd",
                     font: str = "Plus_Jakarta_Sans 10"):
        """Read-only Text widget that renders <ref> and @user links clickable inline."""
        t = Text(parent, bg=bg, fg=fg, font=font, relief=FLAT, bd=0,
                 highlightthickness=0, wrap=WORD, cursor="arrow")
        t.tag_config("normal", foreground=fg, font=font)
        _idx = [0]

        # split on both @user and <ref> patterns
        _COMBINED = re.compile(r'(@\w+)|<([^>]+)>')
        pos = 0
        for m in _COMBINED.finditer(content):
            # plain text before match
            if m.start() > pos:
                t.insert(END, content[pos:m.start()], "normal")
            tname = f"_ir_{_idx[0]}"; _idx[0] += 1
            if m.group(1):                          # @username
                uname = m.group(1)[1:]
                t.tag_config(tname, foreground="#9A9AFF", font=font)
                t.insert(END, m.group(1), tname)
                t.tag_bind(tname, "<Button-1>", lambda _, u=uname: self._open_user_profile(u))
            else:                                   # <ref>
                ref = m.group(2)
                t.tag_config(tname, foreground="#7EC8A4", underline=True, font=font)
                t.insert(END, m.group(0), tname)
                if ref.endswith(".md"):
                    t.tag_bind(tname, "<Button-1>", lambda _, r=ref: self._open_wiki_by_file(r))
                elif ref.isdigit():
                    t.tag_bind(tname, "<Button-1>", lambda _, r=ref: self._open_post_by_short_id(r))
            t.tag_bind(tname, "<Enter>", lambda _: t.config(cursor="hand2"))
            t.tag_bind(tname, "<Leave>", lambda _: t.config(cursor="arrow"))
            pos = m.end()
        if pos < len(content):
            t.insert(END, content[pos:], "normal")

        t.config(state=DISABLED)
        t.update_idletasks()
        lines = max(int(t.index(END).split(".")[0]) - 1, 1)
        t.config(height=lines)
        return t

    def _show_wiki_inline(self, index):
        effect = self.EFFECTS[index]
        file_path = effect.get("preview", "img/jpegs/cat.jpg")
        self._nav_push()
        self._current_view = lambda: self._show_wiki_inline(index)
        for widget in self.wiki_view.winfo_children():
            widget.destroy()

        # banner with back button
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        self._btn(hdr, "← Back", self._nav_back,
                  bg="#221F3A", fg="#888888",
                  font="Plus_Jakarta_Sans 9", padx=12, pady=8).pack(side=LEFT)
        Label(hdr, text=effect["name"], fg="#f5f5f5", bg="#221F3A",
              font="Plus_Jakarta_Sans 14 bold", padx=8).pack(side=LEFT)

        Label(self.wiki_view, justify=LEFT, padx=0, pady=0, anchor=None,
              text=f"ID: #{effect['id']}", fg="#c0c0c0", bg="#424242",
              font="Helvetica 13 bold", height=1
             ).pack(side=TOP, anchor="nw", fill=X)

        Label(self.wiki_view, justify=LEFT, padx=0, pady=0, anchor=None,
              text=f"Author: {effect['author']}",
              fg="#808080", bg="#424242", font="Helvetica 9 italic", height=1
             ).pack(side=TOP, anchor="nw", fill=X)

        # before/after images — before left, after right, padding pulls them inward
        img_before = Image.open(file_path).convert("RGBA").resize((280, 175))
        img_before_tk = ImageTk.PhotoImage(img_before)

        original = self.current_image.copy() if self.current_image else None
        self.current_image = Image.open(file_path).convert("RGBA")
        effect["fn"]()
        img_after = self.current_image.copy()
        self.current_image = original
        img_after_tk = ImageTk.PhotoImage(img_after.resize((280, 175)))

        image_frame = Frame(self.wiki_view, bg="#1E1E1E")
        image_frame.pack(fill=X, pady=(8, 4))

        before_frame = Frame(image_frame, bg="#1E1E1E")
        before_frame.pack(side=LEFT, padx=(100, 8), pady=20)
        before_lbl = Label(before_frame, image=img_before_tk, bg="#1E1E1E")
        before_lbl.image = img_before_tk
        before_lbl.pack()
        Label(before_frame, text="Before", fg="#606060", bg="#1E1E1E", font="Plus_Jakarta_Sans 9 italic").pack()

        after_frame = Frame(image_frame, bg="#1E1E1E")
        after_frame.pack(side=RIGHT, padx=(8, 100), pady=20)
        after_lbl = Label(after_frame, image=img_after_tk, bg="#1E1E1E")
        after_lbl.image = img_after_tk
        after_lbl.pack()
        Label(after_frame, text="After", fg="#606060", bg="#1E1E1E", font="Plus_Jakarta_Sans 9 italic").pack()

        # markdown
        with open(f"wiki/{effect['id']}.md", "r") as f:
            raw_text = f.read()

        wiki_text = Text(self.wiki_view, bg="#2D2D2D", relief=FLAT, padx=12, pady=10)
        wiki_text.pack(fill=BOTH, expand=1)
        self._render_markdown(wiki_text, raw_text)

        self._wiki_open = True
        self.wiki_view.lift()

        self.root.bind("<Escape>", self._close_wiki_inline)
        self._log(f"Wiki: {effect['name']} — press Esc to close.", "info")

    def _show_search_inline(self, query):
        q = query.lower()
        def _matches(e):
            if q in e["name"].lower(): return True
            if q in e.get("desc", "").lower(): return True
            if any(q in tag.lower() for tag in e.get("tags", [])): return True
            return False
        matches = [e for e in self.EFFECTS if _matches(e)]

        for widget in self.wiki_view.winfo_children():
            widget.destroy()

        # title
        Label(self.wiki_view, justify=LEFT, padx=0, pady=0, anchor=None,
              text=f'Search: "{query}"', fg="#f5f5f5", bg="#221F3A",
              font="Helvetica 24 bold", height=2
             ).pack(side=TOP, anchor="nw", fill=X)

        Label(self.wiki_view, justify=LEFT, padx=0, pady=0, anchor=None,
              text=f"{len(matches)} result(s)", fg="#c0c0c0", bg="#424242",
              font="Helvetica 13 bold", height=1
             ).pack(side=TOP, anchor="nw", fill=X)

        Label(self.wiki_view, justify=LEFT, padx=0, pady=0, anchor=None,
              text="Search for more keywords for a better result.", fg="#c0c0c0", bg="#424242",
              font="Helvetica 8 italic", height=1
             ).pack(side=TOP, anchor="nw", fill=X)

        # results list
        results_frame = Frame(self.wiki_view, bg="#2D2D2D")
        results_frame.pack(fill=BOTH, expand=1, padx=0, pady=0)

        if not matches:
            Label(results_frame,
                  text="No effects found.",
                  fg="#E06C75", bg="#2D2D2D",
                  font="Helvetica 12 italic",
                  anchor="w", padx=20, pady=12
                 ).pack(fill=X)
        else:
            for e in matches:
                idx = self.EFFECTS.index(e)

                row = Frame(results_frame, bg="#2D2D2D", cursor="hand2")
                row.pack(fill=X, padx=20, pady=(10, 0))

                id_lbl   = Label(row, text=f"#{e['id']}", fg="#666666", bg="#2D2D2D",
                                 font="Helvetica 11", width=3, anchor="w")
                id_lbl.pack(side=LEFT)

                name_lbl = Label(row, text=e["name"], fg="#f5f5f5", bg="#2D2D2D",
                                 font="Helvetica 13 bold", anchor="w")
                name_lbl.pack(side=LEFT, padx=(6, 0))

                auth_lbl = Label(row, text=f"by {e['author']}", fg="#808080", bg="#2D2D2D",
                                 font="Helvetica 9 italic", anchor="w", padx=8)
                auth_lbl.pack(side=LEFT)

                for widget in (row, id_lbl, name_lbl, auth_lbl):
                    widget.bind("<Button-1>", lambda _, i=idx: self._show_wiki_inline(i))

                sep = Frame(results_frame, bg="#333333", height=1)
                sep.pack(fill=X, padx=20, pady=(8, 0))

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._close_wiki_inline)
        self._log(f"Search: '{query}' — {len(matches)} result(s). Press Esc to close.", "info")

    def _open_base_wiki(self):
        self._nav_push()
        self._current_view = lambda: self._open_base_wiki()
        for widget in self.wiki_view.winfo_children():
            widget.destroy()

        Label(self.wiki_view, justify=LEFT, padx=0, pady=0, anchor=None,
              text="Bogoshop", fg="#f5f5f5", bg="#221F3A",
              font="Helvetica 24 bold", height=2
             ).pack(side=TOP, anchor="nw", fill=X)

        Label(self.wiki_view, justify=LEFT, padx=0, pady=0, anchor=None,
              text="General Documentation", fg="#c0c0c0", bg="#424242",
              font="Helvetica 13 bold", height=1
             ).pack(side=TOP, anchor="nw", fill=X)

        with open("wiki/!base.md", "r") as f:
            raw_text = f.read()

        wiki_text = Text(self.wiki_view, bg="#2D2D2D", relief=FLAT, padx=12, pady=10)
        wiki_text.pack(fill=BOTH, expand=1)
        self._render_markdown(wiki_text, raw_text)

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._close_wiki_inline)
        self._log("Opened base wiki. Press Esc to close.", "info")

    def _open_wiki_by_file(self, filename: str):
        # check if it matches an effect wiki (e.g. "3.md", "03.md", "0003.md")
        stem = filename.replace(".md", "")
        for i, effect in enumerate(self.EFFECTS):
            eid = str(effect["id"])
            if eid == stem or eid == (stem.lstrip("0") or "0") or eid.zfill(4) == stem:
                self._show_wiki_inline(i)
                return

        path = os.path.join("wiki", filename)
        if not os.path.exists(path):
            self._log(f"Wiki page not found: {filename}", "err")
            return
        self._nav_push()
        self._current_view = lambda: self._open_wiki_by_file(filename)
        for w in self.wiki_view.winfo_children():
            w.destroy()
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        self._btn(hdr, "← Back", self._nav_back,
                  bg="#221F3A", fg="#888888",
                  font="Plus_Jakarta_Sans 9", padx=12, pady=8).pack(side=LEFT)
        Label(hdr, text=filename.replace(".md", "").replace("_", " "),
              fg="#f5f5f5", bg="#221F3A", font="Plus_Jakarta_Sans 14 bold",
              padx=8).pack(side=LEFT)
        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)
        with open(path, "r") as f:
            raw = f.read()
        wiki_text = Text(self.wiki_view, bg="#2D2D2D", relief=FLAT, padx=12, pady=10)
        wiki_text.pack(fill=BOTH, expand=1)
        self._render_markdown(wiki_text, raw)
        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._close_wiki_inline)

    def _open_post_by_short_id(self, short_id: str):
        self._log(f"Looking up post <{short_id}>…", "info")
        import threading
        def _fetch():
            post = self.community.fetch_post_by_short_id(short_id)
            if post:
                self.root.after(0, lambda: self._open_post_view(post, None))
            else:
                self.root.after(0, lambda: self._log(f"No post found with ID <{short_id}>.", "err"))
        threading.Thread(target=_fetch, daemon=True).start()

    def _close_wiki_inline(self, event=None):
        if self.current_image is None:
            self._log("Load an image first (folder icon or /r command).", "err")
            return
        self._wiki_open = False
        self.right_panel.lift()
        self.root.unbind("<Escape>")
