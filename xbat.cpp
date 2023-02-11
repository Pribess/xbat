#include <X11/X.h>
#include <X11/Xlib.h>

#include <libudev.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define WIDTH 150
#define HEIGHT 21

#define X 50
#define Y 50

void draw(Display *display, Window window, GC gc, Font font, unsigned long leading_top_border, unsigned long trailing_bottom_border, std::string status, std::string capacity) {
	XClearWindow(display, window);
	XResizeWindow(display, window, WIDTH, HEIGHT);

	XGCValues gcvalue;

	Window root;
	int x;
	int y;
	unsigned int width;
	unsigned int height;
	unsigned int border_width;
	unsigned int depth;
	XGetGeometry(display, window, &root, &x, &y, &width, &height, &border_width, &depth);

	gcvalue.line_width = 4;
	gcvalue.foreground = trailing_bottom_border;
	XChangeGC(display, gc, GCLineWidth | GCForeground, &gcvalue);
	XDrawLine(display, window, gc, width, height, width, 1);
	XDrawLine(display, window, gc, width, height, 1, height);

	gcvalue.line_width = 4;
	gcvalue.foreground = leading_top_border;
	XChangeGC(display, gc, GCLineWidth | GCForeground, &gcvalue);
	XDrawLine(display, window, gc, 0, 0, width - 1, 0);
	XDrawPoint(display, window, gc, width - 1, 0);
	XDrawLine(display, window, gc, 0, 0, 0, height - 1);
	XDrawPoint(display, window, gc, 0, height - 1);

	gcvalue.foreground = BlackPixel(display, 0);
	XChangeGC(display, gc, GCForeground, &gcvalue);

	XDrawString(display, window, gc, 10, 15, status == "Discharging" ? "BAT" : "AC" , status == "Discharging" ? 3 : 2);
	XDrawString(display, window, gc, 35, 15, status.c_str(), status.length());
	capacity.append("%");
	XDrawString(display, window, gc, 120, 15, capacity.c_str(), capacity.length());

	XFlush(display);
}

int main(int argc, char *argv[]) {

	Display *display;
	display = XOpenDisplay(NULL);

	Colormap colormap;
	colormap = XDefaultColormap(display, 0);

	XColor color;
	XColor color_exact;

	XAllocNamedColor(display, colormap, "rgb:B8/B8/B8", &color, &color_exact);
	unsigned long background = color.pixel;

	XAllocNamedColor(display, colormap, "rgb:FF/FF/FF", &color, &color_exact);
	unsigned long leading_top_border = color.pixel;

	XAllocNamedColor(display, colormap, "rgb:60/60/60", &color, &color_exact);
	unsigned long trailing_bottom_border = color.pixel;

	Window root = XDefaultRootWindow(display);
	Window window = XCreateSimpleWindow(display, root, X, Y, HEIGHT, WIDTH, 0, WhitePixel(display, 0), background);

	XSetWindowAttributes attribute;
	attribute.override_redirect = true;
	XChangeWindowAttributes(display, window, CWOverrideRedirect, &attribute);

	XMapWindow(display, window);


	std::string path;

	path = "/sys/class/power_supply/BAT1/status";
	std::ifstream status = std::ifstream(path);
	if (status.fail()) {
		std::perror(("xbat: '" + path + "'").c_str());
		std::exit(EXIT_FAILURE);
	}

	path = "/sys/class/power_supply/BAT1/capacity";
	std::ifstream capacity = std::ifstream(path);
	if (capacity.fail()) {
		std::perror(("xbat: '" + path + "'").c_str());
		std::exit(EXIT_FAILURE);
	}

	
	GC gc;
	XGCValues gcvalue;

	gc = XCreateGC(display, window, 0, &gcvalue);

	Font font = XLoadFont(display, "fixed");

	std::string status_str;
	std::getline(status, status_str);
	status.seekg(0);
	std::string capacity_str;
	std::getline(capacity, capacity_str);
	capacity.seekg(0);

	draw(display, window, gc, font, leading_top_border, trailing_bottom_border, status_str, capacity_str);

	struct udev *udev;
	if (!(udev = udev_new())) {
		std::cerr << "xbat: Failed to create udev context object" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	const char *syspath = "/sys/class/power_supply/BAT1";
	struct udev_device *device;
	if (!(device = udev_device_new_from_syspath(udev, syspath))) {
		std::cerr << "xbat: '" << syspath << "': " << "Failed to load device" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	struct udev_monitor *monitor;
	if (!(monitor = udev_monitor_new_from_netlink(udev, "udev"))) {
		std::cerr << "xbat: Failed to create udev monitor" << std::endl;
		std::exit(EXIT_FAILURE);
	}
	int monitor_fd = udev_monitor_get_fd(monitor);

	if (udev_monitor_filter_add_match_subsystem_devtype(monitor, "power_supply", NULL) < 0) {
		std::cerr << "xbat: Failed to add filter" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	if (udev_monitor_enable_receiving(monitor) < 0) {
		std::cerr << "xbat: Failed to enable monitor recieving" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	int ep;
	if (!(ep = epoll_create(1))) {
		std::perror("xbat");
		std::exit(EXIT_FAILURE);
	}

	epoll_event event;
	std::memset(&event, 0x00, sizeof(struct epoll_event));
	event.events = EPOLLIN;
	event.data.fd = monitor_fd;
	
	if (epoll_ctl(ep, EPOLL_CTL_ADD, monitor_fd, &event) < 0) {
		std::perror("xbat");
		std::exit(EXIT_FAILURE);
	}

	for (;;) {
		epoll_event event[1];
		int cnt = epoll_wait(ep, event, 1, -1);
		if (event[0].data.fd == monitor_fd && event[0].events & EPOLLIN) {
			struct udev_device *dev = udev_monitor_receive_device(monitor);
	
			std::string status_str;
			std::getline(status, status_str);
			status.seekg(0);
			std::string capacity_str;
			std::getline(capacity, capacity_str);
			capacity.seekg(0);

			draw(display, window, gc, font, leading_top_border, trailing_bottom_border, status_str, capacity_str);
			udev_device_unref(dev);
		}
	}

	XUnloadFont(display, font);
	XFreeGC(display, gc);
	XDestroyWindow(display, window);
	XCloseDisplay(display);

	return 0;
}
