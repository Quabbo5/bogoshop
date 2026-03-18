from tkinter import *
from tkinter import ttk
import time
import numpy as np
from PIL import Image, ImageTk

# resize
def _resize(path):
    img = Image.open("img2.jpg")
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

# create list
effect_list = Listbox(left_panel)
effect_list.insert(1, "0001# Threshold")
effect_list.insert(2, "0002# Negative")
effect_list.insert(3,"0003# Glow")

left_panel.add(effect_list)

# build right panel
right_panel = PanedWindow(main, orient= VERTICAL)
main.add(right_panel)

# build header
header_widget = Label(right_panel, text="Dont do silly stuff or the gods of beyond the worlds will devour your soul")
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

# handle input
def on_click(event):
    selection = effect_list.curselection()
    if selection: 
        index = selection[0]
        _apply_effect(index)

effect_list.bind("<Double-Button-1>", on_click)

# apply effect
def _apply_effect(index):
    if index == 0:
        _blackAndWhiteThreshold()
    if index == 1:
        _negative()
    if index == 2:
        _glow()
    new_img = crop_image.resize((800, 800))
    new_canvas = ImageTk.PhotoImage(new_img)
    image.config(image=new_canvas)
    image.image = new_canvas

# define effects

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
    
    # mehrfach unscharf machen und hell aufblenden
    blur = base.filter(ImageFilter.GaussianBlur(radius=15))
    blur_arr = np.array(blur)
    
    glow_arr = np.clip(arr.astype(np.int16) + blur_arr.astype(np.int16), 0, 255).astype(np.uint8)
    
    crop_image = Image.fromarray(glow_arr)
    
    progress_bar["value"] = 100
    root.update_idletasks()
    root.after(800, lambda: progress_bar.configure(value=0))

# struct
EFFECTS = [
    {"id": "0001", "name": "Threshold", "fn": _blackAndWhiteThreshold},
    {"id": "0002", "name": "Negative",  "fn": _negative},
    {"id": "0003", "name": "Glow",      "fn": _glow},
]












# run
root.mainloop()