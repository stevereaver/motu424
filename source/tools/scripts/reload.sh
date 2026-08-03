#!/bin/bash
for i in {1..20}; do
    fuser -k -9 /dev/snd/* 2>/dev/null
    killall -9 pipewire wireplumber pipewire-pulse aplay 2>/dev/null
    rmmod motu_poke_driver 2>/dev/null
    rmmod motu_pci_alsa 2>/dev/null
    if ! lsmod | grep -q motu_pci_alsa && ! lsmod | grep -q motu_poke_driver; then
        echo "Modules unloaded successfully."
        # Clear dmesg
        dmesg -C
        insmod motu_pci_alsa.ko
        insmod motu_poke_driver.ko 2>/dev/null
        exit 0
    fi
    sleep 0.1
done
echo "Failed to unload module"
exit 1
