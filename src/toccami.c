#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/fs.h>     // Header for the Linux file system support
#include <linux/init.h> // Macros used to mark up functions e.g., __init __exit
#include <linux/kernel.h>  // Contains types, macros, functions for the kernel
#include <linux/module.h>  // Core header for loading LKMs into the kernel
#include <linux/uaccess.h> // Required for the copy to user function

#define DEVICE_NAME                                                            \
  "toccamich" ///< The device will appear at /dev/ebbchar using this value
#define CLASS_NAME                                                             \
  "toccami" ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrea Somaini");
MODULE_DESCRIPTION("Virtual Touchpad driver for Toccami");
MODULE_VERSION("0.1");

static int
    majorNumber; ///< Stores the device number -- determined automatically
static int numberOpens = 0; ///< Counts the number of times the device is opened
static struct class *toccamiClass =
    NULL; ///< The device-driver class struct pointer
static struct device *toccamiDevice =
    NULL; ///< The device-driver device struct pointer

static DEFINE_MUTEX(ebbchar_mutex);

// The prototype functions for the character driver -- must come before the
// struct definition
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The
 * file_operations structure from /linux/fs.h lists the callback functions that
 * you wish to associated with your file operations using a C99 syntax
 * structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C
 * file. The __init macro means that for a built-in driver (not a LKM) the
 * function is only used at initialization time and that it can be discarded and
 * its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init toccami_init(void) {
  printk(KERN_INFO "toccami: Initializing char device!\n");

  mutex_init(
      &ebbchar_mutex); /// Initialize the mutex lock dynamically at runtime

  // Try to dynamically allocate a major number for the device -- more difficult
  // but worth it
  majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
  if (majorNumber < 0) {
    printk(KERN_ALERT "Toccami failed to register a major number\n");
    return majorNumber;
  }
  printk(KERN_INFO "Toccami: registered correctly with major number %d\n",
         majorNumber);

  // Register the device class
  toccamiClass = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(toccamiClass)) { // Check for error and clean up if there is
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "Toccami: Failed to register device class\n");
    return PTR_ERR(toccamiClass); // Correct way to return an error on a pointer
  }
  printk(KERN_INFO "Toccami: device class registered correctly\n");

  // Register the device driver
  toccamiDevice = device_create(toccamiClass, NULL, MKDEV(majorNumber, 0), NULL,
                                DEVICE_NAME);
  if (IS_ERR(toccamiDevice)) { // Clean up if there is an error
    class_destroy(
        toccamiClass); // Repeated code but the alternative is goto statements
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "Toccami: Failed to create the device\n");
    return PTR_ERR(toccamiDevice);
  }
  printk(KERN_INFO
         "Toccami: device class created correctly\n"); // Made it! device was
                                                       // initialized

  return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro
 * notifies that if this code is used for a built-in driver (not a LKM) that
 * this function is not required.
 */
static void __exit toccami_exit(void) {
  mutex_destroy(&ebbchar_mutex);

  device_destroy(toccamiClass, MKDEV(majorNumber, 0)); // remove the device
  class_unregister(toccamiClass);              // unregister the device class
  class_destroy(toccamiClass);                 // remove the device class
  unregister_chrdev(majorNumber, DEVICE_NAME); // unregister the major number

  printk(KERN_INFO "Toccami: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is
 * opened This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep) {
  /// Try to acquire the mutex (i.e., put the lock
  /// on/down) returns 1 if successful and 0 if there
  /// is contention
  if (!mutex_trylock(&ebbchar_mutex)) {
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

#define TOCCAMI_EVENT_MOVE 0
#define TOCCAMI_EVENT_UP 1
#define TOCCAMI_EVENT_DOWN 2

#define MESSAGE_LENGTH 12

/** @brief This function is called whenever the device is being written to from
 * user space i.e. data is sent to the device from the user. The data is copied
 * to the message[] array in this LKM using the sprintf() function along with
 * the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const
 * char buffer
 *  @param offset The offset if required
 *  @return Number of bytes read
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len,
                         loff_t *offset) {
  u16 x, y;
  int pointerIndex, eventType;
  char kernelBuffer[MESSAGE_LENGTH];

  printk(KERN_INFO "Toccami: Received %zu characters from the user\n", len);

  // For now, assume a single 4 integers (16 bits) packets, or refuse
  if (len != MESSAGE_LENGTH)
    return -EINVAL;

  // Copy the buffer from user
  if (copy_from_user(kernelBuffer, buffer, len) != 0) {
    return -EFAULT;
  }

  // Now parse parameters
  x = *((u16 *)kernelBuffer);
  y = *(u16 *)(kernelBuffer + 2);
  pointerIndex = *(int *)(kernelBuffer + 4);
  eventType = *(int *)(kernelBuffer + 8);

  printk(KERN_INFO "toccami: x=%u; y=%u; pointerIndex = %d; eventType = %d\n",
         x, y, pointerIndex, eventType);

  return len;
}

/** @brief The device release function that is called whenever the device is
 * closed/released by the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "Toccami: Device successfully closed\n");
  mutex_unlock(&ebbchar_mutex);
  return 0;
}

module_init(toccami_init);
module_exit(toccami_exit);