from tkinter import *
from tkinter import ttk
from tkinter import colorchooser
from PIL import Image, ImageTk
import numpy as np
import ctypes

class App:
    def __init__(self):

        self.root = Tk()
        self.root.title("Bogoshop")
        self.root.geometry("1000x950")
        self.root.minsize(1000, 950)
        #self.root.state("zoomed")
        self.root.config(bg="black")

        # dunkle Titelleiste (Windows 10/11)
        self.root.update()
        hwnd = ctypes.windll.user32.GetParent(self.root.winfo_id())
        ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, 20, ctypes.byref(ctypes.c_int(1)), ctypes.sizeof(ctypes.c_int))

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

        self.current_image = self._resize("img/jpegs/img2.jpg")
        self.img = self.current_image.resize((800, 800))
        self.img_canvas = ImageTk.PhotoImage(self.img)

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
        self.right_panel = PanedWindow(self.right_border, orient=VERTICAL, bg="#2D2D2D")
        self.right_panel.pack(fill=BOTH, expand=1)

        # build header
        self.header_widget = Label(self.right_panel, text="BOGOSHOP v.0.2", font="Plus_Jakarta_Sans 8 bold", bg="#2D2D2D", fg="#f5f5f5")
        self.right_panel.add(self.header_widget)

        # build canvas
        self.image_canvas = PanedWindow(self.main, orient=VERTICAL, bg="#2D2D2D")
        self.right_panel.add(self.image_canvas)

        self.image_label = Label(self.right_panel, image=self.img_canvas, bg="#1B1B1B")
        self.right_panel.add(self.image_label, padx=30, pady=30)

        # build textbox frame
        self.input_frame = Frame(self.right_panel, bg="#2D2D2D")
        self.right_panel.add(self.input_frame, minsize=20)

        self.id_input_field = Label(self.input_frame, text="Enter Effect ID:", font="Plus_Jakarta_Sans 8 bold", bg="#2D2D2D", fg="#f5f5f5")
        self.id_input_field.pack()
        self.id_input_entry = Entry(self.input_frame, width=30, bg="#3C3C3C", fg="#f5f5f5", insertbackground="#f5f5f5")
        self.id_input_entry.pack()

        # build progressbar
        style = ttk.Style()
        style.theme_use("default")
        style.configure("dark.Horizontal.TProgressbar",troughcolor="#3C3C3C", background="#7C7CFF", darkcolor="#7C7CFF", lightcolor="#7C7CFF", bordercolor="#2D2D2D")
        self.progress_bar = ttk.Progressbar(self.right_panel, orient=HORIZONTAL, length=300, mode="determinate", style="dark.Horizontal.TProgressbar")
        self.right_panel.add(self.progress_bar)

        # build effect list
        self.effect_list = Listbox(self.left_panel, font="Plus_Jakarta_Sans 10", bg="#2D2D2D", fg="#f5f5f5", highlightthickness=0, bd=0)
        self.left_panel.add(self.effect_list)

        self.EFFECTS = [
            {"id": "0001", "name": "Posterize 1bit", "fn": self._posterize_1bit, "author": "lea"},
            {"id": "0002", "name": "Negative", "fn": self._negative, "author": "rango"},
            {"id": "0003", "name": "Glow", "fn": self._glow, "author": "rango"},
            {"id": "0004", "name": "Color Grain", "fn": self._color_grain, "author": "rango"},
            {"id": "0005", "name": "RedThreshold", "fn": self._duotone_threshold, "author": "rango"},
            {"id": "0006", "name": "Posterize 3bit", "fn": self._posterize_3bit, "author": "rango"},
        ]

        for e in self.EFFECTS:
            self.effect_list.insert(END, f"{e['id']} – {e['name']}")

        # keybinds
        self.effect_list.bind("<Double-Button-1>", self._on_click)
        self.effect_list.bind("<h>", self._on_h_press)

        self.root.mainloop()

    # --- event handlers ---

    def _on_click(self, event):
        selection = self.effect_list.curselection()
        if selection:
            index = selection[0]
            self._apply_effect(index)

    def _on_h_press(self, event):
        selection = self.effect_list.curselection()
        if selection:
            index = selection[0]
            self._load_wiki(index)

    # --- core ---

    def _resize(self, path):
        img = Image.open("img/jpegs/desert.jpg")
        _W, _H = img.size
        if _W == _H:
            return img
        elif _W > _H:
            cut = round((_W - _H) // 2)
            return img.crop((cut, 0, _W - cut, _H))
        else:
            cut = round((_H - _W) // 2)
            return img.crop((0, cut, _W, _H - cut))

    def _apply_effect(self, index):
        self.EFFECTS[index]["fn"]()
        new_img = self.current_image.resize((800, 800))
        new_canvas = ImageTk.PhotoImage(new_img)
        self.image_label.config(image=new_canvas)
        self.image_label.image = new_canvas

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
            text = f.read()

        wiki.mainloop()

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
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

    def _posterize_3bit(self):
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

    def _duotone_threshold(self):
        threshold = 210
        arr = np.array(self.current_image)
        rgb = arr[..., :3].astype(np.float32)
        luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]
        arr[luma < threshold, :3] = 0
        arr[luma >= threshold, :3] = [255, 0, 0]
        self.current_image = Image.fromarray(arr)

    def _negative(self):
        self.progress_bar["value"] = 0
        arr = np.array(self.current_image)
        arr[..., :3] = 255 - arr[..., :3] 
        self.current_image = Image.fromarray(arr)
        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))

    def _glow(self):
        from PIL import ImageFilter
        arr = np.array(self.current_image)
        blur = Image.fromarray(arr).filter(ImageFilter.GaussianBlur(radius=14))
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

if __name__ == "__main__":
    app = App()