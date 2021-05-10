# Toccami [WIP: currently not working]

Virtual multitouch touchpad driver for Linux 

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