from tkinter import *
from tkinter import ttk
import time
import random
import numpy as np
from PIL import Image, ImageTk

# resize
def _resize(path):
    img = Image.open("test_image.png")
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
def _blackAndWhiteThreshold():
    USER_INPUT = 230
    _W, _H = crop_image.size
    pixels = crop_image.load()
    _T = USER_INPUT, USER_INPUT, USER_INPUT
    print(_T)
    if int(USER_INPUT) > 255:
        print("Threshold is too high -- set to maximum")
        _T = (255, 255, 255)
    else:
        print("Proccessing Image...")

    progress_bar.start()
    progress_bar["value"] = 0
    progress_bar["maximum"] = _H

    for y in range(_H):
        
        for x in range(_W):
            r, g, b = pixels[x,y]
            _rgb = pixels[x,y]
            if _rgb >= _T:
                r, g ,b = (0, 0, 0)          
            else:
                r, g ,b = (255, 255, 255)
            pixels[x,y] = r, g, b
        progress_bar["value"] = y + 1    
        root.update_idletasks()
    
    progress_bar["value"] = _H
    progress_bar.stop()

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
    
    blur = base.filter(ImageFilter.GaussianBlur(radius=15))
    blur_arr = np.array(blur)
    
    glow_arr = np.clip(arr.astype(np.int16) + blur_arr.astype(np.int16), 0, 255).astype(np.uint8)
    
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
    {"id": "0001", "name": "Threshold", "fn": _blackAndWhiteThreshold},
    {"id": "0002", "name": "Negative",  "fn": _negative},
    {"id": "0003", "name": "Glow",      "fn": _glow},
    {"id": "0004", "name": "Color Grain", "fn": _color_grain}
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

# apply effect
def _apply_effect(index):
    EFFECTS[index]["fn"]()

    new_img = crop_image.resize((800, 800))
    new_canvas = ImageTk.PhotoImage(new_img)
    image.config(image=new_canvas)
    image.image = new_canvas

# keybinds
effect_list.bind("<Double-Button-1>", on_click)

# run
root.mainloop()