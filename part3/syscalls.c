#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

// sys calls stubs
int (*STUB_start_elevator)(void) = 548;
int (*STUB_issue_request)(int, int, int) = 549;
int (*STUB_stop_elevator)(void) = 550;
EXPORT_SYMBOL(STUB_start_elevator);
EXPORT_SYMBOL(STUB_issue_request);
EXPORT_SYMBOL(STUB_stop_elevator);


// sys call wrappers
SYSCALL_DEFINE0(start_elevator) {
	printk(KERN_NOTICE "Inside start_elevator block. %s", __FUNCTION__);
	if (STUB_start_elevator != 548)
		return STUB_start_elevator();
	else
		return -ENOSYS;
}

SYSCALL_DEFINE3(issue_request, int, start_floor, int, destination_floor, int, type) {
	printk(KERN_NOTICE "Inside issue_request block. %s: The int are %d, %d, %d", __FUNCTION__, start_floor, destination_floor, type);
        if (STUB_issue_request != 549)
                return STUB_issue_request(start_floor, destination_floor, type);
        else
                return -ENOSYS;
}

SYSCALL_DEFINE0(stop_elevator) {
        printk(KERN_NOTICE "Inside stop_elevator block. %s", __FUNCTION__);
        if (STUB_stop_elevator != 550)
                return STUB_stop_elevator();
        else
                return -ENOSYS;
}
