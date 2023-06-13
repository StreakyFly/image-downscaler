# image-downscaler

Save space by downscalling and compressing images in a selected directory.

This is my first C++ project. I remade the image-downscaler program that I made in Python a while back (*resize_images.py*).


Turns out either I'm a complete fool, or OpenCV is just terrible at Jpeg compression. Usually I'd say it's the first option, but this time I actually believe it's the second. Did countless of tests and comparisons refactoring the code badjillion times and not once has OpenCV performed better than Pillow on a set of 100 completely random mixed images. OpenCV's image compression is much slower compared to Pillow, it produces images of worse quality than Pillow, the compressed image file sizes are much bigger than those compressed by Pillow and the CPU usage is much higher than Pillow's.

I'll try libvips next. Hopefully that'll work better.
