from PIL import Image, ImageTk
import tkinter as tk



# import numpy as np
root = tk.Tk()
# open
img = Image.open("img2.jpg")

_W, _H = img.size
if _W == _H:
    crop_image = img
elif _W > _H:
    cut = round((_W-_H)//2)
    crop_image = img.crop((cut,0,_W-cut,_H))
else:
    cut = round((_H-_W)//2)
    crop_image = img.crop((0,cut,_W,_H-cut))

_W, _H = crop_image.size
pixels = crop_image.load()

img = crop_image.resize((800, 800))

tk_img = ImageTk.PhotoImage(img)
label = tk.Label(root,image=tk_img)
label.pack()
variable = "1"
img_info = tk.Label(root, text=f"Format: {img.format} Size: {img.size} Mode: {img.mode}")
img_info.pack()
text_label = tk.Label(root, text= "Enter Effect ID: ")
text_label.pack()
entry = tk.Entry(root)
entry.pack()

def get_value(int_start, int_end):
    USER_INPUT = entry.get()
    USER_INPUT = int(USER_INPUT)
    print(USER_INPUT)
    if USER_INPUT < int_start or USER_INPUT > int_end:
        text_label = tk.Label(root, text= "Unaccepted value. Please Enter again:")
        text_label.pack()
    elif USER_INPUT == "":
        return
    else:
        return USER_INPUT

def on_enter(event):

    USER_INPUT = entry.get()
    if USER_INPUT == "1":
        entry.delete(0, tk.END)
        text_label = tk.Label(root, text = "Input Threshold: (0-255)")
        text_label.pack()
        
        get_value(0,255)
        print(USER_INPUT)
        USER_INPUT = int(USER_INPUT)
        _T = USER_INPUT, USER_INPUT, USER_INPUT
        print(_T)
        if int(USER_INPUT) > 255:
            print("Threshold is too high -- set to maximum")
            _T = (255, 255, 255)
        else:
            print("Proccessing Image...")
        for y in range(_H):
            for x in range(_W):
                r, g, b = pixels[x,y]
                _rgb = pixels[x,y]
                if _rgb >= _T:
                    r, g ,b = (0, 0, 0)          
                else:
                    r, g ,b = (255, 255, 255)
                pixels[x,y] = r, g, b

    applied = crop_image.resize((800, 800))
    tk_img_applied = ImageTk.PhotoImage(applied)
    label.config(image=tk_img_applied)
    label.image = tk_img_applied
    




entry.delete(0, tk.END)

entry.bind("<Return>", on_enter)

root.mainloop()
