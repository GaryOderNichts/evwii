# Enhanced vWii (evWii)

This is a plugin for [Aroma](https://aroma.foryour.cafe) which tries to improve some of the Wii U's virtual Wii features.  
It works by patching features used for setting up the vWii mode while still in Wii U mode. This also includes [DMCU](https://wiiubrew.org/wiki/Hardware/DMCU) patches.  
Note that the plugin will not be active when booting vWii mode using the [Boot Selector](https://github.com/wiiu-env/AutobootModule).

## Features
- Allows enabling the 4 second power button press to force power off while in vWii mode.
- Allows setting custom viewport values in the DMCU firmware. This can undo the forced cropping of overscan in vWii mode.

## Examples
**GamePad (viewport unchanged):**  
<img src="https://i.imgur.com/3GCQrD0.png" width=600px/>

**GamePad (viewport adjusted):**  
<img src="https://i.imgur.com/7jkR2BQ.png" width=600px/>

*Note:* These are not actual GamePad captures, but visualizations of what viewport adjustments do.

## Bulding
For building you need: 
- [wups](https://github.com/wiiu-env/WiiUPluginSystem)
- [wut](https://github.com/devkitPro/wut)
- [libmocha](https://github.com/wiiu-env/libmocha)

You can also build evwii using docker:
```
# Build docker image (only needed once)
docker build . -t evwii_builder

# make 
docker run -it --rm -v ${PWD}:/project evwii_builder make

# make clean
docker run -it --rm -v ${PWD}:/project evwii_builder make clean
```

## Special Thanks
- [@vaguerant](https://github.com/vaguerant/) and [@Ingunar](https://github.com/Ingunar) for all of the vWii testing they have done for this!
