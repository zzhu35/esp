#ifndef _FFTACCELERATOR_H_
#define _FFTACCELERATOR_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#ifndef __user
#define __user
#endif
#endif /* __KERNEL__ */

#include <esp.h>
#include <esp_accelerator.h>

struct fftaccelerator_access {
	struct esp_access esp;
	/* <<--regs-->> */
	unsigned stride;
	unsigned src_offset;
	unsigned dst_offset;
};

#define FFTACCELERATOR_IOC_ACCESS	_IOW ('S', 0, struct fftaccelerator_access)

#endif /* _FFTACCELERATOR_H_ */
