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

struct usb_endpoint {
	const char *bEndpointAddress;
	const char *bInterval;
	const char *bLength;
	const char *bmAttributes;
	const char *direction;
	const char *type;
	const char *wMaxPacketSize;
};

struct usb_device {
	struct list_head list;			/* connect devices independant of the bus */
	struct usb_device *next;		/* next port on this hub */
	struct usb_interface *first_interface;	/* list of interfaces */
	struct usb_device *first_child;		/* connect devices on this port */
	struct usb_device *parent;		/* hub this device is connected to */
	unsigned int parent_portnum;
	unsigned int portnum;

	unsigned int configuration;
	const char *idProduct;
	const char *idVendor;
	const char *busnum;
	const char *devnum;
	const char *maxchild;
	const char *quirks;
	const char *speed;
	const char *version;

	const char *bConfigurationValue;
	const char *bDeviceClass;
	const char *bDeviceProtocol;
	const char *bDeviceSubClass;
	const char *bNumConfigurations;
	const char *bNumInterfaces;
	const char *bmAttributes;
	const char *bMaxPacketSize0;
	const char *bMaxPower;
	const char *manufacturer;
	const char *bcdDevice;
	const char *product;
	const char *serial;

	char *name;
	char *driver;			/* always "usb" but hey, it's nice to be complete */
};

