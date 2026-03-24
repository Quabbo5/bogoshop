from tkinter import *
from tkinter import ttk
from tkinter import colorchooser, filedialog
from PIL import Image, ImageTk, ImageDraw
import numpy as np
import ctypes
from PIL import ImageFilter
import colorsys
import cv2
import io
import os
import json
import xml.etree.ElementTree as ET
import re
from community import CommunityClient

class App:
    def __init__(self):

        self.root = Tk()
        self.root.title("PhotoPhile")
        self.root.geometry("1054x1020")
        self.root.minsize(1054, 1020)
        #self.root.state("zoomed")
        self.root.config(bg="black")

        # dunkle Titelleiste (Windows 10/11)
        # self.root.update()
        # hwnd = ctypes.windll.user32.GetParent(self.root.winfo_id())
        # ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, 20, ctypes.byref(ctypes.c_int(1)), ctypes.sizeof(ctypes.c_int))

        # build menu
        self.menu_bar = Menu(self.root)
        self.root.config(menu=self.menu_bar)

        self.file_menu = Menu(self.menu_bar)
        self.help_menu = Menu(self.menu_bar)
        self.menu_bar.add_cascade(label="File", menu=self.file_menu)
        self.menu_bar.add_cascade(label="Help", menu=self.help_menu)

        self.file_menu.add_command(label="Open")
        self.file_menu.add_command(label="Save")
        self.file_menu.add_command(label="Save As")
        self.file_menu.add_separator()

        self.help_menu.add_command(label="Open Documentation")

        self.current_image = None
        self._original_image = None  # last freshly-loaded image (pre-effects)
        self._nav_stack   = []       # browser-style back history
        self._current_view = None    # callable that rebuilds the current panel

        # build command box (packed first so it anchors to bottom)
        self.cmd_box = Frame(self.root, bg="#141414", height=175)
        self.cmd_box.pack(side=BOTTOM, fill=X)
        self.cmd_box.pack_propagate(False)

        self.cmd_top_border = Frame(self.cmd_box, bg="#444444", height=1)
        self.cmd_top_border.pack(side=TOP, fill=X)

        self.cmd_entry_frame = Frame(self.cmd_box, bg="#1E1E1E", height=32)
        self.cmd_entry_frame.pack(side=BOTTOM, fill=X)
        self.cmd_entry_frame.pack_propagate(False)

        self.cmd_entry_border = Frame(self.cmd_box, bg="#333333", height=1)
        self.cmd_entry_border.pack(side=BOTTOM, fill=X)

        self._icon_nav = self._load_svg_icon("img/icons/nav-arrow-right.svg", display=16)
        self.cmd_prompt_label = Label(self.cmd_entry_frame, image=self._icon_nav, bg="#1E1E1E")
        self.cmd_prompt_label.pack(side=LEFT, padx=(10, 4), pady=4)

        self.cmd_entry = Entry(self.cmd_entry_frame, bg="#1E1E1E", fg="#f5f5f5", insertbackground="#f5f5f5", relief=FLAT, font="Plus_Jakarta_Sans 10", highlightthickness=0, highlightbackground="#1E1E1E", highlightcolor="#1E1E1E")
        self.cmd_entry.pack(side=LEFT, fill=X, expand=1, padx=(0, 10))

        self.log_text = Text(self.cmd_box, bg="#141414", fg="#888888", font=("Source Code Pro", 9), relief=FLAT, state=DISABLED, padx=10, pady=0, cursor="arrow", highlightthickness=0)
        self.log_text.pack(fill=BOTH, expand=1, pady=(6, 0))
        self.log_text.tag_config("info", foreground="#888888")
        self.log_text.tag_config("ok", foreground="#7EC8A4")
        self.log_text.tag_config("err", foreground="#E06C75")
        self.log_text.tag_config("prompt", foreground="#555555")

        # build toolbar
        self.toolbar = Frame(self.root, bg="#1A1A1A", height=36)
        self.toolbar.pack(side=TOP, fill=X)
        self.toolbar.pack_propagate(False)
        Frame(self.toolbar, bg="#333333", height=1).pack(side=BOTTOM, fill=X)

        self._icon_import    = self._load_svg_icon("img/icons/folder-plus.svg")
        self._icon_save      = self._load_svg_icon("img/icons/floppy-disk.svg")
        self._icon_ws_save   = self._load_svg_icon("img/icons/floppy-disk-arrow-in.svg")
        self._icon_help      = self._load_svg_icon("img/icons/help-circle.svg")
        self._icon_random    = self._load_svg_icon("img/icons/random.svg")
        self._icon_community = self._load_svg_icon("img/icons/community.svg")
        self._icon_workspace = self._load_svg_icon("img/icons/book-stack.svg")
        self._icon_profile   = self._load_svg_icon("img/icons/leaderboard-star.svg")

        _btn = dict(bg="#1A1A1A", activebackground="#2A2A2A", relief=FLAT, bd=0, cursor="hand2", padx=6, pady=4)
        Button(self.toolbar, image=self._icon_import,    command=self._import_image,       **_btn).pack(side=LEFT)
        Button(self.toolbar, image=self._icon_random,    command=self._import_random,      **_btn).pack(side=LEFT)
        Button(self.toolbar, image=self._icon_save,      command=self._save_image,         **_btn).pack(side=LEFT)
        Button(self.toolbar, image=self._icon_ws_save,   command=self._save_to_workspace,  **_btn).pack(side=LEFT)
        Button(self.toolbar, image=self._icon_help,      command=self._open_base_wiki,     **_btn).pack(side=LEFT)
        Button(self.toolbar, image=self._icon_community, command=self._open_community,     **_btn).pack(side=LEFT)
        Button(self.toolbar, image=self._icon_workspace, command=self._open_workspace,     **_btn).pack(side=LEFT)
        Button(self.toolbar, image=self._icon_profile,   command=self._open_profile,       **_btn).pack(side=LEFT)

        self.community = CommunityClient()
        self._applied_effects: list[str] = []
        self._img_cache: dict[str, Image.Image] = {}  # url/path → PIL Image

        # build main container
        self.main = PanedWindow(self.root, orient=HORIZONTAL, bg="#2D2D2D")
        self.main.pack(fill=BOTH, expand=1)

        # build left panel
        self.left_border = Frame(self.main, highlightbackground="#444444", highlightthickness=1, bg="#2D2D2D")
        self.main.add(self.left_border)
        self.left_panel = PanedWindow(self.left_border, orient=VERTICAL, bg="#2D2D2D")
        self.left_panel.pack(fill=BOTH, expand=1)

        self.list_title = Label(self.left_panel, text="Select Effect:", font="Plus_Jakarta_Sans 8 bold", anchor="nw", width=20, bg="#2D2D2D", fg="#f5f5f5")
        self.left_panel.add(self.list_title)

        # build right panel
        self.right_border = Frame(self.main, highlightbackground="#444444", highlightthickness=1, bg="#2D2D2D")
        self.main.add(self.right_border)
        self.right_border.grid_rowconfigure(0, weight=1)
        self.right_border.grid_columnconfigure(0, weight=1)

        self.right_panel = Frame(self.right_border, bg="#2D2D2D")
        self.right_panel.grid(row=0, column=0, sticky="nsew")

        # wiki inline view — lives in same grid cell, lifted on top when active
        self.wiki_view = Frame(self.right_border, bg="#2D2D2D")
        self.wiki_view.grid(row=0, column=0, sticky="nsew")
        self._wiki_open = False

        Label(self.toolbar, text="PhotoPhile v.1.2", font="Plus_Jakarta_Sans 8 bold",
              bg="#1A1A1A", fg="#666666"
             ).place(relx=0.5, rely=0.5, anchor="center")

        # build progressbar (bottom of right panel)
        self.progress_bar_frame = Frame(self.right_panel, bg="#2D2D2D", height=6)
        self.progress_bar_frame.pack(side=BOTTOM, fill=X)

        self.progress_bar = ttk.Progressbar(self.progress_bar_frame, orient=HORIZONTAL, mode="determinate", style="dark.Horizontal.TProgressbar")
        self.progress_bar.pack(fill=X)

        # image label fills remaining space
        self.image_label = Label(self.right_panel, bg="#1B1B1B")
        self.image_label.pack(fill=BOTH, expand=1)

        # build effect list
        self.effect_list = Listbox(self.left_panel, font="Plus_Jakarta_Sans 10", bg="#2D2D2D", fg="#f5f5f5", highlightthickness=0, bd=0)
        self.left_panel.add(self.effect_list)

        # build effect panel
        #self.example_effect = Label(self.effect_panel, text="This is my example text")
        #self.effect_panel.add(self.example_effect)

        #self.scale_test = Scale(self.effect_panel, orient=HORIZONTAL)
        #self.effect_panel.add(self.scale_test)

        self.EFFECTS = [
            {"id": "1", "name": "Posterize 1bit",   "fn": self._posterize_1bit,   "author": "rango", "desc": "posterizes the given image to 1 bit",  "tags": ["black_white", "posterize"], "preview": "img/jpegs/geometry.jpg"},
            {"id": "2", "name": "Negative",         "fn": self._negative,         "author": "rango", "desc": "inverts all colors",                   "tags": ["invert", "color"],           "preview": "img/jpegs/space.jpg"},
            {"id": "3", "name": "Glow",             "fn": self._glow,             "author": "rango", "desc": "adds a soft glow via gaussian blur",   "tags": ["blur", "glow", "light"],     "preview": "img/jpegs/flowers.jpg", "params": [{"name": "radius", "default": 14}]},
            {"id": "4", "name": "Color Grain",      "fn": self._color_grain,      "author": "rango", "desc": "adds colorful noise grain",            "tags": ["grain", "noise", "color"],   "preview": "img/jpegs/desert.jpg"},
            {"id": "5", "name": "RedThreshold",     "fn": self._duotone_threshold,"author": "rango", "desc": "thresholds bright pixels to red",      "tags": ["threshold", "color", "red"], "preview": "img/jpegs/cat.jpg"},
            {"id": "6", "name": "Posterize 3bit",   "fn": self._posterize_3bit,   "author": "rango", "desc": "posterizes the image to 8 luma steps", "tags": ["posterize", "black_white"],  "preview": "img/jpegs/tree.jpg"},
            {"id": "7", "name": "Hue shift",        "fn": self._hue_shift,        "author": "rango", "desc": "rotates the hue of all pixels",        "tags": ["hue", "color"],              "preview": "img/jpegs/wave.jpg",    "params": [{"name": "amount", "default": 22}]},
            {"id": "8", "name": "Brightness Up",    "fn": self._brightness_up,    "author": "rango", "desc": "increases brightness by 10",           "tags": ["brightness", "light"],       "preview": "img/jpegs/car.jpg"},
            {"id": "9", "name": "Brightness Down",  "fn": self._brightness_down,  "author": "rango", "desc": "decreases brightness by 10",           "tags": ["brightness", "dark"],        "preview": "img/jpegs/dice.jpg"},
        ]

        for e in self.EFFECTS:
            self.effect_list.insert(END, f"{e['id']} – {e['name']}")

        # keybinds
        self.effect_list.bind("<Double-Button-1>", self._on_click)
        self.effect_list.bind("<Return>", self._on_click)
        self.effect_list.bind("<h>", self._on_h_press)
        self.cmd_entry.bind("<Return>", self._on_enter)

        self._log("Core ready. Type an effect ID or name and press Enter.", "info")

        self.root.after(100, self._open_base_wiki)
        self.root.mainloop()

    # --- event handlers ---

    def _on_click(self, event):
        selection = self.effect_list.curselection()
        if selection:
            index = selection[0]
            self._apply_effect(index)
    
    def _on_enter(self, event):
        value = self.cmd_entry.get().strip()
        # self.cmd_entry.delete(0, END) # LEAVE THIS LIKE THIS

        if not value:
            return

        if value.startswith("/"):
            self._handle_command(value)
            return

        for i, effect in enumerate(self.EFFECTS):
            if effect["id"] == value or effect["name"].lower() == value.lower():
                self._apply_effect(i)
                return

        self._log(f"Unknown effect: '{value}'", "err")

    def _handle_command(self, value):
        parts = value.split()
        cmd = parts[0]
        args = parts[1:]

        if cmd == "/r":
            if self._original_image is None:
                self._log("No image loaded yet.", "err")
                return
            self.current_image = self._original_image.copy()
            self._applied_effects.clear()
            preview = self.current_image.resize((800, 800))
            tk_img = ImageTk.PhotoImage(preview)
            self.image_label.config(image=tk_img)
            self.image_label.image = tk_img
            self._log("Reset to original.", "ok")

        elif cmd == "/clear":
            self.log_text.config(state=NORMAL)
            self.log_text.delete("1.0", END)
            self.log_text.config(state=DISABLED)
        
        elif cmd == "/help":
            if not args:
                self._log("Usage: /help <effect id or name>", "err")
                return
            query = " ".join(args)
            for i, effect in enumerate(self.EFFECTS):
                if effect["id"] == query or effect["name"].lower() == query.lower():
                    self._show_wiki_inline(i)
                    return
            self._log(f"No effect found for '{query}'", "err")
        elif cmd == "/search":
            if not args:
                self._log("Usage: /search <query>", "err")
                return
            query = " ".join(args)
            self._show_search_inline(query)

        elif cmd == "/window" and args == ["size"]:
            w = self.root.winfo_width()
            h = self.root.winfo_height()
            print(f"Window size: {w}x{h}")
            self._log(f"Window size: {w}x{h}", "info")

        elif cmd[1:].isdigit():
            effect_id = cmd[1:]
            for i, effect in enumerate(self.EFFECTS):
                if effect["id"] == effect_id:
                    params = effect.get("params", [])
                    if len(args) > len(params):
                        self._log(f"Too many arguments. '{effect['name']}' takes {len(params)} param(s).", "err")
                        return
                    kwargs = {}
                    for j, p in enumerate(params):
                        if j < len(args):
                            try:
                                kwargs[p["name"]] = type(p["default"])(args[j])
                            except ValueError:
                                self._log(f"Invalid value for '{p['name']}' — expected {type(p['default']).__name__}.", "err")
                                return
                    self._apply_effect(i, **kwargs)
                    return
            self._log(f"No effect with ID {effect_id}.", "err")

        else:
            self._log(f"Unknown command: '{value}'", "err")

    def _on_h_press(self, event):
        selection = self.effect_list.curselection()
        if selection:
            index = selection[0]
            self._show_wiki_inline(index)

    # --- responsive gallery ---

    def _make_responsive_gallery(self, canvas, frame, items,
                                  image_loader,        # fn(item) -> PIL Image, runs in thread
                                  meta_rows,           # fn(item) -> list of (text, font, fg)
                                  on_click,            # fn(item, img_lbl)
                                  cache_key=None):     # fn(item) -> str, enables image cache
        """4-column gallery that reflows images when the canvas width changes."""
        import threading
        MAX_COLS = 4
        PAD = 12  # px padding per cell (padx=6 each side)

        for c in range(MAX_COLS):
            frame.columnconfigure(c, weight=1)

        def _cell_sz():
            w = canvas.winfo_width()
            if w < 10:
                w = 680
            return max((w - PAD * MAX_COLS) // MAX_COLS, 60)

        # track every image label for resize
        _img_lbls = []

        def _set_image(lbl, pil_img, sz):
            resized = pil_img.resize((sz, sz), Image.LANCZOS)
            tk_img  = ImageTk.PhotoImage(resized)
            lbl.config(image=tk_img, text="", width=sz, height=sz)
            lbl.image = tk_img

        for idx, item in enumerate(items):
            r, c = idx // MAX_COLS, idx % MAX_COLS
            cell = Frame(frame, bg="#2D2D2D", padx=4, pady=4)
            cell.grid(row=r, column=c, padx=6, pady=6, sticky="nsew")

            sz = _cell_sz()
            img_lbl = Label(cell, text="…", bg="#2D2D2D", fg="#555555",
                            width=sz // 8, height=sz // 16)
            img_lbl.pack()
            img_lbl._pil = None  # will hold PIL Image once loaded

            for text, font, fg in meta_rows(item):
                Label(cell, text=text, font=font, bg="#2D2D2D", fg=fg,
                      wraplength=sz, anchor="center").pack(fill=X)

            def _load(it=item, lbl=img_lbl):
                try:
                    key = cache_key(it) if cache_key else None
                    if key and key in self._img_cache:
                        pil = self._img_cache[key]
                    else:
                        pil = image_loader(it)
                        if key:
                            self._img_cache[key] = pil
                    lbl._pil = pil
                    s = _cell_sz()
                    lbl.after(0, lambda l=lbl, p=pil, s=s: _set_image(l, p, s))
                except Exception:
                    lbl.after(0, lambda l=lbl: l.config(text="✗"))

            threading.Thread(target=_load, daemon=True).start()
            _img_lbls.append(img_lbl)

            def _click(_, it=item, lbl=img_lbl):
                on_click(it, lbl)
            cell.bind("<Button-1>", _click)
            img_lbl.bind("<Button-1>", _click)
            img_lbl.config(cursor="hand2")

        # debounced resize
        _last_w = [0]
        _after_id = [None]

        def _on_canvas_resize(e):
            if abs(e.width - _last_w[0]) < 4:
                return
            _last_w[0] = e.width
            if _after_id[0]:
                canvas.after_cancel(_after_id[0])
            def _apply():
                sz = _cell_sz()
                for lbl in _img_lbls:
                    if lbl._pil is not None:
                        _set_image(lbl, lbl._pil, sz)
            _after_id[0] = canvas.after(120, _apply)

        canvas.bind("<Configure>", _on_canvas_resize, add="+")

    # --- navigation history ---

    def _nav_push(self):
        """Push current view onto the back-stack before navigating away."""
        if self._current_view is not None:
            self._nav_stack.append(self._current_view)

    def _nav_back(self):
        """Go back to the previous panel, or community if history is empty."""
        if self._nav_stack:
            self._current_view = None       # prevent re-push inside the popped fn
            fn = self._nav_stack.pop()
            fn()
        else:
            self._show_community_panel()

    # --- label button (macOS-safe colored button) ---

    def _btn(self, parent, text, command, bg="#111111", fg="#f5f5f5",
             font="Plus_Jakarta_Sans 9 bold", padx=14, pady=7):
        """Label-based button that respects bg/fg on macOS."""
        lbl = Label(parent, text=text, bg=bg, fg=fg,
                    font=font, padx=padx, pady=pady, cursor="hand2")
        hover = "#2A2A4A" if bg == "#221F3A" else "#222222"
        lbl.bind("<Enter>",        lambda _: lbl.config(bg=hover))
        lbl.bind("<Leave>",        lambda _: lbl.config(bg=bg))
        lbl.bind("<ButtonPress-1>", lambda _: lbl.config(bg="#333333"))
        lbl.bind("<ButtonRelease-1>", lambda _: (lbl.config(bg=hover), command()))
        return lbl

    def _bind_scroll(self, canvas):
        """Attach trackpad + arrow key scrolling to a Canvas (macOS-safe)."""
        canvas.bind("<MouseWheel>", lambda e: canvas.yview_scroll(int(-e.delta), "units"))
        canvas.bind("<Up>",    lambda _: canvas.yview_scroll(-1, "units"))
        canvas.bind("<Down>",  lambda _: canvas.yview_scroll( 1, "units"))
        canvas.bind("<Prior>", lambda _: canvas.yview_scroll(-5, "units"))
        canvas.bind("<Next>",  lambda _: canvas.yview_scroll( 5, "units"))
        canvas.focus_set()

    # --- logging ---

    def _log(self, message, tag="info"):
        self.log_text.config(state=NORMAL)
        self.log_text.insert(END, f"  {message}\n", tag)
        self.log_text.see(END)
        self.log_text.config(state=DISABLED)

    # --- core ---

    def _resize(self, path):
        img = Image.open(path)
        _W, _H = img.size
        if _W == _H:
            return img
        elif _W > _H:
            cut = round((_W - _H) // 2)
            return img.crop((cut, 0, _W - cut, _H))
        else:
            cut = round((_H - _W) // 2)
            return img.crop((0, cut, _W, _H - cut))

    def _apply_effect(self, index, **kwargs):
        if self._wiki_open:
            self._log("Close the wiki first (Esc) before applying effects.", "err")
            return
        effect = self.EFFECTS[index]
        # fill in defaults for any param not provided
        for p in effect.get("params", []):
            kwargs.setdefault(p["name"], p["default"])
        self._log(f"[{effect['id']}] {effect['name']} – applying...", "prompt")
        effect["fn"](**kwargs)
        new_img = self.current_image.resize((800, 800))
        new_canvas = ImageTk.PhotoImage(new_img)
        self.image_label.config(image=new_canvas)
        self.image_label.image = new_canvas
        self._applied_effects.append(effect["name"])
        self._log(f"[{effect['id']}] {effect['name']} – done.", "ok")
    
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

    _REF_RE = re.compile(r'<([^>]+)>')

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

    def _close_wiki_inline(self, event=None):
        if self.current_image is None:
            self._log("Load an image first (folder icon or /r command).", "err")
            return
        self._wiki_open = False
        self.right_panel.lift()
        self.root.unbind("<Escape>")

    def _load_wiki(self, index):
        file_path = "img/pngs/waves.png"

        def _generate_preview():
            original = self.current_image.copy()
           
            self.current_image = Image.open(file_path).convert("RGBA")
            self.EFFECTS[index]["fn"]()
            preview = self.current_image.copy()
            self.current_image = original
            preview = preview.resize((350, 220))
            return ImageTk.PhotoImage(preview)

        wiki = Toplevel()
        wiki.geometry("800x900")
        wiki.minsize(800,900)
        wiki.title(f"Wiki for: {self.EFFECTS[index]['name']}")

        img_before = Image.open(file_path).convert("RGBA")
        img_before = img_before.resize((350, 220))
        img_before_resized = ImageTk.PhotoImage(img_before)

        img_after_resized = _generate_preview()

        Label(wiki, justify=LEFT, padx=0, pady=0, anchor=None,
                    text=self.EFFECTS[index]["name"], fg="#f5f5f5", bg="#221F3A",
                    font="Helvetica 24 bold", height=2
              ).pack(side=TOP, anchor="nw", fill=X)

        Label(wiki, justify=LEFT, padx=0, pady=0, anchor=None,
                    text=f"ID: #{self.EFFECTS[index]['id']}", fg="#c0c0c0", bg="#424242",
                    font="Helvetica 13 bold", height=1
              ).pack(side=TOP, anchor="nw", fill=X)

        Label(wiki, justify=LEFT, padx=0, pady=0, anchor=None,
                    text=f"Author: {self.EFFECTS[index]['author']}",
                    fg="#808080", bg="#424242", font="Helvetica 9 italic", height=1
              ).pack(side=TOP, anchor="nw", fill=X)

        image_frame = Frame(wiki, bg="#4D4D4D")
        image_frame.pack(fill=X)

        # Before
        before_frame = Frame(image_frame, bg="#4D4D4D")
        before_frame.pack(side=LEFT, padx=10, pady=10)

        before_label = Label(before_frame, image=img_before_resized)
        before_label.image = img_before_resized
        before_label.pack()

        Label(before_frame, text="Before", 
                fg="#808080", bg="#4D4D4D", 
                font="Helvetica 12 italic", height=1
             ).pack()

        # After
        after_frame = Frame(image_frame, bg="#4D4D4D")
        after_frame.pack(side=RIGHT, padx=10, pady=10)

        after_label = Label(after_frame, image=img_after_resized)
        after_label.image = img_after_resized
        after_label.pack()

        Label(after_frame, text="After", 
                fg="#808080", bg="#4D4D4D", 
                font="Helvetica 12 italic", height=1
             ).pack()
        
        with open(f"wiki/{self.EFFECTS[index]['id']}.md", "r") as f:
            raw_text = f.read()

        wiki_text = Text(wiki, bg="#2D2D2D", relief=FLAT, padx=10, pady=10)
        wiki_text.pack(fill=BOTH, expand=1)
        self._render_markdown(wiki_text, raw_text)

        wiki.mainloop()    

    # --- toolbar ---

    def _load_svg_icon(self, path, display=22):
        sc  = 6
        s   = display * sc
        lw  = round(sc * 1.4)
        fg  = (155, 155, 155)

        tree = ET.parse(path)
        root = tree.getroot()
        vb   = root.get("viewBox", "0 0 24 24").split()
        vw, vh = float(vb[2]), float(vb[3])
        sx, sy = s / vw, s / vh

        img = Image.new("RGB", (s, s), (26, 26, 26))
        d   = ImageDraw.Draw(img)

        for elem in root.iter():
            tag = elem.tag.split("}")[-1] if "}" in elem.tag else elem.tag
            if tag == "path":
                segs = self._parse_svg_path(elem.get("d", ""), sx, sy)
                for seg in segs:
                    if len(seg) >= 2:
                        d.line(seg, fill=fg, width=lw)

        img = img.resize((display, display), Image.LANCZOS)
        return ImageTk.PhotoImage(img)

    def _parse_svg_path(self, data, sx, sy):
        segments, cur, cx, cy, start_x, start_y = [], [], 0.0, 0.0, 0.0, 0.0
        tokens = re.findall(r"[MmLlCcZzHhVvSs]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?", data)
        i, cmd = 0, None

        def bezier(p0, p1, p2, p3, steps=16):
            pts = []
            for k in range(steps + 1):
                t = k / steps
                u = 1 - t
                x = u**3*p0[0] + 3*u**2*t*p1[0] + 3*u*t**2*p2[0] + t**3*p3[0]
                y = u**3*p0[1] + 3*u**2*t*p1[1] + 3*u*t**2*p2[1] + t**3*p3[1]
                pts.append((x * sx, y * sy))
            return pts

        while i < len(tokens):
            t = tokens[i]
            if t.isalpha():
                cmd = t; i += 1; continue
            try:
                if cmd in "Mm":
                    x, y = float(tokens[i]), float(tokens[i+1]); i += 2
                    if cmd == "m": x += cx; y += cy
                    if cur: segments.append(cur)
                    start_x, start_y, cx, cy = x, y, x, y
                    cur = [(x*sx, y*sy)]
                    cmd = "L" if cmd == "M" else "l"
                elif cmd in "Ll":
                    x, y = float(tokens[i]), float(tokens[i+1]); i += 2
                    if cmd == "l": x += cx; y += cy
                    cur.append((x*sx, y*sy)); cx, cy = x, y
                elif cmd in "Hh":
                    x = float(tokens[i]); i += 1
                    if cmd == "h": x += cx
                    cur.append((x*sx, cy*sy)); cx = x
                elif cmd in "Vv":
                    y = float(tokens[i]); i += 1
                    if cmd == "v": y += cy
                    cur.append((cx*sx, y*sy)); cy = y
                elif cmd in "Cc":
                    x1,y1 = float(tokens[i]),float(tokens[i+1]); i+=2
                    x2,y2 = float(tokens[i]),float(tokens[i+1]); i+=2
                    x, y  = float(tokens[i]),float(tokens[i+1]); i+=2
                    if cmd == "c":
                        x1+=cx; y1+=cy; x2+=cx; y2+=cy; x+=cx; y+=cy
                    pts = bezier((cx,cy),(x1,y1),(x2,y2),(x,y))
                    cur.extend(pts[1:]);  cx, cy = x, y
                elif cmd in "Ss":
                    x2,y2 = float(tokens[i]),float(tokens[i+1]); i+=2
                    x, y  = float(tokens[i]),float(tokens[i+1]); i+=2
                    if cmd == "s": x2+=cx; y2+=cy; x+=cx; y+=cy
                    x1,y1 = 2*cx - x2, 2*cy - y2
                    pts = bezier((cx,cy),(x1,y1),(x2,y2),(x,y))
                    cur.extend(pts[1:]); cx, cy = x, y
                elif cmd in "Zz":
                    cur.append((start_x*sx, start_y*sy))
                    segments.append(cur); cur = []; cmd = None
                else:
                    i += 1
            except (IndexError, ValueError):
                i += 1

        if cur:
            segments.append(cur)
        return segments

    def _import_image(self):
        path = filedialog.askopenfilename(filetypes=[("Image files", "*.jpg *.jpeg *.png *.bmp *.tiff *.webp")])
        if not path:
            return
        self.current_image = self._resize(path)
        self._original_image = self.current_image.copy()
        self._applied_effects.clear()
        new_img = self.current_image.resize((800, 800))
        new_canvas = ImageTk.PhotoImage(new_img)
        self.image_label.config(image=new_canvas)
        self.image_label.image = new_canvas
        self._log(f"Imported: {path.split('/')[-1]}", "ok")

    def _import_random(self):
        import threading, urllib.request, io as _io
        self._log("Fetching random image…", "info")

        def _fetch():
            try:
                url = "https://picsum.photos/1600/1600"
                req = urllib.request.Request(url, headers={"User-Agent": "Bogoshop/1.0"})
                with urllib.request.urlopen(req, timeout=12) as r:
                    data = r.read()
                img = Image.open(_io.BytesIO(data)).convert("RGBA")
                self.current_image = img
                self._original_image = img.copy()
                self._applied_effects.clear()
                preview = img.resize((800, 800))
                tk_img = ImageTk.PhotoImage(preview)
                def _update():
                    self.image_label.config(image=tk_img)
                    self.image_label.image = tk_img
                    if self._wiki_open:
                        self._close_wiki_inline()
                    self._log("Random image loaded from Picsum.", "ok")
                self.root.after(0, _update)
            except Exception as e:
                self.root.after(0, lambda: self._log(f"Picsum error: {e}", "err"))

        threading.Thread(target=_fetch, daemon=True).start()

    def _save_image(self):
        path = filedialog.asksaveasfilename(defaultextension=".png",
                                            filetypes=[("PNG", "*.png"), ("JPEG", "*.jpg"), ("All files", "*.*")])
        if not path:
            return
        self.current_image.save(path)
        self._log(f"Saved: {path.split('/')[-1]}", "ok")

    # --- community ---

    def _open_community(self):
        self._nav_stack.clear()
        if not self.community.logged_in:
            self._auth_dialog()
        else:
            self._show_community_panel()

    def _auth_dialog(self):
        for w in self.wiki_view.winfo_children():
            w.destroy()

        BG = "#1A1A1A"

        # banner
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        Label(hdr, text="Community", font="Plus_Jakarta_Sans 14 bold",
              bg="#221F3A", fg="#f5f5f5", padx=14, pady=8).pack(side=LEFT)
        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)

        # centered card
        outer = Frame(self.wiki_view, bg=BG)
        outer.pack(fill=BOTH, expand=1)
        card = Frame(outer, bg="#242424")
        card.place(relx=0.5, rely=0.5, anchor="center")

        Label(card, text="Sign in to Bogoshop",
              font="Plus_Jakarta_Sans 14 bold", bg="#242424", fg="#f5f5f5",
              pady=4).pack(padx=30, pady=(24, 2))
        Label(card, text="Login or create an account",
              font="Plus_Jakarta_Sans 8 italic", bg="#242424", fg="#666666"
              ).pack(padx=30)

        Frame(card, bg="#333333", height=1).pack(fill=X, padx=20, pady=(16, 0))

        form = Frame(card, bg="#242424")
        form.pack(padx=30, pady=16, fill=X)

        _lbl = dict(bg="#242424", fg="#aaaaaa", font="Plus_Jakarta_Sans 9", anchor="w")
        _ent = dict(bg="#2D2D2D", fg="#f5f5f5", insertbackground="#f5f5f5",
                    relief=FLAT, font="Plus_Jakarta_Sans 10",
                    highlightthickness=1, highlightbackground="#444444",
                    highlightcolor="#7EC8A4", width=28)

        Label(form, text="Username  (register only)", **_lbl).pack(fill=X)
        user_entry = Entry(form, **_ent)
        user_entry.pack(fill=X, ipady=5, pady=(2, 10))

        Label(form, text="Email", **_lbl).pack(fill=X)
        email_entry = Entry(form, **_ent)
        email_entry.pack(fill=X, ipady=5, pady=(2, 10))

        Label(form, text="Password", **_lbl).pack(fill=X)
        pass_entry = Entry(form, show="•", **_ent)
        pass_entry.pack(fill=X, ipady=5, pady=(2, 0))

        status = Label(card, text="", font="Plus_Jakarta_Sans 8",
                       bg="#242424", fg="#E06C75", wraplength=280)
        status.pack(pady=(6, 0))

        btn_row = Frame(card, bg="#242424")
        btn_row.pack(pady=(10, 24))

        def _attempt(action):
            email    = email_entry.get().strip()
            password = pass_entry.get()
            if action == "register":
                username = user_entry.get().strip()
                if not username or not email or not password:
                    status.config(text="Fill in all fields.")
                    return
                err = self.community.register(email, password, username)
            else:
                if not email or not password:
                    status.config(text="Fill in email and password.")
                    return
                err = self.community.login(email, password)
            if err:
                status.config(text=err)
            else:
                self._log(f"Logged in as {self.community.username}", "ok")
                self._show_community_panel()

        self._btn(btn_row, "Login",    lambda: _attempt("login")).pack(side=LEFT, padx=(0, 8))
        self._btn(btn_row, "Register", lambda: _attempt("register")).pack(side=LEFT)

        email_entry.focus_set()
        self.wiki_view.bind("<Return>", lambda _: _attempt("login"))

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._close_wiki_inline)

    def _show_community_panel(self):
        self._nav_push()
        self._current_view = lambda: self._show_community_panel()
        for widget in self.wiki_view.winfo_children():
            widget.destroy()

        # header
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        Label(hdr, text="Community", font="Plus_Jakarta_Sans 14 bold",
              bg="#221F3A", fg="#f5f5f5", padx=14, pady=8).pack(side=LEFT)
        Label(hdr, text=f"logged in as  {self.community.username}",
              font="Plus_Jakarta_Sans 8 italic",
              bg="#221F3A", fg="#888888").pack(side=RIGHT, padx=14)

        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)

        # gallery
        gallery_outer = Frame(self.wiki_view, bg="#1E1E1E")
        gallery_outer.pack(fill=BOTH, expand=1)

        canvas = Canvas(gallery_outer, bg="#1E1E1E", highlightthickness=0)
        scrollbar = ttk.Scrollbar(gallery_outer, orient=VERTICAL, command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=RIGHT, fill=Y)
        canvas.pack(side=LEFT, fill=BOTH, expand=1)

        gallery = Frame(canvas, bg="#1E1E1E")
        canvas_window = canvas.create_window((0, 0), window=gallery, anchor="nw")

        def _on_resize(e):
            canvas.itemconfig(canvas_window, width=e.width)
        canvas.bind("<Configure>", _on_resize)
        gallery.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        self._bind_scroll(canvas)

        self._load_gallery(canvas, gallery)

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._close_wiki_inline)

    def _load_gallery(self, canvas, gallery: Frame):
        import urllib.request

        posts = self.community.fetch_posts()
        if not posts:
            Label(gallery, text="No posts yet. Be the first to upload!",
                  font="Plus_Jakarta_Sans 10 italic", bg="#1E1E1E", fg="#555555"
                  ).pack(pady=40)
            return

        def _loader(post):
            url = post.get("image_url", "")
            with urllib.request.urlopen(url, timeout=8) as r:
                data = r.read()
            return Image.open(io.BytesIO(data)).convert("RGB")

        def _meta(post):
            t = post.get("title", "untitled")
            if len(t) > 22: t = t[:20] + "…"
            return [
                (t,                              "Plus_Jakarta_Sans 8 bold",   "#f5f5f5"),
                (f"by {post.get('username','?')}","Plus_Jakarta_Sans 7 italic", "#666666"),
            ]

        self._make_responsive_gallery(
            canvas, gallery, posts, _loader, _meta,
            lambda post, lbl: self._open_post_view(post, lbl),
            cache_key=lambda post: post.get("image_url", "")
        )

    def _open_post_view(self, post: dict, img_lbl):
        import threading, urllib.request
        self._nav_push()
        self._current_view = lambda: self._open_post_view(post, img_lbl)
        for w in self.wiki_view.winfo_children():
            w.destroy()

        BG = "#1A1A1A"

        # ── scrollable area ───────────────────────────────────────────────
        outer = Frame(self.wiki_view, bg=BG)
        outer.pack(fill=BOTH, expand=1)

        canvas = Canvas(outer, bg=BG, highlightthickness=0, bd=0)
        sb = Scrollbar(outer, orient=VERTICAL, command=canvas.yview)
        canvas.configure(yscrollcommand=sb.set)
        sb.pack(side=RIGHT, fill=Y)
        canvas.pack(side=LEFT, fill=BOTH, expand=1)

        content = Frame(canvas, bg=BG)
        _cw = canvas.create_window((0, 0), window=content, anchor="nw")

        def _on_canvas_resize(e):
            canvas.itemconfig(_cw, width=e.width)
        canvas.bind("<Configure>", _on_canvas_resize)
        content.bind("<Configure>", lambda _: canvas.configure(
            scrollregion=canvas.bbox("all")))
        self._bind_scroll(canvas)

        # ── banner header ─────────────────────────────────────────────────
        hdr = Frame(content, bg="#221F3A")
        hdr.pack(fill=X)
        self._btn(hdr, "← Back", self._nav_back,
                  bg="#221F3A", fg="#888888",
                  font="Plus_Jakarta_Sans 9", padx=12, pady=8
                  ).pack(side=LEFT)
        Label(hdr, text=post.get("title", "untitled"),
              font="Plus_Jakarta_Sans 14 bold", bg="#221F3A", fg="#f5f5f5",
              padx=8).pack(side=LEFT)

        Frame(content, bg="#333333", height=1).pack(fill=X)
        meta_row = Frame(content, bg="#2A2A2A")
        meta_row.pack(fill=X)

        # left: by <username> (clickable → profile, right-click → copy)
        _uname = post.get("username", "?")
        by_lbl = Label(meta_row, text="by ",
                       font="Plus_Jakarta_Sans 8 italic",
                       bg="#2A2A2A", fg="#888888", padx=12, pady=4)
        by_lbl.pack(side=LEFT)
        uname_lbl = Label(meta_row, text=_uname,
                          font="Plus_Jakarta_Sans 8 italic bold",
                          bg="#2A2A2A", fg="#aaaaaa", pady=4, cursor="hand2")
        uname_lbl.pack(side=LEFT)
        uname_lbl.bind("<Enter>", lambda _: uname_lbl.config(fg="#f5f5f5"))
        uname_lbl.bind("<Leave>", lambda _: uname_lbl.config(fg="#aaaaaa"))
        uname_lbl.bind("<Button-1>", lambda _, u=_uname: self._open_user_profile(u))
        uname_lbl.bind("<Button-2>", lambda _, u=_uname: (
            self.root.clipboard_clear(), self.root.clipboard_append(u),
            uname_lbl.config(fg="#7EC8A4"),
            uname_lbl.after(1200, lambda: uname_lbl.config(fg="#aaaaaa"))
        ))

        # right: Post ID: <number> (click to copy)
        _sid = str(post.get("short_id", ""))
        if _sid:
            id_frame = Frame(meta_row, bg="#2A2A2A")
            id_frame.pack(side=RIGHT, padx=8)
            Label(id_frame, text="Post ID: ",
                  font="Plus_Jakarta_Sans 8", bg="#2A2A2A", fg="#555555",
                  pady=4).pack(side=LEFT)
            sid_lbl = Label(id_frame, text=f"<{_sid}>",
                            font="Plus_Jakarta_Sans 8 bold", bg="#2A2A2A", fg="#555555",
                            cursor="hand2", pady=4)
            sid_lbl.pack(side=LEFT)
            def _copy_id(s=_sid):
                self.root.clipboard_clear()
                self.root.clipboard_append(f"<{s}>")
                sid_lbl.config(fg="#7EC8A4")
                sid_lbl.after(1200, lambda: sid_lbl.config(fg="#555555"))
            sid_lbl.bind("<Button-1>", lambda _: _copy_id())
            sid_lbl.bind("<Enter>", lambda _: sid_lbl.config(fg="#aaaaaa"))
            sid_lbl.bind("<Leave>", lambda _: sid_lbl.config(fg="#555555"))

        Frame(content, bg="#333333", height=1).pack(fill=X)

        # ── image ─────────────────────────────────────────────────────────
        img_container = Label(content, bg=BG, text="loading…",
                              fg="#444444", font="Plus_Jakarta_Sans 10")
        img_container.pack(pady=14, padx=16)

        def _load_full():
            try:
                raw = getattr(img_lbl, "_raw", None)
                if raw is None:
                    with urllib.request.urlopen(post["image_url"], timeout=10) as r:
                        raw = r.read()
                full = Image.open(io.BytesIO(raw)).convert("RGB")
                full.thumbnail((680, 680))
                tk_img = ImageTk.PhotoImage(full)
                img_container.config(image=tk_img, text="")
                img_container.image = tk_img
                img_container._raw = raw
            except Exception as e:
                img_container.config(text=f"Failed: {e}", fg="#E06C75")

        threading.Thread(target=_load_full, daemon=True).start()

        # ── description ───────────────────────────────────────────────────
        desc = post.get("description", "").strip()
        if desc:
            Frame(content, bg="#2A2A2A", height=1).pack(fill=X, padx=16)
            Label(content, text=desc,
                  font="Plus_Jakarta_Sans 10", bg=BG, fg="#aaaaaa",
                  anchor="w", padx=16, wraplength=680, justify=LEFT
                  ).pack(fill=X, pady=10)

        # ── action buttons ────────────────────────────────────────────────
        Frame(content, bg="#2A2A2A", height=1).pack(fill=X, padx=16)
        btn_row = Frame(content, bg=BG)
        btn_row.pack(fill=X, padx=16, pady=10)


        def _import():
            try:
                raw = getattr(img_container, "_raw", None)
                if raw is None:
                    with urllib.request.urlopen(post["image_url"], timeout=10) as r:
                        raw = r.read()
                self.current_image = Image.open(io.BytesIO(raw)).convert("RGBA")
                self._original_image = self.current_image.copy()
                self._applied_effects.clear()
                new_img = self.current_image.resize((800, 800))
                tk_img  = ImageTk.PhotoImage(new_img)
                self.image_label.config(image=tk_img)
                self.image_label.image = tk_img
                self._close_wiki_inline()
                self._log(f"Imported '{post.get('title')}' from community.", "ok")
            except Exception as e:
                self._log(f"Import failed: {e}", "err")

        def _download():
            path = filedialog.asksaveasfilename(
                defaultextension=".jpg",
                filetypes=[("JPEG", "*.jpg")],
                initialfile=f"{post.get('title', 'image')}.jpg"
            )
            if not path:
                return
            try:
                raw = getattr(img_container, "_raw", None)
                if raw is None:
                    with urllib.request.urlopen(post["image_url"], timeout=10) as r:
                        raw = r.read()
                with open(path, "wb") as f:
                    f.write(raw)
                self._log(f"Downloaded '{post.get('title')}' to {path}", "ok")
            except Exception as e:
                self._log(f"Download failed: {e}", "err")

        self._btn(btn_row, "Import to editor", _import).pack(side=LEFT, padx=(0, 8))
        self._btn(btn_row, "Download", _download).pack(side=LEFT)

        # ── likes + comments ──────────────────────────────────────────────
        post_id = post.get("id", "")

        def _time_ago(ts):
            import datetime
            try:
                t = datetime.datetime.fromisoformat(ts.replace("Z", "+00:00"))
                d = int((datetime.datetime.now(datetime.timezone.utc) - t).total_seconds())
                if d < 60:    return "just now"
                if d < 3600:  return f"{d//60}m ago"
                if d < 86400: return f"{d//3600}h ago"
                return f"{d//86400}d ago"
            except Exception:
                return ""

        # like bar
        Frame(content, bg="#2A2A2A", height=1).pack(fill=X)
        like_row = Frame(content, bg=BG)
        like_row.pack(fill=X, padx=16, pady=8)

        _like_state = {"count": 0, "liked": False}

        def _refresh_like():
            h = "♥" if _like_state["liked"] else "♡"
            like_lbl.config(text=f"{h}  {_like_state['count']}  {'Liked' if _like_state['liked'] else 'Like'}")

        def _toggle_like():
            if not self.community.session:
                self._log("Log in to like posts.", "err"); return
            def _do():
                try:
                    s = self.community.toggle_like(post_id)
                    _like_state["count"] = s["likes"]
                    _like_state["liked"] = s["has_liked"]
                    try: like_row.after(0, _refresh_like)
                    except Exception: pass
                except Exception as e:
                    self._log(str(e), "err")
            threading.Thread(target=_do, daemon=True).start()

        like_lbl = self._btn(like_row, "♡  0  Like", _toggle_like,
                             font="Plus_Jakarta_Sans 9", padx=12, pady=5)
        like_lbl.pack(side=LEFT)

        def _load_stats():
            s = self.community.get_post_stats(post_id)
            _like_state["count"] = s["likes"]
            _like_state["liked"] = s["has_liked"]
            try: like_row.after(0, _refresh_like)
            except Exception: pass
        threading.Thread(target=_load_stats, daemon=True).start()

        # comments header
        Frame(content, bg="#2A2A2A", height=1).pack(fill=X)
        chdr = Frame(content, bg=BG)
        chdr.pack(fill=X, padx=16, pady=(10, 4))
        comments_lbl = Label(chdr, text="Comments",
                             font="Plus_Jakarta_Sans 10 bold", bg=BG, fg="#f5f5f5")
        comments_lbl.pack(side=LEFT)
        Frame(content, bg="#2A2A2A", height=1).pack(fill=X)

        # comment input (logged-in only)
        if self.community.session:
            inp_row = Frame(content, bg=BG)
            inp_row.pack(fill=X, padx=16, pady=8)
            PLACEHOLDER = "Write a comment…"
            c_entry = Entry(inp_row, bg="#2D2D2D", fg="#555555",
                            insertbackground="#f5f5f5",
                            font="Plus_Jakarta_Sans 10", relief=FLAT,
                            highlightthickness=1, highlightbackground="#444444",
                            highlightcolor="#6C6C9A")
            c_entry.insert(0, PLACEHOLDER)
            c_entry.pack(side=LEFT, fill=X, expand=1, ipady=6, padx=(0, 8))
            c_entry.bind("<FocusIn>",  lambda _: (c_entry.delete(0, END), c_entry.config(fg="#f5f5f5"))
                         if c_entry.get() == PLACEHOLDER else None)
            c_entry.bind("<FocusOut>", lambda _: (c_entry.insert(0, PLACEHOLDER), c_entry.config(fg="#555555"))
                         if not c_entry.get().strip() else None)

            def _submit_top(pid=None):
                txt = c_entry.get().strip()
                if not txt or txt == PLACEHOLDER: return
                try:
                    self.community.add_comment(post_id, txt, pid)
                    c_entry.delete(0, END)
                    c_entry.config(fg="#555555"); c_entry.insert(0, PLACEHOLDER)
                    _reload_comments()
                except Exception as e:
                    self._log(str(e), "err")

            c_entry.bind("<Return>", lambda _: _submit_top())
            self._btn(inp_row, "Post", _submit_top,
                      font="Plus_Jakarta_Sans 9", padx=12, pady=5).pack(side=LEFT)

        # comments list
        comments_frame = Frame(content, bg=BG)
        comments_frame.pack(fill=X, padx=16, pady=(4, 24))

        def _render_comment(parent, c, replies_map, depth=0):
            INDENT, CBORDER = 20, "#3A3A5A" if depth == 0 else "#2A2A3A"
            block = Frame(parent, bg="#1E1E2E")
            block.pack(fill=X, pady=(0, 5), padx=(depth * INDENT, 0))
            Frame(block, bg=CBORDER, width=3).pack(side=LEFT, fill=Y)
            inner = Frame(block, bg="#1E1E2E")
            inner.pack(side=LEFT, fill=X, expand=1, padx=8, pady=6)

            # header
            hr = Frame(inner, bg="#1E1E2E")
            hr.pack(fill=X)
            unl = Label(hr, text=c.get("username", "?"),
                        font="Plus_Jakarta_Sans 9 bold", bg="#1E1E2E", fg="#9A9AFF",
                        cursor="hand2")
            unl.pack(side=LEFT)
            unl.bind("<Enter>", lambda _: unl.config(fg="#ccccff"))
            unl.bind("<Leave>", lambda _: unl.config(fg="#9A9AFF"))
            unl.bind("<Button-1>", lambda _, u=c.get("username","?"): self._open_user_profile(u))
            Label(hr, text=f"  ·  {_time_ago(c.get('created_at',''))}",
                  font="Plus_Jakarta_Sans 8", bg="#1E1E2E", fg="#555555").pack(side=LEFT)
            Label(hr, text=f"↑ {c.get('upvotes', 0)}",
                  font="Plus_Jakarta_Sans 8 bold", bg="#1E1E2E", fg="#7EC8A4").pack(side=RIGHT)

            # body
            self._inline_text(inner, c.get("content", ""),
                              bg="#1E1E2E", fg="#dddddd",
                              font="Plus_Jakarta_Sans 10").pack(fill=X, pady=(2, 4))

            # actions
            act = Frame(inner, bg="#1E1E2E")
            act.pack(fill=X)

            def _upvote(cid=c["id"]):
                def _do():
                    self.community.upvote_comment(cid)
                    try: comments_frame.after(0, _reload_comments)
                    except Exception: pass
                threading.Thread(target=_do, daemon=True).start()

            for txt, clr, cmd in [("↑ upvote", "#555555", _upvote)]:
                al = Label(act, text=txt, font="Plus_Jakarta_Sans 8",
                           bg="#1E1E2E", fg=clr, cursor="hand2")
                al.pack(side=LEFT, padx=(0, 8))
                al.bind("<Button-1>", lambda _, fn=cmd: fn())
                al.bind("<Enter>", lambda _, w=al: w.config(fg="#7EC8A4"))
                al.bind("<Leave>", lambda _, w=al, c2=clr: w.config(fg=c2))

            if self.community.session and depth < 2:
                rl = Label(act, text="reply", font="Plus_Jakarta_Sans 8",
                           bg="#1E1E2E", fg="#555555", cursor="hand2")
                rl.pack(side=LEFT)
                rl.bind("<Enter>", lambda _: rl.config(fg="#9A9AFF"))
                rl.bind("<Leave>", lambda _: rl.config(fg="#555555"))

                _reply_box_ref = [None]

                def _toggle_reply(cid=c["id"]):
                    if _reply_box_ref[0] and _reply_box_ref[0].winfo_exists():
                        _reply_box_ref[0].destroy(); _reply_box_ref[0] = None; return
                    rb = Frame(inner, bg="#1E1E2E")
                    rb.pack(fill=X, pady=(2, 2))
                    _reply_box_ref[0] = rb
                    rv = Entry(rb, bg="#252535", fg="#f5f5f5",
                               insertbackground="#f5f5f5",
                               font="Plus_Jakarta_Sans 9", relief=FLAT,
                               highlightthickness=1, highlightbackground="#444444")
                    rv.pack(side=LEFT, fill=X, expand=1, ipady=5)

                    def _post_reply(cid=cid):
                        txt = rv.get().strip()
                        if not txt: return
                        try:
                            self.community.add_comment(post_id, txt, cid)
                            rb.destroy(); _reply_box_ref[0] = None
                            _reload_comments()
                        except Exception as e:
                            self._log(str(e), "err")

                    rv.bind("<Return>", lambda _: _post_reply())
                    self._btn(rb, "Reply", _post_reply,
                              font="Plus_Jakarta_Sans 8", padx=8, pady=4
                              ).pack(side=LEFT, padx=(4, 0))
                    rv.focus_set()

                rl.bind("<Button-1>", lambda _: _toggle_reply())

            # render replies
            for rep in replies_map.get(c["id"], []):
                _render_comment(parent, rep, replies_map, depth + 1)

        def _render_all(comments):
            for w in comments_frame.winfo_children():
                w.destroy()
            roots = [c for c in comments if not c.get("parent_id")]
            rmap  = {}
            for c in comments:
                if c.get("parent_id"):
                    rmap.setdefault(c["parent_id"], []).append(c)
            comments_lbl.config(text=f"Comments  ({len(comments)})")
            if not roots:
                Label(comments_frame, text="No comments yet. Be the first!",
                      font="Plus_Jakarta_Sans 9 italic", bg=BG, fg="#444444",
                      anchor="w").pack(fill=X, pady=8)
                return
            for c in roots:
                _render_comment(comments_frame, c, rmap)

        def _reload_comments():
            def _fetch():
                cs = self.community.get_comments(post_id)
                try: comments_frame.after(0, lambda: _render_all(cs))
                except Exception: pass
            threading.Thread(target=_fetch, daemon=True).start()

        _reload_comments()

        # ── effects footer ────────────────────────────────────────────────
        _fx = post.get("effects", "").strip()
        if _fx:
            Frame(content, bg="#222222", height=1).pack(fill=X, pady=(16, 0))
            Label(content, text="Effects applied:",
                  font="Plus_Jakarta_Sans 8 italic", bg="#1A1A1A", fg="#444444",
                  anchor="w", padx=16).pack(fill=X, pady=(6, 2))
            Label(content, text=_fx,
                  font="Plus_Jakarta_Sans 8 italic", bg="#1A1A1A", fg="#3A3A3A",
                  anchor="w", padx=16, justify=LEFT, wraplength=580).pack(fill=X, pady=(0, 20))

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", lambda _: self._show_community_panel())

    def _upload_post(self):
        if self.current_image is None:
            self._log("Load an image first before uploading.", "err")
            return
        self._nav_push()
        self._current_view = lambda: self._upload_post()

        for w in self.wiki_view.winfo_children():
            w.destroy()

        BG   = "#1A1A1A"
        CARD = "#242424"

        # ── banner ────────────────────────────────────────────────────────
        hdr = Frame(self.wiki_view, bg="#221F3A")
        hdr.pack(fill=X)
        self._btn(hdr, "← Back", self._nav_back,
                  bg="#221F3A", fg="#888888",
                  font="Plus_Jakarta_Sans 9", padx=12, pady=8
                  ).pack(side=LEFT)
        Label(hdr, text="Upload to Community",
              font="Plus_Jakarta_Sans 14 bold", bg="#221F3A", fg="#f5f5f5",
              padx=8).pack(side=LEFT)
        Frame(self.wiki_view, bg="#333333", height=1).pack(fill=X)

        # ── scrollable body ───────────────────────────────────────────────
        outer = Frame(self.wiki_view, bg=BG)
        outer.pack(fill=BOTH, expand=1)
        canvas = Canvas(outer, bg=BG, highlightthickness=0, bd=0)
        sb = Scrollbar(outer, orient=VERTICAL, command=canvas.yview)
        canvas.configure(yscrollcommand=sb.set)
        sb.pack(side=RIGHT, fill=Y)
        canvas.pack(side=LEFT, fill=BOTH, expand=1)
        body = Frame(canvas, bg=BG)
        _cw = canvas.create_window((0, 0), window=body, anchor="nw")
        canvas.bind("<Configure>", lambda e: canvas.itemconfig(_cw, width=e.width))
        body.bind("<Configure>", lambda _: canvas.configure(scrollregion=canvas.bbox("all")))
        self._bind_scroll(canvas)

        PAD = 28

        # ── preview card ──────────────────────────────────────────────────
        prev_card = Frame(body, bg=CARD)
        prev_card.pack(fill=X, padx=PAD, pady=(20, 0))
        thumb = self.current_image.copy().convert("RGB")
        thumb.thumbnail((320, 320))
        _tk_thumb = ImageTk.PhotoImage(thumb)
        thumb_lbl = Label(prev_card, image=_tk_thumb, bg=CARD)
        thumb_lbl.pack(pady=16)
        thumb_lbl._tk_thumb = _tk_thumb

        Frame(body, bg="#333333", height=1).pack(fill=X, padx=PAD)

        # ── form card ─────────────────────────────────────────────────────
        form = Frame(body, bg=CARD)
        form.pack(fill=X, padx=PAD, pady=(0, 24))

        _lbl = dict(bg=CARD, fg="#888888", font="Plus_Jakarta_Sans 8", anchor="w")
        _ent = dict(bg="#1E1E1E", fg="#f5f5f5", insertbackground="#f5f5f5",
                    relief=FLAT, font="Plus_Jakarta_Sans 10",
                    highlightthickness=1, highlightbackground="#444444",
                    highlightcolor="#6C6C9A")

        Label(form, text="TITLE", **_lbl).pack(fill=X, padx=16, pady=(16, 2))
        title_entry = Entry(form, **_ent)
        title_entry.pack(fill=X, padx=16, ipady=6, pady=(0, 12))

        Label(form, text="DESCRIPTION", **_lbl).pack(fill=X, padx=16, pady=(0, 2))
        desc_entry = Text(form, height=5, **_ent)
        desc_entry.pack(fill=X, padx=16, pady=(0, 16))

        Frame(form, bg="#333333", height=1).pack(fill=X, padx=0)

        status = Label(form, text="", font="Plus_Jakarta_Sans 8",
                       bg=CARD, fg="#E06C75", wraplength=500, justify=LEFT)
        status.pack(fill=X, padx=16, pady=(8, 0))

        btn_row = Frame(form, bg=CARD)
        btn_row.pack(fill=X, padx=16, pady=(8, 16))

        def _do_upload():
            title = title_entry.get().strip() or "untitled"
            desc  = desc_entry.get("1.0", END).strip()
            status.config(text="Uploading…", fg="#888888")
            body.update_idletasks()
            err = self.community.upload_post(
                self.current_image, title, self._applied_effects, desc)
            if err:
                status.config(text=err, fg="#E06C75")
            else:
                self._log(f"Uploaded '{title}' to community.", "ok")
                self._show_community_panel()

        self._btn(btn_row, "Upload", _do_upload).pack(side=LEFT, padx=(0, 8))
        self._btn(btn_row, "Cancel", self._nav_back).pack(side=LEFT)

        title_entry.focus_set()
        self.wiki_view.bind("<Return>", lambda _: _do_upload())

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", lambda _: self._show_community_panel())

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

    # --- profile ---

    _PROFILE_FILE = "profile.json"
    _PROFILE_PIC  = "profile_pic.jpg"  # base name; actual = profile_pic_<username>.jpg

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
        sb = Scrollbar(outer, orient=VERTICAL, command=canvas.yview)
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

        _avatar_colors = [
            "#E06C75","#E5C07B","#98C379","#56B6C2",
            "#61AFEF","#C678DD","#D19A66","#7EC8A4",
        ]

        def _make_avatar(letter: str, color: str, size: int = 80) -> ImageTk.PhotoImage:
            from PIL import ImageFont
            img = Image.new("RGB", (size, size), color)
            draw = ImageDraw.Draw(img)
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
                try:
                    font = ImageFont.load_default(size=int(size * 0.48))
                except Exception:
                    font = ImageFont.load_default()
            bbox = draw.textbbox((0, 0), letter, font=font)
            tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
            draw.text(((size - tw) // 2, (size - th) // 2 - 2),
                      letter, fill="#ffffff", font=font)
            return ImageTk.PhotoImage(img)

        def _load_pic():
            pic_path = self._profile_pic_path()
            if os.path.exists(pic_path):
                img = Image.open(pic_path).convert("RGB").resize((80, 80))
                tk_img = ImageTk.PhotoImage(img)
            else:
                color = _avatar_colors[hash(username) % len(_avatar_colors)]
                tk_img = _make_avatar(username[0].upper(), color)
            self._profile_pic_label.config(image=tk_img, width=80, height=80)
            self._profile_pic_label.image = tk_img

        _load_pic()

        def _change_pic():
            path = filedialog.askopenfilename(
                filetypes=[("Images", "*.jpg *.jpeg *.png *.webp")])
            if not path:
                return
            img = Image.open(path).convert("RGB")
            w, h = img.size
            m = min(w, h)
            img = img.crop(((w - m) // 2, (h - m) // 2,
                             (w + m) // 2, (h + m) // 2))
            img = img.resize((200, 200))
            img.save(self._profile_pic_path(), format="JPEG", quality=92)
            _load_pic()

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
            data["bio"] = bio_box.get("1.0", END).strip()
            self._profile_save(data)
            self._log("Profile saved.", "ok")

        self._btn(info, "Save", _save_bio,
                  font="Plus_Jakarta_Sans 8 bold", padx=10, pady=4
                  ).pack(anchor="e", pady=(6, 0))

        # ── user posts ────────────────────────────────────────────────────
        Frame(body, bg="#2A2A2A", height=1).pack(fill=X, padx=20)
        Label(body, text="Your Posts",
              font="Plus_Jakarta_Sans 11 bold", bg=BG, fg="#f5f5f5",
              anchor="w", padx=20).pack(fill=X, pady=(12, 6))

        import threading, urllib.request

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
        import threading, urllib.request
        self._nav_push()
        self._current_view = lambda: self._open_user_profile(username)
        for w in self.wiki_view.winfo_children():
            w.destroy()

        BG = "#1A1A1A"
        _avatar_colors = [
            "#E06C75","#E5C07B","#98C379","#56B6C2",
            "#61AFEF","#C678DD","#D19A66","#7EC8A4",
        ]

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
        sb = Scrollbar(outer, orient=VERTICAL, command=canvas.yview)
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

        color = _avatar_colors[hash(username) % len(_avatar_colors)]
        # reuse _make_avatar via a local call — build inline
        from PIL import ImageFont
        _av = Image.new("RGB", (80, 80), color)
        _draw = ImageDraw.Draw(_av)
        _font = None
        for _fp in ["/System/Library/Fonts/Helvetica.ttc",
                    "/System/Library/Fonts/SFNSDisplay.ttf",
                    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]:
            try: _font = ImageFont.truetype(_fp, 38); break
            except Exception: pass
        if _font is None:
            try: _font = ImageFont.load_default(size=38)
            except Exception: _font = ImageFont.load_default()
        _bb = _draw.textbbox((0, 0), username[0].upper(), font=_font)
        _tw, _th = _bb[2] - _bb[0], _bb[3] - _bb[1]
        _draw.text(((80 - _tw) // 2, (80 - _th) // 2 - 2),
                   username[0].upper(), fill="#ffffff", font=_font)
        _tk_av = ImageTk.PhotoImage(_av)

        av_lbl = Label(card, image=_tk_av, bg="#242424", width=80, height=80)
        av_lbl.image = _tk_av
        av_lbl.pack(side=LEFT, padx=16, pady=16)

        info = Frame(card, bg="#242424")
        info.pack(side=LEFT, fill=X, expand=1, padx=(0, 16), pady=16)
        Label(info, text=username, font="Plus_Jakarta_Sans 16 bold",
              bg="#242424", fg="#f5f5f5", anchor="w").pack(fill=X)

        # bio — show own bio from local storage, blank for others
        _bio = ""
        if username == self.community.username:
            _bio = self._profile_load().get("bio", "")
        if _bio:
            Label(info, text=_bio,
                  font="Plus_Jakarta_Sans 9 italic", bg="#242424", fg="#888888",
                  anchor="w", justify=LEFT, wraplength=400).pack(fill=X, pady=(4, 0))

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
                return [
                    (t,                       "Plus_Jakarta_Sans 8 bold",   "#f5f5f5"),
                    (post.get("effects", ""), "Plus_Jakarta_Sans 7 italic", "#666666"),
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

    # --- workspace ---

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
        sb = Scrollbar(outer, orient=VERTICAL, command=canvas.yview)
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

        import threading
        threading.Thread(target=_load, daemon=True).start()

        self._wiki_open = True
        self.wiki_view.lift()
        self.root.bind("<Escape>", self._open_workspace)

    # --- effects ---

    def _posterize_1bit(self):
        self.progress_bar["value"] = 0

        threshold = 140
        arr = np.array(self.current_image)
        rgb = arr[..., :3].astype(np.float32)
        luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]
        arr[luma < threshold, :3] = 0
        arr[luma >= threshold, :3] = 255
        self.current_image = Image.fromarray(arr)

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

    def _posterize_3bit(self):
        self.progress_bar["value"] = 0

        def _step(i):
            return 255 / 7 * i
        _W, _H = self.current_image.size
        arr = np.array(self.current_image)
        rgb = arr[..., :3].astype(np.float32)
        luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]

        steps = [_step(i) for i in range(8)]

        arr[luma <= steps[0], :3] = steps[0]
        arr[(luma > steps[0]) & (luma <= steps[1]), :3] = steps[1]
        arr[(luma > steps[1]) & (luma <= steps[2]), :3] = steps[2]
        arr[(luma > steps[2]) & (luma <= steps[3]), :3] = steps[3]
        arr[(luma > steps[3]) & (luma <= steps[4]), :3] = steps[4]
        arr[(luma > steps[4]) & (luma <= steps[5]), :3] = steps[5]
        arr[(luma > steps[5]) & (luma <= steps[6]), :3] = steps[6]
        arr[luma > steps[6], :3] = steps[7]

        self.current_image = Image.fromarray(arr)

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

    def _duotone_threshold(self):
        self.progress_bar["value"] = 0

        threshold = 210
        arr = np.array(self.current_image)
        rgb = arr[..., :3].astype(np.float32)
        luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]
        arr[luma < threshold, :3] = 0
        arr[luma >= threshold, :3] = [255, 0, 0]
        self.current_image = Image.fromarray(arr)

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

    def _negative(self):
        self.progress_bar["value"] = 0
        arr = np.array(self.current_image)
        arr[..., :3] = 255 - arr[..., :3] 
        self.current_image = Image.fromarray(arr)

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

    def _glow(self, radius=14):
        self.progress_bar["value"] = 0

        arr = np.array(self.current_image)
        blur = Image.fromarray(arr).filter(ImageFilter.GaussianBlur(radius=radius))
        blur_arr = np.array(blur)
        strength = 0.3
        glow_arr = np.clip(arr.astype(np.int16) + blur_arr.astype(np.int16) * strength, 0, 255).astype(np.uint8)
        self.current_image = Image.fromarray(glow_arr)

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(800, lambda: self.progress_bar.configure(value=0))

    def _color_grain(self):
        _W, _H = self.current_image.size
        self.progress_bar["value"] = 0

        arr = np.array(self.current_image).astype(np.float32)
        grain = np.random.randint(0, 256, (_H, _W, 3), dtype=np.uint8).astype(np.float32)
        strength = 0.3
        arr[..., :3] = np.clip(arr[..., :3] * (1 - strength) + grain * strength, 0, 255)
        self.current_image = Image.fromarray(arr.astype(np.uint8))

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

    def _hue_shift(self, amount=10):
        self.progress_bar["value"] = 0

        arr = np.array(self.current_image.convert("RGB"))
        hsv = cv2.cvtColor(arr, cv2.COLOR_RGB2HSV).astype(np.float32)
        hsv[..., 0] = (hsv[..., 0] + amount / 2) % 180
        result = cv2.cvtColor(hsv.astype(np.uint8), cv2.COLOR_HSV2RGB)
        self.current_image = Image.fromarray(result)

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))
    
    def _brightness_up(self):
        self.progress_bar["value"] = 0

        arr = np.array(self.current_image).astype(np.float32)
        arr[..., :3] += 10
        arr = np.clip(arr, 0, 255).astype(np.uint8)
        self.current_image = Image.fromarray(arr)
        

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

    def _brightness_down(self):
        self.progress_bar["value"] = 0

        arr = np.array(self.current_image).astype(np.float32)
        arr[..., :3] -= 10
        arr = np.clip(arr, 0, 255).astype(np.uint8)
        self.current_image = Image.fromarray(arr)

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

if __name__ == "__main__":
    app = App()