#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group-31");
MODULE_DESCRIPTION("TImer kernel module");

#define ENTRY_NAME "timer"
#define PERMS 0644
#define PARENT NULL

static struct proc_dir_entry* timer_entry;

static struct timespec64 last_read_time;

static ssize_t timer_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    struct timespec64 ts_now;
    char buf[256];
    int len = 0;
    long long elapsed_sec, elapsed_nsec;

    ktime_get_real_ts64(&ts_now);

    elapsed_sec = ts_now.tv_sec - last_read_time.tv_sec;
    elapsed_nsec = ts_now.tv_nsec - last_read_time.tv_nsec;

    if (elapsed_nsec < 0) {
        elapsed_sec--;
        elapsed_nsec += 1000000000;
    }

    len = snprintf(buf, sizeof(buf), "current time: %lld\n", (long long)ts_now.tv_sec);
    len += snprintf(buf + len, sizeof(buf) - len, "elapsed time: %lld.%09lld\n", elapsed_sec, elapsed_nsec);

    // Update last read time
    last_read_time = ts_now;

    return simple_read_from_buffer(ubuf, count, ppos, buf, len); // better than copy_from_user
}

static const struct proc_ops timer_fops = {
    .proc_read = timer_read,
};

static int __init timer_init(void)
{
    timer_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &timer_fops);
    if (!timer_entry) {
	printk(KERN_ERR "Failed to crate /proc/timer entry\n");
        return -ENOMEM;
    }
    return 0;
}

static void __exit timer_exit(void)
{
    proc_remove(timer_entry);
}

module_init(timer_init);
module_exit(timer_exit);
