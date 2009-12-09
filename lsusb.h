
#ifndef _LSUSB_H
#define _LSUSB_H

/* From the kernel file, include/linux/stringify.h */
#define __stringify_1(x...)     #x
#define __stringify(x...)       __stringify_1(x)

/* Functions in the core */
void *robust_malloc(size_t size);
char *get_dev_string(struct udev_device *device, const char *name);
extern struct udev *udev;

/* device.c */
void create_usb_device(struct udev_device *device);
void free_usb_devices(void);
void sort_usb_devices(void);
void print_usb_devices(void);

/* interface.c */
void create_usb_interface(struct udev_device *device, struct usb_device *usb_device);
void free_usb_interface(struct usb_interface *usb_intf);

/* endpoint.c */
struct usb_endpoint *create_usb_endpoint(struct udev_device *device,
					 const char *endpoint_name);
void free_usb_endpoint(struct usb_endpoint *usb_endpoint);

/* raw.c */
void read_raw_usb_descriptor(struct udev_device *device, struct usb_device *usb_device);

#endif	/* define _LSUSB_H */
