#ifndef DFU_LOAD_H
#define DFU_LOAD_H
#include "dfu.h"

int dfuload_do_upload(dfu_if *dif, int xfer_size, int expected_size, int fd);
off_t dfuload_do_dnload(dfu_if *dif, int xfer_size, dfu_file *file, int *percent);

#endif /* DFU_LOAD_H */
