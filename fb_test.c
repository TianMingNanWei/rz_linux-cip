#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <errno.h>

// 彩条颜色定义（RGBA 格式）
unsigned int colors[] = {
    0xFF0000FF, // Red
    0xFF00FF00, // Green
    0xFFFF0000, // Blue
    0xFFFFFF00, // Yellow
    0xFFFF00FF, // Magenta
    0xFF00FFFF, // Cyan
    0xFFFFFFFF  // White
};

#define NUM_BARS (sizeof(colors) / sizeof(colors[0]))

void draw_colorbars(unsigned char *fbp, int width, int height, struct fb_var_screeninfo *vinfo) {
    int bytesPerPixel = vinfo->bits_per_pixel / 8;
    int bar_width = width / NUM_BARS;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int bar_index = x / bar_width;
            if (bar_index >= NUM_BARS) bar_index = NUM_BARS - 1;

            unsigned int color = colors[bar_index];
            int offset = (x + y * width) * bytesPerPixel;

            // 根据实际的颜色偏移量写入
            unsigned int r = (color >> 16) & 0xFF;
            unsigned int g = (color >> 8) & 0xFF;
            unsigned int b = color & 0xFF;
            unsigned int a = (color >> 24) & 0xFF;

            // 将RGB值打包到正确的位置
            unsigned int pixel = 0;
            pixel |= (r << vinfo->red.offset);
            pixel |= (g << vinfo->green.offset);
            pixel |= (b << vinfo->blue.offset);
            if (vinfo->transp.length > 0) {
                pixel |= (a << vinfo->transp.offset);
            }

            // 写入像素值
            memcpy(fbp + offset, &pixel, bytesPerPixel);
        }
    }
}

int main() {
    const int target_width = 480;
    const int target_height = 272;

    // 打开 FrameBuffer 设备
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error opening /dev/fb0");
        return EXIT_FAILURE;
    }

    // 获取当前 FrameBuffer 信息
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error reading variable screen info");
        close(fb_fd);
        return EXIT_FAILURE;
    }
    
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error reading fixed screen info");
        close(fb_fd);
        return EXIT_FAILURE;
    }

    // 打印当前分辨率和格式信息
    printf("Current resolution: %dx%d, %d bpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    printf("Red: offset=%d, length=%d\n", vinfo.red.offset, vinfo.red.length);
    printf("Green: offset=%d, length=%d\n", vinfo.green.offset, vinfo.green.length);
    printf("Blue: offset=%d, length=%d\n", vinfo.blue.offset, vinfo.blue.length);
    printf("Transp: offset=%d, length=%d\n", vinfo.transp.offset, vinfo.transp.length);
    printf("Line length: %d\n", finfo.line_length);

    // 保存原始设置
    struct fb_var_screeninfo orig_vinfo = vinfo;

    // 修改分辨率，保持其他参数不变
    vinfo.xres = target_width;
    vinfo.yres = target_height;
    vinfo.xres_virtual = target_width;
    vinfo.yres_virtual = target_height;
    // 不修改 bits_per_pixel，使用硬件原有的设置
    
    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo)) {
        perror("Error setting variable screen info");
        printf("Trying with original bit depth...\n");
        
        // 如果设置失败，恢复原始设置再试一次
        vinfo = orig_vinfo;
        vinfo.xres = target_width;
        vinfo.yres = target_height;
        vinfo.xres_virtual = target_width;
        vinfo.yres_virtual = target_height;
        
        if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo)) {
            perror("Error setting variable screen info with original settings");
            close(fb_fd);
            return EXIT_FAILURE;
        }
    }

    // 打印更改后的分辨率
    printf("New resolution: %dx%d, %d bpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // 映射 FrameBuffer 内存
    long screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    unsigned char *fbp = (unsigned char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbp == MAP_FAILED) {
        perror("Error mapping framebuffer to memory");
        close(fb_fd);
        return EXIT_FAILURE;
    }

    // 绘制彩条
    draw_colorbars(fbp, vinfo.xres, vinfo.yres, &vinfo);

    // 等待用户输入以保持显示
    printf("Press Enter to exit...\n");
    getchar();

    // 清理
    munmap(fbp, screensize);
    close(fb_fd);

    return EXIT_SUCCESS;
}

