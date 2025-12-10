# xmouseleast
Control your mouse cursor with your keyboard (with physics)

## installation
Clone and run
```sudo make clean install```
by default `xmouseleast` will be located at `/usr/local/bin`

## config
Edit `config.h` and rebuild 

## tips and tricks
I use [sxhkd](https://github.com/baskerville/sxhkd) to run `xmouseleast` while a modifier is held.  
Example (NOTE: ISO_Level3_Shift can be replaced with any modifier):
``` conf           
~ISO_Level3_Shift: 
    xmouseleast    
```
Then add `ISO_Level3_Shift` to `modifiers` in `config.h`, so it won't get released when running the program.
Finally add a `QUIT` binding, in `bindings`, mapped to `ISO_Level3_Shift`.

## credits
- [xmouseless](https://github.com/jbensmann/xmouseless) (original program)
- [warpd](https://github.com/rvaiya/warpd)
- bakkeby ([simple](https://github.com/bakkeby/xban/blob/master/xban.c) xinput2 implementation)
