#include <ASICamera2.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>

int main()
{
    printf("=== ZWO Camera Serial Numbers ===\n\n");

    // First get serials through ASI SDK
    printf("Via ASI SDK:\n");
    int numDevices = ASIGetNumOfConnectedCameras();
    if(numDevices <= 0)
    {
        printf("No ZWO cameras detected via SDK.\n");
    }
    else
    {
        ASI_CAMERA_INFO ASICameraInfo;
        for(int i = 0; i < numDevices; i++)
        {
            ASIGetCameraProperty(&ASICameraInfo, i);
            ASI_SN asi_sn;
            if (ASIGetSerialNumber(i, &asi_sn) == ASI_SUCCESS)
            {
                printf("%s: ", ASICameraInfo.Name);
                for (int j = 0; j < 8; ++j)
                    printf("%02x", asi_sn.id[j] & 0xff);
                printf("\n");
            }
        }
    }

    // Now get serials through libusb
    printf("\nVia LibUSB:\n");
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    ssize_t count;

    int ret = libusb_init(&ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to initialize libusb: %s\n", libusb_error_name(ret));
        return -1;
    }

    count = libusb_get_device_list(ctx, &list);
    if (count < 0)
    {
        fprintf(stderr, "Failed to get device list: %s\n", libusb_error_name(count));
        libusb_exit(ctx);
        return -1;
    }

    // ZWO's vendor ID is 0x03c3
    for (ssize_t i = 0; i < count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;
        ret = libusb_get_device_descriptor(device, &desc);
        if (ret < 0)
            continue;

        if (desc.idVendor == 0x03c3)
        {
            libusb_device_handle *handle;
            ret = libusb_open(device, &handle);
            if (ret < 0)
                continue;

            unsigned char string[256];

            printf("Camera at Bus %d, Port %d:\n",
                   libusb_get_bus_number(device),
                   libusb_get_port_number(device));

            if (desc.iProduct > 0)
            {
                ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
                if (ret > 0)
                    printf("  Product: %s\n", string);
            }

            if (desc.iSerialNumber > 0)
            {
                ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string, sizeof(string));
                if (ret > 0)
                    printf("  Serial: %s\n", string);
            }

            libusb_close(handle);
        }
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);

    return 0;
}
