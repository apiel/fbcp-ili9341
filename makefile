# could use:
# pkg-config --cflags --libs bcm_host

all:
	g++ -o fbcp-ili9341 fbcp-ili9341.cpp spi.cpp -lm -lpthread -lbcm_host
