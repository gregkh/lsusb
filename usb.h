#include "short_types.h"

struct usb_endpoint {
	struct list_head list;
	char *bEndpointAddress;
	char *bInterval;
	char *bLength;
	char *bmAttributes;
	char *direction;
	char *type;
	char *wMaxPacketSize;
};

struct usb_config {
	struct list_head list;
	u8 bLength;
	u8 bDescriptorType;
	u16 wTotalLength;
	u8 bNumInterfaces;
	u8 bConfigurationValue;
	u8 iConfiguration;
	u8 bmAttributes;
	u8 bMaxPower;
};

struct usb_interface {
	struct list_head list;
	struct list_head endpoints;
	unsigned int configuration;
	unsigned int ifnum;

	char *sysname;
	char *bAlternateSetting;
	char *bInterfaceClass;
	char *bInterfaceNumber;
	char *bInterfaceProtocol;
	char *bInterfaceSubClass;
	char *bNumEndpoints;

	char *name;
	char *driver;
};

struct usb_device {
	struct list_head list;			/* connect devices independant of the bus */
	struct list_head interfaces;

	char *idProduct;
	char *idVendor;
	char *busnum;
	char *devnum;
	char *maxchild;
	char *quirks;
	char *speed;
	char *version;

	char *bConfigurationValue;
	char *bDeviceClass;
	char *bDeviceProtocol;
	char *bDeviceSubClass;
	char *bNumConfigurations;
	char *bNumInterfaces;
	char *bmAttributes;
	char *bMaxPacketSize0;
	char *bMaxPower;
	char *manufacturer;
	char *bcdDevice;
	char *product;
	char *serial;

	struct usb_endpoint *ep0;
	char *name;
	char *driver;			/* always "usb" but hey, it's nice to be complete */
};

