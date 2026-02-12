target remote :3333

# break HardFault
# break DefaultHandler
# break SysTick
# break main

monitor reset halt
break main.rs:89
continue
