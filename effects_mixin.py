from tkinter import *
from PIL import Image, ImageFilter, ImageTk
import numpy as np
import cv2


class EffectsMixin:

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

    def _ripple(self, amplitude=10, wavelength=30):
        self.progress_bar["value"] = 0

        arr = np.array(self.current_image)
        _H, _W = arr.shape[:2]
        output = np.zeros_like(arr)

        self.progress_bar["value"] = 25
        self.root.update_idletasks()

        x_coords = np.arange(_W)
        y_coords = np.arange(_H)
        xx, yy = np.meshgrid(x_coords, y_coords)

        self.progress_bar["value"] = 50
        self.root.update_idletasks()

        src_x = np.clip(xx + (amplitude * np.sin(2 * np.pi * yy / wavelength)).astype(np.int32), 0, _W - 1)
        src_y = np.clip(yy + (amplitude * np.sin(2 * np.pi * xx / wavelength)).astype(np.int32), 0, _H - 1)

        self.progress_bar["value"] = 75
        self.root.update_idletasks()

        output = arr[src_y, src_x]
        self.current_image = Image.fromarray(output)

        self.progress_bar["value"] = 100
        self.root.update_idletasks()
        self.root.after(20, lambda: self.progress_bar.configure(value=0))
