#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <string.h>

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>
#include <LUFA/Drivers/Board/LEDs.h>

#include <usb_device.h>
#include <uart.h>

struct usb_device* device;

void usb_init(void) {
    USB_Init();
    LEDs_Init();
    usb_update_led();
}

void usb_update_led() {
    LEDs_ToggleLEDs(LEDS_ALL_LEDS);
}

void usb_do(void) {
    USB_USBTask();
}

// Borrowed from one of the LUFA demos
void usb_setup_hardware(void) {
#if (ARCH == ARCH_AVR8)
    /* Disable watchdog if enabled by bootloader/fuses */
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    /* Disable clock division */
    clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
    /* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
    XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
    XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

    /* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
    XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
    XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

    PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

    /* Hardware Initialization */
    uart_init(BAUD_RATE);
    GlobalInterruptEnable();
}

int deferred_addr = -1;
static void _usb_set_addr(uint8_t val){

    while(Endpoint_GetBusyBanks());

    UDADDR = val & 0x7f;
    UDADDR |= (1 << ADDEN);
}

void usb_set_addr(uint8_t val){
    deferred_addr = val;
}

void usb_set_device(struct usb_device* dev, int force) {
    device = dev;
    if(force) {
        UDADDR = dev->addr & 0x7f;
        UDADDR |= 1 << ADDEN;
    } else {
        usb_set_addr(dev->addr);
    }
}

struct usb_device* change = NULL;
void usb_ignore_and_change(struct usb_device* dev) {
    change = dev;
}

void EVENT_USB_Device_ControlRequest(void){
    void* buffer = NULL;
    uint16_t length =  0;
    if(change != NULL) {
        usb_set_device(change, 1);
        Endpoint_ClearSETUP();
        Endpoint_ClearIN();
        Endpoint_ClearOUT();
    } else { 
        Endpoint_ClearSETUP();
    }
    uart_print("Type: %x, req: %x, val: %x, idx: %x, len: %d\n",
                USB_ControlRequest.bmRequestType,
                USB_ControlRequest.bRequest,
                USB_ControlRequest.wValue,
                USB_ControlRequest.wIndex,
                USB_ControlRequest.wLength);

    device->handle_configuration(   device,
                                    USB_ControlRequest.bmRequestType,
                                    USB_ControlRequest.bRequest,
                                    USB_ControlRequest.wValue,
                                    USB_ControlRequest.wIndex,
                                    USB_ControlRequest.wLength,
                                    &buffer,
                                    &length);

    if(buffer) {
        if(length == 0xFFFF) {
            return;
        }

        if(length) {
            if(length & CONST_PTR) {
                Endpoint_Write_Control_PStream_LE(buffer, length & ~CONST_PTR);
            } else {
                Endpoint_Write_Control_Stream_LE(buffer, length);
            }
        }

        Endpoint_ClearStatusStage();

        if(deferred_addr >= 0) {
            _usb_set_addr(deferred_addr);
            uart_print("Setting addr to %d\n", deferred_addr);
            deferred_addr = -1;
        }

    } else {
        Endpoint_StallTransaction();
        uart_print("STALLED!\n");
    }

}

uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue,
                                    const uint8_t wIndex,
                                    const void** const DescriptorAddress){
    return 0;
}
