from PIL import Image
import numpy as np

img = Image.open("img2.jpg")
a = np.array(img)

print(a.shape)
print(a.dtype)   

new_array = a.astype(np.int16) - 25
new_array[new_array > 255] = 255
new_array[new_array < 0] = 0
new_array = new_array.astype(np.uint8)

result = Image.fromarray(new_array)
result.save("output.jpg")

