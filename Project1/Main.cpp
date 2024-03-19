#include <iostream>
#include "TriangleApplication.h"

int main() {
	try {
		// swapchain, framebuffer, color, depth attachment are to fixed with window size
		// every time window resize, you must create new swapcahin and others...
		jhb::HelloTriangleApplication app(800, 600);
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}