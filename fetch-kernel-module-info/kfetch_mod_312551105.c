
#include <asm/errno.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h> /* for sprintf() */
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched.h> /* for_each_process */
#include <linux/sched/signal.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/utsname.h> // for utsname() -> release, nodename
#include <linux/version.h>

#define SUCCESS 0
#define DEVICE_NAME "kfetch" /* Dev name as it appears in /proc/devices   */
#define KFETCH_BUF_SIZE 1024 /* Max length of the message from the device */

#define KFETCH_NUM_INFO 6
#define KFETCH_RELEASE (1 << 0)
#define KFETCH_NUM_CPUS (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM (1 << 3)
#define KFETCH_UPTIME (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)

/* Global variables are declared as static, so are global within the file. */

static char kfetch_buf[KFETCH_BUF_SIZE];

static int kfetch_mask = 0;

static int major; /* major number assigned to our device driver */

/* Is device open? Used to prevent multiple access to device */
static bool already_open = false;

static struct class *cls;

/*  Prototypes */
static int kfetch_init(void);
static void kfetch_exit(void);
static ssize_t kfetch_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t kfetch_write(struct file *, const char __user *, size_t, loff_t *);
static int kfetch_open(struct inode *, struct file *);
static int kfetch_release(struct inode *, struct file *);

const static struct file_operations kfetch_ops = {
    .owner = THIS_MODULE,
    .read = kfetch_read,
    .write = kfetch_write,
    .open = kfetch_open,
    .release = kfetch_release,
};

// kfetch_init is called when the module is loaded
static int kfetch_init(void) {
    major = register_chrdev(0, DEVICE_NAME, &kfetch_ops);

    if (major < 0) {
        pr_alert("Registering char device failed with %d\n", major);
        return major;
    }

    pr_info("I was assigned major number %d.\n", major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_NAME); // we are here: kernel version 5.19.12
#endif
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    // when the module is loaded,
    // the first invocation without any options will display all the information.
    kfetch_mask = ((1 << KFETCH_NUM_INFO) - 1);

    return SUCCESS;
}

// kfetch_exit is called when the module is unloaded
static void kfetch_exit(void) {
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);

    /* Unregister the device */
    unregister_chrdev(major, DEVICE_NAME);
}

/* Methods */

/* Called when a process tries to open the device file.
   - set up protections
 */
static int kfetch_open(struct inode *inode, struct file *file) {

    if (already_open)
        return -EBUSY;

    already_open = true;
    try_module_get(THIS_MODULE); // increments the module's reference count

    return SUCCESS;
}

/* Called when a process closes the device file.
    - clean up protections
*/
static int kfetch_release(struct inode *inode, struct file *file) {

    /* We're now ready for our next caller */
    already_open = false;

    /* Decrement the usage count, or else once you opened the file, you will
     * never get rid of the module.
     */
    module_put(THIS_MODULE);

    return SUCCESS;
}

/*   Format for printing text in color is \033[1;<color code>m,
 *   - \033 -> escape character
 *   - [1;<color code>m -> code for setting the text color.
 *   
 *   - \033[1;33m -> sets the text color to yellow
 *   - \033[1;0m -> resets the text color to default
 */
char *logo[8] = {
    "                      ",
    "         .-.          ",
    "        (.. |         ",
    "       \033[1;33m <> \033[1;0m |         ",
    "       / --- \\        ",
    "      ( |   | |       ",
    "    \033[1;33m|\\\033[1;0m\\_)___/\\)\033[1;33m/\\ \033[1;0m    ",
    "   \033[1;33m<__)\033[1;0m------\033[1;33m(__/\033[1;0m     ",
};

/* Called when a process, which already opened the dev file, attempts to read from it.
 * return logo & information
 *  - first line: machine hostname (mandatory)
 *  - next line: separator line with a length equal to the hostname
 *  - The remaining lines depend on the information mask.
 */
static ssize_t kfetch_read(struct file *filp,   /* see include/linux/fs.h   */
                           char __user *buffer, /* buffer to fill with data */
                           size_t length,       /* length of the buffer     */
                           loff_t *offset) {
    char *machine_hostname;
    char *kernel_release;
    char split_line[30];

    char *cpu_model_name;
    int online_cpus;
    int total_cpus;

    unsigned long free_memory;
    unsigned long total_memory;

    int num_procs = 0;
    long uptime;

    /* ///////////////////  fetching the information ////////////////  */

    // machine_hostname, kernel_release, split_line
    machine_hostname = utsname()->nodename;
    kernel_release = utsname()->release;

    int sl = 0;
    for (; machine_hostname[sl] != '\0'; sl++) {
        split_line[sl] = '-';
    }
    split_line[sl] = '\0';

    // cpu_model_name, online_cpus, total_cpus
    struct cpuinfo_x86 *c = &cpu_data(0);
    cpu_model_name = c->x86_model_id;
    online_cpus = num_online_cpus();
    total_cpus = num_active_cpus();

    printk("cpu_model_name: %s\n", cpu_model_name);
    printk("online_cpus: %d\n", online_cpus);
    printk("total_cpus: %d\n", total_cpus);

    // free_memory, total_memory
    struct sysinfo si;
    si_meminfo(&si);
    free_memory = si.freeram * PAGE_SIZE / 1024 / 1024; // in MB
    total_memory = si.totalram * PAGE_SIZE / 1024 / 1024;

    // num_procs
    struct task_struct *task_list;
    for_each_process(task_list) {
        num_procs++;
    }

    // uptime
    s64 uptime_temp;
    uptime_temp = ktime_divns(ktime_get_coarse_boottime(), NSEC_PER_SEC);
    uptime = uptime_temp / 60; // in minutes

    /* ///////////////////  fetching the information end ////////////////  */

    char info_list[8][64];
    bool contain_info[8] = {true, true, false, false, false, false, false, false};

    strcpy(info_list[0], machine_hostname);
    strcpy(info_list[1], split_line);

    char buf[64] = {0};

    if (kfetch_mask & KFETCH_RELEASE) {
        contain_info[2] = true;
        sprintf(buf, "\033[1;33mKernal:\033[1;0m %s", kernel_release);
        strcpy(info_list[2], buf);
        // printk("info_list[2]: %s\n", info_list[2]);
    }
    if (kfetch_mask & KFETCH_CPU_MODEL) {
        contain_info[3] = true;
        sprintf(buf, "\033[1;33mCPU:\033[1;0m    %s", cpu_model_name);
        strcpy(info_list[3], buf);
        // printk("info_list[3]: %s\n", info_list[3]);
    }
    if (kfetch_mask & KFETCH_NUM_CPUS) {
        contain_info[4] = true;
        sprintf(buf, "\033[1;33mCPUs:\033[1;0m   %d / %d", online_cpus, total_cpus);
        strcpy(info_list[4], buf);
        // printk("info_list[4]: %s\n", info_list[4]);
    }
    if (kfetch_mask & KFETCH_MEM) {
        contain_info[5] = true;
        sprintf(buf, "\033[1;33mMem:\033[1;0m    %ld / %ld MB", free_memory, total_memory);
        strcpy(info_list[5], buf);
        // printk("info_list[5]: %s\n", info_list[5]);
    }
    if (kfetch_mask & KFETCH_NUM_PROCS) {
        contain_info[6] = true;
        sprintf(buf, "\033[1;33mProcs:\033[1;0m  %d", num_procs);
        strcpy(info_list[6], buf);
        // printk("info_list[6]: %s\n", info_list[6]);
    }
    if (kfetch_mask & KFETCH_UPTIME) {
        contain_info[7] = true;
        sprintf(buf, "\033[1;33mUptime:\033[1;0m %ld mins", uptime);
        strcpy(info_list[7], buf);
        // printk("info_list[7]: %s\n", info_list[7]);
    }

    strcpy(kfetch_buf, "");

    int j = 0;
    for (int i = 0; i < 8; i++) {
        strcat(kfetch_buf, logo[i]);

        while (j < 8) {
            if (contain_info[j]) {
                strcat(kfetch_buf, info_list[j]);
                j++;
                break;
            }
            j++;
        }

        strcat(kfetch_buf, "\n");
    }

    if (copy_to_user(buffer, kfetch_buf, sizeof(kfetch_buf))) {
        pr_alert("Failed to copy data to user");
        return 0;
    }

    return sizeof(kfetch_buf);
    
    /* cleaning up */

    // no heap memory allocated, no need to free    
}

/* Called when a process writes to dev file.
 * - should set the information mask in the module
 * - which determines what data is returned by the read operation.
 */
static ssize_t kfetch_write(struct file *filp, const char __user *buffer,
                            size_t length, loff_t *offset) {
    int mask_info;

    // copy from user space buffer to mask_info with length
    if (copy_from_user(&mask_info, buffer, length)) {
        pr_alert("Failed to copy data from user");
        return 0;
    }

    // set the information mask
    kfetch_mask = mask_info;

    return SUCCESS;
}

module_init(kfetch_init);
module_exit(kfetch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("312551105");
MODULE_DESCRIPTION("Fetch kernel module information");