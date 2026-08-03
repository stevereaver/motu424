#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>

static void __iomem *iobase_bar0;
static void __iomem *iobase_bar1;
static void __iomem *iobase_bar2;
static struct pci_dev *pdev;

static void *dma_buf;
static dma_addr_t dma_phys;
static size_t dma_size = 4 * 1024 * 1024; // 4MB buffer

struct motu_ioctl_data {
    uint32_t offset;
    uint32_t value;
    int write;
    int bar;
};

#define MOTU_IOC_MAGIC 'M'
#define MOTU_IOC_POKE _IOWR(MOTU_IOC_MAGIC, 1, struct motu_ioctl_data)
#define MOTU_IOC_GET_DMA _IOR(MOTU_IOC_MAGIC, 2, uint32_t)
#define MOTU_IOC_READ_DMA _IOWR(MOTU_IOC_MAGIC, 3, struct motu_ioctl_data)

static long motu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct motu_ioctl_data data;
    void __iomem *base;
    uint32_t addr32;

    if (cmd == MOTU_IOC_GET_DMA) {
        addr32 = (uint32_t)dma_phys;
        if (copy_to_user((void __user *)arg, &addr32, sizeof(addr32))) return -EFAULT;
        return 0;
    }

    if (cmd == MOTU_IOC_READ_DMA) {
        if (copy_from_user(&data, (void __user *)arg, sizeof(data))) return -EFAULT;
        if (data.offset + 4 > dma_size) return -EINVAL;
        data.value = *(uint32_t *)(dma_buf + data.offset);
        if (copy_to_user((void __user *)arg, &data, sizeof(data))) return -EFAULT;
        return 0;
    }
    
    if (cmd != MOTU_IOC_POKE) return -ENOTTY;
    if (copy_from_user(&data, (void __user *)arg, sizeof(data))) return -EFAULT;
    
    if (data.bar == 0) base = iobase_bar0;
    else if (data.bar == 1) base = iobase_bar1;
    else if (data.bar == 2) base = iobase_bar2;
    else return -EINVAL;

    if (!base) return -ENODEV;

    if (data.write) {
        iowrite32(data.value, base + data.offset);
    } else {
        data.value = ioread32(base + data.offset);
    }

    if (!data.write) {
        if (copy_to_user((void __user *)arg, &data, sizeof(data))) return -EFAULT;
    }
    return 0;
}

static const struct file_operations motu_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = motu_ioctl,
};

static struct miscdevice motu_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "motu_poke",
    .fops = &motu_fops,
};

static int __init motu_poke_init(void)
{
    pdev = pci_get_device(0x137a, 0x0004, NULL);
    if (!pdev) return -ENODEV;
    
    if (pci_enable_device(pdev) < 0) {
        pci_dev_put(pdev);
        return -EIO;
    }
    pci_set_master(pdev);
    
    dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
    dma_buf = dma_alloc_coherent(&pdev->dev, dma_size, &dma_phys, GFP_KERNEL);
    if (!dma_buf) {
        pr_err("MOTU: Failed to allocate 4MB DMA buffer!\n");
    } else {
        memset(dma_buf, 0, dma_size);
        pr_info("MOTU: Allocated 4MB DMA buffer at physical %pad\n", &dma_phys);
    }

    pci_request_regions(pdev, "motu_poke");

    iobase_bar0 = pci_iomap(pdev, 0, 0);
    iobase_bar1 = pci_iomap(pdev, 1, 0);
    iobase_bar2 = pci_iomap(pdev, 2, 0);
    
    return misc_register(&motu_miscdev);
}

static void __exit motu_poke_exit(void)
{
    misc_deregister(&motu_miscdev);
    if (iobase_bar2) pci_iounmap(pdev, iobase_bar2);
    if (iobase_bar1) pci_iounmap(pdev, iobase_bar1);
    if (iobase_bar0) pci_iounmap(pdev, iobase_bar0);
    pci_release_regions(pdev);
    
    if (dma_buf) {
        dma_free_coherent(&pdev->dev, dma_size, dma_buf, dma_phys);
    }
    
    pci_disable_device(pdev);
    if (pdev) pci_dev_put(pdev);
}

module_init(motu_poke_init);
module_exit(motu_poke_exit);
MODULE_LICENSE("GPL");
