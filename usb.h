#include "list.h"

struct usb_device;

struct usb_interface {
	struct list_head list;
	struct usb_interface *next;
	struct usb_device *parent;
	unsigned int configuration;
	unsigned int ifnum;

	unsigned int bAlternateSetting;
	unsigned int bInterfaceClass;
	unsigned int bInterfaceNumber;
	unsigned int bInterfaceProtocol;
	unsigned int bInterfaceSubClass;
	unsigned int bNumEndpoints;

	char *name;
	char *driver;
};


struct usb_device {
	struct list_head list;			/* connect devices independant of the bus */
	struct usb_device *next;		/* next port on this hub */
	struct usb_interface *first_interface;	/* list of interfaces */
	struct usb_device *first_child;		/* connect devices on this port */
	struct usb_device *parent;		/* hub this device is connected to */
	unsigned int parent_portnum;
	unsigned int portnum;

	unsigned int bMaxPacketSize0;
	char bMaxPower[64];
	unsigned int bNumConfigurations;
	unsigned int bNumInterfaces;
	unsigned int bmAttributes;
	unsigned int configuration;
	const char *idProduct;
	const char *idVendor;
	const char *busnum;
	const char *devnum;
	unsigned int maxchild;

	const char *bConfigurationValue;
	const char *bDeviceClass;
	const char *bDeviceProtocol;
	const char *bDeviceSubClass;
	const char *manufacturer;
	const char *bcdDevice;
	const char *product;
	const char *serial;

	char version[64];
	char speed[4 + 1];      /* '1.5','12','480' + '\n' */

	char *name;
	char *driver;
};

