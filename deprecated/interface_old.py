from tkinter import *
from tkinter import ttk
from tkinter import colorchooser
import numpy as np
from PIL import Image, ImageTk

# resize
def _resize(path):
    # destination path: /Volumes/196 GB/img/
    img = Image.open("/Volumes/196 GB/img/wallhaven-vge373.jpg")
    _W, _H = img.size
    if _W == _H:
        return img
    elif _W > _H:
        cut = round((_W-_H)//2)
        return img.crop((cut, 0, _W - cut, _H))
    else:
        cut = round((_H-_W)//2)
        return img.crop((0,cut,_W,_H-cut))

# build main instance
root = Tk()
root.title("Limboshop")

# build main container
main = PanedWindow(root, orient= HORIZONTAL)
main.pack(fill=BOTH, expand=1)

# build left panel
left_panel = PanedWindow(main, orient= VERTICAL)
main.add(left_panel)

list_title = Label(left_panel, text="Select Effect:")
left_panel.add(list_title)

# build right panel
right_panel = PanedWindow(main, orient= VERTICAL)
main.add(right_panel)

# build header
header_widget = Label(right_panel, text="Limboshop")
right_panel.add(header_widget)
# header_widget.insert(END, "This is the message of the day")

# build canvas
image_canvas = PanedWindow(main, orient= VERTICAL)
right_panel.add(image_canvas)

crop_image = _resize("img2.jpg")
pixels = crop_image.load()
img = crop_image.resize((800, 800))
img_canvas = ImageTk.PhotoImage(img)

image = Label(right_panel, image=img_canvas)
right_panel.add(image)

# build textbox frame
input_frame = Frame(right_panel)
right_panel.add(input_frame, minsize=20)

id_input_field = Label(input_frame, text= "Enter Effect ID:")
id_input_field.pack()
id_input_entry = Entry(input_frame, width=30)
id_input_entry.pack()

# build progressbar
progress_bar = ttk.Progressbar(right_panel, orient= HORIZONTAL, length=300, mode= "determinate")
right_panel.add(progress_bar)

# effects
def _Posterize_1bit():
    global crop_image
    progress_bar.start()
    progress_bar["value"] = 0
    threshold = 140
    arr = np.array(crop_image)

    # fetch rgb values as float for multiplication
    rgb = arr[..., :3].astype(np.float32)
    # def luma
    luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]

    # process
    arr[luma < threshold, :3] = 0
    arr[luma >= threshold, :3] = 255

    crop_image = Image.fromarray(arr)
    progress_bar["value"] = 100
    progress_bar.stop()

def _Posterize_3bit():
    color = colorchooser.askcolor()
    print(color)
    def _s3b(int):
        bits = 7
        steps = 255 / bits * (int)
        return steps
    
    global crop_image
    _W, _H = img.size
    arr = np.array(crop_image)
    rgb = arr[...,:3].astype(np.float32)
    luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]

    # def gray values
    bits = 8
    steps_list = []

    for i in range(bits):
        steps_list.append(_s3b(i))
        print(f"{i}: {steps_list[i]}")
    
    arr[luma <= steps_list[0], :3] = _s3b(0)
    arr[(luma > steps_list[0]) & (luma <= steps_list[1]), :3] = _s3b(1)
    arr[(luma > steps_list[1]) & (luma <= steps_list[2]), :3] = _s3b(2)
    arr[(luma > steps_list[2]) & (luma <= steps_list[3]), :3] = _s3b(3)
    arr[(luma > steps_list[3]) & (luma <= steps_list[4]), :3] = _s3b(4)
    arr[(luma > steps_list[4]) & (luma <= steps_list[5]), :3] = _s3b(5)
    arr[(luma > steps_list[5]) & (luma <= steps_list[6]), :3] = _s3b(6)
    arr[luma > steps_list[6], :3] = _s3b(7)

    crop_image = Image.fromarray(arr)

def _redAndWhiteThreshold():
    global crop_image
    
    threshold = 210
    arr = np.array(crop_image)

    rgb = arr[..., :3].astype(np.float32)
    luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]

    # process
    arr[luma < threshold, :3] = 0
    arr[luma >= threshold, :3] = [255,0,0]

    crop_image = Image.fromarray(arr)

def _negative():
    global crop_image
    progress_bar.start()
    progress_bar["value"] = 0

    arr = np.array(crop_image)
    result_arr = 255 - arr
    crop_image = Image.fromarray(result_arr)

    progress_bar["value"] = 100
    root.update_idletasks()
    root.after(20, lambda: progress_bar.configure(value=0))
    progress_bar.stop()

def _glow():
    global crop_image
    from PIL import ImageFilter
    
    arr = np.array(crop_image)
    base = Image.fromarray(arr)
    
    blur = base.filter(ImageFilter.GaussianBlur(radius=14))
    blur_arr = np.array(blur)

    strength = 0.3
    glow_arr = np.clip(arr.astype(np.int16) + blur_arr.astype(np.int16) * strength, 0, 255).astype(np.uint8)
    
    crop_image = Image.fromarray(glow_arr)
    
    progress_bar["value"] = 100
    root.update_idletasks()
    root.after(800, lambda: progress_bar.configure(value=0))

def _color_grain():
    global crop_image
    _W, _H = crop_image.size
    progress_bar["value"] = 0

    arr = np.array(crop_image).astype(np.float32)
    grain = np.random.randint(0, 256, (_H, _W, 3), dtype=np.uint8).astype(np.float32)

    strength = 0.3
    arr[..., :3] = np.clip(arr[..., :3] * (1 - strength) + grain * strength, 0, 255)

    crop_image = Image.fromarray(arr.astype(np.uint8))
    progress_bar["value"] = 100
    root.update_idletasks()
    root.after(20, lambda: progress_bar.configure(value=0))

# struct
EFFECTS = [
    {"id": "0001", "name": "Posterize 1bit", "fn": _Posterize_1bit, "author": "rango"},
    {"id": "0002", "name": "Negative",  "fn": _negative, "author": "rango"},
    {"id": "0003", "name": "Glow",      "fn": _glow, "author": "rango"},
    {"id": "0004", "name": "Color Grain", "fn": _color_grain, "author": "rango"},
    {"id": "0005", "name": "RedThreshold", "fn": _redAndWhiteThreshold, "author": "rango"},
    {"id": "0006", "name": "Posterize 3bit", "fn": _Posterize_3bit, "author": "rango"}
]

# build list
effect_list = Listbox(left_panel)
for e in EFFECTS:
    effect_list.insert(END, f"{e['id']} - {e['name']}")

left_panel.add(effect_list)

# handle input
def on_click(event):
    selection = effect_list.curselection()
    if selection: 
        index = selection[0]
        _apply_effect(index)

def on_h_press(event):
    selection = effect_list.curselection()
    if selection:
        index = selection[0]

        _load_wiki(index)

# apply effect
def _apply_effect(index):
    EFFECTS[index]["fn"]()

    new_img = crop_image.resize((800, 800))
    new_canvas = ImageTk.PhotoImage(new_img)
    image.config(image=new_canvas)
    image.image = new_canvas

def _load_wiki(index):
    def _generate_preview(index):
        EFFECTS[index]["fn"]()

        img_before_resized = ImageTk.PhotoImage(img_after)
        # img_after_resize.config(image=)

    wiki = Toplevel()
    wiki.geometry("600x800")
    wiki.title(f"Wiki for: {EFFECTS[index]["name"]}")

    # fetch icon
    icon = PhotoImage(file="/Volumes/196 GB/img/icon(2).png")

    # fetch example image
    img_before = Image.open("img/wallhaven-ym73l7.png")
    img_before.thumbnail((300,200))
    img_before_resized = ImageTk.PhotoImage(img_before)

    # fetch effect and apply
    img_after = Image.open("img/wallhaven-ym73l7.png")
    img_after.thumbnail((300,200))
    img_after_resized = ImageTk.PhotoImage(img_after)

    # create labels
    # wiki_image = Label(wiki, justify=RIGHT, image=icon).pack(side=RIGHT, anchor="ne")
    wiki_text = Label(wiki,
                            justify=LEFT, 
                            padx=0,
                            pady=0,
                            anchor=None,
                            text=EFFECTS[index]["name"],
                            fg="#f5f5f5",
                            bg="#221F3A",
                            font="Helvetica 24 bold bold",
                            width=None,
                            height=2
                    ).pack(side=TOP, anchor="nw",fill=X)
    wiki_subtext = Label(wiki,
                            justify=LEFT,
                            padx=0,
                            pady=0,
                            anchor=None,
                            text=f"ID: #{EFFECTS[index]["id"]}",
                            fg="#c0c0c0",
                            bg="#424242",
                            font="Helvetica 13 bold",
                            width=None,
                            height=1
                    ).pack(side=TOP, anchor="nw", fill=X)
    wiki_effect_id = Label(wiki,
                            justify=LEFT,
                            padx=0,
                            pady=0,
                            anchor=None,
                            text=f"Author: {EFFECTS[index]["author"]} Type: (Type following)",
                            fg="#808080",
                            bg="#424242",
                            font="Helvetica 9 italic",
                            width=None,
                            height=1
                    ).pack(side=TOP, anchor="s", fill=X)
    
    image_frame = Frame(wiki)
    image_frame.pack(fill=X)

    Label(image_frame, image=img_before_resized).pack(side=LEFT)
    Label(image_frame, image=img_after_resized).pack(side=RIGHT)



    wiki.mainloop()



# keybinds
effect_list.bind("<Double-Button-1>", on_click)

effect_list.bind("<h>", on_h_press)

# run
root.mainloop()