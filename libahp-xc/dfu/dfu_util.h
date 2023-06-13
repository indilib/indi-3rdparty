#ifndef DFU_UTIL_H
#define DFU_UTIL_H

/* USB string descriptor should contain max 126 UTF-16 characters
 * but 254 would even accommodate a UTF-8 encoding + NUL terminator */
#define MAX_DESC_STR_LEN 254

typedef struct dfu_if_t dfu_if;
typedef struct libusb_context libusb_context;

enum mode {
	MODE_NONE,
	MODE_VERSION,
	MODE_LIST,
	MODE_DETACH,
	MODE_UPLOAD,
	MODE_DOWNLOAD
};

void probe_devices(libusb_context *);
void disconnect_devices(void);
void print_dfu_if(dfu_if *);
void list_dfu_interfaces(void);

#endif /* DFU_UTIL_H */
