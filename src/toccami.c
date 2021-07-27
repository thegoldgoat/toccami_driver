#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "toccamich"
#define CLASS_NAME "toccami"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrea Somaini");
MODULE_DESCRIPTION("Virtual Touchpad driver for Toccami");
MODULE_VERSION("1.0");

static int majorNumber;
static int numberOpens = 0;
static struct class *toccamiClass = NULL;
static struct device *toccamiDevice = NULL;

static DEFINE_MUTEX(toccamiMutex);

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static struct input_dev *toccamiInput;

#define AXIS_X_MIN 0
#define AXIS_Y_MIN 0
#define AXIS_X_MAX 1000
#define AXIS_Y_MAX 400
#define MAX_TOUCHES 10

#define TEMP_RESOLUTION_X 10
#define TEMP_RESOLUTION_Y 10

#define EVENT_PER_PACKET 10

static int toccami_device_777_permission(struct device *dev,
                                         struct kobj_uevent_env *env) {
  add_uevent_var(env, "DEVMODE=%#o", 0777);
  return 0;
}

static int __init toccami_init(void) {

  printk(KERN_INFO "toccami: Starting init procedure\n");

  toccamiInput = input_allocate_device();
  if (!toccamiInput)
    return -ENOMEM;

  __clear_bit(EV_MSC, toccamiInput->evbit);
  __clear_bit(ABS_TOOL_WIDTH, toccamiInput->absbit);
  __clear_bit(BTN_0, toccamiInput->keybit);
  __clear_bit(BTN_RIGHT, toccamiInput->keybit);
  __clear_bit(BTN_MIDDLE, toccamiInput->keybit);
  __set_bit(BTN_MOUSE, toccamiInput->keybit);
  __set_bit(BTN_TOOL_FINGER, toccamiInput->keybit);

  __set_bit(EV_ABS, toccamiInput->evbit);
  __set_bit(EV_KEY, toccamiInput->evbit);
  __set_bit(BTN_TOUCH, toccamiInput->keybit);
  __set_bit(INPUT_PROP_POINTER, toccamiInput->propbit);

  __set_bit(EV_ABS, toccamiInput->evbit);

  if (input_mt_init_slots(toccamiInput, MAX_TOUCHES,
                          INPUT_MT_POINTER | INPUT_MT_DROP_UNUSED |
                              INPUT_MT_TRACK)) {
    printk(KERN_ERR "toccami: Error allocating slots\n");
    return -ENOMEM;
  }

  input_set_abs_params(toccamiInput, ABS_X, AXIS_X_MIN, AXIS_X_MAX, 0, 0);
  input_set_abs_params(toccamiInput, ABS_Y, AXIS_Y_MIN, AXIS_Y_MAX, 0, 0);

  input_set_abs_params(toccamiInput, ABS_MT_POSITION_X, AXIS_X_MIN, AXIS_X_MAX,
                       0, 0);
  input_set_abs_params(toccamiInput, ABS_MT_POSITION_Y, AXIS_Y_MIN, AXIS_Y_MAX,
                       0, 0);

  input_abs_set_res(toccamiInput, ABS_X, TEMP_RESOLUTION_X);
  input_abs_set_res(toccamiInput, ABS_Y, TEMP_RESOLUTION_Y);
  input_abs_set_res(toccamiInput, ABS_MT_POSITION_X, TEMP_RESOLUTION_X);
  input_abs_set_res(toccamiInput, ABS_MT_POSITION_Y, TEMP_RESOLUTION_Y);

  input_set_events_per_packet(toccamiInput, EVENT_PER_PACKET);

  toccamiInput->name = "Toccami Driver";
  toccamiInput->phys = "toccami/input0";

  if (input_register_device(toccamiInput)) {
    input_free_device(toccamiInput);
    return -EINVAL;
  }

  mutex_init(&toccamiMutex);

  majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
  if (majorNumber < 0) {
    printk(KERN_ALERT "Toccami failed to register a major number\n");
    return majorNumber;
  }

  toccamiClass = class_create(THIS_MODULE, CLASS_NAME);
  toccamiClass->dev_uevent = toccami_device_777_permission;
  if (IS_ERR(toccamiClass)) {
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "Toccami: Failed to register device class\n");
    return PTR_ERR(toccamiClass);
  }

  toccamiDevice = device_create(toccamiClass, NULL, MKDEV(majorNumber, 0), NULL,
                                DEVICE_NAME);
  if (IS_ERR(toccamiDevice)) {
    class_destroy(toccamiClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "Toccami: Failed to create the device\n");
    return PTR_ERR(toccamiDevice);
  }

  printk(KERN_INFO "toccami: Successful init procedure, ready to use\n");

  return 0;
}

static void __exit toccami_exit(void) {

  input_unregister_device(toccamiInput);

  mutex_destroy(&toccamiMutex);

  device_destroy(toccamiClass, MKDEV(majorNumber, 0));
  class_unregister(toccamiClass);
  class_destroy(toccamiClass);
  unregister_chrdev(majorNumber, DEVICE_NAME);

  printk(KERN_INFO "Toccami: Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep) {
  if (!mutex_trylock(&toccamiMutex)) {
    printk(KERN_ALERT "Toccami: Device in use by another process");
    return -EBUSY;
  }
  numberOpens++;
  printk(KERN_INFO "Toccami: Device has been opened %d time(s)\n", numberOpens);
  return 0;
}

/** @brief This function is called whenever device is being read from user space
 * i.e. data is being sent from the device to the user. In this case is uses the
 * copy_to_user() function to send the buffer string to the user and captures
 * any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the
 * data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len,
                        loff_t *offset) {
  return -EINVAL;
}

#define TOCCAMI_EVENT_RELEASED 0
#define TOCCAMI_EVENT_START 1
#define TOCCAMI_EVENT_DRAG 2
#define TOCCAMI_EVENT_CHANGE_RESOLUTION 3

#define TOCCAMI_EVENT_LENGTH 8

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
                         loff_t *offset) {
  unsigned int i, touchCount;
  u16 x, y, pointerIndex, eventType;
  char kernelBuffer[TOCCAMI_EVENT_LENGTH];

  // Only accept multiple of EVENT_LENGTH
  if (len % TOCCAMI_EVENT_LENGTH != 0) {
    printk(KERN_ERR "toccami: invalid message SIZE: %zu\n", len);
    return -EINVAL;
  }

  touchCount = len / TOCCAMI_EVENT_LENGTH;

  for (i = 0; i < touchCount; i++) {
    // Copy the buffer from user
    if (copy_from_user(kernelBuffer, buffer + i * TOCCAMI_EVENT_LENGTH,
                       TOCCAMI_EVENT_LENGTH) != 0) {
      return -EFAULT;
    }

    // Now parse parameters of touch event
    x = *((u16 *)kernelBuffer);
    y = *(u16 *)(kernelBuffer + 2);
    pointerIndex = *(u16 *)(kernelBuffer + 4);
    eventType = *(u16 *)(kernelBuffer + 6);

    // If trying to change resolution, update accordingly
    if (eventType == TOCCAMI_EVENT_CHANGE_RESOLUTION) {
      printk(KERN_DEBUG
             "toccami: Changing resolution: Width=%u; Height=%u; RES=%u\n",
             x, y, pointerIndex);
      input_abs_set_min(toccamiInput, ABS_X, 0);
      input_abs_set_min(toccamiInput, ABS_Y, 0);
      input_abs_set_max(toccamiInput, ABS_X, x);
      input_abs_set_max(toccamiInput, ABS_Y, y);

      // The driver relies on pointerIndex to communicate the resolution
      input_abs_set_res(toccamiInput, ABS_X, pointerIndex);
      input_abs_set_res(toccamiInput, ABS_Y, pointerIndex);

      continue;
    }

    // Debug information: Bad performance
    // printk(KERN_DEBUG "x=%u, y=%u, pointer=%u, evType=%u\n", x, y,
    // pointerIndex,
    //        eventType);

    input_mt_slot(toccamiInput,
                  input_mt_get_slot_by_key(toccamiInput, pointerIndex));

    if (eventType == TOCCAMI_EVENT_START || eventType == TOCCAMI_EVENT_DRAG) {

      input_report_key(toccamiInput, BTN_TOUCH, 1);
      input_report_key(toccamiInput, BTN_TOOL_FINGER, 1);

      input_mt_report_slot_state(toccamiInput, MT_TOOL_FINGER, 1);

      input_report_abs(toccamiInput, ABS_MT_POSITION_X, x);
      input_report_abs(toccamiInput, ABS_MT_POSITION_Y, y);

    } else {
      input_mt_report_slot_state(toccamiInput, MT_TOOL_FINGER, 0);
    }
  }

  input_mt_sync_frame(toccamiInput);
  input_sync(toccamiInput);

  return len;
}

/** @brief The device release function that is called whenever the device is
 * closed/released by the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "Toccami: Device successfully closed\n");
  mutex_unlock(&toccamiMutex);
  return 0;
}

module_init(toccami_init);
module_exit(toccami_exit);