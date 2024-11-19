#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>

#define RCAR_MEM_FB_WIDTH  480
#define RCAR_MEM_FB_HEIGHT 272
#define RCAR_MEM_FB_BPP    24    // RGB888
#define RCAR_MEM_FB_START  0x58000000
#define RCAR_MEM_FB_SIZE   (RCAR_MEM_FB_WIDTH * RCAR_MEM_FB_HEIGHT * RCAR_MEM_FB_BPP/8)

static struct fb_fix_screeninfo rcar_mem_fb_fix = {
    .id = "rcar-mem-fb",
    .type = FB_TYPE_PACKED_PIXELS,
    .visual = FB_VISUAL_TRUECOLOR,
    .xpanstep = 0,
    .ypanstep = 0,
    .ywrapstep = 0,
    .accel = FB_ACCEL_NONE,
    .line_length = RCAR_MEM_FB_WIDTH * 3,
};

static struct fb_var_screeninfo rcar_mem_fb_var = {
    .xres = RCAR_MEM_FB_WIDTH,
    .yres = RCAR_MEM_FB_HEIGHT,
    .xres_virtual = RCAR_MEM_FB_WIDTH,
    .yres_virtual = RCAR_MEM_FB_HEIGHT,
    .bits_per_pixel = RCAR_MEM_FB_BPP,
    .red = {16, 8, 0},     // RGB888
    .green = {8, 8, 0},
    .blue = {0, 8, 0},
    .activate = FB_ACTIVATE_NOW,
    .height = -1,
    .width = -1,
    .vmode = FB_VMODE_NONINTERLACED,
};

static int rcar_mem_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    // 只支持固定分辨率和颜色深度
    if (var->xres != RCAR_MEM_FB_WIDTH || var->yres != RCAR_MEM_FB_HEIGHT ||
        var->bits_per_pixel != RCAR_MEM_FB_BPP)
        return -EINVAL;
        
    return 0;
}

static struct fb_ops rcar_mem_fb_ops = {
    .owner = THIS_MODULE,
    .fb_check_var = rcar_mem_fb_check_var,
    .fb_fillrect = cfb_fillrect,
    .fb_copyarea = cfb_copyarea, 
    .fb_imageblit = cfb_imageblit,
};

static int rcar_mem_fb_probe(struct platform_device *pdev)
{
    struct fb_info *info;
    int ret;
    struct resource *res;

    // 检查内存区域是否可用
    if (!request_mem_region(RCAR_MEM_FB_START, RCAR_MEM_FB_SIZE, "rcar-mem-fb")) {
        dev_err(&pdev->dev, "Cannot request memory region\n");
        return -EBUSY;
    }

    info = framebuffer_alloc(0, &pdev->dev);
    if (!info) {
        ret = -ENOMEM;
        goto err_release_mem_region;
    }

    info->fix = rcar_mem_fb_fix;
    info->var = rcar_mem_fb_var;
    info->fbops = &rcar_mem_fb_ops;
    info->flags = FBINFO_DEFAULT;
    
    info->fix.smem_start = RCAR_MEM_FB_START;
    info->fix.smem_len = RCAR_MEM_FB_SIZE;
    
    // 使用memremap替代ioremap，因为这是一个内存区域
    info->screen_base = memremap(RCAR_MEM_FB_START, RCAR_MEM_FB_SIZE, MEMREMAP_WB);
    if (!info->screen_base) {
        dev_err(&pdev->dev, "Cannot map framebuffer memory\n");
        ret = -ENOMEM;
        goto err_release_fb;
    }

    // 清空帧缓冲区
    memset_io(info->screen_base, 0, info->fix.smem_len);

    ret = register_framebuffer(info);
    if (ret < 0) {
        dev_err(&pdev->dev, "Cannot register framebuffer\n");
        goto err_unmap;
    }

    platform_set_drvdata(pdev, info);
    dev_info(&pdev->dev, "fb%d: R-Car memory fb device registered successfully\n", info->node);
    return 0;

err_unmap:
    memunmap(info->screen_base);
err_release_fb:
    framebuffer_release(info);
err_release_mem_region:
    release_mem_region(RCAR_MEM_FB_START, RCAR_MEM_FB_SIZE);
    return ret;
}

static int rcar_mem_fb_remove(struct platform_device *pdev)
{
    struct fb_info *info = platform_get_drvdata(pdev);

    unregister_framebuffer(info);
    iounmap(info->screen_base);
    framebuffer_release(info);

    return 0;
}

static struct platform_driver rcar_mem_fb_driver = {
    .probe = rcar_mem_fb_probe,
    .remove = rcar_mem_fb_remove,
    .driver = {
        .name = "rcar-mem-fb",
    },
};

static struct platform_device *rcar_mem_fb_device;

static int __init rcar_mem_fb_init(void)
{
    int ret;

    // 先注册平台设备
    rcar_mem_fb_device = platform_device_alloc("rcar-mem-fb", -1);
    if (!rcar_mem_fb_device) {
        pr_err("Failed to allocate platform device\n");
        return -ENOMEM;
    }

    ret = platform_device_add(rcar_mem_fb_device);
    if (ret) {
        pr_err("Failed to add platform device\n");
        platform_device_put(rcar_mem_fb_device);
        return ret;
    }

    // 然后注册平台驱动
    ret = platform_driver_register(&rcar_mem_fb_driver);
    if (ret) {
        pr_err("Failed to register platform driver\n");
        platform_device_unregister(rcar_mem_fb_device);
        return ret;
    }

    return 0;
}

static void __exit rcar_mem_fb_exit(void)
{
    platform_device_unregister(rcar_mem_fb_device);
    platform_driver_unregister(&rcar_mem_fb_driver);
}

module_init(rcar_mem_fb_init);
module_exit(rcar_mem_fb_exit);

MODULE_AUTHOR("Jiaxuan Sun");
MODULE_DESCRIPTION("R-Car Memory-based Framebuffer Driver");
MODULE_LICENSE("GPL");