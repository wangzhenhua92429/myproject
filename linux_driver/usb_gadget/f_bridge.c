// SPDX-License-Identifier: GPL-2.0+
/*
 * f_loopback.c - USB peripheral loopback configuration driver
 *
 * Copyright (C) 2003-2008 David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 */

/* #define VERBOSE_DEBUG */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/usb/composite.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include "g_zero.h"
#include "u_f.h"

/*
 * LOOPBACK FUNCTION ... a testing vehicle for USB peripherals,
 *
 * This takes messages of various sizes written OUT to a device, and loops
 * them back so they can be read IN from it.  It has been used by certain
 * test applications.  It supports limited testing of data queueing logic.
 */
struct f_loopback {
	struct usb_function	function;

	struct usb_ep		*in_ep;
	struct usb_ep		*out_ep;

	unsigned                qlen;
	unsigned                buflen;
};

static inline struct f_loopback *func_to_loop(struct usb_function *f)
{
	return container_of(f, struct f_loopback, function);
}

/*-------------------------------------------------------------------------*/

static struct usb_interface_descriptor loopback_intf = {
	.bLength =		sizeof(loopback_intf),
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor fs_loop_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_loop_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_loopback_descs[] = {
	(struct usb_descriptor_header *) &loopback_intf,
	(struct usb_descriptor_header *) &fs_loop_sink_desc,
	(struct usb_descriptor_header *) &fs_loop_source_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor hs_loop_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_loop_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *hs_loopback_descs[] = {
	(struct usb_descriptor_header *) &loopback_intf,
	(struct usb_descriptor_header *) &hs_loop_source_desc,
	(struct usb_descriptor_header *) &hs_loop_sink_desc,
	NULL,
};

/* super speed support: */

static struct usb_endpoint_descriptor ss_loop_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_loop_source_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_endpoint_descriptor ss_loop_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_loop_sink_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_descriptor_header *ss_loopback_descs[] = {
	(struct usb_descriptor_header *) &loopback_intf,
	(struct usb_descriptor_header *) &ss_loop_source_desc,
	(struct usb_descriptor_header *) &ss_loop_source_comp_desc,
	(struct usb_descriptor_header *) &ss_loop_sink_desc,
	(struct usb_descriptor_header *) &ss_loop_sink_comp_desc,
	NULL,
};

/* function-specific strings: */

static struct usb_string strings_loopback[] = {
	[0].s = "loop input to output",
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_loop = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_loopback,
};

static struct usb_gadget_strings *loopback_strings[] = {
	&stringtab_loop,
	NULL,
};

struct bridge_dev {
    wait_queue_head_t write_wq;
    wait_queue_head_t read_wq;
    atomic_t write_excl;
    atomic_t read_excl;

    int rx_done;
    int tx_done;

    int tx_is_idle;
    int is_online;

    struct usb_ep *ep_in;
	struct usb_ep *ep_out;

    struct usb_request *req_in;
    struct usb_request *req_out;
};
static struct bridge_dev st_bridge_dev;
/*-------------------------------------------------------------------------*/

static int loopback_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_loopback	*loop = func_to_loop(f);
	int			id;
	int ret;

    // printk("%s(%d)\n", __func__, __LINE__);

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	loopback_intf.bInterfaceNumber = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_loopback[0].id = id;
	loopback_intf.iInterface = id;

	/* allocate endpoints */

	loop->in_ep = usb_ep_autoconfig(cdev->gadget, &fs_loop_source_desc);
	if (!loop->in_ep) {
autoconf_fail:
		ERROR(cdev, "%s: can't autoconfigure on %s\n",
			f->name, cdev->gadget->name);
		return -ENODEV;
	}

    st_bridge_dev.ep_in = loop->in_ep;

	loop->out_ep = usb_ep_autoconfig(cdev->gadget, &fs_loop_sink_desc);
	if (!loop->out_ep)
		goto autoconf_fail;
    st_bridge_dev.ep_out = loop->out_ep;

	/* support high speed hardware */
	hs_loop_source_desc.bEndpointAddress =
		fs_loop_source_desc.bEndpointAddress;
	hs_loop_sink_desc.bEndpointAddress = fs_loop_sink_desc.bEndpointAddress;

	/* support super speed hardware */
	ss_loop_source_desc.bEndpointAddress =
		fs_loop_source_desc.bEndpointAddress;
	ss_loop_sink_desc.bEndpointAddress = fs_loop_sink_desc.bEndpointAddress;

	ret = usb_assign_descriptors(f, fs_loopback_descs, hs_loopback_descs,
			ss_loopback_descs, NULL);
	if (ret)
		return ret;

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
	    (gadget_is_superspeed(c->cdev->gadget) ? "super" :
	     (gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full")),
			f->name, loop->in_ep->name, loop->out_ep->name);
	return 0;
}

static void lb_free_func(struct usb_function *f)
{
	struct f_lb_opts *opts;

	opts = container_of(f->fi, struct f_lb_opts, func_inst);

	mutex_lock(&opts->lock);
	opts->refcnt--;
	mutex_unlock(&opts->lock);

	usb_free_all_descriptors(f);
	kfree(func_to_loop(f));
}

static inline int bridge_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else{
		atomic_dec(excl);
		return -1;
	}
}

static inline void bridge_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static void loopback_complete_out(struct usb_ep *ep, struct usb_request *req)
{
    int ret = 0;
	struct f_loopback	*loop = ep->driver_data;
	struct usb_composite_dev *cdev = loop->function.config->cdev;
	int			status = req->status;

	switch (status) {
	case 0:				/* normal completion? */
		if (ep == loop->out_ep) {
			/*
			 * We received some data from the host so let's
			 * queue it so host can read the from our in ep
			 */
            st_bridge_dev.rx_done = 1;
            // printk("(%d) complete is finish!\n", __LINE__);
            wake_up(&st_bridge_dev.read_wq);
            
		} else {
			/*
			 * We have just looped back a bunch of data
			 * to host. Now let's wait for some more data.
			 */
			//req = req->context;
			//ep = loop->out_ep;

			//bridge_unlock(&st_bridge_dev.write_excl);
            // printk("(%d) complete is finish!\n", __LINE__);
            wake_up(&st_bridge_dev.write_wq);
		}
		break;
		/* "should never get here" */
	default:
		ERROR(cdev, "%s loop complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		/* FALLTHROUGH */

	/* NOTE:  since this driver doesn't maintain an explicit record
	 * of requests it submitted (just maintains qlen count), we
	 * rely on the hardware driver to clean up on disconnect or
	 * endpoint disable.
	 */
	case -ECONNABORTED:		/* hardware forced ep reset */
	case -ECONNRESET:		/* request dequeued */
	case -ESHUTDOWN:		/* disconnect from host */
free_req:
		usb_ep_free_request(ep == loop->in_ep ?
				    loop->out_ep : loop->in_ep,
				    req->context);
		free_ep_req(ep, req);
		return;
	}
}

static void loopback_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct f_loopback	*loop = ep->driver_data;
	struct usb_composite_dev *cdev = loop->function.config->cdev;
	int			status = req->status;

	switch (status) {
	case 0:				/* normal completion? */
		if (ep == loop->out_ep) {
			/*
			 * We received some data from the host so let's
			 * queue it so host can read the from our in ep
			 */

            //printk("ep is out,buf(%d) = %s\n", req->length, (char *)req->buf);
            wake_up(&st_bridge_dev.read_wq);
            
		} else {
			/*
			 * We have just looped back a bunch of data
			 * to host. Now let's wait for some more data.
			 */
			//req = req->context;
			//ep = loop->out_ep;

			//bridge_unlock(&st_bridge_dev.write_excl);
            //printk("complete is finish!\n");
            st_bridge_dev.tx_done = 1;
            st_bridge_dev.tx_is_idle = 1;
            wake_up(&st_bridge_dev.write_wq);

		}
		break;
		/* "should never get here" */
	default:
		ERROR(cdev, "%s loop complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		/* FALLTHROUGH */

	/* NOTE:  since this driver doesn't maintain an explicit record
	 * of requests it submitted (just maintains qlen count), we
	 * rely on the hardware driver to clean up on disconnect or
	 * endpoint disable.
	 */
	case -ECONNABORTED:		/* hardware forced ep reset */
	case -ECONNRESET:		/* request dequeued */
	case -ESHUTDOWN:		/* disconnect from host */
free_req:
		usb_ep_free_request(ep == loop->in_ep ?
				    loop->out_ep : loop->in_ep,
				    req->context);
		free_ep_req(ep, req);
		return;
	}
}

static void disable_loopback(struct f_loopback *loop)
{
	struct usb_composite_dev	*cdev;

	cdev = loop->function.config->cdev;
	//disable_endpoints(cdev, loop->in_ep, loop->out_ep, NULL, NULL);
	usb_ep_disable(loop->in_ep);
	usb_ep_disable(loop->out_ep);
	VDBG(cdev, "%s disabled\n", loop->function.name);
}

static inline struct usb_request *lb_alloc_ep_req(struct usb_ep *ep, int len)
{
	return alloc_ep_req(ep, len);
}

static int alloc_requests(struct usb_composite_dev *cdev,
			  struct f_loopback *loop)
{
	struct usb_request *in_req, *out_req;
	int i;
	int result = 0;

	/*
	 * allocate a bunch of read buffers and queue them all at once.
	 * we buffer at most 'qlen' transfers; We allocate buffers only
	 * for out transfer and reuse them in IN transfers to implement
	 * our loopback functionality
	 */
	for (i = 0; i < loop->qlen && result == 0; i++) {
		result = -ENOMEM;

        printk("buflen = %d\n", loop->buflen);
		in_req = lb_alloc_ep_req(loop->in_ep, loop->buflen);
		if (!in_req)
			goto fail;

		out_req = lb_alloc_ep_req(loop->out_ep, loop->buflen);
		if (!out_req)
			goto fail_in;

		in_req->complete = loopback_complete_in;
		out_req->complete = loopback_complete_out;

#if 0
		in_req->buf = out_req->buf;

		/* length will be set in complete routine */
		in_req->context = out_req;
		out_req->context = in_req;
#endif
        st_bridge_dev.req_in  = in_req;
        st_bridge_dev.req_out = out_req;
#if 0
		result = usb_ep_queue(st_bridge_dev.ep_out, st_bridge_dev.req_out, GFP_ATOMIC);
		if (result) {
			ERROR(cdev, "%s queue req --> %d\n",
					loop->out_ep->name, result);
			goto fail_out;
		}
#endif
	}

	return 0;

fail_out:
	free_ep_req(loop->out_ep, out_req);
fail_in:
	usb_ep_free_request(loop->in_ep, in_req);
fail:
	return result;
}

static int enable_endpoint(struct usb_composite_dev *cdev,
			   struct f_loopback *loop, struct usb_ep *ep)
{
	int					result;

	result = config_ep_by_speed(cdev->gadget, &(loop->function), ep);
	if (result)
		goto out;

	result = usb_ep_enable(ep);
	if (result < 0)
		goto out;
	ep->driver_data = loop;
	result = 0;

out:
	return result;
}

static int
enable_loopback(struct usb_composite_dev *cdev, struct f_loopback *loop)
{
	int					result = 0;

	result = enable_endpoint(cdev, loop, loop->in_ep);
	if (result)
		goto out;

	result = enable_endpoint(cdev, loop, loop->out_ep);
	if (result)
		goto disable_in;

	result = alloc_requests(cdev, loop);
	if (result)
		goto disable_out;

	DBG(cdev, "%s enabled\n", loop->function.name);
	return 0;

disable_out:
	usb_ep_disable(loop->out_ep);
disable_in:
	usb_ep_disable(loop->in_ep);
out:
	return result;
}

static int loopback_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct f_loopback	*loop = func_to_loop(f);
	struct usb_composite_dev *cdev = f->config->cdev;

    // printk("%s(%d)\n", __func__, __LINE__);

    // if(st_bridge_dev.is_online) {
    //     return 0;
    // }

    st_bridge_dev.is_online = 1;

	/* we know alt is zero */
	disable_loopback(loop);
	return enable_loopback(cdev, loop);
}

static void loopback_disable(struct usb_function *f)
{
	struct f_loopback	*loop = func_to_loop(f);

    // printk("%s(%d)\n", __func__, __LINE__);

	disable_loopback(loop);
}

static int bridge_open(struct inode *ip, struct file *fp)
{
    if(1 != st_bridge_dev.is_online) {
        printk("usb is not online!\n");
        return -EIO;
    }

    return 0;
}

static ssize_t bridge_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
    uint32_t ret = 0;
    struct usb_request *req;

    if(1 != st_bridge_dev.is_online) {
        printk("usb is not online!\n");
        return -EIO;
    }

    req = st_bridge_dev.req_out;

    ret = usb_ep_queue(st_bridge_dev.ep_out, st_bridge_dev.req_out, GFP_ATOMIC);
    if (ret < 0) 
    {
        printk("tmc_read: failed to queue req %p (%d)\n", req, ret);
        ret = -EIO;
    }

    ret = wait_event_interruptible(st_bridge_dev.read_wq, st_bridge_dev.rx_done);
    if(ret < 0) {
        printk("%s[%d].wait event interrupt!\n", __func__, __LINE__);
        goto out;
    }

    copy_to_user(buf, (char *)(req->buf), req->actual);
    ret = req->actual;

    //printk("len = %d. buf = 0x%x\n", req->actual, (char *)(req->buf));

out:
    st_bridge_dev.rx_done = 0;
    return ret;
}

static ssize_t bridge_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
    int ret = 0;

    if(1 != st_bridge_dev.is_online) {
        printk("usb is not online!\n");
        return -EIO;
    }

    if(1 != st_bridge_dev.tx_is_idle) {
        printk("waiting for tx is finished!\n");
        return -EBUSY;
    }

    if(count > (16 * 1024)) {
        printk("data package len is over 16KB!\n");
        return -EINVAL;
    }

#if 1
    if (bridge_lock(&st_bridge_dev.write_excl)) {
		printk("locked!\n");
		return -EBUSY;
	}
#endif

    st_bridge_dev.tx_is_idle = 0;

    ret = copy_from_user(st_bridge_dev.req_in->buf, buf, count);
    if(ret < 0) {
        printk("copy from user failed!\n");
        return -EINVAL;
    }
    st_bridge_dev.req_in->length = count;
    st_bridge_dev.req_in->actual = count;

    //printk("write queue begin!\n");
    usb_ep_queue(st_bridge_dev.ep_in, st_bridge_dev.req_in, GFP_ATOMIC);
    //printk("write queue end!\n");

    ret = wait_event_interruptible(st_bridge_dev.write_wq, st_bridge_dev.tx_done);
    if(ret < 0) {
        printk("%s[%d].wait event interruptible failed!\n", __func__, __LINE__);
    }

	//printk("write finish!\n");
    st_bridge_dev.tx_done = 0;
    bridge_unlock(&st_bridge_dev.write_excl);

    return ret;
}

static const struct file_operations bridge_fops = 
{
	.owner = THIS_MODULE,
	.read = bridge_read,
	.write = bridge_write,
	.open = bridge_open,
    //.unlocked_ioctl	= tmc_ioctl,
	//.release = tmc_release,
	//.poll    = tmc_poll,  
};

static struct miscdevice bridge_device = 
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "usb_bridge",
	.fops = &bridge_fops,
};

static struct usb_function *loopback_alloc(struct usb_function_instance *fi)
{
    int ret = 0;
	struct f_loopback	*loop;
	struct f_lb_opts	*lb_opts;

	printk("************************************\n");

	loop = kzalloc(sizeof *loop, GFP_KERNEL);
	if (!loop)
		return ERR_PTR(-ENOMEM);

	lb_opts = container_of(fi, struct f_lb_opts, func_inst);

	mutex_lock(&lb_opts->lock);
	lb_opts->refcnt++;
	mutex_unlock(&lb_opts->lock);

	loop->buflen = lb_opts->bulk_buflen;
	loop->qlen = lb_opts->qlen;
	if (!loop->qlen)
		loop->qlen = 32;

	loop->function.name = "bridge";
	loop->function.bind = loopback_bind;
	loop->function.set_alt = loopback_set_alt;
	loop->function.disable = loopback_disable;
	loop->function.strings = loopback_strings;

	loop->function.free_func = lb_free_func;

    init_waitqueue_head(&st_bridge_dev.write_wq);
    init_waitqueue_head(&st_bridge_dev.read_wq);

    st_bridge_dev.rx_done = 0;
    st_bridge_dev.tx_done = 0;

    st_bridge_dev.tx_is_idle = 1;
    st_bridge_dev.is_online = 0;

    atomic_set(&st_bridge_dev.write_excl, 0);
    atomic_set(&st_bridge_dev.read_excl, 0);

    ret = misc_register(&bridge_device);
	if (ret)
		return ERR_PTR(-EINVAL);

	return &loop->function;
}

static inline struct f_lb_opts *to_f_lb_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_lb_opts,
			    func_inst.group);
}

static void lb_attr_release(struct config_item *item)
{
	struct f_lb_opts *lb_opts = to_f_lb_opts(item);

	usb_put_function_instance(&lb_opts->func_inst);
}

static struct configfs_item_operations lb_item_ops = {
	.release		= lb_attr_release,
};

static ssize_t f_lb_opts_qlen_show(struct config_item *item, char *page)
{
	struct f_lb_opts *opts = to_f_lb_opts(item);
	int result;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%d\n", opts->qlen);
	mutex_unlock(&opts->lock);

	return result;
}

static ssize_t f_lb_opts_qlen_store(struct config_item *item,
				    const char *page, size_t len)
{
	struct f_lb_opts *opts = to_f_lb_opts(item);
	int ret;
	u32 num;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = kstrtou32(page, 0, &num);
	if (ret)
		goto end;

	opts->qlen = num;
	ret = len;
end:
	mutex_unlock(&opts->lock);
	return ret;
}

CONFIGFS_ATTR(f_lb_opts_, qlen);

static ssize_t f_lb_opts_bulk_buflen_show(struct config_item *item, char *page)
{
	struct f_lb_opts *opts = to_f_lb_opts(item);
	int result;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%d\n", opts->bulk_buflen);
	mutex_unlock(&opts->lock);

	return result;
}

static ssize_t f_lb_opts_bulk_buflen_store(struct config_item *item,
				    const char *page, size_t len)
{
	struct f_lb_opts *opts = to_f_lb_opts(item);
	int ret;
	u32 num;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = kstrtou32(page, 0, &num);
	if (ret)
		goto end;

	opts->bulk_buflen = num;
	ret = len;
end:
	mutex_unlock(&opts->lock);
	return ret;
}

CONFIGFS_ATTR(f_lb_opts_, bulk_buflen);

static struct configfs_attribute *lb_attrs[] = {
	&f_lb_opts_attr_qlen,
	&f_lb_opts_attr_bulk_buflen,
	NULL,
};

static const struct config_item_type lb_func_type = {
	.ct_item_ops    = &lb_item_ops,
	.ct_attrs	= lb_attrs,
	.ct_owner       = THIS_MODULE,
};

static void lb_free_instance(struct usb_function_instance *fi)
{
	struct f_lb_opts *lb_opts;

	lb_opts = container_of(fi, struct f_lb_opts, func_inst);
	kfree(lb_opts);
}

static struct usb_function_instance *loopback_alloc_instance(void)
{
	struct f_lb_opts *lb_opts;

	lb_opts = kzalloc(sizeof(*lb_opts), GFP_KERNEL);
	if (!lb_opts)
		return ERR_PTR(-ENOMEM);
	mutex_init(&lb_opts->lock);
	lb_opts->func_inst.free_func_inst = lb_free_instance;
	lb_opts->bulk_buflen = GZERO_BULK_BUFLEN;
	lb_opts->qlen = GZERO_QLEN;

	config_group_init_type_name(&lb_opts->func_inst.group, "",
				    &lb_func_type);

	return  &lb_opts->func_inst;
}
DECLARE_USB_FUNCTION_INIT(Loopback, loopback_alloc_instance, loopback_alloc);

#if 0
int __init lb_modinit(void)
{
	return usb_function_register(&Loopbackusb_func);
}

void __exit lb_modexit(void)
{
	usb_function_unregister(&Loopbackusb_func);
}
#endif
MODULE_LICENSE("GPL");
