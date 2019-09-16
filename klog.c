#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/syslog.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <linux/uaccess.h>
#include <asm/io.h>

#define KLOG_BULK_SIZE 1024

static DECLARE_WAIT_QUEUE_HEAD(tos_wait);
static char * log_buf;
static char * log_head;
static char * log_tail;
static int logbuf_size;

int logbuf_init(size_t size)
{
    int ret = 0;
    printk("logbuf init start!");
    
    log_buf = kmalloc(KLOG_BULK_SIZE * size, GFP_KERNEL|__GFP_ZERO);
    if(!log_buf){
		printk("alloc pages is failed! \n");
		ret = -1;
        goto err;
	}
    log_head = log_tail = log_buf;
    printk("log_head:%p---log_tail:%p---log_buf:%p", log_head, log_tail, log_buf);
    logbuf_size = KLOG_BULK_SIZE * size - 1;

err:
    return ret;
}

int tos_add_log(char *buf, int len)
{
    u64 remain = (logbuf_size - (log_head - log_buf));
    if(len > remain)
    {
        memcpy(log_head, buf, remain);
        memcpy(log_buf, buf + remain, len - remain);
        log_head = log_buf + len - remain;
        if (log_head > log_tail)
            log_tail = log_head + 1;
    }
    else
    {
        memcpy(log_head,buf,len);
        log_head += len;
    }

    if (log_head != log_tail)
        wake_up_interruptible(&tos_wait);
    return len;
}

asmlinkage int _vprintk_emit(const char *fmt, va_list args)
{
    int this_cpu;
	static char textbuf[1024];
	char *text = textbuf;
	size_t text_len;
	
	//unsigned long flags;
	//local_irq_save(flags);
	this_cpu = smp_processor_id();
	text_len = vscnprintf(text, sizeof(textbuf), fmt, args);
	tos_add_log(text, text_len);
	return text_len;
}

int tos_log(const char *fmt, ...)
{
    int r;
	va_list args;
	va_start(args, fmt);
	r = _vprintk_emit(fmt, args);
	va_end(args);
	return r;
}
EXPORT_SYMBOL(tos_log);

int tos_read_log(char __user *buf, size_t count)
{
    char * text;
	int len = 0;

    if (log_head < log_tail) {
        len = (log_buf + logbuf_size) - log_tail + log_head - log_buf;
        text = kmalloc(len, GFP_KERNEL|__GFP_ZERO);
        if (!text)
            return -ENOMEM;

        memcpy(text, log_tail, log_buf + logbuf_size - log_tail);
        memcpy(text + (log_buf + logbuf_size - log_tail), log_buf, log_head - log_buf);
    } else {
        len = log_head - log_tail;
        text = kmalloc(len, GFP_KERNEL|__GFP_ZERO);
        memcpy(text, log_tail, len);
    }

    if (copy_to_user(buf, text, len)) {
        if (!len)
            len = -EFAULT;
        goto err;
	}
    log_tail = log_head;
err:
    if (text)
        kfree(text);
    return len;
}

static int toskmsg_open(struct inode * inode, struct file * file)
{
    return 0;
}

static int toskmsg_release(struct inode * inode, struct file * file)
{
	return 0;
}

static ssize_t toskmsg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    if (file->f_flags & O_NONBLOCK && log_head == log_tail)
	    return -EAGAIN;

    if (!buf || count < 0)
		return -EINVAL;
	if (!count)
		return 0;

    ret = wait_event_interruptible(tos_wait, log_head != log_tail);
    if(ret)
        return ret;

    ret = tos_read_log(buf, count);

    return ret;
}

static unsigned int toskmsg_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &tos_wait, wait);
	if (log_head != log_tail)
		return POLLIN | POLLRDNORM;
	return 0;
}

static const struct file_operations proc_toskmsg_operations = {
	.read		= toskmsg_read,
	.poll		= toskmsg_poll,
	.open		= toskmsg_open,
	.release	= toskmsg_release,
	.llseek		= generic_file_llseek,
};

static __init int proc_toskmsg_init(void)
{
	proc_create("toskmsg", S_IRUSR, NULL, &proc_toskmsg_operations);
    logbuf_init(1);
	return 0;
}

__exit void proc_toskmsg_exit(void)
{
    if (log_buf) kfree(log_buf);
    remove_proc_entry("toskmsg", NULL);
}
module_init(proc_toskmsg_init);
module_exit(proc_toskmsg_exit);
MODULE_LICENSE("GPL");


