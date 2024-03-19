#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group-31");
MODULE_DESCRIPTION("Elevator kernel module");
MODULE_VERSION("0.1");

#define ENTRY_NAME "elevator"
#define PERMS 0644
#define PARENT NULL

#define MAX_FLOORS 5
#define MAX_PASSENGERS 5
#define MAX_WEIGHT 7

struct passenger {
	char type;
	int destination;
	struct list_head list;
} Item;

struct floor {
	int floor_number;
	int passengers_waiting;
	struct list_head passenger_list;
	struct mutex floor_mutex;
};

enum state {
	OFFLINE,
	IDLE,
	LOADING,
	UP,
	DOWN
};

struct elevator {
	enum state state;
	int current_floor;
	int current_load;
	struct list_head passenger_list;
	struct task_struct *thread;
	struct mutex mutex;
};

static struct proc_dir_entry* elevator_entry;
static struct elevator my_elevator;
static struct floor floors[MAX_FLOORS];

int start_elevator(void);                                                           // starts the elevator to pick up and drop off passengers
int issue_request(int start_floor, int destination_floor, int type);                // add passengers requests to specific floors
int stop_elevator(void);                                                            // stops the elevator

extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int,int,int);
extern int (*STUB_stop_elevator)(void);

int start_elevator(void) {
   	if (my_elevator.state != OFFLINE)
        	return 1;

    	my_elevator.state = IDLE;
    	my_elevator.current_floor = 1;
    	my_elevator.current_load = 0;
    	return 0;
}

int issue_request(int start_floor, int destination_floor, int type) {
	if (start_floor < 1 || start_floor > MAX_FLOORS ||
        destination_floor < 1 || destination_floor > MAX_FLOORS ||
        type < 0 || type > 3)
        	return 1;
        	
        struct passenger *new_passenger = kmalloc(sizeof(struct passenger), GFP_KERNEL);
    	if (!new_passenger)
        	return -ENOMEM;

    	new_passenger->type = type;
    	new_passenger->destination = destination_floor;

    	mutex_lock(&floors[start_floor - 1].floor_mutex);
    	list_add_tail(&new_passenger->list, &floors[start_floor - 1].passenger_list);
    	floors[start_floor - 1].passengers_waiting++;
    	mutex_unlock(&floors[start_floor - 1].floor_mutex);

    	return 0;
}

int stop_elevator(void) {
    	return 0;
}

static int elevator_run(void *data)
{
	start_elevator();
	ssleep(10);
	while (!kthread_should_stop()) {
		switch (my_elevator.state) {
        	case IDLE:
			if (floors[my_elevator.current_floor].passengers_waiting > 0) 
				my_elevator.state = LOADING;
			else
				my_elevator.state = UP;
			break;
		
		case LOADING:
			ssleep(1);
			if (my_elevator.current_load < MAX_WEIGHT) {
				// add to elevator list
				// remove from floor list
				// update elevator load
				my_elevator.state = UP;
				my_elevator.current_floor++;
			}
			break;
			
		case UP:
			my_elevator.current_floor++;
			ssleep(2);
			if (my_elevator.current_floor < MAX_FLOORS)
				my_elevator.state = IDLE;
			else
				my_elevator.state = DOWN; 
			break;

		
		case DOWN:
			my_elevator.current_floor--;
			ssleep(2);
			if (my_elevator.current_floor > 1)
				my_elevator.state = IDLE;
			else
				my_elevator.state = DOWN; 
			break;
		}
	}
	return 0;
}
	
static ssize_t elevator_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    	char buf[256];
    	int len = 0;
    	
    	switch (my_elevator.state) {
        case OFFLINE:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: OFFLINE\n");
            break;
        case IDLE:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: IDLE\n");
            break;
        case LOADING:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: LOADING\n");
            break;
        case UP:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: UP\n");
            break;
        case DOWN:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: DOWN\n");
            break;
        default:
            len += snprintf(buf + len, sizeof(buf) - len, "Elevator state: UNKNOWN\n");
            break;
    }
    
    	len += snprintf(buf + len, sizeof(buf) - len, "Current floor: %d\n", my_elevator.current_floor);
  /*   	
    	len += snprintf(buf + len, sizeof(buf) - len, "Current load: %f lbs\n", my_elevator.current_load);
   
    	len += snprintf(buf + len, sizeof(buf) - len, "Elevator status: ");
    	struct passenger *passenger_ptr;
    	list_for_each_entry(passenger_ptr, &my_elevator.passenger_list, list) {
        	len += snprintf(buf + len, sizeof(buf) - len, "%c%d ", passenger_ptr->type, passenger_ptr->destination);
    	}
    	len += snprintf(buf + len, sizeof(buf) - len, "\n");

    	for (int i = 0; i < MAX_FLOORS; ++i) {
        	mutex_lock(&floors[i].floor_mutex);
        	len += snprintf(buf + len, sizeof(buf) - len, "[%s] Floor %d: %d", (my_elevator.current_floor == floors[i].floor_number) ? "*" : " ", floors[i].floor_number, floors[i].passengers_waiting);
        	struct passenger *floor_passenger_ptr;
        	list_for_each_entry(floor_passenger_ptr, &floors[i].passenger_list, list) {
            len += snprintf(buf + len, sizeof(buf) - len, " %c%d", floor_passenger_ptr->type, floor_passenger_ptr->destination);
        	}
        	len += snprintf(buf + len, sizeof(buf) - len, "\n");
        	mutex_unlock(&floors[i].floor_mutex);
    	}
  */  
    	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct proc_ops elevator_fops = {
    	.proc_read = elevator_read,
};

static int __init elevator_init(void) {

    	STUB_start_elevator = start_elevator;
	STUB_issue_request = issue_request;
	STUB_stop_elevator = stop_elevator;
	
	// Create elevator thread
    	my_elevator.thread = kthread_run(elevator_run, NULL, "elevator_thread");
    	if (IS_ERR(my_elevator.thread)) {
        	printk(KERN_ERR "Failed to create elevator thread\n");
        	return PTR_ERR(my_elevator.thread);
    	}
	
	// Create proc entry
    	elevator_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &elevator_fops);
    	if (!elevator_entry) {
        	printk(KERN_ERR "Failed to create /proc/elevator entry\n");
        	return -ENOMEM;
    	}
    	return 0;  // Return 0 to indicate successful loading
}

static void __exit elevator_exit(void) {
    	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;
}

module_init(elevator_init);  // Specify the initialization function
module_exit(elevator_exit);  // Specify the exit/cleanup function
