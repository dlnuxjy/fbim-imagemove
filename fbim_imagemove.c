#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <math.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_LINEAR
#include "stb/stb_image.h"
#include <errno.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_SATURATE_INT
#include "stb/stb_image_resize.h"
#include <sys/time.h>

#define LOADALLONCE

struct rgba {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} __attribute__((packed));


#define MOVEXRIGHT 0
#define MOVEYDOWN  1
#define MOVEXLEFT  2
#define MOVEYUP    3

int main(int argc, char** argv)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	int fd;
	
	char *fb = NULL;
	char *imagePath = NULL;
	switch (argc) {
		case 2:
			fb = "/dev/fb0";
			imagePath = argv[1];
			break;

		case 3:
			fb = argv[1];
			imagePath = argv[2];
			break;

		default:
			(void) fprintf(stderr, "Usage: %s [FB] PATH\n", argv[0]);
			return -1;
	}
	
	//open dev framebuffer
	fd = open(fb, O_RDWR);
	if(fd == -1) {
		printf("open fb[%s] err. errno=[%d]:[%s]\n", fb, errno, strerror(errno));
		return -1;
	} else {
		printf("open fb success\n");
	}
	
	//get dev conf
	if (-1 == ioctl(fd, FBIOGET_FSCREENINFO, &fix)) {
		printf("ioctl FBIOGET_FSCREENINFO fb[%s] err. errno=[%d]:[%s]\n", fb, errno, strerror(errno));
		close(fd);
		return -1;
	} else {
		//printf();
	}

	if (-1 == ioctl(fd, FBIOGET_VSCREENINFO, &var)) {
		printf("ioctl FBIOGET_FSCREENINFO fb[%s] err. errno=[%d]:[%s]\n", fb, errno, strerror(errno));
		close(fd);
		return -1;
	} else {
		//printf();
	}

	/* we do not support 1/8/16/24 BPP framebuffers at the moment */
	if (32 != var.bits_per_pixel) {
		printf("we do not support 1/8/16/24 BPP framebuffers at the moment\n");
		close(fd);	
		return -1;
	}


	//map
	int len = fix.line_length * var.yres;
	unsigned char *fb_data = (unsigned char *) mmap(NULL,
	                                 len,
	                                 PROT_READ | PROT_WRITE,
	                                 MAP_SHARED,
	                                 fd,
	                                 0);
	if (MAP_FAILED == fb_data) {
		printf("mmap err. errno=[%d]:[%s]\n", errno, strerror(errno));
		close(fd);			
	}
	
	//boot image
	unsigned char *im_data = NULL;
	unsigned char *resized_data = NULL;
	size_t line_len;
	uint32_t *pixel = NULL;
	struct rgba *rgba = NULL;
	int x;
	int y;
	int n;
	int new_x;
	int new_y;
	uint32_t red_len;
	uint32_t blue_len;
	uint32_t green_len;
	uint32_t transp_len;
	
	struct timeval start, cur;
	gettimeofday(&start, NULL);
	int i = 0;

	im_data = stbi_load(imagePath, &x, &y, &n, 4);
	if (NULL == im_data) {
		printf("stbi_load [%s] err. errno=[%d]:[%s]\n", imagePath, errno, strerror(errno));
		munmap(fb_data, len);
		close(fd);
		return -1;
	} else {
		/* convert the pixels from RGBA to the framebuffer format */
		red_len = 8 - var.red.length;
		green_len = 8 - var.green.length;
		blue_len = 8 - var.blue.length;
		transp_len = 8 - var.transp.length;
		for (pixel = (uint32_t *) im_data; (uint32_t *) (im_data + (x * y * n)) > pixel; ++pixel) {
			rgba = (struct rgba *) pixel;
			*pixel = ((rgba->r >> red_len) << var.red.offset) |
					 ((rgba->g >> green_len) << var.green.offset) |
					 ((rgba->b >> blue_len) << var.blue.offset) |
					 ((rgba->a >> transp_len) << var.transp.offset);
		}
	}
	
	printf("var.red.offset = [%d]\nvar.green.offset = [%d]\nvar.blue.offset = [%d]\nvar.transp.offset = [%d]\n", var.red.offset, var.green.offset, var.blue.offset, var.transp.offset);
	
	gettimeofday(&cur, NULL);
	printf("image[%s] loaded. [%dms]\n", imagePath, (cur.tv_sec - start.tv_sec)*1000 + (cur.tv_usec - start.tv_usec)/1000);
	
	
	resized_data = im_data;
	
	/* calculate the size of single image line, which is different from the size
	 * of a framebuffer line if their resolutions differ */
	line_len = (size_t) (x * n);

	printf("x = [%d] y = [%d] n = [%d] line_len = [%d]\n", x, y, n, line_len);
	
	printf("fix.line_length = [%d] var.xres = [%d] var.yres = [%d]\n", fix.line_length, var.xres, var.yres);
	
	
	int imagewidth = x, imageheight = y;
	int fbwidth = 592, fbheight = 592;
	int memcpysize = (size_t) (fbwidth * n); //648*4=2368
	int xoffset = 0, yoffset = 0;
	
	if(imagewidth < fbwidth || imageheight < fbheight) {
		printf("image size too small. Minimum size %d*%d.\n", fbwidth, fbheight);
		stbi_image_free((void *) im_data);
		munmap(fb_data, len);
		close(fd);
		return -1;		
	}
	
	int movestatus = MOVEXRIGHT;
	int movestep = 1;
	while(1) {
		for (int y_tmp=fbheight-1;0<=y_tmp; --y_tmp) {
			(void) memcpy((void *) &fb_data[y_tmp * fix.line_length],
						  (void *) &resized_data[(y_tmp+yoffset) * line_len + xoffset * n],
						  memcpysize);
		}
		usleep(32 * 1000);
		//move iamge xoffset yoffset		
		switch(movestatus) {
			case MOVEXRIGHT:
				xoffset += movestep;
				if(xoffset >= imagewidth - fbwidth) {
					xoffset = imagewidth - fbwidth;
					movestatus = MOVEYDOWN;
				}
			break;
			case MOVEYDOWN:
				yoffset += movestep;
				if(yoffset >= imageheight - fbheight) {
					yoffset = imageheight - fbheight;
					movestatus = MOVEXLEFT;
				}
			break;			
			case MOVEXLEFT:
				xoffset -= movestep;
				if(xoffset <= 0) {
					xoffset = 0;
					movestatus = MOVEYUP;
				}
			break;			
			case MOVEYUP:
				yoffset -= movestep;
				if(yoffset <= 0) {
					yoffset = 0;
					movestatus = MOVEXRIGHT;
				}
			break;
			default:
				movestatus = MOVEXRIGHT;
			break;
		}
	}
	
	//
	stbi_image_free((void *) im_data);
	
	(void) munmap(fb_data, len);
	
	(void) close(fd);
	return 0;
}

