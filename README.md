# Toccami

Virtual multitouch touchpad driver for Linux 

## Architecture

Linux Kernel Module that creates a character device (`/dev/toccamich`) that receives input from userspace regarding absolute (x, y) fingers positions, ID of that finger, and the state (UP, DOWN, DRAGGING): these data get sent as raw input using kernel functions (`input_*`)

## Build-Install-Test

```bash
# Build
make

# Install
sudo insmod src/toccami.ko; sudo chmod 777 /dev/toccamich

# Test
cat inputTest > /dev/toccamich

```

To view the logs, use `sudo dmesg -w`