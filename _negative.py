from PIL import Image
import numpy as np

img = Image.open("img2.jpg")
arr = np.array(img)

print(arr.shape)   # z.B. (600, 800, 3)
print(arr.dtype)   # uint8
print(arr[0,0])

result_arr = 255 - arr

result = Image.fromarray(result_arr)
result.save("output.jpg")

