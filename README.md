# Back Story
So the goal of this was to be a quick testing ground for making a USB packet parser hook on HID devices such as all the connected mice on the machine. 
<br>
Hooking at the lowest level of input is needed for a current anticheat project I am working on where one of it's main goals is to detect and block fake inputs that didn't actually come from the keyboard and mouse.
<br>
This includes autoclicker software and mouse software macros like LGUB and Razer Synapse. Now I already have a ton of detections and tricks for detecting fake inputs, but some mouse macros work at a very low driver level.
<br>
To detect this low level device bafoonery, I decided to set up a USB packet hook to just check if the USB signals the mouse device has match up with the actual inputs being sent from it, which is what led me to finding LIBUSB.
<br>
Libusb ended up not being the right tool for the job, as even in their own wiki it is stated to not use it for HID devices (which includes keyboards and mice) and of course I didn't read that until after hours of struggling.
<br>
Basically the reason this code doesn't work for the mouse callback event polling is because windows has a firm driver lock on the HID devices, and even trying to detatch the kernel (which I did) ended up not working.
<br>
So this program in its current state really would only work for linux with some custom kernel that lets you do this monkey buissness. You can still appreciate the minimal abstraction effort I put into this, as its still useful libusb example code.
<br>
I will be trying to make this experiment work properly using hidapi, which is also made by the same people who made libusb.
