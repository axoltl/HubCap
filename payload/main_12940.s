.global start
.arm
.align 4

#define lower16(n)      ((n) & 0xFFFF)
#define upper16(n)      (((n) >> 16) & 0xFFFF)


#define TOP_STACK       0x500000

#define PATCH_OFF       (0x14C)

start:
# Set up the stack
    mov sp, TOP_STACK
    adr r0, 1f + 1
    bx r0

1:
.thumb
# Give a hello
    adr r0, hello_world
    ldr r9, printf
    blx r9

# Turn on the white LED
    ldr r5, led_set
    mov r0, #1
    mov r1, #0
    blx r5

    ldr r10, read_gpio
    mov r11, #0x420000

# Wait for a button press
no_button:
    mov r0, #0x6
    mov r1, r11
    blx r10
    ldr r0, [r11]
    cmp r0, #0
    bne no_button
    
# Fix up what we broke, starting with the hub allocation.
    mov r1, #0x400000
    mov r2, #0x410000
    ldr r3, hub_fixup_addr

# We know r0 is 0, store it into the hub_index.
    str r0, [r1, #4]
# Store the fixup pointers into the immediate pool.
    str r1, [r3, #0]
    str r2, [r3, #4]

# Disable the VerifyImage check
    ldr r6, load_image_patch_addr
    ldr r8, load_image_patch
    str r8, [r6]

# Force the boot down the USB path.
    add r6, r6, #0xC9C
    str r8, [r6]

# Clear dcache    
    ldr r7, dcache_clear
    mov r0, #0x180000
    mov r1, #0x500000
    blx r7

# Invalidate icache
    mcr p15, 0, r15, c7, c5, 0

    adr r0, hello_world
    blx r9

# Right before we vector, turn the white LED back off
    mov r0, #0
    mov r1, #1
    blx r5    

# Jump into the load image portion
    mov r0, #2
    ldr r4, load_image_r4
    ldr r6, load_image
    bx r6
/*
set_led:
    .long 0x0019DA1C
*/
printf:
    .long 0x00181714
led_set:
    .long 0x0019A004
read_gpio:
    .long 0x001833D0
dcache_clear:
    .long 0x001801C0
load_image_patch_addr:
    .long 0x0019A334
load_image_patch:
    .long 0xE320F000
load_image:
    .long 0x0019B7C0
load_image_r4:
    .long 0x001B0094
hub_fixup_addr:
    .long 0x001A115C
hello_world:
    .asciz "Hi\n"
